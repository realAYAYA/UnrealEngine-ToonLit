// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"
#include "UObject/ObjectPtr.h"

/**
 * IGeometryProcessing_UVEditorAssetEditor is a generic high-level interface the UVEditor Asset Editor plugin.
 * 
 * This is an IModularFeature, and so clients can query/enumerate the available UVEditor implementations 
 * based on the ::GetModularFeatureName(). However, the preferred way is to use code like the following:
 * 
 *     IGeometryProcessingInterfacesModule& GeomProcInterfaces = FModuleManager::Get().LoadModuleChecked<IGeometryProcessingInterfacesModule>("GeometryProcessingInterfaces");
 *     IGeometryProcessing_UVEditorAssetEditor* UVEditorAPI = GeomProcInterfaces.GetUVEditorAssetEditorImplementation();
 * 
 * This will automatically determine which available implementation to use (based on any applicable config settings/etc),
 * and cache the result of that decision.
 */
class IGeometryProcessing_UVEditorAssetEditor : public IModularFeature
{
public:
	virtual ~IGeometryProcessing_UVEditorAssetEditor() {}

	/**
	 * Top-level driver function that clients call to launch the UVEditor Asset Editor
	 */
	virtual void LaunchUVEditor(const TArray<TObjectPtr<UObject>>& Objects)
	{
		check(false);		// not implemented in base class
	}

	/**
	* Top-level function to check if the UVEditor can be launched at this point
	*/
	virtual bool CanLaunchUVEditor(const TArray<TObjectPtr<UObject>>& Objects)
	{
		check(false);		// not implemented in base class
		return false;
	}


	// Modular feature name to register for retrieval during runtime
	static const FName GetModularFeatureName()
	{
		return TEXT("GeometryProcessing_UVEditorAssetEditor");
	}
};