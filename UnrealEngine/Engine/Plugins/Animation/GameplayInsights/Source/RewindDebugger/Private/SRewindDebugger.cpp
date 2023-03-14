// Copyright Epic Games, Inc. All Rights Reserved.

#include "SRewindDebugger.h"

#include "ActorPickerMode.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Insights/IUnrealInsightsModule.h"
#include "Modules/ModuleManager.h"
#include "ObjectTrace.h"
#include "RewindDebuggerStyle.h"
#include "RewindDebuggerCommands.h"
#include "SSimpleTimeSlider.h"
#include "SceneOutlinerPublicTypes.h"
#include "SceneOutlinerModule.h"
#include "Selection.h"
#include "Styling/SlateIconFinder.h"
#include "ToolMenu.h"
#include "ToolMenus.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Kismet2/DebuggerCommands.h"
#include "RewindDebuggerSettings.h"

#define LOCTEXT_NAMESPACE "SRewindDebugger"

SRewindDebugger::SRewindDebugger() 
	: SCompoundWidget()
	, ViewRange(0,10)
	, Commands(FRewindDebuggerCommands::Get())
	, DebugComponents(nullptr)
	, Settings(URewindDebuggerSettings::Get())
{ 
}

SRewindDebugger::~SRewindDebugger() 
{
}

void SRewindDebugger::TrackCursor(bool bReverse)
{
	float ScrubTime = ScrubTimeAttribute.Get();
	TRange<double> CurrentViewRange = ViewRange;
	float ViewSize = CurrentViewRange.GetUpperBoundValue() - CurrentViewRange.GetLowerBoundValue();

	static const double LeadingEdgeSize = 0.05;
	static const double TrailingEdgeThreshold = 0.01;

	if(bReverse)
	{
		// playing in reverse (cursor moving left)
		if (ScrubTime < CurrentViewRange.GetLowerBoundValue() + ViewSize * LeadingEdgeSize)
		{
			CurrentViewRange.SetLowerBound(ScrubTime - ViewSize * LeadingEdgeSize);
			CurrentViewRange.SetUpperBound(CurrentViewRange.GetLowerBoundValue() + ViewSize);
		}
		if (ScrubTime > CurrentViewRange.GetUpperBoundValue() + ViewSize * TrailingEdgeThreshold)
		{
			CurrentViewRange.SetUpperBound(ScrubTime);
			CurrentViewRange.SetLowerBound(CurrentViewRange.GetUpperBoundValue() - ViewSize);
		}
	}
	else
	{
		// playing normally or recording (cursor moving right)
		if (ScrubTime > CurrentViewRange.GetUpperBoundValue() - ViewSize * LeadingEdgeSize)
		{
			CurrentViewRange.SetUpperBound(ScrubTime + ViewSize * LeadingEdgeSize);
			CurrentViewRange.SetLowerBound(CurrentViewRange.GetUpperBoundValue() - ViewSize);
		}
		if (ScrubTime < CurrentViewRange.GetLowerBoundValue() - ViewSize * TrailingEdgeThreshold)
		{
			CurrentViewRange.SetLowerBound(ScrubTime);
			CurrentViewRange.SetUpperBound(CurrentViewRange.GetLowerBoundValue() + ViewSize);
		}
	}

	SetViewRange(CurrentViewRange);
}

void SRewindDebugger::SetViewRange(TRange<double> NewRange)
{
	ViewRange = NewRange;
	OnViewRangeChanged.ExecuteIfBound(NewRange);
}

void SRewindDebugger::ToggleDisplayEmptyTracks()
{
	Settings.bShowEmptyObjectTracks = !Settings.bShowEmptyObjectTracks;
	RefreshDebugComponents();
}

bool SRewindDebugger::ShouldDisplayEmptyTracks() const
{
	return Settings.bShowEmptyObjectTracks;
}

FReply SRewindDebugger::OnPreviewKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const FInputChord KeyEventAsInputChord = FInputChord(InKeyEvent.GetKey(), EModifierKey::FromBools(InKeyEvent.IsControlDown(), InKeyEvent.IsAltDown(), InKeyEvent.IsShiftDown(), InKeyEvent.IsCommandDown()));
	FReply bReply = FReply::Unhandled();

	// Handle Rewind Debugger VCR Commands
	if (CommandList->ProcessCommandBindings(InKeyEvent))
	{
		bReply = FReply::Handled();
	}

	// Prevent bubbling up shortcut for "ReversePlay"
	if (Commands.ReversePlay->HasDefaultChord(KeyEventAsInputChord) && !IsPIESimulating.Get())
	{
		bReply = FReply::Handled();
	}

	return bReply;
}

void SRewindDebugger::SetDebugTargetActor(AActor* Actor)
{
	DebugTargetActor.Set(Actor->GetName());
}

TSharedRef<SWidget> SRewindDebugger::MakeSelectActorMenu()
{
	// this menu is partially duplicated from LevelSequenceEditorActorBinding which has a similar workflow for adding actors to sequencer

	FMenuBuilder MenuBuilder(true, nullptr);

	// Set up a menu entry to choosing the selected actor(s) to the sequencer (maybe move this to a submenu and put each selected actor there)
	TArray<AActor*> SelectedActors;
	GEditor->GetSelectedActors()->GetSelectedObjects(SelectedActors);

	if (SelectedActors.Num() >= 1)
	{
		MenuBuilder.BeginSection("From Selection Section", LOCTEXT("FromSelection", "From Selection"));
		if (SelectedActors.Num() == 1)
		{
			AActor* SelectedActor = SelectedActors[0];

			FText SelectedLabel = FText::FromString(SelectedActor->GetActorLabel());
			FSlateIcon ActorIcon = FSlateIconFinder::FindIconForClass(SelectedActors[0]->GetClass());

			MenuBuilder.AddMenuEntry(SelectedLabel, FText(), ActorIcon, FExecuteAction::CreateLambda([this, SelectedActor]{
				FSlateApplication::Get().DismissAllMenus();
				SetDebugTargetActor(SelectedActor);
			}));
		}
		else if (SelectedActors.Num() >= 1)
		{
			MenuBuilder.AddSubMenu(
				LOCTEXT("FromSelection", "From Selection"),
				LOCTEXT("FromSelection_Tooltip", "Select an Actor from the list of selected Actors"),
				FNewMenuDelegate::CreateLambda([this, SelectedActors](FMenuBuilder& SubMenuBuilder)
				{
					for(AActor* SelectedActor : SelectedActors)
					{
						FText SelectedLabel = FText::FromString(SelectedActor->GetActorLabel());
						FSlateIcon ActorIcon = FSlateIconFinder::FindIconForClass(SelectedActors[0]->GetClass());

						SubMenuBuilder.AddMenuEntry(SelectedLabel, FText(), ActorIcon, FExecuteAction::CreateLambda([this, SelectedActor]{
							FSlateApplication::Get().DismissAllMenus();
							SetDebugTargetActor(SelectedActor);
						}));
					}

				})
			);

		}
		MenuBuilder.EndSection();
	}

	// todo: add special menu item for player controlled character

	MenuBuilder.BeginSection("ChooseActorSection", LOCTEXT("ChooseActor", "Choose Actor:"));

	// Set up a menu entry to select any arbitrary actor
	FSceneOutlinerInitializationOptions InitOptions;
	{
		// We hide the header row to keep the UI compact.
		InitOptions.bShowHeaderRow = false;
		InitOptions.bShowSearchBox = true;
		InitOptions.bShowCreateNewFolder = false;
		InitOptions.bFocusSearchBoxWhenOpened = true;

		// Only want the actor label column
		InitOptions.ColumnMap.Add(FSceneOutlinerBuiltInColumnTypes::Label(), FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, 0));

		// todo: optionally filter for only actors that have debug data
		//InitOptions.Filters->AddFilterPredicate<FActorTreeItem>(FActorTreeItem::FFilterPredicate::CreateLambda(IsActorValidForPossession, ExistingPossessedObjects));
	}

	// actor selector to allow the user to choose an actor
	FSceneOutlinerModule& SceneOutlinerModule = FModuleManager::LoadModuleChecked<FSceneOutlinerModule>("SceneOutliner");
	TSharedRef< SWidget > MiniSceneOutliner =
		SNew(SBox)
		.MaxDesiredHeight(400.0f)
		.WidthOverride(300.0f)
		[
			SceneOutlinerModule.CreateActorPicker(
				InitOptions,
				FOnActorPicked::CreateLambda([this](AActor* Actor){
					// Create a new binding for this actor
					FSlateApplication::Get().DismissAllMenus();
					SetDebugTargetActor(Actor);
				})
			)
		];

	MenuBuilder.AddWidget(MiniSceneOutliner, FText::GetEmpty(), true);
	MenuBuilder.EndSection();



	return MenuBuilder.MakeWidget();
}

void SRewindDebugger::Construct(const FArguments& InArgs, TSharedRef<FUICommandList> InCommandList, const TSharedRef<SDockTab>& ConstructUnderMajorTab, const TSharedPtr<SWindow>& ConstructUnderWindow)
{
	OnScrubPositionChanged = InArgs._OnScrubPositionChanged;
	OnViewRangeChanged = InArgs._OnViewRangeChanged;
	OnComponentSelectionChanged = InArgs._OnComponentSelectionChanged;
	BuildComponentContextMenu = InArgs._BuildComponentContextMenu;
	ScrubTimeAttribute = InArgs._ScrubTime;
	DebugComponents = InArgs._DebugComponents;
	TraceTime.Initialize(InArgs._TraceTime);
	RecordingDuration.Initialize(InArgs._RecordingDuration);
	DebugTargetActor.Initialize(InArgs._DebugTargetActor);
	IsPIESimulating = InArgs._IsPIESimulating;
	CommandList = InCommandList;
	
	TrackFilterBox = SNew(SSearchBox).HintText(LOCTEXT("Filter Tracks","Filter Tracks")).OnTextChanged_Lambda([this](const FText&)
	{
		RefreshDebugComponents();
	});
	
	FSlimHorizontalToolBarBuilder ToolBarBuilder(CommandList, FMultiBoxCustomization::None, nullptr, true);
	
	ToolBarBuilder.SetStyle(&FAppStyle::Get(), "PaletteToolBar");
	ToolBarBuilder.BeginSection("Debugger");
	{
		ToolBarBuilder.AddToolBarButton(Commands.FirstFrame);
		ToolBarBuilder.AddToolBarButton(Commands.PreviousFrame);
		ToolBarBuilder.AddToolBarButton(Commands.ReversePlay);
		ToolBarBuilder.AddToolBarButton(Commands.Pause, NAME_None, {}, FText::Format(LOCTEXT("PauseButtonTooltip", "{0} ({1})"), Commands.Pause->GetDescription(), Commands.PauseOrPlay->GetInputText()));
		ToolBarBuilder.AddToolBarButton(Commands.Play, NAME_None, {}, FText::Format(LOCTEXT("PlayButtonTooltip", "{0} ({1}) or"), Commands.Play->GetDescription(), Commands.PauseOrPlay->GetInputText(), Commands.Play->GetInputText()));
		ToolBarBuilder.AddToolBarButton(Commands.NextFrame);
		ToolBarBuilder.AddToolBarButton(Commands.LastFrame);
		ToolBarBuilder.AddToolBarButton(Commands.StartRecording);
		ToolBarBuilder.AddToolBarButton(Commands.StopRecording);
	}
	ToolBarBuilder.EndSection();

	TSharedPtr<SScrollBar> ScrollBar = SNew(SScrollBar); 

	ComponentTreeView =	SNew(SRewindDebuggerComponentTree)
		.ExternalScrollBar(ScrollBar)
		.OnExpansionChanged_Lambda([this]() { TimelinesView->RestoreExpansion(); })
		.OnScrolled_Lambda([this](double ScrollOffset){ TimelinesView->ScrollTo(ScrollOffset); })
		.DebugComponents(InArgs._DebugComponents)
		.OnMouseButtonDoubleClick(InArgs._OnComponentDoubleClicked)
		.OnContextMenuOpening(this, &SRewindDebugger::OnContextMenuOpening)
		.OnSelectionChanged_Lambda(
			[this](TSharedPtr<RewindDebugger::FRewindDebuggerTrack> SelectedItem, ESelectInfo::Type SelectInfo)
			{
				if (!bInSelectionChanged)
				{
					bInSelectionChanged = true;
					TimelinesView->SetSelection(SelectedItem);
					ComponentSelectionChanged(SelectedItem, SelectInfo);
					bInSelectionChanged = false;
				}
			});

	 TimelinesView = SNew(SRewindDebuggerTimelines)
		.ExternalScrollbar(ScrollBar)
		.OnExpansionChanged_Lambda([this]() { ComponentTreeView->RestoreExpansion(); })
		.OnScrolled_Lambda([this](double ScrollOffset){ ComponentTreeView->ScrollTo(ScrollOffset); })
		.DebugComponents(InArgs._DebugComponents)
		.ViewRange_Lambda([this](){return ViewRange;})
		.ClampRange_Lambda(
			 [this]()
			 {
				 return TRange<double>(0.0f, RecordingDuration.Get());
			 })
		.OnViewRangeChanged(this, &SRewindDebugger::SetViewRange)
		.ScrubPosition(ScrubTimeAttribute)
		.OnScrubPositionChanged_Lambda(
			[this](double NewScrubTime, bool bIsScrubbing)
			{
				if (bIsScrubbing)
				{
					OnScrubPositionChanged.ExecuteIfBound( NewScrubTime, bIsScrubbing );
				}
			})
		.OnSelectionChanged_Lambda(
			[this](TSharedPtr<RewindDebugger::FRewindDebuggerTrack> SelectedItem, ESelectInfo::Type SelectInfo)
			{
				if (!bInSelectionChanged)
				{
					bInSelectionChanged = true;
					ComponentTreeView->SetSelection(SelectedItem);
					ComponentSelectionChanged(SelectedItem, SelectInfo);
					bInSelectionChanged = false;
				}
			});

	UToolMenu* Menu = UToolMenus::Get()->FindMenu("RewindDebugger.MainMenu");

	ChildSlot
	[
		SNew(SVerticalBox)
		 + SVerticalBox::Slot().AutoHeight()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SComboButton)
						.ComboButtonStyle(FAppStyle::Get(), "SimpleComboButton")
						.OnGetMenuContent(this, &SRewindDebugger::MakeMainMenu)
						.ButtonContent()
					[
						SNew(SImage)
						.Image(FRewindDebuggerStyle::Get().GetBrush("RewindDebugger.MenuIcon"))
					]
				]
				+SHorizontalBox::Slot().FillWidth(1.0)
				[
					ToolBarBuilder.MakeWidget()
				]
			]
		]
		 + SVerticalBox::Slot().FillHeight(1.0)
		 [
			SNew(SSplitter)
			+SSplitter::Slot().MinSize(280).Value(0)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot() .FillWidth(1.0f)
					[
						SNew(SComboButton)
						.OnGetMenuContent(this, &SRewindDebugger::MakeSelectActorMenu)
						.ButtonContent()
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot().AutoWidth().Padding(3)
							[
								SNew(SImage)
								.Image_Lambda([this]
									{
										FSlateIcon ActorIcon = FSlateIconFinder::FindIconForClass(AActor::StaticClass());
										if (DebugComponents != nullptr && DebugComponents->Num()>0)
										{
											if (UObject* Object = FObjectTrace::GetObjectFromId((*DebugComponents)[0]->GetObjectId()))
											{
												ActorIcon = FSlateIconFinder::FindIconForClass(Object->GetClass());
											}
										}

										return ActorIcon.GetIcon();
									}
								)
							]
							+SHorizontalBox::Slot().Padding(3)
							[
								SNew(STextBlock)
								.Text_Lambda([this](){
									if (DebugComponents == nullptr || DebugComponents->Num()==0)
									{
										return LOCTEXT("Select Actor", "Debug Target Actor");
									}

									FText ReadableName = (*DebugComponents)[0]->GetDisplayName();

									if (UObject* Object = FObjectTrace::GetObjectFromId((*DebugComponents)[0]->GetObjectId()))
									{
										if (AActor* Actor = Cast<AActor>(Object))
										{
											ReadableName = FText::FromString(Actor->GetActorLabel());
										}
									}

									return ReadableName;
								} )
							]
						]
					]
					+SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Right)
					[
						SNew(SButton)
							.ButtonStyle(FAppStyle::Get(), "SimpleButton")
							.OnClicked(this, &SRewindDebugger::OnSelectActorClicked)
							.ToolTipText(LOCTEXT("SelectActorTooltip", "Select Target Actor in Scene (Eject player control first)"))
							.IsEnabled_Lambda([]()
								{
									return !FPlayWorldCommandCallbacks::IsInPIE();
								}
							)
						[
							SNew(SImage)
							.Image(FRewindDebuggerStyle::Get().GetBrush("RewindDebugger.SelectActor"))
						]
					]
					+SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SComboButton)
							.ComboButtonStyle(FAppStyle::Get(), "SimpleComboButton")
							.OnGetMenuContent(this, &SRewindDebugger::MakeFilterMenu)
							.ButtonContent()
						[
							SNew(SImage)
							.Image(FAppStyle::Get().GetBrush("Icons.Filter"))
						]
					]
				]
				+SVerticalBox::Slot().FillHeight(1.0f)
				[
					ComponentTreeView.ToSharedRef()
				]
			]
			+SSplitter::Slot() 
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot().AutoHeight()
				[
					SNew(SSimpleTimeSlider)
						.DesiredSize({100,24})
						.ClampRangeHighlightSize(0.15f)
						.ClampRangeHighlightColor(FLinearColor::Red.CopyWithNewOpacity(0.5f))
						.ScrubPosition(ScrubTimeAttribute)
						.ViewRange_Lambda([this](){ return ViewRange; })
						.OnViewRangeChanged(this, &SRewindDebugger::SetViewRange)
						.ClampRange_Lambda(
								[this]()
								{ 
									return TRange<double>(0.0f,RecordingDuration.Get());
								})	
						.OnScrubPositionChanged_Lambda(
							[this](double NewScrubTime, bool bIsScrubbing)
									{
										if (bIsScrubbing)
										{
											OnScrubPositionChanged.ExecuteIfBound( NewScrubTime, bIsScrubbing );
										}
									})
				]
				+SVerticalBox::Slot().FillHeight(1.0f)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[				
						TimelinesView.ToSharedRef()
					]
					+SOverlay::Slot().HAlign(EHorizontalAlignment::HAlign_Right)
					[
						ScrollBar.ToSharedRef()
					]
					
				]
			]
		]
	];
}

bool FilterTrack(TSharedPtr<RewindDebugger::FRewindDebuggerTrack>& Track, const FString& FilterString, bool bRemoveNoData, bool bParentFilterPassed = false)
{
	const bool bStringFilterEmpty =  FilterString.IsEmpty();
	const bool bStringFilterPassed = bParentFilterPassed || bStringFilterEmpty || Track->GetDisplayName().ToString().Contains(FilterString);

	const bool bThisFilterPassed = (!bStringFilterEmpty && bStringFilterPassed);

	bool bAnyChildVisible = false;
	Track->IterateSubTracks([&bAnyChildVisible, bThisFilterPassed, FilterString, bRemoveNoData](TSharedPtr<RewindDebugger::FRewindDebuggerTrack> ChildTrack)
	{
		const bool bChildIsVisible = FilterTrack(ChildTrack, FilterString, bRemoveNoData, bThisFilterPassed);
		bAnyChildVisible |= bChildIsVisible;
	});

	bool bVisible = bAnyChildVisible || ((!bRemoveNoData || Track->HasDebugData()) && bStringFilterPassed);

	Track->SetIsVisible(bVisible);
	return bVisible;
}

void SRewindDebugger::RefreshDebugComponents()
{
	if (DebugComponents && DebugComponents->Num() > 0)
	{
		FilterTrack((*DebugComponents)[0], TrackFilterBox->GetText().ToString(), !ShouldDisplayEmptyTracks());
	}
	
	ComponentTreeView->Refresh();
	TimelinesView->Refresh();
}

TSharedRef<SWidget> SRewindDebugger::MakeMainMenu()
{
	return UToolMenus::Get()->GenerateWidget("RewindDebugger.MainMenu", FToolMenuContext());
}

TSharedRef<SWidget> SRewindDebugger::MakeFilterMenu()
{
	FMenuBuilder Builder(true, nullptr);
	Builder.AddWidget(TrackFilterBox.ToSharedRef(), FText(), true, false);
	Builder.AddSeparator();
	
	Builder.AddMenuEntry(
		LOCTEXT("DisplayEmptyTracks", "Show Empty Object Tracks"),
		LOCTEXT("DisplayEmptyTracksToolTip", "Show Object/Component tracks which have no sub tracks with any debug data"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateRaw(this, &SRewindDebugger::ToggleDisplayEmptyTracks),
			FCanExecuteAction(),
			FIsActionChecked::CreateRaw(this, &SRewindDebugger::ShouldDisplayEmptyTracks)),
		NAME_None,
		EUserInterfaceActionType::ToggleButton
	);
	
	return Builder.MakeWidget();
}

void SRewindDebugger::ComponentSelectionChanged(TSharedPtr<RewindDebugger::FRewindDebuggerTrack> SelectedItem, ESelectInfo::Type SelectInfo)
{
	SelectedComponent = SelectedItem;

	OnComponentSelectionChanged.ExecuteIfBound(SelectedItem);
}

TSharedPtr<SWidget> SRewindDebugger::OnContextMenuOpening()
{
	return BuildComponentContextMenu.Execute();
}

FReply SRewindDebugger::OnSelectActorClicked()
{
	FActorPickerModeModule& ActorPickerMode = FModuleManager::Get().GetModuleChecked<FActorPickerModeModule>("ActorPickerMode");
	
	// todo: force eject (from within BeginActorPickingMode?)

	ActorPickerMode.BeginActorPickingMode(
		FOnGetAllowedClasses(), 
		FOnShouldFilterActor(),
		FOnActorSelected::CreateRaw(this, &SRewindDebugger::SetDebugTargetActor));



	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
