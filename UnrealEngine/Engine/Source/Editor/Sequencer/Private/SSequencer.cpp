// Copyright Epic Games, Inc. All Rights Reserved.

#include "SSequencer.h"
#include "Engine/Blueprint.h"
#include "MovieSceneSequence.h"
#include "MVVM/CurveEditorExtension.h"
#include "MVVM/ViewModels/CategoryModel.h"
#include "MVVM/ViewModels/ChannelModel.h"
#include "MVVM/ViewModels/ViewModelIterators.h"
#include "MVVM/Extensions/IRenameableExtension.h"
#include "MVVM/Extensions/HierarchicalCacheExtension.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"
#include "MVVM/ViewModels/SequencerTrackAreaViewModel.h"
#include "MVVM/ViewModels/SequencerOutlinerViewModel.h"
#include "MVVM/ViewModels/TrackRowModel.h"
#include "MVVM/Selection/Selection.h"
#include "MVVM/SharedViewModelData.h"
#include "Sections/MovieSceneSubSection.h"
#include "MovieScene.h"
#include "Framework/MultiBox/MultiBoxDefs.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GameFramework/Actor.h"
#include "Misc/FeedbackContext.h"
#include "Modules/ModuleManager.h"
#include "Editor.h"
#include "IDetailsView.h"
#include "IKeyArea.h"
#include "Widgets/Layout/SBorder.h"
#include "ISequencerEditTool.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Images/SLayeredImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Layout/SScrollBorder.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Docking/SDockTab.h"
#include "SPositiveActionButton.h"
#include "Styling/AppStyle.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "MovieSceneSequenceEditor.h"
#include "Engine/Selection.h"
#include "LevelEditorViewport.h"
#include "Widgets/Navigation/SBreadcrumbTrail.h"
#include "SequencerCommands.h"
#include "Widgets/Input/SNumericDropDown.h"
#include "SequencerCommonHelpers.h"
#include "ISequencerWidgetsModule.h"
#include "ScopedTransaction.h"
#include "SequencerTimeSliderController.h"
#include "SequencerToolMenuContext.h"
#include "SSequencerSectionOverlay.h"
#include "STemporarilyFocusedSpinBox.h"
#include "MVVM/Views/STrackAreaView.h"
#include "MVVM/Views/SSequencerTrackAreaView.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "DragAndDrop/ActorDragDropOp.h"
#include "DragAndDrop/ActorDragDropGraphEdOp.h"
#include "DragAndDrop/ClassDragDropOp.h"
#include "DragAndDrop/FolderDragDropOp.h"
#include "DragAndDrop/CompositeDragDropOp.h"
#include "Widgets/Input/SSearchBox.h"
#include "MVVM/Views/SSequencerOutlinerView.h"
#include "MVVM/ViewModels/OutlinerColumns/IOutlinerColumn.h"
#include "MVVM/ViewModels/OutlinerColumns/OutlinerColumnTypes.h"
#include "MovieSceneTrackEditor.h"
#include "SSequencerSplitterOverlay.h"
#include "SequencerHotspots.h"
#include "SSequencerTimePanel.h"
#include "MVVM/ViewModels/VirtualTrackArea.h"
#include "Framework/Commands/GenericCommands.h"
#include "SequencerContextMenus.h"
#include "Math/UnitConversion.h"
#include "Widgets/Input/NumericUnitTypeInterface.inl"
#include "FrameNumberDetailsCustomization.h"
#include "SequencerSettings.h"
#include "SSequencerTransformBox.h"
#include "SSequencerStretchBox.h"
#include "SSequencerDebugVisualizer.h"
#include "SSequencerTreeFilterStatusBar.h"
#include "ISequencerModule.h"
#include "IMovieRendererInterface.h"
#include "IVREditorModule.h"
#include "EditorFontGlyphs.h"
#include "HAL/PlatformApplicationMisc.h"
#include "SSequencerPlayRateCombo.h"
#include "Camera/CameraActor.h"
#include "SCurveEditorPanel.h"
#include "Tree/SCurveEditorTree.h"
#include "Tree/CurveEditorTreeFilter.h"
#include "Tree/SCurveEditorTreeTextFilter.h"
#include "SequencerSelectionCurveFilter.h"
#include "SCurveKeyDetailPanel.h"
#include "MovieSceneTimeHelpers.h"
#include "FrameNumberNumericInterface.h"
#include "LevelSequence.h"
#include "SequencerLog.h"
#include "MovieSceneCopyableBinding.h"
#include "SObjectBindingTagManager.h"
#include "SSequencerGroupManager.h"
#include "SSequencerHierarchyBrowser.h"
#include "MovieSceneCopyableTrack.h"
#include "Tracks/MovieSceneSubTrack.h"
#include "IPropertyRowGenerator.h"
#include "Fonts/FontMeasure.h"
#include "Compilation/MovieSceneCompiledDataManager.h"
#include "SequencerTrackFilters.h"
#include "SequencerTrackFilterExtension.h"
#include "SequencerCustomizationManager.h"
#include "EditorActorFolders.h"
#include "Tracks/MovieSceneEventTrack.h"
#include "ToolMenus.h"
#include "MovieSceneToolHelpers.h"
#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"
#include "UniversalObjectLocators/ActorLocatorFragment.h"

#define LOCTEXT_NAMESPACE "Sequencer"

FSequencerOutlinerColumnVisibility::FSequencerOutlinerColumnVisibility(TSharedPtr<UE::Sequencer::IOutlinerColumn> InColumn)
	: Column(InColumn)
	, bIsColumnVisible(InColumn->IsColumnVisibleByDefault())
{}

FSequencerOutlinerColumnVisibility::FSequencerOutlinerColumnVisibility(TSharedPtr<UE::Sequencer::IOutlinerColumn> InColumn, bool bInIsColumnVisible)
	: Column(InColumn)
	, bIsColumnVisible(bInIsColumnVisible)
{}

/* SSequencer interface
 *****************************************************************************/
UE_DISABLE_OPTIMIZATION_SHIP
void SSequencer::Construct(const FArguments& InArgs, TSharedRef<FSequencer> InSequencer)
{
	using namespace UE::Sequencer;

	SequencerPtr = InSequencer;
	bIsActiveTimerRegistered = false;
	bUserIsSelecting = false;
	CachedClampRange = TRange<double>::Empty();
	CachedViewRange = TRange<double>::Empty();
	
	InitializeTrackFilters();

	ISequencerWidgetsModule& SequencerWidgets = FModuleManager::Get().LoadModuleChecked<ISequencerWidgetsModule>( "SequencerWidgets" );

	OnPlaybackRangeBeginDrag = InArgs._OnPlaybackRangeBeginDrag;
	OnPlaybackRangeEndDrag = InArgs._OnPlaybackRangeEndDrag;
	OnSelectionRangeBeginDrag = InArgs._OnSelectionRangeBeginDrag;
	OnSelectionRangeEndDrag = InArgs._OnSelectionRangeEndDrag;
	OnMarkBeginDrag = InArgs._OnMarkBeginDrag;
	OnMarkEndDrag = InArgs._OnMarkEndDrag;

	OnReceivedFocus = InArgs._OnReceivedFocus;
	OnInitToolMenuContext = InArgs._OnInitToolMenuContext;

	RootCustomization.OnReceivedDragOver = InArgs._OnReceivedDragOver;
	RootCustomization.OnReceivedDrop = InArgs._OnReceivedDrop;
	RootCustomization.OnAssetsDrop = InArgs._OnAssetsDrop;
	RootCustomization.OnClassesDrop = InArgs._OnClassesDrop;
	RootCustomization.OnActorsDrop = InArgs._OnActorsDrop;
	RootCustomization.OnFoldersDrop = InArgs._OnFoldersDrop;

	// Get the desired display format from the user's settings each time.
	TAttribute<EFrameNumberDisplayFormats> GetDisplayFormatAttr = MakeAttributeLambda(
		[this]
		{
			if (USequencerSettings* Settings = GetSequencerSettings())
			{
				return Settings->GetTimeDisplayFormat();
			}
			return EFrameNumberDisplayFormats::Frames;
		}
	);

	// Get the number of zero pad frames from the user's settings as well.
	TAttribute<uint8> GetZeroPadFramesAttr = MakeAttributeLambda(
		[this]()->uint8
		{
			if (USequencerSettings* Settings = GetSequencerSettings())
			{
				return Settings->GetZeroPadFrames();
			}
			return 0;
		}
	);

	TAttribute<FFrameRate> GetTickResolutionAttr = TAttribute<FFrameRate>(InSequencer, &FSequencer::GetFocusedTickResolution);
	TAttribute<FFrameRate> GetDisplayRateAttr    = TAttribute<FFrameRate>(InSequencer, &FSequencer::GetFocusedDisplayRate);

	// Create our numeric type interface so we can pass it to the time slider below.
	TSharedPtr<FFrameNumberInterface> FrameNumberInterface = MakeShareable(new FFrameNumberInterface(GetDisplayFormatAttr, GetZeroPadFramesAttr, GetTickResolutionAttr, GetDisplayRateAttr));
	NumericTypeInterface = FrameNumberInterface;
	
	FTimeSliderArgs TimeSliderArgs;
	{
		TimeSliderArgs.ViewRange = InArgs._ViewRange;
		TimeSliderArgs.ClampRange = InArgs._ClampRange;
		TimeSliderArgs.PlaybackRange = InArgs._PlaybackRange;
		TimeSliderArgs.TimeBounds = InArgs._TimeBounds;
		TimeSliderArgs.DisplayRate = TAttribute<FFrameRate>(InSequencer, &FSequencer::GetFocusedDisplayRate);
		TimeSliderArgs.TickResolution = TAttribute<FFrameRate>(InSequencer, &FSequencer::GetFocusedTickResolution);
		TimeSliderArgs.SelectionRange = InArgs._SelectionRange;
		TimeSliderArgs.OnPlaybackRangeChanged = InArgs._OnPlaybackRangeChanged;
		TimeSliderArgs.OnPlaybackRangeBeginDrag = OnPlaybackRangeBeginDrag;
		TimeSliderArgs.OnPlaybackRangeEndDrag = OnPlaybackRangeEndDrag;
		TimeSliderArgs.OnSelectionRangeChanged = InArgs._OnSelectionRangeChanged;
		TimeSliderArgs.OnSelectionRangeBeginDrag = OnSelectionRangeBeginDrag;
		TimeSliderArgs.OnSelectionRangeEndDrag = OnSelectionRangeEndDrag;
		TimeSliderArgs.OnMarkBeginDrag = OnMarkBeginDrag;
		TimeSliderArgs.OnMarkEndDrag = OnMarkEndDrag;
		TimeSliderArgs.OnViewRangeChanged = InArgs._OnViewRangeChanged;
		TimeSliderArgs.OnClampRangeChanged = InArgs._OnClampRangeChanged;
		TimeSliderArgs.OnGetNearestKey = InArgs._OnGetNearestKey;
		TimeSliderArgs.IsPlaybackRangeLocked = InArgs._IsPlaybackRangeLocked;
		TimeSliderArgs.OnTogglePlaybackRangeLocked = InArgs._OnTogglePlaybackRangeLocked;
		TimeSliderArgs.ScrubPosition = InArgs._ScrubPosition;
		TimeSliderArgs.ScrubPositionText = InArgs._ScrubPositionText;
		TimeSliderArgs.ScrubPositionParent = InArgs._ScrubPositionParent;
		TimeSliderArgs.ScrubPositionParentChain = InArgs._ScrubPositionParentChain;
		TimeSliderArgs.OnScrubPositionParentChanged = InArgs._OnScrubPositionParentChanged;
		TimeSliderArgs.OnBeginScrubberMovement = InArgs._OnBeginScrubbing;
		TimeSliderArgs.OnEndScrubberMovement = InArgs._OnEndScrubbing;
		TimeSliderArgs.OnScrubPositionChanged = InArgs._OnScrubPositionChanged;
		TimeSliderArgs.PlaybackStatus = InArgs._PlaybackStatus;
		TimeSliderArgs.SubSequenceRange = InArgs._SubSequenceRange;
		TimeSliderArgs.VerticalFrames = InArgs._VerticalFrames;
		TimeSliderArgs.MarkedFrames = InArgs._MarkedFrames;
		TimeSliderArgs.GlobalMarkedFrames = InArgs._GlobalMarkedFrames;
		TimeSliderArgs.OnSetMarkedFrame = InArgs._OnSetMarkedFrame;
		TimeSliderArgs.OnAddMarkedFrame = InArgs._OnAddMarkedFrame;
		TimeSliderArgs.OnDeleteMarkedFrame = InArgs._OnDeleteMarkedFrame;
		TimeSliderArgs.OnDeleteAllMarkedFrames = InArgs._OnDeleteAllMarkedFrames;
		TimeSliderArgs.AreMarkedFramesLocked = InArgs._AreMarkedFramesLocked;
		TimeSliderArgs.OnToggleMarkedFramesLocked = InArgs._OnToggleMarkedFramesLocked;

		TimeSliderArgs.NumericTypeInterface = GetNumericTypeInterface();
	}

	TimeSliderController = MakeShareable( new FSequencerTimeSliderController( TimeSliderArgs, SequencerPtr ) );
	
	TSharedRef<FSequencerTimeSliderController> TimeSliderControllerRef = TimeSliderController.ToSharedRef();

	bool bMirrorLabels = false;
	
	// Create the top and bottom sliders
	TopTimeSlider = SequencerWidgets.CreateTimeSlider( TimeSliderControllerRef, bMirrorLabels );
	bMirrorLabels = true;
	TSharedRef<ITimeSlider> BottomTimeSlider = SequencerWidgets.CreateTimeSlider( TimeSliderControllerRef, TAttribute<EVisibility>(this, &SSequencer::GetBottomTimeSliderVisibility), bMirrorLabels );

	EShowRange Ranges = EShowRange(EShowRange::WorkingRange | EShowRange::ViewRange);
	if(InArgs._ShowPlaybackRangeInTimeSlider)
	{
		Ranges |= EShowRange::PlaybackRange;
	}

	// Create bottom time range slider
	TSharedRef<ITimeSlider> BottomTimeRange = SequencerWidgets.CreateTimeRange(
		FTimeRangeArgs(
			Ranges,
			TimeSliderControllerRef,
			TAttribute<EVisibility>(this, &SSequencer::GetTimeRangeVisibility),
			NumericTypeInterface.ToSharedRef()
		),
		SequencerWidgets.CreateTimeRangeSlider(TimeSliderControllerRef)
	);

	OnGetPlaybackSpeeds = InArgs._OnGetPlaybackSpeeds;

	RootCustomization.AddMenuExtender = InArgs._AddMenuExtender;
	RootCustomization.ToolbarExtender = InArgs._ToolbarExtender;

	ColumnFillCoefficients[0] = 0.3f;
	ColumnFillCoefficients[1] = 0.7f;

	if (GetSequencerSettings())
	{
		ColumnFillCoefficients[0] = GetSequencerSettings()->GetTreeViewWidth();
		ColumnFillCoefficients[1] = 1.f - GetSequencerSettings()->GetTreeViewWidth();
	}

	TAttribute<float> FillCoefficient_0, FillCoefficient_1;
	{
		FillCoefficient_0.Bind(TAttribute<float>::FGetter::CreateSP(this, &SSequencer::GetColumnFillCoefficient, 0));
		FillCoefficient_1.Bind(TAttribute<float>::FGetter::CreateSP(this, &SSequencer::GetColumnFillCoefficient, 1));
	}

	TSharedRef<SScrollBar> ScrollBar = SNew(SScrollBar)
		.Thickness(FVector2D(9.0f, 9.0f));

	TSharedRef<SScrollBar> PinnedAreaScrollBar = SNew(SScrollBar)
		.Thickness(FVector2D(9.0f, 9.0f));

	TSharedPtr<FSequencerEditorViewModel> SequencerViewModel = InSequencer->GetViewModel()->CastThisShared<FSequencerEditorViewModel>();
	SAssignNew(PinnedTrackArea, SSequencerTrackAreaView, SequencerViewModel->GetPinnedTrackArea(), TimeSliderControllerRef);
	SAssignNew(PinnedTreeView, SSequencerOutlinerView, SequencerViewModel->GetOutliner(), PinnedTrackArea.ToSharedRef())
		.Selection(InSequencer->GetViewModel()->GetSelection())
		.ExternalScrollbar(PinnedAreaScrollBar)
		.Clipping(EWidgetClipping::ClipToBounds);

	PinnedTrackArea->SetOutliner(PinnedTreeView);
	PinnedTrackArea->SetShowPinned(true);
	PinnedTrackArea->SetIsPinned(true);
	PinnedTreeView->SetShowPinned(true);

	SAssignNew(TrackArea, SSequencerTrackAreaView, SequencerViewModel->GetTrackArea(), TimeSliderControllerRef);
	SAssignNew(TreeView, SSequencerOutlinerView, SequencerViewModel->GetOutliner(), TrackArea.ToSharedRef())
		.Selection(InSequencer->GetViewModel()->GetSelection())
		.ExternalScrollbar(ScrollBar)
		.Clipping(EWidgetClipping::ClipToBounds);

	TrackArea->SetOutliner(TreeView);

	TreeView->AddPinnedTreeView(PinnedTreeView);

	if (GetSequencerSettings())
	{
		InitializeOutlinerColumns();
	}

	SequencerViewModel->GetTrackArea()->CastThisChecked<FSequencerTrackAreaViewModel>()->InitializeDefaultEditTools(*TrackArea);
	SequencerViewModel->GetPinnedTrackArea()->CastThisChecked<FSequencerTrackAreaViewModel>()->InitializeDefaultEditTools(*PinnedTrackArea);


	if (USequencerSettings* Settings = GetSequencerSettings())
	{
		SequencerViewModel->SetViewDensity(Settings->GetViewDensity());
	}

	PlayTimeDisplay = StaticCastSharedRef<STemporarilyFocusedSpinBox<double>>(SequencerPtr.Pin()->MakePlayTimeDisplay(GetNumericTypeInterface()));

	TAttribute<FAnimatedRange> ViewRangeAttribute = InArgs._ViewRange;

	const int32 Column0 = 0, Column1 = 1;
	const int32 Row0 = 0, Row1 = 1, Row2 = 2, Row3 = 3, Row4 = 4;

	const float CommonPadding = 3.f;
	const FMargin ResizeBarPadding(4.f, 0, 0, 0);

	ChildSlot
	[
		SNew(SVerticalBox)

		// Toolbar
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBox)
			.Visibility(this, &SSequencer::GetShowSequencerToolbar)
			.Padding(FMargin(CommonPadding,0.0f,0.0f,0.f))
			[
				SNew(SWrapBox)
				.UseAllottedSize(true)
				.InnerSlotPadding(FVector2D(5, 0))
				+ SWrapBox::Slot()
				.FillEmptySpace(true)
				.FillLineWhenSizeLessThan(600)
				[
					SAssignNew(ToolbarContainer, SBox)
				]

				+ SWrapBox::Slot()
				.FillEmptySpace(true)
				[
					SNew(SHorizontalBox)
					// Right Aligned Breadcrumbs
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					[
						SNew(SSpacer)
					]

					// History Back Button
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
						.FillHeight(1.0f)
						[
							SNew(SButton)
							.Visibility_Lambda([this] { return CanNavigateBreadcrumbs() ? EVisibility::Visible : EVisibility::Collapsed; } )
							.VAlign(EVerticalAlignment::VAlign_Center)
							.ButtonStyle(FAppStyle::Get(), "SimpleButton")
							.ToolTipText_Lambda([this] { return SequencerPtr.Pin()->GetNavigateBackwardTooltip(); })
							.ContentPadding(FMargin(1, 0))
							.OnClicked_Lambda([this] { return SequencerPtr.Pin()->NavigateBackward(); })
							.IsEnabled_Lambda([this] { return SequencerPtr.Pin()->CanNavigateBackward(); })
							[
								SNew(SBox) // scale up since the default icons are 16x16
								.WidthOverride(20)
								.HeightOverride(20)
								[
									SNew(SImage)
									.ColorAndOpacity(FSlateColor::UseForeground())
									.Image(FAppStyle::Get().GetBrush("Icons.ArrowLeft"))
								]
							]
						]
					]

					// History Forward Button
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
						.FillHeight(1.0f)
						[
							SNew(SButton)
							.Visibility_Lambda([this] { return CanNavigateBreadcrumbs() ? EVisibility::Visible : EVisibility::Collapsed; } )
							.VAlign(EVerticalAlignment::VAlign_Center)
							.ButtonStyle(FAppStyle::Get(), "SimpleButton")
							.ToolTipText_Lambda([this] { return SequencerPtr.Pin()->GetNavigateForwardTooltip(); })
							.ContentPadding(FMargin(1, 0))
							.OnClicked_Lambda([this] { return SequencerPtr.Pin()->NavigateForward(); })
							.IsEnabled_Lambda([this] { return SequencerPtr.Pin()->CanNavigateForward(); })
							[
								SNew(SBox) // scale up since the default icons are 16x16
								.WidthOverride(20)
								.HeightOverride(20)
								[
									SNew(SImage)
									.ColorAndOpacity(FSlateColor::UseForeground())
									.Image(FAppStyle::Get().GetBrush("Icons.ArrowRight"))
								]
							]
						]
					]

					// Separator
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(3, 0)
					[
						SNew(SSeparator)
						.Visibility_Lambda([this] { return CanNavigateBreadcrumbs() ? EVisibility::Visible : EVisibility::Collapsed; } )
						.Orientation(Orient_Vertical)
					]
							
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SAssignNew(BreadcrumbPickerButton, SComboButton)
						.Visibility_Lambda([this] { return CanNavigateBreadcrumbs() ? EVisibility::Visible : EVisibility::Collapsed; } )
						.ButtonStyle(FAppStyle::Get(), "SimpleButton")
						.OnGetMenuContent_Lambda([this] { return SNew(SSequencerHierarchyBrowser, SequencerPtr); })
						.HasDownArrow(false)
						.ContentPadding(FMargin(3, 3))
						.ButtonContent()
						[
							SNew(SImage)
							.ColorAndOpacity(FSlateColor::UseForeground())
							.Image(FAppStyle::Get().GetBrush("Icons.FolderOpen"))
						]
					]

					// Right Aligned Breadcrumbs
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SAssignNew(BreadcrumbTrail, SBreadcrumbTrail<FSequencerBreadcrumb>)
						.Visibility(this, &SSequencer::GetBreadcrumbTrailVisibility)
						.OnCrumbClicked(this, &SSequencer::OnCrumbClicked)
						.ButtonStyle(FAppStyle::Get(), "SimpleButton")
						.DelimiterImage(FAppStyle::Get().GetBrush("Sequencer.BreadcrumbIcon"))
						.TextStyle(FAppStyle::Get(), "Sequencer.BreadcrumbText")
					]

					// Sequence Locking symbol
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(SCheckBox)
						.Style(FAppStyle::Get(),"ToggleButtonCheckBoxAlt")
						.Type(ESlateCheckBoxType::CheckBox) // Use CheckBox instead of ToggleType since we're not putting ohter widget inside
						.Padding(FMargin(0.f))
						.IsFocusable(false)
						.IsChecked_Lambda([this] { return GetIsSequenceReadOnly() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; } )
						.OnCheckStateChanged(this, &SSequencer::OnSetSequenceReadOnly)
						.ToolTipText_Lambda([this] { return GetIsSequenceReadOnly() ? LOCTEXT("UnlockSequence", "Unlock the animation so that it is editable") : LOCTEXT("LockSequence", "Lock the animation so that it is not editable"); } )
						.CheckedImage(FAppStyle::Get().GetBrush("Icons.Lock"))
						.CheckedHoveredImage(FAppStyle::Get().GetBrush("Icons.Lock"))
						.CheckedPressedImage(FAppStyle::Get().GetBrush("Icons.Lock"))
						.UncheckedImage(FAppStyle::Get().GetBrush("Icons.Unlock"))
						.UncheckedHoveredImage(FAppStyle::Get().GetBrush("Icons.Unlock"))
						.UncheckedPressedImage(FAppStyle::Get().GetBrush("Icons.Unlock"))
					]
				]
			]
		]

		+ SVerticalBox::Slot()
		[
			SNew(SSplitter)
			.Orientation(Orient_Horizontal)
			
			+ SSplitter::Slot()
			.Value(0.9f)
			[
				SNew(SOverlay)

				+ SOverlay::Slot()
				[
					// track area grid panel
					SNew( SGridPanel )
					.FillRow( 2, 1.f )
					.FillColumn( 0, FillCoefficient_0 )
					.FillColumn( 1, FillCoefficient_1 )

					+ SGridPanel::Slot( Column0, Row1 )
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
						[
							SNew(SSpacer)
						]
					]

					// outliner search box
					+ SGridPanel::Slot( Column0, Row1, SGridPanel::Layer(10) )
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
						.Padding(FMargin(CommonPadding*2, CommonPadding))
						.Clipping(EWidgetClipping::ClipToBounds)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(FMargin(0.f, 0.f, CommonPadding, 0.f))
							[
								MakeAddButton()
							]
						
							+ SHorizontalBox::Slot()
							.VAlign(VAlign_Center)
							[
								SAssignNew(SearchBox, SSearchBox)
								.HintText(LOCTEXT("SearchNodesHint", "Search Tracks"))
								.OnTextChanged( this, &SSequencer::OnOutlinerSearchChanged )
							]

							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(FMargin(0.f, 0.f, CommonPadding, 0.f))
							[
								MakeFilterButton()
							]
						]
					]

					// main sequencer area
					+ SGridPanel::Slot( Column0, Row2, SGridPanel::Layer(10) )
					.ColumnSpan(2)
					[
						SAssignNew(MainSequencerArea, SVerticalBox)

						+SVerticalBox::Slot()
						.AutoHeight()
						.MaxHeight(TAttribute<float>::Create(TAttribute<float>::FGetter::CreateSP(this, &SSequencer::GetPinnedAreaMaxHeight)))
						.Padding(FMargin(0.f, 0.f, 0.f, CommonPadding))
						[
							SNew(SOverlay)
							.Visibility(this, &SSequencer::GetPinnedAreaVisibility)

							+ SOverlay::Slot()
							[
								SNew(SScrollBorder, PinnedTreeView.ToSharedRef())
								[
									SNew(SHorizontalBox)

									// outliner tree
									+ SHorizontalBox::Slot()
									.FillWidth( FillCoefficient_0 )
									[
										PinnedTreeView.ToSharedRef()
									]

									// track area
									+ SHorizontalBox::Slot()
									.FillWidth( FillCoefficient_1 )
									[
										SNew(SBox)
										.Padding(ResizeBarPadding)
										.Clipping(EWidgetClipping::ClipToBounds)
										[
											PinnedTrackArea.ToSharedRef()
										]
									]
								]
							]

							+ SOverlay::Slot()
							.HAlign(HAlign_Right)
							[
								PinnedAreaScrollBar
							]
						]

						+SVerticalBox::Slot()
						[
							SNew(SOverlay)

							+ SOverlay::Slot()
							[
								SNew(SScrollBorder, TreeView.ToSharedRef())
								[
									SNew(SHorizontalBox)

									// outliner tree
									+ SHorizontalBox::Slot()
									.FillWidth( FillCoefficient_0 )
									[
										TreeView.ToSharedRef()
									]

									// track area
									+ SHorizontalBox::Slot()
									.FillWidth( FillCoefficient_1 )
									[
										SNew(SBox)
										.Padding(ResizeBarPadding)
										.Clipping(EWidgetClipping::ClipToBounds)
										[
											TrackArea.ToSharedRef()
										]
									]
								]
							]

							+ SOverlay::Slot()
							.HAlign(HAlign_Right)
							[
								ScrollBar
							]

							+ SOverlay::Slot()
							.VAlign(VAlign_Bottom)
							[
								SNew(SHorizontalBox)

								+ SHorizontalBox::Slot()
								.AutoWidth()
								[
									SAssignNew(SequencerTreeFilterStatusBar, SSequencerTreeFilterStatusBar, InSequencer)
									.Visibility(EVisibility::Hidden) // Initially hidden, visible on hover of the info button
								]
							]
						]
					]

					// Info Button, Transport Controls and Current Frame
					+ SGridPanel::Slot( Column0, Row4, SGridPanel::Layer(10) )
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
							.Visibility(this, &SSequencer::GetInfoButtonVisibility)
							.OnHovered_Lambda([this] { SequencerTreeFilterStatusBar->ShowStatusBar(); })
							.OnUnhovered_Lambda([this] { SequencerTreeFilterStatusBar->FadeOutStatusBar(); })
							.OnClicked_Lambda([this] { SequencerTreeFilterStatusBar->HideStatusBar(); return FReply::Handled(); })
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
								SequencerPtr.Pin()->MakeTransportControls(true)
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
								.Padding(FMargin(CommonPadding, 0.f, 0.f, 0.f))
								[
									SNew(SBorder)
									.BorderImage(nullptr)
									[
										PlayTimeDisplay.ToSharedRef()
									]
								]

								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.HAlign(HAlign_Right)
								.Padding(FMargin(CommonPadding, 0.f, 0.f, 0.f))
								[
									SNew(SBorder)
									.BorderImage(nullptr)
									[
										// Current loop index, if any
										SAssignNew(LoopIndexDisplay, STextBlock)
										.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
										.Text_Lambda([this]() -> FText {
											uint32 LoopIndex = SequencerPtr.Pin()->GetLocalLoopIndex();
											return (LoopIndex != FMovieSceneTimeWarping::InvalidWarpCount) ? FText::AsNumber(LoopIndex + 1) : FText();
										})
									]
								]
							]
						]
					]

					// Second column

					+ SGridPanel::Slot( Column1, Row1 )
					.Padding(ResizeBarPadding)
					.RowSpan(3)
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
						[
							SNew(SSpacer)
						]
					]

					+ SGridPanel::Slot( Column1, Row1, SGridPanel::Layer(10) )
					.Padding(ResizeBarPadding)
					[
						SNew( SBorder )
						.BorderImage( FAppStyle::GetBrush("ToolPanel.GroupBorder") )
						.BorderBackgroundColor( FLinearColor(.50f, .50f, .50f, 1.0f ) )
						.Padding(0)
						.Clipping(EWidgetClipping::ClipToBounds)
						[
							TopTimeSlider.ToSharedRef()
						]
					]

					// Overlay that draws the tick lines
					+ SGridPanel::Slot( Column1, Row2, SGridPanel::Layer(10) )
					.Padding(ResizeBarPadding)
					[
						SNew( SSequencerSectionOverlay, TimeSliderControllerRef )
						.Visibility(this, &SSequencer::GetShowTickLines)
						.DisplayScrubPosition( false )
						.DisplayTickLines( true )
						.Clipping(EWidgetClipping::ClipToBounds)
					]

					// Overlay that draws the scrub position
					+ SGridPanel::Slot( Column1, Row2, SGridPanel::Layer(20) )
					.Padding(ResizeBarPadding)
					[
						SNew( SSequencerSectionOverlay, TimeSliderControllerRef )
						.Visibility( EVisibility::HitTestInvisible )
						.DisplayScrubPosition( true )
						.DisplayTickLines( false )
						.DisplayMarkedFrames( true )
						.PaintPlaybackRangeArgs(this, &SSequencer::GetSectionPlaybackRangeArgs)
						.Clipping(EWidgetClipping::ClipToBounds)
					]

					+ SGridPanel::Slot(Column1, Row2, SGridPanel::Layer(30))
						.Padding(ResizeBarPadding)
						.HAlign(HAlign_Left)
						.VAlign(VAlign_Top)
					[
						// Transform box
						SAssignNew(TransformBox, SSequencerTransformBox, SequencerPtr.Pin().ToSharedRef(), *GetSequencerSettings(), NumericTypeInterface.ToSharedRef())
					]

					+ SGridPanel::Slot(Column1, Row2, SGridPanel::Layer(40))
						.Padding(ResizeBarPadding)
						.HAlign(HAlign_Left)
						.VAlign(VAlign_Top)
					[
						// Stretch box
						SAssignNew(StretchBox, SSequencerStretchBox, SequencerPtr.Pin().ToSharedRef(), *GetSequencerSettings(), NumericTypeInterface.ToSharedRef())
					]

					// debug vis
					+ SGridPanel::Slot( Column1, Row3, SGridPanel::Layer(10) )
					.Padding(ResizeBarPadding)
					[
						SNew(SSequencerDebugVisualizer, InSequencer)
						.ViewRange(FAnimatedRange::WrapAttribute(InArgs._ViewRange))
						.Visibility(this, &SSequencer::GetDebugVisualizerVisibility)
					]

					// play range sliders
					+ SGridPanel::Slot( Column1, Row4, SGridPanel::Layer(10) )
					.Padding(ResizeBarPadding)
					[
						SNew( SBorder )
						.BorderImage( FAppStyle::GetBrush("ToolPanel.GroupBorder") )
						.BorderBackgroundColor( FLinearColor(.50f, .50f, .50f, 1.0f ) )
						.Clipping(EWidgetClipping::ClipToBounds)
						.Padding(0)
						[
							SNew( SOverlay )

							+ SOverlay::Slot()
							[
								BottomTimeSlider
							]

							+ SOverlay::Slot()
							[
								BottomTimeRange
							]
						]
					]
				]

				+ SOverlay::Slot()
				[
					// track area virtual splitter overlay
					SAssignNew(TreeViewSplitter, SSequencerSplitterOverlay)
					.Style(FAppStyle::Get(), "Sequencer.AnimationOutliner.Splitter")
					.Visibility(EVisibility::SelfHitTestInvisible)
					.OnSplitterFinishedResizing(this, &SSequencer::OnSplitterFinishedResizing)

					+ SSplitter::Slot()
					.Value(FillCoefficient_0)
					// Can't use a minsize here because the grid panel that is actually being used to
					//   lay out the widgets only supports fill coefficients and this leads to a disparity between the two
					// .MinSize(200)
					.OnSlotResized(SSplitter::FOnSlotResized::CreateSP(this, &SSequencer::OnColumnFillCoefficientChanged, 0))
					[
						SNew(SSpacer)
					]

					+ SSplitter::Slot()
					.Value(FillCoefficient_1)
					.OnSlotResized(SSplitter::FOnSlotResized::CreateSP(this, &SSequencer::OnColumnFillCoefficientChanged, 1))
					[
						SNew(SSpacer)
					]
				]
			]
		]
	];

	if (InSequencer->GetHostCapabilities().bSupportsCurveEditor)
	{
		FCurveEditorExtension* CurveEditorExtension = SequencerViewModel->CastDynamicChecked<FCurveEditorExtension>();
		CurveEditorExtension->CreateCurveEditor(TimeSliderArgs);
	}

	ApplySequencerCustomization(RootCustomization);

	InSequencer->GetViewModel()->GetSelection()->KeySelection.OnChanged.AddSP(this, &SSequencer::HandleKeySelectionChanged);
	InSequencer->GetViewModel()->GetSelection()->Outliner.OnChanged.AddSP(this, &SSequencer::HandleOutlinerNodeSelectionChanged);

	ResetBreadcrumbs();
}
UE_ENABLE_OPTIMIZATION_SHIP

void SSequencer::BindCommands(TSharedRef<FUICommandList> SequencerCommandBindings, TSharedRef<FUICommandList> CurveEditorSharedBindings)
{
	auto CanPasteFromHistory = [this]{
		if (!HasFocusedDescendants() && !HasKeyboardFocus())
		{
			return false;
		}

		return SequencerPtr.Pin()->GetClipboardStack().Num() != 0;
	};

	auto CanOpenDirectorBlueprint = [this]{
		UMovieSceneSequence* RootSequence = SequencerPtr.Pin()->GetRootMovieSceneSequence();
		if (RootSequence && RootSequence->GetTypedOuter<UBlueprint>() == nullptr && UMovieScene::IsTrackClassAllowed(UMovieSceneEventTrack::StaticClass()))
		{
			return true;
		}
		return false;
	};

	SequencerCommandBindings->MapAction(
		FGenericCommands::Get().Paste,
		FExecuteAction::CreateSP(this, &SSequencer::OnPaste),
		FCanExecuteAction::CreateSP(this, &SSequencer::CanPaste)
	);

	SequencerCommandBindings->MapAction(
		FSequencerCommands::Get().PasteFromHistory,
		FExecuteAction::CreateSP(this, &SSequencer::PasteFromHistory),
		FCanExecuteAction::CreateLambda(CanPasteFromHistory)
	);

	SequencerCommandBindings->MapAction(
		FSequencerCommands::Get().ToggleShowGotoBox,
		FExecuteAction::CreateLambda([this] { PlayTimeDisplay->Setup(); FSlateApplication::Get().SetKeyboardFocus(PlayTimeDisplay, EFocusCause::SetDirectly); })
	);

	SequencerCommandBindings->MapAction(
		FSequencerCommands::Get().ToggleShowTransformBox,
		FExecuteAction::CreateLambda([this] { TransformBox->ToggleVisibility(); })
	);

	// Allow jumping to the Sequencer tree search if you have Sequencer focused
	SequencerCommandBindings->MapAction(
		FSequencerCommands::Get().QuickTreeSearch,
		FExecuteAction::CreateLambda([this] { FSlateApplication::Get().SetKeyboardFocus(SearchBox, EFocusCause::SetDirectly); })
	);
	
	SequencerCommandBindings->MapAction(
		FSequencerCommands::Get().ToggleShowStretchBox,
		FExecuteAction::CreateLambda([this] { StretchBox->ToggleVisibility(); })
	);

	auto OpenDirectorBlueprint = [WeakSequencer = SequencerPtr]
	{
		UMovieSceneSequence*       Sequence       = WeakSequencer.Pin()->GetFocusedMovieSceneSequence();
		FMovieSceneSequenceEditor* SequenceEditor = Sequence ? FMovieSceneSequenceEditor::Find(Sequence) : nullptr;
		if (SequenceEditor)
		{
			UBlueprint* DirectorBP = SequenceEditor->GetOrCreateDirectorBlueprint(Sequence);
			if (DirectorBP)
			{
				FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(DirectorBP);
			}
		}
	};

	SequencerCommandBindings->MapAction(
		FSequencerCommands::Get().OpenDirectorBlueprint,
		FExecuteAction::CreateLambda(OpenDirectorBlueprint),
		FCanExecuteAction::CreateLambda(CanOpenDirectorBlueprint)
	);

	SequencerCommandBindings->MapAction(
		FSequencerCommands::Get().OpenTaggedBindingManager,
		FExecuteAction::CreateSP(this, &SSequencer::OpenTaggedBindingManager)
	);

	SequencerCommandBindings->MapAction(
		FSequencerCommands::Get().OpenNodeGroupsManager,
		FExecuteAction::CreateSP(this, &SSequencer::OpenNodeGroupsManager)
	);

	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();

	for (TSharedRef<FSequencerTrackFilter> TrackFilter : AllTrackFilters)
	{
		if (TrackFilter->SupportsSequence(Sequencer->GetFocusedMovieSceneSequence()))
		{
			TrackFilter->BindCommands(SequencerCommandBindings, CurveEditorSharedBindings, SequencerPtr.Pin());
		}
	}
}

void SSequencer::OpenTickResolutionOptions()
{
	if (TSharedPtr<SWindow> Window = WeakTickResolutionOptionsWindow.Pin())
	{
		Window->DrawAttention(FWindowDrawAttentionParameters());
		return;
	}

	TSharedRef<SWindow> TickResolutionOptionsWindow = SNew(SWindow)
		.Title(LOCTEXT("TickResolutionOptions_Title", "Advanced Time Properties"))
		.SupportsMaximize(false)
		.ClientSize(FVector2D(600.f, 500.f))
		.Content()
		[
			SNew(SSequencerTimePanel, SequencerPtr)
		];

	TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
	if (ParentWindow)
	{
		FSlateApplication::Get().AddWindowAsNativeChild(TickResolutionOptionsWindow, ParentWindow.ToSharedRef());
	}
	else
	{
		FSlateApplication::Get().AddWindow(TickResolutionOptionsWindow);
	}

	WeakTickResolutionOptionsWindow = TickResolutionOptionsWindow;
}


void SSequencer::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FEditPropertyChain* PropertyThatChanged)
{
	// @todo sequencer: is this still needed?
}


/* SSequencer implementation
 *****************************************************************************/

TSharedRef<INumericTypeInterface<double>> SSequencer::GetNumericTypeInterface() const
{
	return NumericTypeInterface.ToSharedRef();
}

void SSequencer::InitializeTrackFilters()
{
	// Add all built-in track filters here
	AllTrackFilters.Add(MakeShared<FSequencerTrackFilter_AudioTracks>());
	AllTrackFilters.Add(MakeShared<FSequencerTrackFilter_EventTracks>());
	AllTrackFilters.Add(MakeShared<FSequencerTrackFilter_LevelVisibilityTracks>());
	AllTrackFilters.Add(MakeShared<FSequencerTrackFilter_ParticleTracks>());
	AllTrackFilters.Add(MakeShared<FSequencerTrackFilter_CinematicShotTracks>());
	AllTrackFilters.Add(MakeShared<FSequencerTrackFilter_SubTracks>());

	AllTrackFilters.Add(MakeShared<FSequencerTrackFilter_CameraObjects>());
	AllTrackFilters.Add(MakeShared<FSequencerTrackFilter_LightObjects>());
	AllTrackFilters.Add(MakeShared<FSequencerTrackFilter_SkeletalMeshObjects>());
	AllTrackFilters.Add(MakeShared<FSequencerTrackFilter_Animated>());

	// Add any global user-defined frontend filters
	for (TObjectIterator<USequencerTrackFilterExtension> ExtensionIt(RF_NoFlags); ExtensionIt; ++ExtensionIt)
	{
		if (USequencerTrackFilterExtension* PotentialExtension = *ExtensionIt)
		{
			if (PotentialExtension->HasAnyFlags(RF_ClassDefaultObject) && !PotentialExtension->GetClass()->HasAnyClassFlags(CLASS_Deprecated | CLASS_Abstract))
			{
				// Grab the filters
				TArray< TSharedRef<FSequencerTrackFilter> > ExtendedTrackFilters;
				PotentialExtension->AddTrackFilterExtensions(ExtendedTrackFilters);
				AllTrackFilters.Append(ExtendedTrackFilters);
			}
		}
	}

	for (TSharedRef<FSequencerTrackFilter> TrackFilter : AllTrackFilters)
	{
		const bool bEnabled = GetSequencerSettings()->IsTrackFilterEnabled(TrackFilter->GetDisplayName().ToString());
		SetTrackFilterEnabled(TrackFilter->GetDisplayName(), bEnabled);
	}

	// Sort by display name
	AllTrackFilters.Sort([](const TSharedRef<FSequencerTrackFilter>& LHS, const TSharedRef<FSequencerTrackFilter>& RHS) { return LHS->GetDisplayName().ToString() < RHS->GetDisplayName().ToString(); });
}

void SSequencer::UpdateOutlinerViewColumns()
{
	using namespace UE::Sequencer;

	// Save updated column list in settings
	TArray<FColumnVisibilitySetting> ColumnVisibilitySettings;

	for (FSequencerOutlinerColumnVisibility ColumnVisibility : OutlinerColumnVisibilities)
	{
		ColumnVisibilitySettings.Add(FColumnVisibilitySetting(ColumnVisibility.Column->GetColumnName(), ColumnVisibility.bIsColumnVisible));
	}

	GetSequencerSettings()->SetOutlinerColumnVisibility(ColumnVisibilitySettings);

	// Filter out hidden columns to create a list of visible columns for the outliner views
	TArray<TSharedPtr<IOutlinerColumn>> VisibleColumns;
	for (FSequencerOutlinerColumnVisibility ColumnVisibility : OutlinerColumnVisibilities)
	{
		if (ColumnVisibility.bIsColumnVisible)
		{
			VisibleColumns.Add(ColumnVisibility.Column);
		}
	}

	// Update both Outliner Views with updated visible outliner columns
	PinnedTreeView->SetOutlinerColumns(VisibleColumns);
	TreeView->SetOutlinerColumns(VisibleColumns);
}

void SSequencer::InitializeOutlinerColumns()
{
	using namespace UE::Sequencer;

	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();
	if (!Sequencer.IsValid())
	{
		return;
	}

	const TMap<FName, TSharedPtr<IOutlinerColumn>>& RegisteredColumns = Sequencer->GetOutlinerColumns();
	
	// Retrieve previously saved column names and visibilities
	TArray<FColumnVisibilitySetting> ColumnSettings = GetSequencerSettings()->GetOutlinerColumnSettings();
	TSet<FName> ColumnNamesFoundInSettings;

	// Add registered columns found in settings with their saved visibility state
	for (const FColumnVisibilitySetting& ColumnVisibility : ColumnSettings)
	{
		const TSharedPtr<IOutlinerColumn>* OutlinerColumn = RegisteredColumns.Find(ColumnVisibility.ColumnName);
		if (OutlinerColumn)
		{
			ColumnNamesFoundInSettings.Add(ColumnVisibility.ColumnName);
			OutlinerColumnVisibilities.Add(FSequencerOutlinerColumnVisibility(*OutlinerColumn, ColumnVisibility.bIsVisible));
		}
	}

	// Add registered columns not found in settings with their default visibility state
	for (const TTuple<FName, TSharedPtr<IOutlinerColumn>>& RegisteredColumn : RegisteredColumns)
	{
		if (!ColumnNamesFoundInSettings.Contains(RegisteredColumn.Key))
		{
			OutlinerColumnVisibilities.Add(FSequencerOutlinerColumnVisibility(RegisteredColumn.Value));
		}
	}

	Algo::Sort(OutlinerColumnVisibilities, [](const FSequencerOutlinerColumnVisibility& A, const FSequencerOutlinerColumnVisibility& B){
		return A.Column->GetPosition() < B.Column->GetPosition();
	});

	UpdateOutlinerViewColumns();
}

/* SSequencer callbacks
 *****************************************************************************/

void SSequencer::HandleKeySelectionChanged()
{
}


void SSequencer::HandleOutlinerNodeSelectionChanged()
{
	using namespace UE::Sequencer;

	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();
	if (!Sequencer.IsValid())
	{
		return;
	}

	FViewModelPtr RootModel = Sequencer->GetViewModel();
	FCurveEditorExtension* CurveEditorExtension = RootModel->CastDynamic<FCurveEditorExtension>();
	if (!CurveEditorExtension)
	{
		return;
	}

	TSharedPtr<FCurveEditor> CurveEditor = CurveEditorExtension->GetCurveEditor();
	if (CurveEditor)
	{
		const USequencerSettings* SequencerSettings = GetSequencerSettings();
		// If we're isolating to the selection and there is one, add the filter
		if (SequencerSettings->ShouldIsolateToCurveEditorSelection() && Sequencer->GetViewModel()->GetSelection()->Outliner.Num() != 0)
		{
			if (!SequencerSelectionCurveEditorFilter)
			{
				SequencerSelectionCurveEditorFilter = MakeShared<FSequencerSelectionCurveFilter>();
			}

			SequencerSelectionCurveEditorFilter->Update(Sequencer->GetViewModel()->GetSelection(), SequencerSettings->GetAutoExpandNodesOnSelection());

			CurveEditor->GetTree()->AddFilter(SequencerSelectionCurveEditorFilter);
		}
		// If we're not isolating to the selection (or there is no selection) remove the filter
		else if (SequencerSelectionCurveEditorFilter)
		{
			CurveEditor->GetTree()->RemoveFilter(SequencerSelectionCurveEditorFilter);
			SequencerSelectionCurveEditorFilter = nullptr;
		}

		if (GetSequencerSettings()->ShouldSyncCurveEditorSelection())
		{
			// We schedule a selection synchronization for the next update. This synchronization must happen
			// after all filters have been applied, because the items we want to select in the curve editor
			// might be currently filtered out, but will be visible when filters are re-evaluated. This is
			// why curve editor integration runs in FSequencerNodeTree after filtering.
			CurveEditorExtension->RequestSyncSelection();
		}
	}

	if (NodeGroupManager.IsValid())
	{
		NodeGroupManager->SelectItemsSelectedInSequencer();
	}

	if (SequencerTreeFilterStatusBar.IsValid())
	{
		SequencerTreeFilterStatusBar->UpdateText();
	}
}

TSharedRef<SWidget> SSequencer::MakeAddButton()
{
	return SNew(SPositiveActionButton)
	.OnGetMenuContent(this, &SSequencer::MakeAddMenu)
	.Icon(FAppStyle::Get().GetBrush("Icons.Plus"))
	.Text(LOCTEXT("Add", "Add"))
	.IsEnabled_Lambda([this]() { return !SequencerPtr.Pin()->IsReadOnly(); });
}

TSharedRef<SWidget> SSequencer::MakeFilterButton()
{
	TSharedPtr<SLayeredImage> FilterImage = SNew(SLayeredImage)
		.Image(FAppStyle::Get().GetBrush("Icons.Filter"))
		.ColorAndOpacity(FSlateColor::UseForeground());

	const TAttribute<const FSlateBrush*> ModifiedIcon = TAttribute<const FSlateBrush*>::CreateLambda([this]()
		{
			for (TSharedRef<FSequencerTrackFilter> TrackFilter : AllTrackFilters)
			{
				if (IsTrackFilterActive(TrackFilter))
				{
					return FAppStyle::Get().GetBrush("Icons.BadgeModified");
				}
			}
			return (const FSlateBrush*)nullptr;
		});

	FilterImage->AddLayer(ModifiedIcon);

	return SNew(SComboButton)
	.ComboButtonStyle(&FAppStyle::Get().GetWidgetStyle<FComboButtonStyle>("SimpleComboButtonWithIcon"))
	.ForegroundColor(FSlateColor::UseStyle())
	.ContentPadding(0)
	.ToolTipText(LOCTEXT("AddTrackFilterToolTip", "Add a track filter."))
	.OnGetMenuContent(this, &SSequencer::MakeFilterMenu)
	.ContentPadding(FMargin(1, 0))
	.HasDownArrow(false)
	.ButtonContent()
	[
		FilterImage.ToSharedRef()
	];
}

TSharedRef<SWidget> SSequencer::MakeToolBar()
{
	static const FName MenuName("Sequencer.MainToolBar");
	
	if (!UToolMenus::Get()->IsMenuRegistered(MenuName))
	{
		UToolMenu* Toolbar = UToolMenus::Get()->RegisterMenu(MenuName, NAME_None, EMultiBoxType::ToolBar);
		Toolbar->AddDynamicSection("PopulateToolBar", FNewToolMenuDelegate::CreateStatic(&SSequencer::PopulateToolBar));
	}

	TArray<TSharedPtr<FExtender>> AllExtenders;
	ISequencerModule& SequencerModule = FModuleManager::GetModuleChecked<ISequencerModule>("Sequencer");
	AllExtenders.Add(SequencerModule.GetToolBarExtensibilityManager()->GetAllExtenders());
	AllExtenders.Append(ToolbarExtenders);

	TSharedPtr<FExtender> Extender = FExtender::Combine(AllExtenders);

	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();

	USequencerToolMenuContext* ContextObject = NewObject<USequencerToolMenuContext>();
	ContextObject->SequencerWidget = SharedThis(this);

	FToolMenuContext Context(SequencerPtr.Pin()->GetCommandBindings(), Extender, ContextObject);

	// Allow any toolkits to initialize their menu context
	OnInitToolMenuContext.ExecuteIfBound(Context);

	return UToolMenus::Get()->GenerateWidget(MenuName, Context);
}

void SSequencer::PopulateToolBar(UToolMenu* InMenu)
{
	const FName SequencerToolbarStyleName = "SequencerToolbar";
	
	USequencerToolMenuContext* ContextObject = InMenu->FindContext<USequencerToolMenuContext>();
	if (!ContextObject)
	{
		return;
	}

	TSharedPtr<SSequencer> SequencerWidget = ContextObject->SequencerWidget.Pin();
	if (!SequencerWidget.Get())
	{
		return;
	}
	
	TWeakPtr<FSequencer> Sequencer = SequencerWidget.Get()->GetSequencer();
	if (!Sequencer.IsValid())
	{
		return;
	}

	{
		FToolMenuSection& Section = InMenu->AddSection("BaseCommands");

		if (Sequencer.Pin()->IsLevelEditorSequencer())
		{
			TAttribute<FSlateIcon> SaveIcon;
			SaveIcon.Bind(TAttribute<FSlateIcon>::FGetter::CreateLambda([Sequencer] 
			{
				if (Sequencer.IsValid())
				{
					TArray<UMovieScene*> MovieScenesToSave;
					MovieSceneHelpers::GetDescendantMovieScenes(Sequencer.Pin()->GetRootMovieSceneSequence(), MovieScenesToSave);
					for (UMovieScene* MovieSceneToSave : MovieScenesToSave)
					{
						UPackage* MovieScenePackageToSave = MovieSceneToSave->GetOuter()->GetOutermost();
						if (MovieScenePackageToSave->IsDirty())
						{
							return FSlateIcon(FAppStyle::Get().GetStyleSetName(), "Icons.SaveChanged");
						}
					}
				}
				return FSlateIcon(FAppStyle::Get().GetStyleSetName(), "Icons.Save");
			}));

			if (Sequencer.Pin()->GetHostCapabilities().bSupportsSaveMovieSceneAsset)
			{
				FToolMenuEntry SaveEntry = FToolMenuEntry::InitToolBarButton(
					"Save",
					FUIAction(FExecuteAction::CreateSP(SequencerWidget.Get(), &SSequencer::OnSaveMovieSceneClicked)),
					LOCTEXT("SaveDirtyPackages", "Save"),
					LOCTEXT("SaveDirtyPackagesTooltip", "Saves the current sequence and any subsequences"),
					SaveIcon
				);
				SaveEntry.StyleNameOverride = SequencerToolbarStyleName;
				Section.AddEntry(SaveEntry);				
			}

			FToolMenuEntry FindInContentBrowserEntry = FToolMenuEntry::InitToolBarButton(FSequencerCommands::Get().FindInContentBrowser, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FAppStyle::Get().GetStyleSetName(), "SystemWideCommands.FindInContentBrowser"));
			FindInContentBrowserEntry.StyleNameOverride = SequencerToolbarStyleName;
			Section.AddEntry(FindInContentBrowserEntry);

			FToolMenuEntry CreateCameraEntry = FToolMenuEntry::InitToolBarButton(FSequencerCommands::Get().CreateCamera);
			CreateCameraEntry.StyleNameOverride = SequencerToolbarStyleName;
			Section.AddEntry(CreateCameraEntry);

			if (Sequencer.Pin()->GetHostCapabilities().bSupportsRenderMovie)
			{
				FToolMenuEntry RenderMovieEntry = FToolMenuEntry::InitToolBarButton(FSequencerCommands::Get().RenderMovie, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.OpenCinematic"));
				RenderMovieEntry.StyleNameOverride = SequencerToolbarStyleName;
				Section.AddEntry(RenderMovieEntry);
			
				FToolMenuEntry RenderMovieOptionsEntry = FToolMenuEntry::InitComboButton(
					"RenderMovieOptions",
					FUIAction(),
					FOnGetContent::CreateSP(SequencerWidget.Get(), &SSequencer::MakeRenderMovieMenu),
					LOCTEXT("RenderMovieOptions", "Render Movie Options"),
					LOCTEXT("RenderMovieOptionsToolTip", "Render Movie Options"),
					TAttribute<FSlateIcon>(),
					true
				);
				RenderMovieOptionsEntry.StyleNameOverride = SequencerToolbarStyleName;
				Section.AddEntry(RenderMovieOptionsEntry);
			}

			UMovieSceneSequence* RootSequence = Sequencer.Pin()->GetRootMovieSceneSequence();
			if (RootSequence->GetTypedOuter<UBlueprint>() == nullptr && UMovieScene::IsTrackClassAllowed(UMovieSceneEventTrack::StaticClass()))
			{
				// Only show this button where it makes sense (ie, if the sequence is not contained within a blueprint already)
				FToolMenuEntry OpenDirectorBlueprintEntry = FToolMenuEntry::InitToolBarButton(FSequencerCommands::Get().OpenDirectorBlueprint, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FAppStyle::Get().GetStyleSetName(), "LevelEditor.OpenLevelBlueprint"));
				OpenDirectorBlueprintEntry.StyleNameOverride = SequencerToolbarStyleName;
				Section.AddEntry(OpenDirectorBlueprintEntry);
			}

			Section.AddSeparator(NAME_None);
		}

		FToolMenuEntry ActionsEntry = FToolMenuEntry::InitComboButton(
			"Actions",
			FUIAction(),
			FOnGetContent::CreateSP(SequencerWidget.Get(), &SSequencer::MakeActionsMenu),
			LOCTEXT("Actions", "Actions"),
			LOCTEXT("ActionsToolTip", "Actions"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Sequencer.Actions")
		);
		ActionsEntry.StyleNameOverride = SequencerToolbarStyleName;
		Section.AddEntry(ActionsEntry);

		FToolMenuEntry ViewOptionsEntry = FToolMenuEntry::InitComboButton(
			"ViewOptions",
			FUIAction(),
			FOnGetContent::CreateSP(SequencerWidget.Get(), &SSequencer::MakeViewMenu),
			LOCTEXT("ViewOptions", "View Options"),
			LOCTEXT("ViewOptionsToolTip", "View Options"),
			FSlateIcon(FAppStyle::Get().GetStyleSetName(), "Icons.Visibility")
		);
		ViewOptionsEntry.StyleNameOverride = SequencerToolbarStyleName;
		Section.AddEntry(ViewOptionsEntry);

		FToolMenuEntry PlaybackOptionsEntry = FToolMenuEntry::InitComboButton(
			"PlaybackOptions",
			FUIAction(),
			FOnGetContent::CreateSP(SequencerWidget.Get(), &SSequencer::MakePlaybackMenu),
			LOCTEXT("PlaybackOptions", "Playback Options"),
			LOCTEXT("PlaybackOptionsToolTip", "Playback Options"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Sequencer.PlaybackOptions")
		);
		PlaybackOptionsEntry.StyleNameOverride = SequencerToolbarStyleName;
		Section.AddEntry(PlaybackOptionsEntry);

		Section.AddSeparator(NAME_None);

		TAttribute<FSlateIcon> KeyGroupModeIcon;
		KeyGroupModeIcon.Bind(TAttribute<FSlateIcon>::FGetter::CreateLambda([Sequencer] 
		{
			switch (Sequencer.Pin()->GetKeyGroupMode())
			{
			case EKeyGroupMode::KeyAll:
				return FSequencerCommands::Get().SetKeyAll->GetIcon();
			case EKeyGroupMode::KeyGroup:
				return FSequencerCommands::Get().SetKeyGroup->GetIcon();
			default: // EKeyGroupMode::KeyChanged
				return FSequencerCommands::Get().SetKeyChanged->GetIcon();
			}
		}));

		TAttribute<FText> KeyGroupModeToolTip;
		KeyGroupModeToolTip.Bind(TAttribute<FText>::FGetter::CreateLambda([Sequencer] 
		{
			switch (Sequencer.Pin()->GetKeyGroupMode())
			{
			case EKeyGroupMode::KeyAll:
				return FSequencerCommands::Get().SetKeyAll->GetDescription();
			case EKeyGroupMode::KeyGroup:
				return FSequencerCommands::Get().SetKeyGroup->GetDescription();
			default: // EKeyGroupMode::KeyChanged
				return FSequencerCommands::Get().SetKeyChanged->GetDescription();
			}
		}));

		FToolMenuEntry KeyGroupEntry = FToolMenuEntry::InitComboButton(
			"KeyGroup",
			FUIAction(),
			FOnGetContent::CreateSP(SequencerWidget.Get(), &SSequencer::MakeKeyGroupMenu),
			LOCTEXT("KeyGroup", "Key All"),
			KeyGroupModeToolTip,
			KeyGroupModeIcon
		);
		KeyGroupEntry.StyleNameOverride = SequencerToolbarStyleName;
		Section.AddEntry(KeyGroupEntry);
	
		if (IVREditorModule::Get().IsVREditorModeActive() || (Sequencer.Pin()->IsLevelEditorSequencer() && ExactCast<ULevelSequence>(Sequencer.Pin()->GetFocusedMovieSceneSequence()) == nullptr))
		{
			TAttribute<FSlateIcon> AutoChangeModeIcon;
			AutoChangeModeIcon.Bind(TAttribute<FSlateIcon>::FGetter::CreateLambda([Sequencer] 
			{
				switch ( Sequencer.Pin()->GetAutoChangeMode() )
				{
				case EAutoChangeMode::AutoKey:
					return FSequencerCommands::Get().SetAutoKey->GetIcon();
				case EAutoChangeMode::AutoTrack:
					return FSequencerCommands::Get().SetAutoTrack->GetIcon();
				case EAutoChangeMode::All:
					return FSequencerCommands::Get().SetAutoChangeAll->GetIcon();
				default: // EAutoChangeMode::None
					return FSequencerCommands::Get().SetAutoChangeNone->GetIcon();
				}
			} ) );

			TAttribute<FText> AutoChangeModeToolTip;
			AutoChangeModeToolTip.Bind( TAttribute<FText>::FGetter::CreateLambda([Sequencer] 
			{
				switch ( Sequencer.Pin()->GetAutoChangeMode() )
				{
				case EAutoChangeMode::AutoKey:
					return FSequencerCommands::Get().SetAutoKey->GetDescription();
				case EAutoChangeMode::AutoTrack:
					return FSequencerCommands::Get().SetAutoTrack->GetDescription();
				case EAutoChangeMode::All:
					return FSequencerCommands::Get().SetAutoChangeAll->GetDescription();
				default: // EAutoChangeMode::None
					return FSequencerCommands::Get().SetAutoChangeNone->GetDescription();
				}
			} ) );
			
			FToolMenuEntry AutoChangeEntry = FToolMenuEntry::InitComboButton(
				"AutoChange",
				FUIAction(),
				FOnGetContent::CreateSP(SequencerWidget.Get(), &SSequencer::MakeAutoChangeMenu),
				LOCTEXT("AutoChangeMode", "Auto-Change Mode"),
				AutoChangeModeToolTip,
				AutoChangeModeIcon
			);
			AutoChangeEntry.StyleNameOverride = SequencerToolbarStyleName;
			Section.AddEntry(AutoChangeEntry);
		}
		else	
		{
			FToolMenuEntry ToggleAutoKeyEntry = FToolMenuEntry::InitToolBarButton(FSequencerCommands::Get().ToggleAutoKeyEnabled);
			ToggleAutoKeyEntry.StyleNameOverride = SequencerToolbarStyleName;
			Section.AddEntry(ToggleAutoKeyEntry);
		}

		if (Sequencer.Pin()->IsLevelEditorSequencer())
		{
			TAttribute<FSlateIcon> AllowEditsModeIcon;
			AllowEditsModeIcon.Bind(TAttribute<FSlateIcon>::FGetter::CreateLambda([Sequencer] 
			{
				switch ( Sequencer.Pin()->GetAllowEditsMode() )
				{
				case EAllowEditsMode::AllEdits:
					return FSequencerCommands::Get().AllowAllEdits->GetIcon();
				case EAllowEditsMode::AllowSequencerEditsOnly:
					return FSequencerCommands::Get().AllowSequencerEditsOnly->GetIcon();
				default: // EAllowEditsMode::AllowLevelEditsOnly
					return FSequencerCommands::Get().AllowLevelEditsOnly->GetIcon();
				}
			} ) );

			TAttribute<FText> AllowEditsModeToolTip;
			AllowEditsModeToolTip.Bind( TAttribute<FText>::FGetter::CreateLambda([Sequencer] 
			{
				switch ( Sequencer.Pin()->GetAllowEditsMode() )
				{
				case EAllowEditsMode::AllEdits:
					return FSequencerCommands::Get().AllowAllEdits->GetDescription();
				case EAllowEditsMode::AllowSequencerEditsOnly:
					return FSequencerCommands::Get().AllowSequencerEditsOnly->GetDescription();
				default: // EAllowEditsMode::AllowLevelEditsOnly
					return FSequencerCommands::Get().AllowLevelEditsOnly->GetDescription();
				}
			} ) );

			FToolMenuEntry AllowEditsModeEntry = FToolMenuEntry::InitComboButton(
				"AllowEditsMode",
				FUIAction(),
				FOnGetContent::CreateSP(SequencerWidget.Get(), &SSequencer::MakeAllowEditsMenu),
				LOCTEXT("AllowEditsMode", "Allow Edits"),
				AllowEditsModeToolTip,
				AllowEditsModeIcon
			);
			AllowEditsModeEntry.StyleNameOverride = SequencerToolbarStyleName;
			Section.AddEntry(AllowEditsModeEntry);
		}

		Section.AddSeparator(NAME_None);
	}

	{
		FToolMenuSection& SnappingSection = InMenu->AddSection("Snapping");

		FToolMenuEntry ToggleIsSnapEnabledEntry = FToolMenuEntry::InitToolBarButton(FSequencerCommands::Get().ToggleIsSnapEnabled, TAttribute<FText>(FText::GetEmpty()));
		ToggleIsSnapEnabledEntry.StyleNameOverride = SequencerToolbarStyleName;
		SnappingSection.AddEntry(ToggleIsSnapEnabledEntry);

		FToolMenuEntry SnapOptionsEntry = FToolMenuEntry::InitComboButton(
			"SnapOptions",
			FUIAction(),
			FOnGetContent::CreateSP(SequencerWidget.Get(), &SSequencer::MakeSnapMenu),
			LOCTEXT("SnapOptions", "Options"),
			LOCTEXT("SnapOptionsToolTip", "Snapping Options"),
			TAttribute<FSlateIcon>(),
			true
		);
		SnapOptionsEntry.StyleNameOverride = SequencerToolbarStyleName;
		SnappingSection.AddEntry(SnapOptionsEntry);

		SnappingSection.AddSeparator(NAME_None);

		FToolMenuEntry PlayRateEntry = FToolMenuEntry::InitWidget(
			"PlayRate",
			SNew(SSequencerPlayRateCombo, Sequencer.Pin(), SequencerWidget.ToSharedRef()),
			LOCTEXT("PlayRate", "PlayRate")
		);
		PlayRateEntry.StyleNameOverride = SequencerToolbarStyleName;
		SnappingSection.AddEntry(PlayRateEntry);
	}

	{
		FToolMenuSection& CurveEditorSection = InMenu->AddSection("CurveEditor");

		// Only add the button if supported
		if (Sequencer.Pin()->GetHostCapabilities().bSupportsCurveEditor)
		{
			FToolMenuEntry ShowCurveEditorEntry = FToolMenuEntry::InitToolBarButton(FSequencerCommands::Get().ToggleShowCurveEditor, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCurveEditor"));
			ShowCurveEditorEntry.StyleNameOverride = SequencerToolbarStyleName;
			CurveEditorSection.AddEntry(ShowCurveEditorEntry);
		}
	}
}

TSharedRef<SWidget> SSequencer::MakeAddMenu()
{
	using namespace UE::Sequencer;

	TSharedPtr<FExtender> Extender = FExtender::Combine(AddMenuExtenders);
	FMenuBuilder MenuBuilder(true, nullptr, Extender);
	{
		TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();
		Sequencer->GetViewModel()->GetOutliner()->CastThisChecked<FSequencerOutlinerViewModel>()->BuildContextMenu(MenuBuilder);
	}

	return MenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SSequencer::MakeFilterMenu()
{
	TSharedPtr<FExtender> Extender = FExtender::Combine(AddMenuExtenders);
	FMenuBuilder MenuBuilder(false, SequencerPtr.Pin()->GetCommandBindings(), Extender);

	// let track editors & object bindings populate the menu
	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();

	MenuBuilder.BeginSection("SequencerTracksResetFilters");
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("FilterListEnableAll", "Enable All"),
			LOCTEXT("FilterListEnableAllToolTip", "Selects all filters"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &SSequencer::OnEnableAllFilters)));

		MenuBuilder.AddMenuEntry(FSequencerCommands::Get().ResetFilters);
	}
	MenuBuilder.EndSection();

	UMovieSceneSequence* FocusedMovieSequence = Sequencer->GetFocusedMovieSceneSequence();
	UMovieScene* FocusedMovieScene = nullptr;
	if (IsValid(FocusedMovieSequence))
	{
		FocusedMovieScene = FocusedMovieSequence->GetMovieScene();
		if (IsValid(FocusedMovieScene))
		{
			if (FocusedMovieScene->GetNodeGroups().Num() > 0)
			{
				MenuBuilder.BeginSection("NodeGroupFilters");
				MenuBuilder.AddSubMenu(LOCTEXT("NodeGroupFilters", "Group Filters"), LOCTEXT("NodeGroupFiltersToolTip", "Enables using this group as a filter for all items"), FNewMenuDelegate::CreateRaw(this, &SSequencer::FillNodeGroupsFilterMenu), false, FSlateIcon(), false);
				MenuBuilder.EndSection();
			}
		}
	}

	UObject* PlaybackContext = Sequencer->GetPlaybackContext();
	UWorld* World = PlaybackContext ? PlaybackContext->GetWorld() : nullptr;

	if (World && World->GetLevels().Num() > 1)
	{
		MenuBuilder.BeginSection("TrackLevelFilters");
		MenuBuilder.AddSubMenu(LOCTEXT("LevelFilters", "Level Filters"), LOCTEXT("LevelFiltersToolTip", "Filter object tracks by level"), FNewMenuDelegate::CreateRaw(this, &SSequencer::FillLevelFilterMenu), false, FSlateIcon(), false);
		MenuBuilder.EndSection();
	}

	MenuBuilder.BeginSection("TrackFilters");
	
	for (TSharedRef<FSequencerTrackFilter> TrackFilter : AllTrackFilters)
	{
		if (TrackFilter->SupportsSequence(Sequencer->GetFocusedMovieSceneSequence()))
		{
			MenuBuilder.AddMenuEntry(
				TrackFilter->GetDisplayName(),
				TrackFilter->GetToolTipText(),
				TrackFilter->GetIcon(),
				FUIAction(
				FExecuteAction::CreateSP(this, &SSequencer::OnTrackFilterClicked, TrackFilter),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP(this, &SSequencer::IsTrackFilterActive, TrackFilter)),
				NAME_None,
				EUserInterfaceActionType::ToggleButton
			);
		}
	}

	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SSequencer::FillNodeGroupsFilterMenu(FMenuBuilder& InMenuBarBuilder)
{
	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();
	UMovieSceneSequence* FocusedMovieSequence = Sequencer->GetFocusedMovieSceneSequence();
	UMovieScene* FocusedMovieScene = nullptr;

	if (IsValid(FocusedMovieSequence))
	{
		FocusedMovieScene = FocusedMovieSequence->GetMovieScene();
		if (IsValid(FocusedMovieScene))
		{
			InMenuBarBuilder.BeginSection("SequencerTracksResetNodeGroupFilters");

			InMenuBarBuilder.AddMenuEntry(
				LOCTEXT("EnableAllNodeGroupFilters", "Enable All"),
				LOCTEXT("EnableAllNodeGroupFiltersToolTip", "Enables all group filters"),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &SSequencer::OnEnableAllNodeGroupFilters, true)));

			InMenuBarBuilder.AddMenuEntry(
				LOCTEXT("ResetNodeGroupFilters", "Reset Filters"),
				LOCTEXT("ResetNodeGroupFiltersToolTip", "Disables all group filters"),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &SSequencer::OnEnableAllNodeGroupFilters, false))
			);
			
			InMenuBarBuilder.EndSection();

			bool bIsReadOnly = GetIsSequenceReadOnly();

			InMenuBarBuilder.BeginSection("SequencerTracksNodeGroupFilters");
			for (UMovieSceneNodeGroup* NodeGroup : FocusedMovieScene->GetNodeGroups())
			{
				InMenuBarBuilder.AddMenuEntry(
					FText::FromName(NodeGroup->GetName()),
					FText::FromName(NodeGroup->GetName()),
					FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateSP(this, &SSequencer::OnNodeGroupFilterClicked, NodeGroup),
						FCanExecuteAction::CreateLambda([bIsReadOnly]() { return !bIsReadOnly; }),
						FIsActionChecked::CreateLambda([NodeGroup]() { return NodeGroup->GetEnableFilter(); })),
					NAME_None,
					EUserInterfaceActionType::ToggleButton
				);

			}
			InMenuBarBuilder.EndSection();
		}
	}
}

void SSequencer::OnEnableAllNodeGroupFilters(bool bEnableAll)
{
	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();
	UMovieSceneSequence* FocusedMovieSequence = Sequencer->GetFocusedMovieSceneSequence();
	UMovieScene* FocusedMovieScene = nullptr;
	if (IsValid(FocusedMovieSequence))
	{
		FocusedMovieScene = FocusedMovieSequence->GetMovieScene();
		if (IsValid(FocusedMovieScene))
		{
			for (UMovieSceneNodeGroup* NodeGroup : FocusedMovieScene->GetNodeGroups())
			{
				NodeGroup->SetEnableFilter(bEnableAll);
			}
		}
	}
}

void SSequencer::OnNodeGroupFilterClicked(UMovieSceneNodeGroup* NodeGroup)
{
	if (NodeGroup)
	{
		NodeGroup->SetEnableFilter(!NodeGroup->GetEnableFilter());
	}
	
}

void SSequencer::FillLevelFilterMenu(FMenuBuilder& InMenuBarBuilder)
{
	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();
	UObject* PlaybackContext = Sequencer->GetPlaybackContext();
	UWorld* World = PlaybackContext ? PlaybackContext->GetWorld() : nullptr;

	if (World)
	{
		const TArray<ULevel*> Levels = World->GetLevels();

		if (Levels.Num() > 0)
		{
			InMenuBarBuilder.BeginSection("SequencerTracksResetLevelFilters");

			InMenuBarBuilder.AddMenuEntry(
				LOCTEXT("EnableAllLevelFilters", "Enable All"),
				LOCTEXT("EnableAllLevelFiltersToolTip", "Enables all level filters"),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &SSequencer::OnEnableAllLevelFilters, true)));

			InMenuBarBuilder.AddMenuEntry(
				LOCTEXT("ResetLevelFilters", "Reset Filters"),
				LOCTEXT("ResetLevelFiltersToolTip", "Resets current level filter selection"),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &SSequencer::OnEnableAllLevelFilters, false)));

			InMenuBarBuilder.EndSection();
		}

		InMenuBarBuilder.BeginSection("SequencerTracksLevelFilters");
		for (ULevel* Level : Levels)
		{
			FString LevelName = FPackageName::GetShortName(Level->GetOutermost()->GetName());
			InMenuBarBuilder.AddMenuEntry(
				FText::FromString(LevelName),
				FText::FromString(Level->GetOutermost()->GetName()),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP(this, &SSequencer::OnTrackLevelFilterClicked, LevelName),
					FCanExecuteAction(),
					FIsActionChecked::CreateSP(this, &SSequencer::IsTrackLevelFilterActive, LevelName)),
				NAME_None,
				EUserInterfaceActionType::ToggleButton
			);
		}
		InMenuBarBuilder.EndSection();
	}
}

void SSequencer::SetTrackFilterEnabled(const FText& InTrackFilterName, bool bEnabled)
{
	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();

	for (TSharedRef<FSequencerTrackFilter> TrackFilter : AllTrackFilters)
	{
		if (TrackFilter->SupportsSequence(Sequencer->GetFocusedMovieSceneSequence()))
		{
			if (InTrackFilterName.EqualToCaseIgnored(TrackFilter->GetDisplayName()))
			{
				if (bEnabled && !IsTrackFilterActive(TrackFilter))
				{
					Sequencer->GetNodeTree()->AddFilter(TrackFilter);
				}
				else if (!bEnabled && IsTrackFilterActive(TrackFilter))
				{
					Sequencer->GetNodeTree()->RemoveFilter(TrackFilter);
				}
			}
		}
	}
}

bool SSequencer::IsTrackFilterEnabled(const FText& InTrackFilterName) const
{
	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();

	for (TSharedRef<FSequencerTrackFilter> TrackFilter : AllTrackFilters)
	{
		if (TrackFilter->SupportsSequence(Sequencer->GetFocusedMovieSceneSequence()))
		{
			if (InTrackFilterName.EqualToCaseIgnored(TrackFilter->GetDisplayName()))
			{
				return Sequencer->GetNodeTree()->IsTrackFilterActive(TrackFilter);
			}
		}
	}

	return false;
}

TArray<FText> SSequencer::GetTrackFilterNames() const
{
	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();

	TArray<FText> Filters;
	for (TSharedRef<FSequencerTrackFilter> TrackFilter : AllTrackFilters)
	{
		if (TrackFilter->SupportsSequence(Sequencer->GetFocusedMovieSceneSequence()))
		{
			Filters.Add(TrackFilter->GetDisplayName());
		}
	}

	return Filters;
}

void SSequencer::ResetFilters()
{
	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();
	Sequencer->GetNodeTree()->RemoveAllFilters();
	OnEnableAllNodeGroupFilters(false);
}

void SSequencer::OnEnableAllFilters()
{
	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();

	for (TSharedRef<FSequencerTrackFilter> TrackFilter : AllTrackFilters)
	{
		if (TrackFilter->SupportsSequence(Sequencer->GetFocusedMovieSceneSequence()))
		{
			if (!Sequencer->GetNodeTree()->IsTrackFilterActive(TrackFilter))
			{
				Sequencer->GetNodeTree()->AddFilter(TrackFilter);
			}
		}
	}
	OnEnableAllNodeGroupFilters(true);
	OnEnableAllLevelFilters(true);
}

void SSequencer::OnTrackFilterClicked(TSharedRef<FSequencerTrackFilter> TrackFilter)
{
	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();
	if (IsTrackFilterActive(TrackFilter))
	{
		Sequencer->GetNodeTree()->RemoveFilter(TrackFilter);
	}
	else
	{
		Sequencer->GetNodeTree()->AddFilter(TrackFilter);
	}
}

bool SSequencer::IsTrackFilterActive(TSharedRef<FSequencerTrackFilter> TrackFilter) const
{
	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();
	return Sequencer->GetNodeTree()->IsTrackFilterActive(TrackFilter);
}

void SSequencer::OnEnableAllLevelFilters(bool bEnableAll)
{
	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();
	UObject* PlaybackContext = Sequencer->GetPlaybackContext();
	UWorld* World = PlaybackContext ? PlaybackContext->GetWorld() : nullptr;

	if (World)
	{
		const TArray<ULevel*> Levels = World->GetLevels();

		for (ULevel* Level : Levels)
		{
			FString LevelName = FPackageName::GetShortName(Level->GetOutermost()->GetName());

			if (bEnableAll)
			{
				Sequencer->GetNodeTree()->AddLevelFilter(LevelName);
			}
			else
			{
				Sequencer->GetNodeTree()->RemoveLevelFilter(LevelName);
			}
		}
	}
}

void SSequencer::OnTrackLevelFilterClicked(const FString LevelName)
{
	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();
	if (IsTrackLevelFilterActive(LevelName))
	{
		Sequencer->GetNodeTree()->RemoveLevelFilter(LevelName);
	}
	else
	{
		Sequencer->GetNodeTree()->AddLevelFilter(LevelName);
	}
}

bool SSequencer::IsTrackLevelFilterActive(const FString LevelName) const
{
	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();
	return Sequencer->GetNodeTree()->IsTrackLevelFilterActive(LevelName);
}


TSharedRef<SWidget> SSequencer::MakeActionsMenu()
{
	using namespace UE::Sequencer;

	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();
	TSharedPtr<FSequencerEditorViewModel> EditorViewModel = Sequencer->GetViewModel();

	ISequencerModule& SequencerModule = FModuleManager::GetModuleChecked<ISequencerModule>("Sequencer");
	TSharedPtr<FExtender> ActionsMenuExtender = SequencerModule.GetActionsMenuExtensibilityManager()->GetAllExtenders();

	FMenuBuilder MenuBuilder(true, Sequencer->GetCommandBindings(), ActionsMenuExtender);

	MenuBuilder.BeginSection("SequenceOptions", LOCTEXT("SequenceOptionsHeader", "Sequence"));
	{
		UMovieSceneSequence* RootSequence = Sequencer->GetRootMovieSceneSequence();
		if (RootSequence->GetTypedOuter<UBlueprint>() == nullptr)
		{
			// Only show this button where it makes sense (ie, if the sequence is not contained within a blueprint already)
			MenuBuilder.AddMenuEntry(FSequencerCommands::Get().OpenDirectorBlueprint, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FAppStyle::Get().GetStyleSetName(), "LevelEditor.OpenLevelBlueprint"));
		}

		MenuBuilder.AddMenuEntry(FSequencerCommands::Get().OpenTaggedBindingManager);
		MenuBuilder.AddMenuEntry(FSequencerCommands::Get().OpenNodeGroupsManager);

		MenuBuilder.AddMenuEntry(FSequencerCommands::Get().RestoreAnimatedState);

		MenuBuilder.AddSubMenu(LOCTEXT("AdvancedHeader", "Advanced"), FText::GetEmpty(), FNewMenuDelegate::CreateRaw(this, &SSequencer::FillAdvancedMenu));
	}
	MenuBuilder.EndSection();

	// transform actions
	MenuBuilder.BeginSection("Transform", LOCTEXT("TransformHeader", "Transform"));
	
	MenuBuilder.AddMenuEntry(FSequencerCommands::Get().ToggleShowTransformBox);
	MenuBuilder.AddMenuEntry(FSequencerCommands::Get().ToggleShowStretchBox);
	MenuBuilder.AddMenuEntry(FSequencerCommands::Get().TranslateLeft);
	MenuBuilder.AddMenuEntry(FSequencerCommands::Get().TranslateRight);
	MenuBuilder.AddMenuEntry(FSequencerCommands::Get().TrimOrExtendSectionLeft);
	MenuBuilder.AddMenuEntry(FSequencerCommands::Get().TrimOrExtendSectionRight);

	MenuBuilder.EndSection();

	// selection range actions
	MenuBuilder.BeginSection("SelectionRange", LOCTEXT("SelectionRangeHeader", "Selection Range"));
	{
		MenuBuilder.AddMenuEntry(FSequencerCommands::Get().SetSelectionRangeStart);
		MenuBuilder.AddMenuEntry(FSequencerCommands::Get().SetSelectionRangeEnd);
		MenuBuilder.AddMenuEntry(FSequencerCommands::Get().ClearSelectionRange);
		MenuBuilder.AddMenuEntry(FSequencerCommands::Get().SelectKeysInSelectionRange);
		MenuBuilder.AddMenuEntry(FSequencerCommands::Get().SelectSectionsInSelectionRange);
		MenuBuilder.AddMenuEntry(FSequencerCommands::Get().SelectAllInSelectionRange);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SSequencer::FillAdvancedMenu(FMenuBuilder& MenuBuilder)
{
	if (SequencerPtr.Pin()->IsLevelEditorSequencer())
	{
		MenuBuilder.BeginSection("Bindings", LOCTEXT("BindingsMenuHeader", "Bindings"));

		MenuBuilder.AddMenuEntry(FSequencerCommands::Get().RebindPossessableReferences);

		MenuBuilder.EndSection();
	}

	MenuBuilder.BeginSection( "NetworkingOptions", LOCTEXT( "NetworkingOptionsHeader", "Networking" ) );
	{
		auto SetNetworkMode = [WeakSequencer = SequencerPtr](EMovieSceneServerClientMask InMode)
		{
			TSharedPtr<FSequencer> SequencerPin = WeakSequencer.Pin();
			if (SequencerPin)
			{
				// When changing the enumlated network mode, we have to re-initialize and re-compile the 
				// sequence data to ensure that the emulation is reading the correct client/server/all data
				FMovieSceneRootEvaluationTemplateInstance& Template = SequencerPin->GetEvaluationTemplate();

				UMovieSceneSequence* RootSequence = Template.GetRootSequence();

				// Set the new emulation mode
				Template.SetEmulatedNetworkMask(InMode);
				// Since sequencer owns its own compiled data manager, it's ok to override the mask here and reset everything
				Template.GetCompiledDataManager()->SetEmulatedNetworkMask(InMode);
				// Reinitialize the template again
				Template.Initialize(*RootSequence, *SequencerPin, Template.GetCompiledDataManager(), Template.GetRunner());
			}
		};
		auto IsNetworkModeChecked = [WeakSequencer = SequencerPtr](EMovieSceneServerClientMask InMode)
		{
			TSharedPtr<FSequencer> SequencerPin = WeakSequencer.Pin();
			if (SequencerPin)
			{
				return InMode == SequencerPin->GetEvaluationTemplate().GetEmulatedNetworkMask();
			}
			return false;
		};

		MenuBuilder.AddMenuEntry(
			LOCTEXT("NetworkEmulationAllLabel", "Do not emulate (default)"),
			LOCTEXT("NetworkEmulationAllTooltip", "Play this sequence with all sub sequences, regardless of their network mask."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda(SetNetworkMode, EMovieSceneServerClientMask::All),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda(IsNetworkModeChecked, EMovieSceneServerClientMask::All)
			),
			NAME_None,
			EUserInterfaceActionType::RadioButton);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("NetworkEmulationClientLabel", "Emulate as Client"),
			LOCTEXT("NetworkEmulationClientTooltip", "Plays this sequence as if it were being played on a client (excludes server only cinematics)."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda(SetNetworkMode, EMovieSceneServerClientMask::Client),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda(IsNetworkModeChecked, EMovieSceneServerClientMask::Client)
			),
			NAME_None,
			EUserInterfaceActionType::RadioButton);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("NetworkEmulationServerLabel", "Emulate as Server"),
			LOCTEXT("NetworkEmulationServerTooltip", "Plays this sequence as if it were being played on a server (excludes client only cinematics)."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda(SetNetworkMode, EMovieSceneServerClientMask::Server),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda(IsNetworkModeChecked, EMovieSceneServerClientMask::Server)
			),
			NAME_None,
			EUserInterfaceActionType::RadioButton);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection( "VolatilityOptions", LOCTEXT( "VolatilityOptionsHeader", "Volatility" ) );
	{
		auto ToggleVolatility = [WeakSequencer = SequencerPtr](EMovieSceneSequenceFlags InFlags)
		{
			TSharedPtr<FSequencer> SequencerPin = WeakSequencer.Pin();
			if (SequencerPin)
			{
				const FScopedTransaction Transaction(LOCTEXT("ToggleVolatility", "Toggle Volatility"));

				UMovieSceneSequence* RootSequence = SequencerPin->GetRootMovieSceneSequence();

				RootSequence->Modify();

				RootSequence->SetSequenceFlags(RootSequence->GetFlags() ^ InFlags);
			}
		};
		auto IsVolatilityChecked = [WeakSequencer = SequencerPtr](EMovieSceneSequenceFlags InFlags)
		{
			TSharedPtr<FSequencer> SequencerPin = WeakSequencer.Pin();
			if (SequencerPin)
			{
				UMovieSceneSequence* RootSequence = SequencerPin->GetRootMovieSceneSequence();
				return ((uint8)RootSequence->GetFlags() & (uint8)InFlags) != 0;
			}
			return false;
		};

		MenuBuilder.AddMenuEntry(
			LOCTEXT("VolatilityVolatileLabel", "Volatile"),
			LOCTEXT("VolatilityVolatileTooltip", "Flag signifying that this sequence can change dynamically at runtime or during the game so the template must be checked for validity and recompiled as necessary before each evaluation.  The absence of this flag will result in the same compiled data being used for the duration of the program, as well as being pre-built during cook. As such, any dynamic changes to the sequence will not be reflected in the evaluation itself. This flag *must* be set if *any* procedural changes will be made to the source sequence data in-game."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda(ToggleVolatility, EMovieSceneSequenceFlags::Volatile),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda(IsVolatilityChecked, EMovieSceneSequenceFlags::Volatile)
			),
			NAME_None,
			EUserInterfaceActionType::ToggleButton);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("VolatilityBlockingEvaluationLabel", "Blocking Evaluation"),
			LOCTEXT("VolatilityBlockingEvaluationTooltip", "Indicates that a sequence must fully evaluate and apply its state every time it is updated, blocking until complete. Should be used sparingly as it will severely affect performance."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda(ToggleVolatility, EMovieSceneSequenceFlags::BlockingEvaluation),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda(IsVolatilityChecked, EMovieSceneSequenceFlags::BlockingEvaluation)
			),
			NAME_None,
			EUserInterfaceActionType::ToggleButton);
	}
	MenuBuilder.EndSection();
}

TSharedRef<SWidget> SSequencer::MakeViewMenu()
{
	FMenuBuilder MenuBuilder( true, SequencerPtr.Pin()->GetCommandBindings() );
	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();

	if (Sequencer->IsLevelEditorSequencer())
	{
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().TogglePilotCamera );
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleRestoreOriginalViewportOnCameraCutUnlock );
	}

	MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleAutoScroll );
	MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleShowRangeSlider );
	MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleShowSelectedNodesOnly );
	MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleLayerBars );
	MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleKeyBars );
	MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleChannelColors );
	MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleShowInfoButton );
	MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleShowPreAndPostRoll );

	// Menu entry for zero padding
	auto OnZeroPadChanged = [this](uint8 NewValue) {
		GetSequencerSettings()->SetZeroPadFrames(NewValue);
	};

	MenuBuilder.AddSubMenu(LOCTEXT("ViewDensityMenuLabel", "View Density"), FText::GetEmpty(), FNewMenuDelegate::CreateRaw(this, &SSequencer::FillViewDensityMenu));

	// Menu Entry for Outliner Column Visibilities
	if (OutlinerColumnVisibilities.Num() > 0)
	{
		MenuBuilder.AddSubMenu(LOCTEXT("ColumnVisibilityHeader", "Columns"), FText::GetEmpty(), FNewMenuDelegate::CreateRaw(this, &SSequencer::FillColumnVisibilityMenu));
	}

	MenuBuilder.AddWidget(
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
			[
				SNew(SSpacer)
			]
		+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SSpinBox<uint8>)
				.Style(&FAppStyle::GetWidgetStyle<FSpinBoxStyle>("Sequencer.HyperlinkSpinBox"))
			.OnValueCommitted_Lambda([=](uint8 Value, ETextCommit::Type) { OnZeroPadChanged(Value); })
			.OnValueChanged_Lambda(OnZeroPadChanged)
			.MinValue(0)
			.MaxValue(8)
			.Value_Lambda([this]() -> uint8 {
			return GetSequencerSettings()->GetZeroPadFrames();
		})
		],
		LOCTEXT("ZeroPaddingText", "Zero Pad Frame Numbers"));

	MenuBuilder.BeginSection("OrganizeAndSort", LOCTEXT("OrganizeAndSortHeader", "Organize and Sort"));
	{
		MenuBuilder.AddMenuEntry(FSequencerCommands::Get().ToggleAutoExpandNodesOnSelection);
		MenuBuilder.AddMenuEntry(FSequencerCommands::Get().ToggleExpandCollapseNodes);
		MenuBuilder.AddMenuEntry(FSequencerCommands::Get().ToggleExpandCollapseNodesAndDescendants);
		MenuBuilder.AddMenuEntry(FSequencerCommands::Get().ExpandAllNodes);
		MenuBuilder.AddMenuEntry(FSequencerCommands::Get().CollapseAllNodes);
		MenuBuilder.AddMenuEntry(FSequencerCommands::Get().SortAllNodesAndDescendants);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("MarkedFrames", LOCTEXT("MarkedFramesHeader", "Marked Frames"));
	{
		MenuBuilder.AddMenuEntry(FSequencerCommands::Get().ToggleShowMarkedFramesGlobally);
		MenuBuilder.AddMenuEntry(FSequencerCommands::Get().ClearGlobalMarkedFrames);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SSequencer::OpenTaggedBindingManager()
{
	if (TSharedPtr<SWindow> Window = WeakExposedBindingsWindow.Pin())
	{
		Window->DrawAttention(FWindowDrawAttentionParameters());
		return;
	}

	TSharedRef<SWindow> ExposedBindingsWindow = SNew(SWindow)
		.Title(FText::Format(LOCTEXT("ExposedBindings_Title", "Bindings Exposed in {0}"), FText::FromName(SequencerPtr.Pin()->GetRootMovieSceneSequence()->GetFName())))
		.SupportsMaximize(false)
		.ClientSize(FVector2D(600.f, 500.f))
		.Content()
		[
			SNew(SObjectBindingTagManager, SequencerPtr)
		];

	TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
	if (ParentWindow)
	{
		FSlateApplication::Get().AddWindowAsNativeChild(ExposedBindingsWindow, ParentWindow.ToSharedRef());
	}
	else
	{
		FSlateApplication::Get().AddWindow(ExposedBindingsWindow);
	}

	WeakExposedBindingsWindow = ExposedBindingsWindow;
}

void SSequencer::OpenNodeGroupsManager()
{
	if (TSharedPtr<SWindow> Window = WeakNodeGroupWindow.Pin())
	{
		Window->DrawAttention(FWindowDrawAttentionParameters());
		return;
	}

	TSharedRef<SWindow> NodeGroupManagerWindow = SNew(SWindow)
		.Title(FText::Format(LOCTEXT("NodeGroup_Title", "Groups in {0}"), FText::FromName(SequencerPtr.Pin()->GetRootMovieSceneSequence()->GetFName())))
		.SupportsMaximize(false)
		.ClientSize(FVector2D(600.f, 500.f))
		.Content()
		[
			SAssignNew(NodeGroupManager, SSequencerGroupManager, SequencerPtr)
		];

	TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
	if (ParentWindow)
	{
		FSlateApplication::Get().AddWindowAsNativeChild(NodeGroupManagerWindow, ParentWindow.ToSharedRef());
	}
	else
	{
		FSlateApplication::Get().AddWindow(NodeGroupManagerWindow);
	}

	WeakNodeGroupWindow = NodeGroupManagerWindow;
}

void SSequencer::FillPlaybackSpeedMenu(FMenuBuilder& InMenuBarBuilder)
{
	TArray<float> PlaybackSpeeds = OnGetPlaybackSpeeds.Execute();

	InMenuBarBuilder.BeginSection("PlaybackSpeed");
	for( int32 PlaybackSpeedIndex = 0; PlaybackSpeedIndex < PlaybackSpeeds.Num(); ++PlaybackSpeedIndex )
	{
		float PlaybackSpeed = PlaybackSpeeds[PlaybackSpeedIndex];
		const FText MenuStr = FText::Format( LOCTEXT("PlaybackSpeedStr", "{0}"), FText::AsNumber( PlaybackSpeed ) );
		InMenuBarBuilder.AddMenuEntry(MenuStr, FText(), FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda( [this, PlaybackSpeed]
				{
					SequencerPtr.Pin()->SetPlaybackSpeed(PlaybackSpeed);
				}),
				FCanExecuteAction::CreateLambda([] { return true; }),
				FIsActionChecked::CreateLambda( [this, PlaybackSpeed]{ return SequencerPtr.Pin()->GetPlaybackSpeed() == PlaybackSpeed; })
				),
			NAME_None,
			EUserInterfaceActionType::RadioButton
			);
	}
	InMenuBarBuilder.EndSection();
}

void SSequencer::FillViewDensityMenu(FMenuBuilder& InMenuBuilder)
{
	using namespace UE::Sequencer;

	auto SetViewDensity = [this](EViewDensity InViewDensity){
		TSharedPtr<FEditorViewModel> Editor = this->SequencerPtr.Pin()->GetViewModel();
		Editor->SetViewDensity(InViewDensity);

		if (USequencerSettings* Settings = this->GetSequencerSettings())
		{
			if (InViewDensity == EViewDensity::Compact)
			{
				Settings->SetViewDensity("Compact");
			}
			else if (InViewDensity == EViewDensity::Relaxed)
			{
				Settings->SetViewDensity("Relaxed");
			}
			else
			{
				Settings->SetViewDensity("Variable");
			}
		}
	};
	auto IsCurrentViewDensity = [this](EViewDensity InViewDensity){
		TSharedPtr<FEditorViewModel> Editor = this->SequencerPtr.Pin()->GetViewModel();
		return Editor->GetViewDensity().Density == InViewDensity;
	};

	InMenuBuilder.AddMenuEntry(
		LOCTEXT("VariableViewDensity_Label", "Variable"),
		LOCTEXT("VariableViewDensity_Tooltip", "Change Sequencer to use a variable height view mode withe inner items displaying more condensed than outer items"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda(SetViewDensity, EViewDensity::Variable),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda(IsCurrentViewDensity, EViewDensity::Variable)
		),
		NAME_None,
		EUserInterfaceActionType::RadioButton
	);

	InMenuBuilder.AddMenuEntry(
		LOCTEXT("CompactViewDensity_Label", "Compact"),
		LOCTEXT("CompactViewDensity_Tooltip", "Change Sequencer to use a compact view mode with uniform track heights"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda(SetViewDensity, EViewDensity::Compact),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda(IsCurrentViewDensity, EViewDensity::Compact)
		),
		NAME_None,
		EUserInterfaceActionType::RadioButton
	);

	InMenuBuilder.AddMenuEntry(
		LOCTEXT("RelaxedViewDensity_Label", "Relaxed"),
		LOCTEXT("RelaxedViewDensity_Tooltip", "Change Sequencer to use a relaxed view mode with larger uniform track heights"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda(SetViewDensity, EViewDensity::Relaxed),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda(IsCurrentViewDensity, EViewDensity::Relaxed)
		),
		NAME_None,
		EUserInterfaceActionType::RadioButton
	);
}

void SSequencer::FillColumnVisibilityMenu(FMenuBuilder& InMenuBuilder)
{
	using namespace UE::Sequencer;

	const bool bShouldCloseWindowAfterMenuSelection = true;

	for (FSequencerOutlinerColumnVisibility& ColumnVisibility : OutlinerColumnVisibilities)
	{
		if (EnumHasAnyFlags(ColumnVisibility.Column->GetLayout().Flags, EOutlinerColumnFlags::Hidden))
		{
			continue;
		}

		auto ToggleVisibility = [this, &ColumnVisibility]
		{
			ColumnVisibility.bIsColumnVisible = !ColumnVisibility.bIsColumnVisible;
			if(ColumnVisibility.bIsColumnVisible)
			{
				FName ColumnName = ColumnVisibility.Column->GetColumnName();
				FSequencerOutlinerColumnVisibility* AutoDisable = nullptr;

				// Auto disable mutually exclusive columns
				if (ColumnName == FCommonOutlinerNames::Nav)
				{
					AutoDisable = Algo::FindBy(this->OutlinerColumnVisibilities, FCommonOutlinerNames::KeyFrame, [](const FSequencerOutlinerColumnVisibility& In) { return In.Column->GetColumnName(); });
				}
				else if (ColumnName == FCommonOutlinerNames::KeyFrame)
				{
					AutoDisable = Algo::FindBy(this->OutlinerColumnVisibilities, FCommonOutlinerNames::Nav, [](const FSequencerOutlinerColumnVisibility& In) { return In.Column->GetColumnName(); });
				}

				if (AutoDisable)
				{
					AutoDisable->bIsColumnVisible = false;
				}
			}
			
			this->UpdateOutlinerViewColumns();
		};


		InMenuBuilder.AddMenuEntry(
			ColumnVisibility.Column->GetColumnLabel(),
			LOCTEXT("SetColumnVisibilityTooltip", "Enable or disable this outliner column"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda(ToggleVisibility),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([&ColumnVisibility] { 
						// Capture by ref here since the array itself cannot be re-allocated while this menu is open
						return ColumnVisibility.bIsColumnVisible;
					})
				),
				NAME_None,
				EUserInterfaceActionType::ToggleButton
			);
	}
}

void SSequencer::FillTimeDisplayFormatMenu(FMenuBuilder& MenuBuilder)
{
	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();
	bool bShouldDisplayDropFormat = FTimecode::UseDropFormatTimecode(Sequencer->GetFocusedDisplayRate());

	const UEnum* FrameNumberDisplayEnum = StaticEnum<EFrameNumberDisplayFormats>();
	check(FrameNumberDisplayEnum);

	if (GetSequencerSettings())
	{
		for (int32 Index = 0; Index < FrameNumberDisplayEnum->NumEnums() - 1; Index++)
		{
			if (!FrameNumberDisplayEnum->HasMetaData(TEXT("Hidden"), Index))
			{
				EFrameNumberDisplayFormats Value = (EFrameNumberDisplayFormats)FrameNumberDisplayEnum->GetValueByIndex(Index);

				// Don't show None Drop Frame Timecode when the format support drop format and the engine wants to use the drop format by default.
				if (Value == EFrameNumberDisplayFormats::NonDropFrameTimecode && bShouldDisplayDropFormat)
				{
					continue;
				}

				// Don't show Drop Frame Timecode when they're in a format that doesn't support it.
				if (Value == EFrameNumberDisplayFormats::DropFrameTimecode && !bShouldDisplayDropFormat)
				{
					continue;
				}

				MenuBuilder.AddMenuEntry(
					FrameNumberDisplayEnum->GetDisplayNameTextByIndex(Index),
					FrameNumberDisplayEnum->GetToolTipTextByIndex(Index),
					FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateUObject(GetSequencerSettings(), &USequencerSettings::SetTimeDisplayFormat, Value),
						FCanExecuteAction(),
						FIsActionChecked::CreateLambda([this, Value] { return GetSequencerSettings()->GetTimeDisplayFormat() == Value; })
					),
					NAME_None,
					EUserInterfaceActionType::RadioButton
				);
			}
		}
	}
}

TSharedRef<SWidget> SSequencer::MakePlaybackMenu()
{
	FMenuBuilder MenuBuilder( true, SequencerPtr.Pin()->GetCommandBindings() );

	// playback range options
	MenuBuilder.BeginSection("PlaybackThisSequence", LOCTEXT("PlaybackThisSequenceHeader", "Playback - This Sequence"));
	{
		// Menu entry for the start position
		auto OnStartChanged = [this](double NewValue){

			FFrameNumber ValueAsFrame = FFrameTime::FromDecimal(NewValue).GetFrame();
			FFrameNumber PlayStart = ValueAsFrame;
			FFrameNumber PlayEnd = UE::MovieScene::DiscreteExclusiveUpper(SequencerPtr.Pin()->GetPlaybackRange());
			if (PlayStart >= PlayEnd)
			{
				FFrameNumber Duration = PlayEnd - UE::MovieScene::DiscreteInclusiveLower(SequencerPtr.Pin()->GetPlaybackRange());
				PlayEnd = PlayStart + Duration;
			}

			SequencerPtr.Pin()->SetPlaybackRange(TRange<FFrameNumber>(PlayStart, PlayEnd));

			TRange<double> PlayRangeSeconds = SequencerPtr.Pin()->GetPlaybackRange() / SequencerPtr.Pin()->GetFocusedTickResolution();
			const double AdditionalRange = (PlayRangeSeconds.GetUpperBoundValue() - PlayRangeSeconds.GetLowerBoundValue()) * 0.1;

			TRange<double> NewClampRange = SequencerPtr.Pin()->GetClampRange();
			NewClampRange.SetLowerBoundValue(SequencerPtr.Pin()->GetPlaybackRange().GetLowerBoundValue() / SequencerPtr.Pin()->GetFocusedTickResolution() - AdditionalRange);
			if (SequencerPtr.Pin()->GetClampRange().GetLowerBoundValue() > NewClampRange.GetLowerBoundValue())
			{
				SequencerPtr.Pin()->SetClampRange(NewClampRange);
			}

			TRange<double> NewViewRange = SequencerPtr.Pin()->GetViewRange();
			NewViewRange.SetLowerBoundValue(SequencerPtr.Pin()->GetPlaybackRange().GetLowerBoundValue() / SequencerPtr.Pin()->GetFocusedTickResolution() - AdditionalRange);
			if (SequencerPtr.Pin()->GetViewRange().GetLowerBoundValue() > NewViewRange.GetLowerBoundValue())
			{
				SequencerPtr.Pin()->SetViewRange(NewViewRange);
			}
		};

		MenuBuilder.AddWidget(
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
				[
					SNew(SSpacer)
				]
			+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SSpinBox<double>)
						.TypeInterface(NumericTypeInterface)
						.IsEnabled_Lambda([this]() {
							return !SequencerPtr.Pin()->IsPlaybackRangeLocked();
						})
						.Style(&FAppStyle::GetWidgetStyle<FSpinBoxStyle>("Sequencer.HyperlinkSpinBox"))
						.OnValueCommitted_Lambda([=](double Value, ETextCommit::Type){ OnStartChanged(Value); })
						.OnValueChanged_Lambda([=](double Value) { OnStartChanged(Value); })
						.OnBeginSliderMovement(OnPlaybackRangeBeginDrag)
						.OnEndSliderMovement_Lambda([=, this](double Value){ OnStartChanged(Value); OnPlaybackRangeEndDrag.ExecuteIfBound(); })
						.MinValue(TOptional<double>())
						.MaxValue(TOptional<double>())
						.Value_Lambda([this]() -> double {
							return SequencerPtr.Pin()->GetPlaybackRange().GetLowerBoundValue().Value;
						})
						.Delta(this, &SSequencer::GetSpinboxDelta)
						.LinearDeltaSensitivity(25)
			],
			LOCTEXT("PlaybackStartLabel", "Start"));

		// Menu entry for the end position
		auto OnEndChanged = [this](double NewValue) {

			FFrameNumber ValueAsFrame = FFrameTime::FromDecimal(NewValue).GetFrame();
			FFrameNumber PlayStart = UE::MovieScene::DiscreteInclusiveLower(SequencerPtr.Pin()->GetPlaybackRange());
			FFrameNumber PlayEnd = ValueAsFrame;
			if (PlayEnd <= PlayStart)
			{
				FFrameNumber Duration = UE::MovieScene::DiscreteExclusiveUpper(SequencerPtr.Pin()->GetPlaybackRange()) - PlayStart;
				PlayStart = PlayEnd - Duration;
			}

			SequencerPtr.Pin()->SetPlaybackRange(TRange<FFrameNumber>(PlayStart, PlayEnd));

			TRange<double> PlayRangeSeconds = SequencerPtr.Pin()->GetPlaybackRange() / SequencerPtr.Pin()->GetFocusedTickResolution();
			const double AdditionalRange = (PlayRangeSeconds.GetUpperBoundValue() - PlayRangeSeconds.GetLowerBoundValue()) * 0.1;

			TRange<double> NewClampRange = SequencerPtr.Pin()->GetClampRange();
			NewClampRange.SetUpperBoundValue(SequencerPtr.Pin()->GetPlaybackRange().GetUpperBoundValue() / SequencerPtr.Pin()->GetFocusedTickResolution() + AdditionalRange);
			if (SequencerPtr.Pin()->GetClampRange().GetUpperBoundValue() < NewClampRange.GetUpperBoundValue())
			{
				SequencerPtr.Pin()->SetClampRange(NewClampRange);
			}

			TRange<double> NewViewRange = SequencerPtr.Pin()->GetViewRange();
			NewViewRange.SetUpperBoundValue(SequencerPtr.Pin()->GetPlaybackRange().GetUpperBoundValue() / SequencerPtr.Pin()->GetFocusedTickResolution() + AdditionalRange);
			if (SequencerPtr.Pin()->GetViewRange().GetUpperBoundValue() < NewViewRange.GetUpperBoundValue())
			{
				SequencerPtr.Pin()->SetViewRange(NewViewRange);
			}
		};

		MenuBuilder.AddWidget(
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
				[
					SNew(SSpacer)
				]
			+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SSpinBox<double>)
						.TypeInterface(NumericTypeInterface)
						.IsEnabled_Lambda([this]() {
					 		return !SequencerPtr.Pin()->IsPlaybackRangeLocked();
						})
						.Style(&FAppStyle::GetWidgetStyle<FSpinBoxStyle>("Sequencer.HyperlinkSpinBox"))
						.OnValueCommitted_Lambda([=](double Value, ETextCommit::Type){ OnEndChanged(Value); })
						.OnValueChanged_Lambda([=](double Value) { OnEndChanged(Value); })
						.OnBeginSliderMovement(OnPlaybackRangeBeginDrag)
						.OnEndSliderMovement_Lambda([=, this](double Value){ OnEndChanged(Value); OnPlaybackRangeEndDrag.ExecuteIfBound(); })
						.MinValue(TOptional<double>())
						.MaxValue(TOptional<double>())
						.Value_Lambda([this]() -> double {
					 		return SequencerPtr.Pin()->GetPlaybackRange().GetUpperBoundValue().Value;
						})
						.Delta(this, &SSequencer::GetSpinboxDelta)
						.LinearDeltaSensitivity(25)
				],
			LOCTEXT("PlaybackStartEnd", "End"));


		MenuBuilder.AddSubMenu(LOCTEXT("PlaybackSpeedHeader", "Playback Speed"), FText::GetEmpty(), FNewMenuDelegate::CreateRaw(this, &SSequencer::FillPlaybackSpeedMenu));

		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().TogglePlaybackRangeLocked );

		if (SequencerPtr.Pin()->IsLevelEditorSequencer())
		{
			MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleCleanPlaybackMode );
			MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleRerunConstructionScripts );
		}

		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleAsyncEvaluation );
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleDynamicWeighting );
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection( "PlaybackAllSequences", LOCTEXT( "PlaybackRangeAllSequencesHeader", "Playback Range - All Sequences" ) );
	{
		if (SequencerPtr.Pin()->IsLevelEditorSequencer())
		{
			MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleEvaluateSubSequencesInIsolation );
			MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleResetPlayheadWhenNavigating );
		}

		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleKeepCursorInPlaybackRangeWhileScrubbing );

		if (!SequencerPtr.Pin()->IsLevelEditorSequencer())
		{
			MenuBuilder.AddMenuEntry(FSequencerCommands::Get().ToggleKeepPlaybackRangeInSectionBounds);
		}
		
		MenuBuilder.AddMenuEntry(FSequencerCommands::Get().ToggleLinkCurveEditorTimeRange);
	}
	MenuBuilder.EndSection();

	// Menu entry for the jump frame increment
	auto OnJumpFrameIncrementChanged = [this](double NewValue) {
		FFrameRate TickResolution = SequencerPtr.Pin()->GetFocusedTickResolution();
		FFrameRate DisplayRate = SequencerPtr.Pin()->GetFocusedDisplayRate();
		FFrameNumber JumpFrameIncrement = FFrameRate::TransformTime(FFrameTime::FromDecimal(NewValue), TickResolution, DisplayRate).CeilToFrame();						
		SequencerPtr.Pin()->GetSequencerSettings()->SetJumpFrameIncrement(JumpFrameIncrement);
	};

	MenuBuilder.AddWidget(
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
			[
				SNew(SSpacer)
			]
		+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SSpinBox<double>)
					.TypeInterface(NumericTypeInterface)
					.Style(&FAppStyle::GetWidgetStyle<FSpinBoxStyle>("Sequencer.HyperlinkSpinBox"))
					.OnValueCommitted_Lambda([=](double Value, ETextCommit::Type){ OnJumpFrameIncrementChanged(Value); })
					.OnValueChanged_Lambda([=](double Value) { OnJumpFrameIncrementChanged(Value); })
					.MinValue(TOptional<double>())
					.MaxValue(TOptional<double>())
					.Value_Lambda([this]() -> double {
						FFrameNumber JumpFrameIncrement = SequencerPtr.Pin()->GetSequencerSettings()->GetJumpFrameIncrement();
						FFrameRate TickResolution = SequencerPtr.Pin()->GetFocusedTickResolution();
						FFrameRate DisplayRate = SequencerPtr.Pin()->GetFocusedDisplayRate();
						int32 ConvertedValue = FFrameRate::TransformTime(JumpFrameIncrement, DisplayRate, TickResolution).CeilToFrame().Value;						
					 	return ConvertedValue;
					})
					.Delta(this, &SSequencer::GetSpinboxDelta)
					.LinearDeltaSensitivity(25)
			],
		LOCTEXT("JumpFrameIncrement", "Jump Frame Increment"));

	return MenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SSequencer::MakeRenderMovieMenu()
{
	FMenuBuilder MenuBuilder( false, SequencerPtr.Pin()->GetCommandBindings() );

	MenuBuilder.BeginSection( "RenderMovie", LOCTEXT( "RenderMovieMenuHeader", "Render Movie" ) );
	{
		ISequencerModule& SequencerModule = FModuleManager::LoadModuleChecked<ISequencerModule>("Sequencer");
		TArray<FString> MovieRendererNames = SequencerModule.GetMovieRendererNames();

		for (FString MovieRendererName : MovieRendererNames)
		{
			MenuBuilder.AddMenuEntry(
				FText::FromString(MovieRendererName),
				FText::FromString(MovieRendererName),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateLambda([this, MovieRendererName] { SequencerPtr.Pin()->GetSequencerSettings()->SetMovieRendererName(MovieRendererName); }),
					FCanExecuteAction(),
					FIsActionChecked::CreateLambda([this, MovieRendererName]
					{ 
						return MovieRendererName == SequencerPtr.Pin()->GetMovieRendererName();
					})),
				NAME_None,
				EUserInterfaceActionType::RadioButton
			);
		}		

		MenuBuilder.AddMenuEntry(
			LOCTEXT("RenderMovieLegacy", "Movie Scene Capture (Legacy)"),
			LOCTEXT("RenderMovieTooltip", "Movie Scene Capture (Legacy)"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this] { SequencerPtr.Pin()->GetSequencerSettings()->SetMovieRendererName(TEXT("MovieSceneCapture")); }),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this]
				{
					return SequencerPtr.Pin()->GetMovieRendererName() == TEXT("MovieSceneCapture");
				})),
			NAME_None,
			EUserInterfaceActionType::RadioButton
		);
	}

	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SSequencer::MakeSnapMenu()
{
	FMenuBuilder MenuBuilder( false, SequencerPtr.Pin()->GetCommandBindings() );

	MenuBuilder.BeginSection( "KeySnapping", LOCTEXT( "SnappingMenuKeyHeader", "Key Snapping" ) );
	{
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleSnapKeyTimesToInterval );
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleSnapKeyTimesToKeys );
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("SectionSnapping", LOCTEXT("SnappingMenuSectionHeader", "Section Snapping"));
	{
		MenuBuilder.AddMenuEntry(FSequencerCommands::Get().ToggleSnapSectionTimesToInterval);
		MenuBuilder.AddMenuEntry(FSequencerCommands::Get().ToggleSnapSectionTimesToSections);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("KeyAndSectionSnapping", LOCTEXT("SnappingMenuKeysAndSectionHeader", "Keys and Sections Snapping"));
	{
		MenuBuilder.AddMenuEntry(FSequencerCommands::Get().ToggleSnapKeysAndSectionsToPlayRange);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection( "PlayHeadSnapping", LOCTEXT( "SnappingMenuPlayHeadHeader", "Play Head Snapping" ) );
	{
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleSnapPlayTimeToInterval );
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleSnapPlayTimeToKeys );
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleSnapPlayTimeToSections );
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleSnapPlayTimeToMarkers );
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleSnapPlayTimeToPressedKey );
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleSnapPlayTimeToDraggedKey );
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection( "CurveSnapping", LOCTEXT( "SnappingMenuCurveHeader", "Curve Snapping" ) );
	{
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleSnapCurveValueToInterval );
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}


TSharedRef<SWidget> SSequencer::MakeAutoChangeMenu()
{
	FMenuBuilder MenuBuilder(false, SequencerPtr.Pin()->GetCommandBindings());

	MenuBuilder.AddMenuEntry(FSequencerCommands::Get().SetAutoKey);

	if (SequencerPtr.Pin()->IsLevelEditorSequencer())
	{
		MenuBuilder.AddMenuEntry(FSequencerCommands::Get().SetAutoTrack);
	}

	if (IVREditorModule::Get().IsVREditorModeActive() || (SequencerPtr.Pin()->IsLevelEditorSequencer() && ExactCast<ULevelSequence>(SequencerPtr.Pin()->GetFocusedMovieSceneSequence()) == nullptr))
	{
		MenuBuilder.AddMenuEntry(FSequencerCommands::Get().SetAutoChangeAll);
	}

	MenuBuilder.AddMenuEntry(FSequencerCommands::Get().SetAutoChangeNone);

	return MenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SSequencer::MakeAllowEditsMenu()
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, SequencerPtr.Pin()->GetCommandBindings());

	MenuBuilder.AddMenuEntry(FSequencerCommands::Get().AllowAllEdits);
	MenuBuilder.AddMenuEntry(FSequencerCommands::Get().AllowSequencerEditsOnly);
	MenuBuilder.AddMenuEntry(FSequencerCommands::Get().AllowLevelEditsOnly);

	return MenuBuilder.MakeWidget();

}

TSharedRef<SWidget> SSequencer::MakeKeyGroupMenu()
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, SequencerPtr.Pin()->GetCommandBindings());

	if (SequencerPtr.Pin()->IsLevelEditorSequencer())
	{
		MenuBuilder.AddMenuEntry(FSequencerCommands::Get().SetKeyAll);
		MenuBuilder.AddMenuEntry(FSequencerCommands::Get().SetKeyGroup);
		MenuBuilder.AddMenuEntry(FSequencerCommands::Get().SetKeyChanged);
	}

	// Interpolation
	MenuBuilder.BeginSection("SequencerInterpolation", LOCTEXT("KeyInterpolationMenu", "Default Key Interpolation"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("SetKeyInterpolationSmartAuto", "Cubic (Smart Auto)"),
			LOCTEXT("SetKeyInterpolationSmartAutoTooltip", "Set key interpolation to smart auto"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Sequencer.IconKeySmartAuto"),
			FUIAction(
				FExecuteAction::CreateLambda([this] { SequencerPtr.Pin()->SetKeyInterpolation(EMovieSceneKeyInterpolation::SmartAuto); }),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this] { return SequencerPtr.Pin()->GetKeyInterpolation() == EMovieSceneKeyInterpolation::SmartAuto; })),
			NAME_None,
			EUserInterfaceActionType::RadioButton
		);
		MenuBuilder.AddMenuEntry(
			LOCTEXT("SetKeyInterpolationAuto", "Cubic (Auto)"),
			LOCTEXT("SetKeyInterpolationAutoTooltip", "Set key interpolation to auto"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Sequencer.IconKeyAuto"),
			FUIAction(
				FExecuteAction::CreateLambda([this] { SequencerPtr.Pin()->SetKeyInterpolation(EMovieSceneKeyInterpolation::Auto); }),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this] { return SequencerPtr.Pin()->GetKeyInterpolation() == EMovieSceneKeyInterpolation::Auto; })),
			NAME_None,
			EUserInterfaceActionType::RadioButton
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("SetKeyInterpolationUser", "Cubic (User)"),
			LOCTEXT("SetKeyInterpolationUserTooltip", "Set key interpolation to user"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Sequencer.IconKeyUser"),
			FUIAction(
				FExecuteAction::CreateLambda([this] { SequencerPtr.Pin()->SetKeyInterpolation(EMovieSceneKeyInterpolation::User); }),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this] { return SequencerPtr.Pin()->GetKeyInterpolation() == EMovieSceneKeyInterpolation::User; })),
			NAME_None,
			EUserInterfaceActionType::RadioButton
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("SetKeyInterpolationBreak", "Cubic (Break)"),
			LOCTEXT("SetKeyInterpolationBreakTooltip", "Set key interpolation to break"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Sequencer.IconKeyBreak"),
			FUIAction(
				FExecuteAction::CreateLambda([this] { SequencerPtr.Pin()->SetKeyInterpolation(EMovieSceneKeyInterpolation::Break); }),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this] { return SequencerPtr.Pin()->GetKeyInterpolation() == EMovieSceneKeyInterpolation::Break; })),
			NAME_None,
			EUserInterfaceActionType::RadioButton
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("SetKeyInterpolationLinear", "Linear"),
			LOCTEXT("SetKeyInterpolationLinearTooltip", "Set key interpolation to linear"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Sequencer.IconKeyLinear"),
			FUIAction(
				FExecuteAction::CreateLambda([this] { SequencerPtr.Pin()->SetKeyInterpolation(EMovieSceneKeyInterpolation::Linear); }),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this] { return SequencerPtr.Pin()->GetKeyInterpolation() == EMovieSceneKeyInterpolation::Linear; })),
			NAME_None,
			EUserInterfaceActionType::RadioButton
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("SetKeyInterpolationConstant", "Constant"),
			LOCTEXT("SetKeyInterpolationConstantTooltip", "Set key interpolation to constant"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Sequencer.IconKeyConstant"),
			FUIAction(
				FExecuteAction::CreateLambda([this] { SequencerPtr.Pin()->SetKeyInterpolation(EMovieSceneKeyInterpolation::Constant); }),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this] { return SequencerPtr.Pin()->GetKeyInterpolation() == EMovieSceneKeyInterpolation::Constant; })),
			NAME_None,
			EUserInterfaceActionType::RadioButton
		);
	}
	MenuBuilder.EndSection(); // SequencerInterpolation

	return MenuBuilder.MakeWidget();

}

TSharedRef<SWidget> SSequencer::MakeTimeRange(const TSharedRef<SWidget>& InnerContent, bool bShowWorkingRange, bool bShowViewRange, bool bShowPlaybackRange)
{
	ISequencerWidgetsModule& SequencerWidgets = FModuleManager::Get().LoadModuleChecked<ISequencerWidgetsModule>( "SequencerWidgets" );

	EShowRange ShowRange = EShowRange::None;
	if (bShowWorkingRange)
	{
		ShowRange |= EShowRange::WorkingRange;
	}
	if (bShowViewRange)
	{
		ShowRange |= EShowRange::ViewRange;
	}
	if (bShowPlaybackRange)
	{
		ShowRange |= EShowRange::PlaybackRange;
	}

	FTimeRangeArgs Args(
		ShowRange,
		TimeSliderController.ToSharedRef(),
		EVisibility::Visible,
		NumericTypeInterface.ToSharedRef()
		);
	return SequencerWidgets.CreateTimeRange(Args, InnerContent);
}

TSharedPtr<ITimeSlider> SSequencer::GetTopTimeSliderWidget() const
{
	return TopTimeSlider;
}

SSequencer::~SSequencer()
{
	using namespace UE::Sequencer;

	USelection::SelectionChangedEvent.RemoveAll(this);

	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();
	if(Sequencer && Sequencer->GetToolkitHost() && Sequencer->GetToolkitHost()->GetTabManager())
	{
		if (Sequencer->GetHostCapabilities().bSupportsCurveEditor)
		{
			TSharedPtr<FEditorViewModel> RootModel = Sequencer->GetViewModel();
			FCurveEditorExtension* CurveEditorExtension = RootModel->CastDynamicChecked<FCurveEditorExtension>();
			CurveEditorExtension->CloseCurveEditor();
		}
	}

	if (TSharedPtr<SWindow> Window = WeakExposedBindingsWindow.Pin())
	{
		Window->DestroyWindowImmediately();
	}

	if (TSharedPtr<SWindow> Window = WeakNodeGroupWindow.Pin())
	{
		Window->DestroyWindowImmediately();
		NodeGroupManager.Reset();
	}
}


void SSequencer::RegisterActiveTimerForPlayback()
{
	if (!bIsActiveTimerRegistered)
	{
		bIsActiveTimerRegistered = true;
		RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SSequencer::EnsureSlateTickDuringPlayback));
	}
}


EActiveTimerReturnType SSequencer::EnsureSlateTickDuringPlayback(double InCurrentTime, float InDeltaTime)
{
	if (SequencerPtr.IsValid())
	{
		auto PlaybackStatus = SequencerPtr.Pin()->GetPlaybackStatus();
		if (PlaybackStatus == EMovieScenePlayerStatus::Playing || PlaybackStatus == EMovieScenePlayerStatus::Scrubbing)
		{
			return EActiveTimerReturnType::Continue;
		}
	}

	bIsActiveTimerRegistered = false;
	return EActiveTimerReturnType::Stop;
}

void SSequencer::UpdateLayoutTree()
{
	using namespace UE::Sequencer;

	TrackArea->Empty();
	PinnedTrackArea->Empty();

	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();
	if ( Sequencer.IsValid() )
	{
		// Update the node tree
		Sequencer->GetNodeTree()->Update();

		// This must come after the selection state has been restored so that the tree and curve editor are populated with the correctly selected nodes
		TreeView->Refresh();

		if (!NodePathToRename.IsEmpty())
		{
			TArray<TSharedPtr<FViewModel>> AllNodes;
			Sequencer->GetNodeTree()->GetAllNodes(AllNodes);
			for (TSharedPtr<FViewModel> Node : AllNodes)
			{
				const FString CurNodePath = IOutlinerExtension::GetPathName(Node);
				if (CurNodePath == NodePathToRename)
				{
					GEditor->GetTimerManager()->SetTimerForNextTick([Node]
					{
						if (IRenameableExtension* Rename = Node->CastThis<IRenameableExtension>())
						{
							Rename->OnRenameRequested().Broadcast();
						}
					});
					break;
				}
			}
			NodePathToRename.Empty();
		}

		if (Sequencer->GetFocusedMovieSceneSequence())
		{
			bool bAnyChanged = false;

			TSharedPtr<FSharedViewModelData> SharedData = Sequencer->GetViewModel()->GetRootModel()->GetSharedData();
			FOutlinerCacheExtension*     OutlinerCache  = SharedData->CastThis<FOutlinerCacheExtension>();
			FMuteStateCacheExtension*    MuteState      = SharedData->CastThis<FMuteStateCacheExtension>();
			FSoloStateCacheExtension*    SoloState      = SharedData->CastThis<FSoloStateCacheExtension>();

			check(OutlinerCache && MuteState && SoloState);

			// Hack - we shouldn't really not just forcibly update these here, but currently this function is getting forcibly called before
			//        UpdateCachedFlags has a chance to naturally update itself in response to the signature change
			OutlinerCache->UpdateCachedFlags();

			const bool bAnySoloNodes = EnumHasAnyFlags(SoloState->GetRootFlags(), ECachedSoloState::Soloed | ECachedSoloState::PartiallySoloedChildren);

			for (TViewModelPtr<ITrackExtension> TrackNode
				: Sequencer->GetNodeTree()->GetRootNode()->GetDescendantsOfType<ITrackExtension>())
			{
				if (TrackNode->GetTrack())
				{
					ECachedMuteState MuteFlags = MuteState->GetCachedFlags(TrackNode.AsModel());
					ECachedSoloState SoloFlags = SoloState->GetCachedFlags(TrackNode.AsModel());

					UMovieSceneTrack* Track = TrackNode->GetTrack();
					const bool bIsMuted  = EnumHasAnyFlags(MuteFlags, ECachedMuteState::Muted  | ECachedMuteState::ImplicitlyMutedByParent);
					const bool bIsSoloed = EnumHasAnyFlags(SoloFlags, ECachedSoloState::Soloed | ECachedSoloState::ImplicitlySoloedByParent);

					const bool bDisableEval = bIsMuted || (bAnySoloNodes && !bIsSoloed);

					TViewModelPtr<FTrackRowModel> TrackRowModel = TrackNode.ImplicitCast();
					if (TrackRowModel)
					{
						if (bDisableEval != Track->IsRowEvalDisabled(TrackNode->GetRowIndex()))
						{
							Track->Modify();
							Track->SetRowEvalDisabled(bDisableEval, TrackNode->GetRowIndex());
							bAnyChanged = true;
						}
					}
					else
					{
						if (bDisableEval != Track->IsEvalDisabled())
						{
							Track->Modify();
							Track->SetEvalDisabled(bDisableEval);
							bAnyChanged = true;
						}	
					}
				}
			}
			if (bAnyChanged)
			{
				Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);
			}
		}
		
		if (NodeGroupManager.IsValid())
		{
			NodeGroupManager->RefreshNodeGroups();
		}
		
		if (SequencerTreeFilterStatusBar.IsValid())
		{
			SequencerTreeFilterStatusBar->UpdateText();
		}
		
	}
}

void SSequencer::UpdateBreadcrumbs()
{
	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();
	if (!Sequencer.IsValid())
	{
		return;
	}

	FMovieSceneSequenceID FocusedID = Sequencer->GetFocusedTemplateID();

	if (BreadcrumbTrail->PeekCrumb().BreadcrumbType == FSequencerBreadcrumb::ShotType)
	{
		BreadcrumbTrail->PopCrumb();
	}

	if( BreadcrumbTrail->PeekCrumb().BreadcrumbType == FSequencerBreadcrumb::MovieSceneType && BreadcrumbTrail->PeekCrumb().SequenceID != FocusedID )
	{
		TWeakObjectPtr<UMovieSceneSubSection> SubSection = Sequencer->FindSubSection(FocusedID);
		TAttribute<FText> CrumbNameAttribute = MakeAttributeSP(this, &SSequencer::GetBreadcrumbTextForSection, SubSection);

		// The current breadcrumb is not a moviescene so we need to make a new breadcrumb in order return to the parent moviescene later
		BreadcrumbTrail->PushCrumb( CrumbNameAttribute, FSequencerBreadcrumb( FocusedID, CrumbNameAttribute.Get()) );
	}
}


void SSequencer::ResetBreadcrumbs()
{
	BreadcrumbTrail->ClearCrumbs();

	TAttribute<FText> CrumbNameAttribute = MakeAttributeSP(this, &SSequencer::GetBreadcrumbTextForSequence, MakeWeakObjectPtr(SequencerPtr.Pin()->GetRootMovieSceneSequence()), true);
	BreadcrumbTrail->PushCrumb(CrumbNameAttribute, FSequencerBreadcrumb(MovieSceneSequenceID::Root, CrumbNameAttribute.Get()));
}

void SSequencer::PopBreadcrumb()
{
	BreadcrumbTrail->PopCrumb();
}

void SSequencer::SetSearchText(const FText& InSearchText)
{
	SearchBox->SetText(InSearchText);
}

void SSequencer::OnOutlinerSearchChanged( const FText& Filter )
{
	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();
	if ( Sequencer.IsValid())
	{
		const FString FilterString = Filter.ToString();

		Sequencer->GetNodeTree()->FilterNodes( FilterString );

		TreeView->Refresh();
	}
}

void SSequencer::OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	// @todo sequencer: Add drop validity cue
}


void SSequencer::OnDragLeave( const FDragDropEvent& DragDropEvent )
{
	// @todo sequencer: Clear drop validity cue
}


FReply SSequencer::OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	for (FOptionalOnDragDrop Delegate : OnReceivedDragOver)
	{
		if (Delegate.IsBound())
		{
			FReply DelegateReply = FReply::Unhandled();
			if (Delegate.Execute(MyGeometry, DragDropEvent, DelegateReply))
			{
				return DelegateReply;
			}
		}
	}

	bool bIsDragSupported = false;

	TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();
	if (Operation.IsValid() && (
		Operation->IsOfType<FAssetDragDropOp>() ||
		Operation->IsOfType<FClassDragDropOp>() ||
		Operation->IsOfType<FActorDragDropOp>() ||
		Operation->IsOfType<FFolderDragDropOp>() ) )
	{
		bIsDragSupported = true;
	}

	return bIsDragSupported ? FReply::Handled() : FReply::Unhandled();
}


FReply SSequencer::OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	for (FOptionalOnDragDrop Delegate : OnReceivedDrop)
	{
		if (Delegate.IsBound())
		{
			FReply DelegateReply = FReply::Unhandled();
			if (Delegate.Execute(MyGeometry, DragDropEvent, DelegateReply))
			{
				return DelegateReply;
			}
		}
	}

	bool bWasDropHandled = false;

	// @todo sequencer: Get rid of hard-code assumptions about dealing with ACTORS at this level?

	// @todo sequencer: We may not want any actor-specific code here actually.  We need systems to be able to
	// register with sequencer to support dropping assets/classes/actors, or OTHER types!

	// @todo sequencer: Handle drag and drop from other FDragDropOperations, including unloaded classes/asset and external drags!

	// @todo sequencer: Consider allowing drops into the level viewport to add to the MovieScene as well.
	//		- Basically, when Sequencer is open it would take over drops into the level and auto-add puppets for these instead of regular actors
	//		- This would let people drag smoothly and precisely into the view to drop assets/classes into the scene

	TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();

	if (Operation.IsValid() )
	{
		if ( Operation->IsOfType<FAssetDragDropOp>() )
		{
			const auto& DragDropOp = StaticCastSharedPtr<FAssetDragDropOp>(Operation);

			OnAssetsDropped( *DragDropOp );
			bWasDropHandled = true;
		}
		else if( Operation->IsOfType<FClassDragDropOp>() )
		{
			const auto& DragDropOp = StaticCastSharedPtr<FClassDragDropOp>(Operation);

			OnClassesDropped( *DragDropOp );
			bWasDropHandled = true;
		}
		else if( Operation->IsOfType<FActorDragDropOp>() )
		{
			const auto& DragDropOp = StaticCastSharedPtr<FActorDragDropOp>(Operation);

			OnActorsDropped( *DragDropOp );
			bWasDropHandled = true;
		}
		else if (Operation->IsOfType<FFolderDragDropOp>())
		{
			const auto& DragDropOp = StaticCastSharedPtr<FFolderDragDropOp>(Operation);

			OnFolderDropped(*DragDropOp);
			bWasDropHandled = true;
		}
		else if (Operation->IsOfType<FCompositeDragDropOp>())
		{
			const TSharedPtr<FCompositeDragDropOp> CompositeOp = StaticCastSharedPtr<FCompositeDragDropOp>(Operation);
			if (const TSharedPtr<FActorDragDropOp> ActorDragDropOp = CompositeOp->GetSubOp<FActorDragDropOp>())
			{
				OnActorsDropped(*ActorDragDropOp);
				bWasDropHandled = true;
			}
			if (const TSharedPtr<FFolderDragDropOp> FolderDragDropOp = CompositeOp->GetSubOp<FFolderDragDropOp>())
			{
				OnFolderDropped(*FolderDragDropOp);
				bWasDropHandled = true;
			}
			
		}
	}

	return bWasDropHandled ? FReply::Handled() : FReply::Unhandled();
}


FReply SSequencer::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) 
{
	// A toolkit tab is active, so direct all command processing to it
	TSharedPtr<FSequencer> SequencerPin = SequencerPtr.Pin();
	if ( SequencerPin.IsValid() && SequencerPin->GetCommandBindings()->ProcessCommandBindings( InKeyEvent ) )
	{
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SSequencer::OnFocusChanging( const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath, const FFocusEvent& InFocusEvent )
{
	if (NewWidgetPath.ContainsWidget(this))
	{
		OnReceivedFocus.ExecuteIfBound();
	}
}

void SSequencer::OnAssetsDropped( const FAssetDragDropOp& DragDropOp )
{
	using namespace UE::Sequencer;

	FSequencer& SequencerRef = *SequencerPtr.Pin();

	TArray< UObject* > DroppedObjects;
	bool bAllAssetsWereLoaded = true;
	bool bNeedsLoad = false;

	for (const FAssetData& AssetData : DragDropOp.GetAssets())
	{
		if (!AssetData.IsAssetLoaded())
		{
			bNeedsLoad = true;
			break;
		}
	}

	if (bNeedsLoad)
	{
		GWarn->BeginSlowTask(LOCTEXT("OnDrop_FullyLoadPackage", "Fully Loading Package For Drop"), true, false);
	}

	for (const FAssetData& AssetData : DragDropOp.GetAssets())
	{
		if (!MovieSceneToolHelpers::IsValidAsset(SequencerRef.GetFocusedMovieSceneSequence(), AssetData))
		{
			continue;
		}

		UObject* Object = AssetData.GetAsset();

		if ( Object != nullptr )
		{
			DroppedObjects.Add( Object );
		}
		else
		{
			bAllAssetsWereLoaded = false;
		}
	}

	if (bNeedsLoad)
	{
		GWarn->EndSlowTask();
	}

	FGuid TargetObjectGuid;
	// if exactly one object node is selected, we have a target object guid
	if (SequencerPtr.Pin()->GetViewModel()->GetSelection()->Outliner.Num() == 1)
	{
		for (TViewModelPtr<FObjectBindingModel> ObjectBindingNode : SequencerPtr.Pin()->GetViewModel()->GetSelection()->Outliner.Filter<FObjectBindingModel>())
		{
			TargetObjectGuid = ObjectBindingNode->GetObjectGuid();
		}
	}

	ESequencerDropResult DropResult = ESequencerDropResult::Unhandled;

	const FScopedTransaction Transaction(LOCTEXT("DropAssets", "Drop Assets"));

	// See if any callback wants to handle this drop.
	for (FOnAssetsDrop Delegate : OnAssetsDrop)
	{
		if (Delegate.IsBound())
		{
			DropResult = Delegate.Execute(DroppedObjects, DragDropOp);
			if (DropResult != ESequencerDropResult::Unhandled)
			{
				break;
			}
		}
	}

	// If nobody took care of it, do the default behaviour.
	if (DropResult == ESequencerDropResult::Unhandled)
	{
		FMovieSceneTrackEditor::BeginKeying(SequencerPtr.Pin()->GetLocalTime().Time.FrameNumber);

		for (TArray<UObject*>::TConstIterator CurObjectIter = DroppedObjects.CreateConstIterator(); CurObjectIter; ++CurObjectIter)
		{
			UObject* CurObject = *CurObjectIter;

			if (!SequencerRef.OnHandleAssetDropped(CurObject, TargetObjectGuid))
			{
				// Doesn't make sense to drop a level sequence asset into sequencer as a spawnable actor
				if (CurObject->IsA<ULevelSequence>())
				{
					UE_LOG(LogSequencer, Warning, TEXT("Can't add '%s' as a spawnable"), *CurObject->GetName());
					continue;
				}

				// If the object dragged resolves to a locator of actor type, or is natively a non-supported object type, attempt to use the actor factory to make a spawnable actor from it.
				// Otherwise, the CreateBinding will have succeeded.
				FUniversalObjectLocator TestLocator(CurObject, SequencerRef.GetPlaybackContext());
				if (TestLocator.GetLastFragmentTypeHandle() == FActorLocatorFragment::FragmentType || !SequencerRef.CreateBinding(*CurObject, CurObject->GetName()).IsValid())
				{
					SequencerRef.MakeNewSpawnable(*CurObject, DragDropOp.GetActorFactory());
				}
			}

			DropResult = ESequencerDropResult::DropHandled;
		}

		FMovieSceneTrackEditor::EndKeying();
	}

	if (DropResult == ESequencerDropResult::DropHandled)
	{
		// Update the sequencers view of the movie scene data when any object is added
		SequencerRef.NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemAdded );

		// Update the tree and synchronize selection
		UpdateLayoutTree();

		SequencerRef.SynchronizeSequencerSelectionWithExternalSelection();
	}
}


void SSequencer::OnClassesDropped( const FClassDragDropOp& DragDropOp )
{
	const FScopedTransaction Transaction(LOCTEXT("DropClasses", "Drop Classes"));

	ESequencerDropResult DropResult = ESequencerDropResult::Unhandled;

	for (FOnClassesDrop Delegate : OnClassesDrop)
	{
		if (Delegate.IsBound())
		{
			DropResult = Delegate.Execute(DragDropOp.ClassesToDrop, DragDropOp);
			if (DropResult != ESequencerDropResult::Unhandled)
			{
				break;
			}
		}
	}

	if (DropResult == ESequencerDropResult::Unhandled)
	{
		FSequencer& SequencerRef = *SequencerPtr.Pin();

		for (auto ClassIter = DragDropOp.ClassesToDrop.CreateConstIterator(); ClassIter; ++ClassIter)
		{
			UClass* Class = (*ClassIter).Get();
			if (Class != nullptr)
			{
				UObject* Object = Class->GetDefaultObject();

				FGuid NewGuid = SequencerRef.MakeNewSpawnable(*Object);
			}
		}
	}
}

void SSequencer::OnActorsDropped( FActorDragDropOp& DragDropOp )
{
	const FScopedTransaction Transaction(LOCTEXT("DropActors", "Drop Actors"));

	ESequencerDropResult DropResult = ESequencerDropResult::Unhandled;

	for (FOnActorsDrop Delegate : OnActorsDrop)
	{
		if (Delegate.IsBound())
		{
			DropResult = Delegate.Execute(DragDropOp.Actors, DragDropOp);
			if (DropResult != ESequencerDropResult::Unhandled)
			{
				break;
			}
		}
	}

	if (DropResult == ESequencerDropResult::Unhandled)
	{
		SequencerPtr.Pin()->OnActorsDropped(DragDropOp.Actors);
	}
}

void SSequencer::OnFolderDropped( FFolderDragDropOp& DragDropOp )
{
	// Sequencer doesn't support dragging folder with a root object
	if (!FFolder::IsRootObjectPersistentLevel(DragDropOp.RootObject))
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("DropActors", "Drop Actors"));

	ESequencerDropResult DropResult = ESequencerDropResult::Unhandled;

	TArray<TWeakObjectPtr<AActor>> DraggedActors;

	// Find any actors in the global editor world that have any of the dragged paths.
	// WARNING: Actor iteration can be very slow, so this needs to be optimized
	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();
	UObject* PlaybackContext = Sequencer->GetPlaybackContext();
	UWorld* World = PlaybackContext ? PlaybackContext->GetWorld() : nullptr;
	if (World)
	{
		FActorFolders::GetWeakActorsFromFolders(*World, DragDropOp.Folders, DraggedActors);
	}
	
	for (FOnFoldersDrop Delegate : OnFoldersDrop)
	{
		if (Delegate.IsBound())
		{
			DropResult = Delegate.Execute(DragDropOp.Folders, DragDropOp);
			if (DropResult != ESequencerDropResult::Unhandled)
			{
				break;
			}
		}
	}

	if (DropResult == ESequencerDropResult::Unhandled)
	{
		SequencerPtr.Pin()->OnActorsDropped(DraggedActors);
	}
}

void SSequencer::OnCrumbClicked(const FSequencerBreadcrumb& Item)
{
	if (Item.BreadcrumbType != FSequencerBreadcrumb::ShotType)
	{
		if( SequencerPtr.Pin()->GetFocusedTemplateID() == Item.SequenceID ) 
		{
			// then do zooming
		}
		else
		{
			SequencerPtr.Pin()->PopToSequenceInstance( Item.SequenceID );
		}
	}
}


FText SSequencer::GetRootAnimationName() const
{
	return SequencerPtr.Pin()->GetRootMovieSceneSequence()->GetDisplayName();
}


TSharedPtr<UE::Sequencer::SOutlinerView> SSequencer::GetTreeView() const
{
	return TreeView;
}


TSharedPtr<UE::Sequencer::SOutlinerView> SSequencer::GetPinnedTreeView() const
{
	return PinnedTreeView;
}


void SSequencer::OnSaveMovieSceneClicked()
{
	SequencerPtr.Pin()->SaveCurrentMovieScene();
}

float SSequencer::GetPinnedAreaMaxHeight() const
{
	if (!MainSequencerArea.IsValid())
	{
		return 0.0f;
	}

	// Allow the pinned area to use up to 2/3rds of the sequencer area
	return MainSequencerArea->GetCachedGeometry().GetLocalSize().Y * 0.666f;
}

EVisibility SSequencer::GetPinnedAreaVisibility() const
{
	return PinnedTreeView->GetNumRootNodes() > 0 ? EVisibility::Visible : EVisibility::Collapsed;
}

FText SSequencer::GetBreadcrumbTextForSection(TWeakObjectPtr<UMovieSceneSubSection> SubSection) const
{
	UMovieSceneSubSection* SubSectionPtr = SubSection.Get();
	return SubSectionPtr ? GetBreadcrumbTextForSequence(SubSectionPtr->GetSequence(), SubSectionPtr->IsActive()) : FText();
}


FText SSequencer::GetBreadcrumbTextForSequence(TWeakObjectPtr<UMovieSceneSequence> Sequence, bool bIsActive) const
{
	UMovieSceneSequence* SequencePtr = Sequence.Get();

	bool bIsDirty = SequencePtr->GetMovieScene()->GetOuter()->GetOutermost()->IsDirty();

	if (bIsActive)
	{
		if (bIsDirty)
		{
			return FText::Format(LOCTEXT("DirtySequenceBreadcrumbFormat", "{0}*"), SequencePtr->GetDisplayName());
		}
		else
		{
			return SequencePtr->GetDisplayName();
		}
	}
	else
	{
		if (bIsDirty)
		{
			return FText::Format(LOCTEXT("DirtyInactiveSequenceBreadcrumbFormat", "{0}* [{1}]"),
				SequencePtr->GetDisplayName(),
				LOCTEXT("InactiveSequenceBreadcrumb", "Inactive"));

		}
		else
		{
			return FText::Format(LOCTEXT("InactiveSequenceBreadcrumbFormat", "{0} [{1}]"),
				SequencePtr->GetDisplayName(),
				LOCTEXT("InactiveSequenceBreadcrumb", "Inactive"));
		}
	}
}


EVisibility SSequencer::GetBreadcrumbTrailVisibility() const
{
	return SequencerPtr.Pin()->IsLevelEditorSequencer() ? EVisibility::Visible : EVisibility::Collapsed;
}

bool SSequencer::CanNavigateBreadcrumbs() const
{
	if (SequencerPtr.Pin()->IsLevelEditorSequencer())
	{
		UMovieSceneSequence* RootSequence = SequencerPtr.Pin()->GetRootMovieSceneSequence();
		UMovieScene* MovieScene = RootSequence ? RootSequence->GetMovieScene() : nullptr;
		if (RootSequence)
		{
			for (UMovieSceneTrack* Track : MovieScene->GetTracks())
			{
				if (Track && Track->IsA<UMovieSceneSubTrack>())
				{
					return true;
				}
			}
		}
	}

	return false;
}


EVisibility SSequencer::GetBottomTimeSliderVisibility() const
{
	return GetSequencerSettings()->GetShowRangeSlider() ? EVisibility::Hidden : EVisibility::Visible;
}


EVisibility SSequencer::GetTimeRangeVisibility() const
{
	return GetSequencerSettings()->GetShowRangeSlider() ? EVisibility::Visible : EVisibility::Hidden;
}

EVisibility SSequencer::GetInfoButtonVisibility() const
{
	return GetSequencerSettings()->GetShowInfoButton() ? EVisibility::SelfHitTestInvisible : EVisibility::Hidden;
}

EVisibility SSequencer::GetShowTickLines() const
{
	return GetSequencerSettings()->GetShowTickLines() ? EVisibility::SelfHitTestInvisible : EVisibility::Hidden;
}

EVisibility SSequencer::GetShowSequencerToolbar() const
{
	return GetSequencerSettings()->GetShowSequencerToolbar() ? EVisibility::Visible : EVisibility::Collapsed;
}

EFrameNumberDisplayFormats SSequencer::GetTimeDisplayFormat() const
{
	return GetSequencerSettings()->GetTimeDisplayFormat();
}

void SSequencer::OnSplitterFinishedResizing()
{
	SSplitter::FSlot const& LeftSplitterSlot = TreeViewSplitter->Splitter->SlotAt(0);
	SSplitter::FSlot const& RightSplitterSlot = TreeViewSplitter->Splitter->SlotAt(1);

	OnColumnFillCoefficientChanged(LeftSplitterSlot.GetSizeValue(), 0);
	OnColumnFillCoefficientChanged(RightSplitterSlot.GetSizeValue(), 1);

	GetSequencerSettings()->SetTreeViewWidth(LeftSplitterSlot.GetSizeValue());
}

void SSequencer::OnColumnFillCoefficientChanged(float FillCoefficient, int32 ColumnIndex)
{
	ColumnFillCoefficients[ColumnIndex] = FillCoefficient;
}

void SSequencer::OnCurveEditorVisibilityChanged(bool bShouldBeVisible)
{
	using namespace UE::Sequencer;

	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();

	// Curve Editor may not be supported
	if (!Sequencer->GetHostCapabilities().bSupportsCurveEditor)
	{
		return;
	}

	TSharedPtr<FEditorViewModel> RootModel = Sequencer->GetViewModel();
	FCurveEditorExtension* CurveEditorExtension = RootModel->CastDynamicChecked<FCurveEditorExtension>();

	if (bShouldBeVisible)
	{
		CurveEditorExtension->OpenCurveEditor();
	}
	else
	{
		CurveEditorExtension->CloseCurveEditor();
	}
}


void SSequencer::OnTimeSnapIntervalChanged(float InInterval)
{
	// @todo: sequencer-timecode: Address dealing with different time intervals
	// TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();
	// if ( Sequencer.IsValid() )
	// {
	// 	UMovieScene* MovieScene = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene();
	// 	if (!FMath::IsNearlyEqual(MovieScene->GetFixedFrameInterval(), InInterval))
	// 	{
	// 		FScopedTransaction SetFixedFrameIntervalTransaction( NSLOCTEXT( "Sequencer", "SetFixedFrameInterval", "Set scene fixed frame interval" ) );
	// 		MovieScene->Modify();
	// 		MovieScene->SetFixedFrameInterval( InInterval );

	// 		// Update the current time to the new interval
	// 		float NewTime = SequencerHelpers::SnapTimeToInterval(Sequencer->GetLocalTime(), InInterval);
	// 		Sequencer->SetLocalTime(NewTime);
	// 	}
	// }
}


FPaintPlaybackRangeArgs SSequencer::GetSectionPlaybackRangeArgs() const
{
	if (GetBottomTimeSliderVisibility() == EVisibility::Visible)
	{
		static FPaintPlaybackRangeArgs Args(FAppStyle::GetBrush("Sequencer.Timeline.PlayRange_L"), FAppStyle::GetBrush("Sequencer.Timeline.PlayRange_R"), 6.f);
		return Args;
	}
	else
	{
		static FPaintPlaybackRangeArgs Args(FAppStyle::GetBrush("Sequencer.Timeline.PlayRange_Bottom_L"), FAppStyle::GetBrush("Sequencer.Timeline.PlayRange_Bottom_R"), 6.f);
		return Args;
	}
}


UE::Sequencer::FVirtualTrackArea SSequencer::GetVirtualTrackArea(const UE::Sequencer::STrackAreaView* InTrackArea) const
{
	using namespace UE::Sequencer;

	const STrackAreaView* TargetTrackArea = TrackArea.Get();
	TSharedPtr<SOutlinerView> TargetTreeView = TreeView;
	
	if (InTrackArea != nullptr)
	{
		TargetTrackArea = InTrackArea;
		TargetTreeView = TargetTrackArea->GetOutliner().Pin();
	}

	TSharedPtr<FTrackAreaViewModel> TargetTrackAreaViewModel = TargetTrackArea->GetViewModel();

	return FVirtualTrackArea(*TargetTrackAreaViewModel, *TargetTreeView.Get(), TargetTrackArea->GetCachedGeometry());
}

FPasteContextMenuArgs SSequencer::GeneratePasteArgs(FFrameNumber PasteAtTime, TSharedPtr<FMovieSceneClipboard> Clipboard)
{
	using namespace UE::Sequencer;

	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();
	if (GetSequencerSettings()->GetIsSnapEnabled())
	{
		FFrameRate TickResolution = Sequencer->GetFocusedTickResolution();
		FFrameRate DisplayRate    = Sequencer->GetFocusedDisplayRate();

		PasteAtTime = ConvertFrameTime(PasteAtTime, TickResolution, DisplayRate).RoundToFrame();
		PasteAtTime = ConvertFrameTime(PasteAtTime, DisplayRate, TickResolution).FrameNumber;
	}

	// Open a paste menu at the current mouse position
	FSlateApplication& Application = FSlateApplication::Get();
	FVector2D LocalMousePosition = TrackArea->GetCachedGeometry().AbsoluteToLocal(Application.GetCursorPos());

	FVirtualTrackArea VirtualTrackArea = GetVirtualTrackArea();

	// Paste into the currently selected sections, or hit test the mouse position as a last resort
	TArray<TViewModelPtr<IOutlinerExtension>> PasteIntoNodes;
	{
		TSharedPtr<FSequencerSelection> Selection = Sequencer->GetViewModel()->GetSelection();

		TSet<UMovieSceneSection*> Sections = Selection->GetSelectedSections();

		for (FKeyHandle Key : Selection->KeySelection)
		{
			TSharedPtr<FChannelModel> Channel = Selection->KeySelection.GetModelForKey(Key);
			UMovieSceneSection*       Section = Channel ? Channel->GetSection() : nullptr;
			if (Channel)
			{
				Sections.Add(Section);
			}
		}

		for (UMovieSceneSection* Section : Sections)
		{
			if (TSharedPtr<FSectionModel> Handle = Sequencer->GetNodeTree()->GetSectionModel(Section))
			{
				TViewModelPtr<IOutlinerExtension> TrackModel = Handle->GetParentTrackModel().ImplicitCast();
				if (TrackModel)
				{
					PasteIntoNodes.Add(TrackModel);
				}
			}
		}

		for (FViewModelPtr SelectedNode : Sequencer->GetViewModel()->GetSelection()->Outliner)
		{
			if (SelectedNode->IsA<FCategoryGroupModel>() || SelectedNode->IsA<ITrackExtension>() || SelectedNode->IsA<FChannelGroupModel>())
			{
				TViewModelPtr<IOutlinerExtension> TrackModel = CastViewModelChecked<IOutlinerExtension>(SelectedNode);
				if (TrackModel)
				{
					PasteIntoNodes.Add(TrackModel);
				}
			}
		}
	}

	if (PasteIntoNodes.Num() == 0)
	{
		TSharedPtr<FViewModel> Node = VirtualTrackArea.HitTestNode(LocalMousePosition.Y);
		if (Node.IsValid())
		{
			PasteIntoNodes.Add(FViewModelPtr(Node).ImplicitCast());
		}
	}

	return FPasteContextMenuArgs::PasteInto(MoveTemp(PasteIntoNodes), PasteAtTime, Clipboard);
}

void SSequencer::OnPaste()
{
	using namespace UE::Sequencer;

	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();
	if (Sequencer->GetViewModel()->GetSelection()->Outliner.Num() == 0)
	{
		if (!OpenPasteMenu())
		{
			DoPaste();
		}
	}
	else
	{
		if (!DoPaste())
		{
			OpenPasteMenu();
		}
	}
}

bool SSequencer::CanPaste()
{
	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();
	
	FString TextToImport;
	FPlatformApplicationMisc::ClipboardPaste(TextToImport);

	// Attempts to deserialize the text into object bindings/tracks that Sequencer understands.
	if (Sequencer->CanPaste(TextToImport))
	{
		return true;
	}

	return SequencerPtr.Pin()->GetClipboardStack().Num() != 0;
}

bool SSequencer::DoPaste()
{
	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();

	return Sequencer->DoPaste();
}

bool SSequencer::OpenPasteMenu()
{
	TSharedPtr<FPasteContextMenu> ContextMenu;

	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();
	if (Sequencer->GetClipboardStack().Num() != 0)
	{
		FPasteContextMenuArgs Args = GeneratePasteArgs(Sequencer->GetLocalTime().Time.FrameNumber, Sequencer->GetClipboardStack().Last());
		ContextMenu = FPasteContextMenu::CreateMenu(*Sequencer, Args);
	}

	if (!ContextMenu.IsValid() || !ContextMenu->IsValidPaste())
	{
		return false;
	}
	else if (ContextMenu->AutoPaste())
	{
		return true;
	}

	const bool bShouldCloseWindowAfterMenuSelection = true;
	TSharedPtr<FExtender> MenuExtender = MakeShared<FExtender>();
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, SequencerPtr.Pin()->GetCommandBindings(), MenuExtender);

	ContextMenu->PopulateMenu(MenuBuilder, MenuExtender);

	FWidgetPath Path;
	FSlateApplication::Get().FindPathToWidget(AsShared(), Path);
	
	FSlateApplication::Get().PushMenu(
		AsShared(),
		Path,
		MenuBuilder.MakeWidget(),
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu)
		);

	return true;
}

void SSequencer::PasteFromHistory()
{
	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();
	if (Sequencer->GetClipboardStack().Num() == 0)
	{
		return;
	}

	FPasteContextMenuArgs Args = GeneratePasteArgs(Sequencer->GetLocalTime().Time.FrameNumber);
	TSharedPtr<FPasteFromHistoryContextMenu> ContextMenu = FPasteFromHistoryContextMenu::CreateMenu(*Sequencer, Args);

	if (ContextMenu.IsValid())
	{
		const bool bShouldCloseWindowAfterMenuSelection = true;
		TSharedPtr<FExtender> MenuExtender = MakeShared<FExtender>();
		FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, Sequencer->GetCommandBindings(), MenuExtender);

		ContextMenu->PopulateMenu(MenuBuilder, MenuExtender);

		FWidgetPath Path;
		FSlateApplication::Get().FindPathToWidget(AsShared(), Path);
		
		FSlateApplication::Get().PushMenu(
			AsShared(),
			Path,
			MenuBuilder.MakeWidget(),
			FSlateApplication::Get().GetCursorPos(),
			FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu)
			);
	}
}

EVisibility SSequencer::GetDebugVisualizerVisibility() const
{
	return GetSequencerSettings()->ShouldShowDebugVisualization() ? EVisibility::Visible : EVisibility::Collapsed;
}

double SSequencer::GetSpinboxDelta() const
{
	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();
	return Sequencer->GetDisplayRateDeltaFrameCount();
}

bool SSequencer::GetIsSequenceReadOnly() const
{
	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();
	return Sequencer->GetFocusedMovieSceneSequence() ? Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene()->IsReadOnly() : false;
}

void SSequencer::OnSetSequenceReadOnly(ECheckBoxState CheckBoxState)
{
	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();
	
	bool bReadOnly = CheckBoxState == ECheckBoxState::Checked;

	if (Sequencer->GetFocusedMovieSceneSequence())
	{
		UMovieScene* MovieScene = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene();
		const FScopedTransaction Transaction(CheckBoxState == ECheckBoxState::Checked ? LOCTEXT("LockMovieScene", "Lock Movie Scene") : LOCTEXT("UnlockMovieScene", "Unlock Movie Scene") );

		MovieScene->Modify();
		MovieScene->SetReadOnly(bReadOnly);

		TArray<UMovieScene*> DescendantMovieScenes;
		MovieSceneHelpers::GetDescendantMovieScenes(Sequencer->GetFocusedMovieSceneSequence(), DescendantMovieScenes);

		for (UMovieScene* DescendantMovieScene : DescendantMovieScenes)
		{
			if (DescendantMovieScene && bReadOnly != DescendantMovieScene->IsReadOnly())
			{
				DescendantMovieScene->Modify();
				DescendantMovieScene->SetReadOnly(bReadOnly);
			}
		}

		Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::Unknown);
	}
}

void SSequencer::SetPlayTimeClampedByWorkingRange(double Frame)
{
	if (SequencerPtr.IsValid())
	{
		TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();
		// Some of our spin boxes need to use an unbounded min/max so that they can drag linearly instead of based on the current value.
		// We clamp the value here by the working range to emulate the behavior of the Cinematic Level Viewport
		FFrameRate   PlayRate = Sequencer->GetLocalTime().Rate;
		UMovieScene* MovieScene = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene();
		double StartInSeconds = MovieScene->GetEditorData().WorkStart;
		double EndInSeconds = MovieScene->GetEditorData().WorkEnd;

		Frame = FMath::Clamp(Frame, (double)(StartInSeconds*PlayRate).GetFrame().Value, (double)(EndInSeconds*PlayRate).GetFrame().Value);

		Sequencer->SetLocalTime(FFrameTime::FromDecimal(Frame));
	}
}

void SSequencer::SetPlayTime(double Frame)
{
	if (SequencerPtr.IsValid())
	{
		FFrameTime NewFrame = FFrameTime::FromDecimal(Frame);

		TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();
		FFrameRate PlayRate = Sequencer->GetLocalTime().Rate;
		double FrameInSeconds = PlayRate.AsSeconds(NewFrame);

		TRange<double> NewWorkingRange = Sequencer->GetClampRange();
		if (FrameInSeconds < NewWorkingRange.GetLowerBoundValue())
		{
			NewWorkingRange.SetLowerBoundValue(FrameInSeconds);
			NewWorkingRange.SetLowerBoundValue(UE::MovieScene::ExpandRange(NewWorkingRange, NewWorkingRange.Size<double>() * 0.1f).GetLowerBoundValue());
		}
		if (FrameInSeconds > NewWorkingRange.GetUpperBoundValue())
		{
			NewWorkingRange.SetUpperBoundValue(FrameInSeconds);
			NewWorkingRange.SetUpperBoundValue(UE::MovieScene::ExpandRange(NewWorkingRange, NewWorkingRange.Size<double>() * 0.1f).GetUpperBoundValue());
		}

		TRange<double> NewViewRange = Sequencer->GetViewRange();
		if (FrameInSeconds < NewViewRange.GetLowerBoundValue())
		{
			NewViewRange.SetLowerBoundValue(FrameInSeconds);
			NewViewRange.SetLowerBoundValue(UE::MovieScene::ExpandRange(NewViewRange, NewViewRange.Size<double>() * 0.1f).GetLowerBoundValue());
		}
		if (FrameInSeconds > NewViewRange.GetUpperBoundValue())
		{
			NewViewRange.SetUpperBoundValue(FrameInSeconds);
			NewViewRange.SetUpperBoundValue(UE::MovieScene::ExpandRange(NewViewRange, NewViewRange.Size<double>() * 0.1f).GetUpperBoundValue());
		}

		Sequencer->SetClampRange(NewWorkingRange);
		
		Sequencer->SetViewRange(NewViewRange);
		
		Sequencer->SetLocalTime(NewFrame);

		// Refocus on the previously focused widget so that user can continue on after setting a time
		if (PlayTimeDisplay)
		{
			PlayTimeDisplay->Refocus();
		}
	}
}

void SSequencer::ApplySequencerCustomizations(const TArrayView<const FSequencerCustomizationInfo> Customizations)
{
	AddMenuExtenders.Reset();
	ToolbarExtenders.Reset();

	OnReceivedDragOver.Reset();
	OnReceivedDrop.Reset();
	OnAssetsDrop.Reset();
	OnActorsDrop.Reset();
	OnClassesDrop.Reset();
	OnFoldersDrop.Reset();

	ApplySequencerCustomization(RootCustomization);
	for (const FSequencerCustomizationInfo& Info : Customizations)
	{
		ApplySequencerCustomization(Info);
	}

	ToolbarContainer->SetContent(MakeToolBar());
}

void SSequencer::ApplySequencerCustomization(const FSequencerCustomizationInfo& Customization)
{
	if (Customization.AddMenuExtender != nullptr)
	{
		AddMenuExtenders.Add(Customization.AddMenuExtender);
	}
	if (Customization.ToolbarExtender != nullptr)
	{
		ToolbarExtenders.Add(Customization.ToolbarExtender);
	}

	if (Customization.OnReceivedDragOver.IsBound())
	{
		OnReceivedDragOver.Add(Customization.OnReceivedDragOver);
	}
	if (Customization.OnReceivedDrop.IsBound())
	{
		OnReceivedDrop.Add(Customization.OnReceivedDrop);
	}
	if (Customization.OnAssetsDrop.IsBound())
	{
		OnAssetsDrop.Add(Customization.OnAssetsDrop);
	}
	if (Customization.OnActorsDrop.IsBound())
	{
		OnActorsDrop.Add(Customization.OnActorsDrop);
	}
	if (Customization.OnClassesDrop.IsBound())
	{
		OnClassesDrop.Add(Customization.OnClassesDrop);
	}
	if (Customization.OnFoldersDrop.IsBound())
	{
		OnFoldersDrop.Add(Customization.OnFoldersDrop);
	}
}

USequencerSettings* SSequencer::GetSequencerSettings() const
{
	if (SequencerPtr.IsValid())
	{
		return SequencerPtr.Pin()->GetSequencerSettings();
	}
	return nullptr;
}

#undef LOCTEXT_NAMESPACE

