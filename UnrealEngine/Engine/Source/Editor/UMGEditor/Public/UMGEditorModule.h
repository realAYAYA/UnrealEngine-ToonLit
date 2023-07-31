// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "IHasDesignerExtensibility.h"
#include "IHasPropertyBindingExtensibility.h"

extern const FName UMGEditorAppIdentifier;

class FUMGEditor;
class FWidgetBlueprintApplicationMode;
class FWorkflowAllowedTabSet;
class IBlueprintWidgetCustomizationExtender;

/** The public interface of the UMG editor module. */
class IUMGEditorModule : 
	public IModuleInterface, 
	public IHasMenuExtensibility, 
	public IHasToolBarExtensibility, 
	public IHasDesignerExtensibility,
	public IHasPropertyBindingExtensibility
{
public:
	virtual class FWidgetBlueprintCompiler* GetRegisteredCompiler() = 0;

	DECLARE_EVENT_TwoParams(IUMGEditorModule, FOnRegisterTabs, const FWidgetBlueprintApplicationMode&, FWorkflowAllowedTabSet&);
	virtual FOnRegisterTabs& OnRegisterTabsForEditor() = 0;

	DECLARE_DELEGATE_RetVal_TwoParams(TSharedRef<FExtender>, FWidgetEditorToolbarExtender, const TSharedRef<FUICommandList>, TSharedRef<FWidgetBlueprintEditor>);

	/** Add Toolbar extender for widget editor, used by widget tool palette. */
	virtual void AddWidgetEditorToolbarExtender(FWidgetEditorToolbarExtender&& InToolbarExtender) = 0;

	/** Get current toolbar extenders for widget editor */
	virtual TArrayView<FWidgetEditorToolbarExtender> GetAllWidgetEditorToolbarExtenders() = 0;

	/** Add customization for widget */
	virtual void AddWidgetCustomizationExtender(const TSharedRef<IBlueprintWidgetCustomizationExtender>& WidgetCustomizationExtender) = 0;

	/** Remove customization for widget */
	virtual void RemoveWidgetCustomizationExtender(const TSharedRef<IBlueprintWidgetCustomizationExtender>& WidgetCustomizationExtender) = 0;

	/** Get current customization extenders for widget */
	virtual TArrayView<TSharedRef<IBlueprintWidgetCustomizationExtender>> GetAllWidgetCustomizationExtenders() = 0;

	/** Support for general layout extenders */
	DECLARE_EVENT_OneParam(IStaticMeshEditor, FOnRegisterLayoutExtensions, FLayoutExtender&);
	virtual FOnRegisterLayoutExtensions& OnRegisterLayoutExtensions() = 0;
};
