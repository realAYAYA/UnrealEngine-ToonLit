// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "Widgets/SWidget.h"
#include "Components/ContentWidget.h"
#include "MenuAnchor.generated.h"

class SMenuAnchor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMenuOpenChangedEvent, bool, bIsOpen);

/**
 * The Menu Anchor allows you to specify an location that a popup menu should be anchored to, 
 * and should be summoned from.
 * * Single Child
 * * Popup
 */
UCLASS()
class UMG_API UMenuAnchor : public UContentWidget
{
	GENERATED_UCLASS_BODY()

public:

	DECLARE_DYNAMIC_DELEGATE_RetVal(UUserWidget*, FGetUserWidget);

	/**
	 * The widget class to spawn when the menu is required.  Creates the widget freshly each time.  
	 * If you want to customize the creation of the popup, you should bind a function to OnGetMenuContentEvent 
	 * instead.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Menu Anchor")
	TSubclassOf<class UUserWidget> MenuClass;

	/** Deprecated. Called when the menu content is requested to allow a more customized handling over what to display */
	UE_DEPRECATED(4.26, "Use OnGetUserMenuContentEvent instead, you may need to make the previous binding return an User Widget.")
	UPROPERTY(EditAnywhere, Category = "Events", meta = (DeprecationMessage = "4.26. Use OnGetUserMenuContentEvent instead, you may need to make the previous binding return an User Widget."))
	FGetWidget OnGetMenuContentEvent;

	/** Called when the menu content is requested to allow a more customized handling over what to display */
	UPROPERTY(EditAnywhere, Category="Events")
	FGetUserWidget OnGetUserMenuContentEvent;
	
	/** The placement location of the summoned widget. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Menu Anchor")
	TEnumAsByte<EMenuPlacement> Placement;

	/** Should the menu anchor attempt to fit the menu inside the window. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Menu Anchor", meta=(ScriptName="ShouldFitInWindow"))
	bool bFitInWindow;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "Menu Anchor")
	bool ShouldDeferPaintingAfterWindowContent;

	/** Does this menu behave like a normal stacked menu? Set it to false to control the menu's lifetime yourself. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "Menu Anchor")
	bool UseApplicationMenuStack;

public:
	/** Called when the opened state of the menu changes */
	UPROPERTY(BlueprintAssignable, Category="Menu Anchor|Event")
	FOnMenuOpenChangedEvent OnMenuOpenChanged;

public:
	//TODO UMG Add Set MenuClass

	UFUNCTION(BlueprintCallable, Category = "Menu Anchor")
	void SetPlacement(TEnumAsByte<EMenuPlacement> InPlacement);

	UFUNCTION(BlueprintCallable, Category = "Menu Anchor")
	void FitInWindow(bool bFit);

public:

	/**
	 * Toggles the menus open state.
	 *
	 * @param bFocusOnOpen  Should we focus the popup as soon as it opens?
	 */
	UFUNCTION(BlueprintCallable, Category="Menu Anchor")
	void ToggleOpen(bool bFocusOnOpen);

	/** Opens the menu if it is not already open */
	UFUNCTION(BlueprintCallable, Category="Menu Anchor")
	void Open(bool bFocusMenu);

	/** Closes the menu if it is currently open. */
	UFUNCTION(BlueprintCallable, Category="Menu Anchor")
	void Close();

	/** Returns true if the popup is open; false otherwise. */
	UFUNCTION(BlueprintCallable, Category="Menu Anchor")
	bool IsOpen() const;

	/**
	 * Returns true if we should open the menu due to a click. Sometimes we should not, if
	 * the same MouseDownEvent that just closed the menu is about to re-open it because it 
	 * happens to land on the button.
	 */
	UFUNCTION(BlueprintCallable, Category="Menu Anchor")
	bool ShouldOpenDueToClick() const;

	/** Returns the current menu position */
	UFUNCTION(BlueprintCallable, Category="Menu Anchor")
	FVector2D GetMenuPosition() const;

	/** Returns whether this menu has open submenus */
	UFUNCTION(BlueprintCallable, Category="Menu Anchor")
	bool HasOpenSubMenus() const;

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

protected:

	// UPanelWidget
	virtual void OnSlotAdded(UPanelSlot* Slot) override;
	virtual void OnSlotRemoved(UPanelSlot* Slot) override;
	// End UPanelWidget

	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget interface

protected:
	TSharedRef<SWidget> HandleGetMenuContent();
	void HandleMenuOpenChanged(bool bIsOpen);

protected:
	TSharedPtr<SMenuAnchor> MyMenuAnchor;
};
