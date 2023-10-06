// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FAssetTypeActions_InterchangeBlueprintPipelineBase;
class FAssetTypeActions_InterchangePipelineBase;
class FAssetTypeActions_InterchangePythonPipelineBase;

/**
 * The public interface to this module
 */
class IInterchangeEditorPipelinesModule : public IModuleInterface
{

public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IInterchangeEditorPipelinesModule& Get()
	{
		return FModuleManager::LoadModuleChecked< IInterchangeEditorPipelinesModule >( "InterchangeEditorPipelines" );
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "InterchangeEditorPipelines" );
	}

	TArray<FName> ClassesToUnregisterOnShutdown;

	TSharedPtr<FAssetTypeActions_InterchangeBlueprintPipelineBase> BlueprintPipelineBase_TypeActions;
	TSharedPtr<FAssetTypeActions_InterchangePipelineBase> PipelineBase_TypeActions;
	TSharedPtr<FAssetTypeActions_InterchangePythonPipelineBase> PythonPipelineBase_TypeActions;
};


#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "Styling/ISlateStyle.h"
#endif
