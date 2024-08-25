// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaGeometryBaseModifier.h"
#include "AvaModifiersPreviewPlane.h"
#include "AvaMirrorModifier.generated.h"

class UStaticMesh;

UCLASS(MinimalAPI, BlueprintType)
class UAvaMirrorModifier : public UAvaGeometryBaseModifier
{
	GENERATED_BODY()

public:
	AVALANCHEMODIFIERS_API void SetMirrorFramePosition(const FVector& InMirrorFramePosition);
	const FVector& GetMirrorFramePosition() const
	{
		return MirrorFramePosition;
	}

	AVALANCHEMODIFIERS_API void SetMirrorFrameRotation(const FRotator& InMirrorFrameRotation);
	const FRotator& GetMirrorFrameRotation() const
	{
		return MirrorFrameRotation;
	}

	AVALANCHEMODIFIERS_API void SetApplyPlaneCut(bool bInApplyPlaneCut);
	bool GetApplyPlaneCut() const
	{
		return bApplyPlaneCut;
	}

	AVALANCHEMODIFIERS_API void SetFlipCutSide(bool bInFlipCutSide);
	bool GetFlipCutSide() const
	{
		return bFlipCutSide;
	}

	AVALANCHEMODIFIERS_API void SetWeldAlongPlane(bool bInWeldAlongPlane);
	bool GetWeldAlongPlane() const
	{
		return bWeldAlongPlane;
	}

protected:
	//~ Begin UObject
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject

	//~ Begin UActorModifierCoreBase
	virtual void OnModifierCDOSetup(FActorModifierCoreMetadata& InMetadata) override;
	virtual void OnModifierAdded(EActorModifierCoreEnableReason InReason) override;
	virtual void Apply() override;
	virtual void OnModifierDisabled(EActorModifierCoreDisableReason InReason) override;
	virtual void OnModifierRemoved(EActorModifierCoreDisableReason InReason) override;
	//~ End UActorModifierCoreBase

	void OnMirrorFrameChanged();
	void OnMirrorOptionChanged();

#if WITH_EDITOR
	void CreatePreviewComponent();
	void DestroyPreviewComponent();
	void UpdatePreviewComponent();

	void OnShowMirrorFrameChanged();
#endif

    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetMirrorFramePosition", Getter="GetMirrorFramePosition", Category="Mirror", meta=(AllowPrivateAccess="true"))
	FVector MirrorFramePosition = FVector::ZeroVector;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetMirrorFrameRotation", Getter="GetMirrorFrameRotation", Category="Mirror", meta=(AllowPrivateAccess="true"))
	FRotator MirrorFrameRotation = FRotator(0, 0, 90);

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetApplyPlaneCut", Getter="GetApplyPlaneCut", Category="Mirror", meta=(AllowPrivateAccess="true"))
	bool bApplyPlaneCut = true;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetFlipCutSide", Getter="GetFlipCutSide", Category="Mirror", meta=(AllowPrivateAccess="true"))
	bool bFlipCutSide = false;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetWeldAlongPlane", Getter="GetWeldAlongPlane", Category="Mirror", meta=(AllowPrivateAccess="true"))
	bool bWeldAlongPlane = true;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Mirror", meta=(AllowPrivateAccess="true"))
	bool bShowMirrorFrame = false;

private:
	UPROPERTY(Transient, DuplicateTransient, TextExportTransient)
	FAvaModifierPreviewPlane PreviewPlane;
#endif
};
