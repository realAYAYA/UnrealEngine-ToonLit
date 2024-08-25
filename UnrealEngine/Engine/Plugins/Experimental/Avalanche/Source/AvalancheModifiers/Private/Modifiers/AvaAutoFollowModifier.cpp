// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modifiers/AvaAutoFollowModifier.h"

#include "AvaModifiersActorUtils.h"
#include "AvaActorUtils.h"
#include "Extensions/AvaSceneTreeUpdateModifierExtension.h"
#include "Extensions/AvaTransformUpdateModifierExtension.h"
#include "GameFramework/Actor.h"
#include "Shared/AvaTransformModifierShared.h"

#define LOCTEXT_NAMESPACE "AvaAutoFollowModifier"

bool UAvaAutoFollowModifier::IsModifierDirtyable() const
{
	AActor* const ActorModified = GetModifiedActor();
	AActor* const FollowedActor = ReferenceActor.ReferenceActorWeak.Get();

	if (!IsValid(FollowedActor) || !IsValid(ActorModified))
	{
		return Super::IsModifierDirtyable();
	}
	
	const FBox ReferenceActorLocalBounds = FAvaModifiersActorUtils::GetActorsBounds(FollowedActor, true);
	const FBox ModifiedActorLocalBounds = FAvaModifiersActorUtils::GetActorsBounds(ActorModified, true);
	
	// Compare bounds/center to detect changes
	if (ReferenceActorLocalBounds.Equals(CachedReferenceBounds, 0.01)
		&& ModifiedActorLocalBounds.Equals(CachedModifiedBounds, 0.01))
	{
		return Super::IsModifierDirtyable();
	}
	
	return true;
}

void UAvaAutoFollowModifier::OnModifierCDOSetup(FActorModifierCoreMetadata& InMetadata)
{
	Super::OnModifierCDOSetup(InMetadata);
	
	InMetadata.AllowTick(true);
	InMetadata.SetName(TEXT("AutoFollow"));
	InMetadata.SetCategory(TEXT("Layout"));
#if WITH_EDITOR
	InMetadata.SetDescription(LOCTEXT("ModifierDescription", "Positions an actor relative to another actor using their bounds"));
#endif
}

void UAvaAutoFollowModifier::OnModifierAdded(EActorModifierCoreEnableReason InReason)
{
	Super::OnModifierAdded(InReason);

	AddExtension<FAvaTransformUpdateModifierExtension>(this);
	AddExtension<FAvaRenderStateUpdateModifierExtension>(this);

	if (FAvaSceneTreeUpdateModifierExtension* SceneExtension = GetExtension<FAvaSceneTreeUpdateModifierExtension>())
	{
		SceneExtension->TrackSceneTree(0, &ReferenceActor);
	}
}

void UAvaAutoFollowModifier::OnModifierEnabled(EActorModifierCoreEnableReason InReason)
{
	Super::OnModifierEnabled(InReason);

	// Save actor layout state
	if (UAvaTransformModifierShared* LayoutShared = GetShared<UAvaTransformModifierShared>(true))
	{
		LayoutShared->SaveActorState(this, GetModifiedActor());
	}
}

void UAvaAutoFollowModifier::OnModifierDisabled(EActorModifierCoreDisableReason InReason)
{
	Super::OnModifierDisabled(InReason);

	// Restore actor layout state
	if (UAvaTransformModifierShared* LayoutShared = GetShared<UAvaTransformModifierShared>(false))
	{
		LayoutShared->RestoreActorState(this, GetModifiedActor());
	}
}

void UAvaAutoFollowModifier::OnModifiedActorTransformed()
{
	const AActor* const FollowedActor = ReferenceActor.ReferenceActorWeak.Get();
	const AActor* const ActorModified = GetModifiedActor();
	
	if (!FollowedActor || !ActorModified)
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

void UAvaAutoFollowModifier::Apply()
{
	AActor* const ModifyActor = GetModifiedActor();
	
	AActor* const FollowedActor = ReferenceActor.ReferenceActorWeak.Get();
	if (!IsValid(FollowedActor))
	{
		Next();
		return;
	}

	const FVector FollowAxisVector = FAvaModifiersActorUtils::GetVectorAxis(FollowedAxis);
	if (FollowAxisVector.IsNearlyZero())
	{
		Next();
		return;
	}

	// Get Padding to use
	const FVector DistancePadding = FMath::Lerp<FVector>(DefaultDistance, MaxDistance, Progress / 100.0f);

	// Get actors bounds
	CachedReferenceBounds = FAvaModifiersActorUtils::GetActorsBounds(FollowedActor, true);
	CachedModifiedBounds = FAvaModifiersActorUtils::GetActorsBounds(ModifyActor, true);

	// Check if bounds are valid
	const bool bReferenceActorZeroSizeBounds = CachedReferenceBounds.GetSize().IsNearlyZero();
	const bool bModifyActorZeroSizeBounds = CachedModifiedBounds.GetSize().IsNearlyZero();

	// Get actors location 
	const FVector ReferenceActorLocation = FollowedActor->GetActorLocation();
	const FVector ModifyActorLocation = ModifyActor->GetActorLocation();
	
	// Get bounds center (pivot)
	const FVector ReferenceActorCenter = !bReferenceActorZeroSizeBounds ? CachedReferenceBounds.GetCenter() : ReferenceActorLocation;
	const FVector ModifierActorCenter = !bModifyActorZeroSizeBounds ? CachedModifiedBounds.GetCenter() : ModifyActorLocation;

	// Get bounds extents
	const FVector ReferenceActorExtent = CachedReferenceBounds.GetExtent();
	const FVector ModifierActorExtent = CachedModifiedBounds.GetExtent();
	
	// using world space extents so that the modified actor is moved also taking into account reference actor rotation
	const FVector ReferenceActorLocalOffset = ReferenceActorExtent * OffsetAxis;
	const FVector ModifierActorLocalOffset = ModifierActorExtent * OffsetAxis;
	
	// Use user alignments for followed and modify actors
    const FVector ReferenceActorBoundsOffset = FollowedAlignment.LocalBoundsOffset(FBox(-ReferenceActorExtent, ReferenceActorExtent));
    const FVector ModifierActorBoundsOffset = LocalAlignment.LocalBoundsOffset(FBox(-ModifierActorExtent, ModifierActorExtent));
	
	// this offset can be non-zero when the actor pivot and bounds origin do not coincide: we need to take this into account
	const FVector ReferenceActorPivotToBoundsOffset = ReferenceActorLocation - ReferenceActorCenter;
	const FVector ModifiedActorPivotToBoundsOffset = ModifyActorLocation - ModifierActorCenter;

	const FVector OffsetLocation =
		// Reference actor extent - reference actor alignment
		ReferenceActorLocalOffset - ReferenceActorBoundsOffset
		// Modified actor extent + modified actor alignment
		+ ModifierActorLocalOffset + ModifierActorBoundsOffset
		// Distance progress offset
		+ DistancePadding
		// finally removing any existing pivot to bounds offset
		+ (ModifiedActorPivotToBoundsOffset - ReferenceActorPivotToBoundsOffset);

	// target location needs to start from reference actor bounds location + proper location offset
	CachedFollowLocation = ModifyActorLocation + (ReferenceActorLocation - ModifyActorLocation + OffsetLocation) * FollowAxisVector;
	
	ModifyActor->SetActorLocation(CachedFollowLocation);
	
	Next();
}

void UAvaAutoFollowModifier::PostLoad()
{
	if (!bDeprecatedPropertiesMigrated)
	{
		ReferenceActor.ReferenceContainer = ReferenceContainer_DEPRECATED;
		ReferenceActor.ReferenceActorWeak = ReferenceActorWeak_DEPRECATED;
		ReferenceActor.bSkipHiddenActors = bIgnoreHiddenActors_DEPRECATED;
		
		bDeprecatedPropertiesMigrated = true;
	}
	
	Super::PostLoad();
}

#if WITH_EDITOR
void UAvaAutoFollowModifier::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName MemberName = PropertyChangedEvent.GetMemberPropertyName();

	static const FName ReferenceActorPropertyName = GET_MEMBER_NAME_CHECKED(UAvaAutoFollowModifier, ReferenceActor);
	static const FName DefaultDistancePropertyName = GET_MEMBER_NAME_CHECKED(UAvaAutoFollowModifier, DefaultDistance);
	static const FName MaxDistancePropertyName = GET_MEMBER_NAME_CHECKED(UAvaAutoFollowModifier, MaxDistance);
	static const FName ProgressPropertyName = GET_MEMBER_NAME_CHECKED(UAvaAutoFollowModifier, Progress);
	static const FName FollowerAlignmentPropertyName = GET_MEMBER_NAME_CHECKED(UAvaAutoFollowModifier, FollowedAlignment);
	static const FName LocalAlignmentPropertyName = GET_MEMBER_NAME_CHECKED(UAvaAutoFollowModifier, LocalAlignment);
	static const FName OffsetAxisPropertyName = GET_MEMBER_NAME_CHECKED(UAvaAutoFollowModifier, OffsetAxis);
	static const FName FollowedAxisPropertyName = GET_MEMBER_NAME_CHECKED(UAvaAutoFollowModifier, FollowedAxis);
	
	if (MemberName == ReferenceActorPropertyName)
	{
		OnReferenceActorChanged();
	}
	else if (MemberName == DefaultDistancePropertyName
		|| MemberName == MaxDistancePropertyName
		|| MemberName == ProgressPropertyName
		|| MemberName == FollowerAlignmentPropertyName
		|| MemberName == LocalAlignmentPropertyName
		|| MemberName == OffsetAxisPropertyName)
	{
		MarkModifierDirty();
	}
	else if (MemberName == FollowedAxisPropertyName)
	{
		OnFollowedAxisChanged();	
	}
}

void UAvaAutoFollowModifier::PostTransacted(const FTransactionObjectEvent& TransactionEvent)
{
	OnReferenceActorChanged();
	
	Super::PostTransacted(TransactionEvent);
}
#endif // WITH_EDITOR

void UAvaAutoFollowModifier::SetReferenceActor(const FAvaSceneTreeActor& InReferenceActor)
{
	if (ReferenceActor == InReferenceActor)
	{
		return;
	}
	
	ReferenceActor = InReferenceActor;
	OnReferenceActorChanged();
}

void UAvaAutoFollowModifier::SetFollowedAxis(int32 InFollowedAxis)
{
	if (FollowedAxis == InFollowedAxis)
	{
		return;
	}

	FollowedAxis = InFollowedAxis;
	OnFollowedAxisChanged();
}

void UAvaAutoFollowModifier::OnFollowedAxisChanged()
{
	const FVector FollowedAxisVector = FAvaModifiersActorUtils::GetVectorAxis(FollowedAxis);

	LocalAlignment.bUseDepth = FollowedAlignment.bUseDepth = FollowedAxisVector.X != 0;
	LocalAlignment.bUseHorizontal = FollowedAlignment.bUseHorizontal = FollowedAxisVector.Y != 0;
	LocalAlignment.bUseVertical = FollowedAlignment.bUseVertical = FollowedAxisVector.Z != 0;

	MarkModifierDirty();
}

void UAvaAutoFollowModifier::SetDefaultDistance(const FVector& NewDefaultDistance)
{
	DefaultDistance = NewDefaultDistance;

	MarkModifierDirty();
}

void UAvaAutoFollowModifier::SetMaxDistance(const FVector& NewMaxDistance)
{
	MaxDistance = NewMaxDistance;

	MarkModifierDirty();
}

void UAvaAutoFollowModifier::SetProgress(const FVector& NewProgress)
{
	Progress = NewProgress;

	MarkModifierDirty();
}

void UAvaAutoFollowModifier::SetFollowedAlignment(const FAvaAnchorAlignment& NewFollowedAlignment)
{
	FollowedAlignment = NewFollowedAlignment;

	MarkModifierDirty();
}

void UAvaAutoFollowModifier::SetLocalAlignment(const FAvaAnchorAlignment& NewLocalAlignment)
{
	LocalAlignment = NewLocalAlignment;

	MarkModifierDirty();
}

void UAvaAutoFollowModifier::SetOffsetAxis(const FVector& NewOffsetAxis)
{
	OffsetAxis = NewOffsetAxis;

	MarkModifierDirty();
}

void UAvaAutoFollowModifier::OnTransformUpdated(AActor* InActor, bool bInParentMoved)
{
	const AActor* ActorModified = GetModifiedActor();
	if (!IsValid(ActorModified))
	{
		return;
	}
	
	const AActor* FollowedActor = ReferenceActor.ReferenceActorWeak.Get();
	const bool bIsAttachedToReferenceActor = InActor->IsAttachedTo(FollowedActor);
	const bool bIsReferenceActor = InActor == FollowedActor;
	
	if (IsValid(FollowedActor) && (bIsAttachedToReferenceActor || bIsReferenceActor))
	{
		MarkModifierDirty();
	}
}

void UAvaAutoFollowModifier::OnRenderStateUpdated(AActor* InActor, UActorComponent* InComponent)
{
	const AActor* ActorModified = GetModifiedActor();
	if (!IsValid(ActorModified))
	{
		return;
	}
	
	const AActor* FollowedActor = ReferenceActor.ReferenceActorWeak.Get();
	const UAvaTransformModifierShared* LayoutShared = GetShared<UAvaTransformModifierShared>(false);
	
	if (!InActor || !FollowedActor || !LayoutShared)
	{
		return;
	}
	
	const bool bIsReferenceActor = FollowedActor == InActor;
	const bool bIsAttachedToReferenceActor = InActor->IsAttachedTo(FollowedActor);
	const bool bModifierDirtyable = IsModifierDirtyable();

	if (!bIsReferenceActor && !bIsAttachedToReferenceActor && !bModifierDirtyable)
	{
		return;
	}
	
	MarkModifierDirty();
}

void UAvaAutoFollowModifier::OnSceneTreeTrackedActorChanged(int32 InIdx, AActor* InPreviousActor, AActor* InNewActor)
{
	Super::OnSceneTreeTrackedActorChanged(InIdx, InPreviousActor, InNewActor);

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

void UAvaAutoFollowModifier::OnSceneTreeTrackedActorChildrenChanged(int32 InIdx,
	const TSet<TWeakObjectPtr<AActor>>& InPreviousChildrenActors,
	const TSet<TWeakObjectPtr<AActor>>& InNewChildrenActors)
{
	Super::OnSceneTreeTrackedActorChildrenChanged(InIdx, InPreviousChildrenActors, InNewChildrenActors);

	// Untrack reference actor children
	if (FAvaTransformUpdateModifierExtension* TransformExtension = GetExtension<FAvaTransformUpdateModifierExtension>())
	{
		TransformExtension->UntrackActors(InPreviousChildrenActors);
		TransformExtension->TrackActors(InNewChildrenActors, false);
	}

	ChildrenActorsWeak = InNewChildrenActors;
	
	MarkModifierDirty();
}

void UAvaAutoFollowModifier::OnReferenceActorChanged()
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
