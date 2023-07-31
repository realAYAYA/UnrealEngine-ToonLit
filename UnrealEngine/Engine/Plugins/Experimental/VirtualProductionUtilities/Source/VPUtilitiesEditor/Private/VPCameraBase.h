// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CineCameraActor.h"
#include "VPCameraBase.generated.h"

class ACameraActor;
class UCameraComponent;


UCLASS()
class AVPCameraBase : public ACineCameraActor
{
	GENERATED_BODY()

public:
	AVPCameraBase(const FObjectInitializer& ObjectInitializer);

public:
	/** Remove the preview and clear the list of selected user. Another user may have the camera selected and will re-add it later. */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Editor Scripting | Camera")
	void ResetPreview();

public:
	virtual bool ShouldTickIfViewportsOnly() const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginDestroy() override;
	PRAGMA_DISABLE_DEPRECATION_WARNINGS // Suppress compiler warning on override of deprecated function
	UE_DEPRECATED(5.0, "Use version that takes FObjectPreSaveContext instead.")
	virtual void PreSave(const class ITargetPlatform* TargetPlatform) override;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;

private:
	void AddPreview();
	void RemovePreview();
	void AddUser();
	void RemoveUser(bool bForceRemovePreview);

	/** Contains the name of users in an MU session that have selected this */
	UPROPERTY()
	TArray<FGuid> SelectedByUsers;

	FGuid CurrentInstanceId;
};