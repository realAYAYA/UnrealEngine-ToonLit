// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchDebuggerView.h"
#include "Modules/ModuleManager.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchDerivedData.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "PoseSearchDebugger.h"
#include "PoseSearchDebuggerDatabaseRowData.h"
#include "PoseSearchDebuggerDatabaseView.h"
#include "PoseSearchDebuggerReflection.h"
#include "PoseSearchDebuggerViewModel.h"
#include "PropertyEditorModule.h"
#include "Trace/PoseSearchTraceProvider.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"

#define LOCTEXT_NAMESPACE "PoseSearchDebugger"

namespace UE::PoseSearch
{

class SDebuggerMessageBox : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SDebuggerMessageBox) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const FString& Message)
	{
		ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Message))
				.Font(FAppStyle::Get().GetFontStyle("DetailsView.CategoryFontStyle"))
			]
		];
	}
};

void SDebuggerDetailsView::Construct(const FArguments& InArgs)
{
	ParentDebuggerViewPtr = InArgs._Parent;

	// Add property editor (detail view) UObject to world root so that it persists when PIE is stopped
	Reflection = NewObject<UPoseSearchDebuggerReflection>();
	Reflection->AddToRoot();
	check(IsValid(Reflection));

	// @TODO: Convert this to a custom builder instead of of a standard details view
	// Load property module and create details view with our reflection UObject
	FPropertyEditorModule& PropPlugin = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
    DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.DefaultsOnlyVisibility = EEditDefaultsOnlyNodeVisibility::Hide;

	Details = PropPlugin.CreateDetailView(DetailsViewArgs);
	Details->SetObject(Reflection);
	
	ChildSlot
	[
		Details.ToSharedRef()
	];
}

void SDebuggerDetailsView::Update(const FTraceMotionMatchingStateMessage& State) const
{
	UpdateReflection(State);
}

SDebuggerDetailsView::~SDebuggerDetailsView()
{
	// Our previously instantiated object attached to root may be cleaned up at this point
	if (UObjectInitialized())
	{
		Reflection->RemoveFromRoot();
	}
}

void SDebuggerDetailsView::UpdateReflection(const FTraceMotionMatchingStateMessage& State) const
{
	check(Reflection);

	const UPoseSearchDatabase* CurrentDatabase = State.GetCurrentDatabase();
	if (FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(CurrentDatabase, ERequestAsyncBuildFlag::ContinueRequest))
	{
		const FPoseSearchIndex& CurrentSearchIndex = CurrentDatabase->GetSearchIndex();
		int32 CurrentDbPoseIdx = State.GetCurrentDatabasePoseIndex();

		Reflection->CurrentDatabaseName = CurrentDatabase->GetName();
		Reflection->ElapsedPoseJumpTime = State.ElapsedPoseJumpTime;

		Reflection->AssetPlayerAssetName = "None";
		if (const FPoseSearchIndexAsset* IndexAsset = CurrentSearchIndex.GetAssetForPoseSafe(CurrentDbPoseIdx))
		{
			Reflection->AssetPlayerAssetName = CurrentDatabase->GetSourceAssetName(*IndexAsset);
		}

		Reflection->AssetPlayerTime = State.AssetPlayerTime;
		Reflection->LastDeltaTime = State.DeltaTime;
		Reflection->SimLinearVelocity = State.SimLinearVelocity;
		Reflection->SimAngularVelocity = State.SimAngularVelocity;
		Reflection->AnimLinearVelocity = State.AnimLinearVelocity;
		Reflection->AnimAngularVelocity = State.AnimAngularVelocity;

		// Query pose
		Reflection->QueryPoseVector.Reset();
		for (const FTraceMotionMatchingStateDatabaseEntry& DbEntry : State.DatabaseEntries)
		{
			const UPoseSearchDatabase* Database = FTraceMotionMatchingState::GetObjectFromId<UPoseSearchDatabase>(DbEntry.DatabaseId);
			if (Database && Database == CurrentDatabase)
			{
				Reflection->QueryPoseVector = DbEntry.QueryVector;
				break;
			}
		}

		// Active pose
		Reflection->ActivePoseVector = CurrentSearchIndex.GetPoseValuesSafe(CurrentDbPoseIdx);
	}

	auto DebuggerView = ParentDebuggerViewPtr.Pin();
	if (DebuggerView.IsValid())
	{
		TArray<TSharedRef<FDebuggerDatabaseRowData>> SelectedRows = DebuggerView->GetSelectedDatabaseRows();
		if (!SelectedRows.IsEmpty())
		{
			const TSharedRef<FDebuggerDatabaseRowData>& Selected = SelectedRows[0];
			if (FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(Selected->SourceDatabase.Get(), ERequestAsyncBuildFlag::ContinueRequest))
			{
				const FPoseSearchIndex& SelectedSearchIndex = Selected->SourceDatabase->GetSearchIndex();
				Reflection->SelectedPoseVector = SelectedSearchIndex.GetPoseValuesSafe(Selected->PoseIdx);
			}
			Reflection->CostVector = Selected->CostVector;
		}
	}
}

void SDebuggerView::Construct(const FArguments& InArgs, uint64 InAnimInstanceId)
{
	ViewModel = InArgs._ViewModel;
	OnViewClosed = InArgs._OnViewClosed;
	
	// Validate the existence of the passed getters
	check(ViewModel.IsBound())
	check(OnViewClosed.IsBound());
	
	AnimInstanceId = InAnimInstanceId;
	SelectedNodeId = INDEX_NONE;

	ChildSlot
	[
		SAssignNew(DebuggerView, SVerticalBox)
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Fill)
		[
			SAssignNew(Switcher, SWidgetSwitcher)
			.WidgetIndex(this, &SDebuggerView::SelectView)

			// [0] Selection view before node selection is made
			+ SWidgetSwitcher::Slot()
			.Padding(40.0f)
			.HAlign(HAlign_Fill)
            .VAlign(VAlign_Center)
			[
				SAssignNew(SelectionView, SVerticalBox)
			]

			// [1] Node selected; node debugger view
			+ SWidgetSwitcher::Slot()
			[
				GenerateNodeDebuggerView()
			]

			// [2] Occluding message box when stopped (no recording)
			+ SWidgetSwitcher::Slot()
			[
				SNew(SDebuggerMessageBox, "Record gameplay to begin debugging")
			]

			// [3] Occluding message box when recording
			+ SWidgetSwitcher::Slot()
			[
				SNew(SDebuggerMessageBox, "Recording...")
			]
			
			// [4] Occluding message box when there is no data for the selected MM node
			+ SWidgetSwitcher::Slot()
			[
				GenerateNoDataMessageView()
			]
		]
	];
}

void SDebuggerView::SetTimeMarker(double InTimeMarker)
{
	if (FDebugger::IsPIESimulating())
	{
		return;
	}

	TimeMarker = InTimeMarker;
}

void SDebuggerView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (FDebugger::IsPIESimulating())
	{
		return;
	}

	const UWorld* DebuggerWorld = FDebugger::GetWorld();
    check(DebuggerWorld);
	
	// @TODO: Handle editor world when those features are enabled for the Rewind Debugger
	// Currently prevents debug draw remnants from stopped world
	if (DebuggerWorld->WorldType != EWorldType::PIE)
	{
		return;
	}
	
	const bool bSameTime = FMath::Abs(TimeMarker - PreviousTimeMarker) < DOUBLE_SMALL_NUMBER;
	PreviousTimeMarker = TimeMarker;

	TSharedPtr<FDebuggerViewModel> Model = ViewModel.Get();

	bool bNeedUpdate = Model->HasSearchableAssetChanged();

	// We haven't reached the update point yet
	if (CurrentConsecutiveFrames < ConsecutiveFramesUpdateThreshold)
	{
		// If we're on the same time marker, it is consecutive
		if (bSameTime)
		{
			++CurrentConsecutiveFrames;
		}
	}
	else
	{
		// New frame after having updated, reset consecutive frames count and start counting again
		if (!bSameTime)
		{
			CurrentConsecutiveFrames = 0;
			bUpdated = false;
		}
		// Haven't updated since passing through frame gate, update once
		else if (!bUpdated)
		{
			bNeedUpdate = true;
		}
	}

	if (bNeedUpdate)
	{
		Model->OnUpdate();
		if (UpdateNodeSelection())
		{
			Model->OnUpdateNodeSelection(SelectedNodeId);
			UpdateViews();
		}
		bUpdated = true;
	}

	Model->UpdateAsset();
	
	// Draw visualization every tick
	DrawVisualization();
}

bool SDebuggerView::UpdateNodeSelection()
{
	TSharedPtr<FDebuggerViewModel> Model = ViewModel.Get();

	// Update selection view if no node selected
	bool bNodeSelected = SelectedNodeId != INDEX_NONE;
	if (!bNodeSelected)
	{
		const TArray<int32>& NodeIds = *Model->GetNodeIds();
		// Only one node active, bypass selection view
		if (NodeIds.Num() == 1)
		{
			SelectedNodeId = *NodeIds.begin();
			bNodeSelected = true;
		}
		// Create selection view with buttons for each node, displaying the database name
		else
		{
			SelectionView->ClearChildren();
			for (int32 NodeId : NodeIds)
			{
				Model->OnUpdateNodeSelection(NodeId);
				SelectionView->AddSlot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Center)
				.Padding(10.0f)
				[
					SNew(SButton)
					.Text(FText::FromString(Model->GetSearchableAsset()->GetName()))
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.ContentPadding(10.0f)
					.OnClicked(this, &SDebuggerView::OnUpdateNodeSelection, NodeId)
				];
			}
		}
	}

	return bNodeSelected;
}

void SDebuggerView::UpdateViews() const
{
	const FTraceMotionMatchingStateMessage* State = ViewModel.Get()->GetMotionMatchingState();
	if (State)
	{
		DatabaseView->Update(*State);
		DetailsView->Update(*State);
	}
}

void SDebuggerView::DrawVisualization() const
{
	const UWorld* DebuggerWorld = FDebugger::GetWorld();
	check(DebuggerWorld);

	const FTraceMotionMatchingStateMessage* State = ViewModel.Get()->GetMotionMatchingState();
	const FTransform* Transform = ViewModel.Get()->GetRootTransform();
	if (State && Transform)
	{
		DrawFeatures(*DebuggerWorld, *State, *Transform, ViewModel.Get()->GetMeshComponent());
	}
}

TArray<TSharedRef<FDebuggerDatabaseRowData>> SDebuggerView::GetSelectedDatabaseRows() const
{
	return DatabaseView->GetDatabaseRows()->GetSelectedItems();
}

void SDebuggerView::DrawFeatures(
	const UWorld& DebuggerWorld,
	const FTraceMotionMatchingStateMessage& State,
	const FTransform& Transform,
	const USkinnedMeshComponent* Mesh
) const
{
	auto SetDrawFlags = [](FDebugDrawParams& InDrawParams, const FPoseSearchDebuggerFeatureDrawOptions& Options)
	{
		InDrawParams.Flags = EDebugDrawFlags::None;
		if (!Options.bDisable)
		{
			if (Options.bDrawBoneNames)
			{
				EnumAddFlags(InDrawParams.Flags, EDebugDrawFlags::DrawBoneNames);
			}
			
			if (Options.bDrawSampleLabels)
			{
				EnumAddFlags(InDrawParams.Flags, EDebugDrawFlags::DrawSampleLabels);
			}
		}
	};

	const TObjectPtr<UPoseSearchDebuggerReflection> Reflection = DetailsView->GetReflection();

	// Draw query vector
	{
		const UPoseSearchDatabase* CurrentDatabase = ViewModel.Get()->GetCurrentDatabase();
		if (CurrentDatabase)
		{
			for (const FTraceMotionMatchingStateDatabaseEntry& DbEntry : State.DatabaseEntries)
			{
				const UPoseSearchDatabase* Database = FTraceMotionMatchingState::GetObjectFromId<UPoseSearchDatabase>(DbEntry.DatabaseId);
				if (Database && Database == CurrentDatabase && DbEntry.QueryVector.Num() == Database->Schema->SchemaCardinality && 
					FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(CurrentDatabase, ERequestAsyncBuildFlag::ContinueRequest))
				{
					// Set shared state
					FDebugDrawParams DrawParams;
					DrawParams.World = &DebuggerWorld;
					DrawParams.RootTransform = Transform;
					DrawParams.DefaultLifeTime = 0.0f; // Single frame render
					DrawParams.Mesh = Mesh;
					DrawParams.Database = CurrentDatabase;
					SetDrawFlags(DrawParams, Reflection ? Reflection->QueryDrawOptions : FPoseSearchDebuggerFeatureDrawOptions());
					EnumAddFlags(DrawParams.Flags, EDebugDrawFlags::DrawQuery);
					DrawFeatureVector(DrawParams, DbEntry.QueryVector);
					EnumRemoveFlags(DrawParams.Flags, EDebugDrawFlags::DrawQuery);
					break;
				}
			}
		}
	}

	// Draw selected poses
	{
		const TSharedPtr<SListView<TSharedRef<FDebuggerDatabaseRowData>>>& DatabaseRows = DatabaseView->GetDatabaseRows();
		TArray<TSharedRef<FDebuggerDatabaseRowData>> SelectedRows = DatabaseRows->GetSelectedItems();
	
		// Red for non-active database view
		FDebugDrawParams DrawParams;
		DrawParams.World = &DebuggerWorld;
		DrawParams.RootTransform = Transform;
		DrawParams.DefaultLifeTime = 0.0f; // Single frame render
		DrawParams.Mesh = Mesh;
		SetDrawFlags(DrawParams, Reflection ? Reflection->SelectedPoseDrawOptions : FPoseSearchDebuggerFeatureDrawOptions());

		// Draw any selected database vectors
		for (const TSharedRef<FDebuggerDatabaseRowData>& Row : SelectedRows)
		{
			DrawParams.Database = Row->SourceDatabase.Get();
			DrawFeatureVector(DrawParams, Row->PoseIdx);
		}
	}


	// Draw active pose
	{
		TArray<TSharedRef<FDebuggerDatabaseRowData>> ActiveRows = DatabaseView->GetActiveRow()->GetSelectedItems();

		// Active row should only have 0 or 1
		check(ActiveRows.Num() < 2);

		if (!ActiveRows.IsEmpty())
		{		
			// Use the motion-matching state's pose idx, as the active row may be update-throttled at this point
			FDebugDrawParams DrawParams;
			DrawParams.World = &DebuggerWorld;
			DrawParams.RootTransform = Transform;
			DrawParams.DefaultLifeTime = 0.0f; // Single frame render
			DrawParams.Mesh = Mesh;
			DrawParams.Database = ActiveRows[0]->SourceDatabase.Get();
			DrawFeatureVector(DrawParams, ActiveRows[0]->PoseIdx);
		}
	}


	// Draw continuing pose
	{
		TArray<TSharedRef<FDebuggerDatabaseRowData>> ContinuingRows = DatabaseView->GetContinuingPoseRow()->GetSelectedItems();

		// ContinuingPose row should only have 0 or 1
		check(ContinuingRows.Num() < 2);

		if (!ContinuingRows.IsEmpty())
		{
			FDebugDrawParams DrawParams;
			DrawParams.World = &DebuggerWorld;
			DrawParams.RootTransform = Transform;
			DrawParams.DefaultLifeTime = 0.0f; // Single frame render
			DrawParams.Mesh = Mesh;
			DrawParams.Database = ContinuingRows[0]->SourceDatabase.Get();
			DrawFeatureVector(DrawParams, ContinuingRows[0]->PoseIdx);
		}
	}


	// Draw skeleton
	{
		FSkeletonDrawParams SkeletonDrawParams;
		if (Reflection && Reflection->bDrawSelectedSkeleton)
		{
			SkeletonDrawParams.Flags |= ESkeletonDrawFlags::SelectedPose;
		}
		if (Reflection && Reflection->bDrawActiveSkeleton)
		{
			SkeletonDrawParams.Flags |= ESkeletonDrawFlags::ActivePose;
		}

		SkeletonDrawParams.Flags |= ESkeletonDrawFlags::Asset;

		ViewModel.Get()->OnDraw(SkeletonDrawParams);
	}
}

int32 SDebuggerView::SelectView() const
{
	// Currently recording
	if (FDebugger::IsPIESimulating() && FDebugger::IsRecording())
	{
		return RecordingMsg;
	}

	// Data has not been recorded yet
	if (FDebugger::GetRecordingDuration() < DOUBLE_SMALL_NUMBER)
	{
		return StoppedMsg;
	}

	const TSharedPtr<FDebuggerViewModel> Model = ViewModel.Get();

	const bool bNoActiveNodes = Model->GetNodesNum() == 0;
	const bool bNodeSelectedWithoutData = SelectedNodeId != INDEX_NONE && Model->GetMotionMatchingState() == nullptr;

	// No active nodes, or node selected has no data
	if (bNoActiveNodes || bNodeSelectedWithoutData)
    {
    	return NoDataMsg;
    }

	// Node not selected yet, showcase selection view
	if (SelectedNodeId == INDEX_NONE)
	{
		return Selection;
	}

	// Standard debugger view
	return Debugger;
}

void SDebuggerView::OnPoseSelectionChanged(const UPoseSearchDatabase* Database, int32 DbPoseIdx, float Time)
{
	const TSharedPtr<FDebuggerViewModel> Model = ViewModel.Get();
	const FTraceMotionMatchingStateMessage* State = Model->GetMotionMatchingState();

	if (State)
	{
		DetailsView->Update(*State);
	}
	
	if (DbPoseIdx == INDEX_NONE)
	{
		Model->ClearSelectedSkeleton();
	}
	else
	{
		Model->ShowSelectedSkeleton(Database, DbPoseIdx, Time);

		// Stop asset player when switching selections
		Model->StopSelection();
	}
}

FReply SDebuggerView::OnUpdateNodeSelection(int32 InSelectedNodeId)
{
	// -1 will backtrack to selection view
	SelectedNodeId = InSelectedNodeId;
	bUpdated = false;
	return FReply::Handled();
}

FReply SDebuggerView::TogglePlaySelectedAssets() const
{
	const TSharedPtr<SListView<TSharedRef<FDebuggerDatabaseRowData>>>& DatabaseRows = DatabaseView->GetDatabaseRows();
	TArray<TSharedRef<FDebuggerDatabaseRowData>> Selected = DatabaseRows->GetSelectedItems();
	const bool bPlaying = ViewModel.Get()->IsPlayingSelections();
	if (!bPlaying)
	{
		if (!Selected.IsEmpty())
		{
			// @TODO: Make functional with multiple poses being selected
			ViewModel.Get()->PlaySelection(Selected[0]->PoseIdx, Selected[0]->AssetTime);
		}
	}
	else
	{
		ViewModel.Get()->StopSelection();
	}

	return FReply::Handled();
}

TSharedRef<SWidget> SDebuggerView::GenerateNoDataMessageView()
{
	TSharedRef<SWidget> ReturnButtonView = GenerateReturnButtonView();
	ReturnButtonView->SetVisibility(TAttribute<EVisibility>::CreateLambda([this]() -> EVisibility
	{
		// Hide the return button for the no data message if we have no nodes at all
		return ViewModel.Get()->GetNodesNum() > 0 ? EVisibility::Visible : EVisibility::Hidden;
	}));
	
	return 
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		[
			SNew(SDebuggerMessageBox, "No recorded data available for the selected frame")
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(20.0f)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			ReturnButtonView
		];
}

TSharedRef<SHorizontalBox> SDebuggerView::GenerateReturnButtonView()
{
	return
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.Padding(10, 5, 0, 0)
		.AutoWidth()
		[
			SNew(SButton)
			.VAlign(VAlign_Center)
			.Visibility_Lambda([this] { return ViewModel.Get()->GetNodesNum() > 1 ? EVisibility::Visible : EVisibility::Hidden; })
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.ContentPadding( FMargin(1, 0) )
			.OnClicked(this, &SDebuggerView::OnUpdateNodeSelection, static_cast<int32>(INDEX_NONE))
			// Contents of button, icon then text
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("Icons.CircleArrowLeft"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FMargin(8.0f, 0.0f, 0.0f, 0.0f))
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString("Return to Database Selection"))
					.Justification(ETextJustify::Center)
				]
			]
		];
}

TSharedRef<SWidget> SDebuggerView::GenerateNodeDebuggerView()
{
	TSharedRef<SHorizontalBox> ReturnButtonView = GenerateReturnButtonView();
	ReturnButtonView->AddSlot()
	.VAlign(VAlign_Center)
	.HAlign(HAlign_Fill)
	.Padding(32, 5, 0, 0)
	.AutoWidth()
	[
		SNew(SButton)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Fill)
		.ButtonStyle(FAppStyle::Get(), "Button")
		.ContentPadding( FMargin(5, 0) )
		.OnClicked(this, &SDebuggerView::TogglePlaySelectedAssets)
		[
			SNew(SHorizontalBox)
			// Icon
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SImage)
				.Image_Lambda([this]
				{
					const bool bPlayingSelections = ViewModel.Get()->IsPlayingSelections();
					return FSlateIcon("FAppStyle", bPlayingSelections ? "PlayWorld.StopPlaySession.Small" : "PlayWorld.PlayInViewport.Small").GetSmallIcon();
				})
			]
			// Text
			+ SHorizontalBox::Slot()
			.Padding(FMargin(8, 0, 0, 0))
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text_Lambda([this] { return ViewModel.Get()->IsPlayingSelections() ? FText::FromString("Stop Selected Asset") : FText::FromString("Play Selected Asset"); })
				.Justification(ETextJustify::Center)
			]
		]
	];
	
	ReturnButtonView->AddSlot()
	.VAlign(VAlign_Center)
	.HAlign(HAlign_Left)
	.Padding(64, 5, 0, 0)
	.AutoWidth()
	[
		SNew(SHorizontalBox)
		
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0, 5, 0, 0)
		[
			SNew(STextBlock)
			.Text(FText::FromString("Asset Play Rate: "))
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(8, 0, 0, 0)
		[
			SNew(SNumericEntryBox<float>)
			.MinValue(0.0f)
			.MaxValue(5.0f)
			.MinSliderValue(0.0f)
			.MaxSliderValue(5.0f)
			.Delta(0.01f)
			.AllowSpin(true)
			// Lambda to accomodate the TOptional this requires (for now)
			.Value_Lambda([this] { return ViewModel.Get()->GetPlayRate(); })
			.OnValueChanged(ViewModel.Get().ToSharedRef(), &FDebuggerViewModel::ChangePlayRate)	
		]
	];

	ReturnButtonView->AddSlot()
	.VAlign(VAlign_Center)
	.HAlign(HAlign_Left)
	.Padding(64, 5, 0, 0)
	.AutoWidth()
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0, 5, 0, 0)
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([this]
			{
				return ViewModel.Get()->IsVerbose() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; 
			})
			.OnCheckStateChanged_Lambda([this](ECheckBoxState State)
			{
				ViewModel.Get()->SetVerbose(State == ECheckBoxState::Checked);
				UpdateViews();
			})
			[
				SNew(STextBlock)
				.Text(LOCTEXT("PoseSearchDebuggerShowVerbose", "Channels Breakdown"))
			]
		]
	];

	return 
		SNew(SSplitter)
		.Orientation(Orient_Horizontal)
		.ResizeMode(ESplitterResizeMode::Fill)
	
		// Database view
		+ SSplitter::Slot()
		.Value(0.65f)
		[
			SNew(SVerticalBox)
			
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				ReturnButtonView
			]
			
			+ SVerticalBox::Slot()
			[
				SAssignNew(DatabaseView, SDebuggerDatabaseView)
				.Parent(SharedThis(this))
				.OnPoseSelectionChanged(this, &SDebuggerView::OnPoseSelectionChanged)
			]
		]

		// Details panel view
		+ SSplitter::Slot()
		.Value(0.35f)
		[
			SAssignNew(DetailsView, SDebuggerDetailsView)
			.Parent(SharedThis(this))
		];
}

FName SDebuggerView::GetName() const
{
	static const FName DebuggerName("PoseSearchDebugger");
	return DebuggerName;
}

uint64 SDebuggerView::GetObjectId() const
{
	return AnimInstanceId;
}

SDebuggerView::~SDebuggerView()
{
	OnViewClosed.Execute(AnimInstanceId);
}

} // namespace UE::PoseSearch

#undef LOCTEXT_NAMESPACE
