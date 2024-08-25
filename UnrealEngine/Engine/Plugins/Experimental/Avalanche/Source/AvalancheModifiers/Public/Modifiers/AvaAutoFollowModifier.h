// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaDefs.h"
#include "AvaAttachmentBaseModifier.h"
#include "AvaModifiersActorUtils.h"
#include "Components/SceneComponent.h"
#include "Extensions/AvaRenderStateUpdateModifierExtension.h"
#include "Extensions/AvaTransformUpdateModifierExtension.h"
#include "AvaAutoFollowModifier.generated.h"

class AActor;
class UActorComponent;

/**
 * Moves the modifying actor along with a specified actor relative to the specified actor's bounds.
 */
UCLASS(MinimalAPI, BlueprintType)
class UAvaAutoFollowModifier : public UAvaAttachmentBaseModifier
	, public IAvaTransformUpdateHandler
	, public IAvaRenderStateUpdateHandler
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Motion Design|Modifiers|AutoFollow")
	AVALANCHEMODIFIERS_API void SetReferenceActor(const FAvaSceneTreeActor& InReferenceActor);

	UFUNCTION(BlueprintPure, Category = "Motion Design|Modifiers|AutoFollow")
	const FAvaSceneTreeActor& GetReferenceActor() const
	{
		return ReferenceActor;
	}

	UFUNCTION(BlueprintCallable, Category = "Motion Design|Modifiers|AutoFollow")
	AVALANCHEMODIFIERS_API void SetFollowedAxis(int32 InFollowedAxis);

	UFUNCTION(BlueprintPure, Category = "Motion Design|Modifiers|AutoFollow")
	int32 GetFollowedAxis() const
	{
		return FollowedAxis;
	}

	/** Sets the distance from this actor to the followed actor. */
	UFUNCTION(BlueprintCallable, Category = "Motion Design|Modifiers|AutoFollow")
	AVALANCHEMODIFIERS_API void SetDefaultDistance(const FVector& NewDefaultDistance);

	/** Gets the distance from this actor to the followed actor. */
	UFUNCTION(BlueprintPure, Category = "Motion Design|Modifiers|AutoFollow")
	FVector GetDefaultDistance() const
	{
		return DefaultDistance;
	}

	/** Sets the maximum distance from this actor to the followed actor. */
	UFUNCTION(BlueprintCallable, Category = "Motion Design|Modifiers|AutoFollow")
	AVALANCHEMODIFIERS_API void SetMaxDistance(const FVector& NewMaxDistance);

	/** Gets the maximum distance from this actor to the followed actor. */
	UFUNCTION(BlueprintPure, Category = "Motion Design|Modifiers|AutoFollow")
	FVector GetMaxDistance() const
	{
		return MaxDistance;
	}

	/** Sets the percent % progress from the maximum distance to the default distance. */
	UFUNCTION(BlueprintCallable, Category = "Motion Design|Modifiers|AutoFollow")
	AVALANCHEMODIFIERS_API void SetProgress(const FVector& NewProgress);

	/** Gets the percent % progress from the maximum distance to the default distance. */
	UFUNCTION(BlueprintPure, Category = "Motion Design|Modifiers|AutoFollow")
	FVector GetProgress() const
	{
		return Progress;
	}

	/** Sets the alignment for the followed actor's center. */
	UFUNCTION(BlueprintCallable, Category = "Motion Design|Modifiers|AutoFollow")
	AVALANCHEMODIFIERS_API void SetFollowedAlignment(const FAvaAnchorAlignment& NewFollowedAlignment);

	/** Gets the alignment for the followed actor's center. */
	UFUNCTION(BlueprintPure, Category = "Motion Design|Modifiers|AutoFollow")
	FAvaAnchorAlignment GetFollowedAlignment() const
	{
		return FollowedAlignment;
	}

	/** Sets the alignment for this actor's center. */
	UFUNCTION(BlueprintCallable, Category = "Motion Design|Modifiers|AutoFollow")
	AVALANCHEMODIFIERS_API void SetLocalAlignment(const FAvaAnchorAlignment& NewLocalAlignment);

	/** Gets the alignment for this actor's center. */
	UFUNCTION(BlueprintPure, Category = "Motion Design|Modifiers|AutoFollow")
	FAvaAnchorAlignment GetLocalAlignment() const
	{
		return LocalAlignment;
	}

	/** Sets the axis direction to offset this actor from the followed actor's bounds. */
	UFUNCTION(BlueprintCallable, Category = "Motion Design|Modifiers|AutoFollow")
	AVALANCHEMODIFIERS_API void SetOffsetAxis(const FVector& NewOffsetAxis);

	/** Gets the axis direction to offset this actor from the followed actor's bounds. */
	UFUNCTION(BlueprintPure, Category = "Motion Design|Modifiers|AutoFollow")
	FVector GetOffsetAxis() const
	{
		return OffsetAxis;
	}

protected:
	//~ Begin UObject
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostTransacted(const FTransactionObjectEvent& TransactionEvent) override;
#endif
	//~ End UObject

	//~ Begin UActorModifierCoreBase
	virtual bool IsModifierDirtyable() const override;
	virtual void OnModifierCDOSetup(FActorModifierCoreMetadata& InMetadata) override;
	virtual void OnModifierAdded(EActorModifierCoreEnableReason InReason) override;
	virtual void OnModifierEnabled(EActorModifierCoreEnableReason InReason) override;
	virtual void OnModifierDisabled(EActorModifierCoreDisableReason InReason) override;
	virtual void OnModifiedActorTransformed() override;
	virtual void Apply() override;
	//~ End UActorModifierCoreBase

	//~ Begin IAvaTransformUpdatedExtension
	virtual void OnTransformUpdated(AActor* InActor, bool bInParentMoved) override;
	//~ End IAvaTransformUpdatedExtension

	//~ Begin IAvaRenderStateUpdateExtension
	virtual void OnRenderStateUpdated(AActor* InActor, UActorComponent* InComponent) override;
	virtual void OnActorVisibilityChanged(AActor* InActor) override {}
	//~ End IAvaRenderStateUpdateExtension

	//~ Begin IAvaSceneTreeUpdateModifierExtension
	virtual void OnSceneTreeTrackedActorChanged(int32 InIdx, AActor* InPreviousActor, AActor* InNewActor) override;
	virtual void OnSceneTreeTrackedActorChildrenChanged(int32 InIdx, const TSet<TWeakObjectPtr<AActor>>& InPreviousChildrenActors, const TSet<TWeakObjectPtr<AActor>>& InNewChildrenActors) override;
	//~ End IAvaSceneTreeUpdateModifierExtension

	void OnReferenceActorChanged();

	void OnFollowedAxisChanged();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetReferenceActor", Getter="GetReferenceActor", Category="AutoFollow", meta=(ShowOnlyInnerProperties, AllowPrivateAccess="true"))
	FAvaSceneTreeActor ReferenceActor;

	/** The method for finding a reference actor based on it's position in the parent's hierarchy. */
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use ReferenceActor instead"))
	EAvaReferenceContainer ReferenceContainer_DEPRECATED = EAvaReferenceContainer::Other;

	/** The actor being followed by the modifier. This is user selectable if the Reference Container is set to "Other". */
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use ReferenceActor instead"))
	TWeakObjectPtr<AActor> ReferenceActorWeak_DEPRECATED = nullptr;

	/** If true, will search for the next visible actor based on the selected reference container. */
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use ReferenceActor instead"))
	bool bIgnoreHiddenActors_DEPRECATED = false;

	/** Which axis should we follow */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetFollowedAxis", Getter="GetFollowedAxis", Category="AutoFollow", meta=(Bitmask, BitmaskEnum="/Script/AvalancheModifiers.EAvaModifiersAxis", AllowPrivateAccess="true"))
	int32 FollowedAxis = static_cast<int32>(
		EAvaModifiersAxis::Y |
		EAvaModifiersAxis::Z
	);

	/** Based on followed axis, the direction to offset this actor from the followed actor's bounds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetOffsetAxis", Getter="GetOffsetAxis", Interp, Category="AutoFollow", meta=(UIMin="-1.0", UIMax="1.0", AllowPrivateAccess="true"))
	FVector OffsetAxis = FVector(0, 1, 0);

	/** The alignment for the followed actor's center. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetFollowedAlignment", Getter="GetFollowedAlignment", Category="AutoFollow", meta=(AllowPrivateAccess="true"))
	FAvaAnchorAlignment FollowedAlignment;

	/** The alignment for this actor's center. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetLocalAlignment", Getter="GetLocalAlignment", Category="AutoFollow", meta=(AllowPrivateAccess="true"))
	FAvaAnchorAlignment LocalAlignment;

	/** The distance from this actor to the followed actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetDefaultDistance", Getter="GetDefaultDistance", Interp, Category="AutoFollow", meta=(AllowPrivateAccess="true"))
	FVector DefaultDistance;

	/** The maximum distance from this actor to the followed actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetMaxDistance", Getter="GetMaxDistance", Interp, Category="AutoFollow", meta=(AllowPrivateAccess="true"))
	FVector MaxDistance;

	/** Percent % progress from the maximum distance to the default distance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetProgress", Getter="GetProgress", Interp, Category="AutoFollow", meta=(ClampMin="0.0", UIMin="0.0", ClampMax="100.0", UIMax="100.0", AllowPrivateAccess="true"))
	FVector Progress;

private:
	UPROPERTY(Transient, DuplicateTransient, TextExportTransient)
	FVector CachedFollowLocation = FVector::ZeroVector;

	UPROPERTY(Transient, DuplicateTransient, TextExportTransient)
	FBox CachedReferenceBounds = FBox(EForceInit::ForceInit);

	UPROPERTY(Transient, DuplicateTransient, TextExportTransient)
	FBox CachedModifiedBounds = FBox(EForceInit::ForceInit);

	UPROPERTY()
	bool bDeprecatedPropertiesMigrated = false;
};