// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Toolkits/AssetEditorToolkit.h"

class IEnvironmentQueryEditor;

DECLARE_LOG_CATEGORY_EXTERN(LogEnvironmentQueryEditor, Log, All);

class FEnvironmentQueryEditorModule : public IModuleInterface, public IHasMenuExtensibility, public IHasToolBarExtensibility
{

public:
	// IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Creates an instance of EQS editor.  Only virtual so that it can be called across the DLL boundary. */
	virtual TSharedRef<IEnvironmentQueryEditor> CreateEnvironmentQueryEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, class UEnvQuery* Query );

	/** Gets the extensibility managers for outside entities to extend static mesh editor's menus and toolbars */
	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() override { return MenuExtensibilityManager; }
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() override { return ToolBarExtensibilityManager; }

	/** EQS Editor app identifier string */
	static const FName EnvironmentQueryEditorAppIdentifier;

	TSharedPtr<struct FGraphNodeClassHelper> GetClassCache() { return ClassCache; }

private:

	TSharedPtr<struct FGraphNodeClassHelper> ClassCache;

	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;
	TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager;

	/** Asset type actions */
	TSharedPtr<class FAssetTypeActions_EnvironmentQuery> ItemDataAssetTypeActions;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "AIGraphTypes.h"
#include "CoreMinimal.h"
#include "Toolkits/IToolkitHost.h"
#endif
