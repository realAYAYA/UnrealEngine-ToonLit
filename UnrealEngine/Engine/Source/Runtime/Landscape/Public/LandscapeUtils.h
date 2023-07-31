// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "RHIDefinitions.h"
#include "UObject/NameTypes.h"

class ULevel;

namespace UE::Landscape
{

/**
* Returns true if edit layers (GPU landscape tools) are enabled on this platform :
* Note: this is intended for the editor but is in runtime code since global shaders need to exist in runtime modules 
*/
LANDSCAPE_API bool DoesPlatformSupportEditLayers(EShaderPlatform InShaderPlatform);

#if WITH_EDITOR

/**
 * Returns a generated path used for Landscape Shared Assets
 * @param	InPath	Path used as a basis to generate shared assets path. If /Temp/, it will be replaced by the last valid path used for level.
 * @return Path used for Landscape Shared Assets
*/
LANDSCAPE_API FString GetSharedAssetsPath(const FString& InPath);

/**
 * Returns a generated path used for Landscape Shared Assets
 * @param	InLevel		Level's Path will be used as a basis to generate shared assets path. If /Temp/, it will be replaced by the last valid path used for level.
 * @return Path used for Landscape Shared Assets
*/
LANDSCAPE_API FString GetSharedAssetsPath(const ULevel* InLevel);

/**
 * Returns a generated package name for a Layer Info Object
 * @param	InLevel		Level's Path will be used as a basis to generate package's path. If /Temp/, it will be replaced by the last valid path used for level.
 * @param	InLayerName		The LayerName of the Layer Info Object
 * @param	OutLayerObjectName	The generated object name for Layer Info Object
 * @return
*/
LANDSCAPE_API FString GetLayerInfoObjectPackageName(const ULevel* InLevel, const FName& InLayerName, FName& OutLayerObjectName);

#endif //!WITH_EDITOR

} // end namespace UE::Landscape
