// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modifiers/AvaAutoSizeModifier.h"

#include "AvaActorUtils.h"
#include "Async/Async.h"
#include "AvaModifiersActorUtils.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/MeshTransforms.h"
#include "AvaShapeActor.h"
#include "DynamicMeshes/AvaShape2DDynMeshBase.h"
#include "Shared/AvaTransformModifierShared.h"

#define LOCTEXT_NAMESPACE "AvaAutoSizeModifier"

bool UAvaAutoSizeModifier::IsModifierDirtyable() const
{
	const AActor* const ActorModified = GetModifiedActor();
	AActor* const TrackedActor = ReferenceActor.ReferenceActorWeak.Get();

	if (!IsValid(TrackedActor) || !IsValid(ActorModified))
	{
		return Super::IsModifierDirtyable();
	}

	const FBox ReferenceActorLocalBounds = FAvaModifiersActorUtils::GetActorsBounds(TrackedActor, bIncludeChildren, true);

	if (ReferenceActorLocalBounds.Equals(CachedReferenceBounds, 0.01))
	{
		return Super::IsModifierDirtyable();
	}

	return true;
}

void UAvaAutoSizeModifier::OnModifierCDOSetup(FActorModifierCoreMetadata& InMetadata)
{
	Super::OnModifierCDOSetup(InMetadata);

	InMetadata.AllowTick(true);
	InMetadata.SetName(TEXT("AutoSize"));
	InMetadata.SetCategory(TEXT("Layout"));
#if WITH_EDITOR
	InMetadata.SetDescription(LOCTEXT("ModifierDescription", "The modified actor will be resized to act as a background for the specified actor"));
#endif

	InMetadata.SetCompatibilityRule([](const AActor* InActor)->bool
	{
		bool bSupported = false;
		if (InActor)
		{
			if (const UDynamicMeshComponent* DynMeshComponent = InActor->FindComponentByClass<UDynamicMeshComponent>())
			{
				DynMeshComponent->ProcessMesh([&bSupported](const FDynamicMesh3& ProcessMesh)
				{
					bSupported = ProcessMesh.VertexCount() > 0 && static_cast<FBox>(ProcessMesh.GetBounds(true)).GetSize().X == 0;
				});
			}
		}
		return bSupported;
	});
}

void UAvaAutoSizeModifier::OnModifiedActorTransformed()
{
	Super::OnModifiedActorTransformed();

	const AActor* ActorModified = GetModifiedActor();
	const AActor* TrackedActor = ReferenceActor.ReferenceActorWeak.Get();
	if (!TrackedActor || !ActorModified)
	{
		return;
	}

	// Compare current location and previous followed location but only component of followed axis to allow movements
	if (FAvaModifiersActorUtils::IsAxisVectorEquals(ActorModified->GetActorLocation(), CachedFollowLocation, FollowedAxis))
	{
		return;
	}

	MarkModifierDirty();
}

void UAvaAutoSizeModifier::OnModifierAdded(EActorModifierCoreEnableReason InReason)
{
	Super::OnModifierAdded(InReason);

	AddExtension<FAvaTransformUpdateModifierExtension>(this);
	AddExtension<FAvaRenderStateUpdateModifierExtension>(this);
	AddExtension<FAvaSceneTreeUpdateModifierExtension>(this);

	if (FAvaSceneTreeUpdateModifierExtension* SceneExtension = GetExtension<FAvaSceneTreeUpdateModifierExtension>())
	{
		SceneExtension->TrackSceneTree(0, &ReferenceActor);
	}
}

void UAvaAutoSizeModifier::OnModifierEnabled(EActorModifierCoreEnableReason InReason)
{
	Super::OnModifierEnabled(InReason);

	// Save actor layout state
	if (UAvaTransformModifierShared* LayoutShared = GetShared<UAvaTransformModifierShared>(true))
	{
		LayoutShared->SaveActorState(this, GetModifiedActor());
	}

	if (InReason == EActorModifierCoreEnableReason::User)
	{
		if (const AAvaShapeActor* const ReferenceAsShapeActor = Cast<AAvaShapeActor>(GetModifiedActor()))
		{
			if (UAvaShape2DDynMeshBase* const Shape2DDynMesh = Cast<UAvaShape2DDynMeshBase>(ReferenceAsShapeActor->GetDynamicMesh()))
			{
				ShapeDynMesh2DWeak = Shape2DDynMesh;
				PreModifierShapeDynMesh2DSize = Shape2DDynMesh->GetSize2D();
			}
		}
	}

	bDeprecatedPropertiesMigrated = true;
}

void UAvaAutoSizeModifier::OnModifierDisabled(EActorModifierCoreDisableReason InReason)
{
	Super::OnModifierDisabled(InReason);

	// Restore actor layout state
	if (UAvaTransformModifierShared* LayoutShared = GetShared<UAvaTransformModifierShared>(false))
	{
		LayoutShared->RestoreActorState(this, GetModifiedActor());
	}

	if (UAvaShape2DDynMeshBase* const Shape2DDynMesh = ShapeDynMesh2DWeak.Get())
	{
		Shape2DDynMesh->SetSize2D(PreModifierShapeDynMesh2DSize);
	}
}

void UAvaAutoSizeModifier::Apply()
{
	AActor* const CurrentlyModifiedActor = GetModifiedActor();

	AActor* TrackedActor = ReferenceActor.ReferenceActorWeak.Get();
	if (!IsValid(TrackedActor))
	{
		Next();
		return;
	}

	UDynamicMeshComponent* const DynMeshComponent = GetMeshComponent();
	if (!IsValid(DynMeshComponent))
	{
		Fail(LOCTEXT("InvalidDynamicMeshComponent", "Invalid dynamic mesh component on modified actor"));
		return;
	}

	if (FAvaRenderStateUpdateModifierExtension* RenderExtension = GetExtension<FAvaRenderStateUpdateModifierExtension>())
	{
		RenderExtension->SetTrackedActorVisibility(TrackedActor, bIncludeChildren);
	}

	CachedReferenceBounds = FAvaModifiersActorUtils::GetActorsBounds(TrackedActor, bIncludeChildren, true);

	const FBox ModifiedActorBounds = FAvaModifiersActorUtils::GetActorsBounds(CurrentlyModifiedActor, false);

	FVector ModifiedActorBoundsOrigin;
	FVector ModifiedActorBoundsExtent;
	ModifiedActorBounds.GetCenterAndExtents(ModifiedActorBoundsOrigin, ModifiedActorBoundsExtent);

	const FMargin& DesiredPadding = Padding;
	FVector ReferenceBoundsExtent = CachedReferenceBounds.GetExtent();

	// Add padding only if there is content inside otherwise don't
	if (!CachedReferenceBounds.GetExtent().IsNearlyZero())
	{
		ReferenceBoundsExtent += FVector(0, DesiredPadding.Right + DesiredPadding.Left, DesiredPadding.Top + DesiredPadding.Bottom);
	}

	// Avoid division by 0
	if (ModifiedActorBoundsExtent.IsNearlyZero())
	{
		ModifiedActorBoundsExtent += FVector(UE_KINDA_SMALL_NUMBER);
	}

	const float ScaleRatioY = ReferenceBoundsExtent.Y/ModifiedActorBoundsExtent.Y;
	const float ScaleRatioZ = ReferenceBoundsExtent.Z/ModifiedActorBoundsExtent.Z;
	const FVector ScaleRatio = FVector(1.0f, ScaleRatioY, ScaleRatioZ);

	// Move only if there is content inside and at least an axis is followed
	const FVector FollowAxisVector = FAvaModifiersActorUtils::GetVectorAxis(FollowedAxis);
	if (!FollowAxisVector.IsNearlyZero() && !CachedReferenceBounds.GetSize().IsNearlyZero())
	{
		const FVector ModifiedActorPivotToBoundsOffset = ModifiedActorBoundsOrigin - CurrentlyModifiedActor->GetActorLocation();
		const FVector ReferenceActorPivotToBoundsOffset = CachedReferenceBounds.GetCenter() - TrackedActor->GetActorLocation();
		const FVector PaddingOffset = FVector(0.f, DesiredPadding.Right - DesiredPadding.Left, DesiredPadding.Top - DesiredPadding.Bottom);

		const FVector DesiredLocationOffset =
			// Add margin padding
			PaddingOffset
			// Remove pivot actor location difference for modified actor
			- ModifiedActorPivotToBoundsOffset
			// add pivot offset difference for reference actor
			+ ReferenceActorPivotToBoundsOffset;

		CachedFollowLocation =
			CurrentlyModifiedActor->GetActorLocation()
			+ (TrackedActor->GetActorLocation() - CurrentlyModifiedActor->GetActorLocation() + DesiredLocationOffset) * FollowAxisVector;

		CurrentlyModifiedActor->SetActorLocation(CachedFollowLocation);
	}

	// check if we can use Shape2DDynMesh SetSize2D for this modified mesh.
	// this allows us to keep properly scaled corner bevels and slants
	if (UAvaShape2DDynMeshBase* const Shape2DDynMesh = ShapeDynMesh2DWeak.Get())
	{
		FVector2D DesiredSize2D = FVector2D::Max(
			FVector2D(ReferenceBoundsExtent.Y, ReferenceBoundsExtent.Z)*2.0f,
			UAvaShape2DDynMeshBase::MinSize2D
		);

		switch (FitMode)
		{
			case EAvaAutoSizeFitMode::WidthAndHeight:
				// nothing to do
				break;
			case EAvaAutoSizeFitMode::WidthOnly:
				{
					DesiredSize2D = FVector2D(DesiredSize2D.X, Shape2DDynMesh->GetSize2D().Y);
				}
				break;
			case EAvaAutoSizeFitMode::HeightOnly:
				{
					DesiredSize2D = FVector2D(Shape2DDynMesh->GetSize2D().X, DesiredSize2D.Y);
				}
				break;
			default: ;
		}

		// Not liking this as this will update the shape and trigger a stack update,
		// we might create a loop where the modifier dirties the shape and the shape dirties modifiers
		if (!Shape2DDynMesh->GetSize2D().Equals(DesiredSize2D, 0.01))
		{
			Shape2DDynMesh->SetSize2D(DesiredSize2D);
		}
	}
	else // for all other dynamic meshes, just scale all vertices
	{
		FVector NewScaleValue = CurrentlyModifiedActor->GetActorScale3D() * ScaleRatio;

		switch (FitMode)
		{
			case EAvaAutoSizeFitMode::WidthAndHeight:
				break;
			case EAvaAutoSizeFitMode::WidthOnly:
				NewScaleValue.Z = 1.0f;
				break;
			case EAvaAutoSizeFitMode::HeightOnly:
				NewScaleValue.Y = 1.0f;
				break;
			default: ;
		}

		DynMeshComponent->GetDynamicMesh()->EditMesh([&NewScaleValue](FDynamicMesh3& EditMesh)
		{
			MeshTransforms::Scale(EditMesh, NewScaleValue, FVector::Zero(), true);
		}
		, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, false);
	}

	Next();
}

void UAvaAutoSizeModifier::PostLoad()
{
	if (!bDeprecatedPropertiesMigrated
		&& ReferenceActor.ReferenceContainer == EAvaReferenceContainer::Other
		&& ReferenceActor.ReferenceActorWeak == nullptr)
	{
		ReferenceActor.ReferenceContainer = ReferenceContainer_DEPRECATED;
		ReferenceActor.ReferenceActorWeak = ReferenceActorWeak_DEPRECATED;
		ReferenceActor.bSkipHiddenActors = bIgnoreHiddenActors_DEPRECATED;
	}

	bDeprecatedPropertiesMigrated = true;

	Super::PostLoad();
}

#if WITH_EDITOR
void UAvaAutoSizeModifier::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName MemberName = PropertyChangedEvent.GetMemberPropertyName();

	static const FName ReferenceActorPropertyName = GET_MEMBER_NAME_CHECKED(UAvaAutoSizeModifier, ReferenceActor);
	static const FName PaddingPropertyName = GET_MEMBER_NAME_CHECKED(UAvaAutoSizeModifier, Padding);
	static const FName IncludeChildrenName = GET_MEMBER_NAME_CHECKED(UAvaAutoSizeModifier, bIncludeChildren);
	static const FName FitModePropertyName = GET_MEMBER_NAME_CHECKED(UAvaAutoSizeModifier, FitMode);

	if (MemberName == ReferenceActorPropertyName)
	{
		OnReferenceActorChanged();
	}
	else if (MemberName == PaddingPropertyName
		|| MemberName == FitModePropertyName
		|| MemberName == IncludeChildrenName)
	{
		MarkModifierDirty();
	}
}
#endif

void UAvaAutoSizeModifier::SetReferenceActor(const FAvaSceneTreeActor& InReferenceActor)
{
	if (ReferenceActor == InReferenceActor)
	{
		return;
	}

	ReferenceActor = InReferenceActor;
	OnReferenceActorChanged();
}

void UAvaAutoSizeModifier::SetFollowedAxis(int32 InFollowedAxis)
{
	if (FollowedAxis == InFollowedAxis)
	{
		return;
	}

	FollowedAxis = InFollowedAxis;
	MarkModifierDirty();
}

void UAvaAutoSizeModifier::SetPadding(const FMargin& InPadding)
{
	if (Padding != InPadding)
	{
		Padding = InPadding;

		MarkModifierDirty();
	}
}

void UAvaAutoSizeModifier::SetFitMode(const EAvaAutoSizeFitMode InFitMode)
{
	if (FitMode != InFitMode)
	{
		FitMode = InFitMode;

		MarkModifierDirty();
	}
}

void UAvaAutoSizeModifier::SetIncludeChildren(bool bInIncludeChildren)
{
	if (bIncludeChildren == bInIncludeChildren)
	{
		return;
	}

	bIncludeChildren = bInIncludeChildren;
	MarkModifierDirty();
}

void UAvaAutoSizeModifier::OnTransformUpdated(AActor* InActor, bool bInParentMoved)
{
	if (bInParentMoved)
	{
		return;
	}

	OnRenderStateUpdated(InActor, nullptr);
}

void UAvaAutoSizeModifier::OnRenderStateUpdated(AActor* InActor, UActorComponent* InComponent)
{
	const AActor* ActorModified = GetModifiedActor();
	if (!ActorModified)
	{
		return;
	}

	const AActor* TrackedActor = ReferenceActor.ReferenceActorWeak.Get();
	if (!TrackedActor)
	{
		return;
	}

	const bool bIsReferenceActor = InActor == TrackedActor;
	const bool bIsReferenceActorChildren = InActor->IsAttachedTo(TrackedActor);

	if (bIsReferenceActor
		|| (bIncludeChildren && bIsReferenceActorChildren))
	{
		const bool bIsModifierDirtyable = IsModifierDirtyable();

		// Only update if bounds changed
		if (bIsModifierDirtyable)
		{
			MarkModifierDirty();
		}
	}
}

void UAvaAutoSizeModifier::OnActorVisibilityChanged(AActor* InActor)
{
	OnRenderStateUpdated(InActor, nullptr);
}

void UAvaAutoSizeModifier::OnSceneTreeTrackedActorChanged(int32 InIdx, AActor* InPreviousActor, AActor* InNewActor)
{
	if (InNewActor == GetModifiedActor())
	{
		OnReferenceActorChanged();
		return;
	}

	// Untrack reference actor
	if (FAvaTransformUpdateModifierExtension* TransformExtension = GetExtension<FAvaTransformUpdateModifierExtension>())
	{
		TransformExtension->UntrackActor(InPreviousActor);
		TransformExtension->TrackActor(InNewActor, true);
	}

	MarkModifierDirty();
}

void UAvaAutoSizeModifier::OnSceneTreeTrackedActorChildrenChanged(int32 InIdx,
	const TSet<TWeakObjectPtr<AActor>>& InPreviousChildrenActors,
	const TSet<TWeakObjectPtr<AActor>>& InNewChildrenActors)
{
	if (bIncludeChildren)
	{
		if (IsModifierDirtyable())
		{
			MarkModifierDirty();
		}
	}
}

void UAvaAutoSizeModifier::OnReferenceActorChanged()
{
	if (ReferenceActor.ReferenceActorWeak.Get() == GetModifiedActor())
	{
		ReferenceActor.ReferenceActorWeak = nullptr;
	}

	if (const FAvaSceneTreeUpdateModifierExtension* SceneExtension = GetExtension<FAvaSceneTreeUpdateModifierExtension>())
	{
		SceneExtension->CheckTrackedActorUpdate(0);
	}
}

#undef LOCTEXT_NAMESPACE
