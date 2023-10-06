// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Textures/SlateIcon.h"
#include "Widgets/SCompoundWidget.h"

enum ETabRole : uint8;
class SRCPanelDrawerButton;
class SVerticalBox;

/** The variety of panels we have in the RC Panel. */
enum class ERCPanels : uint8
{
	RCP_None,

	RCP_Properties,
	
	RCP_EntityDetails,

	RCP_Protocols,

	RCP_OutputLog,

	RCP_Live,

	RCP_Count,
};

/** State of the RC Panels. */
enum class ERCPanelState
{
	/** Panel is actively drawn. */
	Opened,

	/** Panel is collapsed. */
	Collapsed,
};

struct FDockingPanelConstants
{
	static const FVector2D MaxMinorPanelSize;
	static const FVector2D MaxMajorPanelSize;
	static const FVector2D GetMaxPanelSizeFor(ETabRole TabRole);
};

/** Arguments used to create a RC Panel Drawer button. */
struct FRCPanelDrawerArgs
{
	FRCPanelDrawerArgs(ERCPanels InPanel)
		: bDrawnByDefault(false)
		, bRotateIconBy90(false)
		, Icon()
		, Label(FText::FromName(NAME_None))
		, ToolTip(FText::FromName(NAME_None))
		, DrawerVisibility(EVisibility::Visible)
		, PanelID(InPanel)
		, PanelState(ERCPanelState::Collapsed)
	{
	}
	
	bool operator==(const FRCPanelDrawerArgs& OtherPanel)
	{
		return PanelID == OtherPanel.GetPanelID();
	}
	
	bool operator==(const FRCPanelDrawerArgs& OtherPanel) const
	{
		return PanelID == OtherPanel.GetPanelID();
	}

	ERCPanels GetPanelID() const
	{
		return PanelID;
	}

	bool IsActive() const
	{
		return PanelState == ERCPanelState::Opened;
	}

	void SetState(ERCPanelState NewState)
	{
		if (PanelState != NewState)
		{
			PanelState = NewState;
		}
	}

	/** Whether to draw this panel by default. */
	bool bDrawnByDefault;
	
	/** Whether to rotate the icon of this panel button. */
	bool bRotateIconBy90;

	/** The icon to use for the drawer panel button. */
	FSlateIcon Icon;

	/** The label to use for the drawer panel button. */
	FText Label;
	
	/** The tooltip to use for the drawer panel button. */
	FText ToolTip;
	
	/** Visibility of the drawer button. */
	EVisibility DrawerVisibility;

private:

	/** Unique Identifier to identify the panel. */
	ERCPanels PanelID;

	/** Active state of this panel. */
	ERCPanelState PanelState;
};

/** Delegate to be called when a drawer button is clicked. */
DECLARE_DELEGATE_TwoParams(FOnRCPanelDrawerButtonPressed, TSharedRef<FRCPanelDrawerArgs> /* Panel */, const bool /* bRemoveDrawer */)

/** Delegate to be called when a drawer button is clicked. */
DECLARE_DELEGATE_OneParam(FOnRCPanelToggled, ERCPanels /* PanelID */)

/** Delegate to be called to check whether drawer button can be clicked or not. */
DECLARE_DELEGATE_RetVal(bool, FCanToggleRCPanel)

/**
 * A custom tab drawer tailored for RC Panels.
 */
class SRCPanelDrawer : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRCPanelDrawer)
	{}
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

	/**
	 * @return true if this sidebar contains the provided tab
	 */
	bool IsRegistered(TSharedRef<FRCPanelDrawerArgs> InPanel) const;

	/**
	 * Adds a panel to the drawer.
	 */
	void RegisterPanel(TSharedRef<FRCPanelDrawerArgs> InPanel);

	/**
	 * Removes a panel from the drawer.
	 *
	 * Note it is not sufficient to call this to clean up the panel completely
	 * as it would just unregister the panel from this drawer.
	 * 
	 * @return true if the tab was found and removal was successful
	 */
	bool UnregisterPanel(TSharedRef<FRCPanelDrawerArgs> InPanelToRemove);

	/** Delegate to be called to check whether drawer button can be clicked or not. */
	FCanToggleRCPanel& CanToggleRCPanel() { return CanToggleRCPanelDelegate; }
	
	/** Delegate to be called when a drawer button is clicked. */
	FOnRCPanelToggled& OnRCPanelToggled() { return OnRCPanelToggledDelegate; }

	/** Toggles the panel. */
	void TogglePanel(TSharedRef<FRCPanelDrawerArgs> InPanel, const bool bRemoveDrawer = false);

private:

	/** Updates the appearance of the drawn panel. */
	void UpdateAppearance();

private:

	/** Container holds all drawer buttons. */
	TSharedPtr<SVerticalBox> PanelBox;

	/** Array of all registered panels. */
	TArray<TPair<TSharedRef<FRCPanelDrawerArgs>, TSharedRef<SRCPanelDrawerButton>>> Panels;

	/** Delegate to be called to check whether drawer button can be clicked or not. */
	FCanToggleRCPanel CanToggleRCPanelDelegate;
	
	/** Delegate to be called when a drawer button is clicked. */
	FOnRCPanelToggled OnRCPanelToggledDelegate;

	/** Generally speaking one drawer is only ever open at once. */
	ERCPanels OpenedDrawer;
};
