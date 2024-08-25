// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaDefs.h"
#include "AvaGeometryBaseModifier.h"
#include "AvaModifiersActorUtils.h"
#include "Extensions/AvaRenderStateUpdateModifierExtension.h"
#include "Extensions/AvaSceneTreeUpdateModifierExtension.h"
#include "Extensions/AvaTransformUpdateModifierExtension.h"
#include "Layout/Margin.h"
#include "AvaAutoSizeModifier.generated.h"

class UActorComponent;
class UAvaShape2DDynMeshBase;

UENUM()
enum class EAvaAutoSizeFitMode : uint8
{
	WidthAndHeight,
	WidthOnly,
	HeightOnly
};

/**
 * Adapts the modified actor geometry size/scale and position so that it acts as a background for a specified actor
 */
UCLASS(MinimalAPI, BlueprintType)
class UAvaAutoSizeModifier : public UAvaGeometryBaseModifier
	, public IAvaTransformUpdateHandler
	, public IAvaRenderStateUpdateHandler
	, public IAvaSceneTreeUpdateHandler
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Motion Design|Modifiers|AutoSize")
	AVALANCHEMODIFIERS_API void SetReferenceActor(const FAvaSceneTreeActor& InReferenceActor);

	UFUNCTION(BlueprintPure, Category = "Motion Design|Modifiers|AutoSize")
	const FAvaSceneTreeActor& GetReferenceActor() const
	{
		return ReferenceActor;
	}

	UFUNCTION(BlueprintCallable, Category = "Motion Design|Modifiers|AutoSize")
	AVALANCHEMODIFIERS_API void SetFollowedAxis(int32 InFollowedAxis);

	UFUNCTION(BlueprintPure, Category = "Motion Design|Modifiers|AutoSize")
	int32 GetFollowedAxis() const
	{
		return FollowedAxis;
	}

	/** Sets the actor affecting the modifier. This is user selectable if the Reference Container is set to "Other". */
	UFUNCTION(BlueprintCallable, Category = "Motion Design|Modifiers|AutoSize")
	AVALANCHEMODIFIERS_API void SetPadding(const FMargin& InPadding);

	/** Gets the actor affecting the modifier. This is user selectable if the Reference Container is set to "Other". */
	UFUNCTION(BlueprintPure, Category = "Motion Design|Modifiers|AutoSize")
	const FMargin& GetPadding() const
	{
		return Padding;
	}

	UFUNCTION(BlueprintCallable, Category = "Motion Design|Modifiers|AutoSize")
	AVALANCHEMODIFIERS_API void SetFitMode(const EAvaAutoSizeFitMode InFitMode);

	UFUNCTION(BlueprintPure, Category = "Motion Design|Modifiers|AutoSize")
	EAvaAutoSizeFitMode GetFitMode() const
	{
		return FitMode;
	}

	UFUNCTION(BlueprintCallable, Category = "Motion Design|Modifiers|AutoSize")
	AVALANCHEMODIFIERS_API void SetIncludeChildren(bool bInIncludeChildren);

	UFUNCTION(BlueprintPure, Category = "Motion Design|Modifiers|AutoSize")
	bool GetIncludeChildren() const
	{
		return bIncludeChildren;
	}

protected:
	//~ Begin UObject
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject

	//~ Begin UActorModifierCoreBase
	virtual bool IsModifierDirtyable() const override;
	virtual void OnModifierCDOSetup(FActorModifierCoreMetadata& InMetadata) override;
	virtual void OnModifiedActorTransformed() override;
	virtual void OnModifierAdded(EActorModifierCoreEnableReason InReason) override;
	virtual void OnModifierEnabled(EActorModifierCoreEnableReason InReason) override;
	virtual void OnModifierDisabled(EActorModifierCoreDisableReason InReason) override;
	virtual void Apply() override;
	//~ End UActorModifierCoreBase

	//~ Begin IAvaTransformUpdatedExtension
	virtual void OnTransformUpdated(AActor* InActor, bool bInParentMoved) override;
	//~ End IAvaTransformUpdatedExtension

	//~ Begin IAvaRenderStateUpdateExtension
	virtual void OnRenderStateUpdated(AActor* InActor, UActorComponent* InComponent) override;
	virtual void OnActorVisibilityChanged(AActor* InActor) override;
	//~ End IAvaRenderStateUpdateExtension

	//~ Begin IAvaSceneTreeUpdateModifierExtension
	virtual void OnSceneTreeTrackedActorChanged(int32 InIdx, AActor* InPreviousActor, AActor* InNewActor) override;
	virtual void OnSceneTreeTrackedActorChildrenChanged(int32 InIdx, const TSet<TWeakObjectPtr<AActor>>& InPreviousChildrenActors, const TSet<TWeakObjectPtr<AActor>>& InNewChildrenActors) override;
	virtual void OnSceneTreeTrackedActorDirectChildrenChanged(int32 InIdx, const TArray<TWeakObjectPtr<AActor>>& InPreviousChildrenActors, const TArray<TWeakObjectPtr<AActor>>& InNewChildrenActors) override {}
	virtual void OnSceneTreeTrackedActorParentChanged(int32 InIdx, const TArray<TWeakObjectPtr<AActor>>& InPreviousParentActor, const TArray<TWeakObjectPtr<AActor>>& InNewParentActor) override {}
	virtual void OnSceneTreeTrackedActorRearranged(int32 InIdx, AActor* InRearrangedActor) override {}
	//~ End IAvaSceneTreeUpdateModifierExtension

	void OnReferenceActorChanged();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetReferenceActor", Getter="GetReferenceActor", Category="AutoSize", meta=(ShowOnlyInnerProperties, AllowPrivateAccess="true"))
	FAvaSceneTreeActor ReferenceActor;

	/** The method for finding a reference actor based on it's position in the parent's hierarchy. */
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use ReferenceActor instead"))
	EAvaReferenceContainer ReferenceContainer_DEPRECATED = EAvaReferenceContainer::Other;

	/** The actor affecting the modifier. This is user selectable if the Reference Container is set to "Other". */
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use ReferenceActor instead"))
	TWeakObjectPtr<AActor> ReferenceActorWeak_DEPRECATED = nullptr;

	/** If true, will search for the next visible actor based on the selected reference container. */
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use ReferenceActor instead"))
	bool bIgnoreHiddenActors_DEPRECATED = false;

	/** Which axis should we follow, if none selected, it will not follow */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetFollowedAxis", Getter="GetFollowedAxis", Category="AutoSize", meta=(Bitmask, BitmaskEnum="/Script/AvalancheModifiers.EAvaModifiersAxis", AllowPrivateAccess="true"))
	int32 FollowedAxis = static_cast<int32>(
		EAvaModifiersAxis::Y |
		EAvaModifiersAxis::Z
	);

	/* Padding around reference bounds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetPadding", Getter="GetPadding", Interp, Category="AutoSize", meta=(AllowPrivateAccess="true"))
	FMargin Padding = FMargin(0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetFitMode", Getter="GetFitMode", Category="AutoSize", meta=(AllowPrivateAccess="true"))
	EAvaAutoSizeFitMode FitMode = EAvaAutoSizeFitMode::WidthAndHeight;

	/** If true, will include children bounds too and compute the new size */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetIncludeChildren", Getter="GetIncludeChildren", Category="AutoSize", meta=(AllowPrivateAccess="true"))
	bool bIncludeChildren = true;

private:
	UPROPERTY()
	FVector2D PreModifierShapeDynMesh2DSize;

	UPROPERTY()
	TWeakObjectPtr<UAvaShape2DDynMeshBase> ShapeDynMesh2DWeak;

	UPROPERTY(Transient, DuplicateTransient, TextExportTransient)
	FVector CachedFollowLocation = FVector::ZeroVector;

	UPROPERTY(Transient, DuplicateTransient, TextExportTransient)
	FBox CachedReferenceBounds = FBox(EForceInit::ForceInit);

	UPROPERTY()
	bool bDeprecatedPropertiesMigrated = false;
};
