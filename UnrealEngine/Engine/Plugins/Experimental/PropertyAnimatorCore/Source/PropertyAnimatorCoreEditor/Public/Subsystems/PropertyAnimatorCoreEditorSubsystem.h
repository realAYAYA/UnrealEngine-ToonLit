// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorSubsystem.h"
#include "Menus/PropertyAnimatorCoreEditorMenuDefs.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"
#include "PropertyAnimatorCoreEditorSubsystem.generated.h"

class IDetailTreeNode;
class IPropertyHandle;
class SPropertyAnimatorCoreEditorEditPanel;
class SWidget;
class UPropertyAnimatorCoreBase;
class UPropertyAnimatorCoreEditorMenuContext;
class UToolMenu;
struct FPropertyAnimatorCoreData;
struct FPropertyAnimatorCoreEditorEditPanelOptions;
struct FOnGenerateGlobalRowExtensionArgs;
struct FPropertyRowExtensionButton;

/** Singleton class that handles editor operations for property control such as windows */
UCLASS()
class UPropertyAnimatorCoreEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	/** Get this subsystem instance */
	PROPERTYANIMATORCOREEDITOR_API static UPropertyAnimatorCoreEditorSubsystem* Get();

	//~ Begin UEditorSubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End UEditorSubsystem

	/** Gets or creates the property control window, single instance only allowed */
	FPropertyAnimatorCoreEditorEditPanelOptions& OpenPropertyControlWindow();

	/** Closes the property control window */
	void ClosePropertyControlWindow() const;

	/** Checks whether the single window instance is opened */
	bool IsPropertyControlWindowOpened() const;

	/** Fills a menu based on context objects and menu options */
	PROPERTYANIMATORCOREEDITOR_API bool FillAnimatorMenu(UToolMenu* InMenu, const FPropertyAnimatorCoreEditorMenuContext& InContext, const FPropertyAnimatorCoreEditorMenuOptions& InOptions);

protected:
	/** Setup details panel button customization */
	void RegisterDetailPanelCustomization();

	/** Removes details panel button customization */
	void UnregisterDetailPanelCustomization();

	/** Extends the row extension from the details panel to display a button for each property row */
	void OnGetGlobalRowExtension(const FOnGenerateGlobalRowExtensionArgs& InArgs, TArray<FPropertyRowExtensionButton>& OutExtensions);

	/** Called when user press the control property button in details panel */
	void OnControlPropertyClicked(TWeakPtr<IDetailTreeNode> InOwnerTreeNode, TSharedPtr<IPropertyHandle> InPropertyHandle);

	/** Checks whether any controller supports that property */
	bool IsControlPropertySupported(TWeakPtr<IDetailTreeNode> InOwnerTreeNode, TSharedPtr<IPropertyHandle> InPropertyHandle) const;

	/** Checks whether the property control window is opened to display the icon */
	bool IsControlPropertyVisible(TWeakPtr<IDetailTreeNode> InOwnerTreeNode, TSharedPtr<IPropertyHandle> InPropertyHandle) const;

	/** Checks whether any controller is linked to that property */
	bool IsControlPropertyLinked(TWeakPtr<IDetailTreeNode> InOwnerTreeNode, TSharedPtr<IPropertyHandle> InPropertyHandle) const;

	/** Creates the context menu to display when selecting the animator icon */
	TSharedRef<SWidget> GenerateContextMenuWidget(const FPropertyAnimatorCoreData& InProperty);

	/** Extend the context menu on each property row in details panel with additional entries */
	void ExtendPropertyRowContextMenu();

	/** Fills the animator details view extension menu */
	void FillAnimatorExtensionSection(UToolMenu* InToolMenu);

	/** Fills the animator details view row context menu */
	void FillAnimatorRowContextSection(UToolMenu* InToolMenu);

	/** Convert a property handle to a property data */
	TOptional<FPropertyAnimatorCoreData> GetPropertyData(TSharedPtr<IPropertyHandle> InPropertyHandle, bool bInFindMemberProperty = false) const;

	FDelegateHandle OnGetGlobalRowExtensionHandle;

	TWeakPtr<SPropertyAnimatorCoreEditorEditPanel> PropertyControllerPanelWeak;

	TSharedPtr<FPropertyAnimatorCoreEditorMenuData> LastMenuData;
};