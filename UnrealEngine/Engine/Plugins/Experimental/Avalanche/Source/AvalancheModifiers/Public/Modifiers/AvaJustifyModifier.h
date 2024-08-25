// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaArrangeBaseModifier.h"
#include "Components/ActorComponent.h"
#include "AvaJustifyModifier.generated.h"

UENUM(BlueprintType)
enum class EAvaJustifyVertical : uint8
{
	None,
	Top,
	Center,
	Bottom,
};

UENUM(BlueprintType)
enum class EAvaJustifyHorizontal : uint8
{
	None,
	Left,
	Center,
	Right,
};

UENUM(BlueprintType)
enum class EAvaJustifyDepth : uint8
{
	None,
	Front,
	Center,
	Back
};

/**
 * Justify Modifier
 * 
 * Aligns child actors, based on their bounding box, according to the specified justification
 */
UCLASS(MinimalAPI, BlueprintType)
class UAvaJustifyModifier : public UAvaArrangeBaseModifier
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Justify")
	AVALANCHEMODIFIERS_API void SetHorizontalAlignment(EAvaJustifyHorizontal InHorizontalAlignment);

	UFUNCTION(BlueprintPure, Category = "Justify")
	EAvaJustifyHorizontal GetHorizontalAlignment() const
	{
		return HorizontalAlignment;
	}

	UFUNCTION(BlueprintCallable, Category = "Justify")
	AVALANCHEMODIFIERS_API void SetVerticalAlignment(EAvaJustifyVertical InVerticalAlignment);

	UFUNCTION(BlueprintPure, Category = "Justify")
	EAvaJustifyVertical GetVerticalAlignment() const
	{
		return VerticalAlignment;
	}

	UFUNCTION(BlueprintCallable, Category = "Justify")
	AVALANCHEMODIFIERS_API void SetDepthAlignment(EAvaJustifyDepth InDepthAlignment);

	UFUNCTION(BlueprintPure, Category = "Justify")
	EAvaJustifyDepth GetDepthAlignment() const
	{
		return DepthAlignment;
	}

	UFUNCTION(BlueprintCallable, Category = "Justify")
	AVALANCHEMODIFIERS_API void SetHorizontalAnchor(float InHorizontalAnchor);

	UFUNCTION(BlueprintPure, Category = "Justify")
	float GetHorizontalAnchor() const
	{
		return HorizontalAnchor;
	}

	UFUNCTION(BlueprintCallable, Category = "Justify")
	AVALANCHEMODIFIERS_API void SetVerticalAnchor(float InVerticalAnchor);

	UFUNCTION(BlueprintPure, Category = "Justify")
	float GetVerticalAnchor() const
	{
		return VerticalAnchor;
	}

	UFUNCTION(BlueprintCallable, Category = "Justify")
	AVALANCHEMODIFIERS_API void SetDepthAnchor(float InDepthAnchor);

	UFUNCTION(BlueprintPure, Category = "Justify")
	float GetDepthAnchor() const
	{
		return DepthAnchor;
	}

protected:
	//~ Begin UObject
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent) override;
#endif
	//~ End UObject

	//~ Begin UActorModifierCoreBase
	virtual void OnModifierCDOSetup(FActorModifierCoreMetadata& InMetadata) override;
	virtual bool IsModifierDirtyable() const override;
	virtual void OnModifiedActorTransformed() override;
	virtual void Apply() override;
	//~ End UActorModifierCoreBase

	//~ Begin IAvaSceneTreeUpdateModifierExtension
	virtual void OnSceneTreeTrackedActorDirectChildrenChanged(int32 InIdx, const TArray<TWeakObjectPtr<AActor>>& InPreviousChildrenActors, const TArray<TWeakObjectPtr<AActor>>& InNewChildrenActors) override;
	//~ End IAvaSceneTreeUpdateModifierExtension

	//~ Begin IAvaRenderStateUpdateExtension
	virtual void OnRenderStateUpdated(AActor* InActor, UActorComponent* InComponent) override;
	virtual void OnActorVisibilityChanged(AActor* InActor) override;
	//~ End IAvaRenderStateUpdateExtension

	//~ Begin IAvaTransformUpdateExtension
	virtual void OnTransformUpdated(AActor* InActor, bool bInParentMoved) override;
	//~ End IAvaTransformUpdateExtension

	// Get tracked actors from children actors
	void GetChildrenActors(TSet<TWeakObjectPtr<AActor>>& OutChildren) const;
	void GetTrackedActors(const TSet<TWeakObjectPtr<AActor>>& InChildrenActors, TArray<TWeakObjectPtr<const AActor>>& OutTrackedActors) const;

	bool HasDepthAlignment() const;
	bool HasHorizontalAlignment() const;
	bool HasVerticalAlignment() const;

	FVector GetConstraintVector(const FVector& InBoundsCenter, const FVector& InModifiedActorPosition) const;
	FVector MakeConstrainedAxisVector() const;
	FVector GetAnchorOffset() const;
	FVector GetAlignmentOffset(const FVector& InExtent) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetHorizontalAlignment", Getter="GetHorizontalAlignment", Category="Justify", meta=(AllowPrivateAccess="true"))
	EAvaJustifyHorizontal HorizontalAlignment;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetVerticalAlignment", Getter="GetVerticalAlignment", Category="Justify", meta=(AllowPrivateAccess="true"))
	EAvaJustifyVertical VerticalAlignment;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetDepthAlignment", Getter="GetDepthAlignment", Category="Justify", meta=(AllowPrivateAccess="true"))
	EAvaJustifyDepth DepthAlignment;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetHorizontalAnchor", Getter="GetHorizontalAnchor", Interp, Category="Justify", meta=(EditCondition="HorizontalAlignment != EAvaJustifyHorizontal::None", EditConditionHides, AllowPrivateAccess="true"))
	float HorizontalAnchor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetVerticalAnchor", Getter="GetVerticalAnchor", Interp, Category="Justify", meta=(EditCondition="VerticalAlignment != EAvaJustifyVertical::None", EditConditionHides, AllowPrivateAccess="true"))
	float VerticalAnchor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetDepthAnchor", Getter="GetDepthAnchor", Interp, Category="Justify", meta=(EditCondition = "DepthAlignment != EAvaJustifyDepth::None", EditConditionHides, AllowPrivateAccess="true"))
	float DepthAnchor;

private:
	/** Cached actors bounds to detect a change in tick */
	UPROPERTY(Transient, DuplicateTransient, TextExportTransient)
	FBox CachedTrackedBounds = FBox(EForceInit::ForceInit);
};
