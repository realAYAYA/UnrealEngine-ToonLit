// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewModels/NiagaraSystemViewModel.h"

#include "Channels/MovieSceneChannelProxy.h"
#include "EdGraphSchema_NiagaraSystemOverview.h"
#include "EdGraphUtilities.h"
#include "Editor.h"
#include "EditorSupportDelegates.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UICommandList.h"
#include "ISequencerModule.h"
#include "Math/NumericLimits.h"
#include "Modules/ModuleManager.h"
#include "MovieScene.h"
#include "MovieSceneFolder.h"
#include "MovieSceneNiagaraEmitterTrack.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceCurveBase.h"
#include "NiagaraEditorData.h"
#include "NiagaraEditorModule.h"
#include "NiagaraEditorSettings.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterEditorData.h"
#include "NiagaraEmitterFactoryNew.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraGraph.h"
#include "NiagaraMessageManager.h"
#include "NiagaraMessageUtilities.h"
#include "NiagaraMessages.h"
#include "NiagaraNodeAssignment.h"
#include "NiagaraNodeEmitter.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeInput.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraNodeParameterMapGet.h"
#include "NiagaraOverviewNode.h"
#include "NiagaraParameterDefinitionsSubscriber.h"
#include "NiagaraScriptGraphViewModel.h"
#include "NiagaraScriptSource.h"
#include "NiagaraScriptVariable.h"
#include "NiagaraSequence.h"
#include "NiagaraSimulationStageBase.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemEditorData.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraSystemScriptViewModel.h"
#include "ScopedTransaction.h"
#include "ViewModels/NiagaraCurveSelectionViewModel.h"
#include "ViewModels/NiagaraEmitterHandleViewModel.h"
#include "ViewModels/NiagaraEmitterViewModel.h"
#include "ViewModels/NiagaraOverviewGraphViewModel.h"
#include "ViewModels/NiagaraPlaceholderDataInterfaceManager.h"
#include "ViewModels/NiagaraScratchPadUtilities.h"
#include "ViewModels/NiagaraScratchPadViewModel.h"
#include "ViewModels/NiagaraSystemEditorDocumentsViewModel.h"
#include "ViewModels/NiagaraSystemGraphSelectionViewModel.h"
#include "ViewModels/NiagaraSystemSelectionViewModel.h"
#include "ViewModels/Stack/NiagaraStackEntry.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "ViewModels/Stack/NiagaraStackViewModel.h"
#include "ViewModels/TNiagaraViewModelManager.h"
#include "ViewModels/HierarchyEditor/NiagaraUserParametersHierarchyViewModel.h"
#include "ViewModels/NiagaraParameterPanelViewModel.h"


DECLARE_CYCLE_STAT(TEXT("Niagara - SystemViewModel - CompileSystem"), STAT_NiagaraEditor_SystemViewModel_CompileSystem, STATGROUP_NiagaraEditor);

#define LOCTEXT_NAMESPACE "NiagaraSystemViewModel"

template<> TMap<UNiagaraSystem*, TArray<FNiagaraSystemViewModel*>> TNiagaraViewModelManager<UNiagaraSystem, FNiagaraSystemViewModel>::ObjectsToViewModels{};

FNiagaraSystemViewModelOptions::FNiagaraSystemViewModelOptions()
	: bCanAutoCompile(true)
	, bCanSimulate(true)
	, bIsForDataProcessingOnly(false)
{
}

FNiagaraSystemViewModel::FNiagaraSystemViewModel()
	: System(nullptr)
	, PreviewComponent(nullptr)
	, SystemInstance(nullptr)
	, NiagaraSequence(nullptr)
	, bSettingSequencerTimeDirectly(false)
	, bForceAutoCompileOnce(false)
	, bSupportCompileForEdit(true)
	, bUpdatingEmittersFromSequencerDataChange(false)
	, bUpdatingSequencerFromEmitterDataChange(false)
	, bUpdatingSystemSelectionFromSequencer(false)
	, bUpdatingSequencerSelectionFromSystem(false)
	, bResetingSequencerTracks(false)
	, EditorSettings(GetMutableDefault<UNiagaraEditorSettings>())
	, bResetRequestPending(false)
	, bCompilePendingCompletion(false)
	, SystemStackViewModel(nullptr)
	, EditorDocumentsViewModel(nullptr)
	, SelectionViewModel(nullptr)
	, ScriptScratchPadViewModel(nullptr)
	, bPendingAssetMessagesChanged(true)
{
}

void FNiagaraSystemViewModel::Initialize(UNiagaraSystem& InSystem, FNiagaraSystemViewModelOptions InOptions)
{
	System = &InSystem;
	System->bCompileForEdit = bSupportCompileForEdit;

	RegisteredHandle = RegisterViewModelWithMap(System, this);

	bCanModifyEmittersFromTimeline = InOptions.bCanModifyEmittersFromTimeline;
	bCanAutoCompile = InOptions.bCanAutoCompile;
	bCanSimulate = InOptions.bCanSimulate;
	EditMode = InOptions.EditMode;
	OnGetSequencerAddMenuContent = InOptions.OnGetSequencerAddMenuContent;
	SystemMessageLogGuidKey = InOptions.MessageLogGuid;
	bIsForDataProcessingOnly = InOptions.bIsForDataProcessingOnly;

	if (bIsForDataProcessingOnly == false)
	{
		GEditor->RegisterForUndo(this);
	}

	SystemChangedDelegateHandle = System->OnSystemPostEditChange().AddSP(this, &FNiagaraSystemViewModel::SystemChanged);

	SelectionViewModel = NewObject<UNiagaraSystemSelectionViewModel>(GetTransientPackage());
	SelectionViewModel->Initialize(this->AsShared());
	SelectionViewModel->OnEmitterHandleIdSelectionChanged().AddSP(this, &FNiagaraSystemViewModel::SystemSelectionChanged);

	SystemScriptViewModel = MakeShared<FNiagaraSystemScriptViewModel>(bIsForDataProcessingOnly);
	SystemScriptViewModel->Initialize(GetSystem());

	OverviewGraphViewModel = MakeShared<FNiagaraOverviewGraphViewModel>();
	OverviewGraphViewModel->Initialize(this->AsShared());

	SystemStackViewModel = NewObject<UNiagaraStackViewModel>(GetTransientPackage());
	SystemStackViewModel->OnStructureChanged().AddSP(this, &FNiagaraSystemViewModel::StackViewModelStructureChanged);

	EditorDocumentsViewModel = NewObject< UNiagaraSystemEditorDocumentsViewModel >(GetTransientPackage());
	EditorDocumentsViewModel->Initialize(this->AsShared());

	ScriptScratchPadViewModel = NewObject<UNiagaraScratchPadViewModel>(GetTransientPackage());
	ScriptScratchPadViewModel->Initialize(this->AsShared());
	ScriptScratchPadViewModel->OnScriptRenamed().AddSP(this, &FNiagaraSystemViewModel::ScratchPadScriptsChanged);
	ScriptScratchPadViewModel->OnScriptDeleted().AddSP(this, &FNiagaraSystemViewModel::ScratchPadScriptsChanged);

	CurveSelectionViewModel = NewObject<UNiagaraCurveSelectionViewModel>(GetTransientPackage());
	CurveSelectionViewModel->Initialize(this->AsShared());

	ScalabilityViewModel = NewObject<UNiagaraSystemScalabilityViewModel>(GetTransientPackage());
	ScalabilityViewModel->Initialize(this->AsShared());

	UserParameterPanelViewModel = MakeShared<FNiagaraUserParameterPanelViewModel>();
	
	UserParametersHierarchyViewModel = NewObject<UNiagaraUserParametersHierarchyViewModel>(GetTransientPackage());
	UserParametersHierarchyViewModel->Initialize(this->AsShared());
	
	PlaceholderDataInterfaceManager = MakeShared<FNiagaraPlaceholderDataInterfaceManager>(this->AsShared());

	SystemGraphSelectionViewModel = MakeShared<FNiagaraSystemGraphSelectionViewModel>();
	SystemGraphSelectionViewModel->Initialize(this->AsShared());
	
	SetupPreviewComponentAndInstance();
	SetupSequencer();
	RefreshAll();
	AddSystemEventHandlers();
}

bool FNiagaraSystemViewModel::IsValid() const
{
	return System != nullptr;
}

void FNiagaraSystemViewModel::DumpToText(FString& ExportText)
{
	TSet<UObject*> ExportObjs;
	ExportObjs.Add(System);
	FEdGraphUtilities::ExportNodesToText(ExportObjs, ExportText);
}

void FNiagaraSystemViewModel::SetParameterPanelViewModel(TSharedPtr<INiagaraParameterPanelViewModel> InVM) 
{
	if (ParameterPanelViewModel.IsValid())
	{
		ParameterPanelViewModel.Pin()->GetOnInvalidateCachedDependencies().RemoveAll(this);
	}

	ParameterPanelViewModel = InVM;

	if (InVM.IsValid())
	{
		InVM->GetOnInvalidateCachedDependencies().AddRaw(this, &FNiagaraSystemViewModel::InvalidateCachedParams);
	}
}

void FNiagaraSystemViewModel::InvalidateCachedParams()
{
	GetSystemStackViewModel()->InvalidateCachedParameterUsage();
	for (TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleVM : GetEmitterHandleViewModels())
	{
		UNiagaraStackViewModel* EmitterStackViewModel = EmitterHandleVM->GetEmitterStackViewModel();
		if (EmitterStackViewModel)
		{
			EmitterStackViewModel->InvalidateCachedParameterUsage();
		}
	}
}

void FNiagaraSystemViewModel::Cleanup()
{
	if (SystemInstance)
	{
		SystemInstance->OnInitialized().RemoveAll(this);
		SystemInstance->OnReset().RemoveAll(this);
		SystemInstance = nullptr;
	}

	if (PreviewComponent)
	{
		PreviewComponent->OnSystemInstanceChanged().RemoveAll(this);
		PreviewComponent->DeactivateImmediate();
		PreviewComponent = nullptr;
	}

	if(bIsForDataProcessingOnly == false)
	{
		GEditor->UnregisterForUndo(this);
	}

	if (System)
	{
		System->bCompileForEdit = false;
		System->OnSystemPostEditChange().Remove(SystemChangedDelegateHandle);
	}

	// Make sure that we clear out all of our event handlers
	UnregisterViewModelWithMap(RegisteredHandle);

	for (TSharedRef<FNiagaraEmitterHandleViewModel>& HandleRef : EmitterHandleViewModels)
	{
		HandleRef->OnPropertyChanged().RemoveAll(this);
		HandleRef->OnNameChanged().RemoveAll(this);
		HandleRef->GetEmitterViewModel()->OnPropertyChanged().RemoveAll(this);
		HandleRef->GetEmitterViewModel()->OnScriptCompiled().RemoveAll(this);
		HandleRef->GetEmitterViewModel()->OnScriptGraphChanged().RemoveAll(this);
		HandleRef->GetEmitterViewModel()->OnScriptParameterStoreChanged().RemoveAll(this);
		HandleRef->GetEmitterStackViewModel()->OnStructureChanged().RemoveAll(this);
		HandleRef->Cleanup();
	}
	EmitterHandleViewModels.Empty();

	if (Sequencer.IsValid())
	{
		Sequencer->OnMovieSceneDataChanged().RemoveAll(this);
		Sequencer->OnGlobalTimeChanged().RemoveAll(this);
		Sequencer->GetSelectionChangedTracks().RemoveAll(this);
		Sequencer->GetSelectionChangedSections().RemoveAll(this);
		Sequencer.Reset();
	}

	RemoveSystemEventHandlers();
	SystemScriptViewModel.Reset();

	if (SystemStackViewModel != nullptr)
	{
		SystemStackViewModel->OnStructureChanged().RemoveAll(this);
		SystemStackViewModel->Finalize();
		SystemStackViewModel = nullptr;
	}

	if (EditorDocumentsViewModel != nullptr)
	{
		EditorDocumentsViewModel->Finalize();
		EditorDocumentsViewModel = nullptr;
	}

	if (SelectionViewModel != nullptr)
	{
		SelectionViewModel->OnEmitterHandleIdSelectionChanged().RemoveAll(this);
		SelectionViewModel->Finalize();
		SelectionViewModel = nullptr;
	}

	if (ScriptScratchPadViewModel != nullptr)
	{
		ScriptScratchPadViewModel->OnScriptRenamed().RemoveAll(this);
		ScriptScratchPadViewModel->OnScriptDeleted().RemoveAll(this);
		ScriptScratchPadViewModel->Finalize();
		ScriptScratchPadViewModel = nullptr;
	}

	if (CurveSelectionViewModel != nullptr)
	{
		CurveSelectionViewModel->Finalize();
		CurveSelectionViewModel = nullptr;
	}

	if(ScalabilityViewModel != nullptr)
	{
		ScalabilityViewModel = nullptr;
	}
	
	if(UserParametersHierarchyViewModel != nullptr)
	{
		UserParametersHierarchyViewModel->Finalize();
		UserParametersHierarchyViewModel = nullptr;
	}

	if(UserParameterPanelViewModel.IsValid())
	{
		UserParameterPanelViewModel.Reset();
	}

	if (PlaceholderDataInterfaceManager.IsValid())
	{
		PlaceholderDataInterfaceManager.Reset();
	}

	if (SystemGraphSelectionViewModel != nullptr)
	{
		SystemGraphSelectionViewModel = nullptr;
	}

	if (ParameterPanelViewModel.IsValid())
	{
		ParameterPanelViewModel.Pin()->GetOnInvalidateCachedDependencies().RemoveAll(this);
	}

	System = nullptr;
}

FNiagaraSystemViewModel::~FNiagaraSystemViewModel()
{
	Cleanup();
}

INiagaraParameterDefinitionsSubscriber* FNiagaraSystemViewModel::GetParameterDefinitionsSubscriber()
{
	if(EditMode == ENiagaraSystemViewModelEditMode::SystemAsset)
	{ 
		return System;
	}
	else /**EditMode == ENiagaraSystemViewModelEditMode::EmitterAsset */
	{
		return GetEmitterHandleViewModels()[0]->GetEmitterHandle()->GetInstance().Emitter;
	}
}

FText FNiagaraSystemViewModel::GetDisplayName() const
{
	return FText::FromString(System->GetName());
}

const TArray<TSharedRef<FNiagaraEmitterHandleViewModel>>& FNiagaraSystemViewModel::GetEmitterHandleViewModels() const
{
	return EmitterHandleViewModels;
}

TSharedPtr<FNiagaraEmitterHandleViewModel> FNiagaraSystemViewModel::GetEmitterHandleViewModelById(FGuid InEmitterHandleId)
{
	for (TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel : EmitterHandleViewModels)
	{
		if (EmitterHandleViewModel->GetId() == InEmitterHandleId)
		{
			return EmitterHandleViewModel;
		}
	}
	return TSharedPtr<FNiagaraEmitterHandleViewModel>();
}

TSharedPtr<FNiagaraEmitterHandleViewModel> FNiagaraSystemViewModel::GetEmitterHandleViewModelForEmitter(const FVersionedNiagaraEmitter& InEmitter) const
{
	if (FVersionedNiagaraEmitterData* InEmitterData = InEmitter.GetEmitterData())
	{
		for (TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel : EmitterHandleViewModels)
		{
			// we compare the pointer to the emitter data here because depending on the versioning status,
			// two guids might point to the same or to different emitters 
			if (InEmitterData == EmitterHandleViewModel->GetEmitterViewModel()->GetEmitter().GetEmitterData())
			{
				return EmitterHandleViewModel;
			}
		}
	}
	return TSharedPtr<FNiagaraEmitterHandleViewModel>();
}

TSharedPtr<FNiagaraSystemScriptViewModel> FNiagaraSystemViewModel::GetSystemScriptViewModel()
{
	return SystemScriptViewModel;
}

void FNiagaraSystemViewModel::CompileSystem(bool bForce)
{
	if (EditMode == ENiagaraSystemViewModelEditMode::EmitterDuringMerge && bForce == false)
	{
		return;
	}
	SCOPE_CYCLE_COUNTER(STAT_NiagaraEditor_SystemViewModel_CompileSystem);
	check(SystemScriptViewModel.IsValid());
	SystemScriptViewModel->CompileSystem(bForce);
	bCompilePendingCompletion = true;
	InvalidateCachedCompileStatus();

	if (SystemStackViewModel)
	{
		SystemStackViewModel->RequestValidationUpdate();
		for (TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel :GetEmitterHandleViewModels())
		{
			EmitterHandleViewModel->GetEmitterStackViewModel()->RequestValidationUpdate();
		}
	}
}

ENiagaraScriptCompileStatus FNiagaraSystemViewModel::GetLatestCompileStatus() const
{
	return LatestCompileStatusCache.IsSet()
		? LatestCompileStatusCache.GetValue()
		: ENiagaraScriptCompileStatus::NCS_Unknown;
}

UNiagaraSystemEditorData& FNiagaraSystemViewModel::GetEditorData() const
{
	return *CastChecked<UNiagaraSystemEditorData>(GetSystem().GetEditorData(), ECastCheckedType::NullChecked);
}

UNiagaraComponent* FNiagaraSystemViewModel::GetPreviewComponent()
{
	return PreviewComponent;
}

TSharedPtr<ISequencer> FNiagaraSystemViewModel::GetSequencer()
{
	return Sequencer;
}

UNiagaraSystem& FNiagaraSystemViewModel::GetSystem() const
{
	checkf(System != nullptr, TEXT("System view model not initialized before use."));
	return *System;
}

bool FNiagaraSystemViewModel::GetCanModifyEmittersFromTimeline() const
{
	return bCanModifyEmittersFromTimeline;
}

/** Gets the current editing mode for this system view model. */
ENiagaraSystemViewModelEditMode FNiagaraSystemViewModel::GetEditMode() const
{
	return EditMode;
}

FName FNiagaraSystemViewModel::GetWorkflowMode() const
{
	check(OnGetWorkflowModeDelegate.IsBound());
	return OnGetWorkflowModeDelegate.Execute();
}

FOnGetWorkflowMode& FNiagaraSystemViewModel::OnGetWorkflowMode()
{
	return OnGetWorkflowModeDelegate;
}

void FNiagaraSystemViewModel::SetWorkflowMode(FName WorkflowMode)
{
	check(OnChangeWorkflowModeDelegate.IsBound());
	OnChangeWorkflowModeDelegate.Execute(WorkflowMode);
}

FNiagaraSystemViewModel::FOnChangeWorkflowMode& FNiagaraSystemViewModel::OnChangeWorkflowMode()
{
	return OnChangeWorkflowModeDelegate;
}

TSharedPtr<FNiagaraEmitterHandleViewModel> FNiagaraSystemViewModel::AddEmitterFromAssetData(const FAssetData& AssetData)
{
	UNiagaraEmitter* Emitter = Cast<UNiagaraEmitter>(AssetData.GetAsset());
	if (Emitter != nullptr)
	{
		return AddEmitter(*Emitter, Emitter->GetExposedVersion().VersionGuid);
	}

	return nullptr;
}

TSharedPtr<FNiagaraEmitterHandleViewModel> FNiagaraSystemViewModel::AddEmitter(UNiagaraEmitter& Emitter, FGuid EmitterVersion)
{
	// Reset view models before modifying the emitter handle list to prevent accessing deleted data.
	ResetEmitterHandleViewModelsAndTracks();

	// When editing an emitter asset the system is a placeholder and we don't want to make adding an emitter to it undoable.
	bool bSystemIsPlaceholder = EditMode == ENiagaraSystemViewModelEditMode::EmitterAsset;
	if (false == bSystemIsPlaceholder && false == bIsForDataProcessingOnly)
	{
		GEditor->BeginTransaction(LOCTEXT("AddEmitter", "Add emitter"));
	}

	const FGuid NewEmitterHandleId = FNiagaraEditorUtilities::AddEmitterToSystem(GetSystem(), Emitter, EmitterVersion, EditMode != ENiagaraSystemViewModelEditMode::EmitterDuringMerge);

	if (false == bSystemIsPlaceholder && false == bIsForDataProcessingOnly)
	{
		GEditor->EndTransaction();
	}

	if (GetSystem().GetNumEmitters() == 1 && EditorSettings->GetAutoPlay() && Sequencer.IsValid())
	{
		// When adding a new emitter to an empty system start playing.
		Sequencer->SetPlaybackStatus(EMovieScenePlayerStatus::Playing);
	}

	RefreshAll();

	TRange<float> SystemPlaybackRange = GetEditorData().GetPlaybackRange();
	TRange<float> EmitterPlaybackRange = GetEmitterHandleViewModelById(NewEmitterHandleId)->GetEmitterViewModel()->GetEditorData().GetPlaybackRange();
	TRange<float> NewSystemPlaybackRange = TRange<float>(
		FMath::Min(SystemPlaybackRange.GetLowerBoundValue(), EmitterPlaybackRange.GetLowerBoundValue()),
		FMath::Max(SystemPlaybackRange.GetUpperBoundValue(), EmitterPlaybackRange.GetUpperBoundValue()));

	GetEditorData().SetPlaybackRange(NewSystemPlaybackRange);

	TGuardValue<bool> UpdateGuard(bUpdatingSequencerFromEmitterDataChange, true);

	if (NiagaraSequence)
	{
		FFrameTime NewStartFrame = NewSystemPlaybackRange.GetLowerBoundValue() * NiagaraSequence->GetMovieScene()->GetTickResolution();
		int32 NewDuration = (NewSystemPlaybackRange.Size<float>() * NiagaraSequence->GetMovieScene()->GetTickResolution()).FrameNumber.Value;

		NiagaraSequence->GetMovieScene()->SetPlaybackRange(NewStartFrame.RoundToFrame(), NewDuration);
	}

	TSharedPtr<FNiagaraEmitterHandleViewModel> NewEmitterHandleViewModel = GetEmitterHandleViewModelById(NewEmitterHandleId);
	TArray<UNiagaraStackEntry*> SelectedStackEntries;
	SelectedStackEntries.Add(NewEmitterHandleViewModel->GetEmitterStackViewModel()->GetRootEntry());
	SelectionViewModel->UpdateSelectedEntries(SelectedStackEntries, TArray<UNiagaraStackEntry*>(), true);

	bForceAutoCompileOnce = true;

	return NewEmitterHandleViewModel;
}

NIAGARAEDITOR_API TSharedPtr<FNiagaraEmitterHandleViewModel> FNiagaraSystemViewModel::AddEmptyEmitter()
{
	UNiagaraEmitter* EmptyEmitter = NewObject<UNiagaraEmitter>(GetTransientPackage());
	bool bAddDefaultModulesAndRenderers = false;
	UNiagaraEmitterFactoryNew::InitializeEmitter(EmptyEmitter, bAddDefaultModulesAndRenderers);
	EmptyEmitter->TemplateSpecification = ENiagaraScriptTemplateSpecification::Template;
	FName EmptyEmitterName = FNiagaraEditorUtilities::GetUniqueObjectName<UNiagaraEmitter>(GetTransientPackage(), TEXT("Empty"));
	EmptyEmitter->SetUniqueEmitterName(EmptyEmitterName.ToString());
	EmptyEmitter->SetFlags(RF_Transactional);
	return AddEmitter(FVersionedNiagaraEmitter(EmptyEmitter, FGuid()));
}

TSharedPtr<FNiagaraEmitterHandleViewModel> FNiagaraSystemViewModel::AddEmitter(const FVersionedNiagaraEmitter& VersionedEmitter)
{
	return AddEmitter(*VersionedEmitter.Emitter, VersionedEmitter.Version);
}

void FNiagaraSystemViewModel::DuplicateEmitters(TArray<FEmitterHandleToDuplicate> EmitterHandlesToDuplicate)
{
	if (EmitterHandlesToDuplicate.Num() <= 0)
	{
		return;
	}

	// Kill all system instances and reset view models before modifying the emitter handle list to prevent accessing deleted data.
	ResetEmitterHandleViewModelsAndTracks();
	FNiagaraEditorUtilities::KillSystemInstances(GetSystem());
	const FScopedTransaction DeleteTransaction(EmitterHandlesToDuplicate.Num() == 1
		? LOCTEXT("DuplicateEmitter", "Duplicate emitter")
		: LOCTEXT("DuplicateEmitters", "Duplicate emitters"));

	TSet<FName> EmitterHandleNames;
	for (const FNiagaraEmitterHandle& EmitterHandle : GetSystem().GetEmitterHandles())
	{
		EmitterHandleNames.Add(EmitterHandle.GetName());
	}

	GetSystem().Modify();
	for (FEmitterHandleToDuplicate& EmitterHandleToDuplicate : EmitterHandlesToDuplicate)
	{
		UNiagaraSystem* SourceSystem = nullptr;
		FNiagaraEmitterHandle HandleToDuplicate;
		for (TObjectIterator<UNiagaraSystem> OtherSystemIt; OtherSystemIt; ++OtherSystemIt)
		{
			UNiagaraSystem* OtherSystem = *OtherSystemIt;
			if (OtherSystem->GetPathName() == EmitterHandleToDuplicate.SystemPath)
			{
				for (const FNiagaraEmitterHandle& EmitterHandle : OtherSystem->GetEmitterHandles())
				{
					if (EmitterHandle.GetId() == EmitterHandleToDuplicate.EmitterHandleId)
					{
						SourceSystem = OtherSystem;
						HandleToDuplicate = EmitterHandle;
						break;
					}
				}
			}

			if (SourceSystem != nullptr && HandleToDuplicate.IsValid())
			{
				break;
			}
		}

		if (SourceSystem != nullptr && HandleToDuplicate.IsValid())
		{
			const FNiagaraEmitterHandle& EmitterHandle = GetSystem().DuplicateEmitterHandle(HandleToDuplicate, FNiagaraUtilities::GetUniqueName(HandleToDuplicate.GetName(), EmitterHandleNames));
			FNiagaraScratchPadUtilities::FixExternalScratchPadScriptsForEmitter(*SourceSystem, EmitterHandle.GetInstance());
			EmitterHandleNames.Add(EmitterHandle.GetName());
			if (EmitterHandleToDuplicate.OverviewNode)
			{
				EmitterHandleToDuplicate.OverviewNode->Initialize(&GetSystem(), EmitterHandle.GetId());
			}
		}
	}

	FNiagaraStackGraphUtilities::RebuildEmitterNodes(GetSystem());
	GetEditorData().SynchronizeOverviewGraphWithSystem(GetSystem());
	RefreshAll();
	bForceAutoCompileOnce = true;
}

FGuid FNiagaraSystemViewModel::GetMessageLogGuid() const
{
	return SystemMessageLogGuidKey.IsSet() ? SystemMessageLogGuidKey.GetValue() : FGuid();
}

FGuid FNiagaraSystemViewModel::AddMessage(UNiagaraMessageData* NewMessage) const
{
	FGuid NewGuid = FGuid::NewGuid();
	UNiagaraSystem& ViewedSystem = GetSystem();
	if (EditMode == ENiagaraSystemViewModelEditMode::SystemAsset)
	{
		ViewedSystem.AddMessage(NewGuid, static_cast<UNiagaraMessageDataBase*>(NewMessage));
	}
	else if (EditMode == ENiagaraSystemViewModelEditMode::EmitterAsset)
	{
		if (ensureMsgf(EmitterHandleViewModels.Num() == 1, TEXT("There was not exactly 1 EmitterHandleViewModel for the SystemViewModel in Emitter edit mode!")))
		{
			EmitterHandleViewModels[0]->AddMessage(NewMessage, NewGuid);
		}
	}
	else
	{
		checkf(false, TEXT("New system viewmodel edit mode defined! Must implemented AddMessage() for this edit mode!"));
		return FGuid();
	}

	bPendingAssetMessagesChanged = true;
	return NewGuid;
}

void FNiagaraSystemViewModel::RemoveMessage(const FGuid& MessageKey) const
{
	UNiagaraSystem& ViewedSystem = GetSystem();
	if (EditMode == ENiagaraSystemViewModelEditMode::SystemAsset)
	{
		ViewedSystem.RemoveMessage(MessageKey);
	}
	else if (EditMode == ENiagaraSystemViewModelEditMode::EmitterAsset)
	{
		if (ensureMsgf(EmitterHandleViewModels.Num() == 1, TEXT("There was not exactly 1 EmitterHandleViewModel for the SystemViewModel in Emitter edit mode!")))
		{
			EmitterHandleViewModels[0]->RemoveMessage(MessageKey);
		}
	}
	else
	{
		checkf(false, TEXT("New system viewmodel edit mode defined! Must implemented RemoveMessage() for this edit mode!"));
	}

	bPendingAssetMessagesChanged = true;
}

FGuid FNiagaraSystemViewModel::AddStackMessage(UNiagaraMessageData* NewMessage, UNiagaraNodeFunctionCall* TargetFunctionCallNode) const
{
	FGuid NewGuid = FGuid::NewGuid();
	TargetFunctionCallNode->AddMessage(NewGuid, NewMessage);
	bPendingAssetMessagesChanged = true;
	return NewGuid;
}

NIAGARAEDITOR_API void FNiagaraSystemViewModel::RemoveStackMessage(const FGuid& MessageKey, UNiagaraNodeFunctionCall* TargetFunctionCallNode) const
{
	TargetFunctionCallNode->RemoveMessage(MessageKey);
	bPendingAssetMessagesChanged = true;
}

void FNiagaraSystemViewModel::ExecuteMessageDelegateAndRefreshMessages(FSimpleDelegate MessageDelegate)
{
	MessageDelegate.ExecuteIfBound();
	bPendingAssetMessagesChanged = true;
}

void FNiagaraSystemViewModel::DeleteEmitters(TSet<FGuid> EmitterHandleIdsToDelete)
{
	if (EmitterHandleIdsToDelete.Num() > 0)
	{
		// Reset view models before modifying the emitter handle list to prevent accessing deleted data.
		ResetEmitterHandleViewModelsAndTracks();
		FNiagaraEditorUtilities::RemoveEmittersFromSystemByEmitterHandleId(GetSystem(), EmitterHandleIdsToDelete);
		RefreshAll();
		bForceAutoCompileOnce = true;
	}
}

TArray<TSharedRef<FNiagaraEmitterHandleViewModel>> FNiagaraSystemViewModel::GetPinnedEmitterHandles()
{
	return PinnedEmitterHandles;
}

void FNiagaraSystemViewModel::SetEmitterPinnedState(TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleModel, bool bPinnedState)
{
	if (bPinnedState)
	{
		PinnedEmitterHandles.AddUnique(EmitterHandleModel);
	}
	else
	{
		PinnedEmitterHandles.Remove(EmitterHandleModel);
	}
	OnPinnedChangedDelegate.Broadcast();
}

FNiagaraSystemViewModel::FOnPinnedEmittersChanged& FNiagaraSystemViewModel::GetOnPinnedEmittersChanged()
{
	return OnPinnedChangedDelegate;
}

bool FNiagaraSystemViewModel::GetIsEmitterPinned(TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleModel)
{
	return PinnedEmitterHandles.ContainsByPredicate([=](TSharedRef<FNiagaraEmitterHandleViewModel> Model) {return Model==EmitterHandleModel;});
}


FNiagaraSystemViewModel::FOnEmitterHandleViewModelsChanged& FNiagaraSystemViewModel::OnEmitterHandleViewModelsChanged()
{
	return OnEmitterHandleViewModelsChangedDelegate;
}

FNiagaraSystemViewModel::FOnPostSequencerTimeChange& FNiagaraSystemViewModel::OnPostSequencerTimeChanged()
{
	return OnPostSequencerTimeChangeDelegate;
}

FNiagaraSystemViewModel::FOnSystemCompiled& FNiagaraSystemViewModel::OnSystemCompiled()
{
	return OnSystemCompiledDelegate;
}

void FNiagaraSystemViewModel::AddReferencedObjects(FReferenceCollector& Collector)
{
	if (PreviewComponent != nullptr)
	{
		Collector.AddReferencedObject(PreviewComponent);
	}
	if (NiagaraSequence != nullptr)
	{
		Collector.AddReferencedObject(NiagaraSequence);
	}
	if (SystemStackViewModel != nullptr)
	{
		Collector.AddReferencedObject(SystemStackViewModel);
	}
	if (EditorDocumentsViewModel != nullptr)
	{
		Collector.AddReferencedObject(EditorDocumentsViewModel);
	}
	if (SelectionViewModel != nullptr)
	{
		Collector.AddReferencedObject(SelectionViewModel);
	}
	if (ScriptScratchPadViewModel != nullptr)
	{
		Collector.AddReferencedObject(ScriptScratchPadViewModel);
	}
	if (CurveSelectionViewModel != nullptr)
	{
		Collector.AddReferencedObject(CurveSelectionViewModel);
	}
	if (ScalabilityViewModel != nullptr)
	{
		Collector.AddReferencedObject(ScalabilityViewModel);
	}
	if (UserParametersHierarchyViewModel != nullptr)
	{
		Collector.AddReferencedObject(UserParametersHierarchyViewModel);
	}
}

void FNiagaraSystemViewModel::PostUndo(bool bSuccess)
{
	// Reset system stack and emitter handle view models to prevent accessing invalid data if they were in the undo operation.
	ResetStack();
	System->InvalidateActiveCompiles();
	System->RequestCompile(false);
}

bool FNiagaraSystemViewModel::WaitingOnCompilation() const
{
	return GetSystem().HasOutstandingCompilationRequests(true);
}

void FNiagaraSystemViewModel::Tick(float DeltaTime)
{
	if (System == nullptr)
	{
		// If the system pointer is no longer valid, this system view model has been cleaned up and is invalid
		// and is about to be destroyed so don't run tick logic.
		return;
	}

	if (bCompilePendingCompletion && GetSystem().HasOutstandingCompilationRequests() == false)
	{
		bCompilePendingCompletion = false;
		OnSystemCompiled().Broadcast();
	}

	if (!SystemScriptViewModel.IsValid())
	{
		return;
	}

	TickCompileStatus();

	bool bAutoCompileThisFrame = GetDefault<UNiagaraEditorSettings>()->GetAutoCompile() && bCanAutoCompile && GetLatestCompileStatus() == ENiagaraScriptCompileStatus::NCS_Dirty;
	if ((bForceAutoCompileOnce || bAutoCompileThisFrame) && !WaitingOnCompilation())
	{
		CompileSystem(false);
		bForceAutoCompileOnce = false;
	}

	if (bResetRequestPending)
	{
		ResetSystem(ETimeResetMode::AllowResetTime, EMultiResetMode::AllowResetAllInstances, EReinitMode::ReinitializeSystem);
	}

	if (EmitterIdsRequiringSequencerTrackUpdate.Num() > 0)
	{
		UpdateSequencerTracksForEmitters(EmitterIdsRequiringSequencerTrackUpdate);
		EmitterIdsRequiringSequencerTrackUpdate.Empty();
	}

	if (SystemStackViewModel != nullptr)
	{
		SystemStackViewModel->Tick();
	}

	for (TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel : EmitterHandleViewModels)
	{
		EmitterHandleViewModel->GetEmitterStackViewModel()->Tick();
	}

	if (SelectionViewModel != nullptr)
	{
		SelectionViewModel->Tick();
	}

	if (CurveSelectionViewModel != nullptr)
	{
		CurveSelectionViewModel->Tick();
	}

	if (bPendingAssetMessagesChanged)
	{
		bPendingAssetMessagesChanged = false;
		RefreshAssetMessages();
	}
}

void FNiagaraSystemViewModel::NotifyPreSave()
{
	if (System->HasOutstandingCompilationRequests(true))
	{
		UE_LOG(LogNiagaraEditor, Log, TEXT("System %s has pending compile jobs. Waiting for that code to complete before Saving.."), *System->GetName());
		System->WaitForCompilationComplete(true);
	}

	if (bSupportCompileForEdit)
	{
		check(System->bCompileForEdit);
		System->bCompileForEdit = false;
		System->RequestCompile(false);
		System->WaitForCompilationComplete(true);
	}
}

void FNiagaraSystemViewModel::NotifyPostSave()
{
	if (bSupportCompileForEdit)
	{
		check(!System->bCompileForEdit);
		System->bCompileForEdit = true;
		System->RequestCompile(false);
		System->WaitForCompilationComplete(true);
	}
}

void FNiagaraSystemViewModel::NotifyPreClose()
{
	if (GetSystem().HasOutstandingCompilationRequests(true))
	{
		UE_LOG(LogNiagaraEditor, Log, TEXT("System %s has pending compile jobs. Waiting for that code to complete before Closing.."), *GetSystem().GetName());
		GetSystem().WaitForCompilationComplete(true);
	}
	OnPreCloseDelegate.Broadcast();
}

FNiagaraSystemViewModel::FOnPreClose& FNiagaraSystemViewModel::OnPreClose()
{
	return OnPreCloseDelegate;
}

FNiagaraSystemViewModel::FOnRequestFocusTab& FNiagaraSystemViewModel::OnRequestFocusTab()
{
	return OnRequestFocusTabDelegate;
}

void FNiagaraSystemViewModel::FocusTab(FName TabName, bool bDrawAttention)
{
	OnRequestFocusTabDelegate.Broadcast(TabName, bDrawAttention);
}

TSharedPtr<FUICommandList> FNiagaraSystemViewModel::GetToolkitCommands()
{
	return ToolkitCommands.Pin();
}

void FNiagaraSystemViewModel::SetToolkitCommands(const TSharedRef<FUICommandList>& InToolkitCommands)
{
	ToolkitCommands = InToolkitCommands;
}

const TArray<FNiagaraStackModuleData>& FNiagaraSystemViewModel::GetStackModuleDataByModuleEntry(UNiagaraStackEntry* ModuleEntry)
{
	FGuid EmitterHandleId = FGuid();
	FVersionedNiagaraEmitter Emitter;
	if (ModuleEntry->GetEmitterViewModel().IsValid())
	{
		Emitter = ModuleEntry->GetEmitterViewModel()->GetEmitter();
		TSharedPtr<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel = GetEmitterHandleViewModelForEmitter(Emitter);
		if (ensureMsgf(EmitterHandleViewModel.IsValid(), TEXT("Failed to get the emitter handle view model for emitter %s while getting stack module data."), *Emitter.Emitter->GetPathName()))
		{
			EmitterHandleId = EmitterHandleViewModel->GetId();
		}
	}

	TArray<FNiagaraStackModuleData>* StackModuleData = GuidToCachedStackModuleData.Find(EmitterHandleId);
	return StackModuleData != nullptr ? *StackModuleData : BuildAndCacheStackModuleData(EmitterHandleId, Emitter);
}

const TArray<FNiagaraStackModuleData>& FNiagaraSystemViewModel::GetStackModuleDataByEmitterHandleId(FGuid EmitterHandleId)
{
	TArray<FNiagaraStackModuleData>* StackModuleData = GuidToCachedStackModuleData.Find(EmitterHandleId);
	if (StackModuleData != nullptr)
	{
		return *StackModuleData;
	}

	FVersionedNiagaraEmitter Emitter;
	TSharedPtr<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel = GetEmitterHandleViewModelById(EmitterHandleId);
	if (ensureMsgf(EmitterHandleViewModel.IsValid(), TEXT("Failed to get the emitter handle view model for emitter handle id %s while getting stack module data."), *EmitterHandleId.ToString(EGuidFormats::DigitsWithHyphens)))
	{
		Emitter = EmitterHandleViewModel->GetEmitterViewModel()->GetEmitter();
	}

	return BuildAndCacheStackModuleData(EmitterHandleId, Emitter);
}

const TArray<FNiagaraStackModuleData>& FNiagaraSystemViewModel::BuildAndCacheStackModuleData(FGuid EmitterHandleId, const FVersionedNiagaraEmitter& Emitter)
{
	TArray<FNiagaraStackModuleData>& StackModuleData = GuidToCachedStackModuleData.Add(EmitterHandleId);
	TArray<UNiagaraScript*> OrderedScripts;
	GetOrderedScriptsForEmitter(Emitter, OrderedScripts);
	for(UNiagaraScript* OrderedScript : OrderedScripts)
	{
		BuildStackModuleData(OrderedScript, EmitterHandleId, StackModuleData);
	}
	return StackModuleData;
}

TArray<TSharedRef<FNiagaraEmitterHandleViewModel>> FNiagaraSystemViewModel::GetSelectedEmitterHandleViewModels()
{
	TArray<TSharedRef<FNiagaraEmitterHandleViewModel>> SelectedEmitterHandleViewModels;
	const TArray<FGuid>& SelectedEmitterHandleIds = GetSelectionViewModel()->GetSelectedEmitterHandleIds();

	for (const TSharedRef<FNiagaraEmitterHandleViewModel>& EmitterHandleViewModel : EmitterHandleViewModels)
	{
		if (SelectedEmitterHandleIds.Contains(EmitterHandleViewModel->GetId()))
		{
			SelectedEmitterHandleViewModels.Add(EmitterHandleViewModel);
		}
	}
	return SelectedEmitterHandleViewModels;
}

UNiagaraEditorParametersAdapter* FNiagaraSystemViewModel::GetEditorOnlyParametersAdapter() const
{
	if (EditMode == ENiagaraSystemViewModelEditMode::SystemAsset)
	{
		return CastChecked<UNiagaraEditorParametersAdapter>(System->GetEditorParameters());
	}
	/** else EditMode == ENiagaraSystemViewModelEditMode::EmitterAsset */
	FVersionedNiagaraEmitterData* EmitterData = GetEmitterHandleViewModels()[0]->GetEmitterHandle()->GetEmitterData();
	return CastChecked<UNiagaraEditorParametersAdapter>(EmitterData->GetEditorParameters());
}

void FNiagaraSystemViewModel::GetOrderedScriptsForEmitterHandleId(FGuid EmitterHandleId, TArray<UNiagaraScript*>& OutScripts)
{
	FVersionedNiagaraEmitter Emitter;
	TSharedPtr<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel = GetEmitterHandleViewModelById(EmitterHandleId);
	if (EmitterHandleViewModel.IsValid())
	{
		Emitter = EmitterHandleViewModel->GetEmitterViewModel()->GetEmitter();
	}
	GetOrderedScriptsForEmitter(Emitter, OutScripts);
}

void FNiagaraSystemViewModel::GetOrderedScriptsForEmitter(const FVersionedNiagaraEmitter& Emitter, TArray<UNiagaraScript*>& OutScripts)
{
	OutScripts.Add(System->GetSystemSpawnScript());
	OutScripts.Add(System->GetSystemUpdateScript());
	FVersionedNiagaraEmitterData* EmitterData = Emitter.GetEmitterData();
	if (EmitterData != nullptr)
	{
		OutScripts.Add(EmitterData->EmitterSpawnScriptProps.Script);
		OutScripts.Add(EmitterData->EmitterUpdateScriptProps.Script);
		OutScripts.Add(EmitterData->SpawnScriptProps.Script);
		OutScripts.Add(EmitterData->UpdateScriptProps.Script);
		for (UNiagaraSimulationStageBase* SimulationStage : EmitterData->GetSimulationStages())
		{
			if (SimulationStage->bEnabled)
			{
				OutScripts.Add(SimulationStage->Script);
			}
		}
	}
}

UNiagaraStackViewModel* FNiagaraSystemViewModel::GetSystemStackViewModel()
{
	return SystemStackViewModel;
}

UNiagaraSystemSelectionViewModel* FNiagaraSystemViewModel::GetSelectionViewModel()
{
	return SelectionViewModel;
}

UNiagaraScratchPadViewModel* FNiagaraSystemViewModel::GetScriptScratchPadViewModel()
{
	return ScriptScratchPadViewModel;
}

UNiagaraCurveSelectionViewModel* FNiagaraSystemViewModel::GetCurveSelectionViewModel()
{
	return CurveSelectionViewModel;
}

UNiagaraSystemScalabilityViewModel* FNiagaraSystemViewModel::GetScalabilityViewModel()
{
	return ScalabilityViewModel;
}

UNiagaraUserParametersHierarchyViewModel* FNiagaraSystemViewModel::GetUserParametersHierarchyViewModel()
{
	return UserParametersHierarchyViewModel;
}

TArray<float> FNiagaraSystemViewModel::OnGetPlaybackSpeeds() const
{
	return GetDefault<UNiagaraEditorSettings>()->GetPlaybackSpeeds();
}

TStatId FNiagaraSystemViewModel::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FNiagaraSystemViewModel, STATGROUP_Tickables);
}

TSharedRef<FNiagaraPlaceholderDataInterfaceManager> FNiagaraSystemViewModel::GetPlaceholderDataInterfaceManager()
{
	return PlaceholderDataInterfaceManager.ToSharedRef();
}

bool FNiagaraSystemViewModel::GetIsForDataProcessingOnly() const
{
	return bIsForDataProcessingOnly;
}

bool FNiagaraSystemViewModel::RenameParameter(const FNiagaraVariable TargetParameter, const FName NewName, ENiagaraGetGraphParameterReferencesMode RenameScopeMode)
{
	FScopedTransaction RenameParameterTransaction(LOCTEXT("RenameParameter", "Rename parameter"));

	System->Modify();

	bool bExposedParametersRename = false;
	bool bEditorOnlyParametersRename = false;
	bool bAssignmentNodeRename = false;

	// Rename the parameter if it's in the parameter store for user parameters.
	if (System->GetExposedParameters().IndexOf(TargetParameter) != INDEX_NONE)
	{
		FNiagaraParameterStore* ExposedParametersStore = &System->GetExposedParameters();
		TArray<FNiagaraVariable> OwningParameters;
		ExposedParametersStore->GetParameters(OwningParameters);
		if (OwningParameters.ContainsByPredicate([NewName](const FNiagaraVariable& Variable) { return Variable.GetName() == NewName; }))
		{
			// If the parameter store already has a parameter with this name, remove the old parameter to prevent collisions.
			ExposedParametersStore->RemoveParameter(TargetParameter);
		}
		else
		{
			// Otherwise it's safe to rename.
			GetEditorData().RenameUserScriptVariable(TargetParameter, NewName);
			ExposedParametersStore->RenameParameter(TargetParameter, NewName);
		}
		
		bExposedParametersRename = true;
	}

	// Look for set parameters nodes or linked inputs which reference this parameter and rename if so.
	for (const FNiagaraGraphParameterReference& ParameterReference : GetGraphParameterReferences(TargetParameter, RenameScopeMode))
	{
		UNiagaraNode* ReferenceNode = Cast<UNiagaraNode>(ParameterReference.Value);
		if (ReferenceNode != nullptr)
		{
			UNiagaraNodeAssignment* OwningAssignmentNode = ReferenceNode->GetTypedOuter<UNiagaraNodeAssignment>();
			if (OwningAssignmentNode != nullptr)
			{
				// If this is owned by a set variables node and it's not locked, update the assignment target on the assignment node.
				bAssignmentNodeRename |= FNiagaraStackGraphUtilities::TryRenameAssignmentTarget(*OwningAssignmentNode, TargetParameter, NewName);
			}
			else
			{
				// Otherwise if the reference node is a get node it's for a linked input so we can just update pin name.
				UNiagaraNodeParameterMapGet* ReferenceGetNode = Cast<UNiagaraNodeParameterMapGet>(ReferenceNode);
				if (ReferenceGetNode != nullptr)
				{
					if (ReferenceGetNode->Pins.ContainsByPredicate([&ParameterReference](UEdGraphPin* Pin) { return Pin->PersistentGuid == ParameterReference.Key; }))
					{
						ReferenceGetNode->GetNiagaraGraph()->RenameParameter(TargetParameter, NewName, true);
					}
				}
			}
		}
	}

	// Rename the parameter if it is owned directly as an editor only parameter.
	UNiagaraEditorParametersAdapter* EditorParametersAdapter = GetEditorOnlyParametersAdapter();
	if (UNiagaraScriptVariable** ScriptVariablePtr = EditorParametersAdapter->GetParameters().FindByPredicate([&TargetParameter](const UNiagaraScriptVariable* ScriptVariable) { return ScriptVariable->Variable.GetName() == TargetParameter.GetName(); }))
	{
		EditorParametersAdapter->Modify();
		UNiagaraScriptVariable* ScriptVariable = *ScriptVariablePtr;
		ScriptVariable->Modify();
		ScriptVariable->Variable.SetName(NewName);
		ScriptVariable->UpdateChangeId();
		bEditorOnlyParametersRename = true;

		// Check if the rename will give the same name and type as an existing parameter definition, and if so, link to the definition automatically.
		FNiagaraParameterDefinitionsUtilities::TrySubscribeScriptVarToDefinitionByName(ScriptVariable, this);
	}

	// Handle renaming any renderer properties that might match.
	if (bExposedParametersRename | bEditorOnlyParametersRename | bAssignmentNodeRename)
	{
		if (TargetParameter.IsInNameSpace(FNiagaraConstants::ParticleAttributeNamespaceString) || TargetParameter.IsInNameSpace(FNiagaraConstants::EmitterNamespaceString))
		{
			for (const TSharedRef<FNiagaraEmitterHandleViewModel>& EmitterHandleViewModel : GetSelectedEmitterHandleViewModels())
			{
				FNiagaraEmitterHandle* EmitterHandle = EmitterHandleViewModel->GetEmitterHandle();
				EmitterHandle->GetInstance().Emitter->HandleVariableRenamed(TargetParameter, FNiagaraVariableBase(TargetParameter.GetType(), NewName), true, EmitterHandle->GetInstance().Version);
			}
		}
		else
		{
			System->HandleVariableRenamed(TargetParameter, FNiagaraVariableBase(TargetParameter.GetType(), NewName), true);
		}

		return true;
	}

	return false;
}

TArray<UNiagaraGraph*> FNiagaraSystemViewModel::GetAllGraphs()
{
	TArray<UNiagaraGraph*> OutGraphs;

	// Helper lambda to null check graph weak object ptrs before adding them to the retval array.
	auto AddToOutGraphsChecked = [&OutGraphs](const TWeakObjectPtr<UNiagaraGraph>& WeakGraph) {
		UNiagaraGraph* Graph = WeakGraph.Get();
		if (Graph == nullptr)
		{
			ensureMsgf(false, TEXT("Encountered null graph when gathering editable graphs for system parameter panel viewmodel!"));
			return;
		}
		OutGraphs.Add(Graph);
	};

	if (GetEditMode() == ENiagaraSystemViewModelEditMode::SystemAsset)
	{
		TArray<UNiagaraGraph*> Graphs;
		AddToOutGraphsChecked(GetSystemScriptViewModel()->GetGraphViewModel()->GetGraph());

		for (const TSharedRef<FNiagaraEmitterHandleViewModel>& EmitterHandleViewModel : EmitterHandleViewModels)
		{
			FNiagaraEmitterHandle* EmitterHandle = EmitterHandleViewModel->GetEmitterHandle();
			if (EmitterHandle == nullptr)
			{
				continue;
			}
			UNiagaraGraph* Graph = Cast<UNiagaraScriptSource>(EmitterHandle->GetEmitterData()->GraphSource)->NodeGraph;
			if (Graph)
			{
				OutGraphs.Add(Graph);
			}
		}
	}
	else
	{
		FNiagaraEmitterHandle* EmitterHandle = GetEmitterHandleViewModels()[0]->GetEmitterHandle();
		if (EmitterHandle != nullptr)
		{
			OutGraphs.Add(Cast<UNiagaraScriptSource>(EmitterHandle->GetEmitterData()->GraphSource)->NodeGraph);
		}
	}

	TArray<UNiagaraGraph*> EdGraphs = GetDocumentViewModel()->GetEditableGraphsForActiveScriptDocument();
	for (UNiagaraGraph* Graph : EdGraphs)
	{
		OutGraphs.AddUnique(Graph);
	}
	return OutGraphs;
}

TArray<UNiagaraGraph*> FNiagaraSystemViewModel::GetSelectedGraphs()
{
	TArray<UNiagaraGraph*> SelectedGraphs;

	// Helper lambda to null check graph weak object ptrs before adding them to the retval array.
	auto AddToSelectedGraphsChecked = [&SelectedGraphs](const TWeakObjectPtr<UNiagaraGraph>& WeakGraph) {
		UNiagaraGraph* Graph = WeakGraph.Get();
		if (Graph == nullptr)
		{
			ensureMsgf(false, TEXT("Encountered null graph when gathering editable graphs for system parameter panel viewmodel!"));
			return;
		}
		SelectedGraphs.Add(Graph);
	};

	if (GetEditMode() == ENiagaraSystemViewModelEditMode::SystemAsset)
	{
		for (const TWeakObjectPtr<UNiagaraGraph>& WeakGraph : SystemGraphSelectionViewModel->GetSelectedEmitterScriptGraphs())
		{
			AddToSelectedGraphsChecked(WeakGraph);
		}
		AddToSelectedGraphsChecked(GetSystemScriptViewModel()->GetGraphViewModel()->GetGraph());
	}
	else
	{
		SelectedGraphs.Add(Cast<UNiagaraScriptSource>(GetEmitterHandleViewModels()[0]->GetEmitterHandle()->GetEmitterData()->GraphSource)->NodeGraph);
	}

	TArray<UNiagaraGraph*> EdGraphs = GetDocumentViewModel()->GetEditableGraphsForActiveScriptDocument();
	for (UNiagaraGraph* Graph : EdGraphs)
	{
		SelectedGraphs.AddUnique(Graph);
	}
	return SelectedGraphs;
}

TArray<FNiagaraGraphParameterReference> FNiagaraSystemViewModel::GetGraphParameterReferences(const FNiagaraVariable& Parameter, ENiagaraGetGraphParameterReferencesMode Mode)
{
	TArray<UNiagaraGraph*> TargetGraphs;
	if (Mode == ENiagaraGetGraphParameterReferencesMode::AllGraphs)
	{
		TargetGraphs = GetAllGraphs();
	}
	else if (Mode == ENiagaraGetGraphParameterReferencesMode::SelectedGraphs)
	{
		TargetGraphs = GetSelectedGraphs();
	}
	else
	{
		ensureMsgf(false, TEXT("Encountered unexpected ENiagaraGetGraphParameterReferencesMode value!"));
		return TArray<FNiagaraGraphParameterReference>();
	}

	// -For each selected graph perform a parameter map history traversal and collect all graph parameter references associated with the target FNiagaraParameterPanelItem.
	TArray<FNiagaraGraphParameterReference> GraphParameterReferences;
	for (const UNiagaraGraph* Graph : TargetGraphs)
	{
		TArray<UNiagaraNodeOutput*> OutputNodes;
		Graph->GetNodesOfClass<UNiagaraNodeOutput>(OutputNodes);
		for (UNiagaraNodeOutput* OutputNode : OutputNodes)
		{
			UNiagaraNode* NodeToTraverse = OutputNode;
			if (OutputNode->GetUsage() == ENiagaraScriptUsage::SystemSpawnScript || OutputNode->GetUsage() == ENiagaraScriptUsage::SystemUpdateScript)
			{
				// Traverse past the emitter nodes, otherwise the system scripts will pick up all of the emitter and particle script parameters.
				UEdGraphPin* InputPin = FNiagaraStackGraphUtilities::GetParameterMapInputPin(*NodeToTraverse);
				while (NodeToTraverse != nullptr && InputPin != nullptr && InputPin->LinkedTo.Num() == 1 &&
					(NodeToTraverse->IsA<UNiagaraNodeOutput>() || NodeToTraverse->IsA<UNiagaraNodeEmitter>()))
				{
					NodeToTraverse = Cast<UNiagaraNode>(InputPin->LinkedTo[0]->GetOwningNode());
					InputPin = NodeToTraverse != nullptr ? FNiagaraStackGraphUtilities::GetParameterMapInputPin(*NodeToTraverse) : nullptr;
				}
			}

			if (NodeToTraverse == nullptr)
			{
				continue;
			}

			bool bIgnoreDisabled = true;
			FNiagaraParameterMapHistoryBuilder Builder;
			FVersionedNiagaraEmitter GraphOwningEmitter = Graph->GetOwningEmitter();
			FCompileConstantResolver ConstantResolver = GraphOwningEmitter.Emitter != nullptr
				? FCompileConstantResolver(GraphOwningEmitter, ENiagaraScriptUsage::Function)
				: FCompileConstantResolver();

			Builder.SetIgnoreDisabled(bIgnoreDisabled);
			Builder.ConstantResolver = ConstantResolver;
			FName StageName;
			ENiagaraScriptUsage StageUsage = OutputNode->GetUsage();
			if (StageUsage == ENiagaraScriptUsage::ParticleSimulationStageScript && GraphOwningEmitter.Emitter)
			{
				UNiagaraSimulationStageBase* Base = GraphOwningEmitter.GetEmitterData()->GetSimulationStageById(OutputNode->GetUsageId());
				if (Base)
				{
					StageName = Base->GetStackContextReplacementName();
				}
			}
			Builder.BeginUsage(StageUsage, StageName);
			NodeToTraverse->BuildParameterMapHistory(Builder, true, false);
			Builder.EndUsage();

			if (Builder.Histories.Num() != 1)
			{
				// We should only have traversed one emitter (have not visited more than one NiagaraNodeEmitter.)
				ensureMsgf(false, TEXT("Encountered more than one parameter map history when collecting graph parameter reference collections for system parameter panel view model!"));
			}
			if (Builder.Histories.Num() == 0)
			{
				continue;
			}

			const TArray<FName>& CustomIterationSourceNamespaces = Builder.Histories[0].IterationNamespaceOverridesEncountered;
			for (int32 VariableIndex = 0; VariableIndex < Builder.Histories[0].Variables.Num(); VariableIndex++)
			{
				if (Parameter == Builder.Histories[0].Variables[VariableIndex])
				{
					for (const FNiagaraParameterMapHistory::FReadHistory& ReadHistory : Builder.Histories[0].PerVariableReadHistory[VariableIndex])
					{
						if (ReadHistory.ReadPin.Pin->GetOwningNode() != nullptr)
						{
							GraphParameterReferences.Add(FNiagaraGraphParameterReference(ReadHistory.ReadPin.Pin->PersistentGuid, ReadHistory.ReadPin.Pin->GetOwningNode()));
						}
					}

					for (const FModuleScopedPin& Write : Builder.Histories[0].PerVariableWriteHistory[VariableIndex])
					{
						if (Write.Pin->GetOwningNode() != nullptr)
						{
							GraphParameterReferences.Add(FNiagaraGraphParameterReference(Write.Pin->PersistentGuid, Write.Pin->GetOwningNode()));
						}
					}
				}
			}
		}
	}
	return GraphParameterReferences;
}

void FNiagaraSystemViewModel::SendLastCompileMessageJobs() const
{
	if (SystemMessageLogGuidKey.IsSet() == false)
	{
		return;
	}

	struct FNiagaraScriptAndOwningScriptNameString
	{
		FNiagaraScriptAndOwningScriptNameString(const UNiagaraScript* InScript, const FString& InOwningScriptNameString)
			: Script(InScript)
			, OwningScriptNameString(InOwningScriptNameString)
		{
		}

		const UNiagaraScript* Script;
		const FString OwningScriptNameString;
	};

	FNiagaraMessageManager* MessageManager = FNiagaraMessageManager::Get();
	int32 ErrorCount = 0;
	int32 WarningCount = 0;

	TArray<FNiagaraScriptAndOwningScriptNameString> ScriptsToGetCompileEventsFrom;
	ScriptsToGetCompileEventsFrom.Add(FNiagaraScriptAndOwningScriptNameString(GetSystem().GetSystemSpawnScript(), GetSystem().GetName()));
	ScriptsToGetCompileEventsFrom.Add(FNiagaraScriptAndOwningScriptNameString(GetSystem().GetSystemUpdateScript(), GetSystem().GetName()));
	const TArray<FNiagaraEmitterHandle> EmitterHandles = GetSystem().GetEmitterHandles();
	for (const FNiagaraEmitterHandle& Handle : EmitterHandles)
	{
		FVersionedNiagaraEmitter EmitterInSystem = Handle.GetInstance();
		TArray<UNiagaraScript*> EmitterScripts;
		EmitterInSystem.GetEmitterData()->GetScripts(EmitterScripts, false);
		for (UNiagaraScript* EmitterScript : EmitterScripts)
		{
			ScriptsToGetCompileEventsFrom.Add(FNiagaraScriptAndOwningScriptNameString(EmitterScript, EmitterInSystem.Emitter->GetUniqueEmitterName()));
		}
	}

	// Clear out existing compile event messages.
	MessageManager->ClearAssetMessagesForTopic(SystemMessageLogGuidKey.GetValue(), FNiagaraMessageTopics::CompilerTopicName);

	// Make new messages and messages jobs from the compile.
	// Iterate from back to front to avoid reordering the events when they are queued.
	for (int i = ScriptsToGetCompileEventsFrom.Num()-1; i >=0; --i)
	{
		const FNiagaraScriptAndOwningScriptNameString& ScriptInfo = ScriptsToGetCompileEventsFrom[i];
		const TArray<FNiagaraCompileEvent>& CurrentCompileEvents = ScriptInfo.Script->GetVMExecutableData().LastCompileEvents;
		for (int j = CurrentCompileEvents.Num() - 1; j >= 0; --j)
		{
			const FNiagaraCompileEvent& CompileEvent = CurrentCompileEvents[j];
			if (CompileEvent.Severity == FNiagaraCompileEventSeverity::Error)
			{
				ErrorCount++;
			}
			else if (CompileEvent.Severity == FNiagaraCompileEventSeverity::Warning)
			{
				WarningCount++;
			}

			MessageManager->AddMessageJob(MakeUnique<FNiagaraMessageJobCompileEvent>(CompileEvent, MakeWeakObjectPtr(const_cast<UNiagaraScript*>(ScriptInfo.Script)), FGuid(), ScriptInfo.OwningScriptNameString), SystemMessageLogGuidKey.GetValue());
		}

		// Check if there are any GPU compile errors and if so push them.
		const FNiagaraShaderScript* ShaderScript = ScriptInfo.Script->GetRenderThreadScript();
		if (ShaderScript != nullptr && ShaderScript->IsCompilationFinished() &&
			ScriptInfo.Script->Usage == ENiagaraScriptUsage::ParticleGPUComputeScript)
		{
			const TArray<FString>& GPUCompileErrors = ShaderScript->GetCompileErrors();
			for (const FString& String : GPUCompileErrors)
			{
				FNiagaraCompileEventSeverity Severity = FNiagaraCompileEventSeverity::Warning;
				if (String.Contains(TEXT("err0r")))
				{
					Severity = FNiagaraCompileEventSeverity::Error;
					ErrorCount++;
				}
				else
				{
					WarningCount++;
				}
				FNiagaraCompileEvent CompileEvent = FNiagaraCompileEvent(Severity, String);
				MessageManager->AddMessageJob(MakeUnique<FNiagaraMessageJobCompileEvent>(CompileEvent, MakeWeakObjectPtr(const_cast<UNiagaraScript*>(ScriptInfo.Script)), FGuid(),
					ScriptInfo.OwningScriptNameString), SystemMessageLogGuidKey.GetValue());
			}
		}
	}

	const FText PostCompileSummaryText = FNiagaraMessageUtilities::MakePostCompileSummaryText(FText::FromString("System"), GetLatestCompileStatus(), WarningCount, ErrorCount);
	MessageManager->AddMessage(MakeShared<FNiagaraMessageText>(PostCompileSummaryText, EMessageSeverity::Info, FNiagaraMessageTopics::CompilerTopicName), SystemMessageLogGuidKey.GetValue());
}

void FNiagaraSystemViewModel::InvalidateCachedCompileStatus()
{
	LatestCompileStatusCache.Reset();
	ScriptsToCheckForStatus.Empty();
	ScriptCompileStatuses.Empty();
}

void FNiagaraSystemViewModel::TickCompileStatus()
{
	// Checking the compile status is expensive which is why it's updated on tick, and multiple scripts
	// are time sliced across multiple frames.
	if (WaitingOnCompilation())
	{
		// When compiling the compile status is always unknown.
		InvalidateCachedCompileStatus();
	}
	else
	{
		if (LatestCompileStatusCache.IsSet() == false)
		{
			bool bCompileStatusRefreshed = false;
			if (ScriptsToCheckForStatus.Num() > 0)
			{
				// If there are still scripts to check for status do that...
				UNiagaraScript* ScriptToCheck = ScriptsToCheckForStatus.Last();
				ScriptsToCheckForStatus.RemoveAt(ScriptsToCheckForStatus.Num() - 1);

				if (ScriptToCheck->AreScriptAndSourceSynchronized() == false)
				{
					// If any script is not synchronized the entire system is considered dirty and we no longer need
					// to check other script statuses.
					LatestCompileStatusCache = ENiagaraScriptCompileStatus::NCS_Dirty;
					bCompileStatusRefreshed = true;
					ScriptsToCheckForStatus.Empty();
					ScriptCompileStatuses.Empty();
				}
				else
				{
					ENiagaraScriptCompileStatus LastScriptCompileStatus = ScriptToCheck->GetLastCompileStatus();
					if (LastScriptCompileStatus == ENiagaraScriptCompileStatus::NCS_Unknown ||
						LastScriptCompileStatus == ENiagaraScriptCompileStatus::NCS_Dirty ||
						LastScriptCompileStatus == ENiagaraScriptCompileStatus::NCS_BeingCreated)
					{
						// If the script doesn't have a vaild last compile status assume that it's dirty and by extension
						// so is the system and we can ignore the other statuses.
						LatestCompileStatusCache = ENiagaraScriptCompileStatus::NCS_Dirty;
						bCompileStatusRefreshed = true;
						ScriptsToCheckForStatus.Empty();
						ScriptCompileStatuses.Empty();
					}
					else
					{
						// Otherwise save it for processing once all scripts are done.
						ScriptCompileStatuses.Add(LastScriptCompileStatus);
					}
				}
			}
			else
			{
				// Otherwise check for status results.
				if (ScriptCompileStatuses.Num() > 0)
				{
					// If there are statues from scripts figure out the system status.
					if (ScriptCompileStatuses.Contains(ENiagaraScriptCompileStatus::NCS_Error))
					{
						// If any individual script has an error the system is considered to have an error.
						LatestCompileStatusCache = ENiagaraScriptCompileStatus::NCS_Error;
						bCompileStatusRefreshed = true;
					}
					else if (ScriptCompileStatuses.Contains(ENiagaraScriptCompileStatus::NCS_UpToDateWithWarnings) ||
						ScriptCompileStatuses.Contains(ENiagaraScriptCompileStatus::NCS_ComputeUpToDateWithWarnings))
					{
						// If there were no errors and any individual script has a warning the system is considered to have a warning.
						LatestCompileStatusCache = ENiagaraScriptCompileStatus::NCS_UpToDateWithWarnings;
						bCompileStatusRefreshed = true;
					}
					else
					{
						LatestCompileStatusCache = ENiagaraScriptCompileStatus::NCS_UpToDate;
						bCompileStatusRefreshed = true;
					}
					ScriptCompileStatuses.Empty();
				}
				else
				{
					// If there is no current status, no scripts to check, and no status results then
					// collect up the scripts to check deferred.
					ScriptsToCheckForStatus.Add(System->GetSystemSpawnScript());
					ScriptsToCheckForStatus.Add(System->GetSystemUpdateScript());
					for (const FNiagaraEmitterHandle& EmitterHandle : System->GetEmitterHandles())
					{
						if (EmitterHandle.GetIsEnabled())
						{
							EmitterHandle.GetEmitterData()->GetScripts(ScriptsToCheckForStatus, true);
						}
					}
				}
			}

			if (bCompileStatusRefreshed)
			{
				SendLastCompileMessageJobs();
				RefreshStackViewModels();
			}
		}
	}
}

void FNiagaraSystemViewModel::SetupPreviewComponentAndInstance()
{
	if (bCanSimulate)
	{
		PreviewComponent = NewObject<UNiagaraComponent>(GetTransientPackage(), NAME_None, RF_Transient);
		PreviewComponent->CastShadow = 1;
		PreviewComponent->bCastDynamicShadow = 1;
		PreviewComponent->SetAllowScalability(false);
		PreviewComponent->SetAsset(System);
		PreviewComponent->SetForceSolo(true);
		PreviewComponent->SetAgeUpdateMode(ENiagaraAgeUpdateMode::DesiredAge);
		PreviewComponent->SetCanRenderWhileSeeking(false);
		PreviewComponent->Activate(true);

		FTransform OwnerTransform = GetEditorData().GetOwnerTransform();
		PreviewComponent->SetRelativeTransform(OwnerTransform);

		PreviewComponent->OnSystemInstanceChanged().AddRaw(this, &FNiagaraSystemViewModel::PreviewComponentSystemInstanceChanged);
		PreviewComponentSystemInstanceChanged();
	}
}

void FNiagaraSystemViewModel::RefreshAll()
{
	if (GetSystem().HasOutstandingCompilationRequests() == false)
	{
		// Data changes which require full refreshes can cause stability problems when resetting the accompanying system, especially
		// when used in conjunction with warm up.  This is a temporary fix for these crashes.
		CompileSystem(false);
	}
	ResetSystem(ETimeResetMode::AllowResetTime, EMultiResetMode::AllowResetAllInstances, EReinitMode::ReinitializeSystem);
	RefreshEmitterHandleViewModels();
	RefreshSequencerTracks();
	InvalidateCachedCompileStatus();
	ScriptScratchPadViewModel->RefreshScriptViewModels();
	CurveSelectionViewModel->Refresh();
	SystemStackViewModel->InitializeWithViewModels(this->AsShared(), TSharedPtr<FNiagaraEmitterHandleViewModel>(), FNiagaraStackViewModelOptions(true, false));
	SelectionViewModel->Refresh();
	bPendingAssetMessagesChanged = true;
}

void FNiagaraSystemViewModel::ResetStack()
{
	SystemStackViewModel->Reset();
	ResetEmitterHandleViewModelsAndTracks();
	RefreshAll();
	GetDefault<UEdGraphSchema_NiagaraSystemOverview>()->ForceVisualizationCacheClear();
}

void FNiagaraSystemViewModel::NotifyDataObjectChanged(TArray<UObject*> ChangedObjects, ENiagaraDataObjectChange ChangeType)
{
	if (ChangedObjects.Num() == 1 && ChangedObjects[0]->IsA<UNiagaraSystem>())
	{
		// we do nothing on system changes here, because they will trigger a compile and reset on their own, depending on the changed property
		return;
	}

	bool bRefreshCurveSelectionViewModel = false;
	if(ChangedObjects.Num() != 0)
	{
		for(UObject* ChangedObject : ChangedObjects)
		{
			UNiagaraDataInterface* ChangedDataInterface = Cast<UNiagaraDataInterface>(ChangedObject);
			if (ChangedDataInterface)
			{
				UpdateCompiledDataInterfaces(ChangedDataInterface);
			}


			if(ChangedObject->IsA<UNiagaraDataInterfaceCurveBase>() && ChangeType != ENiagaraDataObjectChange::Changed)
			{
				bRefreshCurveSelectionViewModel = true;
			}

			if(UNiagaraEmitter* Emitter = Cast<UNiagaraEmitter>(ChangedObject))
			{
				for (TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel : EmitterHandleViewModels)
				{
					if (EmitterHandleViewModel->GetEmitterViewModel()->GetEmitter().Emitter == Emitter)
					{
						UNiagaraStackViewModel* StackViewModel = EmitterHandleViewModel->GetEmitterStackViewModel();
						if(ensure(StackViewModel))
						{
							StackViewModel->Refresh();
						}
					}
				}
			}
		}
	}
	else
	{
		bRefreshCurveSelectionViewModel = true;
	}

	if (bRefreshCurveSelectionViewModel)
	{
		CurveSelectionViewModel->Refresh();
	}

	ResetSystem(ETimeResetMode::AllowResetTime, EMultiResetMode::AllowResetAllInstances, EReinitMode::ReinitializeSystem);
}

void FNiagaraSystemViewModel::IsolateEmitters(TArray<FGuid> EmitterHandlesIdsToIsolate)
{
	for (TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandle : EmitterHandleViewModels)
	{
		EmitterHandle->GetEmitterHandle()->SetIsolated(false);
	}

	bool bAnyEmitterIsolated = false;
	for (TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandle : EmitterHandleViewModels)
	{
		if (EmitterHandlesIdsToIsolate.Contains(EmitterHandle->GetId()))
		{
			bAnyEmitterIsolated = true;
			EmitterHandle->GetEmitterHandle()->SetIsolated(true);
		}
	}

	GetSystem().SetIsolateEnabled(bAnyEmitterIsolated);
}

void FNiagaraSystemViewModel::DisableEmitters(TArray<FGuid> EmitterHandlesIdsToDisable)
{
	FScopedTransaction Transaction(LOCTEXT("DisableSelectedEmitters", "Disabled selected emitters"));
	GetSystem().Modify();
	
	bool bAnyEmittersDisabled = false;
	
	for (TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandle : EmitterHandleViewModels)
	{
		if(EmitterHandlesIdsToDisable.Contains(EmitterHandle->GetId()))
		{
			const bool bDisabled = EmitterHandle->GetEmitterHandle()->SetIsEnabled(false, GetSystem(), false);
			bAnyEmittersDisabled = bAnyEmittersDisabled || bDisabled;
		}
	}

	if(bAnyEmittersDisabled)
	{
		GetSystem().RequestCompile(false);
	}
	else
	{
		Transaction.Cancel();		
	}
}

void FNiagaraSystemViewModel::ToggleEmitterIsolation(TSharedRef<FNiagaraEmitterHandleViewModel> InEmitterHandle)
{
	InEmitterHandle->GetEmitterHandle()->SetIsolated(!InEmitterHandle->GetEmitterHandle()->IsIsolated());

	bool bAnyEmitterIsolated = false;
	for (TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandle : EmitterHandleViewModels)
	{
		if (EmitterHandle->GetEmitterHandle()->IsIsolated())
		{
			bAnyEmitterIsolated = true;
			break;
		}
	}

	GetSystem().SetIsolateEnabled(bAnyEmitterIsolated);
}

void FNiagaraSystemViewModel::ResetEmitterHandleViewModelsAndTracks()
{
	for (TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel : EmitterHandleViewModels)
	{
		EmitterHandleViewModel->Reset();
	}

	if (NiagaraSequence)
	{
		TGuardValue<bool> UpdateGuard(bResetingSequencerTracks, true);
		TArray<UMovieSceneTrack*> MainTracks = NiagaraSequence->GetMovieScene()->GetMasterTracks();
		for (UMovieSceneTrack* MainTrack : MainTracks)
		{
			if (MainTrack != nullptr)
			{
				NiagaraSequence->GetMovieScene()->RemoveMasterTrack(*MainTrack);
			}
		}
	}
}

void FNiagaraSystemViewModel::RefreshEmitterHandleViewModels()
{
	TArray<TSharedRef<FNiagaraEmitterHandleViewModel>> OldViewModels = EmitterHandleViewModels;
	EmitterHandleViewModels.Empty();
	GuidToCachedStackModuleData.Empty();

	// Map existing view models to the real instances that now exist. Reuse if we can. Create a new one if we cannot.
	TArray<FGuid> ValidEmitterHandleIds;
	int32 i;
	for (i = 0; i < GetSystem().GetNumEmitters(); ++i)
	{
		FNiagaraEmitterHandle* EmitterHandle = &GetSystem().GetEmitterHandle(i);
		TSharedPtr<FNiagaraEmitterInstance, ESPMode::ThreadSafe> Simulation = SystemInstance ? SystemInstance->GetSimulationForHandle(*EmitterHandle) : nullptr;
		ValidEmitterHandleIds.Add(EmitterHandle->GetId());

		TSharedPtr<FNiagaraEmitterHandleViewModel> ViewModel;
		bool bAdd = OldViewModels.Num() <= i;
		if (bAdd)
		{
			ViewModel = MakeShared<FNiagaraEmitterHandleViewModel>(bIsForDataProcessingOnly);
			ViewModel->OnPropertyChanged().AddRaw(this, &FNiagaraSystemViewModel::EmitterHandlePropertyChanged, EmitterHandle->GetId());
			ViewModel->OnNameChanged().AddRaw(this, &FNiagaraSystemViewModel::EmitterHandleNameChanged);
			ViewModel->GetEmitterViewModel()->OnPropertyChanged().AddRaw(this, &FNiagaraSystemViewModel::EmitterPropertyChanged);
			ViewModel->GetEmitterViewModel()->OnScriptCompiled().AddRaw(this, &FNiagaraSystemViewModel::ScriptCompiled);
			ViewModel->GetEmitterViewModel()->OnScriptGraphChanged().AddRaw(this, &FNiagaraSystemViewModel::EmitterScriptGraphChanged, EmitterHandle->GetId());
			ViewModel->GetEmitterViewModel()->OnScriptParameterStoreChanged().AddRaw(this, &FNiagaraSystemViewModel::EmitterParameterStoreChanged);
			ViewModel->GetEmitterStackViewModel()->OnStructureChanged().AddRaw(this, &FNiagaraSystemViewModel::StackViewModelStructureChanged);
		}
		else
		{
			ViewModel = OldViewModels[i];
		}

		EmitterHandleViewModels.Add(ViewModel.ToSharedRef());
		ViewModel->Initialize(this->AsShared(), i, Simulation);
	}

	check(EmitterHandleViewModels.Num() == GetSystem().GetNumEmitters());

	// Clear out any old view models that may still be left around.
	for (; i < OldViewModels.Num(); i++)
	{
		TSharedRef<FNiagaraEmitterHandleViewModel> ViewModel = OldViewModels[i];
		ViewModel->OnPropertyChanged().RemoveAll(this);
		ViewModel->OnNameChanged().RemoveAll(this);
		ViewModel->GetEmitterViewModel()->OnPropertyChanged().RemoveAll(this);
		ViewModel->GetEmitterViewModel()->OnScriptCompiled().RemoveAll(this);
		ViewModel->GetEmitterViewModel()->OnScriptGraphChanged().RemoveAll(this);
		ViewModel->GetEmitterViewModel()->OnScriptParameterStoreChanged().RemoveAll(this);
		ViewModel->GetEmitterStackViewModel()->OnStructureChanged().RemoveAll(this);
		ViewModel->Reset();
	}

	bool bAnyEmitterIsolated = false;
	for (TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandle : EmitterHandleViewModels)
	{
		if (EmitterHandle->GetIsIsolated())
		{
			bAnyEmitterIsolated = true;
		}
	}
	GetSystem().SetIsolateEnabled(bAnyEmitterIsolated);
	OnEmitterHandleViewModelsChangedDelegate.Broadcast();
}

void PopulateChildMovieSceneFoldersFromNiagaraFolders(const UNiagaraSystemEditorFolder* NiagaraFolder, UMovieSceneFolder* MovieSceneFolder, const TMap<FGuid, UMovieSceneNiagaraEmitterTrack*>& EmitterHandleIdToTrackMap)
{
	for (const UNiagaraSystemEditorFolder* ChildNiagaraFolder : NiagaraFolder->GetChildFolders())
	{
		UMovieSceneFolder* MatchingMovieSceneFolder = nullptr;
		for (UMovieSceneFolder* ChildMovieSceneFolder : MovieSceneFolder->GetChildFolders())
		{
			if (ChildMovieSceneFolder->GetFolderName() == ChildNiagaraFolder->GetFolderName())
			{
				MatchingMovieSceneFolder = ChildMovieSceneFolder;
			}
		}

		if (MatchingMovieSceneFolder == nullptr)
		{
			MatchingMovieSceneFolder = NewObject<UMovieSceneFolder>(MovieSceneFolder, ChildNiagaraFolder->GetFolderName(), RF_Transactional);
			MatchingMovieSceneFolder->SetFolderName(ChildNiagaraFolder->GetFolderName());
			MovieSceneFolder->AddChildFolder(MatchingMovieSceneFolder);
		}

		PopulateChildMovieSceneFoldersFromNiagaraFolders(ChildNiagaraFolder, MatchingMovieSceneFolder, EmitterHandleIdToTrackMap);
	}

	for (const FGuid& ChildEmitterHandleId : NiagaraFolder->GetChildEmitterHandleIds())
	{
		UMovieSceneNiagaraEmitterTrack* const* TrackPtr = EmitterHandleIdToTrackMap.Find(ChildEmitterHandleId);
		if (TrackPtr != nullptr && MovieSceneFolder->GetChildMasterTracks().Contains(*TrackPtr) == false)
		{
			MovieSceneFolder->AddChildMasterTrack(*TrackPtr);
		}
	}
}

void FNiagaraSystemViewModel::RefreshSequencerTracks()
{
	TGuardValue<bool> UpdateGuard(bUpdatingSequencerFromEmitterDataChange, true);

	if (Sequencer.IsValid())
	{
		TArray<UMovieSceneTrack*> MainTracks = NiagaraSequence->GetMovieScene()->GetMasterTracks();
		for (UMovieSceneTrack* MainTrack : MainTracks)
		{
			if (MainTrack != nullptr)
			{
				NiagaraSequence->GetMovieScene()->RemoveMasterTrack(*MainTrack);
			}
		}

		TMap<FGuid, UMovieSceneNiagaraEmitterTrack*> EmitterHandleIdToTrackMap;
		for (TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel : EmitterHandleViewModels)
		{
			UMovieSceneNiagaraEmitterTrack* EmitterTrack = Cast<UMovieSceneNiagaraEmitterTrack>(NiagaraSequence->GetMovieScene()->AddMasterTrack(UMovieSceneNiagaraEmitterTrack::StaticClass()));
			EmitterTrack->Initialize(*this, EmitterHandleViewModel, NiagaraSequence->GetMovieScene()->GetTickResolution());
			EmitterHandleIdToTrackMap.Add(EmitterHandleViewModel->GetId(), EmitterTrack);
		}

		UMovieScene* MovieScene = NiagaraSequence->GetMovieScene();
		MovieScene->EmptyRootFolders();

		const UNiagaraSystemEditorData& SystemEditorData = GetEditorData();
		UNiagaraSystemEditorFolder& RootFolder = SystemEditorData.GetRootFolder();
		for (const UNiagaraSystemEditorFolder* RootChildFolder : RootFolder.GetChildFolders())
		{
			UMovieSceneFolder* MovieSceneRootFolder = NewObject<UMovieSceneFolder>(MovieScene, RootChildFolder->GetFolderName(), RF_Transactional);
			MovieSceneRootFolder->SetFolderName(RootChildFolder->GetFolderName());
			MovieScene->AddRootFolder(MovieSceneRootFolder);
			PopulateChildMovieSceneFoldersFromNiagaraFolders(RootChildFolder, MovieSceneRootFolder, EmitterHandleIdToTrackMap);
		}
		Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
	}

	// Since we just rebuilt all of the sequencer tracks, these updates don't need to be done.
	EmitterIdsRequiringSequencerTrackUpdate.Empty();
}

void FNiagaraSystemViewModel::UpdateSequencerTracksForEmitters(const TArray<FGuid>& EmitterIdsRequiringUpdate)
{
	if (Sequencer.IsValid())
	{
		TGuardValue<bool> UpdateGuard(bUpdatingSequencerFromEmitterDataChange, true);
		for (UMovieSceneTrack* Track : NiagaraSequence->GetMovieScene()->GetMasterTracks())
		{
			UMovieSceneNiagaraEmitterTrack* EmitterTrack = CastChecked<UMovieSceneNiagaraEmitterTrack>(Track);
			if (EmitterIdsRequiringUpdate.Contains(EmitterTrack->GetEmitterHandleViewModel()->GetId()))
			{
				EmitterTrack->UpdateTrackFromEmitterGraphChange(NiagaraSequence->GetMovieScene()->GetTickResolution());
			}
		}
		Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
	}
}

UMovieSceneNiagaraEmitterTrack* FNiagaraSystemViewModel::GetTrackForHandleViewModel(TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel)
{
	if (NiagaraSequence)
	{
		for (UMovieSceneTrack* Track : NiagaraSequence->GetMovieScene()->GetMasterTracks())
		{
			UMovieSceneNiagaraEmitterTrack* EmitterTrack = CastChecked<UMovieSceneNiagaraEmitterTrack>(Track);
			if (EmitterTrack->GetEmitterHandleViewModel() == EmitterHandleViewModel)
			{
				return EmitterTrack;
			}
		}
	}
	return nullptr;
}

void FNiagaraSystemViewModel::SetupSequencer()
{
	if (EditMode == ENiagaraSystemViewModelEditMode::EmitterDuringMerge || !FSlateApplicationBase::IsInitialized())
	{
		// we don't need a sequencer when merging emitters or if we're in a commandlet with no slate application
		return;
	}
	NiagaraSequence = NewObject<UNiagaraSequence>(GetTransientPackage());
	UMovieScene* MovieScene = NewObject<UMovieScene>(NiagaraSequence, FName("Niagara System MovieScene"), RF_Transactional);
	MovieScene->SetDisplayRate(FFrameRate(240, 1));

	NiagaraSequence->Initialize(this, MovieScene);

	FFrameTime StartTime = GetEditorData().GetPlaybackRange().GetLowerBoundValue() * MovieScene->GetTickResolution();
	int32      Duration  = (GetEditorData().GetPlaybackRange().Size<float>() * MovieScene->GetTickResolution()).FrameNumber.Value;

	MovieScene->SetPlaybackRange(StartTime.RoundToFrame(), Duration);

	FMovieSceneEditorData& EditorData = NiagaraSequence->GetMovieScene()->GetEditorData();
	float ViewTimeOffset = .1f;
	EditorData.WorkStart = GetEditorData().GetPlaybackRange().GetLowerBoundValue() - ViewTimeOffset;
	EditorData.WorkEnd = GetEditorData().GetPlaybackRange().GetUpperBoundValue() + ViewTimeOffset;
	EditorData.ViewStart = EditorData.WorkStart;
	EditorData.ViewEnd = EditorData.WorkEnd;

	FSequencerViewParams ViewParams(TEXT("NiagaraSequencerSettings"));
	{
		ViewParams.UniqueName = "NiagaraSequenceEditor";
		ViewParams.OnGetAddMenuContent = OnGetSequencerAddMenuContent;
		ViewParams.OnGetPlaybackSpeeds = ISequencer::FOnGetPlaybackSpeeds::CreateRaw(this, &FNiagaraSystemViewModel::OnGetPlaybackSpeeds);
	}

	FSequencerInitParams SequencerInitParams;
	{
		SequencerInitParams.ViewParams = ViewParams;
		SequencerInitParams.RootSequence = NiagaraSequence;
		SequencerInitParams.bEditWithinLevelEditor = false;
		SequencerInitParams.ToolkitHost = nullptr;
	}

	UNiagaraEditorSettings::OnSettingsChanged().AddRaw(this, &FNiagaraSystemViewModel::SnapToNextSpeed);
	
	ISequencerModule &SequencerModule = FModuleManager::LoadModuleChecked< ISequencerModule >("Sequencer");
	Sequencer = SequencerModule.CreateSequencer(SequencerInitParams);
	Sequencer->OnMovieSceneDataChanged().AddRaw(this, &FNiagaraSystemViewModel::SequencerDataChanged);
	Sequencer->OnGlobalTimeChanged().AddRaw(this, &FNiagaraSystemViewModel::SequencerTimeChanged);
	Sequencer->GetSelectionChangedTracks().AddRaw(this, &FNiagaraSystemViewModel::SequencerTrackSelectionChanged);
	Sequencer->GetSelectionChangedSections().AddRaw(this, &FNiagaraSystemViewModel::SequencerSectionSelectionChanged);
	Sequencer->SetPlaybackStatus(GetSystem().GetNumEmitters() > 0 && EditorSettings->GetAutoPlay()
		? EMovieScenePlayerStatus::Playing
		: EMovieScenePlayerStatus::Stopped);
}

void FNiagaraSystemViewModel::SnapToNextSpeed(const FString& PropertyName, const UNiagaraEditorSettings* Settings)
{
	// we update the speed in any case
	if (Sequencer.IsValid())
	{
		Sequencer->SnapToClosestPlaybackSpeed();
	}
}

void FNiagaraSystemViewModel::IsolateSelectedEmitters()
{
	const TArray<FGuid> EmitterHandles = GetSelectionViewModel()->GetSelectedEmitterHandleIds();
	IsolateEmitters(EmitterHandles);
}

void FNiagaraSystemViewModel::DisableSelectedEmitters()
{
	const TArray<FGuid> EmitterHandles = GetSelectionViewModel()->GetSelectedEmitterHandleIds();
	DisableEmitters(EmitterHandles);
}

void FNiagaraSystemViewModel::ResetSystem()
{
	ResetSystem(ETimeResetMode::AllowResetTime, EMultiResetMode::ResetThisInstance, EReinitMode::ResetSystem);
}

void FNiagaraSystemViewModel::ResetSystem(ETimeResetMode TimeResetMode, EMultiResetMode MultiResetMode, EReinitMode ReinitMode)
{
	bool bResetAge = false;
	if (Sequencer.IsValid())
	{
		bResetAge = TimeResetMode == ETimeResetMode::AllowResetTime && (Sequencer->GetPlaybackStatus() == EMovieScenePlayerStatus::Playing || Sequencer->GetPlaybackStatus() == EMovieScenePlayerStatus::Stopped || EditorSettings->GetResimulateOnChangeWhilePaused() == false);
		if (bResetAge)
		{
			TGuardValue<bool> Guard(bSettingSequencerTimeDirectly, true);
			if (Sequencer->GetPlaybackStatus() == EMovieScenePlayerStatus::Playing)
			{
				Sequencer->SetPlaybackStatus(EMovieScenePlayerStatus::Paused);
				Sequencer->SetGlobalTime(0);
				Sequencer->SetPlaybackStatus(EMovieScenePlayerStatus::Playing);
			}
			else
			{
				Sequencer->SetGlobalTime(0);
			}
		}
	}

	FNiagaraSystemUpdateContext UpdateContext;

	UpdateContext.GetPostWork().BindLambda(
		[ReinitMode, bResetAge](UNiagaraComponent* Component)
		{
			if (ReinitMode == EReinitMode::ResetSystem && bResetAge  && Component->GetAgeUpdateMode() == ENiagaraAgeUpdateMode::DesiredAge)
			{
				Component->SetDesiredAge(0);
			}
		}
	);

	if (MultiResetMode == EMultiResetMode::ResetThisInstance)
	{
		// Resetting the single preview instance is only valid when the change causing the reset hasn't invalidated any cached simulation data.
		// Examples of these changes are bounds changes or preview changes such as resetting on loop or changing the preview quality level.
		if (PreviewComponent != nullptr)
		{
			UpdateContext.AddSoloComponent(PreviewComponent, ReinitMode == EReinitMode::ReinitializeSystem);
		}
	}
	else
	{
		UNiagaraSystem* NiagaraSystem = &GetSystem();
		UpdateContext.Add(NiagaraSystem, ReinitMode == EReinitMode::ReinitializeSystem);
	}

	UpdateContext.CommitUpdate();

	if (EditMode == ENiagaraSystemViewModelEditMode::EmitterAsset && MultiResetMode == EMultiResetMode::AllowResetAllInstances && EditorSettings->GetResetDependentSystemsWhenEditingEmitters())
	{
		FNiagaraEditorUtilities::ResetSystemsThatReferenceSystemViewModel(*this);
	}

	FEditorSupportDelegates::RedrawAllViewports.Broadcast();
	bResetRequestPending = false;
}

void FNiagaraSystemViewModel::RequestResetSystem()
{
	bResetRequestPending = true;
}

void GetCompiledScriptsAndEmitterNameFromInputNode(UNiagaraNode& StackNode, UNiagaraSystem& OwningSystem, TArray<UNiagaraScript*>& OutCompiledScripts, FString& OutEmitterName)
{
	OutEmitterName = FString();
	UNiagaraNodeOutput* OutputNode = FNiagaraStackGraphUtilities::GetEmitterOutputNodeForStackNode(StackNode);
	if (OutputNode != nullptr)
	{
		if (OutputNode->GetUsage() == ENiagaraScriptUsage::SystemSpawnScript)
		{
			OutCompiledScripts.Add(OwningSystem.GetSystemSpawnScript());
		}
		else if (OutputNode->GetUsage() == ENiagaraScriptUsage::SystemUpdateScript)
		{
			OutCompiledScripts.Add(OwningSystem.GetSystemUpdateScript());
		}
		else
		{
			const FNiagaraEmitterHandle* OwningEmitterHandle = OwningSystem.GetEmitterHandles().FindByPredicate([&StackNode](const FNiagaraEmitterHandle& EmitterHandle)
			{
				return CastChecked<UNiagaraScriptSource>(EmitterHandle.GetEmitterData()->GraphSource)->NodeGraph == StackNode.GetNiagaraGraph();
			});

			if(OwningEmitterHandle != nullptr)
			{
				OutEmitterName = OwningEmitterHandle->GetInstance().Emitter->GetUniqueEmitterName();
				switch (OutputNode->GetUsage())
				{
				case ENiagaraScriptUsage::EmitterSpawnScript:
					OutCompiledScripts.Add(OwningSystem.GetSystemSpawnScript());
					break;
				case ENiagaraScriptUsage::EmitterUpdateScript:
					OutCompiledScripts.Add(OwningSystem.GetSystemUpdateScript());
					break;
				case ENiagaraScriptUsage::ParticleSpawnScript:
				case ENiagaraScriptUsage::ParticleUpdateScript:
				case ENiagaraScriptUsage::ParticleEventScript:
				case ENiagaraScriptUsage::ParticleSimulationStageScript:
					//TODO extract emitterdata into one var
					OutCompiledScripts.Add(OwningEmitterHandle->GetEmitterData()->GetScript(OutputNode->GetUsage(), OutputNode->GetUsageId()));
					if (OwningEmitterHandle->GetEmitterData()->SimTarget == ENiagaraSimTarget::GPUComputeSim)
					{
						OutCompiledScripts.Add(OwningEmitterHandle->GetEmitterData()->GetScript(ENiagaraScriptUsage::ParticleGPUComputeScript, FGuid()));
					}
					break;
				}
			}
		}
	}
}

void UpdateCompiledDataInterfacesForScript(UNiagaraScript& TargetScript, FName TargetDataInterfaceName, UNiagaraDataInterface& SourceDataInterface)
{
	for (FNiagaraScriptDataInterfaceInfo& DataInterfaceInfo : TargetScript.GetCachedDefaultDataInterfaces())
	{
		if (DataInterfaceInfo.Name == TargetDataInterfaceName)
		{
			SourceDataInterface.CopyTo(DataInterfaceInfo.DataInterface);
			break;
		}
	}
}

void FNiagaraSystemViewModel::UpdateCompiledDataInterfaces(UNiagaraDataInterface* ChangedDataInterface)
{
	UNiagaraNodeInput* OuterInputNode = ChangedDataInterface->GetTypedOuter<UNiagaraNodeInput>();
	if (OuterInputNode != nullptr)
	{
		// If the data interface's owning node has been removed from it's graph then it's not valid so early out here.
		bool bIsValidInputNode = OuterInputNode->GetGraph()->Nodes.Contains(OuterInputNode);
		if (bIsValidInputNode == false)
		{
			return;
		}

		// If the data interface was owned by an input node, then we need to try to update the compiled version.
		TArray<UNiagaraScript*> CompiledScripts;
		FString EmitterName;
		GetCompiledScriptsAndEmitterNameFromInputNode(*OuterInputNode, GetSystem(), CompiledScripts, EmitterName);
		if (ensureMsgf(CompiledScripts.Num() > 0, TEXT("Could not find compiled scripts for data interface input node.")))
		{
			for(UNiagaraScript* CompiledScript : CompiledScripts)
			{
				bool bIsParameterMapDataInterface = false;
				FName DataInterfaceName = FHlslNiagaraTranslator::GetDataInterfaceName(OuterInputNode->Input.GetName(), EmitterName, bIsParameterMapDataInterface);
				UpdateCompiledDataInterfacesForScript(*CompiledScript, DataInterfaceName, *ChangedDataInterface);
			}
		}
	}
	else
	{
		// If the data interface wasn't owned by a script, try to find it in the exposed parameter data interfaces.
		const FNiagaraVariableBase* FoundExposedDataInterface = GetSystem().GetExposedParameters().FindVariable(ChangedDataInterface);
		if (FoundExposedDataInterface != nullptr)
		{
			GetSystem().GetExposedParameters().OnInterfaceChange();
		}
	}
}

void FNiagaraSystemViewModel::EmitterHandlePropertyChanged(FGuid OwningEmitterHandleId)
{
	// When the emitter handle changes, refresh the System scripts emitter nodes and the sequencer tracks just in case the
	// property that changed was the handles emitter.
	if (bUpdatingEmittersFromSequencerDataChange == false && NiagaraSequence)
	{
		TGuardValue<bool> UpdateGuard(bUpdatingSequencerFromEmitterDataChange, true);
		for (UMovieSceneTrack* Track : NiagaraSequence->GetMovieScene()->GetMasterTracks())
		{
			UMovieSceneNiagaraEmitterTrack* EmitterTrack = CastChecked<UMovieSceneNiagaraEmitterTrack>(Track);
			if (EmitterTrack->GetEmitterHandleViewModel()->GetId() == OwningEmitterHandleId)
			{
				EmitterTrack->UpdateTrackFromEmitterGraphChange(NiagaraSequence->GetMovieScene()->GetTickResolution());
			}
		}
		Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
	}

	// Refresh the overview nodes and the emitter stacks just in case the emitter enabled state changed.
	GetEditorData().SynchronizeOverviewGraphWithSystem(GetSystem());
	TSharedPtr<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel = GetEmitterHandleViewModelById(OwningEmitterHandleId);
	if (EmitterHandleViewModel.IsValid())
	{
		EmitterHandleViewModel->GetEmitterStackViewModel()->GetRootEntry()->RefreshChildren();
	}

	ResetSystem(ETimeResetMode::AllowResetTime, EMultiResetMode::ResetThisInstance, EReinitMode::ReinitializeSystem);
}

void FNiagaraSystemViewModel::EmitterHandleNameChanged()
{
	GetDefault<UEdGraphSchema_NiagaraSystemOverview>()->ForceVisualizationCacheClear();
	CompileSystem(false);
	CurveSelectionViewModel->Refresh();
}

void FNiagaraSystemViewModel::EmitterPropertyChanged()
{
	ResetSystem(ETimeResetMode::AllowResetTime, EMultiResetMode::AllowResetAllInstances, EReinitMode::ReinitializeSystem);
}

void FNiagaraSystemViewModel::ScriptCompiled(UNiagaraScript*, const FGuid&)
{
	bCompilePendingCompletion = true;
	//ReInitializeSystemInstances();
}

void FNiagaraSystemViewModel::SystemParameterStoreChanged(const FNiagaraParameterStore& ChangedParameterStore, const UNiagaraScript* OwningScript)
{
	UpdateSimulationFromParameterChange();
}

void FNiagaraSystemViewModel::EmitterScriptGraphChanged(const FEdGraphEditAction& InAction, const UNiagaraScript& OwningScript, FGuid OwningEmitterHandleId)
{
	if (bUpdatingEmittersFromSequencerDataChange == false)
	{
		EmitterIdsRequiringSequencerTrackUpdate.AddUnique(OwningEmitterHandleId);
	}
	// Remove from cache on graph change
	GuidToCachedStackModuleData.Remove(OwningEmitterHandleId);
	InvalidateCachedCompileStatus();

	// Do a deferred refresh when responding to graph changes since we may be mid change and the graph could be invalid.
	CurveSelectionViewModel->RefreshDeferred();

	bPendingAssetMessagesChanged = true;
}

void FNiagaraSystemViewModel::SystemScriptGraphChanged(const FEdGraphEditAction& InAction)
{
	GuidToCachedStackModuleData.Empty();
	InvalidateCachedCompileStatus();
	// Do a deferred refresh when responding to graph changes since we may be mid change and the graph could be invalid.
	CurveSelectionViewModel->RefreshDeferred();
	bPendingAssetMessagesChanged = true;
}

void FNiagaraSystemViewModel::EmitterParameterStoreChanged(const FNiagaraParameterStore& ChangedParameterStore, const UNiagaraScript& OwningScript)
{
	if (bUpdatingEmittersFromSequencerDataChange == false)
	{
		if (EmitterIdsRequiringSequencerTrackUpdate.Num() > 0)
		{
			UpdateSequencerTracksForEmitters(EmitterIdsRequiringSequencerTrackUpdate);
		}
		else if (Sequencer.IsValid())
		{
			TGuardValue<bool> UpdateGuard(bUpdatingSequencerFromEmitterDataChange, true);
			for (UMovieSceneTrack* Track : NiagaraSequence->GetMovieScene()->GetMasterTracks())
			{
				UMovieSceneNiagaraEmitterTrack* EmitterTrack = CastChecked<UMovieSceneNiagaraEmitterTrack>(Track);
				EmitterTrack->UpdateTrackFromEmitterParameterChange(NiagaraSequence->GetMovieScene()->GetTickResolution());
			}
			
			Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);
		}
	}
	UpdateSimulationFromParameterChange();
}

void FNiagaraSystemViewModel::UpdateSimulationFromParameterChange()
{
	if (EditorSettings->GetResetSimulationOnChange())
	{
		/* Calling RequestResetSystem here avoids reentrancy into ResetSystem() when we edit the system parameter store on
		** UNiagaraComponent::Activate() as we always call PrepareRapidIterationParameters().  */
		RequestResetSystem();
	}
	else
	{
		if (Sequencer && Sequencer->GetPlaybackStatus() == EMovieScenePlayerStatus::Stopped)
		{
			// TODO: Update the view when paused and reset on change is turned off.
		}
	}
}

void PopulateNiagaraFoldersFromMovieSceneFolders(TArrayView<UMovieSceneFolder* const> MovieSceneFolders, const TArray<UMovieSceneTrack*>& MovieSceneTracks, UNiagaraSystemEditorFolder* ParentFolder)
{
	TArray<FName> ValidFolderNames;
	for (UMovieSceneFolder* MovieSceneFolder : MovieSceneFolders)
	{
		ValidFolderNames.Add(MovieSceneFolder->GetFolderName());
		UNiagaraSystemEditorFolder* MatchingNiagaraFolder = nullptr;
		for (UNiagaraSystemEditorFolder* ChildNiagaraFolder : ParentFolder->GetChildFolders())
		{
			CA_ASSUME(ChildNiagaraFolder != nullptr);
			if (ChildNiagaraFolder->GetFolderName() == MovieSceneFolder->GetFolderName())
			{
				MatchingNiagaraFolder = ChildNiagaraFolder;
				break;
			}
		}

		if (MatchingNiagaraFolder == nullptr)
		{
			MatchingNiagaraFolder = NewObject<UNiagaraSystemEditorFolder>(ParentFolder, MovieSceneFolder->GetFolderName(), RF_Transactional);
			MatchingNiagaraFolder->SetFolderName(MovieSceneFolder->GetFolderName());
			ParentFolder->AddChildFolder(MatchingNiagaraFolder);
		}

		PopulateNiagaraFoldersFromMovieSceneFolders(MovieSceneFolder->GetChildFolders(), MovieSceneFolder->GetChildMasterTracks(), MatchingNiagaraFolder);
	}

	TArray<UNiagaraSystemEditorFolder*> ChildNiagaraFolders = ParentFolder->GetChildFolders();
	for (UNiagaraSystemEditorFolder* ChildNiagaraFolder : ChildNiagaraFolders)
	{
		if (ValidFolderNames.Contains(ChildNiagaraFolder->GetFolderName()) == false)
		{
			ParentFolder->RemoveChildFolder(ChildNiagaraFolder);
		}
	}

	TArray<FGuid> ValidEmitterHandleIds;
	for (UMovieSceneTrack* MovieSceneTrack : MovieSceneTracks)
	{
		UMovieSceneNiagaraEmitterTrack* NiagaraEmitterTrack = Cast<UMovieSceneNiagaraEmitterTrack>(MovieSceneTrack);
		if (NiagaraEmitterTrack != nullptr)
		{
			FGuid EmitterHandleId = NiagaraEmitterTrack->GetEmitterHandleViewModel()->GetId();
			ValidEmitterHandleIds.Add(EmitterHandleId);
			if (ParentFolder->GetChildEmitterHandleIds().Contains(EmitterHandleId) == false)
			{
				ParentFolder->AddChildEmitterHandleId(EmitterHandleId);
			}
		}
	}

	TArray<FGuid> ChildEmitterHandleIds = ParentFolder->GetChildEmitterHandleIds();
	for (FGuid& ChildEmitterHandleId : ChildEmitterHandleIds)
	{
		if (ValidEmitterHandleIds.Contains(ChildEmitterHandleId) == false)
		{
			ParentFolder->RemoveChildEmitterHandleId(ChildEmitterHandleId);
		}
	}
}

void FNiagaraSystemViewModel::SequencerDataChanged(EMovieSceneDataChangeType DataChangeType)
{
	if (bUpdatingSequencerFromEmitterDataChange == false && GIsTransacting == false && NiagaraSequence)
	{
		TGuardValue<bool> UpdateGuard(bUpdatingEmittersFromSequencerDataChange, true);

		GetEditorData().Modify();
		TRange<FFrameNumber> FramePlaybackRange = NiagaraSequence->GetMovieScene()->GetPlaybackRange();
		float StartTimeSeconds = NiagaraSequence->GetMovieScene()->GetTickResolution().AsSeconds(FramePlaybackRange.GetLowerBoundValue());
		float EndTimeSeconds = NiagaraSequence->GetMovieScene()->GetTickResolution().AsSeconds(FramePlaybackRange.GetUpperBoundValue());
		GetEditorData().SetPlaybackRange(TRange<float>(StartTimeSeconds, EndTimeSeconds));

		TSet<FGuid> VaildTrackEmitterHandleIds;
		TArray<FEmitterHandleToDuplicate> EmittersToDuplicate;
		TArray<TTuple<TSharedPtr<FNiagaraEmitterHandleViewModel>, FName>> EmitterHandlesToRename;

		bool bRefreshAllTracks = false;
		for (UMovieSceneTrack* Track : NiagaraSequence->GetMovieScene()->GetMasterTracks())
		{
			UMovieSceneNiagaraEmitterTrack* EmitterTrack = CastChecked<UMovieSceneNiagaraEmitterTrack>(Track);
			if (EmitterTrack->GetEmitterHandleViewModel().IsValid())
			{
				if (EmitterTrack->GetAllSections().Num() == 0 && EmitterTrack->GetSectionsWereModified())
				{
					// If there are no sections and the section collection was modified, the section was deleted so skip adding
					// it's handle to the valid collection so that it's emitter is deleted.
					continue;
				}

				VaildTrackEmitterHandleIds.Add(EmitterTrack->GetEmitterHandleViewModel()->GetId());
				EmitterTrack->UpdateEmitterHandleFromTrackChange(NiagaraSequence->GetMovieScene()->GetTickResolution());
				EmitterTrack->GetEmitterHandleViewModel()->GetEmitterViewModel()->GetOrCreateEditorData().Modify();
				EmitterTrack->GetEmitterHandleViewModel()->GetEmitterViewModel()->GetOrCreateEditorData().SetPlaybackRange(GetEditorData().GetPlaybackRange());
				if (EmitterTrack->GetDisplayName().ToString() != EmitterTrack->GetEmitterHandleViewModel()->GetNameText().ToString())
				{
					EmitterHandlesToRename.Add(TTuple<TSharedPtr<FNiagaraEmitterHandleViewModel>, FName>(EmitterTrack->GetEmitterHandleViewModel(), *EmitterTrack->GetDisplayName().ToString()));
				}

				if (EmitterTrack->GetAllSections().Num() > 1)
				{
					// If a section was duplicated, force a refresh to remove the duplicated section since that's not currently supported.
					// TODO: Detect duplicated sections and create a duplicate emitter with the correct values.
					bRefreshAllTracks = true;
				}
			}
			else
			{
				if (EmitterTrack->GetEmitterHandleId().IsValid())
				{
					// The emitter handle is invalid, but the track has a valid Id, most probably because of a copy/paste event
					FEmitterHandleToDuplicate EmitterHandleToDuplicate;
					EmitterHandleToDuplicate.SystemPath = EmitterTrack->GetSystemPath();
					EmitterHandleToDuplicate.EmitterHandleId = EmitterTrack->GetEmitterHandleId();
					EmittersToDuplicate.AddUnique(EmitterHandleToDuplicate);
				}
			}
		}

		bRefreshAllTracks |= EmitterHandlesToRename.Num() > 0;

		for (TTuple<TSharedPtr<FNiagaraEmitterHandleViewModel>, FName>& EmitterHandletoRename : EmitterHandlesToRename)
		{
			EmitterHandletoRename.Get<0>()->SetName(EmitterHandletoRename.Get<1>());
		}

		TSet<FGuid> AllEmitterHandleIds;
		for (TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel : EmitterHandleViewModels)
		{
			AllEmitterHandleIds.Add(EmitterHandleViewModel->GetId());
		}

		TSet<FGuid> RemovedEmitterHandleIds = AllEmitterHandleIds.Difference(VaildTrackEmitterHandleIds);
		if (RemovedEmitterHandleIds.Num() > 0)
		{
			if (bCanModifyEmittersFromTimeline)
			{
				DeleteEmitters(RemovedEmitterHandleIds);
			}
			else
			{
				bRefreshAllTracks = true;
			}
		}

		if (EmittersToDuplicate.Num() > 0)
		{
			if (bCanModifyEmittersFromTimeline)
			{
				DuplicateEmitters(EmittersToDuplicate);
			}
			else
			{
				bRefreshAllTracks = true;
			}
		}

		TArray<UMovieSceneTrack*> RootTracks;
		TArrayView<UMovieSceneFolder* const> RootFolders = NiagaraSequence->GetMovieScene()->GetRootFolders();
		if (RootFolders.Num() != 0 || GetEditorData().GetRootFolder().GetChildFolders().Num() != 0)
		{
			PopulateNiagaraFoldersFromMovieSceneFolders(RootFolders, RootTracks, &GetEditorData().GetRootFolder());
		}

		if (bRefreshAllTracks)
		{
			RefreshSequencerTracks();
		}
	}
}

void FNiagaraSystemViewModel::SequencerTimeChanged()
{
	if (!PreviewComponent || !SystemInstance || !SystemInstance->GetAreDataInterfacesInitialized() || !Sequencer.IsValid())
	{
		return;
	}
	EMovieScenePlayerStatus::Type CurrentStatus = Sequencer->GetPlaybackStatus();
	float CurrentSequencerTime = Sequencer->GetGlobalTime().AsSeconds();
	if (SystemInstance != nullptr)
	{
		// Avoid reentrancy if we're setting the time directly.
		if (bSettingSequencerTimeDirectly == false && CurrentSequencerTime != PreviousSequencerTime)
		{
			// Skip the first update after going from stopped to playing or from playing to stopped because snapping in sequencer may have made
			// the time reverse by a small amount, and sending that update to the System will reset it unnecessarily.
			bool bStartedPlaying = CurrentStatus == EMovieScenePlayerStatus::Playing && PreviousSequencerStatus != EMovieScenePlayerStatus::Playing;
			bool bEndedPlaying = CurrentStatus != EMovieScenePlayerStatus::Playing && PreviousSequencerStatus == EMovieScenePlayerStatus::Playing;

			bool bUpdateDesiredAge = bStartedPlaying == false;
			bool bResetSystemInstance = SystemInstance->IsComplete();

			if (bUpdateDesiredAge)
			{
				if (CurrentStatus == EMovieScenePlayerStatus::Playing)
				{
					PreviewComponent->SetDesiredAge(FMath::Max(CurrentSequencerTime, 0.0f));
				}
				else
				{
					PreviewComponent->SeekToDesiredAge(FMath::Max(CurrentSequencerTime, 0.0f));
				}
			}

			if (bResetSystemInstance)
			{
				// We don't want to reset the current time if we're scrubbing.
				if (CurrentStatus == EMovieScenePlayerStatus::Playing)
				{
					ResetSystem(ETimeResetMode::AllowResetTime, EMultiResetMode::ResetThisInstance, EReinitMode::ReinitializeSystem);
				}
				else
				{
					ResetSystem(ETimeResetMode::KeepCurrentTime, EMultiResetMode::ResetThisInstance, EReinitMode::ResetSystem);
				}
			}
		}
	}

	PreviousSequencerStatus = CurrentStatus;
	PreviousSequencerTime = CurrentSequencerTime;

	OnPostSequencerTimeChangeDelegate.Broadcast();
}

void FNiagaraSystemViewModel::SystemSelectionChanged()
{
	if (bUpdatingSystemSelectionFromSequencer == false)
	{
		UpdateSequencerFromEmitterHandleSelection();
	}
}

void FNiagaraSystemViewModel::SequencerTrackSelectionChanged(TArray<UMovieSceneTrack*> SelectedTracks)
{
	if (bUpdatingSequencerSelectionFromSystem == false && bResetingSequencerTracks == false)
	{
		UpdateEmitterHandleSelectionFromSequencer();
	}
}

void FNiagaraSystemViewModel::SequencerSectionSelectionChanged(TArray<UMovieSceneSection*> SelectedSections)
{
	if (bUpdatingSequencerSelectionFromSystem == false && bResetingSequencerTracks == false)
	{
		UpdateEmitterHandleSelectionFromSequencer();
	}
}

void FNiagaraSystemViewModel::UpdateEmitterHandleSelectionFromSequencer()
{
	if (!Sequencer.IsValid())
	{
		return;
	}
	
	TArray<FGuid> NewSelectedEmitterHandleIds;

	TArray<UMovieSceneTrack*> SelectedTracks;
	Sequencer->GetSelectedTracks(SelectedTracks);
	for (UMovieSceneTrack* SelectedTrack : SelectedTracks)
	{
		UMovieSceneNiagaraEmitterTrack* SelectedEmitterTrack = Cast<UMovieSceneNiagaraEmitterTrack>(SelectedTrack);
		if (SelectedEmitterTrack != nullptr && SelectedEmitterTrack->GetEmitterHandleViewModel().IsValid())
		{
			NewSelectedEmitterHandleIds.AddUnique(SelectedEmitterTrack->GetEmitterHandleViewModel()->GetId());
		}
	}

	TArray<UMovieSceneSection*> SelectedSections;
	Sequencer->GetSelectedSections(SelectedSections);
	for (UMovieSceneSection* SelectedSection : SelectedSections)
	{
		UMovieSceneNiagaraEmitterSectionBase* SelectedEmitterSection = Cast<UMovieSceneNiagaraEmitterSectionBase>(SelectedSection);
		if (SelectedEmitterSection != nullptr && SelectedEmitterSection->GetEmitterHandleViewModel().IsValid())
		{
			NewSelectedEmitterHandleIds.AddUnique(SelectedEmitterSection->GetEmitterHandleViewModel()->GetId());
		}
	}

	TGuardValue<bool> UpdateGuard(bUpdatingSystemSelectionFromSequencer, true);
	if (SelectionViewModel != nullptr)
	{
		TArray<UNiagaraStackEntry*> EntriesToSelect;
		TArray<UNiagaraStackEntry*> EntriesToDeselect;
		UNiagaraStackEntry* SystemRootEntry = SystemStackViewModel->GetRootEntry();
		if (NewSelectedEmitterHandleIds.Num() > 0 && GetEditorData().GetOwningSystemIsPlaceholder() == false)
		{
			EntriesToSelect.Add(SystemRootEntry);
		}
		else
		{
			EntriesToDeselect.Add(SystemRootEntry);
		}
		for (TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel : EmitterHandleViewModels)
		{
			UNiagaraStackEntry* EmitterRootEntry = EmitterHandleViewModel->GetEmitterStackViewModel()->GetRootEntry();
			if (NewSelectedEmitterHandleIds.Contains(EmitterHandleViewModel->GetId()))
			{
				EntriesToSelect.Add(EmitterRootEntry);
			}
			else
			{
				EntriesToDeselect.Add(EmitterRootEntry);
			}
		}
		bool bClearCurrentSelection = FSlateApplication::Get().GetModifierKeys().IsControlDown() == false;
		SelectionViewModel->UpdateSelectedEntries(EntriesToSelect, EntriesToDeselect, bClearCurrentSelection);
	}
}

void FNiagaraSystemViewModel::UpdateSequencerFromEmitterHandleSelection()
{
	if (!Sequencer.IsValid())
	{
		return;
	}
	TGuardValue<bool> UpdateGuard(bUpdatingSequencerSelectionFromSystem, true);
	Sequencer->EmptySelection();
	for (FGuid SelectedEmitterHandleId : SelectionViewModel->GetSelectedEmitterHandleIds())
	{
		for (UMovieSceneTrack* MainTrack : NiagaraSequence->GetMovieScene()->GetMasterTracks())
		{
			UMovieSceneNiagaraEmitterTrack* EmitterTrack = Cast<UMovieSceneNiagaraEmitterTrack>(MainTrack);
			if (EmitterTrack != nullptr && EmitterTrack->GetEmitterHandleViewModel()->GetId() == SelectedEmitterHandleId)
			{
				Sequencer->SelectTrack(EmitterTrack);
			}
		}
	}
}

void FNiagaraSystemViewModel::SystemInstanceReset()
{
	SystemInstanceInitialized();
}

void FNiagaraSystemViewModel::PreviewComponentSystemInstanceChanged()
{
	FNiagaraSystemInstance* OldSystemInstance = SystemInstance;
	FNiagaraSystemInstanceControllerPtr SystemInstanceController = PreviewComponent->GetSystemInstanceController();
	SystemInstance = SystemInstanceController.IsValid() ? SystemInstanceController->GetSoloSystemInstance() : nullptr;
	if (SystemInstance != OldSystemInstance)
	{
		if (SystemInstance != nullptr)
		{
			SystemInstance->OnInitialized().AddRaw(this, &FNiagaraSystemViewModel::SystemInstanceInitialized);
			SystemInstance->OnReset().AddRaw(this, &FNiagaraSystemViewModel::SystemInstanceReset);
		}
		else
		{
			for (TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel : EmitterHandleViewModels)
			{
				if (EmitterHandleViewModel->GetEmitterHandle())
				{
					EmitterHandleViewModel->SetSimulation(nullptr);
				}
			}
		}
	}
}

void FNiagaraSystemViewModel::SystemInstanceInitialized()
{
	for (TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel : EmitterHandleViewModels)
	{
		if (EmitterHandleViewModel->IsValid())
		{
			EmitterHandleViewModel->SetSimulation(SystemInstance->GetSimulationForHandle(*EmitterHandleViewModel->GetEmitterHandle()));
		}
	}
}

void FNiagaraSystemViewModel::UpdateEmitterFixedBounds()
{
	for (TSharedRef<FNiagaraEmitterHandleViewModel>& EmitterHandleViewModel : EmitterHandleViewModels)
	{
		// if we are an emitter asset we don't require pre-selection
		// if we are a system asset instead we filter out unselected emitters
		if (EditMode != ENiagaraSystemViewModelEditMode::EmitterAsset && SelectionViewModel->GetSelectedEmitterHandleIds().Contains(EmitterHandleViewModel->GetId()) == false)
		{
			continue;
		}
		FNiagaraEmitterHandle* SelectedEmitterHandle = EmitterHandleViewModel->GetEmitterHandle();
		check(SelectedEmitterHandle);
		for (TSharedRef<FNiagaraEmitterInstance, ESPMode::ThreadSafe>& EmitterInst : SystemInstance->GetEmitters())
		{
			if (&EmitterInst->GetEmitterHandle() == SelectedEmitterHandle && !EmitterInst->IsComplete())
			{
				EmitterInst->CalculateFixedBounds(PreviewComponent->GetComponentToWorld().Inverse());
			}
		}
	}
	PreviewComponent->MarkRenderTransformDirty();
	ResetSystem(ETimeResetMode::KeepCurrentTime, EMultiResetMode::ResetThisInstance, EReinitMode::ResetSystem);
}

void FNiagaraSystemViewModel::UpdateSystemFixedBounds()
{
	// early out as we only allow system fixed bounds update on system assets
	if(GetEditMode() != ENiagaraSystemViewModelEditMode::SystemAsset)
	{
		return;
	}

	if(SystemInstance != nullptr)
	{
		GetSystem().Modify();
		
		GetSystem().bFixedBounds = true;
		GetSystem().SetFixedBounds(SystemInstance->GetLocalBounds());

		PreviewComponent->MarkRenderTransformDirty();
		ResetSystem(ETimeResetMode::KeepCurrentTime, EMultiResetMode::ResetThisInstance, EReinitMode::ResetSystem);
	}
}

void FNiagaraSystemViewModel::ClearEmitterStats()
{
#if STATS
	for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		if (FVersionedNiagaraEmitterData* EmitterData = Handle.GetEmitterData())
		{
			EmitterData->GetStatData().ClearStatCaptures();
		}
	}
#endif
}

// this switches the viewed emitter version in the editor, for the version selector of the referenced parent version see SNiagaraOverviewStackNode::SwitchToVersion
bool FNiagaraSystemViewModel::ChangeEmitterVersion(const FVersionedNiagaraEmitter& Emitter, const FGuid& NewVersion)
{
	if (System->ChangeEmitterVersion(Emitter, NewVersion))
	{
		ResetStack();
		System->RequestCompile(false);
		return true;
	}
	return false;
}

void FNiagaraSystemViewModel::AddSystemEventHandlers()
{
	if (System != nullptr)
	{
		TArray<UNiagaraScript*> Scripts;
		Scripts.Add(System->GetSystemSpawnScript());
		Scripts.Add(System->GetSystemUpdateScript());

		for (UNiagaraScript* Script : Scripts)
		{
			if (Script != nullptr)
			{
				FDelegateHandle OnParameterStoreChangedHandle = Script->RapidIterationParameters.AddOnChangedHandler(
					FNiagaraParameterStore::FOnChanged::FDelegate::CreateThreadSafeSP<FNiagaraSystemViewModel, const FNiagaraParameterStore&, const UNiagaraScript*>(
						this->AsShared(), &FNiagaraSystemViewModel::SystemParameterStoreChanged, Script->RapidIterationParameters, Script));
				ScriptToOnParameterStoreChangedHandleMap.Add(FObjectKey(Script), OnParameterStoreChangedHandle);
			}
		}

		UserParameterStoreChangedHandle = System->GetExposedParameters().AddOnChangedHandler(
			FNiagaraParameterStore::FOnChanged::FDelegate::CreateThreadSafeSP<FNiagaraSystemViewModel, const FNiagaraParameterStore&, const UNiagaraScript*>(
				this->AsShared(), &FNiagaraSystemViewModel::SystemParameterStoreChanged, System->GetExposedParameters(), nullptr));

		SystemScriptGraphChangedHandle = SystemScriptViewModel->GetGraphViewModel()->GetGraph()->AddOnGraphChangedHandler(
			FOnGraphChanged::FDelegate::CreateSP(this->AsShared(), &FNiagaraSystemViewModel::SystemScriptGraphChanged));
		SystemScriptGraphNeedsRecompileHandle = SystemScriptViewModel->GetGraphViewModel()->GetGraph()->AddOnGraphNeedsRecompileHandler(
			FOnGraphChanged::FDelegate::CreateSP(this->AsShared(), &FNiagaraSystemViewModel::SystemScriptGraphChanged));
	}
}

void FNiagaraSystemViewModel::RemoveSystemEventHandlers()
{
	if (System != nullptr)
	{
		TArray<UNiagaraScript*> Scripts;
		Scripts.Add(System->GetSystemSpawnScript());
		Scripts.Add(System->GetSystemUpdateScript());

		for (UNiagaraScript* Script : Scripts)
		{
			if (Script != nullptr)
			{
				FDelegateHandle* OnParameterStoreChangedHandle = ScriptToOnParameterStoreChangedHandleMap.Find(FObjectKey(Script));
				if (OnParameterStoreChangedHandle != nullptr)
				{
					Script->RapidIterationParameters.RemoveOnChangedHandler(*OnParameterStoreChangedHandle);
				}
			}
		}

		System->GetExposedParameters().RemoveOnChangedHandler(UserParameterStoreChangedHandle);
		if (SystemScriptViewModel.IsValid())
		{
			SystemScriptViewModel->GetGraphViewModel()->GetGraph()->RemoveOnGraphChangedHandler(SystemScriptGraphChangedHandle);
			SystemScriptViewModel->GetGraphViewModel()->GetGraph()->RemoveOnGraphNeedsRecompileHandler(SystemScriptGraphNeedsRecompileHandle);
		}
	}

	ScriptToOnParameterStoreChangedHandleMap.Empty();
	UserParameterStoreChangedHandle.Reset();
	SystemScriptGraphChangedHandle.Reset();
	SystemScriptGraphNeedsRecompileHandle.Reset();
}

void FNiagaraSystemViewModel::BuildStackModuleData(UNiagaraScript* Script, FGuid InEmitterHandleId, TArray<FNiagaraStackModuleData>& OutStackModuleData)
{
	UNiagaraNodeOutput* OutputNode = FNiagaraEditorUtilities::GetScriptOutputNode(*Script);
	TArray<FNiagaraStackGraphUtilities::FStackNodeGroup> StackGroups;
	FNiagaraStackGraphUtilities::GetStackNodeGroups((UNiagaraNode&)*OutputNode, StackGroups);

	int StackIndex = 0;
	if (StackGroups.Num() > 2)
	{
		for (int i = 1; i < StackGroups.Num() - 1; i++)
		{
			FNiagaraStackGraphUtilities::FStackNodeGroup& StackGroup = StackGroups[i];
			StackIndex = i - 1;
			TArray<UNiagaraNode*> GroupNodes;
			StackGroup.GetAllNodesInGroup(GroupNodes);
			UNiagaraNodeFunctionCall * ModuleNode = Cast<UNiagaraNodeFunctionCall>(StackGroup.EndNode);
			if (ModuleNode != nullptr)
			{
				ENiagaraScriptUsage Usage = Script->GetUsage();
				FGuid UsageId = Script->GetUsageId();
				int32 Index = StackIndex;
				FNiagaraStackModuleData ModuleData = { ModuleNode, Usage, UsageId, Index, InEmitterHandleId };
				OutStackModuleData.Add(ModuleData);
			}
		}
	}

}

void FNiagaraSystemViewModel::SystemChanged(UNiagaraSystem* ChangedSystem)
{
	if (GIsTransacting == false)
	{
		check(System == ChangedSystem);
		RefreshAll();
	}
}

void FNiagaraSystemViewModel::StackViewModelStructureChanged(ENiagaraStructureChangedFlags Flags)
{
	if ((Flags & StructureChanged) != 0 && SelectionViewModel != nullptr)
	{
		SelectionViewModel->RefreshDeferred();
	}
}

void FNiagaraSystemViewModel::ScratchPadScriptsChanged()
{
	SystemStackViewModel->GetRootEntry()->RefreshChildren();
	for (TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel : EmitterHandleViewModels)
	{
		EmitterHandleViewModel->GetEmitterStackViewModel()->GetRootEntry()->RefreshChildren();
	}
}

void FNiagaraSystemViewModel::RefreshStackViewModels()
{
	if (SystemStackViewModel)
	{
		SystemStackViewModel->Refresh();
	}
	for (TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel : EmitterHandleViewModels)
	{
		EmitterHandleViewModel->GetEmitterStackViewModel()->Refresh();
	}
}

void FNiagaraSystemViewModel::RefreshAssetMessages()
{
	FNiagaraMessageManager* MessageManager = FNiagaraMessageManager::Get();
	for (const FObjectKey& FunctionCallNodeObjectKey : LastFunctionCallNodeObjectKeys)
	{
		MessageManager->ClearAssetMessagesForObject(SystemMessageLogGuidKey.GetValue(), FunctionCallNodeObjectKey);
	}
	LastFunctionCallNodeObjectKeys.Reset();

	auto PublishMessages = [this, &MessageManager](auto& MessageSource, const auto RemoveMessageFuncSig)->TArray<FObjectKey> /* MessageSourceObjectKeys */ {
		TArray<FObjectKey> NewObjectKeys;
		for (auto It = MessageSource.GetMessages().CreateConstIterator(); It; ++It)
		{
			const UNiagaraMessageData* MessageData = static_cast<UNiagaraMessageData*>(It.Value());
			FGenerateNiagaraMessageInfo GenerateNiagaraMessageInfo = FGenerateNiagaraMessageInfo();

			TArray<FObjectKey> AssociatedObjectKeys = { FObjectKey(&MessageSource) };
			NewObjectKeys.Append(AssociatedObjectKeys);
			GenerateNiagaraMessageInfo.SetAssociatedObjectKeys(AssociatedObjectKeys);

			TArray<FLinkNameAndDelegate> Links;
			const FText LinkText = LOCTEXT("AcknowledgeAndClearIssue", "Acknowledge and clear this issue.");
			const FGuid MessageKey = It.Key();
			FSimpleDelegate MessageDelegate = FSimpleDelegate::CreateUObject(&MessageSource, RemoveMessageFuncSig, MessageKey);
			FSimpleDelegate WrapperDelegate = FSimpleDelegate::CreateSP(this, &FNiagaraSystemViewModel::ExecuteMessageDelegateAndRefreshMessages, MessageDelegate);
			const FLinkNameAndDelegate Link = FLinkNameAndDelegate(LinkText, WrapperDelegate);
			Links.Add(Link);
			GenerateNiagaraMessageInfo.SetLinks(Links);

			TSharedRef<const INiagaraMessage> Message = MessageData->GenerateNiagaraMessage(GenerateNiagaraMessageInfo);
			MessageManager->AddMessage(Message, SystemMessageLogGuidKey.GetValue());
		}
		return NewObjectKeys;
	};

	auto PublishScriptMessages = [&](const TArray<UNiagaraScript*>& Scripts)->TArray<FObjectKey> /* FunctionCallNodeObjectKeys */ {
		TArray<FObjectKey> NewObjectKeys;
		for(UNiagaraScript* Script : Scripts)
		{
			UNiagaraNodeOutput* OutputNode = FNiagaraEditorUtilities::GetScriptOutputNode(*Script);
			if (OutputNode == nullptr)
			{
				// The output node may be null if the target script is a GPU compute script that is not in use.
				continue;
			}
			TArray<UNiagaraNodeFunctionCall*> FunctionCallNodes;
			FNiagaraStackGraphUtilities::GetOrderedModuleNodes(*OutputNode, FunctionCallNodes);
			for(UNiagaraNodeFunctionCall* FunctionCallNode : FunctionCallNodes)
			{
				NewObjectKeys.Append(PublishMessages(*FunctionCallNode, &UNiagaraNodeFunctionCall::RemoveMessageDelegateable));
			}
		}
		return NewObjectKeys;
	};

	UNiagaraSystem& ViewedSystem = GetSystem();
	if (EditMode == ENiagaraSystemViewModelEditMode::SystemAsset)
	{
		LastFunctionCallNodeObjectKeys.Append(PublishMessages(ViewedSystem, &UNiagaraSystem::RemoveMessageDelegateable));

		TArray<UNiagaraScript*> Scripts;
		Scripts.Add(ViewedSystem.GetSystemSpawnScript());
		Scripts.Add(ViewedSystem.GetSystemUpdateScript());
		for (const FNiagaraEmitterHandle& EmitterHandle : ViewedSystem.GetEmitterHandles())
		{
			FVersionedNiagaraEmitter VersionedEmitter = EmitterHandle.GetInstance();
			LastFunctionCallNodeObjectKeys.Append(PublishMessages(*VersionedEmitter.Emitter, &UNiagaraEmitter::RemoveMessageDelegateable));
			VersionedEmitter.GetEmitterData()->GetScripts(Scripts, false);
		}
		LastFunctionCallNodeObjectKeys.Append(PublishScriptMessages(Scripts));

	}
	else if (EditMode == ENiagaraSystemViewModelEditMode::EmitterAsset)
	{
		const TArray<FNiagaraEmitterHandle>& EmitterHandles = ViewedSystem.GetEmitterHandles();
		if (ensureMsgf(EmitterHandles.Num() == 1, TEXT("There was not exactly 1 Emitter Handle for the SystemViewModel in Emitter edit mode!")))
		{
			FVersionedNiagaraEmitter VersionedEmitter = EmitterHandles[0].GetInstance();
			LastFunctionCallNodeObjectKeys.Append(PublishMessages(*VersionedEmitter.Emitter, &UNiagaraEmitter::RemoveMessageDelegateable));

			TArray<UNiagaraScript*> Scripts;
			VersionedEmitter.GetEmitterData()->GetScripts(Scripts, false);
			LastFunctionCallNodeObjectKeys.Append(PublishScriptMessages(Scripts));
		}
	}
	else
	{
		checkf(false, TEXT("New system viewmodel edit mode defined! Must implemented RefreshAssetMessages() for this edit mode!"));
	}
}

TSharedPtr<FNiagaraOverviewGraphViewModel> FNiagaraSystemViewModel::GetOverviewGraphViewModel() const
{
	return OverviewGraphViewModel;
}

#undef LOCTEXT_NAMESPACE // NiagaraSystemViewModel
