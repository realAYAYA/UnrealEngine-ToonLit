// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGSurfaceSampler.h"

#include "PCGCommon.h"
#include "PCGComponent.h"
#include "PCGCustomVersion.h"
#include "PCGEdge.h"
#include "PCGGraph.h"
#include "PCGPin.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Helpers/PCGAsync.h"
#include "Helpers/PCGHelpers.h"
#include "Helpers/PCGSettingsHelpers.h"

#include "HAL/UnrealMemory.h"
#include "Math/RandomStream.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGSurfaceSampler)

#define LOCTEXT_NAMESPACE "PCGSurfaceSamplerElement"

namespace PCGSurfaceSamplerConstants
{
	const FName SurfaceLabel = TEXT("Surface");
	const FName BoundingShapeLabel = TEXT("Bounding Shape");
}

namespace PCGSurfaceSampler
{
	bool FSurfaceSamplerSettings::Initialize(const UPCGSurfaceSamplerSettings* InSettings, FPCGContext* Context, const FBox& InputBounds)
	{
		if (!Context)
		{
			return false;
		}

		Settings = InSettings;

		if (Settings)
		{
			// Compute used values
			PointsPerSquaredMeter = Settings->PointsPerSquaredMeter;
			PointExtents = Settings->PointExtents;
			Looseness = Settings->Looseness;
			bApplyDensityToPoints = Settings->bApplyDensityToPoints;
			PointSteepness = Settings->PointSteepness;
#if WITH_EDITOR
			bKeepZeroDensityPoints = Settings->bKeepZeroDensityPoints;
#endif
		}

		Seed = Context->GetSeed();

		// Conceptually, we will break down the surface bounds in a N x M grid
		InterstitialDistance = PointExtents * 2;
		InnerCellSize = InterstitialDistance * Looseness;
		CellSize = InterstitialDistance + InnerCellSize;
		check(CellSize.X > 0 && CellSize.Y > 0);

		// By using scaled indices in the world, we can easily make this process deterministic
		CellMinX = FMath::CeilToInt((InputBounds.Min.X) / CellSize.X);
		CellMaxX = FMath::FloorToInt((InputBounds.Max.X) / CellSize.X);
		CellMinY = FMath::CeilToInt((InputBounds.Min.Y) / CellSize.Y);
		CellMaxY = FMath::FloorToInt((InputBounds.Max.Y) / CellSize.Y);

		{
			const int64 CellCountX = 1 + CellMaxX - CellMinX;
			const int64 CellCountY = 1 + CellMaxY - CellMinY;
			if (CellCountX <= 0 || CellCountY <= 0)
			{
				if (Context)
				{
					PCGE_LOG_C(Verbose, LogOnly, Context, FText::Format(LOCTEXT("InvalidCellBounds", "Skipped - invalid cell bounds({0} x {1})"), CellCountX, CellCountY));
				}

				return false;
			}

			const int64 CellCount64 = CellCountX * CellCountY;
			if (CellCount64 <= 0 || 
				CellCount64 >= MAX_int32 ||
				(PCGFeatureSwitches::CVarCheckSamplerMemory.GetValueOnAnyThread() && FPlatformMemory::GetStats().AvailablePhysical < sizeof(FPCGPoint) * CellCount64))
			{
				if (Context)
				{
					PCGE_LOG_C(Error, GraphAndLog, Context, FText::Format(LOCTEXT("TooManyPoints", "Skipped - tried to generate too many points {0}"), CellCount64));
				}

				return false;
			}

			CellCount = static_cast<int32>(CellCount64);
		}

		check(CellCount > 0);

		const FVector::FReal InvSquaredMeterUnits = 1.0 / (100.0 * 100.0);
		TargetPointCount = (InputBounds.Max.X - InputBounds.Min.X) * (InputBounds.Max.Y - InputBounds.Min.Y) * PointsPerSquaredMeter * InvSquaredMeterUnits;

		if (TargetPointCount == 0)
		{
			if (Context)
			{
				PCGE_LOG_C(Verbose, LogOnly, Context, LOCTEXT("NoPointsFromDensity", "Skipped - density yields no points"));
			}
			
			return false;
		}
		else if (TargetPointCount > CellCount)
		{
			TargetPointCount = CellCount;
		}

		Ratio = TargetPointCount / (FVector::FReal)CellCount;

		InputBoundsMinZ = InputBounds.Min.Z;
		InputBoundsMaxZ = InputBounds.Max.Z;

		return true;
	}

	FIntVector2 FSurfaceSamplerSettings::ComputeCellIndices(int32 Index) const
	{
		check(Index >= 0 && Index < CellCount);
		const int32 CellCountX = 1 + CellMaxX - CellMinX;

		return FIntVector2(CellMinX + (Index % CellCountX), CellMinY + (Index / CellCountX));
	}

	UPCGPointData* SampleSurface(FPCGContext* Context, const UPCGSpatialData* InSurface, const UPCGSpatialData* InBoundingShape, const FSurfaceSamplerSettings& LoopData)
	{
		// We don't support time slicing here
		if (LoopData.bEnableTimeSlicing)
		{
			PCGE_LOG_C(Error, LogOnly, Context, LOCTEXT("MissingOutputPointData", "An output point data must be provided to support sampling with time slicing"));
			return nullptr;
		}

		UPCGPointData* SampledData = NewObject<UPCGPointData>();
		SampledData->InitializeFromData(InSurface);

		SampleSurface(Context, InSurface, InBoundingShape, LoopData, SampledData);

		return SampledData;
	}

	bool SampleSurface(FPCGContext* Context, const UPCGSpatialData* InSurface, const UPCGSpatialData* InBoundingShape, const FSurfaceSamplerSettings& LoopData, UPCGPointData* SampledData)
	{
		check(InSurface);

		TArray<FPCGPoint>& SampledPoints = SampledData->GetMutablePoints();

		FPCGProjectionParams ProjectionParams{};

		// Drop points slightly by an epsilon otherwise point can be culled. If the sampler has a volume connected as the Bounding Shape,
		// the volume will call through to PCGHelpers::IsInsideBounds() which is a one sided test and points at the top of the volume
		// will fail it. TODO perhaps the one-sided check can be isolated to component-bounds
		const FVector::FReal ZMultiplier = 1.0 - UE_DOUBLE_SMALL_NUMBER;
		// Try to use a multiplier instead of a simply offset to combat loss of precision in floats. However if MaxZ is very small,
		// then multiplier will not work, so just use an offset.
		FVector::FReal SampleZ = (FMath::Abs(LoopData.InputBoundsMaxZ) > UE_DOUBLE_SMALL_NUMBER) ? LoopData.InputBoundsMaxZ * ZMultiplier : -UE_DOUBLE_SMALL_NUMBER;
		// Make sure we're still in bounds though!
		SampleZ = FMath::Max(SampleZ, LoopData.InputBoundsMinZ);

		// Cache pointer ahead of time to avoid dereferencing object pointer which does access tracking and supports lazy loading, and can come with substantial
		// overhead (add trace marker to FObjectPtr::Get to see).
		UPCGMetadata* OutMetadata = SampledData->Metadata.Get();

		auto AsyncProcessFunc = [&LoopData, SampledData, InBoundingShape, InSurface, &ProjectionParams, SampleZ, OutMetadata](int32 Index, FPCGPoint& OutPoint)
		{
			const FIntVector2 Indices = LoopData.ComputeCellIndices(Index);

			const FVector::FReal CurrentX = Indices.X * LoopData.CellSize.X;
			const FVector::FReal CurrentY = Indices.Y * LoopData.CellSize.Y;
			const FVector InnerCellSize = LoopData.InnerCellSize;

			FRandomStream RandomSource(PCGHelpers::ComputeSeed(LoopData.Seed, Indices.X, Indices.Y));
			float Chance = RandomSource.FRand();

			const float Ratio = LoopData.Ratio;

			if (Chance >= Ratio)
			{
				return false;
			}

			const float RandX = RandomSource.FRand();
			const float RandY = RandomSource.FRand();

			const FVector TentativeLocation = FVector(CurrentX + RandX * InnerCellSize.X, CurrentY + RandY * InnerCellSize.Y, SampleZ);
			const FBox LocalBound(-LoopData.PointExtents, LoopData.PointExtents);

			// The output at this point is not initialized
			OutPoint = FPCGPoint();

			// Firstly project onto elected generating shape to move to final position.
			if (!InSurface->ProjectPoint(FTransform(TentativeLocation), LocalBound, ProjectionParams, OutPoint, OutMetadata))
			{
				return false;
			}

			// Now run gauntlet of shape network (if there is one) to accept or reject the point.
			if (InBoundingShape)
			{
				FPCGPoint BoundingShapeSample;
#if WITH_EDITOR
				if (!InBoundingShape->SamplePoint(OutPoint.Transform, OutPoint.GetLocalBounds(), BoundingShapeSample, nullptr) && !LoopData.bKeepZeroDensityPoints)
#else
				if (!InBoundingShape->SamplePoint(OutPoint.Transform, OutPoint.GetLocalBounds(), BoundingShapeSample, nullptr))
#endif
				{
					return false;
				}

				// Produce smooth density field
				OutPoint.Density *= BoundingShapeSample.Density;
			}

			// Apply final parameters on the point
			OutPoint.SetExtents(LoopData.PointExtents);
			OutPoint.Density *= (LoopData.bApplyDensityToPoints ? ((Ratio - Chance) / Ratio) : 1.0f);
			OutPoint.Steepness = LoopData.PointSteepness;
			OutPoint.Seed = RandomSource.GetCurrentSeed();

			return true;
		};

		bool bAsyncDone = FPCGAsync::AsyncProcessing<FPCGPoint>(Context ? &Context->AsyncState : nullptr, LoopData.CellCount, SampledPoints, AsyncProcessFunc, LoopData.bEnableTimeSlicing);

		if (Context && bAsyncDone)
		{
			PCGE_LOG_C(Verbose, LogOnly, Context, FText::Format(LOCTEXT("GenerationInfo", "Generated {0} points in {1} cells"), SampledPoints.Num(), LoopData.CellCount));
		}

		return bAsyncDone;
	}

#if WITH_EDITOR
	static bool IsPinOnlyConnectedToInputNode(UPCGPin* DownstreamPin, UPCGNode* GraphInputNode)
	{
		if (DownstreamPin->Edges.Num() == 1)
		{
			const UPCGEdge* Edge = DownstreamPin->Edges[0];
			const UPCGNode* UpstreamNode = (Edge && Edge->InputPin) ? Edge->InputPin->Node : nullptr;
			const bool bConnectedToInputNode = UpstreamNode && (GraphInputNode == UpstreamNode);
			const bool bConnectedToInputPin = Edge && (Edge->InputPin->Properties.Label == FName(TEXT("In")) || Edge->InputPin->Properties.Label == FName(TEXT("Input")));
			return bConnectedToInputNode && bConnectedToInputPin;
		}

		return false;
	}
#endif
}

UPCGSurfaceSamplerSettings::UPCGSurfaceSamplerSettings()
{
	bUseSeed = true;
}

#if WITH_EDITOR
FText UPCGSurfaceSamplerSettings::GetNodeTooltipText() const
{
	return LOCTEXT("SurfaceSamplerNodeTooltip", "Generates points in two dimensional domain that sample the Surface input and lie within the Bounding Shape input.");
}
#endif

TArray<FPCGPinProperties> UPCGSurfaceSamplerSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGSurfaceSamplerConstants::SurfaceLabel, EPCGDataType::Surface, /*bAllowMultipleConnections=*/true, /*bAllowMultipleData=*/true, LOCTEXT("SurfaceSamplerSurfacePinTooltip",
		"The surface to sample with points. Points will be generated in the two dimensional footprint of the combined bounds of the Surface and the Bounding Shape (if any) "
		"and then projected onto this surface. If this input is omitted then the network of shapes connected to the Bounding Shape pin will be inspected for a surface "
		"shape to use to project the points onto."
	));
	// Only one connection allowed, user can union multiple shapes
	PinProperties.Emplace(PCGSurfaceSamplerConstants::BoundingShapeLabel, EPCGDataType::Spatial, /*bInAllowMultipleConnections=*/false, /*bAllowMultipleData=*/false, LOCTEXT("SurfaceSamplerBoundingShapePinTooltip",
		"All sampled points must be contained within this shape. If this input is omitted then bounds will be taken from the actor so that points are contained within actor bounds. "
		"The Unbounded property disables this and instead generates over the entire bounds of Surface."
	));

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGSurfaceSamplerSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point);

	return PinProperties;
}

void UPCGSurfaceSamplerSettings::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	if (PointRadius_DEPRECATED != 0)
	{
		PointExtents = FVector(PointRadius_DEPRECATED);
		PointRadius_DEPRECATED = 0;
	}
#endif // WITH_EDITOR
}

#if WITH_EDITOR
bool UPCGSurfaceSamplerSettings::IsPinUsedByNodeExecution(const UPCGPin* InPin) const
{
	return !bUnbounded || InPin->Properties.Label != PCGSurfaceSamplerConstants::BoundingShapeLabel;
}
#endif

FPCGSurfaceSamplerContext::~FPCGSurfaceSamplerContext()
{
	if (bUnionDataCreated && BoundingShapeSpatialInput)
	{
		// We created the data so can cast away constness
		UPCGSpatialData* BoundingShapeMutable = const_cast<UPCGSpatialData*>(BoundingShapeSpatialInput);
		BoundingShapeMutable->RemoveFromRoot();
		BoundingShapeMutable->MarkAsGarbage();

		BoundingShapeSpatialInput = nullptr;
	}
}

FPCGElementPtr UPCGSurfaceSamplerSettings::CreateElement() const
{
	return MakeShared<FPCGSurfaceSamplerElement>();
}

FPCGContext* FPCGSurfaceSamplerElement::Initialize(const FPCGDataCollection& InInputData, TWeakObjectPtr<UPCGComponent> InSourceComponent, const UPCGNode* InNode)
{
	FPCGSurfaceSamplerContext* Context = new FPCGSurfaceSamplerContext();
	Context->InputData = InInputData;
	Context->SourceComponent = InSourceComponent;
	Context->Node = InNode;

	return Context;
}

bool FPCGSurfaceSamplerElement::AddGeneratingShapesToContext(FPCGSurfaceSamplerContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGSurfaceSamplerElement::SetupContextForGeneratingShape);

	check(InContext);
	const UPCGSurfaceSamplerSettings* Settings = InContext->GetInputSettings<UPCGSurfaceSamplerSettings>();
	check(Settings);

	TArray<FPCGTaggedData>& Outputs = InContext->OutputData.TaggedData;

	// Early out on invalid settings
	// TODO: we could compute an approximate radius based on the points per squared meters if that's useful
	const FVector& PointExtents = Settings->PointExtents;
	if (PointExtents.X <= 0 || PointExtents.Y <= 0)
	{
		PCGE_LOG_C(Warning, GraphAndLog, InContext, LOCTEXT("SkippedInvalidPointExtents", "Skipped - Invalid point extents"));
		return false;
	}

	TArray<FPCGTaggedData> SurfaceInputs = InContext->InputData.GetInputsByPin(PCGSurfaceSamplerConstants::SurfaceLabel);

	// Construct a list of shapes to generate samples from. Prefer to get these directly from the first input pin.
	for (FPCGTaggedData& TaggedData : SurfaceInputs)
	{
		if (const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(TaggedData.Data))
		{
			// Find a concrete shape for sampling. Prefer a 2D surface if we can find one.
			if (const UPCGSpatialData* SurfaceData = SpatialData->FindShapeFromNetwork(/*InDimension=*/2))
			{
				InContext->GeneratingShapes.Add(SurfaceData);
				Outputs.Add(TaggedData);
			}
			else if (const UPCGSpatialData* ConcreteData = SpatialData->FindFirstConcreteShapeFromNetwork())
			{
				// Alternatively surface-sample any concrete data - can be used to sprinkle samples down onto shapes like volumes.
				// Searching like this allows the user to plonk in any composite network and it will often find the shape of interest.
				// A potential extension would be to find all (unique?) concrete shapes and use all of them rather than just the first.
				InContext->GeneratingShapes.Add(ConcreteData);
				Outputs.Add(TaggedData);
			}
		}
	}

	// Grab the Bounding Shape input if there is one.
	TArray<FPCGTaggedData> BoundingShapeInputs = InContext->InputData.GetInputsByPin(PCGSurfaceSamplerConstants::BoundingShapeLabel);

	if (!Settings->bUnbounded)
	{
		InContext->BoundingShapeSpatialInput = InContext->InputData.GetSpatialUnionOfInputsByPin(PCGSurfaceSamplerConstants::BoundingShapeLabel, InContext->bUnionDataCreated);

		// Root dynamically created data to keep it safe from GC
		if (InContext->bUnionDataCreated && InContext->BoundingShapeSpatialInput)
		{
			const_cast<UPCGSpatialData*>(InContext->BoundingShapeSpatialInput)->AddToRoot();
		}

		// Fallback to getting bounds from actor but only if we actually have some inputs
		if (!InContext->BoundingShapeSpatialInput && InContext->SourceComponent.IsValid() && !InContext->GeneratingShapes.IsEmpty())
		{
			InContext->BoundingShapeSpatialInput = Cast<UPCGSpatialData>(InContext->SourceComponent->GetActorPCGData());
		}
	}
	else if (BoundingShapeInputs.Num() > 0)
	{
		PCGE_LOG_C(Verbose, LogOnly, InContext, LOCTEXT("BoundsIgnored", "The bounds of the Bounding Shape input pin will be ignored because the Unbounded option is enabled."));
	}

	if (InContext->BoundingShapeSpatialInput)
	{
		InContext->BoundingShapeBounds = InContext->BoundingShapeSpatialInput->GetBounds();
	}

	// If no shapes were obtained from the first input pin, try to find a shape to sample from nodes connected to the second pin.
	if (InContext->GeneratingShapes.Num() == 0 && InContext->BoundingShapeSpatialInput)
	{
		if (const UPCGSpatialData* GeneratorFromBoundingShapeInput = InContext->BoundingShapeSpatialInput->FindShapeFromNetwork(/*InDimension=*/2))
		{
			InContext->GeneratingShapes.Add(GeneratorFromBoundingShapeInput);

			// If there was a bounding shape input, use it as the starting point to get the tags
			if (BoundingShapeInputs.Num() > 0)
			{
				Outputs.Add(BoundingShapeInputs[0]);
			}
			else
			{
				Outputs.Emplace();
			}
		}
	}

	// Warn if something is connected but no shape could be obtained for sampling
	if (InContext->GeneratingShapes.Num() == 0 && (BoundingShapeInputs.Num() > 0 || SurfaceInputs.Num() > 0))
	{
		PCGE_LOG_C(Warning, GraphAndLog, InContext, LOCTEXT("NoGenerator", "No Surface input was provided, and no surface could be found in the Bounding Shape input for sampling. Connect the surface to be sampled to the Surface input."));
		return false;
	}

	return true;
}

void FPCGSurfaceSamplerElement::AllocateOutputs(FPCGSurfaceSamplerContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGSurfaceSamplerElement::AllocateOutputs);

	check(InContext);
	TArray<FPCGTaggedData>& Outputs = InContext->OutputData.TaggedData;

	const UPCGSurfaceSamplerSettings* Settings = InContext->GetInputSettings<UPCGSurfaceSamplerSettings>();
	check(Settings);

	int32 GeneratingShapeIndex = 0;
	while (GeneratingShapeIndex < InContext->GeneratingShapes.Num())
	{
		// If we have generating shape inputs, use them
		const UPCGSpatialData* GeneratingShape = InContext->GeneratingShapes[GeneratingShapeIndex];
		check(GeneratingShape);

		// Calculate the intersection of bounds of the provided inputs
		FBox InputBounds = FBox(EForceInit::ForceInit);

		if (GeneratingShape->IsBounded())
		{
			InputBounds = GeneratingShape->GetBounds();

			if (InContext->BoundingShapeBounds.IsValid)
			{
				InputBounds = PCGHelpers::OverlapBounds(InputBounds, InContext->BoundingShapeBounds);
			}
		}
		else
		{
			InputBounds = InContext->BoundingShapeBounds;
		}

		PCGSurfaceSampler::FSurfaceSamplerSettings TentativeLoopData = PCGSurfaceSampler::FSurfaceSamplerSettings{};
		TentativeLoopData.bEnableTimeSlicing = true;
		if (!InputBounds.IsValid || !TentativeLoopData.Initialize(Settings, InContext, InputBounds))
		{
			if (!GeneratingShape->IsBounded())
			{
				// Some inputs are unable to provide bounds, like the WorldRayHit, in which case the user must provide bounds.
				PCGE_LOG_C(Warning, GraphAndLog, InContext, LOCTEXT("CouldNotObtainInputBounds", "Input data is not bounded, so bounds must be provided for sampling. Consider providing a Bounding Shape input."));
			}
			else if(!InputBounds.IsValid)
			{
				PCGE_LOG_C(Verbose, LogOnly, InContext, LOCTEXT("InvalidSamplingBounds", "Final sampling bounds is invalid/zero-sized."));
			}

			Outputs.RemoveAt(GeneratingShapeIndex);
			InContext->GeneratingShapes.RemoveAt(GeneratingShapeIndex);
			continue;
		}
		
		InContext->LoopData.Emplace(std::move(TentativeLoopData));

		UPCGPointData* NewPointData = NewObject<UPCGPointData>();
		NewPointData->InitializeFromData(GeneratingShape);

		// Directly set in the output, will allow for the data to be rooted if we are postponed.
		Outputs[GeneratingShapeIndex].Data = NewPointData;
		++GeneratingShapeIndex;
	}
}

bool FPCGSurfaceSamplerElement::PrepareDataInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGSurfaceSamplerElement::PrepareData);
	FPCGSurfaceSamplerContext* Context = static_cast<FPCGSurfaceSamplerContext*>(InContext);

	check(Context);

	if (AddGeneratingShapesToContext(Context))
	{
		AllocateOutputs(Context);
		Context->bDataPrepared = true;
	}

	return true;
}

bool FPCGSurfaceSamplerElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGSurfaceSamplerElement::Execute);
	FPCGSurfaceSamplerContext* Context = static_cast<FPCGSurfaceSamplerContext*>(InContext);

	check(Context);

	// Prepare data failed, no need to execute
	if (!Context->bDataPrepared)
	{
		return true;
	}

	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	while (Context->CurrentGeneratingShape < Context->GeneratingShapes.Num())
	{
		UPCGPointData* PointData = Cast<UPCGPointData>(Outputs[Context->CurrentGeneratingShape].Data);

		if (PointData)
		{
			bool bIsDone = PCGSurfaceSampler::SampleSurface(Context, Context->GeneratingShapes[Context->CurrentGeneratingShape], Context->BoundingShapeSpatialInput, Context->LoopData[Context->CurrentGeneratingShape], PointData);

			if (!bIsDone)
			{
				return false;
			}
		}

		Context->CurrentGeneratingShape++;
	}

	return true;
}

void FPCGSurfaceSamplerElement::GetDependenciesCrc(const FPCGDataCollection& InInput, const UPCGSettings* InSettings, UPCGComponent* InComponent, FPCGCrc& OutCrc) const
{
	FPCGCrc Crc;
	IPCGElement::GetDependenciesCrc(InInput, InSettings, InComponent, Crc);

	if (const UPCGSurfaceSamplerSettings* Settings = Cast<UPCGSurfaceSamplerSettings>(InSettings))
	{
		bool bUnbounded;
		PCGSettingsHelpers::GetOverrideValue(InInput, Settings, GET_MEMBER_NAME_CHECKED(UPCGSurfaceSamplerSettings, bUnbounded), Settings->bUnbounded, bUnbounded);
		const bool bBoundsConnected = InInput.GetInputsByPin(PCGSurfaceSamplerConstants::BoundingShapeLabel).Num() > 0;

		// If we're operating in bounded mode and there is no bounding shape connected then we'll use actor bounds, and therefore take
		// dependency on actor data.
		if (!bUnbounded && !bBoundsConnected && InComponent)
		{
			if (const UPCGData* Data = InComponent->GetActorPCGData())
			{
				Crc.Combine(Data->GetOrComputeCrc(/*bFullDataCrc=*/false));
			}
		}
	}

	OutCrc = Crc;
}

#if WITH_EDITOR
void UPCGSurfaceSamplerSettings::ApplyDeprecationBeforeUpdatePins(UPCGNode* InOutNode, TArray<TObjectPtr<UPCGPin>>& InputPins, TArray<TObjectPtr<UPCGPin>>& OutputPins)
{
	if (DataVersion < FPCGCustomVersion::SplitSamplerNodesInputs && ensure(InOutNode))
	{
		if (InputPins.Num() > 0 && InputPins[0])
		{
			// The node will function the same if we move all connections from "In" to "Bounding Shape". To make this happen, rename "In" to
			// "Bounding Shape" just prior to pin update and the edges will be moved over. In ApplyDeprecation we'll see if we can do better than
			// this baseline functional setup.
			InputPins[0]->Properties.Label = PCGSurfaceSamplerConstants::BoundingShapeLabel;
		}

		// A new params pin was added, migrate the first param connection there if any
		PCGSettingsHelpers::DeprecationBreakOutParamsToNewPin(InOutNode, InputPins, OutputPins);
	}

	Super::ApplyDeprecationBeforeUpdatePins(InOutNode, InputPins, OutputPins);
}

void UPCGSurfaceSamplerSettings::ApplyDeprecation(UPCGNode* InOutNode)
{
	if (DataVersion < FPCGCustomVersion::SplitSamplerNodesInputs && ensure(InOutNode && InOutNode->GetInputPins().Num() >= 2))
	{
		UE_LOG(LogPCG, Log, TEXT("Surface Sampler node migrated from an older version. Review edges on the input pins and then save this graph to upgrade the data."));

		UPCGPin* SurfacePin = InOutNode->GetInputPin(FName(TEXT("Surface")));
		UPCGPin* BoundingShapePin = InOutNode->GetInputPin(FName(TEXT("Bounding Shape")));
		UPCGNode* GraphInputNode = InOutNode->GetGraph() ? InOutNode->GetGraph()->GetInputNode() : nullptr;

		if (SurfacePin && BoundingShapePin && GraphInputNode)
		{
			auto MoveEdgeOnInputNodeToLandscapePin = [InOutNode, GraphInputNode, SurfacePin](UPCGPin* DownstreamPin) {
				// Detect if we're connected to the Input node.
				if (PCGSurfaceSampler::IsPinOnlyConnectedToInputNode(DownstreamPin, GraphInputNode))
				{
					// If we are connected to the Input node, make just a connection from the Surface pin to the Landscape pin and rely on Unbounded setting to provide bounds.
					if (UPCGPin* LandscapePin = GraphInputNode->GetOutputPin(FName(TEXT("Landscape"))))
					{
						DownstreamPin->BreakAllEdges();

						LandscapePin->AddEdgeTo(SurfacePin);
					}
				}
			};

			// The input pin has been split into two. Detect if we have inputs on only one pin and are dealing with older data - if so there's a good chance we can rewire
			// in a better way.
			if (SurfacePin->Edges.Num() == 0 && BoundingShapePin->Edges.Num() > 0)
			{
				MoveEdgeOnInputNodeToLandscapePin(BoundingShapePin);
			}
			else if (SurfacePin->Edges.Num() > 0 && BoundingShapePin->Edges.Num() == 0)
			{
				MoveEdgeOnInputNodeToLandscapePin(SurfacePin);
			}
		}
	}

	Super::ApplyDeprecation(InOutNode);
}
#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
