// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassEntitySettings.h"
#include "MassProcessingPhaseManager.h"
#include "VisualLogger/VisualLogger.h"
#include "UObject/UObjectHash.h"
#include "Misc/CoreDelegates.h"
#include "MassArchetypeData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MassEntitySettings)

#if WITH_EDITOR
#include "ObjectEditorUtils.h"
#include "CoreGlobals.h"
#endif // WITH_EDITOR

//----------------------------------------------------------------------//
//  UMassEntitySettings
//----------------------------------------------------------------------//
UMassEntitySettings::UMassEntitySettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	for (int i = 0; i < (int)EMassProcessingPhase::MAX; ++i)
	{
		ProcessingPhasesConfig[i].PhaseName = *UEnum::GetDisplayValueAsText(EMassProcessingPhase(i)).ToString();
	}

	FCoreDelegates::OnPostEngineInit.AddUObject(this, &UMassEntitySettings::OnPostEngineInit);
}

void UMassEntitySettings::PostInitProperties()
{
	Super::PostInitProperties();
	ChunkMemorySize = UE::Mass::SanitizeChunkMemorySize(ChunkMemorySize);
}

void UMassEntitySettings::BeginDestroy()
{
	FCoreDelegates::OnPostEngineInit.RemoveAll(this);
	Super::BeginDestroy();
}

void UMassEntitySettings::OnPostEngineInit()
{
	bEngineInitialized = true;
	BuildProcessorListAndPhases();
}

void UMassEntitySettings::BuildProcessorListAndPhases()
{
	if (bInitialized == true || bEngineInitialized == false)
	{
		return;
	}

	BuildProcessorList();
	BuildPhases();
	bInitialized = true;

	OnInitializedEvent.Broadcast();
}

void UMassEntitySettings::BuildPhases()
{
#if WITH_EDITOR
	if (GIsEditor)
	{
		for (int i = 0; i < int(EMassProcessingPhase::MAX); ++i)
		{
			FMassProcessingPhaseConfig& PhaseConfig = ProcessingPhasesConfig[i];
			PhaseConfig.PhaseProcessor = NewObject<UMassCompositeProcessor>(this, PhaseConfig.PhaseGroupClass
				, *FString::Printf(TEXT("ProcessingPhase_%s"), *PhaseConfig.PhaseName.ToString()));
			PhaseConfig.PhaseProcessor->SetGroupName(PhaseConfig.PhaseName);
			PhaseConfig.PhaseProcessor->SetProcessingPhase(EMassProcessingPhase(i));
			const FString PhaseDumpDependencyGraphFileName = !DumpDependencyGraphFileName.IsEmpty() ? DumpDependencyGraphFileName + TEXT("_") + PhaseConfig.PhaseName.ToString() : FString();

			FMassProcessorDependencySolver::FResult Result;
			Result.DependencyGraphFileName = PhaseDumpDependencyGraphFileName;
			FMassPhaseProcessorConfigurationHelper Configurator(*PhaseConfig.PhaseProcessor, PhaseConfig, *this, EMassProcessingPhase(i));
			Configurator.bInitializeCreatedProcessors = false;
			Configurator.bIsGameRuntime = false;
			Configurator.Configure({}, EProcessorExecutionFlags::All, /*EntityManager=*/nullptr, &Result);

			if (Result.PrunedProcessorClasses.Num() > 0)
			{
				UE_VLOG_UELOG(this, LogMass, Warning, TEXT("Calculating dependencies for phase %s resulted in pruned processors:")
					, *PhaseConfig.PhaseName.ToString());
				for (const TSubclassOf<UMassProcessor>& ProcessorClass : Result.PrunedProcessorClasses)
				{
					UE_VLOG_UELOG(this, LogMass, Warning, TEXT("\t%s"), *GetNameSafe(ProcessorClass));
				}
				UE_VLOG_UELOG(this, LogMass, Warning, TEXT("Make sure EntityQueries owned by these processors got registered or override UMassProcessor::ShouldAllowQueryBasedPruning to return the appropriate result"));
			}

			FStringOutputDevice Ar;
			PhaseConfig.PhaseProcessor->DebugOutputDescription(Ar);
			PhaseConfig.Description = FText::FromString(Ar);
		}
	}
#endif // WITH_EDITOR
}

void UMassEntitySettings::BuildProcessorList()
{
	ProcessorCDOs.Reset();
	for (FMassProcessingPhaseConfig& PhaseConfig : ProcessingPhasesConfig)
	{
		PhaseConfig.ProcessorCDOs.Reset();
	}

	TArray<UClass*> SubClassess;
	GetDerivedClasses(UMassProcessor::StaticClass(), SubClassess);

	for (int i = SubClassess.Num() - 1; i >= 0; --i)
	{
		if (SubClassess[i]->HasAnyClassFlags(CLASS_Abstract))
		{
			continue;
		}

		UMassProcessor* ProcessorCDO = GetMutableDefault<UMassProcessor>(SubClassess[i]);
		// we explicitly restrict adding UMassCompositeProcessor. If needed by specific project a derived class can be added
		if (ProcessorCDO && SubClassess[i] != UMassCompositeProcessor::StaticClass()
#if WITH_EDITOR
			&& ProcessorCDO->ShouldShowUpInSettings()
#endif // WITH_EDITOR
		)
		{
			ProcessorCDOs.Add(ProcessorCDO);
			if (ProcessorCDO->ShouldAutoAddToGlobalList())
			{
				ProcessingPhasesConfig[int(ProcessorCDO->GetProcessingPhase())].ProcessorCDOs.Add(ProcessorCDO);
			}
		}
	}

	ProcessorCDOs.Sort([](UMassProcessor& LHS, UMassProcessor& RHS) {
		return LHS.GetName().Compare(RHS.GetName()) < 0;
	});
}

void UMassEntitySettings::AddToActiveProcessorsList(TSubclassOf<UMassProcessor> ProcessorClass)
{
	if (UMassProcessor* ProcessorCDO = GetMutableDefault<UMassProcessor>(ProcessorClass))
	{
		if (ProcessorClass == UMassCompositeProcessor::StaticClass())
		{
			UE_VLOG_UELOG(this, LogMass, Log, TEXT("%s adding MassCompositeProcessor to the global processor list is unsupported"), ANSI_TO_TCHAR(__FUNCTION__));
		}
		else if (ProcessorClass->HasAnyClassFlags(CLASS_Abstract))
		{
			UE_VLOG_UELOG(this, LogMass, Log, TEXT("%s unable to add %s due to it being an abstract class"), ANSI_TO_TCHAR(__FUNCTION__), *ProcessorClass->GetName());
		}
		else if (ProcessorCDOs.Find(ProcessorCDO) != INDEX_NONE)
		{
			UE_VLOG_UELOG(this, LogMass, Log, TEXT("%s already in global processor list"), *ProcessorCDO->GetName());
		}
		else 
		{
			ensureMsgf(ProcessorCDO->ShouldAutoAddToGlobalList() == false, TEXT("%s missing from the global list while it's already marked to be auto-added"), *ProcessorCDO->GetName());
			ProcessorCDOs.Add(ProcessorCDO);
			ProcessorCDO->SetShouldAutoRegisterWithGlobalList(true);
		}
	}
}

TConstArrayView<FMassProcessingPhaseConfig> UMassEntitySettings::GetProcessingPhasesConfig()
{
	BuildProcessorListAndPhases();
	return MakeArrayView(ProcessingPhasesConfig, int32(EMassProcessingPhase::MAX));
}

#if WITH_EDITOR
void UMassEntitySettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	static const FName ProcessorCDOsName = GET_MEMBER_NAME_CHECKED(UMassEntitySettings, ProcessorCDOs);
	static const FName ChunkMemorySizeName = GET_MEMBER_NAME_CHECKED(UMassEntitySettings, ChunkMemorySize);

	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayAdd)
	{
		// ignore adding elements to arrays since it would be 'None' at first
		return;
	}

	if (PropertyChangedEvent.Property)
	{
		const FName PropName = PropertyChangedEvent.Property->GetFName();
		if (PropName == ProcessorCDOsName)
		{
			BuildProcessorList();
		}
		else if (PropName == ChunkMemorySizeName)
		{
			ChunkMemorySize = UE::Mass::SanitizeChunkMemorySize(ChunkMemorySize);
		}

		BuildPhases();
		OnSettingsChange.Broadcast(PropertyChangedEvent);
	}
}

void UMassEntitySettings::PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent)
{
	static const FName AutoRegisterName = TEXT("bAutoRegisterWithProcessingPhases");

	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	FProperty* Property = PropertyChangedEvent.Property;
	FProperty* MemberProperty = nullptr;
	FEditPropertyChain::TDoubleLinkedListNode* LastPropertyNode = PropertyChangedEvent.PropertyChain.GetActiveMemberNode();
	while (LastPropertyNode && LastPropertyNode->GetNextNode())
	{
		LastPropertyNode = LastPropertyNode->GetNextNode();
	}

	if (LastPropertyNode)
	{
		MemberProperty = LastPropertyNode->GetValue();
	}

	if (MemberProperty && MemberProperty->GetFName() == AutoRegisterName)
	{
		BuildProcessorList();
	}
}
#endif // WITH_EDITOR

