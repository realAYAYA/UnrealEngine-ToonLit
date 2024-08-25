// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVM/CurveEditorExtension.h"
#include "MVVM/Selection/Selection.h"

#include "FrameNumberDetailsCustomization.h"
#include "Filters/SCurveEditorFilterPanel.h"
#include "Framework/Docking/TabManager.h"
#include "IPropertyRowGenerator.h"
#include "IStructureDetailsView.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"
#include "SCurveEditorPanel.h"
#include "SCurveEditorToolProperties.h"
#include "SCurveKeyDetailPanel.h"
#include "SSequencerTreeFilterStatusBar.h"
#include "STemporarilyFocusedSpinBox.h"
#include "Sequencer.h"
#include "SequencerCommands.h"
#include "Toolkits/IToolkitHost.h"
#include "Tree/SCurveEditorTree.h"
#include "Tree/SCurveEditorTreeFilterStatusBar.h"
#include "Tree/SCurveEditorTreeTextFilter.h"
#include "Widgets/CurveEditor/SSequencerCurveEditor.h"
#include "Widgets/CurveEditor/SequencerCurveEditorTimeSliderController.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBorder.h"

#define LOCTEXT_NAMESPACE "SequencerCurveEditorExtension"

namespace UE
{
namespace Sequencer
{

class FSequencerCurveEditor : public FCurveEditor
{
public:
	TWeakPtr<FSequencer> WeakSequencer;

	FSequencerCurveEditor(TWeakPtr<FSequencer> InSequencer)
		: WeakSequencer(InSequencer)
	{}

	virtual void GetGridLinesX(TArray<float>& MajorGridLines, TArray<float>& MinorGridLines, TArray<FText>* MajorGridLabels) const override
	{
		TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
		FCurveEditorScreenSpaceH PanelInputSpace = GetPanelInputSpace();

		double MajorGridStep = 0.0;
		int32  MinorDivisions = 0;

		if (Sequencer.IsValid() && Sequencer->GetGridMetrics(PanelInputSpace.GetPhysicalWidth(), PanelInputSpace.GetInputMin(), PanelInputSpace.GetInputMax(), MajorGridStep, MinorDivisions))
		{
			const double FirstMajorLine = FMath::FloorToDouble(PanelInputSpace.GetInputMin() / MajorGridStep) * MajorGridStep;
			const double LastMajorLine = FMath::CeilToDouble(PanelInputSpace.GetInputMax() / MajorGridStep) * MajorGridStep;

			for (double CurrentMajorLine = FirstMajorLine; CurrentMajorLine < LastMajorLine; CurrentMajorLine += MajorGridStep)
			{
				MajorGridLines.Add(PanelInputSpace.SecondsToScreen(CurrentMajorLine));

				for (int32 Step = 1; Step < MinorDivisions; ++Step)
				{
					MinorGridLines.Add(PanelInputSpace.SecondsToScreen(CurrentMajorLine + Step * MajorGridStep / MinorDivisions));
				}
			}
		}
	}
	int32 GetSupportedTangentTypes() override
	{
		return ((int32)ECurveEditorTangentTypes::InterpolationConstant	|
			(int32)ECurveEditorTangentTypes::InterpolationLinear		|
			(int32)ECurveEditorTangentTypes::InterpolationCubicAuto		|
			(int32)ECurveEditorTangentTypes::InterpolationCubicUser		|
			(int32)ECurveEditorTangentTypes::InterpolationCubicBreak	|
			(int32)ECurveEditorTangentTypes::InterpolationCubicWeighted |
			(int32)ECurveEditorTangentTypes::InterpolationCubicSmartAuto);
	}
};

struct FSequencerCurveEditorBounds : ICurveEditorBounds
{
	FSequencerCurveEditorBounds(TSharedRef<FSequencer> InSequencer)
		: WeakSequencer(InSequencer)
	{
		TRange<double> Bounds = InSequencer->GetViewRange();
		InputMin = Bounds.GetLowerBoundValue();
		InputMax = Bounds.GetUpperBoundValue();
	}

	virtual void GetInputBounds(double& OutMin, double& OutMax) const override
	{
		TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
		if (Sequencer.IsValid())
		{
			const bool bLinkTimeRange = Sequencer->GetSequencerSettings()->GetLinkCurveEditorTimeRange();
			if (bLinkTimeRange)
			{
				TRange<double> Bounds = Sequencer->GetViewRange();
				OutMin = Bounds.GetLowerBoundValue();
				OutMax = Bounds.GetUpperBoundValue();
			}
			else
			{
				// If they don't want to link the time range with Sequencer we return the cached value.
				OutMin = InputMin;
				OutMax = InputMax;
			}
		}
	}

	virtual void SetInputBounds(double InMin, double InMax) override
	{
		TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
		if (Sequencer.IsValid())
		{
			const bool bLinkTimeRange = Sequencer->GetSequencerSettings()->GetLinkCurveEditorTimeRange();
			if (bLinkTimeRange)
			{
				FFrameRate TickResolution = Sequencer->GetFocusedTickResolution();

				if (InMin * TickResolution > TNumericLimits<int32>::Lowest() && InMax * TickResolution < TNumericLimits<int32>::Max())
				{
					Sequencer->SetViewRange(TRange<double>(InMin, InMax), EViewRangeInterpolation::Immediate);
				}
			}

			// We update these even if you are linked to the Sequencer Timeline so that when you turn off the link setting
			// you don't pop to your last values, instead your view stays as is and just stops moving when Sequencer moves.
			InputMin = InMin;
			InputMax = InMax;
		}
	}

	/** The min/max values for the viewing range. Only used if Curve Editor/Sequencer aren't linked ranges. */
	double InputMin, InputMax;
	TWeakPtr<FSequencer> WeakSequencer;
};

const FName FCurveEditorExtension::CurveEditorTabName = FName(TEXT("SequencerGraphEditor"));

TSharedRef<IPropertyTypeCustomization> CreateFrameNumberCustomization(TWeakPtr<ISequencer> WeakSequencer)
{
	TSharedPtr<ISequencer> SequencerPtr = WeakSequencer.Pin();
	return MakeShared<FFrameNumberDetailsCustomization>(SequencerPtr->GetNumericTypeInterface());
}

FCurveEditorExtension::FCurveEditorExtension()
{
}

void FCurveEditorExtension::OnCreated(TSharedRef<FViewModel> InWeakOwner)
{
	ensureMsgf(!WeakOwnerModel.Pin().IsValid(), TEXT("This extension was already created!"));
	WeakOwnerModel = InWeakOwner->CastThisShared<FSequencerEditorViewModel>();
}

void FCurveEditorExtension::CreateCurveEditor(const FTimeSliderArgs& TimeSliderArgs)
{
	TSharedPtr<FSequencerEditorViewModel> OwnerModel = WeakOwnerModel.Pin();
	if (!ensure(OwnerModel))
	{
		return;
	}

	TSharedPtr<FSequencer> Sequencer = OwnerModel->GetSequencerImpl();
	if (!ensure(Sequencer))
	{
		return;
	}

	// If they've said they want to support the curve editor then they need to provide a toolkit host
	// so that we know where to spawn our tab into.
	if (!ensure(Sequencer->GetToolkitHost().IsValid()))
	{
		return;
	}

	// Create the curve editor;
	{
		FCurveEditorInitParams CurveEditorInitParams;

		USequencerSettings* SequencerSettings = Sequencer->GetSequencerSettings();

		CurveEditorModel = MakeShared<FSequencerCurveEditor>(Sequencer);
		CurveEditorModel->SetBounds(MakeUnique<FSequencerCurveEditorBounds>(Sequencer.ToSharedRef()));
		CurveEditorModel->InitCurveEditor(CurveEditorInitParams);

		CurveEditorModel->InputSnapEnabledAttribute = MakeAttributeLambda([SequencerSettings] { return SequencerSettings->GetIsSnapEnabled(); });
		CurveEditorModel->OnInputSnapEnabledChanged = FOnSetBoolean::CreateLambda([SequencerSettings](bool NewValue) { SequencerSettings->SetIsSnapEnabled(NewValue); });

		CurveEditorModel->OutputSnapEnabledAttribute = MakeAttributeLambda([SequencerSettings] { return SequencerSettings->GetSnapCurveValueToInterval(); });
		CurveEditorModel->OnOutputSnapEnabledChanged = FOnSetBoolean::CreateLambda([SequencerSettings](bool NewValue) { SequencerSettings->SetSnapCurveValueToInterval(NewValue); });

		CurveEditorModel->FixedGridSpacingAttribute = MakeAttributeLambda([SequencerSettings]() -> TOptional<float> { return SequencerSettings->GetGridSpacing(); });
		CurveEditorModel->InputSnapRateAttribute = MakeAttributeSP(Sequencer.Get(), &FSequencer::GetFocusedDisplayRate);

		CurveEditorModel->DefaultKeyAttributes = MakeAttributeLambda([this]() { return GetDefaultKeyAttributes(); });
	}

	// We create a custom Time Slider Controller which is just a wrapper around the actual one, but is 
	// aware of our custom bounds logic. Currently the range the bar displays is tied to Sequencer 
	// timeline and not the Bounds, so we need a way of changing it to look at the Bounds but only for 
	// the Curve Editor time slider controller. We want everything else to just pass through though.
	TSharedRef<ITimeSliderController> CurveEditorTimeSliderController = MakeShared<FSequencerCurveEditorTimeSliderController>(
			TimeSliderArgs, Sequencer, CurveEditorModel.ToSharedRef());
	
	PlayTimeDisplay = StaticCastSharedRef<STemporarilyFocusedSpinBox<double>>(Sequencer->MakePlayTimeDisplay(TimeSliderArgs.NumericTypeInterface.ToSharedRef()));

	CurveEditorTreeView = SNew(SCurveEditorTree, CurveEditorModel);
	CurveEditorPanel = SNew(SCurveEditorPanel, CurveEditorModel.ToSharedRef())
		// Grid lines match the color specified in FSequencerTimeSliderController::OnPaintViewArea
		.GridLineTint(FLinearColor(0.f, 0.f, 0.f, 0.3f))
		.ExternalTimeSliderController(CurveEditorTimeSliderController)
		.MinimumViewPanelHeight(0.f)
		.TabManager(Sequencer->GetToolkitHost()->GetTabManager())
		.DisabledTimeSnapTooltip(LOCTEXT("CurveEditorTimeSnapDisabledTooltip", "Time Snapping is currently driven by Sequencer."))
		.TreeContent()
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(CurveEditorSearchBox, SCurveEditorTreeTextFilter, CurveEditorModel)
			]

			+ SVerticalBox::Slot()
			[
				SNew(SOverlay)

				+ SOverlay::Slot()
				[
					SNew(SScrollBorder, CurveEditorTreeView.ToSharedRef())
					[
						CurveEditorTreeView.ToSharedRef()
					]
				]

				+ SOverlay::Slot()
				.VAlign(VAlign_Bottom)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SAssignNew(CurveEditorTreeFilterStatusBar, SCurveEditorTreeFilterStatusBar, CurveEditorModel)
						.Visibility(EVisibility::Hidden) // Initially hidden, visible on hover of the info button
					]
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				[
					SNew(SButton)
					.VAlign(EVerticalAlignment::VAlign_Center)
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.ToolTipText_Lambda([this] { return LOCTEXT("ShowStatus", "Show Status"); })
					.ContentPadding(FMargin(1, 0))
					.OnHovered_Lambda([this] { CurveEditorTreeFilterStatusBar->ShowStatusBar(); })
					.OnUnhovered_Lambda([this] { CurveEditorTreeFilterStatusBar->FadeOutStatusBar(); })
					.OnClicked_Lambda([this] { CurveEditorTreeFilterStatusBar->HideStatusBar(); return FReply::Handled(); })
					[
						SNew(SImage)
						.ColorAndOpacity(FSlateColor::UseForeground())
						.Image(FAppStyle::Get().GetBrush("Icons.Info.Small"))
					]
				]

				+ SHorizontalBox::Slot()
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
					.HAlign(HAlign_Center)
					[
						Sequencer->MakeTransportControls(true)
					]
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				[
					SNew(SButton)
					.VAlign(EVerticalAlignment::VAlign_Center)
					.ButtonStyle(FAppStyle::Get(), "NoBorder")
					.ContentPadding(FMargin(1, 0))
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Right)
						.Padding(FMargin(3.f, 0.f, 0.f, 0.f))
						[
							SNew(SBorder)
							.BorderImage(nullptr)
							[
								PlayTimeDisplay.ToSharedRef()
							]
						]
					]
				]
			]
		];

	// Register an instanced custom property type layout to handle converting FFrameNumber from Tick Resolution to Display Rate.
	TWeakPtr<ISequencer> WeakSequencer(Sequencer);
	CurveEditorPanel->GetKeyDetailsView()->GetPropertyRowGenerator()->RegisterInstancedCustomPropertyTypeLayout(
			"FrameNumber", 
			FOnGetPropertyTypeCustomizationInstance::CreateStatic(CreateFrameNumberCustomization, WeakSequencer));
	CurveEditorPanel->GetToolPropertiesPanel()->GetStructureDetailsView()->GetDetailsView()->RegisterInstancedCustomPropertyTypeLayout(
		"FrameNumber",
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(CreateFrameNumberCustomization, WeakSequencer));

	// And jump to the Curve Editor tree search if you have the Curve Editor focused
	CurveEditorModel->GetCommands()->MapAction(
		FSequencerCommands::Get().QuickTreeSearch,
		FExecuteAction::CreateLambda([this] { FSlateApplication::Get().SetKeyboardFocus(CurveEditorSearchBox, EFocusCause::SetDirectly); })
	);

	CurveEditorModel->GetCommands()->MapAction(
		FSequencerCommands::Get().ToggleShowGotoBox,
		FExecuteAction::CreateLambda([this] { PlayTimeDisplay->Setup(); FSlateApplication::Get().SetKeyboardFocus(PlayTimeDisplay, EFocusCause::SetDirectly); })
	);

	CurveEditorWidget = SNew(SSequencerCurveEditor, CurveEditorPanel.ToSharedRef(), Sequencer);

	CurveEditorPanel->OnFilterClassChanged.BindRaw(this, &FCurveEditorExtension::FilterClassChanged);

	// Check to see if the tab is already opened due to the saved window layout.
	FTabId TabId = FTabId(FCurveEditorExtension::CurveEditorTabName);
	TSharedPtr<SDockTab> ExistingCurveEditorTab = Sequencer->GetToolkitHost()->GetTabManager()->FindExistingLiveTab(TabId);
	if (ExistingCurveEditorTab)
	{
		ExistingCurveEditorTab->SetContent(CurveEditorWidget.ToSharedRef());
	}
}

void FCurveEditorExtension::FilterClassChanged()
{
	TSharedPtr<FSequencerEditorViewModel> OwnerModel = WeakOwnerModel.Pin();
	if (!ensure(OwnerModel))
	{
		return;
	}

	TSharedPtr<FSequencer> Sequencer = OwnerModel->GetSequencerImpl();
	if (!ensure(Sequencer))
	{
		return;
	}

	if (CurveEditorPanel)
	{
		TSharedPtr<SCurveEditorFilterPanel> FilterPanel = CurveEditorPanel->GetFilterPanel();
		if (FilterPanel)
		{
			TWeakPtr<ISequencer> WeakSequencer(Sequencer);
			FilterPanel->GetDetailsView()->RegisterInstancedCustomPropertyTypeLayout(
				"FrameNumber",
				FOnGetPropertyTypeCustomizationInstance::CreateStatic(CreateFrameNumberCustomization, WeakSequencer));
		}
	}
}

void FCurveEditorExtension::OpenCurveEditor()
{
	TSharedPtr<FSequencerEditorViewModel> OwnerModel = WeakOwnerModel.Pin();
	if (!ensure(OwnerModel))
	{
		return;
	}

	TSharedPtr<ISequencer> Sequencer = OwnerModel->GetSequencer();
	if (!Sequencer)
	{
		return;
	}

	// Request the Tab Manager invoke the tab. This will spawn the tab if needed, otherwise pull it to focus. This assumes
	// that the Toolkit Host's Tab Manager has already registered a tab with a NullWidget for content.
	FTabId TabId = FTabId(FCurveEditorExtension::CurveEditorTabName);
	TSharedPtr<SDockTab> CurveEditorTab = Sequencer->GetToolkitHost()->GetTabManager()->TryInvokeTab(TabId);
	if (CurveEditorTab.IsValid())
	{
		CurveEditorTab->SetContent(CurveEditorWidget.ToSharedRef());

		const FSlateIcon SequencerGraphIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCurveEditor.TabIcon");
		CurveEditorTab->SetTabIcon(SequencerGraphIcon.GetIcon());

		CurveEditorTab->SetLabel(LOCTEXT("SequencerMainGraphEditorTitle", "Sequencer Curves"));

		CurveEditorModel->ZoomToFit();
	}
}

bool FCurveEditorExtension::IsCurveEditorOpen() const
{
	TSharedPtr<FSequencerEditorViewModel> OwnerModel = WeakOwnerModel.Pin();
	if (!ensure(OwnerModel))
	{
		return false;
	}

	TSharedPtr<ISequencer> Sequencer = OwnerModel->GetSequencer();
	if (!Sequencer)
	{
		return false;
	}

	FTabId TabId = FTabId(FCurveEditorExtension::CurveEditorTabName);
	return Sequencer->GetToolkitHost()->GetTabManager()->FindExistingLiveTab(TabId).IsValid();
}

void FCurveEditorExtension::CloseCurveEditor()
{
	TSharedPtr<FSequencerEditorViewModel> OwnerModel = WeakOwnerModel.Pin();
	if (!ensure(OwnerModel))
	{
		return;
	}

	TSharedPtr<ISequencer> Sequencer = OwnerModel->GetSequencer();
	if (!Sequencer)
	{
		return;
	}

	FTabId TabId = FTabId(FCurveEditorExtension::CurveEditorTabName);
	TSharedPtr<SDockTab> CurveEditorTab = Sequencer->GetToolkitHost()->GetTabManager()->FindExistingLiveTab(TabId);
	if (CurveEditorTab)
	{
		CurveEditorTab->RequestCloseTab();
	}
}

FKeyAttributes FCurveEditorExtension::GetDefaultKeyAttributes() const
{
	TSharedPtr<FSequencerEditorViewModel> OwnerModel = WeakOwnerModel.Pin();
	check(OwnerModel);
	TSharedPtr<ISequencer> Sequencer = OwnerModel->GetSequencer();
	check(Sequencer);
	USequencerSettings* Settings = Sequencer->GetSequencerSettings();
	check(Settings);

	switch (Settings->GetKeyInterpolation())
	{
	case EMovieSceneKeyInterpolation::User:     return FKeyAttributes().SetInterpMode(RCIM_Cubic).SetTangentMode(RCTM_User);
	case EMovieSceneKeyInterpolation::Break:    return FKeyAttributes().SetInterpMode(RCIM_Cubic).SetTangentMode(RCTM_Break);
	case EMovieSceneKeyInterpolation::Linear:   return FKeyAttributes().SetInterpMode(RCIM_Linear).SetTangentMode(RCTM_Auto);
	case EMovieSceneKeyInterpolation::Constant: return FKeyAttributes().SetInterpMode(RCIM_Constant).SetTangentMode(RCTM_Auto);
	case EMovieSceneKeyInterpolation::Auto:     return FKeyAttributes().SetInterpMode(RCIM_Cubic).SetTangentMode(RCTM_Auto);
	case EMovieSceneKeyInterpolation::SmartAuto:
	default:                                    return FKeyAttributes().SetInterpMode(RCIM_Cubic).SetTangentMode(RCTM_SmartAuto);
	}
}

static bool bSyncSelectionRequested = false;
void FCurveEditorExtension::RequestSyncSelection()
{
	if (bSyncSelectionRequested)
	{
		return;
	}

	bSyncSelectionRequested = true;

	// We schedule selection syncing to the next editor tick because we might want to select items that
	// have just been added to the curve editor tree this tick. If it happened after the Slate update,
	// these items don't yet have a UI widget, and so selecting them doesn't do anything.
	//
	// Note that we capture a weak pointer of our owner model because selection changes can happen
	// right around the time when we want to unload everything (such as when loading a new map in the
	// editor). We don't want to extend the lifetime of our stuff in that case.
	TWeakPtr<FSequencerEditorViewModel> WeakRootViewModel(WeakOwnerModel);
	GEditor->GetTimerManager()->SetTimerForNextTick([WeakRootViewModel]()
	{
		bSyncSelectionRequested = false;
	
		TSharedPtr<FSequencerEditorViewModel> RootViewModel = WeakRootViewModel.Pin();
		if (!RootViewModel.IsValid())
		{
			return;
		}

		FCurveEditorExtension* This = RootViewModel->CastDynamic<FCurveEditorExtension>();
		if (This)
		{
			This->SyncSelection();
		}
	});
}

void FCurveEditorExtension::SyncSelection()
{
	if (!ensure(CurveEditorModel && CurveEditorTreeView))
	{
		return;
	}

	TSharedPtr<FSequencerEditorViewModel> OwnerModel = WeakOwnerModel.Pin();
	if (!ensure(OwnerModel))
	{
		return;
	}

	TSharedPtr<ISequencer> Sequencer = OwnerModel->GetSequencer();
	if (!ensure(Sequencer))
	{
		return;
	}

	CurveEditorModel->SuspendBroadcast();

	CurveEditorTreeView->ClearSelection();

	FCurveEditorTreeItemID FirstCurveEditorTreeItemID;
	for (TViewModelPtr<IOutlinerExtension> SelectedItem : OwnerModel->GetSelection()->Outliner)
	{
		if (TViewModelPtr<ICurveEditorTreeItemExtension> CurveEditorItem = SelectedItem.ImplicitCast())
		{
			FCurveEditorTreeItemID CurveEditorTreeItem = CurveEditorItem->GetCurveEditorItemID();
			if (CurveEditorTreeItem != FCurveEditorTreeItemID::Invalid())
			{
				if (!CurveEditorTreeView->IsItemSelected(CurveEditorTreeItem))
				{
					CurveEditorTreeView->SetItemSelection(CurveEditorTreeItem, true);
					if (!FirstCurveEditorTreeItemID.IsValid())
					{
						FirstCurveEditorTreeItemID = CurveEditorTreeItem;
					}
				}
			}
		}
	}
	if (FirstCurveEditorTreeItemID.IsValid())
	{
		CurveEditorTreeView->RequestScrollIntoView(FirstCurveEditorTreeItemID);
	}

	CurveEditorModel->ResumeBroadcast();
}

} // namespace Sequencer
} // namespace UE

#undef LOCTEXT_NAMESPACE
