// Copyright Epic Games, Inc. All Rights Reserved.

#include "Toolkit/RenderGridEditor.h"

#include "RenderGrid/RenderGrid.h"
#include "RenderGrid/RenderGridManager.h"
#include "RenderGrid/RenderGridQueue.h"
#include "RenderGridEditorToolbar.h"
#include "IRenderGridEditorModule.h"
#include "IRenderGridModule.h"

#include "BlueprintCompilationManager.h"
#include "BlueprintModes/RenderGridApplicationModeBase.h"
#include "BlueprintModes/RenderGridApplicationModeListing.h"
#include "BlueprintModes/RenderGridApplicationModeLogic.h"
#include "BlueprintModes/RenderGridApplicationModes.h"
#include "Blueprints/RenderGridBlueprint.h"
#include "Commands/RenderGridEditorCommands.h"
#include "EdGraphNode_Comment.h"
#include "EditorModeManager.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/MessageDialog.h"
#include "PropertyCustomizationHelpers.h"
#include "SBlueprintEditorToolbar.h"
#include "SGraphPanel.h"
#include "SKismetInspector.h"
#include "SMyBlueprint.h"
#include "Stats/StatsHierarchical.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "FRenderGridEditor"


namespace UE::RenderGrid::Private
{
	const FName RenderGridEditorAppName(TEXT("RenderGridEditorApp"));
}


void UE::RenderGrid::Private::FRenderGridEditor::InitRenderGridEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, URenderGridBlueprint* InRenderGridBlueprint)
{
	check(InRenderGridBlueprint);

	FBlueprintCompilationManager::FlushCompilationQueue(nullptr);

	TSharedPtr<IRenderGridEditor> ThisPtr(SharedThis(this));

	if (!Toolbar.IsValid())
	{
		Toolbar = MakeShareable(new FBlueprintEditorToolbar(SharedThis(this)));
	}

	RenderGridToolbar = MakeShared<FRenderGridBlueprintEditorToolbar>(ThisPtr);

	// Build up a list of objects being edited in this asset editor
	TArray<UObject*> ObjectsBeingEdited;
	ObjectsBeingEdited.Add(InRenderGridBlueprint);

	// Initialize the asset editor and spawn tabs
	constexpr bool bCreateDefaultStandaloneMenu = true;
	constexpr bool bCreateDefaultToolbar = true;
	InitAssetEditor(Mode, InitToolkitHost, RenderGridEditorAppName, FTabManager::FLayout::NullLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, ObjectsBeingEdited);

	CreateDefaultCommands();

	TArray<UBlueprint*> RenderGridBlueprints;
	RenderGridBlueprints.Add(InRenderGridBlueprint);

	CommonInitialization(RenderGridBlueprints, false);

	BindCommands();

	ExtendMenu();
	ExtendToolbar();
	RegenerateMenusAndToolbars();

	UpdateInstance(GetRenderGridBlueprint(), true);

	constexpr bool bShouldOpenInDefaultsMode = true;
	RegisterApplicationModes(RenderGridBlueprints, bShouldOpenInDefaultsMode, InRenderGridBlueprint->bIsNewlyCreated);

	// Post-layout initialization
	PostLayoutBlueprintEditorInitialization();
}

UE::RenderGrid::Private::FRenderGridEditor::FRenderGridEditor()
	: PreviewBlueprint(nullptr)
	, bRunRenderNewBatch(false)
	, BatchRenderQueue(nullptr)
	, PreviewRenderQueue(nullptr)
	, DebuggingTimeInSecondsRemaining(0)
	, bIsDebugging(false)
{}

UE::RenderGrid::Private::FRenderGridEditor::~FRenderGridEditor()
{
	URenderGridBlueprint* RenderGridBlueprint = FRenderGridEditor::GetRenderGridBlueprint();
	if (IsValid(RenderGridBlueprint))
	{
		RenderGridEditorClosedDelegate.Broadcast(this, RenderGridBlueprint);
	}
	RenderGridBlueprint = nullptr;

	DestroyInstance();
}

void UE::RenderGrid::Private::FRenderGridEditor::CreateDefaultCommands()
{
	if (GetBlueprintObj())
	{
		FBlueprintEditor::CreateDefaultCommands();
	}
	else
	{
		ToolkitCommands->MapAction(FGenericCommands::Get().Undo, FExecuteAction::CreateSP(this, &FRenderGridEditor::UndoAction));
		ToolkitCommands->MapAction(FGenericCommands::Get().Redo, FExecuteAction::CreateSP(this, &FRenderGridEditor::RedoAction));
	}
}

UBlueprint* UE::RenderGrid::Private::FRenderGridEditor::GetBlueprintObj() const
{
	for (UObject* Obj : GetEditingObjects())
	{
		if (!IsValid(Obj))
		{
			continue;
		}
		if (UBlueprint* Result = Cast<UBlueprint>(Obj))
		{
			return Result;
		}
	}
	return nullptr;
}

FGraphAppearanceInfo UE::RenderGrid::Private::FRenderGridEditor::GetGraphAppearance(UEdGraph* InGraph) const
{
	FGraphAppearanceInfo AppearanceInfo = FBlueprintEditor::GetGraphAppearance(InGraph);
	if (GetBlueprintObj()->IsA(URenderGridBlueprint::StaticClass()))
	{
		AppearanceInfo.CornerText = LOCTEXT("AppearanceCornerText_RenderGrid", "RENDER PAGES");
	}
	return AppearanceInfo;
}

void UE::RenderGrid::Private::FRenderGridEditor::OnActiveTabChanged(TSharedPtr<SDockTab> PreviouslyActive, TSharedPtr<SDockTab> NewlyActivated)
{
	if (!NewlyActivated.IsValid())
	{
		TArray<UObject*> ObjArray;
		Inspector->ShowDetailsForObjects(ObjArray);
	}
	else
	{
		FBlueprintEditor::OnActiveTabChanged(PreviouslyActive, NewlyActivated);
	}
}

void UE::RenderGrid::Private::FRenderGridEditor::SetupGraphEditorEvents(UEdGraph* InGraph, SGraphEditor::FGraphEditorEvents& InEvents)
{
	FBlueprintEditor::SetupGraphEditorEvents(InGraph, InEvents);

	InEvents.OnCreateActionMenu = SGraphEditor::FOnCreateActionMenu::CreateSP(this, &FRenderGridEditor::HandleCreateGraphActionMenu);
}

void UE::RenderGrid::Private::FRenderGridEditor::RegisterApplicationModes(const TArray<UBlueprint*>& InBlueprints, bool bShouldOpenInDefaultsMode, bool bNewlyCreated)
{
	if (InBlueprints.Num() == 1)
	{
		TSharedPtr<FRenderGridEditor> ThisPtr(SharedThis(this));

		// Create the modes and activate one (which will populate with a real layout)
		TArray<TSharedRef<FApplicationMode>> TempModeList;
		TempModeList.Add(MakeShareable(new FRenderGridApplicationModeListing(ThisPtr)));
		TempModeList.Add(MakeShareable(new FRenderGridApplicationModeLogic(ThisPtr)));

		for (TSharedRef<FApplicationMode>& AppMode : TempModeList)
		{
			AddApplicationMode(AppMode->GetModeName(), AppMode);
		}

		SetCurrentMode(FRenderGridApplicationModes::ListingMode);

		// Activate our edit mode
		GetEditorModeManager().SetDefaultMode(FRenderGridApplicationModes::ListingMode);
		GetEditorModeManager().ActivateMode(FRenderGridApplicationModes::ListingMode);
	}
}

void UE::RenderGrid::Private::FRenderGridEditor::Compile()
{
	DestroyInstance();
	FBlueprintEditor::Compile();
}

URenderGridBlueprint* UE::RenderGrid::Private::FRenderGridEditor::GetRenderGridBlueprint() const
{
	return Cast<URenderGridBlueprint>(GetBlueprintObj());
}

URenderGrid* UE::RenderGrid::Private::FRenderGridEditor::GetInstance() const
{
	return RenderGridWeakPtr.Get();
}

void UE::RenderGrid::Private::FRenderGridEditor::SetIsDebugging(const bool bInIsDebugging)
{
	if (bInIsDebugging)
	{
		bIsDebugging = true;
		DebuggingTimeInSecondsRemaining = TimeInSecondsToRemainDebugging;
		GetBlueprintObj()->SetObjectBeingDebugged(GetInstance());
	}
	else
	{
		bIsDebugging = false;
	}
}

bool UE::RenderGrid::Private::FRenderGridEditor::IsBatchRendering() const
{
	return IsValid(BatchRenderQueue);
}

bool UE::RenderGrid::Private::FRenderGridEditor::IsPreviewRendering() const
{
	return IsValid(PreviewRenderQueue);
}

void UE::RenderGrid::Private::FRenderGridEditor::SetPreviewRenderQueue(URenderGridQueue* Queue)
{
	PreviewRenderQueue = Queue;
	SetIsDebugging(IsValid(Queue));
}

void UE::RenderGrid::Private::FRenderGridEditor::MarkAsModified()
{
	if (URenderGrid* Instance = GetInstance(); IsValid(Instance))
	{
		Instance->Modify();
	}
	if (UBlueprint* BlueprintObj = GetBlueprintObj(); IsValid(BlueprintObj))
	{
		BlueprintObj->Modify();
	}
}

TArray<URenderGridJob*> UE::RenderGrid::Private::FRenderGridEditor::GetSelectedRenderGridJobs() const
{
	TArray<URenderGridJob*> SelectedJobs;
	if (URenderGrid* Grid = GetInstance(); IsValid(Grid))
	{
		for (URenderGridJob* Job : Grid->GetRenderGridJobs())
		{
			if (SelectedRenderGridJobIds.Contains(Job->GetGuid()))
			{
				SelectedJobs.Add(Job);
			}
		}
	}
	return SelectedJobs;
}

void UE::RenderGrid::Private::FRenderGridEditor::SetSelectedRenderGridJobs(const TArray<URenderGridJob*>& Jobs)
{
	TSet<FGuid> PreviouslySelectedJobIds;
	PreviouslySelectedJobIds.Append(SelectedRenderGridJobIds);

	SelectedRenderGridJobIds.Empty();
	for (URenderGridJob* Job : Jobs)
	{
		if (!IsValid(Job))
		{
			continue;
		}
		SelectedRenderGridJobIds.Add(Job->GetGuid());
	}

	if (SelectedRenderGridJobIds.Num() != PreviouslySelectedJobIds.Num())
	{
		OnRenderGridJobsSelectionChanged().Broadcast();
		return;
	}
	for (const FGuid& JobId : SelectedRenderGridJobIds)
	{
		if (!PreviouslySelectedJobIds.Contains(JobId))
		{
			OnRenderGridJobsSelectionChanged().Broadcast();
			return;
		}
	}
}

FName UE::RenderGrid::Private::FRenderGridEditor::GetToolkitFName() const
{
	return FName("RenderGridEditor");
}

FText UE::RenderGrid::Private::FRenderGridEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "Render Grid Editor");
}

FString UE::RenderGrid::Private::FRenderGridEditor::GetDocumentationLink() const
{
	return FString();
}

FText UE::RenderGrid::Private::FRenderGridEditor::GetToolkitToolTipText() const
{
	return GetToolTipTextForObject(GetBlueprintObj());
}

FLinearColor UE::RenderGrid::Private::FRenderGridEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.5f, 0.25f, 0.35f, 0.5f);
}

FString UE::RenderGrid::Private::FRenderGridEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "Render Grid ").ToString();
}

void UE::RenderGrid::Private::FRenderGridEditor::InitToolMenuContext(FToolMenuContext& MenuContext)
{
	FBlueprintEditor::InitToolMenuContext(MenuContext);
}

void UE::RenderGrid::Private::FRenderGridEditor::Tick(float DeltaTime)
{
	FBlueprintEditor::Tick(DeltaTime);

	// Note: The weak ptr can become stale if the actor is reinstanced due to a Blueprint change, etc. In that case we 
	//       look to see if we can find the new instance in the preview world and then update the weak ptr.
	if (RenderGridWeakPtr.IsStale(true))
	{
		RefreshInstance();
	}

	if (bRunRenderNewBatch)
	{
		bRunRenderNewBatch = false;
		BatchRenderListAction();
	}

	if ((DebuggingTimeInSecondsRemaining > 0) && !bIsDebugging)
	{
		DebuggingTimeInSecondsRemaining -= DeltaTime;
		if (DebuggingTimeInSecondsRemaining <= 0)
		{
			GetBlueprintObj()->SetObjectBeingDebugged(nullptr);
		}
	}
}

TStatId UE::RenderGrid::Private::FRenderGridEditor::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FRenderGridEditor, STATGROUP_Tickables);
}

void UE::RenderGrid::Private::FRenderGridEditor::RefreshInstance()
{
	UpdateInstance(GetRenderGridBlueprint(), true);
}

void UE::RenderGrid::Private::FRenderGridEditor::OnBlueprintChangedImpl(UBlueprint* InBlueprint, bool bIsJustBeingCompiled)
{
	DestroyInstance();

	FBlueprintEditor::OnBlueprintChangedImpl(InBlueprint, bIsJustBeingCompiled);

	if (InBlueprint)
	{
		RefreshInstance();
	}
}

void UE::RenderGrid::Private::FRenderGridEditor::AddReferencedObjects(FReferenceCollector& Collector)
{
	FBlueprintEditor::AddReferencedObjects(Collector);

	if (URenderGrid* Instance = GetInstance(); IsValid(Instance))
	{
		Collector.AddReferencedObject(Instance);
	}
}

void UE::RenderGrid::Private::FRenderGridEditor::NotifyPreChange(FProperty* PropertyAboutToChange)
{
	FBlueprintEditor::NotifyPreChange(PropertyAboutToChange);

	if (URenderGridBlueprint* RenderGridBP = GetRenderGridBlueprint())
	{
		RenderGridBP->Modify();
	}
}

void UE::RenderGrid::Private::FRenderGridEditor::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged)
{
	FBlueprintEditor::NotifyPostChange(PropertyChangedEvent, PropertyThatChanged);
}

void UE::RenderGrid::Private::FRenderGridEditor::OnFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent) {}

void UE::RenderGrid::Private::FRenderGridEditor::BindCommands()
{
	const auto& Commands = FRenderGridEditorCommands::Get();

	ToolkitCommands->MapAction(Commands.AddJob, FExecuteAction::CreateSP(this, &FRenderGridEditor::AddJobAction));
	ToolkitCommands->MapAction(Commands.DuplicateJob, FExecuteAction::CreateSP(this, &FRenderGridEditor::DuplicateJobAction));
	ToolkitCommands->MapAction(Commands.DeleteJob, FExecuteAction::CreateSP(this, &FRenderGridEditor::DeleteJobAction));
	ToolkitCommands->MapAction(Commands.BatchRenderList, FExecuteAction::CreateSP(this, &FRenderGridEditor::BatchRenderListAction));
}

void UE::RenderGrid::Private::FRenderGridEditor::AddJobAction()
{
	if (URenderGrid* Instance = GetInstance(); IsValid(Instance))
	{
		if (URenderGridJob* Job = Instance->CreateAndAddNewRenderGridJob(); IsValid(Job))
		{
			OnRenderGridJobCreated().Broadcast(Job);
			MarkAsModified();
			OnRenderGridChanged().Broadcast();
		}
	}
}

void UE::RenderGrid::Private::FRenderGridEditor::DuplicateJobAction()
{
	TArray<URenderGridJob*> SelectedJobs = GetSelectedRenderGridJobs();
	if (SelectedJobs.Num() <= 0)
	{
		return;
	}

	if (URenderGrid* Instance = GetInstance(); IsValid(Instance))
	{
		for (URenderGridJob* SelectedJob : SelectedJobs)
		{
			Instance->DuplicateAndAddRenderGridJob(SelectedJob);
		}
		MarkAsModified();
		OnRenderGridChanged().Broadcast();
	}
}

void UE::RenderGrid::Private::FRenderGridEditor::DeleteJobAction()
{
	TArray<URenderGridJob*> SelectedJobs = GetSelectedRenderGridJobs();
	if (SelectedJobs.Num() <= 0)
	{
		return;
	}

	const FText TitleText = LOCTEXT("ConfirmToDeleteTitle", "Confirm To Delete");
	const EAppReturnType::Type DialogResult = FMessageDialog::Open(
		EAppMsgType::OkCancel,
		((SelectedJobs.Num() == 1) ? LOCTEXT("ConfirmToDeleteSingleText", "Are you sure you want to delete the selected job?") : LOCTEXT("ConfirmToDeleteMultipleText", "Are you sure you want to delete the selected jobs?")),
		&TitleText);

	if (DialogResult != EAppReturnType::Ok)
	{
		return;
	}

	if (URenderGrid* Instance = GetInstance(); IsValid(Instance))
	{
		for (URenderGridJob* SelectedJob : SelectedJobs)
		{
			Instance->RemoveRenderGridJob(SelectedJob);
		}

		MarkAsModified();
		OnRenderGridChanged().Broadcast();
	}
}

void UE::RenderGrid::Private::FRenderGridEditor::BatchRenderListAction()
{
	if (!CanCurrentlyRender())
	{
		bRunRenderNewBatch = true;
		return;
	}

	if (URenderGrid* Instance = GetInstance(); IsValid(Instance))
	{
		if (Instance->GetRenderGridJobs().Num() <= 0)
		{
			const FText TitleText = LOCTEXT("NoJobsToRenderTitle", "No Jobs To Render");
			FMessageDialog::Open(
				EAppMsgType::Ok,
				LOCTEXT("NoJobsToRenderText", "There are no jobs in this grid, and so nothing can be rendered. Please add a job and try again."),
				&TitleText);
			return;
		}

		if (URenderGridQueue* RenderQueue = IRenderGridModule::Get().GetManager().CreateBatchRenderQueue(Instance); IsValid(RenderQueue))
		{
			RenderQueue->OnExecuteFinished().AddRaw(this, &FRenderGridEditor::OnBatchRenderListActionFinished);
			BatchRenderQueue = RenderQueue;
			OnRenderGridBatchRenderingStarted().Broadcast(RenderQueue);

			SetIsDebugging(true);
			RenderQueue->Execute();
		}
	}
}

void UE::RenderGrid::Private::FRenderGridEditor::OnBatchRenderListActionFinished(URenderGridQueue* Queue, bool bSuccess)
{
	SetIsDebugging(false);

	URenderGridQueue* FinishedRenderQueue = BatchRenderQueue;
	BatchRenderQueue = nullptr;
	OnRenderGridBatchRenderingFinished().Broadcast(FinishedRenderQueue);
}

void UE::RenderGrid::Private::FRenderGridEditor::UndoAction()
{
	GEditor->UndoTransaction();
}

void UE::RenderGrid::Private::FRenderGridEditor::RedoAction()
{
	GEditor->RedoTransaction();
}

void UE::RenderGrid::Private::FRenderGridEditor::ExtendMenu()
{
	if (MenuExtender.IsValid())
	{
		RemoveMenuExtender(MenuExtender);
		MenuExtender.Reset();
	}

	MenuExtender = MakeShareable(new FExtender);

	AddMenuExtender(MenuExtender);

	// add extensible menu if exists
	IRenderGridEditorModule& RenderGridEditorModule = IRenderGridEditorModule::Get();
	AddMenuExtender(RenderGridEditorModule.GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));
}

void UE::RenderGrid::Private::FRenderGridEditor::ExtendToolbar()
{
	// If the ToolbarExtender is valid, remove it before rebuilding it
	if (ToolbarExtender.IsValid())
	{
		RemoveToolbarExtender(ToolbarExtender);
		ToolbarExtender.Reset();
	}

	ToolbarExtender = MakeShareable(new FExtender);

	AddToolbarExtender(ToolbarExtender);

	IRenderGridEditorModule& RenderGridEditorModule = IRenderGridEditorModule::Get();
	AddToolbarExtender(RenderGridEditorModule.GetToolBarExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));

	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateSP(this, &FRenderGridEditor::FillToolbar)
	);
}

void UE::RenderGrid::Private::FRenderGridEditor::FillToolbar(FToolBarBuilder& ToolbarBuilder)
{
	ToolbarBuilder.BeginSection("Common");
	{
		ToolbarBuilder.AddToolBarButton(FRenderGridEditorCommands::Get().BatchRenderList, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.OpenCinematic"));
	}
	ToolbarBuilder.EndSection();
}

void UE::RenderGrid::Private::FRenderGridEditor::DestroyInstance()
{
	if (URenderGrid* Instance = GetInstance(); IsValid(Instance))
	{
		// Execute the blueprint event
		Instance->EndEditor();

		// Store the data
		Instance->OnClose();

		// Garbage collection
		RenderGridWeakPtr.Reset();
		Instance->MarkAsGarbage();
	}
}

void UE::RenderGrid::Private::FRenderGridEditor::UpdateInstance(UBlueprint* InBlueprint, bool bInForceFullUpdate)
{
	// If the Blueprint is changing
	if ((InBlueprint != PreviewBlueprint.Get()) || bInForceFullUpdate)
	{
		// Destroy the previous instance
		DestroyInstance();

		// Save the Blueprint we're creating a preview for
		PreviewBlueprint = Cast<URenderGridBlueprint>(InBlueprint);

		URenderGrid* RenderGrid;
		// Create the Widget, we have to do special swapping out of the widget tree.
		{
			// Assign the outer to the game instance if it exists, otherwise use the world
			{
				FMakeClassSpawnableOnScope TemporarilySpawnable(PreviewBlueprint->GeneratedClass);
				RenderGrid = NewObject<URenderGrid>(PreviewScene.GetWorld(), PreviewBlueprint->GeneratedClass);
			}
		}

		// Set the debug object again, if it was debugging
		GetBlueprintObj()->SetObjectBeingDebugged(bIsDebugging ? RenderGrid : nullptr);

		// Store a reference to the preview actor.
		RenderGridWeakPtr = RenderGrid;

		// Broadcast the events
		OnRenderGridChanged().Broadcast();
		OnRenderGridJobsSelectionChanged().Broadcast();

		// Execute the blueprint event
		RenderGrid->BeginEditor();
	}
}

FActionMenuContent UE::RenderGrid::Private::FRenderGridEditor::HandleCreateGraphActionMenu(UEdGraph* InGraph, const FVector2D& InNodePosition, const TArray<UEdGraphPin*>& InDraggedPins, bool bAutoExpand, SGraphEditor::FActionMenuClosed InOnMenuClosed)
{
	return OnCreateGraphActionMenu(InGraph, InNodePosition, InDraggedPins, bAutoExpand, InOnMenuClosed);
}


#undef LOCTEXT_NAMESPACE
