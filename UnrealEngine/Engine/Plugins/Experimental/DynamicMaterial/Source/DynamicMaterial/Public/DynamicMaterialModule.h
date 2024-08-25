// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"
#include "Delegates/DelegateCombinations.h"
#include "Modules/ModuleInterface.h"

#if WITH_EDITOR
#include "UObject/ScriptInterface.h"
#endif

#if WITH_EDITOR
class IDynamicMaterialModelEditorOnlyDataInterface;
class UDynamicMaterialModel;
#endif

DECLARE_LOG_CATEGORY_EXTERN(LogDynamicMaterial, Log, All);

#if WITH_EDITOR
DECLARE_DELEGATE_RetVal_OneParam(TScriptInterface<IDynamicMaterialModelEditorOnlyDataInterface>, FDMCreateEditorOnlyDataDelegate, UDynamicMaterialModel*)
#endif

/**
 * Material Designer - Build your own materials in a slimline editor!
 */
class DYNAMICMATERIAL_API FDynamicMaterialModule : public IModuleInterface
{
public:
	static FDynamicMaterialModule& Get();
	static bool AreUObjectsSafe();
	static bool IsMaterialExportEnabled();

#if WITH_EDITOR
	static TScriptInterface<IDynamicMaterialModelEditorOnlyDataInterface> CreateEditorOnlyData(UDynamicMaterialModel* InMaterialModel);
	static FDMCreateEditorOnlyDataDelegate& GetCreateEditorOnlyDataDelegate() { return CreateEditorOnlyDataDelegate; }
#endif

	//~ Begin IDynamicMaterialModule
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IDynamicMaterialModule

protected:
	static bool bIsEngineExiting;

#if WITH_EDITOR
	static FDMCreateEditorOnlyDataDelegate CreateEditorOnlyDataDelegate;
#endif
	
	static void HandleEnginePreExit();

	FDelegateHandle EnginePreExitHandle;
};
