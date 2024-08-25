// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaGeometryBaseModifier.h"
#include "AvaModifiersPreviewPlane.h"
#include "AvaPlaneCutModifier.generated.h"

/** This modifier cuts a shape based on a 2D plane */
UCLASS(MinimalAPI, BlueprintType)
class UAvaPlaneCutModifier : public UAvaGeometryBaseModifier
{
	GENERATED_BODY()

public:
	AVALANCHEMODIFIERS_API void SetPlaneOrigin(float InOrigin);
	float GetPlaneOrigin() const
	{
		return PlaneOrigin;
	}

	AVALANCHEMODIFIERS_API void SetPlaneRotation(const FRotator& InRotation);
	const FRotator& GetPlaneRotation() const
	{
		return PlaneRotation;
	}

	AVALANCHEMODIFIERS_API void SetInvertCut(bool bInInvertCut);
	bool GetInvertCut() const
	{
		return bInvertCut;
	}

	AVALANCHEMODIFIERS_API void SetFillHoles(bool bInFillHoles);
	bool GetFillHoles() const
	{
		return bFillHoles;
	}

protected:
	//~ Begin UObject
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent) override;
#endif
	//~ End UObject

	//~ Begin UActorModifierCoreBase
	virtual void OnModifierCDOSetup(FActorModifierCoreMetadata& InMetadata) override;
	virtual void OnModifierAdded(EActorModifierCoreEnableReason InReason) override;
	virtual void Apply() override;
	virtual void OnModifierDisabled(EActorModifierCoreDisableReason InReason) override;
	virtual void OnModifierRemoved(EActorModifierCoreDisableReason InReason) override;
	//~ End UActorModifierCoreBase

	/** Returns actual location of plane bounds restricted */
	FVector GetPlaneLocation() const;

	void OnPlaneRotationChanged();
	void OnFillHolesChanged();
	void OnInvertCutChanged();
	void OnPlaneOriginChanged();

#if WITH_EDITOR
	void OnUsePreviewChanged();
	void CreatePreviewComponent();
	void DestroyPreviewComponent();
	void UpdatePreviewComponent();
#endif

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetPlaneOrigin", Getter="GetPlaneOrigin", Category="PlaneCut", meta=(Delta="0.5", LinearDeltaSensitivity="1", AllowPrivateAccess="true"))
	float PlaneOrigin = 0.f;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetPlaneRotation", Getter="GetPlaneRotation", Category="PlaneCut", meta=(AllowPrivateAccess="true"))
	FRotator PlaneRotation = FRotator(0, 0, 90);

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetInvertCut", Getter="GetInvertCut", Category="PlaneCut", meta=(AllowPrivateAccess="true"))
	bool bInvertCut = false;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetFillHoles", Getter="GetFillHoles", Category="PlaneCut", meta=(AllowPrivateAccess="true"))
	bool bFillHoles = true;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="PlaneCut", meta=(AllowPrivateAccess="true"))
	bool bUsePreview = false;

private:
	UPROPERTY(Transient, DuplicateTransient, TextExportTransient)
	FAvaModifierPreviewPlane PreviewPlane;
#endif
};
