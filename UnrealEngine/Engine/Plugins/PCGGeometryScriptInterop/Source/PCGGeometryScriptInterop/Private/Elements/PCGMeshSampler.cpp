// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGMeshSampler.h"

#include "PCGComponent.h"
#include "PCGModule.h"
#include "PCGPin.h"
#include "Data/PCGPointData.h"
#include "Helpers/PCGAsync.h"
#include "Helpers/PCGBlueprintHelpers.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttributeTpl.h"

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

void UPCGMeshSamplerSettings::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	if (!StaticMeshPath_DEPRECATED.IsNull())
	{
		StaticMesh = StaticMeshPath_DEPRECATED;
		StaticMeshPath_DEPRECATED.Reset();
	}

	if (bUseRedAsDensity_DEPRECATED)
	{
		// It was only available for one point per vertex before. Keep that.
		bUseColorChannelAsDensity = (SamplingMethod == EPCGMeshSamplingMethod::OnePointPerVertex);
		ColorChannelAsDensity = EPCGColorChannel::Red;
		bUseRedAsDensity_DEPRECATED = false;
	}
#endif
}

TArray<FPCGPinProperties> UPCGMeshSamplerSettings::InputPinProperties() const
{
	return TArray<FPCGPinProperties>{};
}

TArray<FPCGPinProperties> UPCGMeshSamplerSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;
	Properties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point, /*bInAllowMultipleConnections =*/ false, /*bAllowMultipleData =*/ false);
	return Properties;
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
}

void FPCGMeshSamplerContext::AddExtraStructReferencedObjects(FReferenceCollector& Collector)
{
	if (DynamicMesh)
	{
		Collector.AddReferencedObject(DynamicMesh);
	}
}

FPCGContext* FPCGMeshSamplerElement::CreateContext()
{
	return new FPCGMeshSamplerContext();
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

	const TSoftObjectPtr<UStaticMesh> StaticMeshPtr = Settings->StaticMesh;
	if (StaticMeshPtr.IsNull())
	{
		return true;
	}

	// 1. Request load for mesh. Return false if we need to wait, otherwise continue.
	if (!Context->WasLoadRequested())
	{
		if (!Context->RequestResourceLoad(Context, { StaticMeshPtr.ToSoftObjectPath() }, !Settings->bSynchronousLoad))
		{
			return false;
		}
	}

	UStaticMesh* StaticMesh = StaticMeshPtr.Get();

	if (!StaticMesh)
	{
		PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("MeshDoesNotExist", "Provided static mesh does not exist or could not be loaded: '{0}'"), FText::FromString(StaticMeshPtr.ToString())));
		return true;
	}

	Context->DynamicMesh = NewObject<UDynamicMesh>();

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

	// It's not clear how to compute UVs for Vertices as they are part of multiple triangles. So disable for this mode. Same for triangle ids.
	if (Settings->SamplingMethod != EPCGMeshSamplingMethod::OnePointPerVertex)
	{
		if (Settings->bExtractUVAsAttribute)
		{
			Context->UVAttribute = Context->OutPointData->Metadata->CreateAttribute<FVector2D>(Settings->UVAttributeName, FVector2D::ZeroVector, /*bAllowsInterpolation=*/true, /*bOverrideParent=*/true);
			if (!Context->UVAttribute)
			{
				PCGE_LOG(Warning, GraphAndLog, FText::Format(LOCTEXT("AttributeUVFailed", "Failed to create attribute {0} for UVs. UVs won't be computed"), FText::FromName(Settings->UVAttributeName)));
			}
		}

		if (Settings->bOutputTriangleIds)
		{
			Context->TriangleIdAttribute = Context->OutPointData->Metadata->CreateAttribute<int32>(Settings->TriangleIdAttributeName, 0, /*bAllowsInterpolation=*/false, /*bOverrideParent=*/true);
			if (!Context->TriangleIdAttribute)
			{
				PCGE_LOG(Warning, GraphAndLog, FText::Format(LOCTEXT("AttributeTriangleIdFailed", "Failed to create attribute {0} for triangles ids. Triangle Ids won't be output"), FText::FromName(Settings->TriangleIdAttributeName)));
			}
		}
	}

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

	auto SetUVValueAndTriangleId = [Context, Settings](int32 TriangleId, const FVector& BarycentricCoord, FPCGPoint& OutPoint)
	{
		if (Context->UVAttribute)
		{
			bool bHasValidUVs = false;
			FVector2D InterpolatedUV{};
			UGeometryScriptLibrary_MeshQueryFunctions::GetInterpolatedTriangleUV(Context->DynamicMesh, /*UVSetIndex=*/ Settings->UVChannel, TriangleId, BarycentricCoord, bHasValidUVs, InterpolatedUV);
			if (bHasValidUVs)
			{
				Context->OutPointData->Metadata->InitializeOnSet(OutPoint.MetadataEntry);
				Context->UVAttribute->SetValue(OutPoint.MetadataEntry, InterpolatedUV);
			}
		}

		if (Context->TriangleIdAttribute)
		{
			Context->OutPointData->Metadata->InitializeOnSet(OutPoint.MetadataEntry);
			Context->TriangleIdAttribute->SetValue(OutPoint.MetadataEntry, TriangleId);
		}
	};

	// Preparing the set function to extract the color to density.
	auto SetPointDensityTo1 = [](const FLinearColor&, FPCGPoint& OutPoint){ OutPoint.Density = 1.0f; };
	auto SetPointDensityToRed = [](const FLinearColor& Color, FPCGPoint& OutPoint){ OutPoint.Density = Color.R; };
	auto SetPointDensityToGreen = [](const FLinearColor& Color, FPCGPoint& OutPoint){ OutPoint.Density = Color.G; };
	auto SetPointDensityToBlue = [](const FLinearColor& Color, FPCGPoint& OutPoint){ OutPoint.Density = Color.B; };
	auto SetPointDensityToAlpha = [](const FLinearColor& Color, FPCGPoint& OutPoint){ OutPoint.Density = Color.A; };

	// Store it in a function pointer.
	void(*SetPointDensityPtr)(const FLinearColor&, FPCGPoint&) = SetPointDensityTo1;

	if (Settings->bUseColorChannelAsDensity)
	{
		switch (Settings->ColorChannelAsDensity)
		{
		case EPCGColorChannel::Red:
			SetPointDensityPtr = SetPointDensityToRed;
			break;
		case EPCGColorChannel::Green:
			SetPointDensityPtr = SetPointDensityToGreen;
			break;
		case EPCGColorChannel::Blue:
			SetPointDensityPtr = SetPointDensityToBlue;
			break;
		case EPCGColorChannel::Alpha:
			SetPointDensityPtr = SetPointDensityToAlpha;
			break;
		default:
			checkNoEntry();
			break;
		}
	}

	auto SetPointColorAndDensity = [Context, Settings, SetPointDensityPtr](int32 TriangleId, const FVector& BarycentricCoord, FPCGPoint& OutPoint)
	{
		FLinearColor Color;
		bool bValidVertexColor = false;
		UGeometryScriptLibrary_MeshQueryFunctions::GetInterpolatedTriangleVertexColor(Context->DynamicMesh, TriangleId, BarycentricCoord, FLinearColor::White, bValidVertexColor, Color);
		if (bValidVertexColor)
		{
			OutPoint.Color = Color;
			SetPointDensityPtr(Color, OutPoint);
		}
		else
		{
			OutPoint.Density = 1.0f;
		}
	};

	switch (Settings->SamplingMethod)
	{
	case EPCGMeshSamplingMethod::OnePointPerVertex:
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGMeshSamplerElement::Execute::OnePointPerVertex);

		const TArray<FVector>& Positions = *Context->Positions.List.Get();
		const TArray<FLinearColor>& Colors = *Context->Colors.List.Get();
		const TArray<FVector>& Normals = *Context->Normals.List.Get();

		auto IterationBody = [&Positions, &Colors, &Normals, Settings, Context, SetPointDensityPtr](int32 Index, FPCGPoint& OutPoint) -> bool
		{
			const FVector& Position = Positions[Index];
			const FLinearColor& Color = Colors[Index];
			const FVector& Normal = Normals[Index];

			OutPoint = FPCGPoint{};
			OutPoint.Transform = FTransform{ FRotationMatrix::MakeFromZ(Normal).Rotator(), Position, FVector{1.0, 1.0, 1.0} };
			OutPoint.Color = Color;
			OutPoint.Steepness = Settings->PointSteepness;

			SetPointDensityPtr(Color, OutPoint);

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

		auto IterationBody = [DynamicMesh = Context->DynamicMesh, PointSteepness = Settings->PointSteepness, &TriangleIds, SetUVValueAndTriangleId, SetPointColorAndDensity](int32 Index, FPCGPoint& OutPoint) -> bool
		{
			const int32 TriangleId = TriangleIds[Index];

			FVector Vertex1{}, Vertex2{}, Vertex3{};
			bool bIsValidTriangle;

			UGeometryScriptLibrary_MeshQueryFunctions::GetTrianglePositions(DynamicMesh, TriangleId, bIsValidTriangle, Vertex1, Vertex2, Vertex3);
			const FVector Normal = UGeometryScriptLibrary_MeshQueryFunctions::GetTriangleFaceNormal(DynamicMesh, TriangleId, bIsValidTriangle);
			const FVector Position = (Vertex1 + Vertex2 + Vertex3) / 3.0;

			OutPoint = FPCGPoint{};
			OutPoint.Transform = FTransform{ FRotationMatrix::MakeFromZ(Normal).Rotator(), Position, FVector{1.0, 1.0, 1.0} };
			OutPoint.Steepness = PointSteepness;

			FVector Dummy1, Dummy2, Dummy3;
			FVector BarycentricCoord;
			bool bIsValid = false;
			UGeometryScriptLibrary_MeshQueryFunctions::ComputeTriangleBarycentricCoords(DynamicMesh, TriangleId, bIsValid, Position, Dummy1, Dummy2, Dummy3, BarycentricCoord);
			
			SetPointColorAndDensity(TriangleId, BarycentricCoord, OutPoint);
			SetUVValueAndTriangleId(TriangleId, BarycentricCoord, OutPoint);

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

			auto SamplingFuture = [Settings, Context, Seed, SetUVValueAndTriangleId, SetPointColorAndDensity]() -> bool
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

				PointSampling.bComputeBarycentrics = true;

				PointSampling.ComputePoissonSampling(Context->DynamicMesh->GetMeshRef(), Context->SamplingProgess.Get());

				if (Context->StopSampling)
				{
					return true;
				}

				TArray<FPCGPoint>& Points = Context->OutPointData->GetMutablePoints();
				Points.Reserve(PointSampling.Samples.Num());

				int Count = 0;
				for (int32 i = 0; i < PointSampling.Samples.Num(); ++i)
				{
					UE::Geometry::FFrame3d& Sample = PointSampling.Samples[i];
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

					const int32 TriangleId = PointSampling.TriangleIDs[i];
					const FVector BarycentricCoords = PointSampling.BarycentricCoords[i];

					FPCGPoint& OutPoint = Points.Emplace_GetRef();
					OutPoint.Transform = Sample.ToTransform();
					OutPoint.Steepness = Settings->PointSteepness;

					SetPointColorAndDensity(TriangleId, BarycentricCoords, OutPoint);
					SetUVValueAndTriangleId(TriangleId, BarycentricCoords, OutPoint);

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
