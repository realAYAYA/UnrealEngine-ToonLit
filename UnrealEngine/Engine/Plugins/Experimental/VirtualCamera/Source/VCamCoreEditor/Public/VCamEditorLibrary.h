// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "VCamEditorLibrary.generated.h"

class UVcamComponent;

UCLASS()
class UVCamEditorLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:

	/**
	 * Find all loaded VCam Components own by an actor in the world editor. Exclude actor that are pending kill, in PIE, PreviewEditor, ...
	 * @param VCamComponents Output List of found VCamComponents
	 */
    UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
    static void GetAllVCamComponentsInLevel(TArray<UVCamComponent*>& VCamComponents);
};