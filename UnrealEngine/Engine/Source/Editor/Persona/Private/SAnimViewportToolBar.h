// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SViewportToolBar.h"
#include "Layout/Visibility.h"
#include "SAnimationEditorViewport.h"
#include "SEditorViewport.h"
#include "ScopedTransaction.h"
#include "Styling/SlateColor.h"
#include "Types/SlateEnums.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"

class FMenuBuilder;
class SComboButton;


/**
 * A level viewport toolbar widget that is placed in a viewport
 */
class SAnimViewportToolBar : public SViewportToolBar
{
public:
	SLATE_BEGIN_ARGS( SAnimViewportToolBar )
		: _ShowShowMenu(true)
		, _ShowCharacterMenu(true)
		, _ShowLODMenu(true)
		, _ShowPlaySpeedMenu(true)
		, _ShowPhysicsMenu(false)
	{}

	SLATE_ARGUMENT(TArray<TSharedPtr<FExtender>>, Extenders)

	SLATE_ARGUMENT(FName, ContextName)

	SLATE_ARGUMENT(bool, ShowShowMenu)

	SLATE_ARGUMENT(bool, ShowCharacterMenu)

	SLATE_ARGUMENT(bool, ShowLODMenu)

	SLATE_ARGUMENT(bool, ShowPlaySpeedMenu)

	SLATE_ARGUMENT(bool, ShowFloorOptions)

	SLATE_ARGUMENT(bool, ShowTurnTable)

	SLATE_ARGUMENT(bool, ShowPhysicsMenu)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<class SAnimationEditorViewportTabBody> InViewport, TSharedPtr<class SEditorViewport> InRealViewport);

	/** Get the pinned commands widget */
	TSharedPtr<IPinnedCommandList> GetPinnedCommandList() { return PinnedCommands; }

private:
	/**
	 * Generates the toolbar view menu content 
	 */
	TSharedRef<SWidget> GenerateViewMenu() const;

	/**
	* Generates the physics view menu content
	*/
	TSharedRef<SWidget> GeneratePhysicsMenu() const;

	/*
	 * Generates the toolbar character menu content 
	 */
	TSharedRef<SWidget> GenerateCharacterMenu() const;

	/**
	 * Generates the toolbar show menu content 
	 */
	TSharedRef<SWidget> GenerateShowMenu() const;

	/**
	 * Generates the Show -> Scene sub menu content
	 */
	void FillCharacterSceneMenu(FMenuBuilder& MenuBuilder) const;

	/**
	 * Generates the Show -> Advanced sub menu content
	 */
	void FillCharacterAdvancedMenu(FMenuBuilder& MenuBuilder) const;

	/**
	* Generates the Show -> Mirror sub menu content
	*/
	void FillCharacterMirrorMenu(FMenuBuilder& MenuBuilder) const;

	/**
	* Generates the Show -> Clothing sub menu content
	*/
	void FillCharacterClothingMenu(FMenuBuilder& MenuBuilder);

	/**
	 * Generates the toolbar LOD menu content 
	 */
	TSharedRef<SWidget> GenerateLODMenu() const;

	/**
	 * Returns the label for the "LOD" tool bar menu, which changes depending on the current LOD selection
	 *
	 * @return	Label to use for this LOD label
	 */
	FText GetLODMenuLabel() const;

	/**
	 * Generates the toolbar viewport type menu content 
	 */
	TSharedRef<SWidget> GenerateViewportTypeMenu() const;

	/**
	 * Generates the toolbar playback menu content 
	 */
	TSharedRef<SWidget> GeneratePlaybackMenu() const;

	/** Generate the turntable menu entries */
	void GenerateTurnTableMenu(FMenuBuilder& MenuBuilder) const;

	/**
	* Generate color of the text on the top
	*/ 
	FSlateColor GetFontColor() const;

	/**
	 * Returns the label for the Playback tool bar menu, which changes depending on the current playback speed
	 *
	 * @return	Label to use for this Menu
	 */
	FText GetPlaybackMenuLabel() const;

	/**
	 * Returns the label for the Viewport type tool bar menu, which changes depending on the current selected type
	 *
	 * @return	Label to use for this Menu
	 */
	FText GetCameraMenuLabel() const;

	/* Returns the label icon for the Camera tool bar menu, which changes depending on viewport type 
	 *
	 * @return	Label icon to use for this menu label
	 *
	 */
	const FSlateBrush* GetCameraMenuLabelIcon() const;

	/** Called by the FOV slider in the perspective viewport to get the FOV value */
	TOptional<float> OnGetFOVValue() const;
	/** Called when the FOV slider is adjusted in the perspective viewport */
	void OnFOVValueChanged( float NewValue );
	/** Called when a value is entered into the FOV slider/box in the perspective viewport */
	void OnFOVValueCommitted( float NewValue, ETextCommit::Type CommitInfo );

	/** Called when Camera speed is changed */
	void OnCamSpeedChanged(int32 NewValue);
	/** Called when Camera speed scalar is changed */
	void OnCamSpeedScalarChanged(float NewValue);

	/** Called by the floor offset slider in the perspective viewport to get the offset value */
	float OnGetFloorOffset() const;
	/** Called when the floor offset slider is adjusted in the perspective viewport */
	void OnBeginSliderMovementFloorOffset();
	void OnFloorOffsetChanged( float NewValue );
	void OnFloorOffsetCommitted ( float NewValue, ETextCommit::Type CommitType );

	/** Called when users select the mirror table in the menu */
	void OnMirrorDataTableSelected(const FAssetData& SelectedMirrorTableData);
	
	// Called to determine if the gizmos can be used in the current preview
	EVisibility GetTransformToolbarVisibility() const;

	/** Build the floor offset widget */
	TSharedRef<SWidget> MakeFloorOffsetWidget() const;

	/** Build the FOV widget */
	TSharedRef<SWidget> MakeFOVWidget() const;

	/** Build the follow bone widget */
	TSharedRef<SWidget> MakeFollowBoneWidget() const;
	TSharedRef<SWidget> MakeFollowBoneWidget(TWeakPtr<SComboButton> InWeakComboButton) const;

	/** Build the follow bone widget (as a combo button) */
	TSharedRef<SWidget> MakeFollowBoneComboWidget() const;

public:
	/** Extend the anim menu */
	void AddMenuExtender(FName MenuToExtend, FMenuExtensionDelegate MenuBuilderDelegate);

private:
	/** Build the main toolbar */
	TSharedRef<SWidget> MakeViewportToolbar(TSharedPtr<class SEditorViewport> InRealViewport);

	/** Extend the view menu */
	TSharedRef<FExtender> GetViewMenuExtender(TSharedPtr<class SEditorViewport> InRealViewport);

private:
	/** The viewport that we are in */
	TWeakPtr<class SAnimationEditorViewportTabBody> Viewport;

	/** Command list */
	TSharedPtr<FUICommandList> CommandList;

	/** Extenders */
	TArray<TSharedPtr<FExtender>> Extenders;

	/** Pinned commands widget */
	TSharedPtr<IPinnedCommandList> PinnedCommands;

	/** Transaction to handle scoping updates for sliders we own that transact objects (i.e. floor mesh) */
	TUniquePtr<FScopedTransaction> PendingTransaction;

	/** Whether to show the 'Show' menu */
	bool bShowShowMenu;

	/** Whether to show the 'Character' menu */
	bool bShowCharacterMenu;

	/** Whether to show the 'LOD' menu */
	bool bShowLODMenu;

	/** Whether to show the 'Play Speed' menu */
	bool bShowPlaySpeedMenu;

	/** Whether to show options relating to floor height */
	bool bShowFloorOptions;

	/** Whether to show options relating to turntable */
	bool bShowTurnTable;

	/** Whether to show the physics menu*/
	bool bShowPhysicsMenu;
};
