// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FViewport;
class UTextureShareObject;

/**
* TextureShare object logic for the game thread
*/
struct FTextureShareWorldSubsystemObject
{
public:
	FTextureShareWorldSubsystemObject(const FString& InProcessName, UTextureShareObject* InTextureShareObject);
	
	// Update on tick for specified viewport
	bool Tick(FViewport* InViewport);

private:
	bool UpdateFrameMarker();
	bool UpdateResourceRequests();

	bool SendCustomData();
	bool ReceiveCustomData();

private:
	TSharedPtr<class ITextureShareObject, ESPMode::ThreadSafe> Object;
	UTextureShareObject* TextureShareObject;
};

