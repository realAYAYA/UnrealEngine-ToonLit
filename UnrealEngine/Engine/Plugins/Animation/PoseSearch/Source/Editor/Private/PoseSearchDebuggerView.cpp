// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchDebuggerView.h"
#include "Modules/ModuleManager.h"
#include "Editor.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchDerivedData.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "PoseSearchDatabaseEditor.h"
#include "PoseSearchDebugger.h"
#include "PoseSearchDebuggerDatabaseRowData.h"
#include "PoseSearchDebuggerDatabaseView.h"
#include "PoseSearchDebuggerReflection.h"
#include "PoseSearchDebuggerViewModel.h"
#include "PoseSearchEditor.h"
#include "PropertyEditorModule.h"
#include "Subsystems/AssetEditorSubsystem.h"
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

	Reflection->ElapsedPoseSearchTime = State.ElapsedPoseSearchTime;
	Reflection->AssetPlayerTime = State.AssetPlayerTime;
	Reflection->LastDeltaTime = State.DeltaTime;
	Reflection->SimLinearVelocity = State.SimLinearVelocity;
	Reflection->SimAngularVelocity = State.SimAngularVelocity;
	Reflection->AnimLinearVelocity = State.AnimLinearVelocity;
	Reflection->AnimAngularVelocity = State.AnimAngularVelocity;
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
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		[
			SAssignNew(Switcher, SWidgetSwitcher)
			.WidgetIndex(this, &SDebuggerView::SelectView)

			// [0] Selection view before node selection is made
			+ SWidgetSwitcher::Slot()
			.Padding(40.0f)
			.HAlign(HAlign_Fill)
            .VAlign(VAlign_Fill)
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

	// @todo: Handle editor world when those features are enabled for the Rewind Debugger
	// Currently prevents debug draw remnants from stopped world
	if (DebuggerWorld->WorldType != EWorldType::PIE)
	{
		return;
	}

	const bool bSameTime = FMath::Abs(TimeMarker - PreviousTimeMarker) < DOUBLE_SMALL_NUMBER;
	PreviousTimeMarker = TimeMarker;

	TSharedPtr<FDebuggerViewModel> Model = ViewModel.Get();
	check(Model.IsValid());

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
			Model->OnUpdate();
			if (UpdateNodeSelection())
			{
				Model->OnUpdateNodeSelection(SelectedNodeId);
				UpdateViews();
			}
			bUpdated = true;
		}
	}

	// Draw features
	if (const FTraceMotionMatchingStateMessage* State = Model->GetMotionMatchingState())
	{
		FRoleToIndex RoleToIndex;
		TArray<const USkinnedMeshComponent*> Meshes;
		TArray<const IPoseHistory*> PoseHistories;

		const int32 NumRoles = State->Roles.Num();

		RoleToIndex.Reserve(NumRoles);
		Meshes.SetNum(NumRoles);
		PoseHistories.SetNum(NumRoles);

		for (int32 RoleIndex = 0; RoleIndex < NumRoles; ++RoleIndex)
		{
			const uint64 ActorSkeletalMeshComponentId = State->SkeletalMeshComponentIds[RoleIndex];
			if (const TWeakObjectPtr<AActor>* ActorPtr = Model->GetDebugDrawActors().Find(ActorSkeletalMeshComponentId))
			{
				if (ActorPtr->IsValid())
				{
					const FRole& Role = State->Roles[RoleIndex];

					for (UActorComponent* ActorComponent : (*ActorPtr)->GetInstanceComponents())
					{
						if (UPoseSearchMeshComponent* PoseSearchMeshComponent = Cast<UPoseSearchMeshComponent>(ActorComponent))
						{
							RoleToIndex.Add(Role) = RoleIndex;
							Meshes[RoleIndex] = PoseSearchMeshComponent;
							PoseHistories[RoleIndex] = &State->PoseHistories[RoleIndex];
							break;
						}
					}
				}
			}
		}

		// checking if all roles have been resolved properly
		if (RoleToIndex.Num() != NumRoles)
		{
			return;
		}

		// Draw world space trajectory
#if ENABLE_ANIM_DEBUG
		const bool bDrawTrajectory = Model->GetDrawTrajectory();
		const bool bDrawHistory = Model->GetDrawHistory();

		if (bDrawTrajectory || bDrawHistory)
		{
			for (int32 RoleIndex = 0; RoleIndex < NumRoles; ++RoleIndex)
			{
				UWorld* World = Meshes[RoleIndex]->GetWorld();
				const FPoseSearchQueryTrajectory& Trajectory = State->PoseHistories[RoleIndex].Trajectory;
				
				if (bDrawTrajectory)
				{
					Trajectory.DebugDrawTrajectory(World);
				}

				if (bDrawHistory)
				{
					State->PoseHistories[RoleIndex].DebugDraw(World, FColor::Red);
				}
			}
		}
#endif

		// Draw query vector
		if (Model->GetDrawQuery())
		{
			if (const UPoseSearchDatabase* CurrentDatabase = Model->GetCurrentDatabase())
			{
				for (const FTraceMotionMatchingStateDatabaseEntry& DbEntry : State->DatabaseEntries)
				{
					const UPoseSearchDatabase* Database = FTraceMotionMatchingStateMessage::GetObjectFromId<UPoseSearchDatabase>(DbEntry.DatabaseId);
					if (Database && Database == CurrentDatabase &&
						EAsyncBuildIndexResult::Success == FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(CurrentDatabase, ERequestAsyncBuildFlag::ContinueRequest) &&
						DbEntry.QueryVector.Num() == Database->Schema->SchemaCardinality)
					{
						FDebugDrawParams DrawParams(Meshes, PoseHistories, RoleToIndex, CurrentDatabase, EDebugDrawFlags::DrawQuery);
						DrawParams.DrawFeatureVector(DbEntry.QueryVector);
						break;
					}
				}
			}
		}

		// Draw selected poses
		const TSharedPtr<SListView<TSharedRef<FDebuggerDatabaseRowData>>>& DatabaseRows = DatabaseView->GetDatabaseRows();
		TArray<TSharedRef<FDebuggerDatabaseRowData>> SelectedRows = DatabaseRows->GetSelectedItems();

		// Draw any selected database vectors
		constexpr int32 MaxRowsToDraw = 250;

		const int32 NumRowsToDraw = FMath::Min(MaxRowsToDraw, SelectedRows.Num());
		for (int32 RowIdx = 0; RowIdx < NumRowsToDraw; ++RowIdx)
		{
			const TSharedRef<FDebuggerDatabaseRowData>& Row = SelectedRows[RowIdx];
			const UPoseSearchDatabase* RowDatabase = Row->SharedData->SourceDatabase.Get();
			if (EAsyncBuildIndexResult::Success == FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(RowDatabase, ERequestAsyncBuildFlag::ContinueRequest))
			{
				FDebugDrawParams DrawParams(Meshes, PoseHistories, RoleToIndex, RowDatabase);
				DrawParams.DrawFeatureVector(Row->PoseIdx);
			}
		}

		// Draw active pose
		TArray<TSharedRef<FDebuggerDatabaseRowData>> ActiveRows = DatabaseView->GetActiveRow()->GetSelectedItems();

		// Active row should only have 0 or 1
		check(ActiveRows.Num() < 2);

		if (!ActiveRows.IsEmpty())
		{
			const UPoseSearchDatabase* Database = ActiveRows[0]->SharedData->SourceDatabase.Get();
			if (EAsyncBuildIndexResult::Success == FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(Database, ERequestAsyncBuildFlag::ContinueRequest))
			{
				// Use the motion-matching state's pose idx, as the active row may be update-throttled at this point
				FDebugDrawParams DrawParams(Meshes, PoseHistories, RoleToIndex, Database);
				DrawParams.DrawFeatureVector(ActiveRows[0]->PoseIdx);
			}
		}


		// Draw continuing pose
		TArray<TSharedRef<FDebuggerDatabaseRowData>> ContinuingRows = DatabaseView->GetContinuingPoseRow()->GetSelectedItems();

		// ContinuingPose row should only have 0 or 1
		check(ContinuingRows.Num() < 2);

		if (!ContinuingRows.IsEmpty())
		{
			const UPoseSearchDatabase* Database = ContinuingRows[0]->SharedData->SourceDatabase.Get();
			if (EAsyncBuildIndexResult::Success == FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(Database, ERequestAsyncBuildFlag::ContinueRequest))
			{
				FDebugDrawParams DrawParams(Meshes, PoseHistories, RoleToIndex, Database);
				DrawParams.DrawFeatureVector(ContinuingRows[0]->PoseIdx);
			}
		}
	}

	// synchronizing the model DrawQuery state with all the open PoseSearchDatabaseEditor(s)
	const bool bDrawQuery = Model->GetDrawQuery();
	if (UAssetEditorSubsystem* AssetEditorSS = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
	{
		TArray<UObject*> EditedAssets = AssetEditorSS->GetAllEditedAssets();
		for (UObject* EditedAsset : EditedAssets)
		{
			if (Cast<UPoseSearchDatabase>(EditedAsset))
			{
				if (IAssetEditorInstance* Editor = AssetEditorSS->FindEditorForAsset(EditedAsset, false))
				{
					if (Editor->GetEditorName() == FName("PoseSearchDatabaseEditor"))
					{
						FDatabaseEditor* DatabaseEditor = static_cast<FDatabaseEditor*>(Editor);
						DatabaseEditor->SetDrawQueryVector(bDrawQuery);
					}
				}
			}
		}
	}
}

bool SDebuggerView::UpdateNodeSelection()
{
	TSharedPtr<FDebuggerViewModel> Model = ViewModel.Get();
	check(Model.IsValid());

	// Update selection view if no node selected
	bool bNodeSelected = SelectedNodeId != INDEX_NONE;
	if (!bNodeSelected)
	{
		const TArray<FTraceMotionMatchingStateMessage>& MotionMatchingStates = Model->GetMotionMatchingStates();
		// Only one active state, bypass selection view
		if (MotionMatchingStates.Num() == 1)
		{
			SelectedNodeId = MotionMatchingStates[0].NodeId;
			bNodeSelected = true;
		}
		// Create selection view with buttons for each node, displaying the database name
		else
		{
			SelectionView->ClearChildren();
			for (const FTraceMotionMatchingStateMessage& MotionMatchingState : MotionMatchingStates)
			{
				Model->OnUpdateNodeSelection(MotionMatchingState.NodeId);
				SelectionView->AddSlot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Center)
				.Padding(10.0f)
				[
					SNew(SButton)
					.Text(FText::FromString(GetNameSafe(Model->GetCurrentDatabase())))
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.ContentPadding(10.0f)
					.OnClicked(this, &SDebuggerView::OnUpdateNodeSelection, MotionMatchingState.NodeId)
				];
			}
		}
	}

	return bNodeSelected;
}

void SDebuggerView::UpdateViews() const
{
	if (const FTraceMotionMatchingStateMessage* State = ViewModel.Get()->GetMotionMatchingState())
	{
		DatabaseView->Update(*State);
		DetailsView->Update(*State);
	}
}

TArray<TSharedRef<FDebuggerDatabaseRowData>> SDebuggerView::GetSelectedDatabaseRows() const
{
	return DatabaseView->GetDatabaseRows()->GetSelectedItems();
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
	check(Model.IsValid());

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
	check(Model.IsValid());

	if (const FTraceMotionMatchingStateMessage* State = Model->GetMotionMatchingState())
	{
		DetailsView->Update(*State);
	}
}

FReply SDebuggerView::OnUpdateNodeSelection(int32 InSelectedNodeId)
{
	// -1 will backtrack to selection view
	SelectedNodeId = InSelectedNodeId;
	bUpdated = false;
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
	return SNew(SHorizontalBox)

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
		]
		+SHorizontalBox::Slot()
		.VAlign(VAlign_Top)
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
					return ViewModel.Get()->GetDrawQuery() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; 
				})
				.OnCheckStateChanged_Lambda([this](ECheckBoxState State)
				{
					ViewModel.Get()->SetDrawQuery(State == ECheckBoxState::Checked);
				})
				[
					SNew(STextBlock)
					.Text(LOCTEXT("PoseSearchDebuggerDrawQuery", "Draw Query"))
				]
			]
		]
		+SHorizontalBox::Slot()
		.VAlign(VAlign_Top)
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
					return ViewModel.Get()->GetDrawTrajectory() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; 
				})
				.OnCheckStateChanged_Lambda([this](ECheckBoxState State)
				{
					ViewModel.Get()->SetDrawTrajectory(State == ECheckBoxState::Checked);
				})
				[
					SNew(STextBlock)
					.Text(LOCTEXT("PoseSearchDebuggerDrawTrajectory", "Draw Trajectory"))
				]
			]
		]
		+SHorizontalBox::Slot()
		.VAlign(VAlign_Top)
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
					return ViewModel.Get()->GetDrawHistory() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; 
				})
				.OnCheckStateChanged_Lambda([this](ECheckBoxState State)
				{
					ViewModel.Get()->SetDrawHistory(State == ECheckBoxState::Checked);
				})
				[
					SNew(STextBlock)
					.Text(LOCTEXT("PoseSearchDebuggerDrawHistory", "Draw History"))
				]
			]
		]
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Top)
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
}

TSharedRef<SWidget> SDebuggerView::GenerateNodeDebuggerView()
{
	return 
		SNew(SSplitter)
		.Orientation(Orient_Vertical)
		.ResizeMode(ESplitterResizeMode::Fill)
	
		// Database view
		+ SSplitter::Slot()
		.Value(0.8f)
		[
			SNew(SVerticalBox)
			
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				GenerateReturnButtonView()
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
		.Value(0.2f)
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
