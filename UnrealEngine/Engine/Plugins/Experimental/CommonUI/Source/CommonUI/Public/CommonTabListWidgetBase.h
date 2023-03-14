// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonUserWidget.h"
#include "CommonUITypes.h"

#include "CommonTabListWidgetBase.generated.h"

class UCommonButtonBase;
class UCommonButtonGroupBase;

/** Information about a registered tab in the tab list */
USTRUCT()
struct FCommonRegisteredTabInfo
{
	GENERATED_BODY()

public:
	/** The index of the tab in the list */
	UPROPERTY()
	int32 TabIndex;
	
	/** The actual button widget that represents this tab on-screen */
	UPROPERTY()
	TObjectPtr<UCommonButtonBase> TabButton;

	/** The actual instance of the content widget to display when this tab is selected. Can be null if a load is required. */
	UPROPERTY()
	TObjectPtr<UWidget> ContentInstance;

	FCommonRegisteredTabInfo()
		: TabIndex(INDEX_NONE)
		, TabButton(nullptr)
		, ContentInstance(nullptr)
	{}
};

/** Base class for a list of selectable tabs that correspondingly activate and display an arbitrary widget in a linked switcher */
UCLASS(Abstract, Blueprintable, ClassGroup = UI, meta = (Category = "Common UI", DisableNativeTick))
class COMMONUI_API UCommonTabListWidgetBase : public UCommonUserWidget
{
	GENERATED_UCLASS_BODY()

public:
	/** Delegate broadcast when a new tab is selected. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTabSelected, FName, TabId);

	/** Broadcasts when a new tab is selected. */
	UPROPERTY(BlueprintAssignable, Category = TabList)
	FOnTabSelected OnTabSelected;

	/** Delegate broadcast when a new tab is created. Allows hook ups after creation. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTabButtonCreation, FName, TabId, UCommonButtonBase*, TabButton);

	/** Broadcasts when a new tab is created. */
	UPROPERTY(BlueprintAssignable, Category = TabList)
	FOnTabButtonCreation OnTabButtonCreation;

	/** Delegate broadcast when a tab is being removed. Allows clean ups after destruction. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTabButtonRemoval, FName, TabId, UCommonButtonBase*, TabButton);

	/** Broadcasts when a new tab is created. */
	UPROPERTY(BlueprintAssignable, Category = TabList)
	FOnTabButtonRemoval OnTabButtonRemoval;
	
	/** Delegate broadcast when the tab list has been rebuilt (after a new tab has been inserted rather than added to the end). */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTabListRebuilt);
	
	/** Broadcasts when the tab list has been rebuilt (after a new tab has been inserted rather than added to the end). */
	UPROPERTY(BlueprintAssignable, Category = TabList)
	FOnTabListRebuilt OnTabListRebuilt;

	/** @return The currently active (selected) tab */
	UFUNCTION(BlueprintCallable, Category = TabList)
	FName GetActiveTab() const { return ActiveTabID; }

	/**
	 * Establishes the activatable widget switcher instance that this tab list should interact with
	 * @param CommonSwitcher The switcher that this tab list should be associated with and manipulate
	 */
	UFUNCTION(BlueprintCallable, Category = TabList)
	virtual void SetLinkedSwitcher(UCommonAnimatedSwitcher* CommonSwitcher);

	/** @return The switcher that this tab list is associated with and manipulates */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = TabList)
	UCommonAnimatedSwitcher* GetLinkedSwitcher() const;

	/**
	 * Registers and adds a new tab to the list that corresponds to a given widget instance. If not present in the linked switcher, it will be added.
	 * @param TabID The name ID used to keep track of this tab. Attempts to register a tab under a duplicate ID will fail.
	 * @param ButtonWidgetType The widget type to create for this tab
	 * @param ContentWidget The widget to associate with the registered tab
	 * @param TabIndex Determines where in the tab list to insert the new tab (-1 means tab will be added to end of the list)
	 * @return True if the new tab registered successfully and there were no name ID conflicts
	 */
	UFUNCTION(BlueprintCallable, Category = TabList)
	bool RegisterTab(FName TabNameID, TSubclassOf<UCommonButtonBase> ButtonWidgetType, UWidget* ContentWidget, const int32 TabIndex = -1 /*INDEX_NONE*/);

	UFUNCTION(BlueprintCallable, Category = TabList)
	bool RemoveTab(FName TabNameID);

	UFUNCTION(BlueprintCallable, Category = TabList)
	void RemoveAllTabs();

	UFUNCTION(BlueprintCallable, Category = TabList)
	int32 GetTabCount() const;

	/** 
	 * Selects the tab registered under the provided name ID
	 * @param TabNameID The name ID for the tab given when registered
	 */
	UFUNCTION(BlueprintCallable, Category = TabList)
	bool SelectTabByID(FName TabNameID, bool bSuppressClickFeedback = false );

	UFUNCTION(BlueprintCallable, Category = TabList)
	FName GetSelectedTabId() const;

	UFUNCTION(BlueprintCallable, Category = TabList)
	FName GetTabIdAtIndex(int32 Index) const;

	/** Sets the visibility of the tab associated with the given ID  */
	UFUNCTION(BlueprintCallable, Category = TabList)
	void SetTabVisibility(FName TabNameID, ESlateVisibility NewVisibility);

	/** Sets whether the tab associated with the given ID is enabled/disabled */
	UFUNCTION(BlueprintCallable, Category = TabList)
	void SetTabEnabled(FName TabNameID, bool bEnable);

	/** Sets whether the tab associated with the given ID is interactable */
	UFUNCTION(BlueprintCallable, Category = TabList)
	void SetTabInteractionEnabled(FName TabNameID, bool bEnable);

	/** Disables the tab associated with the given ID with a reason */
	UFUNCTION(BlueprintCallable, Category = TabList)
	void DisableTabWithReason(FName TabNameID, const FText& Reason);

	UFUNCTION(BlueprintCallable, Category = TabList)
	virtual void SetListeningForInput(bool bShouldListen);

	/** Returns the tab button matching the ID, if found */
	UFUNCTION(BlueprintCallable, Category = TabList)
	UCommonButtonBase* GetTabButtonBaseByID(FName TabNameID);

protected:
	// UUserWidget interface
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	// End UUserWidget

	virtual void UpdateBindings();

	bool IsRebuildingList() const;

	UFUNCTION(BlueprintImplementableEvent, Category = TabList, meta = (BlueprintProtected = "true"))
	void HandlePreLinkedSwitcherChanged_BP();

	virtual void HandlePreLinkedSwitcherChanged();

	UFUNCTION(BlueprintImplementableEvent, Category = TabList, meta = (BlueprintProtected = "true"))
	void HandlePostLinkedSwitcherChanged_BP();

	virtual void HandlePostLinkedSwitcherChanged();

	UFUNCTION(BlueprintNativeEvent, Category = TabList, meta = (BlueprintProtected = "true"))
	void HandleTabCreation(FName TabNameID, UCommonButtonBase* TabButton);

	UFUNCTION(BlueprintNativeEvent, Category = TabList, meta = (BlueprintProtected = "true"))
	void HandleTabRemoval(FName TabNameID, UCommonButtonBase* TabButton);

	/** The input action to listen for causing the next tab to be selected */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = TabList, meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle NextTabInputActionData;

	/** The input action to listen for causing the previous tab to be selected */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = TabList, meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle PreviousTabInputActionData;

	/** Whether to register to handle tab list input immediately upon construction */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = TabList, meta = (ExposeOnSpawn = "true"))
	bool bAutoListenForInput;

	/**
	* Whether to defer until next tick rebuilding tab list when inserting new tab (rather than adding to the end).
	* Useful if inserting multiple tabs in the same tick as the tab list will only be rebuilt once.
	*/
	UPROPERTY(EditAnywhere, Category = TabList)
	bool bDeferRebuildingTabList;

protected:
	const TMap<FName, FCommonRegisteredTabInfo>& GetRegisteredTabsByID() const;

	UFUNCTION()
	void HandleTabButtonSelected(UCommonButtonBase* SelectedTabButton, int32 ButtonIndex);

	UFUNCTION()
	void HandlePreviousTabInputAction(bool& bPassthrough);
	
	UFUNCTION()
	void HandleNextTabInputAction(bool& bPassthrough);

	/** The activatable widget switcher that this tab list is associated with and manipulates */
	UPROPERTY(Transient)
	TWeakObjectPtr<UCommonAnimatedSwitcher> LinkedSwitcher;
	
	/** The button group that manages all the created tab buttons */
	UPROPERTY(Transient)
	TObjectPtr<UCommonButtonGroupBase> TabButtonGroup;

	/** Is the tab list currently listening for tab input actions? */
	bool bIsListeningForInput = false;

private:
	void HandleNextTabAction();
	void HandlePreviousTabAction();

	bool DeferredRebuildTabList(float DeltaTime);
	void RebuildTabList();

	/** Info about each of the currently registered tabs organized by a given registration name ID */
	UPROPERTY(Transient)
	TMap<FName, FCommonRegisteredTabInfo> RegisteredTabsByID;

	/** The registration ID of the currently active tab */
	FName ActiveTabID;

	bool bIsRebuildingList = false;
	bool bPendingRebuild = false;

	FUIActionBindingHandle NextTabActionHandle;
	FUIActionBindingHandle PrevTabActionHandle;
};
