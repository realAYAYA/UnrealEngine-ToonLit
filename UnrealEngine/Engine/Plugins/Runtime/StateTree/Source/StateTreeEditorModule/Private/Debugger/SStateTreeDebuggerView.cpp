// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_STATETREE_DEBUGGER

#include "SStateTreeDebuggerView.h"
#include "SStateTreeDebuggerViewRow.h"
#include "Debugger/StateTreeDebugger.h"
#include "DetailsViewArgs.h"
#include "Editor.h"
#include "Factories.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Kismet2/DebuggerCommands.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "ScopedTransaction.h"
#include "SStateTreeDebuggerInstanceTree.h"
#include "SStateTreeDebuggerTimelines.h"
#include "StateTree.h"
#include "StateTreeCompiler.h"
#include "StateTreeDebuggerCommands.h"
#include "StateTreeDebuggerTrack.h"
#include "StateTreeDelegates.h"
#include "StateTreeEditorData.h"
#include "StateTreeEditorSettings.h"
#include "StateTreeEditorStyle.h"
#include "StateTreeModule.h"
#include "StateTreeState.h"
#include "StateTreeViewModel.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"

#define LOCTEXT_NAMESPACE "StateTreeEditor"


namespace UE::StateTreeDebugger
{
/**
 * Iterates over all tree elements for the frame events
 * @param Elements Container of hierarchical tree element to visit
 * @param InFunc function called at each element, should return true if visiting is continued or false to stop.
 */
void VisitEventTreeElements(const TConstArrayView<TSharedPtr<FStateTreeDebuggerEventTreeElement>> Elements, TFunctionRef<bool(TSharedPtr<FStateTreeDebuggerEventTreeElement>& VisitedElement)> InFunc)
{
	TArray<TSharedPtr<FStateTreeDebuggerEventTreeElement>> Stack;
	bool bContinue = true;

	for (const TSharedPtr<FStateTreeDebuggerEventTreeElement>& RootElement : Elements)
	{
		if (RootElement == nullptr)
		{
			continue;
		}

		Stack.Add(RootElement);

		while (!Stack.IsEmpty() && bContinue)
		{
			TSharedPtr<FStateTreeDebuggerEventTreeElement> StackedElement = Stack[0];
			check(StackedElement);

			Stack.RemoveAt(0);

			bContinue = InFunc(StackedElement);

			if (bContinue)
			{
				for (const TSharedPtr<FStateTreeDebuggerEventTreeElement>& Child : StackedElement->Children)
				{
					if (Child.IsValid())
					{
						Stack.Add(Child);
					}
				}
			}
		}

		if (!bContinue)
		{
			break;
		}
	}
}

//----------------------------------------------------------------------//
// FTraceTextObjectFactory
//----------------------------------------------------------------------//
struct FTraceTextObjectFactory : FCustomizableTextObjectFactory
{
	UObject* NodeInstanceObject = nullptr;
	FTraceTextObjectFactory() : FCustomizableTextObjectFactory(GWarn) {}

protected:
	virtual bool CanCreateClass(UClass* ObjectClass, bool& bOmitSubObjs) const override
	{
		return true;
	}

	virtual void ProcessConstructedObject(UObject* CreatedObject) override
	{
		NodeInstanceObject = CreatedObject;
	}
};

/**
 * Prototype methode to try import struct/object from exported text.
 * Currently not used since it might fail if type is not found and for requires every property to be created
 * but some types (i.e. Object based) will simply set none as the imported value but for the debugger we still
 * want text to describe the dumped values.
 */
void GenerateElementForProperties(const TCHAR* TypeAsText, const TCHAR* ValueAsText, TSharedRef<FStateTreeDebuggerEventTreeElement>& ParentElement)
{
	const UStruct* StructTypeToVisit = nullptr;
	const void* StructValueToVisit = nullptr;

	// UStruct
	UScriptStruct* ScriptStruct = FindObject<UScriptStruct>(nullptr, TypeAsText, /*ExactClass*/false);
	if (ScriptStruct == nullptr)
	{
		ScriptStruct = LoadObject<UScriptStruct>(nullptr, TypeAsText);
	}

	FInstancedStruct NodeDataStruct;
	if (ScriptStruct != nullptr)
	{
		NodeDataStruct.InitializeAs(ScriptStruct);

		ScriptStruct->ImportText(ValueAsText, NodeDataStruct.GetMutableMemory(), /*OwnerObject*/nullptr, PPF_None, GLog, ScriptStruct->GetName());
		StructTypeToVisit = ScriptStruct;
		StructValueToVisit = NodeDataStruct.GetMemory();
	}

	// UObject
	UClass* Class = FindObject<UClass>(nullptr, TypeAsText, /*ExactClass*/false);
	if (Class == nullptr)
	{
		Class = LoadObject<UClass>(nullptr, TypeAsText);
	}

	TWeakObjectPtr<> NodeDataObject;
	if (Class != nullptr)
	{
		FTraceTextObjectFactory ObjectFactory;
		if (ObjectFactory.CanCreateObjectsFromText(ValueAsText))
		{
			ObjectFactory.ProcessBuffer(GetTransientPackage(), RF_Transactional, ValueAsText);
			if (ObjectFactory.NodeInstanceObject)
			{
				NodeDataObject = ObjectFactory.NodeInstanceObject;
				NodeDataObject->AddToRoot();

				StructTypeToVisit = ObjectFactory.NodeInstanceObject->GetClass();
				StructValueToVisit = ObjectFactory.NodeInstanceObject;
			}
		}
	}

	if (StructTypeToVisit != nullptr && StructValueToVisit != nullptr)
	{
		for (TPropertyValueIterator<const FProperty> PropertyIt(StructTypeToVisit, StructValueToVisit); PropertyIt; ++PropertyIt)
		{
			const FProperty* const Property = PropertyIt.Key();
			check(Property);
			const EStateTreePropertyUsage Usage = UE::StateTree::GetUsageFromMetaData(Property);

			// If the property is set to one of these usages, display it even if it is not edit on instance.
			// It is a common mistake to forget to set the "eye" on these properties it and wonder why it does not show up.
			const bool bShouldShowByUsage = Usage == EStateTreePropertyUsage::Input || Usage == EStateTreePropertyUsage::Output || Usage == EStateTreePropertyUsage::Context;
			const bool bIsEditable = Property->HasAllPropertyFlags(CPF_Edit) && !Property->HasAllPropertyFlags(CPF_DisableEditOnInstance);

			if (bShouldShowByUsage || bIsEditable)
			{
				FText Name = Property->GetDisplayNameText();
				FString ValueString;
				Property->ExportTextItem_Direct(ValueString, PropertyIt.Value(), nullptr, nullptr, PPF_None);

				// Create new property Log
				FStateTreeTracePropertyEvent PropertyEvent(/*RecordingWorldTime*/0, FString::Printf(TEXT("%s = %s"), *Name.ToString(), *ValueString));

				// Create Tree element to hold the event
				const TSharedPtr<FStateTreeDebuggerEventTreeElement> NewChildElement = MakeShareable(new FStateTreeDebuggerEventTreeElement(
						ParentElement->Frame,
						FStateTreeTraceEventVariantType(TInPlaceType<FStateTreeTracePropertyEvent>(), PropertyEvent),
						ParentElement->WeakStateTree.Get()));

				ParentElement->Children.Add(NewChildElement);
			}
		}
	}
}

} // UE::StateTreeDebugger

//----------------------------------------------------------------------//
// SStateTreeDebuggerView
//----------------------------------------------------------------------//
SStateTreeDebuggerView::SStateTreeDebuggerView()
{
	FEditorDelegates::BeginPIE.AddRaw(this, &SStateTreeDebuggerView::OnPIEStarted);
	FEditorDelegates::EndPIE.AddRaw(this, &SStateTreeDebuggerView::OnPIEStopped);
	FEditorDelegates::PausePIE.AddRaw(this, &SStateTreeDebuggerView::OnPIEPaused);
	FEditorDelegates::ResumePIE.AddRaw(this, &SStateTreeDebuggerView::OnPIEResumed);
	FEditorDelegates::SingleStepPIE.AddRaw(this, &SStateTreeDebuggerView::OnPIESingleStepped);
}

SStateTreeDebuggerView::~SStateTreeDebuggerView()
{
	UE::StateTree::Delegates::OnTracingStateChanged.RemoveAll(this);

	check(Debugger);
	Debugger->OnScrubStateChanged.Unbind();
	Debugger->OnBreakpointHit.Unbind();
	Debugger->OnNewSession.Unbind();
	Debugger->OnNewInstance.Unbind();
	Debugger->OnSelectedInstanceCleared.Unbind();

	FEditorDelegates::BeginPIE.RemoveAll(this);
	FEditorDelegates::EndPIE.RemoveAll(this);
	FEditorDelegates::PausePIE.RemoveAll(this);
	FEditorDelegates::ResumePIE.RemoveAll(this);
	FEditorDelegates::SingleStepPIE.RemoveAll(this);
}

void SStateTreeDebuggerView::StartRecording()
{
	if (CanStartRecording())
	{
		check(Debugger);
		Debugger->ClearSelection();

		// We give priority to the Editor actions even if an analysis was active (remote process)
		// This will stop current analysis and connect to the new live trace.
		bRecording = Debugger->RequestAnalysisOfEditorSession();
	}
}

void SStateTreeDebuggerView::StopRecording()
{
	if (CanStopRecording())
	{
		IStateTreeModule& StateTreeModule = FModuleManager::GetModuleChecked<IStateTreeModule>("StateTreeModule");
		StateTreeModule.StopTraces();
		
		bRecording = false;

		// Update max duration from current recording until track data gets reset
		check(Debugger);
		MaxTrackRecordingDuration = FMath::Max(MaxTrackRecordingDuration, Debugger->GetRecordingDuration());

		// Mark all tracks from the stopped session as stale to have different look.
		for (const TSharedPtr<RewindDebugger::FRewindDebuggerTrack>& DebugTrack : InstanceOwnerTracks)
		{
			if (FStateTreeDebuggerBaseTrack* StateTreeTrack = static_cast<FStateTreeDebuggerBaseTrack*>(DebugTrack.Get()))
			{
				StateTreeTrack->MarkAsStale();
			}
		}
	}
}

bool SStateTreeDebuggerView::CanResumeDebuggerAnalysis() const
{
	check(Debugger);
	return Debugger->IsAnalysisSessionPaused() && Debugger->HasHitBreakpoint(); 
}

void SStateTreeDebuggerView::ResumeDebuggerAnalysis() const
{
	check(Debugger);
	if (Debugger->IsAnalysisSessionPaused())
	{
		Debugger->ResumeSessionAnalysis();
	}
}

bool SStateTreeDebuggerView::CanResetTracks() const
{
	return !InstanceOwnerTracks.IsEmpty();
}

void SStateTreeDebuggerView::ResetTracks()
{
	InstanceOwnerTracks.Reset();

	check(Debugger);
	Debugger->ResetEventCollections();

	MaxTrackRecordingDuration = 0;

	// Refresh tree view
	if (InstancesTreeView)
	{
		InstancesTreeView->Refresh();
	}

	if (InstanceTimelinesTreeView)
	{
		InstanceTimelinesTreeView->Refresh();
	}
}

void SStateTreeDebuggerView::OnPIEStarted(const bool bIsSimulating)
{
	if (UStateTreeEditorSettings::Get().bShouldDebuggerAutoRecordOnPIE)
	{
		StartRecording();
	}
}

void SStateTreeDebuggerView::OnPIEStopped(const bool bIsSimulating)
{
	StopRecording();

	check(Debugger);
	Debugger->StopSessionAnalysis();
}

void SStateTreeDebuggerView::OnPIEPaused(const bool bIsSimulating) const
{
	check(Debugger);
	Debugger->PauseSessionAnalysis();
}

void SStateTreeDebuggerView::OnPIEResumed(const bool bIsSimulating) const
{
	check(Debugger);
	Debugger->ResumeSessionAnalysis();
}

void SStateTreeDebuggerView::OnPIESingleStepped(bool bSimulating) const
{
	check(Debugger);
	Debugger->SyncToCurrentSessionDuration();
}

void SStateTreeDebuggerView::Construct(const FArguments& InArgs, const UStateTree& InStateTree, const TSharedRef<FStateTreeViewModel>& InStateTreeViewModel, const TSharedRef<FUICommandList>& InCommandList)
{
	StateTreeViewModel = InStateTreeViewModel;
	StateTree = &InStateTree;
	StateTreeEditorData = Cast<UStateTreeEditorData>(InStateTree.EditorData.Get());
	CommandList = InCommandList;

	Debugger = InStateTreeViewModel->GetDebugger();
	check(Debugger);

	IStateTreeModule& StateTreeModule = FModuleManager::GetModuleChecked<IStateTreeModule>("StateTreeModule");
	bRecording = StateTreeModule.IsTracing();
	UE::StateTree::Delegates::OnTracingStateChanged.AddSPLambda(this, [&bRecording=bRecording](const bool bTracesEnabled)
		{
			bRecording = bTracesEnabled;
		});

	// Bind callbacks to the debugger delegates
	Debugger->OnNewSession.BindSP(this, &SStateTreeDebuggerView::OnNewSession);
	Debugger->OnNewInstance.BindSP(this, &SStateTreeDebuggerView::OnNewInstance);
	Debugger->OnScrubStateChanged.BindSP(this, &SStateTreeDebuggerView::OnDebuggerScrubStateChanged);
	Debugger->OnBreakpointHit.BindSP(this, &SStateTreeDebuggerView::OnBreakpointHit, InCommandList);
	Debugger->OnSelectedInstanceCleared.BindSP(this, &SStateTreeDebuggerView::OnSelectedInstanceCleared);

	// Bind our scrub time attribute to follow the value computed by the debugger
	ScrubTimeAttribute = TAttribute<double>(InStateTreeViewModel->GetDebugger(), &FStateTreeDebugger::GetScrubTime);

	// Put debugger in proper simulation state when view is constructed after PIE/SIE was started
	if (FPlayWorldCommandCallbacks::HasPlayWorldAndPaused())
	{
		Debugger->PauseSessionAnalysis();
	}

	// Add & Bind commands
	BindDebuggerToolbarCommands(InCommandList);

	// Register the play world commands
	InCommandList->Append(FPlayWorldCommands::GlobalPlayWorldActions.ToSharedRef());

	// Debug commands
	InCommandList->MapAction(
		FStateTreeDebuggerCommands::Get().EnableOnEnterStateBreakpoint,
		FExecuteAction::CreateLambda([this] { HandleEnableStateBreakpoint(EStateTreeBreakpointType::OnEnter); }),
		FCanExecuteAction(),
		FGetActionCheckState::CreateLambda([this]{ return GetStateBreakpointCheckState(EStateTreeBreakpointType::OnEnter); }),
		FIsActionButtonVisible::CreateLambda([this] { return CanAddStateBreakpoint(EStateTreeBreakpointType::OnEnter) || CanRemoveStateBreakpoint(EStateTreeBreakpointType::OnEnter); }));

	InCommandList->MapAction(
		FStateTreeDebuggerCommands::Get().EnableOnExitStateBreakpoint,
		FExecuteAction::CreateLambda([this] { HandleEnableStateBreakpoint(EStateTreeBreakpointType::OnExit); }),
		FCanExecuteAction(),
		FGetActionCheckState::CreateLambda([this] { return GetStateBreakpointCheckState(EStateTreeBreakpointType::OnExit); }),
		FIsActionButtonVisible::CreateLambda([this] { return CanAddStateBreakpoint(EStateTreeBreakpointType::OnExit) || CanRemoveStateBreakpoint(EStateTreeBreakpointType::OnExit); }));

	// Toolbars
	FSlimHorizontalToolBarBuilder LeftToolbar(InCommandList, FMultiBoxCustomization::None, /*InExtender*/ nullptr, /*InForceSmallIcons*/ true);
	LeftToolbar.BeginSection(TEXT("Debugging"));
	{
		LeftToolbar.BeginStyleOverride(FName("Toolbar.BackplateLeft"));
		const FPlayWorldCommands& PlayWorldCommands = FPlayWorldCommands::Get();
		LeftToolbar.AddToolBarButton(PlayWorldCommands.RepeatLastPlay);
		LeftToolbar.AddToolBarButton(PlayWorldCommands.PausePlaySession,
					NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FAppStyle::GetAppStyleSetName(), "PlayWorld.PausePlaySession.Small"));
		LeftToolbar.AddToolBarButton(PlayWorldCommands.ResumePlaySession,
					NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FAppStyle::GetAppStyleSetName(), "PlayWorld.ResumePlaySession.Small"));
		LeftToolbar.BeginStyleOverride(FName("Toolbar.BackplateRight"));
		LeftToolbar.AddToolBarButton(PlayWorldCommands.StopPlaySession,
					NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FAppStyle::GetAppStyleSetName(), "PlayWorld.StopPlaySession.Small"));
		LeftToolbar.EndStyleOverride();

		LeftToolbar.AddSeparator();

		const FStateTreeDebuggerCommands& DebuggerCommands = FStateTreeDebuggerCommands::Get();
		LeftToolbar.AddToolBarButton(DebuggerCommands.StartRecording);
		LeftToolbar.AddToolBarButton(DebuggerCommands.StopRecording);

		LeftToolbar.AddSeparator();

		LeftToolbar.AddToolBarButton(DebuggerCommands.ResumeDebuggerAnalysis);

		LeftToolbar.AddToolBarButton(DebuggerCommands.PreviousFrameWithStateChange);
		LeftToolbar.AddToolBarButton(DebuggerCommands.PreviousFrameWithEvents);
		LeftToolbar.AddToolBarButton(DebuggerCommands.NextFrameWithEvents);
		LeftToolbar.AddToolBarButton(DebuggerCommands.NextFrameWithStateChange);

		LeftToolbar.AddToolBarButton(DebuggerCommands.ResetTracks);
	}
	LeftToolbar.EndSection();

	FSlimHorizontalToolBarBuilder RightToolbar(nullptr, FMultiBoxCustomization::None);
	RightToolbar.BeginSection("Auto-Scroll");
	{
		FUIAction AutoScrollToggleButtonAction;
		AutoScrollToggleButtonAction.GetActionCheckState.BindSPLambda(this, [&bAutoScroll=bAutoScroll]
		{
			return bAutoScroll ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		});
		AutoScrollToggleButtonAction.ExecuteAction.BindSPLambda(this, [&bAutoScroll=bAutoScroll]
		{
			bAutoScroll = !bAutoScroll;
		});

		RightToolbar.AddToolBarButton
		(
			AutoScrollToggleButtonAction,
			NAME_None,
			TAttribute<FText>(),
			LOCTEXT("AutoScrollToolTip", "Auto-Scroll"),
			FSlateIcon(FStateTreeEditorStyle::Get().GetStyleSetName(), "StateTreeEditor.AutoScroll"),
			EUserInterfaceActionType::ToggleButton);
	}
	RightToolbar.EndSection();

	// Place holder toolbar for now but the intent to add more functionalities (e.g. Search)
	FSlimHorizontalToolBarBuilder FrameDetailsToolbar(nullptr, FMultiBoxCustomization::None);
	FrameDetailsToolbar.BeginSection("FrameDetails");
	{
		FrameDetailsToolbar.AddWidget(SNew(STextBlock).Text(FText::FromString(GetNameSafe(StateTree.Get()))));
	}
	FrameDetailsToolbar.EndSection();

	// Trace selection combo
	const TSharedRef<SWidget> TraceSelectionBox = SNew(SComboButton)
		.OnGetMenuContent(this, &SStateTreeDebuggerView::OnGetDebuggerTracesMenu)
		.ButtonContent()
		[
			SNew(STextBlock)
			.ToolTipText(LOCTEXT("SelectTraceSession", "Pick trace session to debug"))
			.Text_Lambda([Debugger = Debugger]()
			{
				check(Debugger);
				return Debugger->GetSelectedTraceDescription();
			})
		];

	const TSharedPtr<SScrollBar> ScrollBar = SNew(SScrollBar); 
	
	// Instances TreeView
	InstancesTreeView =	SNew(SStateTreeDebuggerInstanceTree)
		.ExternalScrollBar(ScrollBar)
		.OnExpansionChanged_Lambda([this]() { InstanceTimelinesTreeView->RestoreExpansion(); })
		.OnScrolled_Lambda([this](double ScrollOffset)
		{
			InstanceTimelinesTreeView->ScrollTo(ScrollOffset);
		})
		.InstanceTracks(&InstanceOwnerTracks)
		.OnSelectionChanged_Lambda([this](TSharedPtr<RewindDebugger::FRewindDebuggerTrack> SelectedItem, ESelectInfo::Type SelectInfo)
			{
				InstanceTimelinesTreeView->SetSelection(SelectedItem);

				if (FStateTreeDebuggerBaseTrack* StateTreeTrack = static_cast<FStateTreeDebuggerBaseTrack*>(SelectedItem.Get()))
				{
					StateTreeTrack->OnSelected();
				}
			});
	
	// Timelines TreeView
	InstanceTimelinesTreeView = SNew(SStateTreeDebuggerTimelines)
		.ExternalScrollbar(ScrollBar)
		.OnExpansionChanged_Lambda([this]() { InstancesTreeView->RestoreExpansion(); })
		.OnScrolled_Lambda([this](double ScrollOffset) { InstancesTreeView->ScrollTo(ScrollOffset); })
		.InstanceTracks(&InstanceOwnerTracks)
		.ViewRange_Lambda([this]() { return ViewRange; })
		.ClampRange_Lambda([this]() { return TRange<double>(0, MaxTrackRecordingDuration); })
		.OnViewRangeChanged_Lambda([this](TRange<double> NewRange) { ViewRange = NewRange; })
		.ScrubPosition(ScrubTimeAttribute)
		.OnScrubPositionChanged_Lambda([this](double NewScrubTime, bool bIsScrubbing) { OnTimeLineScrubPositionChanged(NewScrubTime, bIsScrubbing); });

	// EventsTreeView
	EventsTreeView = SNew(STreeView<TSharedPtr<FStateTreeDebuggerEventTreeElement>>)
			.OnGenerateRow_Lambda([this](const TSharedPtr<FStateTreeDebuggerEventTreeElement>& InElement, const TSharedRef<STableViewBase>& InOwnerTableView)
			{
				return SNew(SStateTreeDebuggerViewRow, InOwnerTableView, InElement);
			})
			.OnGetChildren_Lambda([](const TSharedPtr<const FStateTreeDebuggerEventTreeElement>& InParent, TArray<TSharedPtr<FStateTreeDebuggerEventTreeElement>>& OutChildren)
			{
				if (const FStateTreeDebuggerEventTreeElement* Parent = InParent.Get())
				{
					OutChildren.Append(Parent->Children);
				}
			})
		.TreeItemsSource(&EventsTreeElements)
		.ItemHeight(32)
		.AllowOverscroll(EAllowOverscroll::No);

	ChildSlot
	[
		SNew(SBorder)
		.Padding(4.0f)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			[
				SNew(SSplitter)
				.Orientation(Orient_Horizontal)
				+ SSplitter::Slot()
				.MinSize(600)
				[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						.HAlign(HAlign_Fill)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.HAlign(HAlign_Left)
							.FillWidth(1.0f)
							[
								LeftToolbar.MakeWidget()
							]
							+ SHorizontalBox::Slot()
							.HAlign(HAlign_Right)
							.AutoWidth()
							[
								RightToolbar.MakeWidget()
							]
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SAssignNew(HeaderSplitter, SSplitter)
							.Orientation(Orient_Horizontal)
							+ SSplitter::Slot()
							.Value(0.2f)
							.MinSize(350)
							.Resizable(false)
							[
								TraceSelectionBox
							]
							+ SSplitter::Slot()
							.Resizable(false)
							[
								SNew(SSimpleTimeSlider)
								.DesiredSize({100, 24})
								.ClampRangeHighlightSize(0.15f)
								.ClampRangeHighlightColor(FLinearColor::Red.CopyWithNewOpacity(0.5f))
								.ScrubPosition(ScrubTimeAttribute)
								.ViewRange_Lambda([this]() { return ViewRange; })
								.OnViewRangeChanged_Lambda([this](TRange<double> NewRange) { ViewRange = NewRange; })
								.ClampRange_Lambda([this]() { return TRange<double>(0, MaxTrackRecordingDuration); })
								.OnScrubPositionChanged_Lambda([this](double NewScrubTime, bool bIsScrubbing) { OnTimeLineScrubPositionChanged(NewScrubTime, bIsScrubbing); })
							]
						]
						+ SVerticalBox::Slot()
						.FillHeight(1.0f)
						[
							SAssignNew(TreeViewsSplitter, SSplitter)
							.Orientation(Orient_Horizontal)
							+ SSplitter::Slot()
							.Value(0.2f)
							.MinSize(350)
							.OnSlotResized_Lambda([this](float Size)
								{
									// Sync both header and content
									TreeViewsSplitter->SlotAt(0).SetSizeValue(Size);
									HeaderSplitter->SlotAt(0).SetSizeValue(Size);
								})
							[
								SNew(SScrollBox)
								.Orientation(Orient_Horizontal)
								+ SScrollBox::Slot()
								.FillSize(1.0f)
								[
									InstancesTreeView.ToSharedRef()
								]
							]
							+ SSplitter::Slot()
							.OnSlotResized_Lambda([this](float Size)
								{
									TreeViewsSplitter->SlotAt(1).SetSizeValue(Size);
									HeaderSplitter->SlotAt(1).SetSizeValue(Size);
								})
							[
								SNew(SOverlay)
								+ SOverlay::Slot()
								[
									InstanceTimelinesTreeView.ToSharedRef()
								]
								+ SOverlay::Slot().HAlign(EHorizontalAlignment::HAlign_Right)
								[
									ScrollBar.ToSharedRef()
								]
							]
						]
				]
				+ SSplitter::Slot()
				.MinSize(400)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Fill)
					[
						FrameDetailsToolbar.MakeWidget()
					]
					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					[
						SNew(SScrollBox)
						.Orientation(Orient_Horizontal)
						+ SScrollBox::Slot()
						.FillSize(1.0f)
						[
							EventsTreeView.ToSharedRef()
						]
					]
				]
			]
		]
	];
	
	// Auto-select session if there is only one available and that we are trac
	// Do that after creating all our widgets in case we receive a callback
	TArray<FStateTreeDebugger::FTraceDescriptor> TraceDescriptors;
	Debugger->GetLiveTraces(TraceDescriptors);
	if (TraceDescriptors.Num() == 1 && bRecording)
	{
		Debugger->RequestSessionAnalysis(TraceDescriptors.Last());
	}
}

FReply SStateTreeDebuggerView::OnPreviewKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	// We consider the key as handled regardless if the action can be executed or not since we don't want
	// some of them affecting other widgets once the action can no longer be executed
	// (e.g. can no longer scrub once reaching beginning or end of the timeline)
	// This is why we test the input manually instead of relying on ProcessCommandBindings.
	const FInputChord InputChord(
		InKeyEvent.GetKey(),
		EModifierKey::FromBools(InKeyEvent.IsControlDown(), InKeyEvent.IsAltDown(), InKeyEvent.IsShiftDown(), InKeyEvent.IsCommandDown()));

	const TSharedPtr<FUICommandInfo> CommandInfo = FInputBindingManager::Get().FindCommandInContext(
		FStateTreeDebuggerCommands::Get().GetContextName(), InputChord, /*bCheckDefault*/false);

	if (CommandInfo.IsValid())
	{
		CommandList->TryExecuteAction(CommandInfo.ToSharedRef());
		return FReply::Handled();
	}

	return SCompoundWidget::OnPreviewKeyDown(MyGeometry, InKeyEvent);
}

void SStateTreeDebuggerView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_StateTreeDebuggerView_TickView);

	check(Debugger);

	const double RecordingDuration = Debugger->GetRecordingDuration();
	const bool bHasMoreRecentData = LastUpdatedTrackRecordingDuration != RecordingDuration;
	LastUpdatedTrackRecordingDuration = RecordingDuration;
	MaxTrackRecordingDuration = FMath::Max(MaxTrackRecordingDuration, RecordingDuration);
	
	if ((Debugger->IsAnalysisSessionActive() && !Debugger->IsAnalysisSessionPaused())
		|| bHasMoreRecentData)
	{
		if (bAutoScroll)
		{
			// Stick to most recent data if auto scroll is enabled.
			// Autoscroll is disabled when paused.
			// This allows the user to pause the analysis, inspect the data, and continue and the autoscroll will catch up with latest.
			// Complementary logic in OnTimeLineScrubPositionChanged().
			Debugger->SetScrubTime(Debugger->GetRecordingDuration());
		}
		else
		{
			// Set scrub time to self to request update the UI.
			Debugger->SetScrubTime(Debugger->GetScrubTime());
		}
	}
	
	RefreshTracks();
}

void SStateTreeDebuggerView::RefreshTracks()
{
	bool bChanged = false;
	for (const TSharedPtr<RewindDebugger::FRewindDebuggerTrack>& DebugTrack : InstanceOwnerTracks)
	{
		bChanged = DebugTrack->Update() || bChanged;
	}

	if (bChanged)
	{
		InstancesTreeView->Refresh();
		InstanceTimelinesTreeView->Refresh();
		TrackCursor();
	}
}

void SStateTreeDebuggerView::BindDebuggerToolbarCommands(const TSharedRef<FUICommandList>& ToolkitCommands)
{
	const FStateTreeDebuggerCommands& Commands = FStateTreeDebuggerCommands::Get();

	ToolkitCommands->MapAction(
		Commands.StartRecording,
		FExecuteAction::CreateSP(this, &SStateTreeDebuggerView::StartRecording),
		FCanExecuteAction::CreateSP(this, &SStateTreeDebuggerView::CanStartRecording),
		FIsActionChecked(),
		FIsActionButtonVisible::CreateLambda([this]() { return !IsRecording();}));

	ToolkitCommands->MapAction(
		Commands.StopRecording,
		FExecuteAction::CreateSP(this, &SStateTreeDebuggerView::StopRecording),
		FCanExecuteAction::CreateSP(this, &SStateTreeDebuggerView::CanStopRecording),
		FIsActionChecked(),
		FIsActionButtonVisible::CreateRaw(this, &SStateTreeDebuggerView::CanStopRecording));
	
	ToolkitCommands->MapAction(
		Commands.PreviousFrameWithStateChange,
		FExecuteAction::CreateSP(this, &SStateTreeDebuggerView::StepBackToPreviousStateChange),
		FCanExecuteAction::CreateSP(this, &SStateTreeDebuggerView::CanStepBackToPreviousStateChange));

	ToolkitCommands->MapAction(
		Commands.PreviousFrameWithEvents,
		FExecuteAction::CreateSP(this, &SStateTreeDebuggerView::StepBackToPreviousStateWithEvents),
		FCanExecuteAction::CreateSP(this, &SStateTreeDebuggerView::CanStepBackToPreviousStateWithEvents));

	ToolkitCommands->MapAction(
		Commands.NextFrameWithEvents,
		FExecuteAction::CreateSP(this, &SStateTreeDebuggerView::StepForwardToNextStateWithEvents),
		FCanExecuteAction::CreateSP(this, &SStateTreeDebuggerView::CanStepForwardToNextStateWithEvents));

	ToolkitCommands->MapAction(
		Commands.NextFrameWithStateChange,
		FExecuteAction::CreateSP(this, &SStateTreeDebuggerView::StepForwardToNextStateChange),
		FCanExecuteAction::CreateSP(this, &SStateTreeDebuggerView::CanStepForwardToNextStateChange));

	ToolkitCommands->MapAction(
		Commands.ResumeDebuggerAnalysis,
		FExecuteAction::CreateSP(this, &SStateTreeDebuggerView::ResumeDebuggerAnalysis),
		FCanExecuteAction::CreateSP(this, &SStateTreeDebuggerView::CanResumeDebuggerAnalysis));

	ToolkitCommands->MapAction(
		Commands.ResetTracks,
		FExecuteAction::CreateSP(this, &SStateTreeDebuggerView::ResetTracks),
		FCanExecuteAction::CreateSP(this, &SStateTreeDebuggerView::CanResetTracks));
}

bool SStateTreeDebuggerView::CanUseScrubButtons() const
{
	// Nothing preventing use of scrub buttons on the Editor side at the moment.
	return true;
}

bool SStateTreeDebuggerView::CanStepBackToPreviousStateWithEvents() const
{
	check(Debugger);
	return CanUseScrubButtons() && Debugger->CanStepBackToPreviousStateWithEvents();
}

void SStateTreeDebuggerView::StepBackToPreviousStateWithEvents()
{
	check(Debugger);
	Debugger->StepBackToPreviousStateWithEvents();
	bAutoScroll = false;
}

bool SStateTreeDebuggerView::CanStepForwardToNextStateWithEvents() const
{
	check(Debugger);
	return CanUseScrubButtons() && Debugger->CanStepForwardToNextStateWithEvents();
}

void SStateTreeDebuggerView::StepForwardToNextStateWithEvents()
{
	check(Debugger);
	Debugger->StepForwardToNextStateWithEvents();
	bAutoScroll = false;
}

bool SStateTreeDebuggerView::CanStepBackToPreviousStateChange() const
{
	check(Debugger);
	return CanUseScrubButtons() && Debugger->CanStepBackToPreviousStateChange();
}

void SStateTreeDebuggerView::StepBackToPreviousStateChange()
{
	check(Debugger);
	Debugger->StepBackToPreviousStateChange();
	bAutoScroll = false;
}

bool SStateTreeDebuggerView::CanStepForwardToNextStateChange() const
{
	check(Debugger);
	return CanUseScrubButtons() && Debugger->CanStepForwardToNextStateChange();
}

void SStateTreeDebuggerView::StepForwardToNextStateChange()
{
	check(Debugger);
	Debugger->StepForwardToNextStateChange();
	bAutoScroll = false;
}

bool SStateTreeDebuggerView::CanAddStateBreakpoint(const EStateTreeBreakpointType Type) const
{
	check(StateTreeViewModel);

	TArray<UStateTreeState*> SelectedStates;
	StateTreeViewModel->GetSelectedStates(SelectedStates);
	if (SelectedStates.IsEmpty())
	{
		return false;
	}

	const UStateTreeEditorData* EditorData = StateTreeEditorData.Get();
	if (!ensure(EditorData != nullptr))
	{
		return false;
	}

	for (const UStateTreeState* SelectedState : SelectedStates)
	{
		if (SelectedState != nullptr)
		{
			if (EditorData->HasBreakpoint(SelectedState->ID, Type) == false)
			{
				return true;
			}
		}
	}

	return false;
}

bool SStateTreeDebuggerView::CanRemoveStateBreakpoint(const EStateTreeBreakpointType Type) const
{
	check(StateTreeViewModel);

	TArray<UStateTreeState*> SelectedStates;
	StateTreeViewModel->GetSelectedStates(SelectedStates);
	if (SelectedStates.IsEmpty())
	{
		return false;
	}

	const UStateTreeEditorData* EditorData = StateTreeEditorData.Get();
	if (!ensure(EditorData != nullptr))
	{
		return false;
	}

	for (const UStateTreeState* SelectedState : SelectedStates)
	{
		if (SelectedState != nullptr)
		{
			if (EditorData->HasBreakpoint(SelectedState->ID, Type))
			{
				return true;
			}
		}
	}

	return false;
}


ECheckBoxState SStateTreeDebuggerView::GetStateBreakpointCheckState(const EStateTreeBreakpointType Type) const
{
	const bool bCanAdd = CanAddStateBreakpoint(Type);
	const bool bCanRemove = CanRemoveStateBreakpoint(Type);
	if (bCanAdd && bCanRemove)
	{
		return ECheckBoxState::Undetermined;
	}

	if (bCanRemove)
	{
		return ECheckBoxState::Checked;
	}

	if (bCanAdd)
	{
		return ECheckBoxState::Unchecked;
	}

	// Should not happen since action is not visible in this case
	return ECheckBoxState::Undetermined;
}

void SStateTreeDebuggerView::HandleEnableStateBreakpoint(EStateTreeBreakpointType Type)
{
	check(StateTreeViewModel);

	TArray<UStateTreeState*> SelectedStates;
	StateTreeViewModel->GetSelectedStates(SelectedStates);
	if (SelectedStates.IsEmpty())
	{
		return;
	}

	UStateTreeEditorData* EditorData = StateTreeEditorData.Get();
	if (!ensure(EditorData != nullptr))
	{
		return;
	}

	TBitArray<> HasBreakpoint;
	HasBreakpoint.Reserve(SelectedStates.Num());
	for (const UStateTreeState* SelectedState : SelectedStates)
	{
		HasBreakpoint.Add(SelectedState != nullptr && EditorData->HasBreakpoint(SelectedState->ID, Type));
	}

	check(HasBreakpoint.Num() == SelectedStates.Num());

	// Process CanAdd first so in case of undetermined state (mixed selection) we add by default. 
	if (CanAddStateBreakpoint(Type))
	{
		const FScopedTransaction Transaction(LOCTEXT("AddStateBreakpoint", "Add State Breakpoint(s)"));
		EditorData->Modify();
		for (int Index = 0; Index < SelectedStates.Num(); ++Index)
		{
			const UStateTreeState* SelectedState = SelectedStates[Index];
			if (HasBreakpoint[Index] == false && SelectedState != nullptr)
			{
				EditorData->AddBreakpoint(SelectedState->ID, Type);	
			}
		}
	}
	else if (CanRemoveStateBreakpoint(Type))
	{
		const FScopedTransaction Transaction(LOCTEXT("RemoveStateBreakpoint", "Remove State Breakpoint(s)"));
		EditorData->Modify();
		for (int Index = 0; Index < SelectedStates.Num(); ++Index)
		{
			const UStateTreeState* SelectedState = SelectedStates[Index];
			if (HasBreakpoint[Index] && SelectedState != nullptr)
			{
				EditorData->RemoveBreakpoint(SelectedState->ID, Type);	
			}
		}
	}
}

UStateTreeState* SStateTreeDebuggerView::FindStateAssociatedToBreakpoint(FStateTreeDebuggerBreakpoint Breakpoint) const
{
	const UStateTree* Tree = StateTree.Get();
	UStateTreeEditorData* TreeEditorData = StateTreeEditorData.Get();
	if (Tree == nullptr || TreeEditorData == nullptr)
	{
		return nullptr;
	}

	UStateTreeState* StateTreeState = nullptr;

	if (const FStateTreeStateHandle* StateHandle = Breakpoint.ElementIdentifier.TryGet<FStateTreeStateHandle>())
	{
		const FGuid StateId = StateTree->GetStateIdFromHandle(*StateHandle);
		StateTreeState = TreeEditorData->GetMutableStateByID(StateId);
	}
	else if (const FStateTreeDebuggerBreakpoint::FStateTreeTaskIndex* TaskIndex = Breakpoint.ElementIdentifier.TryGet<FStateTreeDebuggerBreakpoint::FStateTreeTaskIndex>())
	{
		const FGuid TaskId = StateTree->GetNodeIdFromIndex(TaskIndex->Index);

		TreeEditorData->VisitHierarchy([&TaskId, &StateTreeState](UStateTreeState& State, UStateTreeState* /*ParentState*/)
			{
				for (const FStateTreeEditorNode& EditorNode : State.Tasks)
				{
					if (EditorNode.ID == TaskId)
					{
						StateTreeState = &State;
						return EStateTreeVisitor::Break;
					}
				}
				return EStateTreeVisitor::Continue;
			});
	}
	else if (const FStateTreeDebuggerBreakpoint::FStateTreeTransitionIndex* TransitionIndex = Breakpoint.ElementIdentifier.TryGet<FStateTreeDebuggerBreakpoint::FStateTreeTransitionIndex>())
	{
		const FGuid TransitionId = StateTree->GetTransitionIdFromIndex(TransitionIndex->Index);

		TreeEditorData->VisitHierarchy([&TransitionId, &StateTreeState](UStateTreeState& State, UStateTreeState* /*ParentState*/)
			{
				for (const FStateTreeTransition& StateTransition : State.Transitions)
				{
					if (StateTransition.ID == TransitionId)
					{
						StateTreeState = &State;
						return EStateTreeVisitor::Break;
					}
				}
				return EStateTreeVisitor::Continue;
			});
	}

	return StateTreeState;
}

void SStateTreeDebuggerView::OnTimeLineScrubPositionChanged(double Time, bool bIsScrubbing)
{
	check(Debugger);
	// Disable auto scroll when scrubbing.
	// But, do not disable it if the analysis is in progress but paused.
	// This allows the user to pause the analysis, inspect the data, and continue and the autoscroll will catch up with latest.
	// Complementary logic in Tick().
	if (Debugger->IsAnalysisSessionActive() && !Debugger->IsAnalysisSessionPaused())
	{
		bAutoScroll = false;
	}
	Debugger->SetScrubTime(Time);
}

void SStateTreeDebuggerView::OnDebuggerScrubStateChanged(const UE::StateTreeDebugger::FScrubState& ScrubState)
{
	TrackCursor();

	// Rebuild frame details from the events of that frame
	EventsTreeElements.Reset();
	ON_SCOPE_EXIT
	{
		EventsTreeView->ClearExpandedItems();
		ExpandAll(EventsTreeElements);
		EventsTreeView->RequestTreeRefresh();
	};

	const UE::StateTreeDebugger::FInstanceEventCollection& EventCollection = ScrubState.GetEventCollection();
	const TConstArrayView<const FStateTreeTraceEventVariantType> Events = EventCollection.Events;

	if (Events.IsEmpty() || ScrubState.IsInBounds() == false)
	{
		return;
	}

	const TConstArrayView<UE::StateTreeDebugger::FFrameSpan> Spans = EventCollection.FrameSpans;
	check(Spans.Num());
	check(StateTree.IsValid());

	TArray<TSharedPtr<FStateTreeDebuggerEventTreeElement>, TInlineAllocator<8>> ScopeStack;

	const int32 SpanIdx = ScrubState.GetFrameSpanIndex();
	const int32 FirstEventIdx = Spans[SpanIdx].EventIdx;
	const TraceServices::FFrame Frame = Spans[SpanIdx].Frame;
	const int32 MaxEventIdx = Spans.IsValidIndex(SpanIdx+1) ? Spans[SpanIdx+1].EventIdx : Events.Num();

	const UStateTree* const RootTree = StateTree.Get();
	const UStateTree* ActiveTree = RootTree;
	
	for (int32 EventIdx = FirstEventIdx; EventIdx < MaxEventIdx; EventIdx++)
	{
		const FStateTreeTraceEventVariantType& Event = Events[EventIdx];
		FString CustomDescription;
		bool bShouldAddToScopeStack = false;
		bool bShouldPopScopeStack = false;

		if (const FStateTreeTraceStateEvent* StateEvent = Event.TryGet<FStateTreeTraceStateEvent>())
		{
			if (StateEvent->EventType == EStateTreeTraceEventType::OnEntering
			 	|| StateEvent->EventType == EStateTreeTraceEventType::OnExiting)
			{
				bShouldAddToScopeStack = true;
			}
			else if (StateEvent->EventType == EStateTreeTraceEventType::OnEntered
				|| StateEvent->EventType == EStateTreeTraceEventType::OnExited)
			{
				bShouldPopScopeStack = true;
			}
		}
		else if (const FStateTreeTracePhaseEvent* PhaseEvent = Event.TryGet<FStateTreeTracePhaseEvent>())
		{
			if (PhaseEvent->EventType == EStateTreeTraceEventType::Push)
			{
				bShouldAddToScopeStack = true;
			}
			else if (PhaseEvent->EventType == EStateTreeTraceEventType::Pop)
			{
				bShouldPopScopeStack = true;
			}
		}
		else if (const FStateTreeTraceInstanceFrameEvent* FrameEvent = Event.TryGet<FStateTreeTraceInstanceFrameEvent>())
		{
			ActiveTree = FrameEvent->WeakStateTree.Get();
			check(ActiveTree);

			// We don't want to create an entry.
			continue;
		}

		if (bShouldPopScopeStack)
		{
			// Pop scope and remove associated element if empty
			TSharedPtr<FStateTreeDebuggerEventTreeElement> Scope = ScopeStack.Pop();
			if (Scope->Children.IsEmpty())
			{
				TArray<TSharedPtr<FStateTreeDebuggerEventTreeElement>>& TreeElements = ScopeStack.IsEmpty() ? EventsTreeElements : ScopeStack.Top()->Children;
				TreeElements.Remove(Scope);
			}
			// We don't want to create a child when a scope is popped.
			continue;
		}

		const TSharedRef<FStateTreeDebuggerEventTreeElement> NewElement = MakeShareable(new FStateTreeDebuggerEventTreeElement(Frame, Event, ActiveTree));
		NewElement->Description = CustomDescription;

		TArray<TSharedPtr<FStateTreeDebuggerEventTreeElement>>& TreeElements = ScopeStack.IsEmpty() ? EventsTreeElements : ScopeStack.Top()->Children;
		const TSharedPtr<FStateTreeDebuggerEventTreeElement>& ElementPtr = TreeElements.Add_GetRef(NewElement);

		if (bShouldAddToScopeStack)
		{
			ScopeStack.Push(ElementPtr);
		}

		GenerateElementsForProperties(Event, NewElement);
	}
}

void SStateTreeDebuggerView::GenerateElementsForProperties(const FStateTreeTraceEventVariantType& Event, const TSharedRef<FStateTreeDebuggerEventTreeElement>& ParentElement)
{
	FString TypePath;
	FString InstanceDataAsString;

	Visit([&TypePath, &InstanceDataAsString](auto& TypedEvent)
		{
			TypePath = TypedEvent.GetDataTypePath();
			InstanceDataAsString = TypedEvent.GetDataAsText();
		}, Event);

	if (!InstanceDataAsString.IsEmpty())
	{
		auto CreatePropertyElement = [ParentElement](const FStringView Line, const int32 NestedCount = 0)
			{
				constexpr int32 Indent = 4;
				FString ConvertedString = FString::Printf(TEXT("%*s"), NestedCount*Indent, TEXT(""));
				ConvertedString.Append(Line);
				ConvertedString.ReplaceInline(TEXT("="), TEXT(" = "));
				ConvertedString.ReplaceInline(TEXT("\""), TEXT(""));

				// Create new property event
				FStateTreeTracePropertyEvent PropertyEvent(/*RecordingWorldTime*/0, *ConvertedString);

				// Create Tree element to hold the event
				const TSharedPtr<FStateTreeDebuggerEventTreeElement> NewChildElement = MakeShareable(new FStateTreeDebuggerEventTreeElement(
					ParentElement->Frame,
					FStateTreeTraceEventVariantType(TInPlaceType<FStateTreeTracePropertyEvent>(), PropertyEvent),
					ParentElement->WeakStateTree.Get()));

				ParentElement->Children.Add(NewChildElement);
			};

		// Try to parse Struct for which properties are exported between '(' and ')' 
		if (InstanceDataAsString.StartsWith("(") && InstanceDataAsString.EndsWith(")"))
		{
			const FStringView View(GetData(InstanceDataAsString) + 1, InstanceDataAsString.Len() - 2);
			const TCHAR* ViewIt = View.GetData();
			const TCHAR* const ViewEnd = ViewIt + View.Len();
			const TCHAR* NextToken = ViewIt;
			int32 NestedCount = 0;
			
			for (; ViewIt != ViewEnd; ++ViewIt)
			{
				int32 LocalNestedCount = 0;
				if (*ViewIt == TCHAR('('))
				{
					LocalNestedCount++;
				}
				else if (*ViewIt == TCHAR(')'))
				{
					LocalNestedCount--;
				}
				else if (*ViewIt != TCHAR(','))
				{
					continue;
				}

				if (ViewIt != NextToken)
				{
					CreatePropertyElement(FStringView(NextToken, UE_PTRDIFF_TO_INT32(ViewIt - NextToken)), NestedCount);
				}
				NextToken = ViewIt+1;
				NestedCount += LocalNestedCount;
			}

			if (ViewIt != NextToken)
			{
				CreatePropertyElement(FStringView(NextToken, UE_PTRDIFF_TO_INT32(ViewIt - NextToken)), NestedCount);
			}
		}
		else
		{
			const TCHAR* Buffer = *InstanceDataAsString;
			FParse::Next(&Buffer);
			FString StrLine;
			while (FParse::Line(&Buffer, StrLine))
			{
				const TCHAR* Str = *StrLine;
				if (!FParse::Command(&Str, TEXT("BEGIN OBJECT"))
					&& !FParse::Command(&Str, TEXT("END OBJECT")))
				{
					CreatePropertyElement(Str);
				}
			}
		}
	}
}

void SStateTreeDebuggerView::ExpandAll(const TArray<TSharedPtr<FStateTreeDebuggerEventTreeElement>>& Items)
{
	for (const TSharedPtr<FStateTreeDebuggerEventTreeElement>& Item : Items)
	{
		bool bExpand = true;
		if (Item->Children.Num() > 0)
		{
			const FStateTreeDebuggerEventTreeElement* FirstChild = Item->Children[0].Get();
			if (FirstChild && FirstChild->Event.IsType<FStateTreeTracePropertyEvent>())
			{
				bExpand = false;
			}	
		}

		if (bExpand)
		{
			EventsTreeView->SetItemExpansion(Item, true);
			ExpandAll(Item->Children);
		}
	}
}

void SStateTreeDebuggerView::OnBreakpointHit(const FStateTreeInstanceDebugId InstanceId, const FStateTreeDebuggerBreakpoint Breakpoint, const TSharedRef<FUICommandList> ActionList) const
{
	// Pause PIE session if possible
	if (FPlayWorldCommands::Get().PausePlaySession.IsValid())
	{
		if (ActionList->CanExecuteAction(FPlayWorldCommands::Get().PausePlaySession.ToSharedRef()))
		{
			ActionList->ExecuteAction(FPlayWorldCommands::Get().PausePlaySession.ToSharedRef());
		}
	}

	// Extract associated UStateTreeState to focus on it.
	if (UStateTreeState* AssociatedState = FindStateAssociatedToBreakpoint(Breakpoint))
	{
		check(StateTreeViewModel);
		StateTreeViewModel->SetSelection(AssociatedState);
	}

	// Find matching event in the tree view and select it
	if (EventsTreeView.IsValid())
	{
		TSharedPtr<FStateTreeDebuggerEventTreeElement> MatchingElement = nullptr;
		UE::StateTreeDebugger::VisitEventTreeElements(EventsTreeElements, [&MatchingElement, Breakpoint](const TSharedPtr<FStateTreeDebuggerEventTreeElement>& VisitedElement)
			{
				if (Breakpoint.IsMatchingEvent(VisitedElement->Event))
				{
					MatchingElement = VisitedElement;
				}

				// Continue visit until we find a matching event
				return MatchingElement.IsValid() == false;
			});

		if (MatchingElement.IsValid())
		{
			EventsTreeView->SetSelection(MatchingElement);
		}
	}
}

void SStateTreeDebuggerView::OnNewSession()
{
	// We clear tracks:
	//  - analysis is not for an Editor session
	//  - explicitly set in the settings for Editor sessions
	//  - if previous analysis was not an Editor session
	check(Debugger);
	if (!Debugger->IsAnalyzingEditorSession()
		|| UStateTreeEditorSettings::Get().bShouldDebuggerResetDataOnNewPIESession
		|| !Debugger->WasAnalyzingEditorSession())
	{
		ResetTracks();
	}

	// Restore automatic scroll to most recent data.
	bAutoScroll = true;
}

void SStateTreeDebuggerView::OnNewInstance(FStateTreeInstanceDebugId InstanceId)
{
	check(Debugger);
	const UE::StateTreeDebugger::FInstanceDescriptor* FoundDescriptor = Debugger->GetInstanceDescriptor(InstanceId);
	if (!ensureMsgf(FoundDescriptor != nullptr, TEXT("This callback is from the Debugger so we expect to be able to find matching descriptor.")))
	{
		return;
	}

	const TSharedPtr<RewindDebugger::FRewindDebuggerTrack>* ExistingOwnerTrack = InstanceOwnerTracks.FindByPredicate(
		[FoundDescriptor](const TSharedPtr<RewindDebugger::FRewindDebuggerTrack>& Track)
		{
			return Track.Get()->GetName() == FoundDescriptor->Name;
		});

	if (ExistingOwnerTrack == nullptr)
	{
		ExistingOwnerTrack = &InstanceOwnerTracks.Add_GetRef(MakeShared<FStateTreeDebuggerOwnerTrack>(FText::FromString(FoundDescriptor->Name)));
	}

	if (FStateTreeDebuggerOwnerTrack* OwnerTrack = static_cast<FStateTreeDebuggerOwnerTrack*>(ExistingOwnerTrack->Get()))
	{
		const FString TrackName = FString::Printf(TEXT("Execution #%d"), OwnerTrack->NumSubTracks()+1);
		const TSharedPtr<FStateTreeDebuggerInstanceTrack> SubTrack = MakeShared<FStateTreeDebuggerInstanceTrack>(Debugger, InstanceId, FText::FromString(TrackName), ViewRange);
		OwnerTrack->AddSubTrack(SubTrack);

		// Look at current selection; if nothing selected or stale track then select new track
		const TSharedPtr<RewindDebugger::FRewindDebuggerTrack> Selection = InstancesTreeView->GetSelection();
		if (const FStateTreeDebuggerBaseTrack* DebuggerBaseTrack = static_cast<FStateTreeDebuggerBaseTrack*>(Selection.Get()))
		{
			if (DebuggerBaseTrack->IsStale())
			{
				InstancesTreeView->SetSelection(SubTrack);
				InstancesTreeView->ScrollTo(SubTrack);
			}
		}
		else
		{
			InstancesTreeView->SetSelection(SubTrack);
		}
	}

	InstancesTreeView->Refresh();
	InstanceTimelinesTreeView->Refresh();
}

void SStateTreeDebuggerView::OnSelectedInstanceCleared()
{
	EventsTreeElements.Reset();
	if (EventsTreeView)
	{
		EventsTreeView->RequestTreeRefresh();
	}
}

TSharedRef<SWidget> SStateTreeDebuggerView::OnGetDebuggerTracesMenu() const
{
	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection*/true, /*InCommandList*/nullptr);

	TArray<FStateTreeDebugger::FTraceDescriptor> TraceDescriptors;

	check(Debugger);
	Debugger->GetLiveTraces(TraceDescriptors);

	for (const FStateTreeDebugger::FTraceDescriptor& TraceDescriptor : TraceDescriptors)
	{
		const FText Desc = Debugger->DescribeTrace(TraceDescriptor);

		FUIAction ItemAction(FExecuteAction::CreateSPLambda(Debugger.ToSharedRef(), [Debugger = Debugger, TraceDescriptor]()
			{
				Debugger->RequestSessionAnalysis(TraceDescriptor);
			}));
		MenuBuilder.AddMenuEntry(Desc, TAttribute<FText>(), FSlateIcon(), ItemAction);
	}

	// Failsafe when no match
	if (TraceDescriptors.Num() == 0)
	{
		const FText Desc = LOCTEXT("NoLiveSessions", "Can't find live trace sessions");
		FUIAction ItemAction(FExecuteAction::CreateSPLambda(Debugger.ToSharedRef(), [Debugger = Debugger]()
			{
				Debugger->RequestSessionAnalysis(FStateTreeDebugger::FTraceDescriptor());
			}));
		MenuBuilder.AddMenuEntry(Desc, TAttribute<FText>(), FSlateIcon(), ItemAction);
	}

	return MenuBuilder.MakeWidget();
}

void SStateTreeDebuggerView::TrackCursor()
{
	const double ScrubTime = ScrubTimeAttribute.Get();
	TRange<double> CurrentViewRange = ViewRange;
	const double ViewRangeDuration = CurrentViewRange.GetUpperBoundValue() - CurrentViewRange.GetLowerBoundValue();

	static constexpr double LeadingMarginFraction = 0.05;
	static constexpr double TrailingMarginFraction = 0.01;

	if (ScrubTime > (CurrentViewRange.GetUpperBoundValue() - (ViewRangeDuration * LeadingMarginFraction)))
	{
		CurrentViewRange.SetUpperBound(ScrubTime + (ViewRangeDuration * LeadingMarginFraction));
		CurrentViewRange.SetLowerBound(CurrentViewRange.GetUpperBoundValue() - ViewRangeDuration);
	}

	if (ScrubTime < (CurrentViewRange.GetLowerBoundValue() + (ViewRangeDuration * TrailingMarginFraction)))
	{
		CurrentViewRange.SetLowerBound(ScrubTime - (ViewRangeDuration * TrailingMarginFraction));
		CurrentViewRange.SetUpperBound(CurrentViewRange.GetLowerBoundValue() + ViewRangeDuration);
	}

	ViewRange = CurrentViewRange;
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_STATETREE_DEBUGGER