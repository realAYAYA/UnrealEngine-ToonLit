// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MuCO/CustomizableObjectInstance.h"


/**
 * Schedules the async update of the target instance. It will clear previous update data.
 * This method will also take care of setting and later resetting the state of the Customizable Object System.
 * @param InInstance The instance we want to update so we can later bake it's resources.
 * @param InInstanceUpdateDelegate Delegate to be called after the instance gets updated.
 */
CUSTOMIZABLEOBJECTEDITOR_API
void UpdateInstanceForBaking(UCustomizableObjectInstance& InInstance, FInstanceUpdateNativeDelegate& InInstanceUpdateDelegate);


/**
 * Serializes onto disk the resources used by the targeted Customizable Object Instance. The operation can be configured in the FInstanceBakingSettings settings object.
 * @param InInstance The mutable COI instance whose resources we want to bake onto disk
 * @param FileName The base name for the all the files generated during the baking process
 * @param AssetPath The path where to save the assets
 * @param bExportAllResources Determines if we want a full or partial export
 * @param bGenerateConstantMaterialInstances Determines if we want to generate constant material instances or not
 */
CUSTOMIZABLEOBJECTEDITOR_API
void BakeCustomizableObjectInstance (UCustomizableObjectInstance& InInstance, const FString& FileName, const FString& AssetPath,
	const bool bExportAllResources, const bool bGenerateConstantMaterialInstances);