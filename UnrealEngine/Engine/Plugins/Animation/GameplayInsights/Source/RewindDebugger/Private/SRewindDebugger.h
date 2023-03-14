// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "BindableProperty.h"
#include "CoreMinimal.h"
#include "Framework/Commands/UICommandList.h"
#include "IRewindDebuggerView.h"
#include "RewindDebuggerCommands.h"
#include "SRewindDebuggerComponentTree.h"
#include "SRewindDebuggerTimelines.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/SCompoundWidget.h"
#include "RewindDebuggerSettings.h"

class FRewindDebuggerModule;
class SDockTab;

class SSearchBox;
class SCheckBox;

class SRewindDebugger : public SCompoundWidget
{
	typedef TBindablePropertyInitializer<FString, BindingType_Out> DebugTargetInitializer;

public:
	DECLARE_DELEGATE_TwoParams( FOnScrubPositionChanged, double, bool )
	DECLARE_DELEGATE_OneParam( FOnViewRangeChanged, const TRange<double>& )
	DECLARE_DELEGATE_OneParam( FOnDebugTargetChanged, TSharedPtr<FString> )
	DECLARE_DELEGATE_OneParam( FOnComponentDoubleClicked, TSharedPtr<RewindDebugger::FRewindDebuggerTrack> )
	DECLARE_DELEGATE_OneParam( FOnComponentSelectionChanged, TSharedPtr<RewindDebugger::FRewindDebuggerTrack> )
	DECLARE_DELEGATE_RetVal( TSharedPtr<SWidget>, FBuildComponentContextMenu )

	SLATE_BEGIN_ARGS(SRewindDebugger) { }
		SLATE_ARGUMENT( TArray< TSharedPtr< RewindDebugger::FRewindDebuggerTrack > >*, DebugComponents );
		SLATE_ARGUMENT(DebugTargetInitializer, DebugTargetActor);
		SLATE_ARGUMENT(TBindablePropertyInitializer<double>, TraceTime);
		SLATE_ARGUMENT(TBindablePropertyInitializer<float>, RecordingDuration);
		SLATE_ATTRIBUTE(double, ScrubTime);
		SLATE_ATTRIBUTE(bool, IsPIESimulating);
		SLATE_EVENT( FOnScrubPositionChanged, OnScrubPositionChanged);
		SLATE_EVENT( FOnViewRangeChanged, OnViewRangeChanged);
		SLATE_EVENT( FBuildComponentContextMenu, BuildComponentContextMenu );
		SLATE_EVENT( FOnComponentDoubleClicked, OnComponentDoubleClicked );
		SLATE_EVENT( FOnComponentSelectionChanged, OnComponentSelectionChanged );
	SLATE_END_ARGS()
	
public:

	/**
	* Default constructor.
	*/
	SRewindDebugger();
	virtual ~SRewindDebugger();

	/**
	* Constructs the application.
	*
	* @param InArgs The Slate argument list.
	* @param ConstructUnderMajorTab The major tab which will contain the session front-end.
	* @param ConstructUnderWindow The window in which this widget is being constructed.
	* @param InStyleSet The style set to use.
	*/
	void Construct(const FArguments& InArgs, TSharedRef<FUICommandList> CommandList, const TSharedRef<SDockTab>& ConstructUnderMajorTab, const TSharedPtr<SWindow>& ConstructUnderWindow);

	void TrackCursor(bool bReverse);
	void RefreshDebugComponents();
private:
	void SetViewRange(TRange<double> NewRange);

	void MainFrameCreationFinished(TSharedPtr<SWindow> InRootWindow, bool bIsNewProjectWindow);

	void ToggleDisplayEmptyTracks();
	bool ShouldDisplayEmptyTracks() const;
	
	virtual FReply OnPreviewKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual bool SupportsKeyboardFocus() const override { return true; }
	
	// Time Slider
	TAttribute<double> ScrubTimeAttribute;
	TAttribute<bool> IsPIESimulating;
	TAttribute<bool> TrackScrubbingAttribute;
	FOnScrubPositionChanged OnScrubPositionChanged;
	FOnViewRangeChanged OnViewRangeChanged;
	TRange<double> ViewRange;
	TBindableProperty<double> TraceTime;
	TBindableProperty<float> RecordingDuration;

	TSharedPtr<FUICommandList> CommandList;
	const FRewindDebuggerCommands & Commands;
	
	// debug actor selector
	TSharedRef<SWidget> MakeSelectActorMenu();
	void SetDebugTargetActor(AActor* Actor);
	FReply OnSelectActorClicked();

	TBindableProperty<FString, BindingType_Out> DebugTargetActor;

	void TraceTimeChanged(double Time);

	TSharedRef<SWidget> MakeMainMenu();
	TSharedRef<SWidget> MakeFilterMenu();
	void MakeViewsMenu(FMenuBuilder& MenuBuilder);
	
	// component tree view
	TArray<TSharedPtr<RewindDebugger::FRewindDebuggerTrack>>* DebugComponents;
	TSharedPtr<RewindDebugger::FRewindDebuggerTrack> SelectedComponent;
    void ComponentSelectionChanged(TSharedPtr<RewindDebugger::FRewindDebuggerTrack> SelectedItem, ESelectInfo::Type SelectInfo);
	FBuildComponentContextMenu BuildComponentContextMenu;
	FOnComponentSelectionChanged OnComponentSelectionChanged;
	
	TSharedPtr<SRewindDebuggerComponentTree> ComponentTreeView;
	TSharedPtr<SRewindDebuggerTimelines> TimelinesView;
	TSharedPtr<SWidget> OnContextMenuOpening();
	
	TSharedPtr<SSearchBox> TrackFilterBox;
	
	bool bInSelectionChanged = false;
	bool bDisplayEmptyTracks = false;

	URewindDebuggerSettings & Settings;
};
