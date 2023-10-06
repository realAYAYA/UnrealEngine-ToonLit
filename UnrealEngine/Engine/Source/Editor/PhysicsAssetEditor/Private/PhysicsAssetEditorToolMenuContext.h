// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PhysicsAssetEditorToolMenuContext.generated.h"

class FPhysicsAssetEditor;

UCLASS()
class UPhysicsAssetEditorToolMenuContext : public UObject
{
	GENERATED_BODY()

public:
	TWeakPtr<FPhysicsAssetEditor> PhysicsAssetEditor;
};
