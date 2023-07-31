// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewModels/Stack/NiagaraStackRoot.h"
#include "ViewModels/Stack/NiagaraStackScriptItemGroup.h"
#include "ViewModels/Stack/NiagaraStackEmitterPropertiesGroup.h"
#include "ViewModels/Stack/NiagaraStackEmitterSettingsGroup.h"
#include "ViewModels/Stack/NiagaraStackRenderItemGroup.h"
#include "ViewModels/Stack/NiagaraStackEventScriptItemGroup.h"
#include "ViewModels/Stack/NiagaraStackSimulationStageGroup.h"
#include "ViewModels/NiagaraSystemViewModel.h"
#include "ViewModels/NiagaraEmitterViewModel.h"
#include "ViewModels/NiagaraScriptViewModel.h"
#include "ViewModels/NiagaraSystemScriptViewModel.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemEditorData.h"
#include "NiagaraEmitterEditorData.h"
#include "ViewModels/Stack/NiagaraStackSystemSettingsGroup.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraStackRoot)

#define LOCTEXT_NAMESPACE "NiagaraStackViewModel"

UNiagaraStackRoot::UNiagaraStackRoot()
	: SystemPropertiesGroup(nullptr)
	, SystemUserParametersGroup(nullptr)
	, EmitterPropertiesGroup(nullptr)
	, EmitterSummaryGroup(nullptr)
	, EmitterSpawnGroup(nullptr)
	, EmitterUpdateGroup(nullptr)
	, ParticleSpawnGroup(nullptr)
	, ParticleUpdateGroup(nullptr)
	, RenderGroup(nullptr)
{
}

void UNiagaraStackRoot::Initialize(FRequiredEntryData InRequiredEntryData, bool bInIncludeSystemInformation, bool bInIncludeEmitterInformation)
{
	Super::Initialize(InRequiredEntryData, TEXT("Root"));
	bIncludeSystemInformation = bInIncludeSystemInformation;
	bIncludeEmitterInformation = bInIncludeEmitterInformation;
	SystemPropertiesGroup = nullptr;
	SystemUserParametersGroup = nullptr;
	EmitterPropertiesGroup = nullptr;
	EmitterSummaryGroup = nullptr;
	EmitterSpawnGroup = nullptr;
	EmitterUpdateGroup = nullptr;
	ParticleSpawnGroup = nullptr;
	ParticleUpdateGroup = nullptr;
	RenderGroup = nullptr;

	if (bInIncludeEmitterInformation && GetEmitterViewModel())
	{
		GetEmitterViewModel()->GetOrCreateEditorData().OnSummaryViewStateChanged().AddUObject(this, &UNiagaraStackRoot::OnSummaryViewStateChanged);
	}
}

void UNiagaraStackRoot::FinalizeInternal()
{
	if (bIncludeEmitterInformation && GetEmitterViewModel() && GetEmitterViewModel()->GetEmitter().GetEmitterData())
	{
		GetEmitterViewModel()->GetOrCreateEditorData().OnSummaryViewStateChanged().RemoveAll(this);
	}	
	Super::FinalizeInternal();
}

bool UNiagaraStackRoot::GetCanExpand() const
{
	return false;
}

bool UNiagaraStackRoot::GetShouldShowInStack() const
{
	return false;
}

void UNiagaraStackRoot::RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues)
{
	// Create static entries as needed.
	if (bIncludeSystemInformation && SystemPropertiesGroup == nullptr)
	{
		SystemPropertiesGroup = NewObject<UNiagaraStackSystemPropertiesGroup>(this);
		FRequiredEntryData RequiredEntryData(GetSystemViewModel(), GetEmitterViewModel(),
			FExecutionCategoryNames::System, FExecutionSubcategoryNames::Settings,
			GetSystemViewModel()->GetEditorData().GetStackEditorData());
		SystemPropertiesGroup->Initialize(RequiredEntryData);
	}

	if (bIncludeSystemInformation && SystemUserParametersGroup == nullptr)
	{
		SystemUserParametersGroup = NewObject<UNiagaraStackSystemUserParametersGroup>(this);
		FRequiredEntryData RequiredEntryData(GetSystemViewModel(), GetEmitterViewModel(),
			FExecutionCategoryNames::System, FExecutionSubcategoryNames::Settings,
			GetSystemViewModel()->GetEditorData().GetStackEditorData());
		SystemUserParametersGroup->Initialize(RequiredEntryData, &GetSystemViewModel()->GetSystem(), &GetSystemViewModel()->GetSystem().GetExposedParameters());
	}

	if (bIncludeSystemInformation && SystemSpawnGroup == nullptr)
	{
		SystemSpawnGroup = NewObject<UNiagaraStackScriptItemGroup>(this);
		FRequiredEntryData RequiredEntryData(GetSystemViewModel(), GetEmitterViewModel(),
			FExecutionCategoryNames::System, FExecutionSubcategoryNames::Spawn,
			GetSystemViewModel()->GetEditorData().GetStackEditorData());
		FText DisplayName = LOCTEXT("SystemSpawnGroupName", "System Spawn");
		FText ToolTip = LOCTEXT("SystemSpawnGroupToolTip", "Occurs once at System creation on the CPU. Modules in this stage should initialize defaults and/or do initial setup.\r\nModules are executed in order from top to bottom of the stack.");
		SystemSpawnGroup->Initialize(RequiredEntryData, DisplayName, ToolTip, GetSystemViewModel()->GetSystemScriptViewModel().ToSharedRef(), ENiagaraScriptUsage::SystemSpawnScript);
	}

	if (bIncludeSystemInformation && SystemUpdateGroup == nullptr)
	{
		SystemUpdateGroup = NewObject<UNiagaraStackScriptItemGroup>(this);
		FRequiredEntryData RequiredEntryData(GetSystemViewModel(), GetEmitterViewModel(),
			FExecutionCategoryNames::System, FExecutionSubcategoryNames::Update,
			GetSystemViewModel()->GetEditorData().GetStackEditorData());
		FText DisplayName = LOCTEXT("SystemUpdateGroupName", "System Update");
		FText ToolTip = LOCTEXT("SystemUpdateGroupToolTip", "Occurs every Emitter tick on the CPU.Modules in this stage should compute values for parameters for emitter or particle update or spawning this frame.\r\nModules are executed in order from top to bottom of the stack.");
		SystemUpdateGroup->Initialize(RequiredEntryData, DisplayName, ToolTip, GetSystemViewModel()->GetSystemScriptViewModel().ToSharedRef(), ENiagaraScriptUsage::SystemUpdateScript);
	}

	bool bShouldShowSummaryView = GetEmitterViewModel()? GetEmitterViewModel()->GetOrCreateEditorData().ShouldShowSummaryView() : false;

	// we clear references to entries we don't want to display in summary view.
	// Since they also won't get added to the NewChildren array, they will get finalized and eventually GC'd
	if(bShouldShowSummaryView)
	{
		EmitterSpawnGroup = nullptr;
		EmitterUpdateGroup = nullptr;
		ParticleSpawnGroup = nullptr;
		ParticleUpdateGroup = nullptr;
	}
	
	if (bIncludeEmitterInformation && EmitterPropertiesGroup == nullptr)
	{
		EmitterPropertiesGroup = NewObject<UNiagaraStackEmitterPropertiesGroup>(this);
		FRequiredEntryData RequiredEntryData(GetSystemViewModel(), GetEmitterViewModel(),
			FExecutionCategoryNames::Emitter, FExecutionSubcategoryNames::Settings,
			GetEmitterViewModel()->GetOrCreateEditorData().GetStackEditorData());
		EmitterPropertiesGroup->Initialize(RequiredEntryData);
	}

	if (bIncludeEmitterInformation && EmitterSummaryGroup == nullptr)
	{
		EmitterSummaryGroup = NewObject<UNiagaraStackEmitterSummaryGroup>(this);
		FRequiredEntryData RequiredEntryData(GetSystemViewModel(), GetEmitterViewModel(),
			FExecutionCategoryNames::Emitter, FExecutionSubcategoryNames::Settings,
			GetEmitterViewModel()->GetOrCreateEditorData().GetStackEditorData());
		FText DisplayName = LOCTEXT("EmitterSummaryGroupName", "Emitter Summary");
		FText Tooltip = LOCTEXT("EmitterSummaryTooltip", "Summary of parameters for this Emitter.");
		EmitterSummaryGroup->Initialize(RequiredEntryData, DisplayName, Tooltip, nullptr);
	}

	if (bIncludeEmitterInformation && !bShouldShowSummaryView && EmitterSpawnGroup == nullptr)
	{
		EmitterSpawnGroup = NewObject<UNiagaraStackScriptItemGroup>(this);
		FRequiredEntryData RequiredEntryData(GetSystemViewModel(), GetEmitterViewModel(),
			FExecutionCategoryNames::Emitter, FExecutionSubcategoryNames::Spawn,
			GetEmitterViewModel()->GetOrCreateEditorData().GetStackEditorData());
		FText DisplayName = LOCTEXT("EmitterSpawnGroupName", "Emitter Spawn");
		FText ToolTip = LOCTEXT("EmitterSpawnGroupTooltip", "Occurs once at Emitter creation on the CPU. Modules in this stage should initialize defaults and/or do initial setup.\r\nModules are executed in order from top to bottom of the stack.");
		EmitterSpawnGroup->Initialize(RequiredEntryData, DisplayName, ToolTip, GetEmitterViewModel()->GetSharedScriptViewModel(), ENiagaraScriptUsage::EmitterSpawnScript);
	}

	if (bIncludeEmitterInformation && !bShouldShowSummaryView && EmitterUpdateGroup == nullptr)
	{
		EmitterUpdateGroup = NewObject<UNiagaraStackScriptItemGroup>(this);
		FRequiredEntryData RequiredEntryData(GetSystemViewModel(), GetEmitterViewModel(),
			FExecutionCategoryNames::Emitter, FExecutionSubcategoryNames::Update,
			GetEmitterViewModel()->GetOrCreateEditorData().GetStackEditorData());
		FText DisplayName = LOCTEXT("EmitterUpdateGroupName", "Emitter Update");
		FText ToolTip = LOCTEXT("EmitterUpdateGroupTooltip", "Occurs every Emitter tick on the CPU. Modules in this stage should compute values for parameters for Particle Update or Spawning this frame.\r\nModules are executed in order from top to bottom of the stack.");
		EmitterUpdateGroup->Initialize(RequiredEntryData, DisplayName, ToolTip, GetEmitterViewModel()->GetSharedScriptViewModel(), ENiagaraScriptUsage::EmitterUpdateScript);
	}

	if (bIncludeEmitterInformation && !bShouldShowSummaryView && ParticleSpawnGroup == nullptr)
	{
		ParticleSpawnGroup = NewObject<UNiagaraStackScriptItemGroup>(this);
		FRequiredEntryData RequiredEntryData(GetSystemViewModel(), GetEmitterViewModel(),
			FExecutionCategoryNames::Particle, FExecutionSubcategoryNames::Spawn,
			GetEmitterViewModel()->GetOrCreateEditorData().GetStackEditorData());
		FText DisplayName = LOCTEXT("ParticleSpawnGroupName", "Particle Spawn");
		FText ToolTip = LOCTEXT("ParticleSpawnGroupTooltip", "Called once per created particle. Modules in this stage should set up initial values for each particle.\r\nIf \"Use Interpolated Spawning\" is set, we will also run the Particle Update stage after the Particle Spawn stage.\r\nModules are executed in order from top to bottom of the stack.");
		ParticleSpawnGroup->Initialize(RequiredEntryData, DisplayName, ToolTip, GetEmitterViewModel()->GetSharedScriptViewModel(), ENiagaraScriptUsage::ParticleSpawnScript);
	}

	if (bIncludeEmitterInformation && !bShouldShowSummaryView && ParticleUpdateGroup == nullptr)
	{
		ParticleUpdateGroup = NewObject<UNiagaraStackScriptItemGroup>(this);
		FRequiredEntryData RequiredEntryData(GetSystemViewModel(), GetEmitterViewModel(),
			FExecutionCategoryNames::Particle, FExecutionSubcategoryNames::Update,
			GetEmitterViewModel()->GetOrCreateEditorData().GetStackEditorData());
		FText DisplayName = LOCTEXT("ParticleUpdateGroupName", "Particle Update");
		FText ToolTip = LOCTEXT("ParticleUpdateGroupTooltip", "Called every frame per particle. Modules in this stage should update new values for this frame.\r\nModules are executed in order from top to bottom of the stack.");
		ParticleUpdateGroup->Initialize(RequiredEntryData, DisplayName, ToolTip, GetEmitterViewModel()->GetSharedScriptViewModel(), ENiagaraScriptUsage::ParticleUpdateScript);
	}

	if (bIncludeEmitterInformation && RenderGroup == nullptr)
	{
		RenderGroup = NewObject<UNiagaraStackRenderItemGroup>(this);
		FRequiredEntryData RequiredEntryData(GetSystemViewModel(), GetEmitterViewModel(),
			FExecutionCategoryNames::Render, FExecutionSubcategoryNames::Render,
			GetEmitterViewModel()->GetOrCreateEditorData().GetStackEditorData());
		RenderGroup->Initialize(RequiredEntryData);
	}

	if (bIncludeEmitterInformation && SummaryCollapseButton == nullptr)
	{
		SummaryCollapseButton = NewObject<UNiagaraStackSummaryViewCollapseButton>(this);
		FRequiredEntryData RequiredEntryData(GetSystemViewModel(), GetEmitterViewModel(),
			FExecutionCategoryNames::Emitter, FExecutionSubcategoryNames::Settings,
			GetEmitterViewModel()->GetOrCreateEditorData().GetStackEditorData());
		SummaryCollapseButton->Initialize(RequiredEntryData);
	}

	// Populate new children
	if (bIncludeSystemInformation)
	{
		NewChildren.Add(SystemPropertiesGroup);
		NewChildren.Add(SystemUserParametersGroup);
		NewChildren.Add(SystemSpawnGroup);
		NewChildren.Add(SystemUpdateGroup);
	}

	if (bIncludeEmitterInformation)
	{
		NewChildren.Add(EmitterPropertiesGroup);	
		NewChildren.Add(EmitterSummaryGroup);

		if (!bShouldShowSummaryView)
		{
			NewChildren.Add(EmitterSpawnGroup);
			NewChildren.Add(EmitterUpdateGroup);

			NewChildren.Add(ParticleSpawnGroup);
			NewChildren.Add(ParticleUpdateGroup);

			for (const FNiagaraEventScriptProperties& EventScriptProperties : GetEmitterViewModel()->GetEmitter().GetEmitterData()->GetEventHandlers())
			{
				UNiagaraStackEventScriptItemGroup* EventHandlerGroup = FindCurrentChildOfTypeByPredicate<UNiagaraStackEventScriptItemGroup>(CurrentChildren,
					[&](UNiagaraStackEventScriptItemGroup* CurrentEventHandlerGroup) { return CurrentEventHandlerGroup->GetScriptUsageId() == EventScriptProperties.Script->GetUsageId(); });

				if (EventHandlerGroup == nullptr)
				{
					EventHandlerGroup = NewObject<UNiagaraStackEventScriptItemGroup>(this);
					FRequiredEntryData RequiredEntryData(GetSystemViewModel(), GetEmitterViewModel(),
						FExecutionCategoryNames::Particle, FExecutionSubcategoryNames::Event,
						GetEmitterViewModel()->GetEditorData().GetStackEditorData());
					EventHandlerGroup->Initialize(RequiredEntryData, GetEmitterViewModel()->GetSharedScriptViewModel(), ENiagaraScriptUsage::ParticleEventScript, EventScriptProperties.Script->GetUsageId(), EventScriptProperties.SourceEmitterID);
					EventHandlerGroup->SetOnModifiedEventHandlers(UNiagaraStackEventScriptItemGroup::FOnModifiedEventHandlers::CreateUObject(this, &UNiagaraStackRoot::EmitterArraysChanged));
				}

				NewChildren.Add(EventHandlerGroup);
			}

			for (UNiagaraSimulationStageBase* SimulationStage : GetEmitterViewModel()->GetEmitter().GetEmitterData()->GetSimulationStages())
			{
				UNiagaraStackSimulationStageGroup* SimulationStageGroup = FindCurrentChildOfTypeByPredicate<UNiagaraStackSimulationStageGroup>(CurrentChildren,
					[SimulationStage](UNiagaraStackSimulationStageGroup* CurrentSimulationStageGroup) { return CurrentSimulationStageGroup->GetSimulationStage() == SimulationStage; });

				if (SimulationStageGroup == nullptr)
				{
					SimulationStageGroup = NewObject<UNiagaraStackSimulationStageGroup>(this);
					FRequiredEntryData RequiredEntryData(GetSystemViewModel(), GetEmitterViewModel(),
						FExecutionCategoryNames::Particle, FExecutionSubcategoryNames::SimulationStage,
						GetEmitterViewModel()->GetEditorData().GetStackEditorData());
					SimulationStageGroup->Initialize(RequiredEntryData, GetEmitterViewModel()->GetSharedScriptViewModel(), SimulationStage);
					SimulationStageGroup->SetOnModifiedSimulationStages(UNiagaraStackSimulationStageGroup::FOnModifiedSimulationStages::CreateUObject(this, &UNiagaraStackRoot::EmitterArraysChanged));
				}

				NewChildren.Add(SimulationStageGroup);
			}
		}

		NewChildren.Add(RenderGroup);
		NewChildren.Add(SummaryCollapseButton);
	}
}

void UNiagaraStackRoot::EmitterArraysChanged()
{
	RefreshChildren();
}

void UNiagaraStackRoot::OnSummaryViewStateChanged()
{
	if (!IsFinalized())
	{
		RefreshChildren();
	}
}

#undef LOCTEXT_NAMESPACE

