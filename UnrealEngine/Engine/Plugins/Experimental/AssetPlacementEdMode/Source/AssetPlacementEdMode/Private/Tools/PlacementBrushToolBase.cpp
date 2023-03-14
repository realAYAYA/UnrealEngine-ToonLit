// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/PlacementBrushToolBase.h"
#include "UObject/Object.h"
#include "ToolContextInterfaces.h"
#include "InteractiveToolManager.h"
#include "InstancedFoliageActor.h"
#include "FoliageHelper.h"
#include "Components/PrimitiveComponent.h"
#include "Components/BrushComponent.h"
#include "Components/ModelComponent.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "FoliageInstancedStaticMeshComponent.h"
#include "AssetPlacementSettings.h"
#include "Elements/Framework/EngineElementsLibrary.h"
#include "Elements/Framework/TypedElementRegistry.h"
#include "Elements/Framework/TypedElementSelectionSet.h"
#include "Elements/Interfaces/TypedElementWorldInterface.h"
#include "Elements/Interfaces/TypedElementSelectionInterface.h"
#include "ActorPartition/ActorPartitionSubsystem.h"
#include "Editor.h"
#include "Modes/PlacementModeSubsystem.h"
#include "ActorFactories/ActorFactory.h"
#include "BaseGizmos/GizmoRenderingUtil.h"
#include "Tools/AssetEditorContextInterface.h"
#include "ContextObjectStore.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PlacementBrushToolBase)

bool UPlacementToolBuilderBase::CanBuildTool(const FToolBuilderState& SceneState) const
{	
	TWeakObjectPtr<const UAssetPlacementSettings> PlacementSettings = GEditor->GetEditorSubsystem<UPlacementModeSubsystem>()->GetModeSettingsObject();
	return PlacementSettings.IsValid() && PlacementSettings->GetActivePaletteItems().Num();
}

UInteractiveTool* UPlacementToolBuilderBase::BuildTool(const FToolBuilderState& SceneState) const
{
	return FactoryToolInstance(SceneState.ToolManager);
}

bool UPlacementBrushToolBase::HitTest(const FRay& Ray, FHitResult& OutHit)
{
	const FVector TraceStart(Ray.Origin);
	const FVector TraceEnd(Ray.Origin + Ray.Direction * HALF_WORLD_MAX);

	return FindHitResultWithStartAndEndTraceVectors(OutHit, TraceStart, TraceEnd);
}

bool UPlacementBrushToolBase::AreAllTargetsValid() const
{
	return Target ? Target->IsValid() : true;
}

void UPlacementBrushToolBase::Render(IToolsContextRenderAPI* RenderAPI)
{
	Super::Render(RenderAPI);

	// Transform the brush radius to standard pixel size
	LastBrushStampWorldToPixelScale = GizmoRenderingUtil::CalculateLocalPixelToWorldScale(RenderAPI->GetSceneView(), LastBrushStamp.WorldPosition);
}

void UPlacementBrushToolBase::OnClickPress(const FInputDeviceRay& PressPos)
{
	LastDeviceInputRay = PressPos;
	Super::OnClickPress(PressPos);
}

void UPlacementBrushToolBase::OnClickDrag(const FInputDeviceRay& DragPos)
{
	LastDeviceInputRay = DragPos;
	GetToolManager()->PostInvalidation();
	Super::OnClickDrag(DragPos);
}

double UPlacementBrushToolBase::EstimateMaximumTargetDimension()
{
	return 1000.0;
}

bool UPlacementBrushToolBase::FindHitResultWithStartAndEndTraceVectors(FHitResult& OutHit, const FVector& TraceStart, const FVector& TraceEnd, float TraceRadius)
{
	UWorld* EditingWorld = GetToolManager()->GetWorld();
	constexpr TCHAR NAME_PlacementBrushTool[] = TEXT("PlacementBrushTool");

	TWeakObjectPtr<const UAssetPlacementSettings> PlacementSettings = GEditor->GetEditorSubsystem<UPlacementModeSubsystem>()->GetModeSettingsObject();

	auto FilterFunc = [PlacementSettings](const UPrimitiveComponent* InComponent) {
		if (InComponent && PlacementSettings.IsValid())
		{
			bool bFoliageOwned = InComponent->GetOwner() && FFoliageHelper::IsOwnedByFoliage(InComponent->GetOwner());
			const bool bAllowLandscape = PlacementSettings->bLandscape;
			const bool bAllowStaticMesh = PlacementSettings->bStaticMeshes;
			const bool bAllowBSP = PlacementSettings->bBSP;
			const bool bAllowFoliage = PlacementSettings->bFoliage;
			const bool bAllowTranslucent = PlacementSettings->bTranslucent;

			// allow list
			bool bAllowed =
				(bAllowLandscape && InComponent->IsA(ULandscapeHeightfieldCollisionComponent::StaticClass())) ||
				(bAllowStaticMesh && InComponent->IsA(UStaticMeshComponent::StaticClass()) && !InComponent->IsA(UFoliageInstancedStaticMeshComponent::StaticClass()) && !bFoliageOwned) ||
				(bAllowBSP && (InComponent->IsA(UBrushComponent::StaticClass()) || InComponent->IsA(UModelComponent::StaticClass()))) ||
				(bAllowFoliage && (InComponent->IsA(UFoliageInstancedStaticMeshComponent::StaticClass()) || bFoliageOwned));

			// deny list
			bAllowed &=
				(bAllowTranslucent || !(InComponent->GetMaterial(0) && IsTranslucentBlendMode(InComponent->GetMaterial(0)->GetBlendMode())));

			return bAllowed;
		}

		return false; };

	return AInstancedFoliageActor::FoliageTrace(EditingWorld, OutHit, FDesiredFoliageInstance(TraceStart, TraceEnd, /* FoliageType= */ nullptr, TraceRadius), NAME_PlacementBrushTool, /* bReturnFaceIndex */ false, FilterFunc);
}

FTransform UPlacementBrushToolBase::GenerateTransformFromHitLocationAndNormal(const FVector& InLocation, const FVector& InNormal)
{
	const UAssetPlacementSettings* PlacementSettings = GEditor->GetEditorSubsystem<UPlacementModeSubsystem>()->GetModeSettingsObject();
	FTransform FinalizedTransform(GenerateRandomRotation(PlacementSettings), InLocation, GenerateRandomScale(PlacementSettings));
	return FinalizeTransform(FinalizedTransform, InNormal, PlacementSettings);
}

FQuat UPlacementBrushToolBase::GenerateRandomRotation(const UAssetPlacementSettings* PlacementSettings)
{
	if (!PlacementSettings)
	{
		return FQuat::Identity;
	}

	FRotator GeneratedRotation = FRotator::ZeroRotator;
	auto GetRandomSignedValueInRange = [](const FFloatInterval& Range, bool bAllowSigned) -> float
	{
		float Sign = (FMath::RandBool() && bAllowSigned) ? -1.0f : 1.0f;
		return Range.Interpolate(FMath::FRand()) * Sign;
	};

	if (PlacementSettings->bUseRandomRotationX)
	{
		GeneratedRotation.Roll = GetRandomSignedValueInRange(PlacementSettings->RandomRotationX, PlacementSettings->bAllowNegativeRotationX);
	}

	if (PlacementSettings->bUseRandomRotationY)
	{
		GeneratedRotation.Pitch = GetRandomSignedValueInRange(PlacementSettings->RandomRotationY, PlacementSettings->bAllowNegativeRotationY);
	}

	if (PlacementSettings->bUseRandomRotationZ)
	{
		GeneratedRotation.Yaw = GetRandomSignedValueInRange(PlacementSettings->RandomRotationZ, PlacementSettings->bAllowNegativeRotationZ);
	}

	return GeneratedRotation.Quaternion();
}

FVector UPlacementBrushToolBase::GenerateRandomScale(const UAssetPlacementSettings* PlacementSettings)
{
	FVector GeneratedScale(1.0f);
	if (!PlacementSettings || !PlacementSettings->bUseRandomScale)
	{
		return GeneratedScale;
	}

	auto GenerateRandomScaleComponent = [PlacementSettings]() -> float
	{
		float Sign = (FMath::RandBool() && PlacementSettings->bAllowNegativeScale) ? -1.0f : 1.0f;
		return PlacementSettings->ScaleRange.Interpolate(FMath::FRand()) * Sign;
	};

	switch (PlacementSettings->ScalingType)
	{
		case EFoliageScaling::Free:
		{
			GeneratedScale = FVector(GenerateRandomScaleComponent(), GenerateRandomScaleComponent(), GenerateRandomScaleComponent());
			break;
		}
		case EFoliageScaling::Uniform:
		{
			float ScaleComponent = GenerateRandomScaleComponent();
			GeneratedScale = FVector(ScaleComponent, ScaleComponent, ScaleComponent);
			break;
		}
		case EFoliageScaling::LockXY:
		{
			GeneratedScale.Z = GenerateRandomScaleComponent();
			break;
		}
		case EFoliageScaling::LockYZ:
		{
			GeneratedScale.X = GenerateRandomScaleComponent();
			break;
		}
		case EFoliageScaling::LockXZ:
		{
			GeneratedScale.Y = GenerateRandomScaleComponent();
			break;
		}
	}

	return GeneratedScale;
}

FQuat UPlacementBrushToolBase::AlignRotationWithNormal(const FQuat& InRotation, const FVector& InNormal, EAxis::Type InAlignmentAxis, bool bInvertAxis)
{
	FVector AlignmentVector = FVector::UpVector;
	switch (InAlignmentAxis)
	{
		case EAxis::Type::X:
		{
			AlignmentVector = bInvertAxis ? FVector::BackwardVector : FVector::ForwardVector;
		}
		break;

		case EAxis::Type::Y:
		{
			AlignmentVector = bInvertAxis ? FVector::LeftVector : FVector::RightVector;
		}
		break;

		case EAxis::Type::Z:
		{
			AlignmentVector = bInvertAxis ? FVector::DownVector : FVector::UpVector;
		}
		break;
	}

	return FindActorAlignmentRotation(InRotation, AlignmentVector, InNormal);
}

FTransform UPlacementBrushToolBase::FinalizeTransform(const FTransform& OriginalTransform, const FVector& InNormal, const UAssetPlacementSettings* PlacementSettings)
{
	if (!PlacementSettings)
	{
		return OriginalTransform;
	}

	FTransform FinalizedTransform(FQuat::Identity, OriginalTransform.GetTranslation(), OriginalTransform.GetScale3D());

	// Add the world offset.
	FVector WorldOffset = PlacementSettings->WorldLocationOffset;
	if (PlacementSettings->bScaleWorldLocationOffset)
	{
		WorldOffset *= OriginalTransform.GetScale3D();
	}
	FinalizedTransform.AddToTranslation(WorldOffset);

	// Align to normal.
	FQuat AdjustedRotation(OriginalTransform.GetRotation());
	if (PlacementSettings->bAlignToNormal)
	{
		AdjustedRotation = AlignRotationWithNormal(AdjustedRotation, InNormal, PlacementSettings->AxisToAlignWithNormal, PlacementSettings->bInvertNormalAxis);
	}
	AdjustedRotation.Normalize();
	FinalizedTransform.SetRotation(AdjustedRotation);

	// Add the relative offset.
	{
		FVector RelativeOffset = PlacementSettings->RelativeLocationOffset;
		if (PlacementSettings->bScaleRelativeLocationOffset)
		{
			RelativeOffset *= OriginalTransform.GetScale3D();
		}
		FinalizedTransform.SetTranslation(FinalizedTransform.TransformPosition(RelativeOffset));
	}

	return FinalizedTransform;
}

FTypedElementListRef UPlacementBrushToolBase::GetElementsInBrushRadius(const FInputDeviceRay& DragPos) const
{
	FTypedElementListRef ElementHandles = UTypedElementRegistry::GetInstance()->CreateElementList();

	const UTypedElementSelectionSet* SelectionSet = nullptr;
	if (IAssetEditorContextInterface* AssetEditorAPI = GetToolManager()->GetContextObjectStore()->FindContext<IAssetEditorContextInterface>())
	{
		SelectionSet = AssetEditorAPI->GetSelectionSet();
	}

	if (!SelectionSet)
	{
		return ElementHandles;
	}

	UPlacementModeSubsystem* PlacementModeSubsystem = GEditor->GetEditorSubsystem<UPlacementModeSubsystem>();
	if (!PlacementModeSubsystem || !PlacementModeSubsystem->GetModeSettingsObject())
	{
		return ElementHandles;
	}

	FCollisionQueryParams QueryParams(TEXT("PlacementBrushTool"), SCENE_QUERY_STAT_ONLY(EdMode_PlacementTrace), true);
	QueryParams.bReturnFaceIndex = false;
	TArray<FHitResult> Hits;
	FCollisionShape BrushSphere;
	BrushSphere.SetSphere(LastBrushStamp.Radius);

	const FVector TraceStart(LastWorldRay.Origin);
	const FVector TraceEnd(LastWorldRay.Origin + LastWorldRay.Direction * HALF_WORLD_MAX);

	GetToolManager()->GetWorld()->SweepMultiByObjectType(Hits, TraceStart, TraceEnd, FQuat::Identity,
		FCollisionObjectQueryParams(FCollisionObjectQueryParams::InitType::AllObjects), BrushSphere, QueryParams);

	for (const FHitResult& Hit : Hits)
	{
		if (UInstancedStaticMeshComponent* ISMComponent = Cast<UInstancedStaticMeshComponent>(Hit.GetComponent()))
		{
			FTypedElementHandle StaticMeshObjectHandle = UEngineElementsLibrary::AcquireEditorObjectElementHandle(ISMComponent->GetStaticMesh());
			if (!PlacementModeSubsystem->GetModeSettingsObject()->DoesActivePaletteSupportElement(StaticMeshObjectHandle))
			{
				continue;
			}

			TArray<int32> OverlappingInstances = ISMComponent->GetInstancesOverlappingSphere(LastBrushStamp.WorldPosition, LastBrushStamp.Radius);
			ElementHandles->Reserve(OverlappingInstances.Num());
			for (const int32& InstanceIdx : OverlappingInstances)
			{
				if (FTypedElementHandle SMInstanceHandle = UEngineElementsLibrary::AcquireEditorSMInstanceElementHandle(ISMComponent, InstanceIdx))
				{
					ElementHandles->Add(MoveTemp(SMInstanceHandle));
				}
			}
		}
		else
		{
			FTypedElementHandle PrimarySelectionHandle = SelectionSet->GetSelectionElement(UEngineElementsLibrary::AcquireEditorComponentElementHandle(Hit.GetComponent()), ETypedElementSelectionMethod::Primary);
			if (!PlacementModeSubsystem->GetModeSettingsObject()->DoesActivePaletteSupportElement(PrimarySelectionHandle))
			{
				continue;
			}

			ElementHandles->Add(MoveTemp(PrimarySelectionHandle));
		}
	}

	return ElementHandles;
}

