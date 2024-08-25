// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCO/ICustomizableObjectEditorModule.h"

#include "MuCOE/CustomizableObjectEditorLogger.h"
#include "Toolkits/AssetEditorToolkit.h"

class USkeletalMeshComponent;
class FPropertyEditorModule;
class FExtensibilityManager;
class ICustomizableObjectEditor;
class ICustomizableObjectInstanceEditor;
class ICustomizableObjectDebugger;


/** Get a list of packages that are used by the compilation but are not directly referenced.
  * List includes:
  * - Child UCustomizableObjects: Have inverted references.
  * - UDataTable: Data Tables used by Composite Data Tables are indirectly referenced by the UStruct and filtered by path. */
void GetReferencingPackages(const UCustomizableObject& Object, TArray<FName>& ObjectNames);


/**
 * StaticMesh editor module
 */
class FCustomizableObjectEditorModule : public ICustomizableObjectEditorModule
{
public:
	// IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	// ICustomizableObjectEditorModule interface
	virtual FCustomizableObjectEditorLogger& GetLogger() override;
	virtual bool IsCompilationOutOfDate(const UCustomizableObject& Object, TArray<FName>* OutOfDatePackages) const override;

	virtual TSharedPtr<FExtensibilityManager> GetCustomizableObjectEditorToolBarExtensibilityManager() override { return CustomizableObjectEditor_ToolBarExtensibilityManager; }
	virtual TSharedPtr<FExtensibilityManager> GetCustomizableObjectEditorMenuExtensibilityManager() override { return CustomizableObjectEditor_MenuExtensibilityManager; }

	bool HandleSettingsSaved();
	void RegisterSettings();

private:	
	TSharedPtr<FExtensibilityManager> CustomizableObjectEditor_ToolBarExtensibilityManager;
	TSharedPtr<FExtensibilityManager> CustomizableObjectEditor_MenuExtensibilityManager;

	/** List of registered custom details to remove later. */
	TArray<FName> RegisteredCustomDetails;

	/** Register Custom details. Also adds them to RegisteredCustomDetails list. */
	void RegisterCustomDetails(FPropertyEditorModule& PropertyModule, const UClass* Class, FOnGetDetailCustomizationInstance DetailLayoutDelegate);

	FCustomizableObjectEditorLogger Logger;

	// Command to look for Customizable Object Instance in the player pawn of the current world and open its Customizable Object Instance Editor
	IConsoleCommand* LaunchCOIECommand;

	FTSTicker::FDelegateHandle WarningsTickerHandle;
	
	static void OpenCOIE(const TArray<FString>& Arguments);

	/** Register the COI factory */
	void RegisterFactory();
};