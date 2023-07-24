// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGMeshSampler.h"

#include "PCGComponent.h"
#include "PCGModule.h"
#include "PCGPin.h"
#include "Data/PCGPointData.h"
#include "Helpers/PCGAsync.h"
#include "Helpers/PCGBlueprintHelpers.h"
#include "Helpers/PCGHelpers.h"

#include "UDynamicMesh.h"
#include "Engine/AssetManager.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "GeometryScript/MeshAssetFunctions.h"
#include "GeometryScript/MeshNormalsFunctions.h"
#include "GeometryScript/MeshQueryFunctions.h"
#include "GeometryScript/MeshRepairFunctions.h"
#include "GeometryScript/MeshVertexColorFunctions.h"
#include "GeometryScript/MeshVoxelFunctions.h"
#include "Sampling/MeshSurfacePointSampling.h"

#define LOCTEXT_NAMESPACE "PCGMeshSampler"

UPCGMeshSamplerSettings::UPCGMeshSamplerSettings()
	: UPCGSettings()
{
	bUseSeed = true;
}

TArray<FPCGPinProperties> UPCGMeshSamplerSettings::InputPinProperties() const
{
	return TArray<FPCGPinProperties>{};
}

FPCGElementPtr UPCGMeshSamplerSettings::CreateElement() const
{
	return MakeShared<FPCGMeshSamplerElement>();
}

#if WITH_EDITOR
FText UPCGMeshSamplerSettings::GetNodeTooltipText() const
{
	return LOCTEXT("MeshSamplerNodeTooltip", "Sample points on a static mesh.");
}
#endif // WITH_EDITOR

FPCGMeshSamplerContext::~FPCGMeshSamplerContext()
{
	// The context can be destroyed if the task is canceled. In that case, we need to notify the future that it should stop,
	// and wait for it to finish (as the future uses some data stored in the context that will go dangling if the context is destroyed).
	if (SamplingFuture.IsValid() && !SamplingFuture.IsReady())
	{
		StopSampling = true;
		SamplingFuture.Wait();
	}

	// DynamicMesh needs to be removed from root, as we added it to root when we created it to avoid getting GC between executions.
	if (DynamicMesh)
	{
		DynamicMesh->RemoveFromRoot();
		DynamicMesh->MarkAsGarbage();
	}
}

FPCGContext* FPCGMeshSamplerElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGMeshSamplerContext* Context = new FPCGMeshSamplerContext();
	Context->InputData = InputData;
	Context->SourceComponent = SourceComponent;
	Context->Node = Node;

	return Context;
}

bool FPCGMeshSamplerElement::PrepareDataInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGMeshSamplerElement::PrepareData);
	FPCGMeshSamplerContext* Context = static_cast<FPCGMeshSamplerContext*>(InContext);

	check(Context);

	if (!Context->SourceComponent.IsValid())
	{
		return true;
	}

	const UPCGMeshSamplerSettings* Settings = Context->GetInputSettings<UPCGMeshSamplerSettings>();
	check(Settings);

	// TODO: Could be async
	TSoftObjectPtr<UStaticMesh> StaticMeshPtr{ Settings->StaticMeshPath };
	UStaticMesh* StaticMesh = StaticMeshPtr.LoadSynchronous();

	if (!StaticMesh)
	{
		PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("MeshDoesNotExist", "Provided static mesh does not exist: '{0}'"), FText::FromString(Settings->StaticMeshPath.ToString())));
		return true;
	}

	Context->DynamicMesh = NewObject<UDynamicMesh>();
	// Need to add to root to make sure we don't delete it. Will be removed from root in the context destructor.
	Context->DynamicMesh->AddToRoot();

	EGeometryScriptOutcomePins Outcome;

	UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromStaticMesh(StaticMesh, Context->DynamicMesh, FGeometryScriptCopyMeshFromAssetOptions{},
		FGeometryScriptMeshReadLOD{ Settings->RequestedLODType, Settings->RequestedLODIndex }, Outcome);

	if (Outcome == EGeometryScriptOutcomePins::Failure)
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("StaticToDynamicMeshFailed", "Static to Dynamic mesh failed"));
		return true;
	}
	
	if (Settings->bVoxelize)
	{
		FGeometryScriptSolidifyOptions SolidifyOptions{};
		SolidifyOptions.GridParameters.GridCellSize = Settings->VoxelSize;
		SolidifyOptions.GridParameters.SizeMethod = EGeometryScriptGridSizingMethod::GridCellSize;

		UGeometryScriptLibrary_MeshVoxelFunctions::ApplyMeshSolidify(Context->DynamicMesh, SolidifyOptions);

		if (Settings->bRemoveHiddenTriangles)
		{
			FGeometryScriptRemoveHiddenTrianglesOptions RemoveTriangleOptions{};
			
			UGeometryScriptLibrary_MeshRepairFunctions::RemoveHiddenTriangles(Context->DynamicMesh, RemoveTriangleOptions);
		}
	}

	switch (Settings->SamplingMethod)
	{
	case EPCGMeshSamplingMethod::OnePointPerVertex:
	{
		bool Dummy, Dummy2;
		UGeometryScriptLibrary_MeshQueryFunctions::GetAllVertexPositions(Context->DynamicMesh, Context->Positions, /*bSkipGaps=*/false, /*bHasVertexIDGaps=*/Dummy);
		UGeometryScriptLibrary_MeshVertexColorFunctions::GetMeshPerVertexColors(Context->DynamicMesh, Context->Colors, /*bIsValidColorSet=*/Dummy, /*bHasVertexIDGaps=*/Dummy2);
		UGeometryScriptLibrary_MeshNormalsFunctions::GetMeshPerVertexNormals(Context->DynamicMesh, Context->Normals, /*bIsValidNormalSet=*/Dummy, /*bHasVertexIDGaps=*/Dummy2);
		Context->Iterations = Context->Positions.List->Num();
		break;
	}
	case EPCGMeshSamplingMethod::OnePointPerTriangle:
	{
		bool Dummy;
		UGeometryScriptLibrary_MeshQueryFunctions::GetAllTriangleIDs(Context->DynamicMesh, Context->TriangleIds, /*bHasTriangleIDGaps=*/Dummy);
		Context->Iterations = Context->TriangleIds.List->Num();
		break;
	}
	case EPCGMeshSamplingMethod::PoissonSampling:
	{
		// No preparation needed
		break;
	}
	default:
	{
		return true;
	}
	}

	TArray<FPCGTaggedData>& Outputs = InContext->OutputData.TaggedData;
	Context->OutPointData = NewObject<UPCGPointData>();
	Context->OutPointData->TargetActor = Context->SourceComponent->GetOwner();
	Outputs.Emplace_GetRef().Data = Context->OutPointData;

	Context->bDataPrepared = true;
	return true;
}

bool FPCGMeshSamplerElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGMeshSamplerElement::Execute);
	FPCGMeshSamplerContext* Context = static_cast<FPCGMeshSamplerContext*>(InContext);

	check(Context);

	if (!Context->bDataPrepared)
	{
		return true;
	}

	check(Context->OutPointData);

	const UPCGMeshSamplerSettings* Settings = Context->GetInputSettings<UPCGMeshSamplerSettings>();
	check(Settings);

	bool bIsDone = false;
	const bool bEnableTimeSlicing = true;
	const int Seed = Context->GetSeed();

	switch (Settings->SamplingMethod)
	{
	case EPCGMeshSamplingMethod::OnePointPerVertex:
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGMeshSamplerElement::Execute::OnePointPerVertex);

		const TArray<FVector>& Positions = *Context->Positions.List.Get();
		const TArray<FLinearColor>& Colors = *Context->Colors.List.Get();
		const TArray<FVector>& Normals = *Context->Normals.List.Get();

		auto IterationBody = [&Positions, &Colors, &Normals, Settings](int32 Index, FPCGPoint& OutPoint) -> bool
		{
			const FVector& Position = Positions[Index];
			const FLinearColor& Color = Colors[Index];
			const FVector& Normal = Normals[Index];

			OutPoint = FPCGPoint{};
			OutPoint.Transform = FTransform{ FRotationMatrix::MakeFromZ(Normal).Rotator(), Position, FVector{1.0, 1.0, 1.0} };
			OutPoint.Density = Settings->bUseRedAsDensity ? Color.R : 1.0f;
			OutPoint.Color = Color;

			UPCGBlueprintHelpers::SetSeedFromPosition(OutPoint);

			return true;
		};

		bIsDone = FPCGAsync::AsyncProcessing<FPCGPoint>(&Context->AsyncState, Context->Iterations, Context->OutPointData->GetMutablePoints(), IterationBody, bEnableTimeSlicing);
		break;
	}
	case EPCGMeshSamplingMethod::OnePointPerTriangle:
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGMeshSamplerElement::Execute::OnePointPerTriangle);

		const TArray<int32>& TriangleIds = *Context->TriangleIds.List.Get();

		auto IterationBody = [DynamicMesh = Context->DynamicMesh, &TriangleIds](int32 Index, FPCGPoint& OutPoint) -> bool
		{
			const int32 TriangleId = TriangleIds[Index];

			FVector Vertex1{}, Vertex2{}, Vertex3{};
			bool bIsValidTriangle;

			UGeometryScriptLibrary_MeshQueryFunctions::GetTrianglePositions(DynamicMesh, TriangleId, bIsValidTriangle, Vertex1, Vertex2, Vertex3);
			const FVector Normal = UGeometryScriptLibrary_MeshQueryFunctions::GetTriangleFaceNormal(DynamicMesh, TriangleId, bIsValidTriangle);

			const FVector Position = (Vertex1 + Vertex2 + Vertex3) / 3.0;

			OutPoint = FPCGPoint{};
			OutPoint.Transform = FTransform{ FRotationMatrix::MakeFromZ(Normal).Rotator(), Position, FVector{1.0, 1.0, 1.0} };
			OutPoint.Density = 1.0f;

			UPCGBlueprintHelpers::SetSeedFromPosition(OutPoint);

			return true;
		};

		bIsDone = FPCGAsync::AsyncProcessing<FPCGPoint>(&Context->AsyncState, Context->Iterations, Context->OutPointData->GetMutablePoints(), IterationBody, bEnableTimeSlicing);
		break;
	}
	case EPCGMeshSamplingMethod::PoissonSampling:
	{
		// For Poisson sampling, we are calling a "all-in-one" function, where we don't have control for timeslicing.
		// Since Poisson sampling can be expensive (depending on the radius used), we will do the sampling in a future, put this task to sleep,
		// and wait for the sampling to wake us up.
		if (!Context->SamplingFuture.IsValid())
		{
			// Put the task asleep
			Context->bIsPaused = true;
			Context->SamplingProgess = MakeUnique<FProgressCancel>();
			Context->SamplingProgess->CancelF = [Context]() -> bool { return Context->StopSampling; };

			auto SamplingFuture = [Settings, Context, Seed]() -> bool
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FPCGMeshSamplerElement::Execute::PoissonSampling);

				UE::Geometry::FMeshSurfacePointSampling PointSampling;
				PointSampling.SampleRadius = Settings->SamplingOptions.SamplingRadius;
				PointSampling.MaxSamples = Settings->SamplingOptions.MaxNumSamples;
				PointSampling.RandomSeed = PCGHelpers::ComputeSeed(Seed, Settings->SamplingOptions.RandomSeed);
				PointSampling.SubSampleDensity = Settings->SamplingOptions.SubSampleDensity;

				if (Settings->NonUniformSamplingOptions.MaxSamplingRadius > PointSampling.SampleRadius)
				{
					PointSampling.MaxSampleRadius = Settings->NonUniformSamplingOptions.MaxSamplingRadius;
					PointSampling.SizeDistribution = UE::Geometry::FMeshSurfacePointSampling::ESizeDistribution(static_cast<int>(Settings->NonUniformSamplingOptions.SizeDistribution));
					PointSampling.SizeDistributionPower = FMath::Clamp(Settings->NonUniformSamplingOptions.SizeDistributionPower, 1.0, 10.0);
				}

				PointSampling.ComputePoissonSampling(Context->DynamicMesh->GetMeshRef(), Context->SamplingProgess.Get());

				if (Context->StopSampling)
				{
					return true;
				}

				TArray<FPCGPoint>& Points = Context->OutPointData->GetMutablePoints();
				Points.Reserve(PointSampling.Samples.Num());

				int Count = 0;
				for (UE::Geometry::FFrame3d& Sample : PointSampling.Samples)
				{
					// Avoid to check too many times
					constexpr int CancelledCheckNum = 25;
					if (++Count == CancelledCheckNum)
					{
						Count = 0;
						if (Context->StopSampling)
						{
							return true;
						}
					}

					FPCGPoint& OutPoint = Points.Emplace_GetRef();
					OutPoint.Transform = Sample.ToTransform();
					OutPoint.Density = 1.0f;

					UPCGBlueprintHelpers::SetSeedFromPosition(OutPoint);
				}

				// Unpause the task
				Context->bIsPaused = false;
				return true;
			};

			Context->SamplingFuture = Async(EAsyncExecution::ThreadPool, std::move(SamplingFuture));
		}

		bIsDone = Context->SamplingFuture.IsReady();
		if (bIsDone)
		{
			Context->SamplingFuture.Reset();
		}

		break;
	}
	default:
	{
		return true;
	}
	}

	return bIsDone;
}

#undef LOCTEXT_NAMESPACE
