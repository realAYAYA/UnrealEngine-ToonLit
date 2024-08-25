// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaSequencer.h"
#include "AvaSequence.h"
#include "AvaSequencePlaybackObject.h"
#include "AvaSequencePlayer.h"
#include "AvaSequencerArgs.h"
#include "AvaSequencerModule.h"
#include "AvaSequencerUtils.h"
#include "Clipboard/AvaSequenceExporter.h"
#include "Clipboard/AvaSequenceImporter.h"
#include "Commands/AvaSequencerAction.h"
#include "Commands/AvaSequencerCommands.h"
#include "Commands/Stagger/AvaSequencerStagger.h"
#include "CoreGlobals.h"
#include "DetailsView/SAvaSequenceDetails.h"
#include "DetailsView/Section/AvaSequencePlaybackDetails.h"
#include "DetailsView/Section/AvaSequenceSelectionDetails.h"
#include "DetailsView/Section/AvaSequenceSettingsDetails.h"
#include "EaseCurveTool/AvaEaseCurveTool.h"
#include "EaseCurveTool/AvaEaseCurveToolCommands.h"
#include "Editor/Sequencer/Private/Sequencer.h"
#include "EngineUtils.h"
#include "EntitySystem/MovieSceneEntitySystemRunner.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IAvaSequenceProvider.h"
#include "ISequencer.h"
#include "ISequencerModule.h"
#include "ISequencerTrackEditor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "MVVM/Selection/Selection.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"
#include "MVVM/Views/SOutlinerView.h"
#include "Misc/TextFilter.h"
#include "MovieScene.h"
#include "Playback/AvaSequencerCleanView.h"
#include "Playback/AvaSequencerController.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"
#include "Selection/AvaEditorSelection.h"
#include "SequenceTree/AvaSequenceItem.h"
#include "SequenceTree/Columns/AvaSequenceNameColumn.h"
#include "SequenceTree/Columns/AvaSequenceStatusColumn.h"
#include "SequenceTree/Columns/IAvaSequenceColumn.h"
#include "SequenceTree/Widgets/SAvaSequenceTree.h"
#include "SequencerCommands.h"
#include "SequencerSettings.h"
#include "SequencerUtilities.h"
#include "Settings/AvaSequencerSettings.h"
#include "ToolMenu.h"
#include "ToolMenuEntry.h"
#include "ToolMenuSection.h"
#include "ToolMenus.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Widgets/Views/STreeView.h"

DEFINE_LOG_CATEGORY_STATIC(LogAvaSequencer, Log, All);

#define LOCTEXT_NAMESPACE "AvaSequencer"

namespace UE::AvaSequencer::Private
{
	void SetObjectSelection(USelection* InSelection, const TArray<UObject*>& InObjects, bool bIsTransactional)
	{
		if (!InSelection)
		{
			return;
		}

		if (bIsTransactional)
		{
			InSelection->Modify();
		}
		InSelection->BeginBatchSelectOperation();
		InSelection->DeselectAll();

		for (UObject* const Object : InObjects)
		{
			InSelection->Select(Object);
		}

		InSelection->EndBatchSelectOperation(bIsTransactional);
	}
	
	struct FScopedSelection
	{
		FScopedSelection(USelection* InTargetSelection, USelection* InSourceSelection)
			: TargetSelection(InTargetSelection)
		{
			check(InTargetSelection && InSourceSelection);

			TArray<UObject*> SourceSelectedObjects;
			InSourceSelection->GetSelectedObjects(SourceSelectedObjects);

			TargetSelection->GetSelectedObjects(OriginalTargetSelectedObjects);
			SetObjectSelection(TargetSelection, SourceSelectedObjects, false);
		}

		~FScopedSelection()
		{
			check(TargetSelection);
			SetObjectSelection(TargetSelection, OriginalTargetSelectedObjects, false);
		}

	private:
		/** The Target we're temporarily selecting and mirroring Source Selection */
		USelection* TargetSelection;

		/** Cached selected objects of Target */
		TArray<UObject*> OriginalTargetSelectedObjects;
	};
}

FAvaSequencer::FAvaSequencer(IAvaSequencerProvider& InProvider, FAvaSequencerArgs&& InArgs)
	: Provider(InProvider)
	, CommandList(MakeShared<FUICommandList>())
	, CleanView(MakeShared<FAvaSequencerCleanView>())
	, bUseCustomCleanPlaybackMode(InArgs.bUseCustomCleanPlaybackMode)
	, bCanProcessSequencerSelections(InArgs.bCanProcessSequencerSelections)
{
	SequencerController = MoveTemp(InArgs.SequencerController);

	SequencerActions =
		{
			MakeShared<FAvaSequencerStagger>(*this),
		};

	BindCommands();

	OnSequenceStartedHandle = UAvaSequencePlayer::OnSequenceStarted().AddLambda(
		[this](UAvaSequencePlayer*, UAvaSequence*){ return NotifyOnSequencePlayed(); });

	OnSequenceFinishedHandle = UAvaSequencePlayer::OnSequenceFinished().AddLambda(
		[this](UAvaSequencePlayer*, UAvaSequence*){ return NotifyOnSequenceStopped(); });

	// Register sequencer menu extenders.
	ISequencerModule& SequencerModule = FAvaSequencerUtils::GetSequencerModule();
	{
		const int32 NewIndex = SequencerModule.GetAddTrackMenuExtensibilityManager()->GetExtenderDelegates().Add(
			FAssetEditorExtender::CreateRaw(this, &FAvaSequencer::GetAddTrackSequencerExtender));
		
		SequencerAddTrackExtenderHandle = SequencerModule.GetAddTrackMenuExtensibilityManager()->GetExtenderDelegates()[NewIndex].GetHandle();
	}

	// Register to update when an undo/redo operation has been called to update our list of items
	GEditor->RegisterForUndo(this);
}

FAvaSequencer::~FAvaSequencer()
{
	UAvaSequencePlayer::OnSequenceStarted().Remove(OnSequenceStartedHandle);
	UAvaSequencePlayer::OnSequenceFinished().Remove(OnSequenceFinishedHandle);
	OnSequenceStartedHandle.Reset();
	OnSequenceFinishedHandle.Reset();

	if(GEngine)
	{
		GEditor->UnregisterForUndo(this);
	}
	
	if (FAvaSequencerUtils::IsSequencerModuleLoaded())
	{
		ISequencerModule& SequencerModule = FAvaSequencerUtils::GetSequencerModule();
		SequencerModule.GetAddTrackMenuExtensibilityManager()->GetExtenderDelegates().RemoveAll(
			[this](const FAssetEditorExtender& Extender)
			{
				return SequencerAddTrackExtenderHandle == Extender.GetHandle();
			});
	}
}

void FAvaSequencer::BindCommands()
{
	const FAvaSequencerCommands& AvaSequencerCommands = FAvaSequencerCommands::Get();

	for (const TSharedRef<FAvaSequencerAction>& SequencerAction : SequencerActions)
	{
		SequencerAction->MapAction(CommandList);
	}

	CommandList->MapAction(AvaSequencerCommands.FixBindingPaths
		, FExecuteAction::CreateRaw(this, &FAvaSequencer::FixBindingPaths));

	CommandList->MapAction(AvaSequencerCommands.FixInvalidBindings
		, FExecuteAction::CreateRaw(this, &FAvaSequencer::FixInvalidBindings));

	CommandList->MapAction(AvaSequencerCommands.FixBindingHierarchy
		, FExecuteAction::CreateRaw(this, &FAvaSequencer::FixBindingHierarchy));

	const FSequencerCommands& SequencerCommands = FSequencerCommands::Get();

	CommandList->MapAction(SequencerCommands.AddTransformKey
		, FExecuteAction::CreateRaw(this, &FAvaSequencer::AddTransformKey, EMovieSceneTransformChannel::All));

	CommandList->MapAction(SequencerCommands.AddTranslationKey
		, FExecuteAction::CreateRaw(this, &FAvaSequencer::AddTransformKey, EMovieSceneTransformChannel::Translation));

	CommandList->MapAction(SequencerCommands.AddRotationKey
		, FExecuteAction::CreateRaw(this, &FAvaSequencer::AddTransformKey, EMovieSceneTransformChannel::Rotation));

	CommandList->MapAction(SequencerCommands.AddScaleKey
		, FExecuteAction::CreateRaw(this, &FAvaSequencer::AddTransformKey, EMovieSceneTransformChannel::Scale));	
}

TSharedPtr<IAvaSequenceColumn> FAvaSequencer::FindSequenceColumn(FName InColumnName) const
{
	if (const TSharedPtr<IAvaSequenceColumn>* const FoundColumn = SequenceColumns.Find(InColumnName))
	{
		return *FoundColumn;
	}
	
	return nullptr;
}

void FAvaSequencer::EnsureSequencer()
{
	TSharedPtr<ISequencer> Sequencer = SequencerWeak.Pin();
	if (Sequencer.IsValid())
	{
		return;
	}

	// Instantiate Sequencer Controller first so it Ticks before FSequencer
	if (!SequencerController.IsValid())
	{
		SequencerController = MakeShared<FAvaSequencerController>();
	}

	Sequencer = Provider.GetExternalSequencer();

	// External Implementation could call GetSequencer again (e.g. to get the underlying sequencer widget),
	// so need to give priority to that call and initialize from there.
	// If this is the case, SequencerWeak is now initialized/valid and should return early to avoid double init.
	if (SequencerWeak.IsValid())
	{
		return;
	}

	if (Sequencer.IsValid())
	{
		checkf(Sequencer.GetSharedReferenceCount() > 1
			, TEXT("IAvaSequencerProvider::GetExternalSequencer should return a sequencer and hold reference to it"));
	}
	else
	{
		// Create Sequencer if one was not provided
		Sequencer = CreateSequencer();
		check(Sequencer.IsValid());
	}

	SequencerWeak = Sequencer;

	SequencerController->SetSequencer(Sequencer);

	GetDefaultSequence();

	InitSequencerCommandList();

	// Register Events
	Sequencer->OnActivateSequence().AddSP(this, &FAvaSequencer::OnActivateSequence);
	Sequencer->OnPlayEvent().AddSP(this, &FAvaSequencer::NotifyOnSequencePlayed);
	Sequencer->OnStopEvent().AddSP(this, &FAvaSequencer::NotifyOnSequenceStopped);
	Sequencer->OnMovieSceneBindingsPasted().AddSP(this, &FAvaSequencer::OnMovieSceneBindingsPasted);
	Sequencer->GetSelectionChangedObjectGuids().AddSP(this, &FAvaSequencer::OnSequencerSelectionChanged);
	Sequencer->OnGetIsBindingVisible().BindSP(this, &FAvaSequencer::IsBindingSelected);
	Sequencer->OnCameraCut().AddSP(this, &FAvaSequencer::OnUpdateCameraCut);

	if (IAvaSequenceProvider* const SequenceProvider = Provider.GetSequenceProvider())
	{
		SequenceProvider->OnEditorSequencerCreated(Sequencer);
	}

	// Create ease curve tool and map commands
	EaseCurveTool = MakeShared<FAvaEaseCurveTool>(SharedThis(this));

	const TSharedRef<FAvaEaseCurveTool> EaseCurveToolRef = EaseCurveTool.ToSharedRef();
	const FAvaEaseCurveToolCommands& EaseCurveToolCommands = FAvaEaseCurveToolCommands::Get();

	CommandList->MapAction(EaseCurveToolCommands.QuickEaseIn
		, FExecuteAction::CreateSP(EaseCurveToolRef, &FAvaEaseCurveTool::ApplyQuickEaseToSequencerKeySelections, FAvaEaseCurveTool::EOperation::In));

	CommandList->MapAction(EaseCurveToolCommands.QuickEase
		, FExecuteAction::CreateSP(EaseCurveToolRef, &FAvaEaseCurveTool::ApplyQuickEaseToSequencerKeySelections, FAvaEaseCurveTool::EOperation::InOut));

	CommandList->MapAction(EaseCurveToolCommands.QuickEaseOut
		, FExecuteAction::CreateSP(EaseCurveToolRef, &FAvaEaseCurveTool::ApplyQuickEaseToSequencerKeySelections, FAvaEaseCurveTool::EOperation::Out));
}

TSharedRef<ISequencer> FAvaSequencer::CreateSequencer()
{
	const UAvaSequencerSettings* SequencerSettings = GetDefault<UAvaSequencerSettings>();

	// Configure Init Params
	FSequencerInitParams SequencerInitParams;
	{
		UAvaSequence* const DefaultSequence = GetDefaultSequence();
		SetViewedSequence(DefaultSequence);
		ensure(GetViewedSequence() == DefaultSequence);

		SequencerInitParams.RootSequence           = GetViewedSequence();
		SequencerInitParams.bEditWithinLevelEditor = false;
		SequencerInitParams.ToolkitHost            = Provider.GetSequencerToolkitHost();
		SequencerInitParams.PlaybackContext.Bind(this, &FAvaSequencer::GetPlaybackContext);

		SequencerInitParams.ViewParams.UniqueName      = SequencerSettings->GetName();
		SequencerInitParams.ViewParams.ScrubberStyle   = ESequencerScrubberStyle::FrameBlock;
		SequencerInitParams.ViewParams.ToolbarExtender = MakeShared<FExtender>();

		// Host Capabilities
		SequencerInitParams.HostCapabilities.bSupportsCurveEditor = true;
		SequencerInitParams.HostCapabilities.bSupportsSaveMovieSceneAsset = false;
	};

	InstancedSequencer = FAvaSequencerUtils::GetSequencerModule().CreateSequencer(SequencerInitParams);

	return InstancedSequencer.ToSharedRef();
}

void FAvaSequencer::GetSelectedObjects(const TArray<FGuid>& InObjectGuids
	, TArray<UObject*>& OutSelectedActors
	, TArray<UObject*>& OutSelectedComponents
	, TArray<UObject*>& OutSelectedObjects) const
{
	TSharedRef<ISequencer> Sequencer = GetSequencer();

	UMovieSceneSequence* const ActiveSequence = Sequencer->GetFocusedMovieSceneSequence();
	UObject* const PlaybackContext = Provider.GetPlaybackContext();

	TSet<UObject*> ProcessedObjects;
	// Prepare for Worst Case where all Objects are Unique
	ProcessedObjects.Reserve(InObjectGuids.Num());

	for (const FGuid& Guid : InObjectGuids)
	{
		TArrayView<TWeakObjectPtr<>> BoundObjects = ResolveBoundObjects(Guid, ActiveSequence);

		if (BoundObjects.IsEmpty())
		{
			continue;
		}

		UObject* const BoundObject = BoundObjects[0].Get();

		// Skip invalid or objects already processed
		if (!IsValid(BoundObject) || ProcessedObjects.Contains(BoundObject))
		{
			continue;
		}

		ProcessedObjects.Add(BoundObject);

		if (AActor* const Actor = Cast<AActor>(BoundObject))
		{
			OutSelectedActors.AddUnique(Actor);
		}
		else if (UActorComponent* ActorComponent = Cast<UActorComponent>(BoundObject))
		{
			OutSelectedComponents.AddUnique(ActorComponent);
		}
		else
		{
			OutSelectedObjects.AddUnique(BoundObject);
		}
	}
}

bool FAvaSequencer::IsBindingSelected(const FMovieSceneBinding& InBinding) const
{
	if (!ViewedSequenceWeak.IsValid())
	{
		return false;
	}

	TArrayView<TWeakObjectPtr<>> ResolvedObjects = ResolveBoundObjects(InBinding.GetObjectGuid(), ViewedSequenceWeak.Get());
	
	if (ResolvedObjects.IsEmpty())
	{
		return false;
	}

	if (FEditorModeTools* const ModeTools = Provider.GetSequencerModeTools())
	{
		UObject* const ResolvedObject = ResolvedObjects[0].Get();

		if (Cast<AActor>(ResolvedObject))
		{
			return ModeTools->GetSelectedActors()->IsSelected(ResolvedObject);
		}

		if (Cast<UActorComponent>(ResolvedObject))
		{
			return ModeTools->GetSelectedComponents()->IsSelected(ResolvedObject);
		}

		return ModeTools->GetSelectedObjects()->IsSelected(ResolvedObject);
	}

	return false;
}

void FAvaSequencer::OnSequencerSelectionChanged(TArray<FGuid> InObjectGuids)
{
	using namespace UE::AvaSequencer::Private;

	if (!bCanProcessSequencerSelections || bUpdatingSelection)
	{
		return;
	}

	TGuardValue<bool> Guard(bUpdatingSelection, true);

	TArray<UObject*> SelectedActors;
	TArray<UObject*> SelectedComponents;
	TArray<UObject*> SelectedObjects;

	GetSelectedObjects(InObjectGuids, SelectedActors, SelectedComponents, SelectedObjects);

	if (FEditorModeTools* const ModeTools = Provider.GetSequencerModeTools())
	{
		SetObjectSelection(ModeTools->GetSelectedActors(), SelectedActors, true);
		SetObjectSelection(ModeTools->GetSelectedComponents(), SelectedComponents, true);
		SetObjectSelection(ModeTools->GetSelectedObjects(), SelectedObjects, true);
	}
	
	bSelectedFromSequencer = true;
}

TSharedRef<FExtender> FAvaSequencer::GetAddTrackSequencerExtender(const TSharedRef<FUICommandList> InCommandList
	, const TArray<UObject*> InContextSensitiveObjects)
{
	TSharedRef<FExtender> AddTrackMenuExtender = MakeShared<FExtender>();
	
	AddTrackMenuExtender->AddMenuExtension(
		SequencerMenuExtensionPoints::AddTrackMenu_PropertiesSection,
		EExtensionHook::Before,
		InCommandList,
		FMenuExtensionDelegate::CreateSP(this, &FAvaSequencer::ExtendSequencerAddTrackMenu
			, InContextSensitiveObjects));
	
	return AddTrackMenuExtender;
}

void FAvaSequencer::ExtendSequencerAddTrackMenu(FMenuBuilder& OutAddTrackMenuBuilder
	, const TArray<UObject*> InContextObjects)
{
}

void FAvaSequencer::NotifyViewedSequenceChanged(UAvaSequence* InOldSequence)
{
	UAvaSequence* ViewedSequence = ViewedSequenceWeak.Get();

	Provider.OnViewedSequenceChanged(InOldSequence, ViewedSequence);

	OnViewedSequenceChanged.Broadcast(ViewedSequence);

	TSharedPtr<ISequencer> Sequencer = SequencerWeak.Pin();
	if (Sequencer.IsValid() && ViewedSequence)
	{
		Sequencer->ResetToNewRootSequence(*ViewedSequence);
	}

	if (SequenceTree.IsValid())
	{
		SequenceTree->OnPostSetViewedSequence(ViewedSequence);
	}
}

UAvaSequence* FAvaSequencer::GetDefaultSequence() const
{
	IAvaSequenceProvider* const SequenceManager = Provider.GetSequenceProvider();
	if (!SequenceManager)
	{
		return nullptr;
	}

	if (UAvaSequence* const DefaultSequence = SequenceManager->GetDefaultSequence())
	{
		return DefaultSequence;
	}

	UAvaSequence* const NewDefaultSequence = CreateSequence();
	SequenceManager->SetDefaultSequence(NewDefaultSequence);
	return NewDefaultSequence;
}

UAvaSequence* FAvaSequencer::CreateSequence() const
{
	IAvaSequenceProvider* const SequenceProvider = Provider.GetSequenceProvider();
	if (!SequenceProvider)
	{
		return nullptr;
	}

	UObject* const Outer = SequenceProvider->ToUObject();
	if (!ensure(Outer))
	{
		return nullptr;
	}

	UAvaSequence* const Sequence = NewObject<UAvaSequence>(Outer, NAME_None, RF_Transactional);
	check(Sequence);

	UMovieScene* const MovieScene = Sequence->GetMovieScene();
	if (!ensure(MovieScene))
	{
		return Sequence;
	}

	const UAvaSequencerSettings* Settings = GetDefault<UAvaSequencerSettings>();
	if (!ensure(Settings))
	{
		return Sequence;
	}

	MovieScene->SetDisplayRate(Settings->GetDisplayRate());

	const double InTime  = Settings->GetStartTime();
	const double OutTime = Settings->GetEndTime();

	const FFrameTime InFrame  = InTime  * MovieScene->GetTickResolution();
	const FFrameTime OutFrame = OutTime * MovieScene->GetTickResolution();
	
	MovieScene->SetPlaybackRange(TRange<FFrameNumber>(InFrame.FrameNumber, OutFrame.FrameNumber+1));
	MovieScene->GetEditorData().WorkStart = InTime;
	MovieScene->GetEditorData().WorkEnd   = OutTime;
	
	return Sequence;
}

UObject* FAvaSequencer::GetPlaybackContext() const
{
	return Provider.GetPlaybackContext();
}

TSharedRef<SWidget> FAvaSequencer::GetSequenceTreeWidget()
{
	if (!SequenceTree.IsValid())
	{
		SequenceTreeHeaderRow = SNew(SHeaderRow)
			.Visibility(EVisibility::Visible)
			.CanSelectGeneratedColumn(true);

		SequenceColumns.Reset();
		SequenceTreeHeaderRow->ClearColumns();

		TArray<TSharedPtr<IAvaSequenceColumn>> Columns;
		Columns.Add(MakeShared<FAvaSequenceNameColumn>());
		Columns.Add(MakeShared<FAvaSequenceStatusColumn>());

		const TSharedPtr<FAvaSequencer> This = SharedThis(this);

		for (const TSharedPtr<IAvaSequenceColumn>& Column : Columns)
		{
			const FName ColumnId = Column->GetColumnId();
			SequenceColumns.Add(ColumnId, Column);
			SequenceTreeHeaderRow->AddColumn(Column->ConstructHeaderRowColumn());
			SequenceTreeHeaderRow->SetShowGeneratedColumn(ColumnId, true);
		}

		SequenceTree = SNew(SAvaSequenceTree, This, SequenceTreeHeaderRow);
		SequenceTreeView = SequenceTree->GetSequenceTreeView();

		// Make sure the Tree is synced to latest viewed sequence
		NotifyOnSequenceTreeChanged();
		NotifyViewedSequenceChanged(nullptr);
	}
	return SequenceTree.ToSharedRef();
}

TSharedRef<SWidget> FAvaSequencer::CreatePlayerToolBar(const TSharedRef<FUICommandList>& InCommandList)
{
	FSlimHorizontalToolBarBuilder ToolBarBuilder(InCommandList, FMultiBoxCustomization::None);
	ToolBarBuilder.SetLabelVisibility(EVisibility::Collapsed);

	const FAvaSequencerCommands& Commands = FAvaSequencerCommands::Get();

	ToolBarBuilder.AddToolBarButton(Commands.PlaySelected, NAME_None, TAttribute<FText>(), TAttribute<FText>()
		, FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Toolbar.Play"));

	ToolBarBuilder.AddToolBarButton(Commands.ContinueSelected, NAME_None, TAttribute<FText>(), TAttribute<FText>()
		, FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.JumpToEvent"));

	ToolBarBuilder.AddToolBarButton(Commands.StopSelected, NAME_None, TAttribute<FText>(), TAttribute<FText>()
		, FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Toolbar.Stop"));

	return SNew(SBox)
		.HAlign(EHorizontalAlignment::HAlign_Center)
		[
			ToolBarBuilder.MakeWidget()
		];
}

void FAvaSequencer::OnSequenceSearchChanged(const FText& InSearchText, FText& OutErrorMessage)
{
	NotifyOnSequenceTreeChanged();
	
	if (!InSearchText.IsEmpty())
	{
		TTextFilter<FAvaSequenceItemPtr> TextFilter(TTextFilter<FAvaSequenceItemPtr>::FItemToStringArray::CreateLambda(
			[](FAvaSequenceItemPtr InSequence, TArray<FString>& OutFilterStrings)
			{
				OutFilterStrings.Add(InSequence->GetDisplayNameText().ToString());
			}));
		
		TextFilter.SetRawFilterText(InSearchText);
		OutErrorMessage = TextFilter.GetFilterErrorText();

		//TODO: Tree View is not accounted for here
		for (TArray<FAvaSequenceItemPtr>::TIterator Iter(RootSequenceItems); Iter; ++Iter)
		{
			const FAvaSequenceItemPtr& Item = *Iter;
			if (!Item.IsValid() || !TextFilter.PassesFilter(Item))
			{
				Iter.RemoveCurrent();
			}
		}
	}
	else
	{
		OutErrorMessage = FText::GetEmpty();
	}
}

void FAvaSequencer::OnActivateSequence(FMovieSceneSequenceIDRef InSequenceID)
{
	TSharedPtr<ISequencer> Sequencer = SequencerWeak.Pin();
	if (!Sequencer.IsValid())
	{
		return;
	}

	FMovieSceneRootEvaluationTemplateInstance& RootInstance = Sequencer->GetEvaluationTemplate();
	UMovieSceneSequence* Sequence = RootInstance.GetSequence(InSequenceID);

	SetViewedSequence(Cast<UAvaSequence>(Sequence));
}

void FAvaSequencer::NotifyOnSequencePlayed()
{
	if (!bUseCustomCleanPlaybackMode)
	{
		return;
	}

	if (const USequencerSettings* const SequencerSettings = GetSequencerSettings())
	{
		if (SequencerSettings->GetCleanPlaybackMode())
		{
			TArray<TWeakPtr<FEditorViewportClient>> ViewportClients;
			Provider.GetCustomCleanViewViewportClients(ViewportClients);
			CleanView->Apply(ViewportClients);
		}
		else
		{
			CleanView->Restore();
		}
	}
}

void FAvaSequencer::NotifyOnSequenceStopped()
{
	if (!bUseCustomCleanPlaybackMode)
	{
		return;
	}

	CleanView->Restore();
}

void FAvaSequencer::OnMovieSceneBindingsPasted(const TArray<FMovieSceneBinding>& InBindings)
{
	UAvaSequence* const Sequence = GetViewedSequence();
	if (!Sequence)
	{
		return;
	}

	UMovieScene* const MovieScene = Sequence->GetMovieScene();
	if (!MovieScene)
	{
		return;
	}

	UObject* const PlaybackContext = GetPlaybackContext();
	if (!PlaybackContext)
	{
		return;
	}

	TSet<const FMovieScenePossessable*> ProcessedPossessables;

	for (const FMovieSceneBinding& Binding : InBindings)
	{
		if (FMovieScenePossessable* const Possessable = MovieScene->FindPossessable(Binding.GetObjectGuid()))
		{
			FixPossessable(*Sequence, *Possessable, PlaybackContext, ProcessedPossessables);
		}
	}
}

void FAvaSequencer::AddTransformKey(EMovieSceneTransformChannel InTransformChannel)
{
	using namespace UE::AvaSequencer::Private;

	FEditorModeTools* const EditorModeTools = Provider.GetSequencerModeTools();
	if (!EditorModeTools)
	{
		return;
	}

	TSharedRef<ISequencer> Sequencer = GetSequencer();
	const TArray<TSharedPtr<ISequencerTrackEditor>>& TrackEditors = StaticCastSharedRef<FSequencer>(Sequencer)->GetTrackEditors();

	TArray<TSharedPtr<ISequencerTrackEditor>> TransformTrackEditors;

	bool bUseOverridePriority = false;

	for (const TSharedPtr<ISequencerTrackEditor>& TrackEditor : TrackEditors)
	{
		if (TrackEditor.IsValid() && TrackEditor->HasTransformKeyBindings())
		{
			TransformTrackEditors.Add(TrackEditor);
			bUseOverridePriority |= TrackEditor->HasTransformKeyOverridePriority();
		}
	}

	if (TransformTrackEditors.IsEmpty())
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("AddTransformKey", "Add Transform Key"));

	// Temporarily set the GEditor Selections to our Ed Mode Tools Selections
	FScopedSelection ActorSelection(GEditor->GetSelectedActors(), EditorModeTools->GetSelectedActors());
	FScopedSelection CompSelection(GEditor->GetSelectedComponents(), EditorModeTools->GetSelectedComponents());
	FScopedSelection ObjectSelection(GEditor->GetSelectedObjects(), EditorModeTools->GetSelectedObjects());

	for (const TSharedPtr<ISequencerTrackEditor>& TransformTrackEditor : TransformTrackEditors)
	{
		if (!bUseOverridePriority || TransformTrackEditor->HasTransformKeyOverridePriority())
		{
			TransformTrackEditor->OnAddTransformKeysForSelectedObjects(InTransformChannel);
		}
	}
}

void FAvaSequencer::ApplyDefaultPresetToSelection(FName InPresetName)
{
	const UAvaSequencerSettings* SequencerSettings = GetDefault<UAvaSequencerSettings>();
	if (!SequencerSettings)
	{
		return;
	}

	TConstArrayView<FAvaSequencePreset> DefaultSequencePresets = SequencerSettings->GetDefaultSequencePresets();

	int32 PresetIndex = DefaultSequencePresets.Find(FAvaSequencePreset(InPresetName));
	if (PresetIndex == INDEX_NONE)
	{
		return;
	}

	ApplyPresetToSelection(DefaultSequencePresets[PresetIndex]);
}

void FAvaSequencer::ApplyCustomPresetToSelection(FName InPresetName)
{
	const UAvaSequencerSettings* SequencerSettings = GetDefault<UAvaSequencerSettings>();
	if (!SequencerSettings)
	{
		return;
	}

	const FAvaSequencePreset* SequencePreset = SequencerSettings->GetCustomSequencePresets().Find(FAvaSequencePreset(InPresetName));
	if (!SequencePreset)
	{
		return;
	}

	ApplyPresetToSelection(*SequencePreset);
}

void FAvaSequencer::ApplyPresetToSelection(const FAvaSequencePreset& InPreset)
{
	TArray<TSharedPtr<IAvaSequenceItem>> SelectedItems = SequenceTreeView->GetSelectedItems();
	if (SelectedItems.IsEmpty())
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("ApplySequencePreset", "Apply Sequence Preset"));

	for (const TSharedPtr<IAvaSequenceItem>& SequenceItem : SelectedItems)
	{
		if (SequenceItem.IsValid())
		{
			InPreset.ApplyPreset(SequenceItem->GetSequence());
		}
	}
}

bool FAvaSequencer::AddSequence_CanExecute() const
{
	return GetProvider().CanEditOrPlaySequences();
}

void FAvaSequencer::AddSequence_Execute()
{
	IAvaSequenceProvider* const SequenceProvider = Provider.GetSequenceProvider();
	if (!SequenceProvider)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddSequenceTransaction", "Add Sequence"));

	UAvaSequence* const Sequence = CreateSequence();
	SequenceProvider->AddSequence(Sequence);
}

bool FAvaSequencer::DuplicateSequence_CanExecute() const
{
	return GetProvider().CanEditOrPlaySequences();
}

void FAvaSequencer::DuplicateSequence_Execute()
{
	IAvaSequenceProvider* const SequenceProvider = Provider.GetSequenceProvider();
	if (!SequenceTreeView.IsValid() || !SequenceProvider)
	{
		return;
	}

	const TArray<FAvaSequenceItemPtr> SelectedItems = SequenceTreeView->GetSelectedItems();
	if (SelectedItems.IsEmpty())
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("DuplicateSequenceTransaction", "Duplicate Sequence"));

	UObject* const Outer = SequenceProvider->ToUObject();
	Outer->Modify();

	for (const FAvaSequenceItemPtr& Item : SelectedItems)
	{
		UAvaSequence* const TemplateSequence = Item->GetSequence();
		check(TemplateSequence);

		UAvaSequence* const Sequence = DuplicateObject<UAvaSequence>(TemplateSequence, Outer);
		SequenceProvider->AddSequence(Sequence);
	}
}

bool FAvaSequencer::ExportSequence_IsVisible() const
{
	return Provider.CanExportSequences();
}

bool FAvaSequencer::ExportSequence_CanExecute() const
{
	return SequenceTreeView.IsValid()
		&& !SequenceTreeView->GetSelectedItems().IsEmpty();
}

void FAvaSequencer::ExportSequence_Execute()
{
	if (!Provider.CanExportSequences() || !SequenceTreeView.IsValid())
	{
		return;
	}

	const TArray<FAvaSequenceItemPtr> SelectedItems = SequenceTreeView->GetSelectedItems();
	if (SelectedItems.IsEmpty())
	{
		return;
	}

	TArray<UAvaSequence*> SequencesToExport;
	SequencesToExport.Reserve(SelectedItems.Num());

	for (const FAvaSequenceItemPtr& Item : SelectedItems)
	{
		if (!Item.IsValid())
		{
			continue;
		}

		if (UAvaSequence* Sequence = Item->GetSequence())
		{
			SequencesToExport.Add(Sequence);
		}
	}

	Provider.ExportSequences(SequencesToExport);
}

bool FAvaSequencer::DeleteSequence_CanExecute() const
{
	return GetProvider().CanEditOrPlaySequences();
}

void FAvaSequencer::DeleteSequence_Execute()
{
	IAvaSequenceProvider* const SequenceProvider = Provider.GetSequenceProvider();
	if (!SequenceProvider)
	{
		return;
	}

	TArray<FAvaSequenceItemPtr> SelectedItems = SequenceTreeView->GetSelectedItems();

	const FScopedTransaction Transaction(LOCTEXT("DeleteSequenceTransaction", "Delete Sequence"));

	UObject* const Outer = SequenceProvider->ToUObject();
	check(Outer);
	Outer->Modify();

	TArray<UAvaSequence*> RemovedSequences;
	RemovedSequences.Reserve(SelectedItems.Num());

	// Remove the Selected Sequences from the List (not marked as garbage yet)
	for (const FAvaSequenceItemPtr& Item : SelectedItems)
	{
		check(Item.IsValid());
		if (UAvaSequence* Sequence = Item->GetSequence())
		{
			Sequence->Modify();
			SequenceProvider->RemoveSequence(Sequence);
			RemovedSequences.Add(Sequence);
		}
	}

	// Set the Viewed Sequence to the Default one
	SetViewedSequence(GetDefaultSequence());

	// Once a new viewed sequence is set, the removed sequences can now be marked as garbage 
	for (UAvaSequence* Sequence : RemovedSequences)
	{
		Sequence->OnSequenceRemoved();
	}

	if (UBlueprint* const Blueprint = Cast<UBlueprint>(Outer))
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}
}

bool FAvaSequencer::RelabelSequence_CanExecute() const
{
	const bool bCanEditSequences = GetProvider().CanEditOrPlaySequences();
	return bCanEditSequences && SequenceTreeView->GetNumItemsSelected() == 1;
}

void FAvaSequencer::RelabelSequence_Execute()
{
	TArray<FAvaSequenceItemPtr> SelectedItems = SequenceTreeView->GetSelectedItems();
	check(SelectedItems.Num() == 1);

	const FAvaSequenceItemPtr& SelectedItem = SelectedItems[0];
	SelectedItem->RequestRelabel();
}

bool FAvaSequencer::PlaySelected_CanExecute() const
{
	const bool bCanEditSequences = GetProvider().CanEditOrPlaySequences();
	return bCanEditSequences && SequenceTreeView->GetNumItemsSelected() > 0;
}

void FAvaSequencer::PlaySelected_Execute()
{
	const TArray<FAvaSequenceItemPtr> SelectedItems = SequenceTreeView->GetSelectedItems();

	FAvaSequencePlayParams PlaySettings;
	PlaySettings.AdvancedSettings.bRestoreState = true;

	for (const FAvaSequenceItemPtr& Item : SelectedItems)
	{
		UAvaSequence* const Sequence = Item.IsValid()
			? Item->GetSequence()
			: nullptr;

		IAvaSequencePlaybackObject* const PlaybackObject = Provider.GetPlaybackObject();
		if (Sequence && PlaybackObject)
		{
			PlaybackObject->PlaySequence(Sequence, PlaySettings);
		}
	}
}

bool FAvaSequencer::ContinueSelected_CanExecute() const
{
	const bool bCanEditSequences = GetProvider().CanEditOrPlaySequences();
	return bCanEditSequences && SequenceTreeView->GetNumItemsSelected() > 0;
}

void FAvaSequencer::ContinueSelected_Execute()
{
	const TArray<FAvaSequenceItemPtr> SelectedItems = SequenceTreeView->GetSelectedItems();
	
	for (const FAvaSequenceItemPtr& Item : SelectedItems)
	{
		UAvaSequence* const Sequence = Item.IsValid()
			? Item->GetSequence()
			: nullptr;

		IAvaSequencePlaybackObject* const PlaybackObject = Provider.GetPlaybackObject();
		if (Sequence && PlaybackObject)
		{
			PlaybackObject->ContinueSequence(Sequence);
		}
	}
}

bool FAvaSequencer::StopSelected_CanExecute() const
{
	return SequenceTreeView->GetNumItemsSelected() > 0;
}

void FAvaSequencer::StopSelected_Execute()
{
	const TArray<FAvaSequenceItemPtr> SelectedItems = SequenceTreeView->GetSelectedItems();

	for (const FAvaSequenceItemPtr& Item : SelectedItems)
	{
		UAvaSequence* const Sequence = Item.IsValid()
			? Item->GetSequence()
			: nullptr;

		IAvaSequencePlaybackObject* const PlaybackObject = Provider.GetPlaybackObject();
		if (Sequence && PlaybackObject)
		{
			PlaybackObject->StopSequence(Sequence);
		}
	}
}

UObject* FAvaSequencer::FindObjectToPossess(UObject* InParentObject, const FMovieScenePossessable& InPossessable)
{
	if (!IsValid(InParentObject))
	{
		return nullptr;
	}

	UClass* const PossessableClass = const_cast<UClass*>(InPossessable.GetPossessedObjectClass());

	// Try to find the Object that matches BOTH the Possessable Name and Possessed Object Class
	constexpr bool bExactClass = true;
	if (UObject* const FoundObject = StaticFindObject(PossessableClass, InParentObject, *InPossessable.GetName(), bExactClass))
	{
		return FoundObject;
	}

	const FName ObjectName(*InPossessable.GetName(), FNAME_Add);

	// If nothing was found via StaticFindObject, there is the possibility this is a nested subobject that just happens
	// to be under the Parent Object (e.g. an Actor) to avoid nesting in outliner or limitations of how the sequence resolves bindings
	TArray<UObject*> Objects;
	constexpr bool bIncludeNestedObjects = true;
	GetObjectsWithOuter(InParentObject, Objects, bIncludeNestedObjects);

	for (UObject* Object : Objects)
	{
		if (!Object)
		{
			continue;
		}

		const bool bMatchesName  = Object->GetFName() == ObjectName;
		const bool bMatchesClass = Object->GetClass() == PossessableClass;

		if (bMatchesName && bMatchesClass)
		{
			return Object;
		}
	}

	return nullptr;
}

void FAvaSequencer::FixBindingPaths()
{
	UAvaSequence* const Sequence   = GetViewedSequence();
	UObject* const PlaybackContext = GetPlaybackContext();
	if (!IsValid(Sequence) || !IsValid(PlaybackContext))
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("FixBindingPaths", "Fix Binding Paths"));

	Sequence->Modify();

	// pass in a null old context, which forces a replacement of all base bindings. Without further parameters, there's no knowledge of what the old context is
	int32 BindingsUpdatedCount = Sequence->UpdateBindings(nullptr, FTopLevelAssetPath(PlaybackContext));
	if (BindingsUpdatedCount == 0)
	{
		Transaction.Cancel();
		return;
	}

	TSharedRef<ISequencer> Sequencer = GetSequencer();
	Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
}

void FAvaSequencer::FixInvalidBindings()
{
	UObject* const PlaybackContext = GetPlaybackContext();
	if (!IsValid(PlaybackContext))
	{
		return;
	}

	UAvaSequence* const Sequence = GetViewedSequence();
	if (!IsValid(Sequence))
	{
		return;
	}

	UMovieScene* const MovieScene = Sequence->GetMovieScene();
	if (!IsValid(MovieScene))
	{
		return;
	}

	if (MovieScene->IsReadOnly())
	{
		FSequencerUtilities::ShowReadOnlyError();
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("FixInvalidBindings", "Fix Invalid Bindings"));

	TSet<const FMovieScenePossessable*> ProcessedPossessables;

	for (int32 PossessableIndex = 0; PossessableIndex < MovieScene->GetPossessableCount(); ++PossessableIndex)
	{
		FMovieScenePossessable& Possessable = MovieScene->GetPossessable(PossessableIndex);
		FixPossessable(*Sequence, Possessable, PlaybackContext, ProcessedPossessables);
	}
}

FString FAvaSequencer::GetObjectName(UObject* InObject)
{
	check(InObject);
	if (AActor* const Actor = Cast<AActor>(InObject))
	{
		return Actor->GetActorLabel();
	}
	return InObject->GetName();
}

UObject* FAvaSequencer::FindResolutionContext(UAvaSequence& InSequence
	, UMovieScene& InMovieScene
	, const FGuid& InParentPossessableGuid
	, UObject* InPlaybackContext
	, TFunctionRef<TArray<UObject*, TInlineAllocator<1>>(const FGuid&, UObject*)> InFindObjectsFunc)
{
	if (!InPlaybackContext || !InParentPossessableGuid.IsValid() || !InSequence.AreParentContextsSignificant())
	{
		return InPlaybackContext;
	}

	UObject* ResolutionContext = nullptr;

	// Recursive call up the hierarchy
	if (FMovieScenePossessable* const ParentPossessable = InMovieScene.FindPossessable(InParentPossessableGuid))
	{
		ResolutionContext = FAvaSequencer::FindResolutionContext(InSequence
			, InMovieScene
			, ParentPossessable->GetParent()
			, InPlaybackContext
			, InFindObjectsFunc);
	}

	if (!ResolutionContext)
	{
		ResolutionContext = InPlaybackContext;
	}

	TArray<UObject*, TInlineAllocator<1>> FoundObjects = InFindObjectsFunc(InParentPossessableGuid, ResolutionContext);
	if (FoundObjects.IsEmpty())
	{
		return InPlaybackContext;
	}

	return FoundObjects[0] ? FoundObjects[0] : InPlaybackContext;
}

UObject* FAvaSequencer::FindResolutionContext(UAvaSequence& InSequence
	, UMovieScene& InMovieScene
	, const FGuid& InParentGuid
	, UObject* InPlaybackContext)
{
	auto FindObjectsFunc = [&InSequence](const FGuid& InGuid, UObject* InContextChecked)
		{
			TArray<UObject*, TInlineAllocator<1>> BoundObjects;

			InSequence.LocateBoundObjects(InGuid, UE::UniversalObjectLocator::FResolveParams(InContextChecked), BoundObjects);
			return BoundObjects;
		};

	return FindResolutionContext(InSequence, InMovieScene, InParentGuid, InPlaybackContext, FindObjectsFunc);
}

bool FAvaSequencer::FixPossessable(UAvaSequence& InSequence
	, const FMovieScenePossessable& InPossessable
	, UObject* InPlaybackContext
	, TSet<const FMovieScenePossessable*>& InOutProcessedPossessables)
{
	if (!ensureAlways(InPlaybackContext && InSequence.GetMovieScene()))
	{
		return false;
	}

	// This Possessable has already been fixed or was already verified as valid, skip
	if (InOutProcessedPossessables.Contains(&InPossessable))
	{
		return true;
	}

	UMovieScene& MovieScene = *InSequence.GetMovieScene();

	const FGuid& ParentGuid = InPossessable.GetParent();

	// Fix Parent Possessable first since if Parent Contexts are significant the parent will be needed to resolve the child
	if (FMovieScenePossessable* const PossessableParent = MovieScene.FindPossessable(ParentGuid))
	{
		if (!FixPossessable(InSequence, *PossessableParent, InPlaybackContext, InOutProcessedPossessables))
		{
			UE_LOG(LogAvaSequencer, Warning
				, TEXT("Parent '%s' of Possessable '%s' could not be fixed.")
				, *PossessableParent->GetName()
				, *InPossessable.GetName());
			return false;
		}
	}

	const FGuid& Guid = InPossessable.GetGuid();

	UObject* const ResolutionContext = FAvaSequencer::FindResolutionContext(InSequence
		, MovieScene
		, ParentGuid
		, InPlaybackContext);


	TArrayView<TWeakObjectPtr<>> BoundObjects = ResolveBoundObjects(Guid, &InSequence);

	// If Bound Objects isn't empty, then it means Possessable is valid, so add to Valid List and early return
	if (!BoundObjects.IsEmpty() && BoundObjects[0].IsValid())
	{
		InOutProcessedPossessables.Add(&InPossessable);
		return true;
	}

	if (UObject* const Object = FAvaSequencer::FindObjectToPossess(ResolutionContext, InPossessable))
	{
		InSequence.Modify();
		InSequence.BindPossessableObject(Guid, *Object, ResolutionContext);
		InOutProcessedPossessables.Add(&InPossessable);
		return true;
	}

	return false;
}

void FAvaSequencer::FixBindingHierarchy()
{
	UAvaSequence* const Sequence   = GetViewedSequence();
    UObject* const PlaybackContext = GetPlaybackContext();
    if (!Sequence || !PlaybackContext)
    {
    	return;
    }

	UMovieScene* const MovieScene = Sequence->GetMovieScene();
	if (!MovieScene || MovieScene->IsReadOnly())
	{
		return;
	}

	TSharedRef<ISequencer> Sequencer = GetSequencer();

	FScopedTransaction Transaction(LOCTEXT("FixBindingHierarchy", "Fix Binding Hierarchy"));
	MovieScene->Modify();

	// Iterates all the possessables that are not necessarily under a set parent (via FMovieScenePossessable::GetParent), but
	// have a bound object that does have a valid parent found via UAvaSequence::GetParentObject, hence the word usage of "found" over "set"
	auto ForEachPossessableWithFoundParent = [this, MovieScene, Sequence, PlaybackContext]
		(TFunctionRef<void(UObject& InObject, FMovieScenePossessable& InPossessable, UObject& InParentObject)> InFunc)
		{
			for (int32 Index = 0; Index < MovieScene->GetPossessableCount(); ++Index)
			{
				FMovieScenePossessable& Possessable = MovieScene->GetPossessable(Index);

				TArrayView<TWeakObjectPtr<>> BoundObjects = ResolveBoundObjects(Possessable.GetGuid(), Sequence);

				if (BoundObjects.IsEmpty() || !BoundObjects[0].IsValid())
				{
					continue;
				}

				if (UObject* ParentObject = Sequence->GetParentObject(BoundObjects[0].Get()))
				{
					InFunc(*BoundObjects[0], Possessable, *ParentObject);
				}
			}
		};

	// Pass #1: Ensure that all the Parent Objects have a valid possessable handle
	ForEachPossessableWithFoundParent(
		[Sequencer](UObject& InPossessableObject, FMovieScenePossessable& InPossessable, UObject& InParentObject)
		{
			constexpr bool bCreateHandleToObject = true;
			Sequencer->GetHandleToObject(&InParentObject, bCreateHandleToObject);
		});

	// Pass #2: Fix the hierarchy now that all relevant objects have a valid handle
	ForEachPossessableWithFoundParent(
		[Sequencer, MovieScene, Sequence, PlaybackContext](UObject& InObject, FMovieScenePossessable& InPossessable, UObject& InParentObject)
		{
			constexpr bool bCreateHandleToObject = false;
			const FGuid ParentGuid = Sequencer->GetHandleToObject(&InParentObject, bCreateHandleToObject);

			// Parent Guid must be valid, as it was created in pass #1 if missing
			if (!ParentGuid.IsValid())
			{
				UE_LOG(LogAvaSequencer, Error
					, TEXT("Could not create handle to parent object %s for Possessable %s (GUID: %s)")
					, *InParentObject.GetName()
					, *InPossessable.GetName()
					, *InPossessable.GetGuid().ToString());
				return;
			}

			if (InPossessable.GetParent() != ParentGuid)
			{
				InPossessable.SetParent(ParentGuid, MovieScene);

				UObject* const Context = Sequence->AreParentContextsSignificant()
					? &InParentObject
					: PlaybackContext;

				// Recalculate the Binding Path
				Sequence->UnbindPossessableObjects(InPossessable.GetGuid());
				Sequence->BindPossessableObject(InPossessable.GetGuid(), InObject, Context);
			}
		});

	Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
}

TArrayView<TWeakObjectPtr<>> FAvaSequencer::ResolveBoundObjects(const FGuid& InBindingId, class UMovieSceneSequence* Sequence) const
{
	TSharedRef<ISequencer> Sequencer = GetSequencer();
	TSharedRef<UE::MovieScene::FSharedPlaybackState> SharedPlaybackState = Sequencer->GetSharedPlaybackState();
	// TODO: It would be better if FAvaSequencer saved the SequenceID. It's possible looking that this might always be FMovieSceneSequenceID::Root, but I'm unsure.
	if (FMovieSceneEvaluationState* EvaluationState = SharedPlaybackState->FindCapability<FMovieSceneEvaluationState>())
	{
		FMovieSceneSequenceIDRef SequenceID = EvaluationState->FindSequenceId(Sequence);
		return Sequencer->FindBoundObjects(InBindingId, SequenceID);
	}
	return TArrayView<TWeakObjectPtr<>>();
}

TSharedRef<ISequencer> FAvaSequencer::GetSequencer() const
{
	const_cast<FAvaSequencer*>(this)->EnsureSequencer();
	return SequencerWeak.Pin().ToSharedRef();
}

USequencerSettings* FAvaSequencer::GetSequencerSettings() const
{
	if (TSharedPtr<ISequencer> Sequencer = SequencerWeak.Pin())
	{
		return Sequencer->GetSequencerSettings();
	}
	return nullptr;
}

void FAvaSequencer::SetBaseCommandList(const TSharedPtr<FUICommandList>& InBaseCommandList)
{
	if (InBaseCommandList.IsValid())
	{
		InBaseCommandList->Append(CommandList);	
	}
}

UAvaSequence* FAvaSequencer::GetViewedSequence() const
{
	return ViewedSequenceWeak.Get();
}

void FAvaSequencer::SetViewedSequence(UAvaSequence* InSequenceToView)
{
	if (InSequenceToView == ViewedSequenceWeak)
	{
		return;
	}

	UAvaSequence* OldSequence = ViewedSequenceWeak.Get(/*bEvenIfPendingKill*/true);
	ViewedSequenceWeak = InSequenceToView;
	NotifyViewedSequenceChanged(OldSequence);
}

TArray<UAvaSequence*> FAvaSequencer::GetSequencesForObject(UObject* InObject) const
{
	TArray<UAvaSequence*> OutSequences;

	if (!IsValid(InObject))
	{
		return OutSequences;
	}

	IAvaSequenceProvider* const SequenceProvider = Provider.GetSequenceProvider();
	if (!SequenceProvider)
	{
		return OutSequences;
	}

	for (UAvaSequence* const Sequence : SequenceProvider->GetSequences())
	{
		if (!IsValid(Sequence))
		{
			continue;
		}

		const FGuid Guid = Sequence->FindGuidFromObject(InObject);

		if (Guid.IsValid())
		{
			OutSequences.Add(Sequence);
		}
	}
	return OutSequences;
}

TSharedRef<SWidget> FAvaSequencer::CreateSequenceWidget()
{
	TSharedRef<ISequencer> Sequencer = GetSequencer();

	TArray<TSharedRef<IAvaSequenceSectionDetails>> Sections =
		{
			MakeShared<FAvaSequenceSelectionDetails>(),
			MakeShared<FAvaSequenceSettingsDetails>(),
			MakeShared<FAvaSequencePlaybackDetails>(),
		};

	return SNew(SSplitter)
		+ SSplitter::Slot()
		.Value(0.15f)
		[
			GetSequenceTreeWidget()
		]
		+ SSplitter::Slot()
		.Value(0.65f)
		[
			SNew(SOverlay)
			.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("Sequencer")))
			+ SOverlay::Slot()
			[
				Sequencer->GetSequencerWidget()
			]
		]
		+ SSplitter::Slot()
		.Value(0.2f)
		[
			SNew(SAvaSequenceDetails, SharedThis(this), MoveTemp(Sections))
			.InitiallySelectedSections(SelectedSections)
			.OnSelectedSectionsChanged_Lambda([this](const TSet<FName>& InNewSelectedSections)
				{
					SelectedSections = InNewSelectedSections;
				})
		];
}

void FAvaSequencer::OnActorsCopied(FString& InOutCopiedData, TConstArrayView<AActor*> InCopiedActors)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FAvaSequencer::OnActorsCopied);
	EnsureSequencer();
	FAvaSequenceExporter Exporter(SharedThis(this));
	Exporter.ExportText(InOutCopiedData, InCopiedActors);
}

void FAvaSequencer::OnActorsPasted(FStringView InPastedData, const TMap<FName, AActor*>& InPastedActors)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FAvaSequencer::OnActorsPasted);
	EnsureSequencer();
	FAvaSequenceImporter Importer(SharedThis(this));
	Importer.ImportText(InPastedData, InPastedActors);
}

void FAvaSequencer::OnEditorSelectionChanged(const FAvaEditorSelection& InEditorSelection)
{
	if (!bCanProcessSequencerSelections)
	{
		return;
	}

	if (bUpdatingSelection || bSelectedFromSequencer)
	{
		bSelectedFromSequencer = false;
		return;
	}

	TGuardValue<bool> Guard(bUpdatingSelection, true);

	TSharedRef<ISequencer> Sequencer = GetSequencer();
	Sequencer->EmptySelection();

	using namespace UE::Sequencer;

	{
		FSelectionEventSuppressor SuppressSelectionEvents = Sequencer->GetViewModel()->GetSelection()->SuppressEvents();

		for (UObject* const SelectedObject : InEditorSelection.GetSelectedObjects<UObject, EAvaSelectionSource::All>())
		{
			if (IsValid(SelectedObject))
			{
				FGuid SelectedObjectGuid = Sequencer->FindObjectId(*SelectedObject, Sequencer->GetFocusedTemplateID());
				Sequencer->SelectObject(SelectedObjectGuid);
			}
		}
	}

	// Scroll Selected Node to View
	if (TSharedPtr<SOutlinerView> OutlinerView = GetOutlinerView())
	{
		for (const TViewModelPtr<FViewModel>& SelectedNode : Sequencer->GetViewModel()->GetSelection()->Outliner)
		{
			TSharedPtr<FViewModel> Parent = SelectedNode->GetParent();
			while (Parent.IsValid())
			{
				OutlinerView->SetItemExpansion(UE::Sequencer::CastViewModel<IOutlinerExtension>(Parent), true);
				Parent = Parent->GetParent();
			}
			OutlinerView->RequestScrollIntoView(UE::Sequencer::CastViewModel<IOutlinerExtension>(SelectedNode));
			break;
		}
	}
}

void FAvaSequencer::NotifyOnSequenceTreeChanged()
{
	IAvaSequenceProvider* const SequenceProvider = Provider.GetSequenceProvider();
	if (!SequenceTreeView.IsValid() || !SequenceProvider)
	{
		return;
	}

	const TSet<TWeakObjectPtr<UAvaSequence>> RootSequences(SequenceProvider->GetRootSequences());

	TSet<TWeakObjectPtr<UAvaSequence>> SeenRoots;
	SeenRoots.Reserve(RootSequences.Num());

	// Remove Current Root Items that are not in the Latest Root Set
	for (TArray<FAvaSequenceItemPtr>::TIterator Iter(RootSequenceItems); Iter; ++Iter)
	{
		const FAvaSequenceItemPtr Item = *Iter;

		if (!Item.IsValid())
		{
			Iter.RemoveCurrent();
			continue;
		}

		TObjectPtr<UAvaSequence> UnderlyingSequence = Item->GetSequence();

		if (UnderlyingSequence && RootSequences.Contains(UnderlyingSequence))
		{
			SeenRoots.Add(UnderlyingSequence);
		}
		else
		{
			Iter.RemoveCurrent();
		}
	}

	// Make New Root Items for the Sequences that were not Seen
	{
		TArray<TWeakObjectPtr<UAvaSequence>> NewRoots = RootSequences.Difference(SeenRoots).Array();
		
		RootSequenceItems.Reserve(RootSequenceItems.Num() + NewRoots.Num());

		const TSharedPtr<FAvaSequencer> This = SharedThis(this);
		
		for (const TWeakObjectPtr<UAvaSequence>& Sequence : NewRoots)
		{
			TSharedPtr<FAvaSequenceItem> NewItem = MakeShared<FAvaSequenceItem>(Sequence.Get(), This);
			RootSequenceItems.Add(NewItem);
		}
	}

	//Refresh Children Iteratively
	TArray<FAvaSequenceItemPtr> RemainingItems = RootSequenceItems;
	while (!RemainingItems.IsEmpty())
	{
		if (FAvaSequenceItemPtr Item = RemainingItems.Pop())
		{
			Item->RefreshChildren();
			RemainingItems.Append(Item->GetChildren());
		}
	}

	// Ensure the new item representing the Viewed Sequence is selected
	UAvaSequence* ViewedSequence = GetViewedSequence();
	if (ViewedSequence && SequenceTree.IsValid())
	{
		SequenceTree->OnPostSetViewedSequence(ViewedSequence);
	}

	SequenceTreeView->RequestTreeRefresh();
}

void FAvaSequencer::PostUndo(bool bSuccess)
{
	// A just-added sequence might be removed due to this undo, so refresh 
	NotifyOnSequenceTreeChanged();
}

TSharedPtr<UE::Sequencer::SOutlinerView> FAvaSequencer::GetOutlinerView() const
{
	if (OutlinerViewWeak.IsValid())
	{
		return OutlinerViewWeak.Pin();
	}

	TSharedPtr<ISequencer> Sequencer = SequencerWeak.Pin();
	if (!Sequencer.IsValid())
	{
		return nullptr;
	}

	TSharedRef<SWidget> SequencerWidget = Sequencer->GetSequencerWidget();

	TArray<FChildren*> ChildrenRemaining = { SequencerWidget->GetChildren() };

	while (!ChildrenRemaining.IsEmpty())
	{
		FChildren* const Children = ChildrenRemaining.Pop();
		if (!Children)
		{
			continue;
		}

		const int32 WidgetCount = Children->Num();

		for (int32 Index = 0; Index < WidgetCount; ++Index)
		{
			const TSharedRef<SWidget> Widget = Children->GetChildAt(Index);
			if (Widget->GetType() == TEXT("SOutlinerView"))
			{
				TSharedRef<UE::Sequencer::SOutlinerView> OutlinerView = StaticCastSharedRef<UE::Sequencer::SOutlinerView>(Widget);
				const_cast<FAvaSequencer*>(this)->OutlinerViewWeak = OutlinerView;
				return OutlinerView;
			}
			ChildrenRemaining.Add(Widget->GetChildren());
		}
	}

	return nullptr;
}

void FAvaSequencer::InitSequencerCommandList()
{
	TSharedPtr<ISequencer> Sequencer = SequencerWeak.Pin();
	if (!Sequencer.IsValid())
	{
		return;
	}

	TSharedPtr<FUICommandList> SequencerCommandList = Sequencer->GetCommandBindings(ESequencerCommandBindings::Sequencer);
	if (!ensure(SequencerCommandList.IsValid()))
	{
		return;
	}

	SequencerCommandList->Append(CommandList);

	const FGenericCommands& GenericCommands = FGenericCommands::Get();

	// Remap Duplicate Action
	if (const FUIAction* const DuplicateAction = SequencerCommandList->GetActionForCommand(GenericCommands.Duplicate))
	{
		FUIAction OverrideAction;

		OverrideAction.ExecuteAction = FExecuteAction::CreateSP(this
			, &FAvaSequencer::ExecuteSequencerDuplication
			, DuplicateAction->ExecuteAction);

		OverrideAction.CanExecuteAction = DuplicateAction->CanExecuteAction;

		SequencerCommandList->UnmapAction(GenericCommands.Duplicate);
		SequencerCommandList->MapAction(GenericCommands.Duplicate, OverrideAction);
	}

	// Unmap Key Transform Commands
	const FSequencerCommands& SequencerCommands = FSequencerCommands::Get();
	SequencerCommandList->UnmapAction(SequencerCommands.AddTransformKey);
	SequencerCommandList->UnmapAction(SequencerCommands.AddTranslationKey);
	SequencerCommandList->UnmapAction(SequencerCommands.AddRotationKey);
	SequencerCommandList->UnmapAction(SequencerCommands.AddScaleKey);
}

void FAvaSequencer::ExecuteSequencerDuplication(FExecuteAction InExecuteAction)
{
	if (UWorld* InWorld = Provider.GetPlaybackContext()->GetWorld())
	{
		// HACK: Sequencer Duplicates Actors via UUnrealEdEngine::edactDuplicateSelected
		// Sequencer then expects that after this function is called, the GSelectedActors are the newly duplicated actors.
		// However, ULevelFactory::FactoryCreateText only changes selection when the World in question is the GWorld,
		// which is not true for Motion Design.
		// So for this we temporarily set GWorld to our Motion Design World so that selections happen correctly.
		UWorld* const OldGWorld = GWorld;
		GWorld = InWorld;
		InExecuteAction.ExecuteIfBound();
		GWorld = OldGWorld;
	}
}

void FAvaSequencer::OnUpdateCameraCut(UObject* InCameraObject, bool bInJumpCut)
{
	GetProvider().OnUpdateCameraCut(InCameraObject, bInJumpCut);
}

TSharedRef<FAvaEaseCurveTool> FAvaSequencer::GetEaseCurveTool() const
{
	return EaseCurveTool.ToSharedRef(); 
}

#undef LOCTEXT_NAMESPACE
