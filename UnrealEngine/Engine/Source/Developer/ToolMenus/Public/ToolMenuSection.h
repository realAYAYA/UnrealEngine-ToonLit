// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ToolMenuDelegates.h"
#include "ToolMenuEntry.h"
#include "Misc/Attribute.h"

#include "ToolMenuSection.generated.h"

UCLASS(Blueprintable)
class TOOLMENUS_API UToolMenuSectionDynamic : public UObject
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintImplementableEvent, Category = "Tool Menus")
	void ConstructSections(UToolMenu* Menu, const FToolMenuContext& Context);
};

USTRUCT(BlueprintType)
struct TOOLMENUS_API FToolMenuSection
{
	GENERATED_BODY()

public:

	FToolMenuSection();

	void InitSection(const FName InName, const TAttribute< FText >& InLabel, const FToolMenuInsert InPosition);

	FToolMenuEntry& AddEntryObject(UToolMenuEntryScript* InObject);
	FToolMenuEntry& AddEntry(const FToolMenuEntry& Args);
	FToolMenuEntry& AddMenuEntry(const FName InName, const TAttribute<FText>& InLabel, const TAttribute<FText>& InToolTip, const TAttribute<FSlateIcon>& InIcon, const FToolUIActionChoice& InAction, const EUserInterfaceActionType UserInterfaceActionType = EUserInterfaceActionType::Button, const FName InTutorialHighlightName = NAME_None);
	FToolMenuEntry& AddMenuEntry(const TSharedPtr< const FUICommandInfo >& InCommand, const TAttribute<FText>& InLabelOverride = TAttribute<FText>(), const TAttribute<FText>& InToolTipOverride = TAttribute<FText>(), const TAttribute<FSlateIcon>& InIconOverride = TAttribute<FSlateIcon>(), const FName InTutorialHighlightName = NAME_None, const TOptional<FName> InNameOverride = TOptional<FName>());
	FToolMenuEntry& AddMenuEntry(const FName InNameOverride, const TSharedPtr< const FUICommandInfo >& InCommand, const TAttribute<FText>& InLabelOverride = TAttribute<FText>(), const TAttribute<FText>& InToolTipOverride = TAttribute<FText>(), const TAttribute<FSlateIcon>& InIconOverride = TAttribute<FSlateIcon>(), const FName InTutorialHighlightName = NAME_None);
	FToolMenuEntry& AddMenuEntryWithCommandList(const TSharedPtr<const FUICommandInfo>& InCommand, const TSharedPtr<const FUICommandList>& InCommandList, const TAttribute<FText>& InLabelOverride = TAttribute<FText>(), const TAttribute<FText>& InToolTipOverride = TAttribute<FText>(), const TAttribute<FSlateIcon>& InIconOverride = TAttribute<FSlateIcon>(), const FName InTutorialHighlightName = NAME_None, const TOptional<FName> InNameOverride = TOptional<FName>());

	FToolMenuEntry& AddDynamicEntry(const FName InName, const FNewToolMenuSectionDelegate& InConstruct);
	FToolMenuEntry& AddDynamicEntry(const FName InName, const FNewToolMenuDelegateLegacy& InConstruct);

	UE_DEPRECATED(4.26, "AddMenuSeparator has been deprecated.  Use AddSeparator instead.")
	FToolMenuEntry& AddMenuSeparator(const FName InName);
	FToolMenuEntry& AddSeparator(const FName InName);

	FToolMenuEntry& AddSubMenu(const FName InName, const TAttribute<FText>& InLabel, const TAttribute<FText>& InToolTip, const FNewToolMenuChoice& InMakeMenu, const FToolUIActionChoice& InAction, const EUserInterfaceActionType InUserInterfaceActionType, bool bInOpenSubMenuOnClick = false, const TAttribute<FSlateIcon>& InIcon = TAttribute<FSlateIcon>(), const bool bShouldCloseWindowAfterMenuSelection = true);
	FToolMenuEntry& AddSubMenu(const FName InName, const TAttribute<FText>& InLabel, const TAttribute<FText>& InToolTip, const FNewToolMenuChoice& InMakeMenu, bool bInOpenSubMenuOnClick = false, const TAttribute<FSlateIcon>& InIcon = TAttribute<FSlateIcon>(), const bool bShouldCloseWindowAfterMenuSelection = true);
	FToolMenuEntry& AddSubMenu(const FName InName, const FToolUIActionChoice& InAction, const TSharedRef<SWidget>& InWidget, const FNewToolMenuChoice& InMakeMenu, bool bShouldCloseWindowAfterMenuSelection = true);

	template <typename TContextType>
	TContextType* FindContext() const
	{
		return Context.FindContext<TContextType>();
	}

	FToolMenuEntry* FindEntry(const FName InName);
	const FToolMenuEntry* FindEntry(const FName InName) const;

private:

	void InitGeneratedSectionCopy(const FToolMenuSection& Source, FToolMenuContext& InContext);

	int32 RemoveEntry(const FName InName);
	int32 RemoveEntriesByOwner(const FToolMenuOwner InOwner);

	int32 IndexOfBlock(const FName InName) const;
	int32 FindBlockInsertIndex(const FToolMenuEntry& InBlock) const;

	bool IsNonLegacyDynamic() const;

	bool IsRegistering() const;

	friend class UToolMenuSectionExtensions;
	friend class UToolMenus;
	friend class UToolMenu;

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool Menus")
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool Menus")
	FToolMenuOwner Owner;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool Menus")
	TArray<FToolMenuEntry> Blocks;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool Menus")
	FToolMenuInsert InsertPosition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool Menus")
	FToolMenuContext Context;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool Menus")
	TObjectPtr<UToolMenuSectionDynamic> ToolMenuSectionDynamic;

	TAttribute<FText> Label;

	FNewSectionConstructChoice Construct;

private:

	bool bIsRegistering;
	bool bAddedDuringRegister;
};
