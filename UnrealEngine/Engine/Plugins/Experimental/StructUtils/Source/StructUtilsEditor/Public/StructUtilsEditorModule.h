// Copyright Epic Games, Inc. All Rights Reserved.

#pragma  once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "AssetTypeCategories.h"
#include "Toolkits/IToolkitHost.h"
#include "Toolkits/AssetEditorToolkit.h"


class IStructUtilsEditor;
class FAssetTypeActions_Base;
struct FGraphPanelNodeFactory;

/**
* The public interface to this module
*/
class STRUCTUTILSEDITOR_API FStructUtilsEditorModule : public IModuleInterface//, public IHasMenuExtensibility, public IHasToolBarExtensibility
{
public:
	// Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
