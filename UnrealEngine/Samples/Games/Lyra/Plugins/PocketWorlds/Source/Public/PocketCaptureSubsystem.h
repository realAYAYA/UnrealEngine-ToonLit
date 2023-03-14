// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Ticker.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/SubclassOf.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include "PocketCaptureSubsystem.generated.h"

class FSubsystemCollectionBase;
class UObject;
class UPocketCapture;
class UPrimitiveComponent;
struct FFrame;

UCLASS(BlueprintType)
class POCKETWORLDS_API UPocketCaptureSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UPocketCaptureSubsystem();

	// Begin USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	// End USubsystem

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "PocketCaptureClass"))
	UPocketCapture* CreateThumbnailRenderer(TSubclassOf<UPocketCapture> PocketCaptureClass);

	UFUNCTION(BlueprintCallable)
	void DestroyThumbnailRenderer(UPocketCapture* ThumbnailRenderer);

	void StreamThisFrame(TArray<UPrimitiveComponent*>& PrimitiveComponents);

protected:
	bool Tick(float DeltaTime);

	TArray<TWeakObjectPtr<UPrimitiveComponent>> StreamNextFrame;
	TArray<TWeakObjectPtr<UPrimitiveComponent>> StreamedLastFrameButNotNext;

private:
	TArray<TWeakObjectPtr<UPocketCapture>> ThumbnailRenderers;

	FTSTicker::FDelegateHandle TickHandle;
};