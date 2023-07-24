﻿// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Layout/Margin.h"
#include "GenericPlatform/GenericWindow.h"
#include "Widgets/SWidget.h"
#include "Components/ContentWidget.h"

#include "WindowTitleBarArea.generated.h"

class SWindowTitleBarArea;

/**
* A panel for defining a region of the UI that should allow users to drag the window on desktop platforms.
*/

UCLASS()
class UMG_API UWindowTitleBarArea : public UContentWidget
{
	GENERATED_UCLASS_BODY()

public:

	UE_DEPRECATED(5.2, "Direct access to bWindowButtonsEnabled is deprecated. Please use the getter or setter.")
	/** Should the title bar area diaplay window minimize/maximize/close buttons. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "IsWindowButtonsEnabled", Setter = "SetWindowButtonsEnabled", Category = "Interaction", meta = (DisplayName = "Window Buttons Enabled"))
	bool bWindowButtonsEnabled;

	UE_DEPRECATED(5.2, "Direct access to bDoubleClickTogglesFullscreen is deprecated. Please use the getter or setter.")
	/** Should double clicking the title bar area toggle fullscreen instead of maximizing the window. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "IsDoubleClickTogglesFullscreen", Setter = "SetDoubleClickTogglesFullscreen", Category = "Interaction", meta = (DisplayName = "Toggle Fullscreen"))
	bool bDoubleClickTogglesFullscreen;

public:

	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetPadding(FMargin InPadding);

	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetHorizontalAlignment(EHorizontalAlignment InHorizontalAlignment);

	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetVerticalAlignment(EVerticalAlignment InVerticalAlignment);

	void SetWindowButtonsEnabled(bool InWindowButtonsEnabled);

	bool IsWindowButtonsEnabled() const;

	void SetDoubleClickTogglesFullscreen(bool InDoubleClickTogglesFullscreen);

	bool IsDoubleClickTogglesFullscreen() const;

public:

	//~ Begin UVisual Interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~ End UVisual Interface

	//~ Begin UObject Interface
	virtual void PostLoad() override;
	//~ End UObject Interface

	//~ Begin UWidget Interface
#if WITH_EDITOR	
	virtual const FText GetPaletteCategory() override;
#endif
	//~ End UWidget Interface

protected:
	//~ Begin UWidget Interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	//~ End UWidget Interface

protected:

	// UPanelWidget
	virtual UClass* GetSlotClass() const override;
	virtual void OnSlotAdded(UPanelSlot* Slot) override;
	virtual void OnSlotRemoved(UPanelSlot* Slot) override;
	// End UPanelWidget

protected:
	/** Cached pointer to the underlying slate button owned by this UWidget */
	TSharedPtr<SWindowTitleBarArea> MyWindowTitleBarArea;

private:

	bool HandleWindowAction(const TSharedRef<FGenericWindow>& PlatformWindow, EWindowAction::Type WindowAction);
	void RequestToggleFullscreen();

private:

	FDelegateHandle WindowActionNotificationHandle;
};
