// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"
#include "Tickable.h"

#include "AppleVisionTypes.h"

#include "AppleVisionBlueprintProxy.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAppleVisionDetectFacesDelegate, const FFaceDetectionResult&, FaceDetectionResult);

class FAppleVisionDetectFacesAsyncTaskBase;

UCLASS(MinimalAPI)
class UAppleVisionDetectFacesAsyncTaskBlueprintProxy :
	public UObject,
	public FTickableGameObject
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FAppleVisionDetectFacesDelegate OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FAppleVisionDetectFacesDelegate OnFailure;

	/**
	 * Detects faces within an image
	 *
	 * @param SourceImage the image to detect faces in
	 */
	UFUNCTION(BlueprintCallable, Meta=(BlueprintInternalUseOnly="true", DisplayName="Detect Faces"), Category="Apple Vision")
	static UAppleVisionDetectFacesAsyncTaskBlueprintProxy* CreateProxyObjectForDetectFaces(UTexture* SourceImage);

	//~ Begin FTickableObject Interface
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return bShouldTick; }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UAppleVisionDetectFacesAsyncTaskBlueprintProxy, STATGROUP_Tickables); }
	//~ End FTickableObject Interface

	/** The async task to check during Tick() */
	TSharedPtr<FAppleVisionDetectFacesAsyncTaskBase, ESPMode::ThreadSafe> AsyncTask;

	UPROPERTY(BlueprintReadOnly, Category="Apple Vision")
	FFaceDetectionResult FaceDetectionResult;

private:
	/** True until the async task completes, then false */
	bool bShouldTick;
};
