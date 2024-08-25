// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsAssetEditorSkeletalMeshComponent.h"
#include "MaterialDomain.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/Material.h"
#include "Preferences/PhysicsAssetEditorOptions.h"
#include "SceneManagement.h"
#include "PhysicsAssetEditorSharedData.h"
#include "PhysicsAssetEditorHitProxies.h"
#include "PhysicsAssetEditorSkeletalMeshComponent.h"
#include "PhysicsAssetEditorAnimInstance.h"
#include "PhysicsAssetRenderUtils.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Chaos/Core.h"
#include "SkeletalMeshTypes.h"
#include "AnimPreviewInstance.h"
#include "UObject/Package.h"
#include "Styling/AppStyle.h"
#include "Chaos/WeightedLatticeImplicitObject.h"
#include "Chaos/Levelset.h"

namespace
{
	bool bDebugViewportClicks = false;
	FAutoConsoleVariableRef CVarChaosImmPhysStepTime(TEXT("p.PhAT.DebugViewportClicks"), bDebugViewportClicks, TEXT("Set to 1 to show mouse click results in PhAT"));
}

UPhysicsAssetEditorSkeletalMeshComponent::UPhysicsAssetEditorSkeletalMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, BoneUnselectedColor(170, 155, 225)
	, NoCollisionColor(200, 200, 200)
	, FixedColor(125, 125, 0)
	, ConstraintBone1Color(255, 166, 0)
	, ConstraintBone2Color(0, 150, 150)
	, HierarchyDrawColor(220, 255, 220)
	, AnimSkelDrawColor(255, 64, 64)
	, COMRenderSize(5.0f)
	, InfluenceLineLength(2.0f)
	, InfluenceLineColor(0, 255, 0)
{
	if (!HasAnyFlags(RF_DefaultSubObject | RF_ArchetypeObject | RF_ClassDefaultObject))
	{
		// Body materials
		UMaterialInterface* BaseElemSelectedMaterial = LoadObject<UMaterialInterface>(NULL, TEXT("/Engine/EditorMaterials/PhAT_ElemSelectedMaterial.PhAT_ElemSelectedMaterial"), NULL, LOAD_None, NULL);
		ElemSelectedMaterial = UMaterialInstanceDynamic::Create(BaseElemSelectedMaterial, GetTransientPackage());
		check(ElemSelectedMaterial);

		BoneMaterialHit = UMaterial::GetDefaultMaterial(MD_Surface);
		check(BoneMaterialHit);

		UMaterialInterface* BaseBoneUnselectedMaterial = LoadObject<UMaterialInterface>(NULL, TEXT("/Engine/EditorMaterials/PhAT_UnselectedMaterial.PhAT_UnselectedMaterial"), NULL, LOAD_None, NULL);
		BoneUnselectedMaterial = UMaterialInstanceDynamic::Create(BaseBoneUnselectedMaterial, GetTransientPackage());
		check(BoneUnselectedMaterial);

		UMaterialInterface* BaseBoneNoCollisionMaterial = LoadObject<UMaterialInterface>(NULL, TEXT("/Engine/EditorMaterials/PhAT_NoCollisionMaterial.PhAT_NoCollisionMaterial"), NULL, LOAD_None, NULL);
		BoneNoCollisionMaterial = UMaterialInstanceDynamic::Create(BaseBoneNoCollisionMaterial, GetTransientPackage());
		check(BoneNoCollisionMaterial);

		// this is because in phat editor, you'd like to see fixed bones to be fixed without animation force update
		KinematicBonesUpdateType = EKinematicBonesUpdateToPhysics::SkipSimulatingBones;
		bUpdateJointsFromAnimation = false;
		SetForcedLOD(1);

		static FName CollisionProfileName(TEXT("PhysicsActor"));
		SetCollisionProfileName(CollisionProfileName);
	}

	bSelectable = false;
}

TObjectPtr<UAnimPreviewInstance> UPhysicsAssetEditorSkeletalMeshComponent::CreatePreviewInstance()
{
	return NewObject<UPhysicsAssetEditorAnimInstance>(this, TEXT("PhatAnimScriptInstance"));
}

void UPhysicsAssetEditorSkeletalMeshComponent::DebugDraw(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	check(SharedData);

	UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();

	if (!PhysicsAsset)
	{
		// Nothing to draw without an asset, this can happen if the preview scene has no skeletal mesh
		return;
	}

	EPhysicsAssetEditorCollisionViewMode CollisionViewMode = SharedData->GetCurrentCollisionViewMode(SharedData->bRunningSimulation);

	if (bDebugViewportClicks)
	{
		PDI->DrawLine(SharedData->LastClickOrigin, SharedData->LastClickOrigin + SharedData->LastClickDirection * 5000.0f, FLinearColor(1, 1, 0, 1), SDPG_Foreground);
		PDI->DrawPoint(SharedData->LastClickOrigin, FLinearColor(1, 1, 0), 5, SDPG_Foreground);
		PDI->DrawLine(SharedData->LastClickHitPos, SharedData->LastClickHitPos + SharedData->LastClickHitNormal * 10.0f, FLinearColor(1, 0, 0, 1), SDPG_Foreground);
		PDI->DrawPoint(SharedData->LastClickHitPos, FLinearColor(1, 0, 0), 5, SDPG_Foreground);
	}

	// set opacity of our materials
	static FName OpacityName(TEXT("Opacity"));
	ElemSelectedMaterial->SetScalarParameterValue(OpacityName, SharedData->EditorOptions->CollisionOpacity);
	BoneUnselectedMaterial->SetScalarParameterValue(OpacityName, SharedData->EditorOptions->bSolidRenderingForSelectedOnly ? 0.0f : SharedData->EditorOptions->CollisionOpacity);
	BoneNoCollisionMaterial->SetScalarParameterValue(OpacityName, SharedData->EditorOptions->bSolidRenderingForSelectedOnly ? 0.0f : SharedData->EditorOptions->CollisionOpacity);

	static FName SelectionColorName(TEXT("SelectionColor"));
	const FSlateColor SelectionColor = FAppStyle::GetSlateColor(SelectionColorName);
	const FLinearColor LinearSelectionColor(SelectionColor.IsColorSpecified() ? SelectionColor.GetSpecifiedColor() : FLinearColor::White);

	ElemSelectedMaterial->SetVectorParameterValue(SelectionColorName, LinearSelectionColor);

	

	FPhysicsAssetRenderSettings* const RenderSettings = UPhysicsAssetRenderUtilities::GetSettings(PhysicsAsset);
	
	if (RenderSettings)
	{
		// Copy render settings from editor viewport. These settings must be applied to the rendering in all editors 
		// when an asset is open in the Physics Asset Editor but should not persist after the editor has been closed.
		RenderSettings->CollisionViewMode = SharedData->GetCurrentCollisionViewMode(SharedData->bRunningSimulation);
		RenderSettings->ConstraintViewMode = SharedData->GetCurrentConstraintViewMode(SharedData->bRunningSimulation);
		RenderSettings->ConstraintDrawSize = SharedData->EditorOptions->ConstraintDrawSize;
		RenderSettings->PhysicsBlend = SharedData->EditorOptions->PhysicsBlend;
		RenderSettings->bHideKinematicBodies = SharedData->EditorOptions->bHideKinematicBodies;
		RenderSettings->bHideSimulatedBodies = SharedData->EditorOptions->bHideSimulatedBodies;
		RenderSettings->bRenderOnlySelectedConstraints = SharedData->EditorOptions->bRenderOnlySelectedConstraints;
		RenderSettings->bShowConstraintsAsPoints = SharedData->EditorOptions->bShowConstraintsAsPoints;
		RenderSettings->bDrawViolatedLimits = SharedData->EditorOptions->bDrawViolatedLimits;

		// Draw Bodies.
		{
			auto TransformFn = [this](const UPhysicsAsset* PhysicsAsset, const FTransform& BoneTM, const int32 BodyIndex, const EAggCollisionShape::Type PrimType, const int32 PrimIndex, const float Scale) { return this->GetPrimitiveTransform(BoneTM, BodyIndex, PrimType, PrimIndex, Scale);  };
			auto ColorFn = [this](const int32 BodyIndex, const EAggCollisionShape::Type PrimitiveType, const int32 PrimitiveIndex, const FPhysicsAssetRenderSettings& Settings) { return this->GetPrimitiveColor(BodyIndex, PrimitiveType, PrimitiveIndex); };
			auto MaterialFn = [this](const int32 BodyIndex, const EAggCollisionShape::Type PrimitiveType, const int32 PrimitiveIndex, const FPhysicsAssetRenderSettings& Settings) { return this->GetPrimitiveMaterial(BodyIndex, PrimitiveType, PrimitiveIndex); };
			auto HitProxyFn = [](const int32 BodyIndex, const EAggCollisionShape::Type PrimitiveType, const int32 PrimitiveIndex) { return new HPhysicsAssetEditorEdBoneProxy(BodyIndex, PrimitiveType, PrimitiveIndex); };

			PhysicsAssetRender::DebugDrawBodies(this, PhysicsAsset, PDI, ColorFn, MaterialFn, TransformFn, HitProxyFn);
		}

		// Draw Constraints.
		{
			auto HitProxyFn = [](const int32 InConstraintIndex) { return new HPhysicsAssetEditorEdConstraintProxy(InConstraintIndex); };
			auto IsConstraintSelectedFn = [this](const uint32 InConstraintIndex) { return this->SharedData->IsConstraintSelected(InConstraintIndex); };

			PhysicsAssetRender::DebugDrawConstraints(this, PhysicsAsset, PDI, IsConstraintSelectedFn, SharedData->bRunningSimulation, HitProxyFn);
		}
	}
}

FPrimitiveSceneProxy* UPhysicsAssetEditorSkeletalMeshComponent::CreateSceneProxy()
{
	FPrimitiveSceneProxy* Proxy = NULL;
	EPhysicsAssetEditorMeshViewMode MeshViewMode = SharedData->GetCurrentMeshViewMode(SharedData->bRunningSimulation);
	if (MeshViewMode != EPhysicsAssetEditorMeshViewMode::None)
	{
		Proxy = UDebugSkelMeshComponent::CreateSceneProxy();
	}

	return Proxy;
}

bool ConstraintInSelected(int32 Index, const TArray<FPhysicsAssetEditorSharedData::FSelection> & Constraints)
{
	for (int32 i = 0; i<Constraints.Num(); ++i)
	{

		if (Constraints[i].Index == Index)
		{
			return true;
		}
	}

	return false;
}

FTransform UPhysicsAssetEditorSkeletalMeshComponent::GetPrimitiveTransform(const FTransform& BoneTM, const int32 BodyIndex, const EAggCollisionShape::Type PrimType, const int32 PrimIndex, const float Scale) const
{
	UBodySetup* SharedBodySetup = SharedData->PhysicsAsset->SkeletalBodySetups[BodyIndex];
	FVector Scale3D(Scale);

	FTransform ManTM = FTransform::Identity;

	if (SharedData->bManipulating && !SharedData->bRunningSimulation)
	{
		FPhysicsAssetEditorSharedData::FSelection Body(BodyIndex, PrimType, PrimIndex);
		for (int32 i = 0; i<SharedData->SelectedBodies.Num(); ++i)
		{
			if (Body == SharedData->SelectedBodies[i])
			{
				ManTM = SharedData->SelectedBodies[i].ManipulateTM;
				break;
			}

		}
	}


	if (PrimType == EAggCollisionShape::Sphere)
	{
		FTransform PrimTM = ManTM * SharedBodySetup->AggGeom.SphereElems[PrimIndex].GetTransform();
		PrimTM.ScaleTranslation(Scale3D);
		return PrimTM * BoneTM;
	}
	else if (PrimType == EAggCollisionShape::Box)
	{
		FTransform PrimTM = ManTM * SharedBodySetup->AggGeom.BoxElems[PrimIndex].GetTransform();
		PrimTM.ScaleTranslation(Scale3D);
		return PrimTM * BoneTM;
	}
	else if (PrimType == EAggCollisionShape::Sphyl)
	{
		FTransform PrimTM = ManTM * SharedBodySetup->AggGeom.SphylElems[PrimIndex].GetTransform();
		PrimTM.ScaleTranslation(Scale3D);
		return PrimTM * BoneTM;
	}
	else if (PrimType == EAggCollisionShape::Convex)
	{
		FTransform PrimTM = ManTM * SharedBodySetup->AggGeom.ConvexElems[PrimIndex].GetTransform();
		PrimTM.ScaleTranslation(Scale3D);
		return PrimTM * BoneTM;
	}
	else if (PrimType == EAggCollisionShape::TaperedCapsule)
	{
		FTransform PrimTM = ManTM * SharedBodySetup->AggGeom.TaperedCapsuleElems[PrimIndex].GetTransform();
		PrimTM.ScaleTranslation(Scale3D);
		return PrimTM * BoneTM;
	}
	else if (PrimType == EAggCollisionShape::LevelSet)
	{
		FTransform PrimTM = ManTM * SharedBodySetup->AggGeom.LevelSetElems[PrimIndex].GetTransform();
		PrimTM.ScaleTranslation(Scale3D);
		return PrimTM * BoneTM;
	}
	else if (PrimType == EAggCollisionShape::SkinnedLevelSet)
	{
		FTransform PrimTM = ManTM * SharedBodySetup->AggGeom.SkinnedLevelSetElems[PrimIndex].GetTransform();
		PrimTM.ScaleTranslation(Scale3D);
		return PrimTM * BoneTM;
	}

	// Should never reach here
	check(0);
	return FTransform::Identity;
}

FColor UPhysicsAssetEditorSkeletalMeshComponent::GetPrimitiveColor(const int32 BodyIndex, const EAggCollisionShape::Type PrimitiveType, const int32 PrimitiveIndex) const
{
	UBodySetup* SharedBodySetup = SharedData->PhysicsAsset->SkeletalBodySetups[BodyIndex];

	if (!SharedData->bRunningSimulation && SharedData->GetSelectedConstraint())
	{
		UPhysicsConstraintTemplate* cs = SharedData->PhysicsAsset->ConstraintSetup[SharedData->GetSelectedConstraint()->Index];

		if (cs->DefaultInstance.ConstraintBone1 == SharedBodySetup->BoneName)
		{
			return ConstraintBone1Color;
		}
		else if (cs->DefaultInstance.ConstraintBone2 == SharedBodySetup->BoneName)
		{
			return ConstraintBone2Color;
		}
	}

	FPhysicsAssetEditorSharedData::FSelection Body(BodyIndex, PrimitiveType, PrimitiveIndex);

	static FName SelectionColorName(TEXT("SelectionColor"));
	const FSlateColor SelectionColor = FAppStyle::GetSlateColor(SelectionColorName);
	const FLinearColor SelectionColorLinear(SelectionColor.IsColorSpecified() ? SelectionColor.GetSpecifiedColor() : FLinearColor::White);
	const FColor ElemSelectedColor = SelectionColorLinear.ToFColor(true);
	const FColor ElemSelectedBodyColor = (SelectionColorLinear* 0.5f).ToFColor(true);

	bool bInBody = false;
	for (int32 i = 0; i<SharedData->SelectedBodies.Num(); ++i)
	{
		if (BodyIndex == SharedData->SelectedBodies[i].Index)
		{
			bInBody = true;
		}

		if (Body == SharedData->SelectedBodies[i] && !SharedData->bRunningSimulation)
		{
			return ElemSelectedColor;
		}
	}

	if (bInBody && !SharedData->bRunningSimulation)	//this primitive is in a body that's currently selected, but this primitive itself isn't selected
	{
		return ElemSelectedBodyColor;
	}
	if(PrimitiveType == EAggCollisionShape::TaperedCapsule)
	{
		return NoCollisionColor;
	}

	if (SharedData->bRunningSimulation)
	{
		const bool bIsSimulatedAtAll = SharedBodySetup->PhysicsType == PhysType_Simulated || (SharedBodySetup->PhysicsType == PhysType_Default && SharedData->EditorOptions->PhysicsBlend > 0.f);
		if (!bIsSimulatedAtAll)
		{
			return FixedColor;
		}
	}
	else
	{
		if (!SharedData->bRunningSimulation && SharedData->SelectedBodies.Num())
		{
			// If there is no collision with this body, use 'no collision material'.
			if (SharedData->NoCollisionBodies.Find(BodyIndex) != INDEX_NONE)
			{
				return NoCollisionColor;
			}
		}
	}

	return BoneUnselectedColor;
}

UMaterialInterface* UPhysicsAssetEditorSkeletalMeshComponent::GetPrimitiveMaterial(const int32 BodyIndex, const EAggCollisionShape::Type PrimitiveType, const int32 PrimitiveIndex) const
{
	if (SharedData->bRunningSimulation)
	{
		return PrimitiveType == EAggCollisionShape::TaperedCapsule ? BoneNoCollisionMaterial : BoneUnselectedMaterial;
	}

	FPhysicsAssetEditorSharedData::FSelection Body(BodyIndex, PrimitiveType, PrimitiveIndex);

	for (int32 i = 0; i < SharedData->SelectedBodies.Num(); ++i)
	{
		if (Body == SharedData->SelectedBodies[i] && !SharedData->bRunningSimulation)
		{
			return ElemSelectedMaterial;
		}
	}

	if (PrimitiveType == EAggCollisionShape::TaperedCapsule)
	{
		return BoneNoCollisionMaterial;
	}

	// If there is no collision with this body, use 'no collision material'.
	if (SharedData->SelectedBodies.Num() && SharedData->NoCollisionBodies.Find(BodyIndex) != INDEX_NONE && !SharedData->bRunningSimulation)
	{
		return BoneNoCollisionMaterial;
	}
	else
	{
		return BoneUnselectedMaterial;
	}
}

void UPhysicsAssetEditorSkeletalMeshComponent::RefreshBoneTransforms(FActorComponentTickFunction* TickFunction)
{
	Super::RefreshBoneTransforms(TickFunction);

	// Horrible kludge, but we need to flip the buffer back here as we need to wait on the physics tick group.
	// However UDebugSkelMeshComponent passes NULL to force non-threaded work, which assumes a flip is needed straight away
	if (ShouldBlendPhysicsBones())
	{
		bNeedToFlipSpaceBaseBuffers = true;
		FinalizeBoneTransform();
		bNeedToFlipSpaceBaseBuffers = true;
	}
	UpdateSkinnedLevelSets();
}

void UPhysicsAssetEditorSkeletalMeshComponent::AddImpulseAtLocation(FVector Impulse, FVector Location, FName BoneName)
{
	if (PreviewInstance != nullptr)
	{
		PreviewInstance->AddImpulseAtLocation(Impulse, Location, BoneName);
	}
}

bool UPhysicsAssetEditorSkeletalMeshComponent::ShouldCreatePhysicsState() const
{
	// @todo(chaos): the main physics scene is not running (and never runs) in the physics editor,
	// and currently this means it will accumulate body create/destroy commands every time
	// we hit "Simulate". Fix this!  However, we still need physics state for mouse ray hit detection 
	// on the bodies so we can't just avoid creating physics state...
	return Super::ShouldCreatePhysicsState();
}


void UPhysicsAssetEditorSkeletalMeshComponent::Grab(FName InBoneName, const FVector& Location, const FRotator& Rotation, bool bRotationConstrained)
{
	UPhysicsAssetEditorAnimInstance* PhatPreviewInstance = Cast<UPhysicsAssetEditorAnimInstance>(PreviewInstance);
	if (PhatPreviewInstance != nullptr)
	{
		PhatPreviewInstance->Grab(InBoneName, Location, Rotation, bRotationConstrained);
	}
}

void UPhysicsAssetEditorSkeletalMeshComponent::Ungrab()
{
	UPhysicsAssetEditorAnimInstance* PhatPreviewInstance = Cast<UPhysicsAssetEditorAnimInstance>(PreviewInstance);
	if (PhatPreviewInstance != nullptr)
	{
		PhatPreviewInstance->Ungrab();
	}
}

void UPhysicsAssetEditorSkeletalMeshComponent::UpdateHandleTransform(const FTransform& NewTransform)
{
	UPhysicsAssetEditorAnimInstance* PhatPreviewInstance = Cast<UPhysicsAssetEditorAnimInstance>(PreviewInstance);
	if (PhatPreviewInstance != nullptr)
	{
		PhatPreviewInstance->UpdateHandleTransform(NewTransform);
	}
}

void UPhysicsAssetEditorSkeletalMeshComponent::UpdateDriveSettings(bool bLinearSoft, float LinearStiffness, float LinearDamping)
{
	UPhysicsAssetEditorAnimInstance* PhatPreviewInstance = Cast<UPhysicsAssetEditorAnimInstance>(PreviewInstance);
	if (PhatPreviewInstance != nullptr)
	{
		PhatPreviewInstance->UpdateDriveSettings(bLinearSoft, LinearStiffness, LinearDamping);
	}
}

void UPhysicsAssetEditorSkeletalMeshComponent::CreateSimulationFloor(FBodyInstance* FloorBodyInstance, const FTransform& Transform)
{
	UPhysicsAssetEditorAnimInstance* PhatPreviewInstance = Cast<UPhysicsAssetEditorAnimInstance>(PreviewInstance);
	if (PhatPreviewInstance != nullptr)
	{
		PhatPreviewInstance->CreateSimulationFloor(FloorBodyInstance, Transform);
	}
}

void UPhysicsAssetEditorSkeletalMeshComponent::UpdateSkinnedLevelSets()
{
	UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();
	if (!PhysicsAsset)
	{
		return;
	}
	for (int32 i = 0; i < PhysicsAsset->SkeletalBodySetups.Num(); ++i)
	{
		const int32 BoneIndex = GetBoneIndex(PhysicsAsset->SkeletalBodySetups[i]->BoneName);
		if (BoneIndex != INDEX_NONE)
		{
			FKAggregateGeom* const AggGeom = &PhysicsAsset->SkeletalBodySetups[i]->AggGeom;
			if (AggGeom)
			{
				for (FKSkinnedLevelSetElem& SkinnedLevelSet : AggGeom->SkinnedLevelSetElems)
				{
					if (SkinnedLevelSet.WeightedLevelSet().IsValid())
					{
						const TArray<FName>& UsedBoneNames = SkinnedLevelSet.WeightedLevelSet()->GetUsedBones();

						const FTransform RootTransformInv = GetBoneTransform(BoneIndex, FTransform::Identity).Inverse();
						TArray<FTransform> Transforms;
						Transforms.SetNum(UsedBoneNames.Num());

						for (int32 LocalIdx = 0; LocalIdx < UsedBoneNames.Num(); ++LocalIdx)
						{
							const int32 LocalBoneIndex = GetBoneIndex(UsedBoneNames[LocalIdx]);
							if (LocalBoneIndex != INDEX_NONE)
							{
								const FTransform BoneTransformTimesRootTransformInv = GetBoneTransform(LocalBoneIndex, RootTransformInv);
								Transforms[LocalIdx] = BoneTransformTimesRootTransformInv;
							}
							else
							{
								Transforms[LocalIdx] = RootTransformInv;
							}
						}

						SkinnedLevelSet.WeightedLevelSet()->DeformPoints(Transforms);
					}
				}
			}
		}
	}
}