// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ToolMenuOwner.h"
#include "ToolMenuDelegates.h"
#include "ToolMenuMisc.h"

#include "Misc/Attribute.h"
#include "Framework/MultiBox/MultiBoxDefs.h"
#include "Framework/Commands/UICommandInfo.h"
#include "UObject/TextProperty.h"
#include "ToolMenuEntry.generated.h"

class UToolMenuEntryScript;
struct FKeyEvent;

struct FToolMenuEntrySubMenuData
{
public:
	FToolMenuEntrySubMenuData() :
		bIsSubMenu(false),
		bOpenSubMenuOnClick(false),
		bAutoCollapse(false)
	{
	}

	bool bIsSubMenu;
	bool bOpenSubMenuOnClick;
	/** Entry placed into the parent's menu when there is only one entry */
	bool bAutoCollapse;
	FNewToolMenuChoice ConstructMenu;
};

struct FToolMenuEntryOptionsDropdownData
{
	FNewToolMenuChoice MenuContentGenerator;
	TAttribute<FText> ToolTip;
	FUIAction Action;
};

struct FToolMenuEntryToolBarData
{
public:
	FToolMenuEntryToolBarData() :
		bSimpleComboBox(false),
		bIsFocusable(false),
		bForceSmallIcons(false)
	{
	}

	/** Delegate that generates a widget for this combo button's menu content.  Called when the menu is summoned. */
	FNewToolMenuChoice ComboButtonContextMenuGenerator;

	/** Legacy delegate that generates a widget for this combo button's menu content.  Called when the menu is summoned. */
	FNewToolBarDelegateLegacy ConstructLegacy;

	TSharedPtr<FToolMenuEntryOptionsDropdownData> OptionsDropdownData;

	bool bSimpleComboBox;

	/** Whether ToolBar will have Focusable buttons */
	bool bIsFocusable;

	/** Whether this toolbar should always use small icons, regardless of the current settings */
	bool bForceSmallIcons;

};


struct FToolMenuEntryWidgetData
{
public:
	FToolMenuEntryWidgetData() :
		bNoIndent(false),
		bSearchable(false),
		bNoPadding(false)
	{
	}

	/** Remove the padding from the left of the widget that lines it up with other menu items */
	bool bNoIndent;

	/** If true, widget will be searchable */
	bool bSearchable;

	/** If true, no padding will be added */
	bool bNoPadding;
};

struct FToolMenuCustomWidgetContext
{
	// The style used by the menu creating the widget
	const class ISlateStyle* StyleSet = nullptr;

	// The name of the style used by the menu creating the widget
	FName StyleName;
};

/**
 * Represents entries in menus such as buttons, checkboxes, and sub-menus.
 *
 * Many entries are created for you via the methods of FToolMenuSection, such as FToolMenuSection::AddMenuEntry.
 */
USTRUCT(BlueprintType)
struct TOOLMENUS_API FToolMenuEntry
{
	GENERATED_BODY()

	FToolMenuEntry();
	FToolMenuEntry(const FToolMenuOwner InOwner, const FName InName, EMultiBlockType InType);

	FToolMenuEntry(const FToolMenuEntry&);
	FToolMenuEntry(FToolMenuEntry&&);

	FToolMenuEntry& operator=(const FToolMenuEntry&);
	FToolMenuEntry& operator=(FToolMenuEntry&&);

	static FToolMenuEntry InitMenuEntry(const FName InName, const TAttribute<FText>& InLabel, const TAttribute<FText>& InToolTip, const TAttribute<FSlateIcon>& InIcon, const FToolUIActionChoice& InAction, const EUserInterfaceActionType UserInterfaceActionType = EUserInterfaceActionType::Button, const FName InTutorialHighlightName = NAME_None);
	static FToolMenuEntry InitMenuEntry(const TSharedPtr< const FUICommandInfo >& InCommand, const TAttribute<FText>& InLabelOverride = TAttribute<FText>(), const TAttribute<FText>& InToolTipOverride = TAttribute<FText>(), const TAttribute<FSlateIcon>& InIconOverride = TAttribute<FSlateIcon>(), const FName InTutorialHighlightName = NAME_None, const TOptional<FName> InNameOverride = TOptional<FName>());
	static FToolMenuEntry InitMenuEntry(const FName InNameOverride, const TSharedPtr< const FUICommandInfo >& InCommand, const TAttribute<FText>& InLabelOverride = TAttribute<FText>(), const TAttribute<FText>& InToolTipOverride = TAttribute<FText>(), const TAttribute<FSlateIcon>& InIconOverride = TAttribute<FSlateIcon>(), const FName InTutorialHighlightName = NAME_None);
	static FToolMenuEntry InitMenuEntryWithCommandList(const TSharedPtr<const FUICommandInfo>& InCommand, const TSharedPtr<const FUICommandList>& InCommandList, const TAttribute<FText>& InLabelOverride = TAttribute<FText>(), const TAttribute<FText>& InToolTipOverride = TAttribute<FText>(), const TAttribute<FSlateIcon>& InIconOverride = TAttribute<FSlateIcon>(), const FName InTutorialHighlightName = NAME_None, const TOptional<FName> InNameOverride = TOptional<FName>());
	static FToolMenuEntry InitMenuEntry(const FName InName, const FToolUIActionChoice& InAction, const TSharedRef<SWidget>& Widget);

	static FToolMenuEntry InitSubMenu(const FName InName, const TAttribute<FText>& InLabel, const TAttribute<FText>& InToolTip, const FNewToolMenuChoice& InMakeMenu, const FToolUIActionChoice& InAction, const EUserInterfaceActionType InUserInterfaceActionType, bool bInOpenSubMenuOnClick = false, const TAttribute<FSlateIcon>& InIcon = TAttribute<FSlateIcon>(), const bool bShouldCloseWindowAfterMenuSelection = true);
	static FToolMenuEntry InitSubMenu(const FName InName, const TAttribute<FText>& InLabel, const TAttribute<FText>& InToolTip, const FNewToolMenuChoice& InMakeMenu, bool bInOpenSubMenuOnClick = false, const TAttribute<FSlateIcon>& InIcon = TAttribute<FSlateIcon>(), const bool bShouldCloseWindowAfterMenuSelection = true, const FName InTutorialHighlightName = NAME_None);
	static FToolMenuEntry InitSubMenu(const FName InName, const FToolUIActionChoice& InAction, const TSharedRef<SWidget>& InWidget, const FNewToolMenuChoice& InMakeMenu, bool bShouldCloseWindowAfterMenuSelection = true);

	static FToolMenuEntry InitToolBarButton(const FName InName, const FToolUIActionChoice& InAction, const TAttribute<FText>& InLabel = TAttribute<FText>(), const TAttribute<FText>& InToolTip = TAttribute<FText>(), const TAttribute<FSlateIcon>& InIcon = TAttribute<FSlateIcon>(), const EUserInterfaceActionType UserInterfaceActionType = EUserInterfaceActionType::Button, FName InTutorialHighlightName = NAME_None);
	static FToolMenuEntry InitToolBarButton(const TSharedPtr< const FUICommandInfo >& InCommand, const TAttribute<FText>& InLabelOverride = TAttribute<FText>(), const TAttribute<FText>& InToolTipOverride = TAttribute<FText>(), const TAttribute<FSlateIcon>& InIconOverride = TAttribute<FSlateIcon>(), FName InTutorialHighlightName = NAME_None, const TOptional<FName> InNameOverride = TOptional<FName>());
	static FToolMenuEntry InitComboButton(const FName InName, const FToolUIActionChoice& InAction, const FNewToolMenuChoice& InMenuContentGenerator, const TAttribute<FText>& InLabelOverride = TAttribute<FText>(), const TAttribute<FText>& InToolTipOverride = TAttribute<FText>(), const TAttribute<FSlateIcon>& InIconOverride = TAttribute<FSlateIcon>(), bool bInSimpleComboBox = false, FName InTutorialHighlightName = NAME_None);

	static FToolMenuEntry InitSeparator(const FName InName);

	static FToolMenuEntry InitWidget(const FName InName, const TSharedRef<SWidget>& InWidget, const FText& Label, bool bNoIndent = false, bool bSearchable = true, bool bNoPadding = false, const FText& InToolTipText = FText());

	bool IsSubMenu() const { return SubMenuData.bIsSubMenu; }

	bool IsConstructLegacy() const { return ConstructLegacy.IsBound(); }

	const FUIAction* GetActionForCommand(const FToolMenuContext& InContext, TSharedPtr<const FUICommandList>& OutCommandList) const;

	void SetCommandList(const TSharedPtr<const FUICommandList>& InCommandList);

	UE_DEPRECATED(5.1, "AddOptionsDropdown is deprecated. Use InitComboButton with bSimpleComboBox=true")
	void AddOptionsDropdown(FUIAction InAction, const FOnGetContent InMenuContentGenerator, const TAttribute<FText>& InToolTip = TAttribute<FText>());

	void AddKeybindFromCommand(const TSharedPtr< const FUICommandInfo >& InCommand);

	bool IsCommandKeybindOnly() const;
	bool CommandAcceptsInput(const FKeyEvent& InKeyEvent) const;
	bool TryExecuteToolUIAction(const FToolMenuContext& InContext);
	friend struct FToolMenuSection;
	friend class UToolMenuEntryScript;

private:

	void SetCommand(const TSharedPtr< const FUICommandInfo >& InCommand, TOptional<FName> InName, const TAttribute<FText>& InLabel, const TAttribute<FText>& InToolTip, const TAttribute<FSlateIcon>& InIcon);

	void ResetActions();

	bool IsScriptObjectDynamicConstruct() const;

	bool IsNonLegacyDynamicConstruct() const;

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool Menus")
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool Menus")
	FToolMenuOwner Owner;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool Menus")
	EMultiBlockType Type;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool Menus")
	EUserInterfaceActionType UserInterfaceActionType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool Menus")
	FName TutorialHighlightName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool Menus")
	FToolMenuInsert InsertPosition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool Menus")
	bool bShouldCloseWindowAfterMenuSelection;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool Menus")
	TObjectPtr<UToolMenuEntryScript> ScriptObject;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool Menus")
	FName StyleNameOverride;

	FToolMenuEntrySubMenuData SubMenuData;

	FToolMenuEntryToolBarData ToolBarData;

	FToolMenuEntryWidgetData WidgetData;

	UE_DEPRECATED(5.0, "Use MakeCustomWidget instead")
	FNewToolMenuWidget MakeWidget;

	/** Optional delegate that returns a widget to use as this menu entry */
	FNewToolMenuCustomWidget MakeCustomWidget;

	TAttribute<FText> Label;
	TAttribute<FText> ToolTip;
	TAttribute<FSlateIcon> Icon;
	TAttribute<FText> InputBindingLabel;

private:

	friend class UToolMenus;
	friend class UToolMenuEntryExtensions;
	friend class FPopulateMenuBuilderWithToolMenuEntry;

	FToolUIActionChoice Action;

	FToolMenuStringCommand StringExecuteAction;

	TSharedPtr< const FUICommandInfo > Command;
	TSharedPtr< const FUICommandList > CommandList;

	FNewToolMenuSectionDelegate Construct;
	FNewToolMenuDelegateLegacy ConstructLegacy;

	bool bAddedDuringRegister;

	UPROPERTY()
	bool bCommandIsKeybindOnly;
};
