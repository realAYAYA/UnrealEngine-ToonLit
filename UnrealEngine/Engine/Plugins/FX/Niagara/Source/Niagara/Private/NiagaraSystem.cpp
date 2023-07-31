// Copyright Epic Games, Inc. All Rights Reserved.


#include "NiagaraSystem.h"

#include "AssetRegistry/IAssetRegistry.h"
#include "INiagaraEditorOnlyDataUtlities.h"
#include "NiagaraAsyncCompile.h"
#include "NiagaraConstants.h"
#include "NiagaraCustomVersion.h"
#include "NiagaraEditorDataBase.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraModule.h"
#include "NiagaraPrecompileContainer.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraScratchPadContainer.h"
#include "NiagaraScriptSourceBase.h"
#include "NiagaraSettings.h"
#include "NiagaraShared.h"
#include "NiagaraSimulationStageBase.h"
#include "NiagaraStats.h"
#include "NiagaraTrace.h"
#include "NiagaraTypes.h"
#include "NiagaraWorldManager.h"
#include "Algo/RemoveIf.h"
#include "Async/Async.h"
#include "Interfaces/ITargetPlatform.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/ScopeExit.h"
#include "Modules/ModuleManager.h"
#include "ProfilingDebugging/CookStats.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/Package.h"
#include "UObject/UObjectThreadContext.h"
#include "ShaderCompiler.h"
#include "PipelineStateCache.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraSystem)

#define LOCTEXT_NAMESPACE "NiagaraSystem"

#if WITH_EDITOR
#include "DerivedDataCacheInterface.h"
#endif

DECLARE_CYCLE_STAT(TEXT("Niagara - System - CompileScript"), STAT_Niagara_System_CompileScript, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("Niagara - System - CompileScript_ResetAfter"), STAT_Niagara_System_CompileScriptResetAfter, STATGROUP_Niagara);

#if ENABLE_COOK_STATS
namespace NiagaraScriptCookStats
{
	extern FCookStats::FDDCResourceUsageStats UsageStats;
}
#endif

//Disable for now until we can spend more time on a good method of applying the data gathered.
int32 GEnableNiagaraRuntimeCycleCounts = 0;
static FAutoConsoleVariableRef CVarEnableNiagaraRuntimeCycleCounts(TEXT("fx.EnableNiagaraRuntimeCycleCounts"), GEnableNiagaraRuntimeCycleCounts, TEXT("Toggle for runtime cylce counts tracking Niagara's frame time. \n"), ECVF_ReadOnly);

static int GNiagaraLogDDCStatusForSystems = 0;
static FAutoConsoleVariableRef CVarLogDDCStatusForSystems(
	TEXT("fx.NiagaraLogDDCStatusForSystems"),
	GNiagaraLogDDCStatusForSystems,
	TEXT("When enabled UNiagaraSystems will log out when their subscripts are pulled from the DDC or not."),
	ECVF_Default
);

static float GNiagaraScalabiltiyMinumumMaxDistance = 1.0f;
static FAutoConsoleVariableRef CVarNiagaraScalabiltiyMinumumMaxDistance(
	TEXT("fx.Niagara.Scalability.MinMaxDistance"),
	GNiagaraScalabiltiyMinumumMaxDistance,
	TEXT("Minimum value for Niagara's Max distance value. Primariy to prevent divide by zero issues and ensure a sensible distance value for sorted significance culling."),
	ECVF_Default
);

static float GNiagaraCompileWaitLoggingThreshold = 30.0f;
static FAutoConsoleVariableRef CVarNiagaraCompileWaitLoggingThreshold(
	TEXT("fx.Niagara.CompileWaitLoggingThreshold"),
	GNiagaraCompileWaitLoggingThreshold,
	TEXT("During automation, how long do we wait for a compile result before logging."),
	ECVF_Default
);

static int GNiagaraCompileWaitLoggingTerminationCap = 3;
static FAutoConsoleVariableRef CVarNiagaraCompileWaitLoggingTerminationCap(
	TEXT("fx.Niagara.CompileWaitLoggingCap"),
	GNiagaraCompileWaitLoggingTerminationCap,
	TEXT("During automation, how many times do we log before failing compilation?"),
	ECVF_Default
);

//////////////////////////////////////////////////////////////////////////

class FNiagaraOptimizeCompleteTask
{
public:
	FNiagaraOptimizeCompleteTask(UNiagaraSystem* Owner, FGraphEventRef* InRefToClear)
		: WeakOwner(Owner)
		, RefToClear(InRefToClear)
		, ReferenceValue(InRefToClear != nullptr ? InRefToClear->GetReference() : nullptr)
	{
	}

	FORCEINLINE TStatId GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(FNiagaraOptimizeCompleteTask, STATGROUP_TaskGraphTasks); }
	ENamedThreads::Type GetDesiredThread() { return ENamedThreads::GameThread; }
	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		if (UNiagaraSystem* Owner = WeakOwner.Get())
		{
			if (RefToClear && (RefToClear->GetReference() == ReferenceValue) )
			{
				*RefToClear = nullptr;
			}
		}
	}

	TWeakObjectPtr<UNiagaraSystem> WeakOwner;
	FGraphEventRef* RefToClear = nullptr;
	FGraphEvent* ReferenceValue = nullptr;
};

//////////////////////////////////////////////////////////////////////////

UNiagaraSystem::UNiagaraSystem(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
#if WITH_EDITORONLY_DATA
, bCompileForEdit(false)
, bBakeOutRapidIteration(false)
, bBakeOutRapidIterationOnCook(true)
, bTrimAttributes(false)
, bTrimAttributesOnCook(true)
, bIgnoreParticleReadsForAttributeTrim(false)
, bDisableDebugSwitches(false)
, bDisableDebugSwitchesOnCook(true)
#endif
, bSupportLargeWorldCoordinates(true)
, bDisableExperimentalVM(false)
, bFixedBounds(false)
#if WITH_EDITORONLY_DATA
, bIsolateEnabled(false)
#endif
, FixedBounds(FBox(FVector(-100), FVector(100)))
, bAutoDeactivate(true)
, WarmupTime(0.0f)
, WarmupTickCount(0)
, WarmupTickDelta(1.0f / 15.0f)
, bHasSystemScriptDIsWithPerInstanceData(false)
, bNeedsGPUContextInitForDataInterfaces(false)
, bNeedsAsyncOptimize(true)
, bHasDIsWithPostSimulateTick(false)
, bAllDIsPostSimulateCanOverlapFrames(true)
, bHasAnyGPUEmitters(false)
, bNeedsSortedSignificanceCull(false)
, ActiveInstances(0)
{
	ExposedParameters.SetOwner(this);
#if WITH_EDITORONLY_DATA
	EditorOnlyAddedParameters.SetOwner(this);
#endif
	MaxPoolSize = 32;

	EffectType = nullptr;
	bOverrideScalabilitySettings = false;

#if WITH_EDITORONLY_DATA
	AssetGuid = FGuid::NewGuid();
#endif

#if STATS
	StatDatabase.Init();
#endif
	
	const UNiagaraSettings* Settings = GetDefault<UNiagaraSettings>();
	bLwcEnabledSettingCached = Settings ? Settings->bSystemsSupportLargeWorldCoordinates : true;
}

UNiagaraSystem::UNiagaraSystem(FVTableHelper& Helper)
	: Super(Helper)
{
}

void UNiagaraSystem::BeginDestroy()
{
	Super::BeginDestroy();

#if WITH_EDITORONLY_DATA
	while (ActiveCompilations.Num() > 0)
	{
		KillAllActiveCompilations();
	}
#endif

#if WITH_EDITOR
	CleanupDefinitionsSubscriptions();
#endif

	FNiagaraWorldManager::DestroyAllSystemSimulations(this);
}

void UNiagaraSystem::PreSave(const class ITargetPlatform* TargetPlatform)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS;
	Super::PreSave(TargetPlatform);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS;
}

void UNiagaraSystem::PreSave(FObjectPreSaveContext ObjectSaveContext)
{
	Super::PreSave(ObjectSaveContext);
	EnsureFullyLoaded();
#if WITH_EDITORONLY_DATA
	WaitForCompilationComplete();
#endif
}

#if WITH_EDITOR
void UNiagaraSystem::BeginCacheForCookedPlatformData(const ITargetPlatform *TargetPlatform)
{
	//UE_LOG(LogNiagara, Display, TEXT("UNiagaraSystem::BeginCacheForCookedPlatformData %s %s"), *GetFullName(), GIsSavingPackage ? TEXT("Saving...") : TEXT("Not Saving..."));
	Super::BeginCacheForCookedPlatformData(TargetPlatform);
	
	EnsureFullyLoaded();
#if WITH_EDITORONLY_DATA
	WaitForCompilationComplete();
#endif
}

void UNiagaraSystem::HandleVariableRenamed(const FNiagaraVariable& InOldVariable, const FNiagaraVariable& InNewVariable, bool bUpdateContexts)
{
	if (InOldVariable.IsInNameSpace(FNiagaraConstants::UserNamespaceString))
	{
		if (GetExposedParameters().IndexOf(InOldVariable) != INDEX_NONE)
			GetExposedParameters().RenameParameter(InOldVariable, InNewVariable.GetName());
		InitSystemCompiledData();
	}

	for (const FNiagaraEmitterHandle& Handle : GetEmitterHandles())
	{
		FVersionedNiagaraEmitter VersionedEmitter = Handle.GetInstance();
		if (VersionedEmitter.Emitter)
		{
			VersionedEmitter.Emitter->HandleVariableRenamed(InOldVariable, InNewVariable, false, VersionedEmitter.Version);
		}
	}

	if (bUpdateContexts)
	{
		FNiagaraSystemUpdateContext UpdateCtx(this, true);
	}
}

void UNiagaraSystem::HandleVariableRemoved(const FNiagaraVariable& InOldVariable, bool bUpdateContexts)
{
	if (InOldVariable.IsInNameSpace(FNiagaraConstants::UserNamespaceString))
	{
		if (GetExposedParameters().IndexOf(InOldVariable) != INDEX_NONE)
			GetExposedParameters().RemoveParameter(InOldVariable);
		InitSystemCompiledData();
	}
	for (const FNiagaraEmitterHandle& Handle : GetEmitterHandles())
	{
		FVersionedNiagaraEmitter VersionedEmitter = Handle.GetInstance();
		if (VersionedEmitter.Emitter)
		{
			VersionedEmitter.Emitter->HandleVariableRemoved(InOldVariable, false, VersionedEmitter.Version);
		}
	}
	if (bUpdateContexts)
	{
		FNiagaraSystemUpdateContext UpdateCtx(this, true);
	}
}

TArray<UNiagaraScriptSourceBase*> UNiagaraSystem::GetAllSourceScripts()
{
	EnsureFullyLoaded();
	return { SystemSpawnScript->GetLatestSource(), SystemUpdateScript->GetLatestSource() };
}

FString UNiagaraSystem::GetSourceObjectPathName() const
{
	return GetPathName();
}

TArray<UNiagaraEditorParametersAdapterBase*> UNiagaraSystem::GetEditorOnlyParametersAdapters()
{
	return { GetEditorParameters() };
}

TArray<INiagaraParameterDefinitionsSubscriber*> UNiagaraSystem::GetOwnedParameterDefinitionsSubscribers()
{
	TArray<INiagaraParameterDefinitionsSubscriber*> OutSubscribers;
	for (const FNiagaraEmitterHandle& EmitterHandle : GetEmitterHandles())
	{
		OutSubscribers.Add(EmitterHandle.GetInstance().Emitter);
	}
	return OutSubscribers;
}

bool UNiagaraSystem::ChangeEmitterVersion(const FVersionedNiagaraEmitter& VersionedEmitter, const FGuid& NewVersion)
{
	if (VersionedEmitter.Emitter == nullptr || VersionedEmitter.Emitter->IsVersioningEnabled() == false)
	{
		return false;
	}
	for (int i = 0; i < EmitterHandles.Num(); i++)
	{
		FNiagaraEmitterHandle& Handle = EmitterHandles[i];
		if (Handle.GetInstance() == VersionedEmitter)
		{
			KillAllActiveCompilations();
			Modify();
			EmitterHandles[i].GetInstance().Version = NewVersion;
			return true;
		}
	}
	return false;
}

#endif

void UNiagaraSystem::PostInitProperties()
{
	Super::PostInitProperties();
	if (HasAnyFlags(RF_ClassDefaultObject | RF_NeedLoad) == false)
	{
		SystemSpawnScript = NewObject<UNiagaraScript>(this, "SystemSpawnScript", RF_Transactional);
		SystemSpawnScript->SetUsage(ENiagaraScriptUsage::SystemSpawnScript);

		SystemUpdateScript = NewObject<UNiagaraScript>(this, "SystemUpdateScript", RF_Transactional);
		SystemUpdateScript->SetUsage(ENiagaraScriptUsage::SystemUpdateScript);

#if WITH_EDITORONLY_DATA && WITH_EDITOR
		INiagaraModule& NiagaraModule = FModuleManager::GetModuleChecked<INiagaraModule>("Niagara");
		EditorData = NiagaraModule.GetEditorOnlyDataUtilities().CreateDefaultEditorData(this);

		if (EditorParameters == nullptr)
		{
			EditorParameters = NiagaraModule.GetEditorOnlyDataUtilities().CreateDefaultEditorParameters(this);
		}
#endif
	}

	ResolveScalabilitySettings();
	UpdateDITickFlags();
	UpdateHasGPUEmitters();

#if WITH_EDITORONLY_DATA
	if (SystemSpawnScript && SystemSpawnScript->GetLatestSource() != nullptr)
	{
		ensure(SystemSpawnScript->GetLatestSource() == SystemUpdateScript->GetLatestSource());
		SystemSpawnScript->GetLatestSource()->OnChanged().AddUObject(this, &UNiagaraSystem::GraphSourceChanged);
	}
#endif
}

bool UNiagaraSystem::IsLooping() const
{ 
	return false; 
} //sckime todo fix this!

bool UNiagaraSystem::UsesCollection(const UNiagaraParameterCollection* Collection)const
{
	if (SystemSpawnScript->UsesCollection(Collection) ||
		SystemUpdateScript->UsesCollection(Collection))
	{
		return true;
	}

	for (const FNiagaraEmitterHandle& EmitterHandle : GetEmitterHandles())
	{
		FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData();
		if (EmitterData && EmitterData->UsesCollection(Collection))
		{
			return true;
		}
	}

	return false;
}

void UNiagaraSystem::UpdateSystemAfterLoad()
{
	// guard against deadlocks by having wait called on it during the update
	if (bFullyLoaded)
	{
		return;
	}
	bFullyLoaded = true;

#if WITH_EDITORONLY_DATA
	if (SystemSpawnScript && SystemUpdateScript && !GetOutermost()->bIsCookedForEditor)
	{
		SystemSpawnScript->ConditionalPostLoad();
		SystemUpdateScript->ConditionalPostLoad();
		ensure(SystemSpawnScript->GetLatestSource() != nullptr);
		ensure(SystemSpawnScript->GetLatestSource() == SystemUpdateScript->GetLatestSource());
		SystemSpawnScript->GetLatestSource()->OnChanged().AddUObject(this, &UNiagaraSystem::GraphSourceChanged);
	}
#endif

	for (FNiagaraEmitterHandle& EmitterHandle : EmitterHandles)
	{
		if (EmitterHandle.GetInstance().Emitter)
		{
			EmitterHandle.GetInstance().Emitter->UpdateEmitterAfterLoad();
		}
	}

#if WITH_EDITORONLY_DATA

	// We remove emitters and scripts on dedicated servers (and platforms which don't use AV data), so skip further work.
	const bool bIsDedicatedServer = !GIsClient && GIsServer;
	const bool bTargetRequiresAvData = WillNeedAudioVisualData();
	if (bIsDedicatedServer || !bTargetRequiresAvData)
	{
		ResetToEmptySystem();
		return;
	}

	TArray<UNiagaraScript*> AllSystemScripts;
	if (!GetOutermost()->bIsCookedForEditor)
	{
		UNiagaraScriptSourceBase* SystemScriptSource;
		if (SystemSpawnScript == nullptr)
		{
			SystemSpawnScript = NewObject<UNiagaraScript>(this, "SystemSpawnScript", RF_Transactional);
			SystemSpawnScript->SetUsage(ENiagaraScriptUsage::SystemSpawnScript);
			INiagaraModule& NiagaraModule = FModuleManager::GetModuleChecked<INiagaraModule>("Niagara");
			SystemScriptSource = NiagaraModule.GetEditorOnlyDataUtilities().CreateDefaultScriptSource(this);
			SystemSpawnScript->SetLatestSource(SystemScriptSource);
		}
		else
		{
			SystemSpawnScript->ConditionalPostLoad();
			SystemScriptSource = SystemSpawnScript->GetLatestSource();
		}
		AllSystemScripts.Add(SystemSpawnScript);

		if (SystemUpdateScript == nullptr)
		{
			SystemUpdateScript = NewObject<UNiagaraScript>(this, "SystemUpdateScript", RF_Transactional);
			SystemUpdateScript->SetUsage(ENiagaraScriptUsage::SystemUpdateScript);
			SystemUpdateScript->SetLatestSource(SystemScriptSource);
		}
		else
		{
			SystemUpdateScript->ConditionalPostLoad();
		}
		AllSystemScripts.Add(SystemUpdateScript);

		// Synchronize with parameter definitions
		PostLoadDefinitionsSubscriptions();

#if 0
		UE_LOG(LogNiagara, Log, TEXT("PreMerger"));
		for (FNiagaraEmitterHandle& EmitterHandle : EmitterHandles)
		{
			UE_LOG(LogNiagara, Log, TEXT("Emitter Handle: %s"), *EmitterHandle.GetUniqueInstanceName());
			UNiagaraScript* UpdateScript = EmitterHandle.GetInstance()->GetScript(ENiagaraScriptUsage::ParticleUpdateScript, FGuid());
			UNiagaraScript* SpawnScript = EmitterHandle.GetInstance()->GetScript(ENiagaraScriptUsage::ParticleSpawnScript, FGuid());
			UE_LOG(LogNiagara, Log, TEXT("Spawn Parameters"));
			SpawnScript->GetVMExecutableData().Parameters.DumpParameters();
			UE_LOG(LogNiagara, Log, TEXT("Spawn RI Parameters"));
			SpawnScript->RapidIterationParameters.DumpParameters();
			UE_LOG(LogNiagara, Log, TEXT("Update Parameters"));
			UpdateScript->GetVMExecutableData().Parameters.DumpParameters();
			UE_LOG(LogNiagara, Log, TEXT("Update RI Parameters"));
			UpdateScript->RapidIterationParameters.DumpParameters();
		}
#endif

		if (UNiagaraEmitter::GetForceCompileOnLoad())
		{
			ForceGraphToRecompileOnNextCheck();
			UE_LOG(LogNiagara, Log, TEXT("System %s being rebuilt because UNiagaraEmitter::GetForceCompileOnLoad() == true."), *GetPathName());
		}

		if (EmitterCompiledData.Num() == 0 || EmitterCompiledData[0]->DataSetCompiledData.Variables.Num() == 0)
		{
			InitEmitterCompiledData();
		}

		if (SystemCompiledData.InstanceParamStore.ReadParameterVariables().Num() == 0 ||SystemCompiledData.DataSetCompiledData.Variables.Num() == 0)
		{
			InitSystemCompiledData();
		}

#if 0
		UE_LOG(LogNiagara, Log, TEXT("Before"));
		for (FNiagaraEmitterHandle& EmitterHandle : EmitterHandles)
		{
			UE_LOG(LogNiagara, Log, TEXT("Emitter Handle: %s"), *EmitterHandle.GetUniqueInstanceName());
			UNiagaraScript* UpdateScript = EmitterHandle.GetInstance()->GetScript(ENiagaraScriptUsage::ParticleUpdateScript, FGuid());
			UNiagaraScript* SpawnScript = EmitterHandle.GetInstance()->GetScript(ENiagaraScriptUsage::ParticleSpawnScript, FGuid());
			UE_LOG(LogNiagara, Log, TEXT("Spawn Parameters"));
			SpawnScript->GetVMExecutableData().Parameters.DumpParameters();
			UE_LOG(LogNiagara, Log, TEXT("Spawn RI Parameters"));
			SpawnScript->RapidIterationParameters.DumpParameters();
			UE_LOG(LogNiagara, Log, TEXT("Update Parameters"));
			UpdateScript->GetVMExecutableData().Parameters.DumpParameters();
			UE_LOG(LogNiagara, Log, TEXT("Update RI Parameters"));
			UpdateScript->RapidIterationParameters.DumpParameters();
		}
		
		UE_LOG(LogNiagara, Log, TEXT("After"));
		for (FNiagaraEmitterHandle& EmitterHandle : EmitterHandles)
		{
			UE_LOG(LogNiagara, Log, TEXT("Emitter Handle: %s"), *EmitterHandle.GetUniqueInstanceName());
			UNiagaraScript* UpdateScript = EmitterHandle.GetInstance()->GetScript(ENiagaraScriptUsage::ParticleUpdateScript, FGuid());
			UNiagaraScript* SpawnScript = EmitterHandle.GetInstance()->GetScript(ENiagaraScriptUsage::ParticleSpawnScript, FGuid());
			UE_LOG(LogNiagara, Log, TEXT("Spawn Parameters"));
			SpawnScript->GetVMExecutableData().Parameters.DumpParameters();
			UE_LOG(LogNiagara, Log, TEXT("Spawn RI Parameters"));
			SpawnScript->RapidIterationParameters.DumpParameters();
			UE_LOG(LogNiagara, Log, TEXT("Update Parameters"));
			UpdateScript->GetVMExecutableData().Parameters.DumpParameters();
			UE_LOG(LogNiagara, Log, TEXT("Update RI Parameters"));
			UpdateScript->RapidIterationParameters.DumpParameters();
		}
#endif
	}
#endif

	if ( FPlatformProperties::RequiresCookedData() )
	{
		bIsValidCached = IsValidInternal();
		bIsReadyToRunCached = IsReadyToRunInternal();
	}

	ResolveScalabilitySettings();

	ComputeEmittersExecutionOrder();

	ComputeRenderersDrawOrder();

	CacheFromCompiledData();

	//TODO: Move to serialized properties?
	UpdateDITickFlags();
	UpdateHasGPUEmitters();

	// Run task to prime pools this must happen on the GameThread
	if (PoolPrimeSize > 0 && MaxPoolSize > 0)
	{
		FNiagaraWorldManager::PrimePoolForAllWorlds(this);
	}

#if WITH_EDITORONLY_DATA
	// check if the system needs to be compiled and start a compile task if necessary
	if (GetOutermost()->HasAnyPackageFlags(EPackageFlags::PKG_Cooked) == false)
	{
		bool bSystemScriptsAreSynchronized = true;
		for (UNiagaraScript* SystemScript : AllSystemScripts)
		{
			bSystemScriptsAreSynchronized &= SystemScript->AreScriptAndSourceSynchronized();
		}

		bool bEmitterScriptsAreSynchronized = true;
		for (FNiagaraEmitterHandle& EmitterHandle : EmitterHandles)
		{
			if (EmitterHandle.GetIsEnabled() && EmitterHandle.GetEmitterData() && !EmitterHandle.GetEmitterData()->AreAllScriptAndSourcesSynchronized())
			{
				bEmitterScriptsAreSynchronized = false;
			}
		}

		if (bSystemScriptsAreSynchronized == false && GEnableVerboseNiagaraChangeIdLogging)
		{
			UE_LOG(LogNiagara, Log, TEXT("System %s being compiled because there were changes to a system script Change ID."), *GetPathName());
		}

		if (bEmitterScriptsAreSynchronized == false && GEnableVerboseNiagaraChangeIdLogging)
		{
			UE_LOG(LogNiagara, Log, TEXT("System %s being compiled because there were changes to an emitter script Change ID."), *GetPathName());
		}

		if (bSystemScriptsAreSynchronized == false || bEmitterScriptsAreSynchronized == false)
		{
			if (IsRunningCommandlet())
			{
				// Call modify here so that the system will resave the compile ids and script vm when running the resave
				// commandlet. We don't need it for normal post-loading.
				Modify();
			}
			RequestCompile(false);
		}
	}
#endif
}

#if WITH_EDITORONLY_DATA

void UNiagaraSystem::GraphSourceChanged()
{
	InvalidateCachedData();
}

bool UNiagaraSystem::UsesScript(const UNiagaraScript* Script) const
{
	EnsureFullyLoaded();
	if (SystemSpawnScript == Script ||
		SystemUpdateScript == Script)
	{
		return true;
	}

	for (const FNiagaraEmitterHandle& EmitterHandle : GetEmitterHandles())
	{
		FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData();
		if (EmitterData && EmitterData->UsesScript(Script))
		{
			return true;
		}
	}
	
	return false;
}

bool UNiagaraSystem::UsesEmitter(UNiagaraEmitter* Emitter) const
{
	if (!Emitter)
	{
		return false;
	}
	for (const FNiagaraEmitterHandle& EmitterHandle : GetEmitterHandles())
	{
		if (EmitterHandle.UsesEmitter(*Emitter))
		{
			return true;
		}
	}
	return false;
}

bool UNiagaraSystem::UsesEmitter(const FVersionedNiagaraEmitter& VersionedEmitter) const
{
	if (!VersionedEmitter.Emitter)
	{
		return false;
	}
	for (const FNiagaraEmitterHandle& EmitterHandle : GetEmitterHandles())
	{
		if (EmitterHandle.UsesEmitter(VersionedEmitter))
		{
			return true;
		}
	}
	return false;
}

void UNiagaraSystem::RequestCompileForEmitter(const FVersionedNiagaraEmitter& InEmitter)
{
	for (TObjectIterator<UNiagaraSystem> It; It; ++It)
	{
		UNiagaraSystem* Sys = *It;
		if (Sys && Sys->UsesEmitter(InEmitter))
		{
			Sys->RequestCompile(false);
		}
	}
}

void UNiagaraSystem::RecomputeExecutionOrderForEmitter(const FVersionedNiagaraEmitter& InEmitter)
{
	for (TObjectIterator<UNiagaraSystem> It; It; ++It)
	{
		UNiagaraSystem* Sys = *It;
		if (Sys && Sys->UsesEmitter(InEmitter))
		{
			Sys->ComputeEmittersExecutionOrder();
		}
	}
}

void UNiagaraSystem::RecomputeExecutionOrderForDataInterface(class UNiagaraDataInterface* DataInterface)
{
	if (UNiagaraEmitter* Emitter = DataInterface->GetTypedOuter<UNiagaraEmitter>())
	{
		RecomputeExecutionOrderForEmitter(FVersionedNiagaraEmitter(Emitter, Emitter->GetExposedVersion().VersionGuid));
	}
	else
	{
		// In theory we should never hit this, but just incase let's handle it
		for (TObjectIterator<UNiagaraSystem> It; It; ++It)
		{
			if ( UNiagaraSystem* Sys = *It )
			{
				Sys->ComputeEmittersExecutionOrder();
			}
		}
	}
}

#endif

void UNiagaraSystem::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FNiagaraCustomVersion::GUID);
	if (Ar.CustomVer(FNiagaraCustomVersion::GUID) < FNiagaraCustomVersion::ChangeSystemDeterministicDefault)
	{
		bDeterminism = true;
	}

	Super::Serialize(Ar);

	if (Ar.CustomVer(FNiagaraCustomVersion::GUID) >= FNiagaraCustomVersion::ChangeEmitterCompiledDataToSharedRefs)
	{
		UScriptStruct* NiagaraEmitterCompiledDataStruct = FNiagaraEmitterCompiledData::StaticStruct();

		int32 EmitterCompiledDataNum = 0;
		if (Ar.IsSaving())
		{
			EmitterCompiledDataNum = EmitterCompiledData.Num();
		}
		Ar << EmitterCompiledDataNum;

		if (Ar.IsLoading())
		{
			// Clear out EmitterCompiledData when loading or else we will end up with duplicate entries. 
			EmitterCompiledData.Reset();
		}
		for (int32 EmitterIndex = 0; EmitterIndex < EmitterCompiledDataNum; ++EmitterIndex)
		{
			if (Ar.IsLoading())
			{
				EmitterCompiledData.Add(MakeShared<FNiagaraEmitterCompiledData>());
			}

			NiagaraEmitterCompiledDataStruct->SerializeTaggedProperties(Ar, (uint8*)&ConstCastSharedRef<FNiagaraEmitterCompiledData>(EmitterCompiledData[EmitterIndex]).Get(), NiagaraEmitterCompiledDataStruct, nullptr);
		}
	}
}

void UNiagaraSystem::ResolveWarmupTickCount()
{
	//Set the WarmupTickCount to feed back to the user.
	if (FMath::IsNearlyZero(WarmupTickDelta))
	{
		WarmupTickDelta = 0.0f;
		WarmupTickCount = 0;
	}
	else
	{
		WarmupTickCount = WarmupTime / WarmupTickDelta;
		WarmupTime = WarmupTickDelta * WarmupTickCount;
	}
}

#if WITH_EDITOR

void UNiagaraSystem::PreEditChange(FProperty* PropertyThatWillChange)
{
	Super::PreEditChange(PropertyThatWillChange);

	if (PropertyThatWillChange && PropertyThatWillChange->GetFName() == GET_MEMBER_NAME_CHECKED(UNiagaraSystem, EffectType))
	{
		UpdateContext.SetDestroyOnAdd(true);
		UpdateContext.Add(this, false);
	}
}

void UNiagaraSystem::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property != nullptr)
	{
		if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UNiagaraSystem, WarmupTickCount))
		{
			//Set the WarmupTime to feed back to the user.
			WarmupTime = WarmupTickCount * WarmupTickDelta;
			ResolveWarmupTickCount();
		}
		else if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UNiagaraSystem, WarmupTime))
		{
			ResolveWarmupTickCount();
		}
		else if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UNiagaraSystem, WarmupTickDelta))
		{
			ResolveWarmupTickCount();
		}
	}
	else
	{
		// User parameter values may have changed off of Undo/Redo, which calls this with a nullptr, so we need to propagate those. 
		// The editor may no longer be open, so we should do this within the system to properly propagate.
		ExposedParameters.PostGenericEditChange();
	}

	UpdateDITickFlags();
	UpdateHasGPUEmitters();
	ResolveScalabilitySettings();
	OnScalabilityChanged().Broadcast();
	
	UpdateContext.CommitUpdate();

	static FName SkipReset = TEXT("SkipSystemResetOnChange");
	bool bPropertyHasSkip = PropertyChangedEvent.Property && PropertyChangedEvent.Property->HasMetaData(SkipReset);
	bool bMemberHasSkip = PropertyChangedEvent.MemberProperty && PropertyChangedEvent.MemberProperty->HasMetaData(SkipReset);
	if (!bPropertyHasSkip && !bMemberHasSkip)
	{
		OnSystemPostEditChangeDelegate.Broadcast(this);
	}
}
#endif 

void UNiagaraSystem::PostLoad()
{
	Super::PostLoad();

	// Workaround for UE-104235 where a CDO loads a NiagaraSystem before the NiagaraModule has had a chance to load
	// We force the module to load here we makes sure the type registry, etc, is all setup in time.
	static bool bLoadChecked = false;
	if ( !bLoadChecked )
	{
		// We don't implement IsPostLoadThreadSafe so should be on the GT, but let's not assume.
		if ( ensure(IsInGameThread()) )
		{
			bLoadChecked = true;
			FModuleManager::LoadModuleChecked<INiagaraModule>("Niagara");
		}
	}

	ExposedParameters.PostLoad();
	ExposedParameters.SanityCheckData();

	SystemCompiledData.InstanceParamStore.PostLoad();

	if (GIsEditor)
	{
		SetFlags(RF_Transactional);
	}

	// Previously added emitters didn't have their stand alone and public flags cleared so
	// they 'leak' into the system package.  Clear the flags here so they can be collected
	// during the next save.
	UPackage* PackageOuter = Cast<UPackage>(GetOuter());
	if (PackageOuter != nullptr && HasAnyFlags(RF_Public | RF_Standalone))
	{
		TArray<UObject*> ObjectsInPackage;
		GetObjectsWithPackage(PackageOuter, ObjectsInPackage);
		for (UObject* ObjectInPackage : ObjectsInPackage)
		{
			UNiagaraEmitter* Emitter = Cast<UNiagaraEmitter>(ObjectInPackage);
			if (Emitter != nullptr)
			{
				Emitter->ConditionalPostLoad();
				Emitter->ClearFlags(RF_Standalone | RF_Public);
			}
		}
	}

	const int32 NiagaraVer = GetLinkerCustomVersion(FNiagaraCustomVersion::GUID);
	if (NiagaraVer < FNiagaraCustomVersion::PlatformScalingRefactor)
	{
		for (int32 DL = 0; DL < ScalabilityOverrides_DEPRECATED.Num(); ++DL)
		{
			FNiagaraSystemScalabilityOverride& LegacyOverride = ScalabilityOverrides_DEPRECATED[DL];
			FNiagaraSystemScalabilityOverride& NewOverride = SystemScalabilityOverrides.Overrides.AddDefaulted_GetRef();
			NewOverride = LegacyOverride;
			NewOverride.Platforms = FNiagaraPlatformSet(FNiagaraPlatformSet::CreateQualityLevelMask(DL));
		}
	}

	for (FNiagaraSystemScalabilityOverride& Override : SystemScalabilityOverrides.Overrides)
	{
		Override.PostLoad(NiagaraVer);
	}

#if UE_EDITOR
	ExposedParameters.RecreateRedirections();
#endif

	for (FNiagaraEmitterHandle& EmitterHandle : EmitterHandles)
	{
#if WITH_EDITORONLY_DATA
		EmitterHandle.ConditionalPostLoad(NiagaraVer);
#else
		if (UNiagaraEmitter* NiagaraEmitter = EmitterHandle.GetInstance().Emitter)
		{
			NiagaraEmitter->ConditionalPostLoad();
		}
#endif
	}

#if WITH_EDITORONLY_DATA
	FixupPositionUserParameters();
	
	if (EditorData == nullptr)
	{
		INiagaraModule& NiagaraModule = FModuleManager::GetModuleChecked<INiagaraModule>("Niagara");
		EditorData = NiagaraModule.GetEditorOnlyDataUtilities().CreateDefaultEditorData(this);
	}
	else
	{
		EditorData->PostLoadFromOwner(this);
	}

	if (EditorParameters == nullptr)
	{
		INiagaraModule& NiagaraModule = FModuleManager::GetModuleChecked<INiagaraModule>("Niagara");
		EditorParameters = NiagaraModule.GetEditorOnlyDataUtilities().CreateDefaultEditorParameters(this);
	}

	// see the equivalent in NiagaraEmitter for details
	if (bIsTemplateAsset_DEPRECATED)
	{
		TemplateSpecification = bIsTemplateAsset_DEPRECATED ? ENiagaraScriptTemplateSpecification::Template : ENiagaraScriptTemplateSpecification::None;
	}
	
	if(bExposeToLibrary_DEPRECATED)
	{
		LibraryVisibility = ENiagaraScriptLibraryVisibility::Unexposed;
	}
#endif // WITH_EDITORONLY_DATA

	//Apply platform set redirectors
	auto ApplyPlatformSetRedirects = [](FNiagaraPlatformSet& Platforms)
	{
		Platforms.ApplyRedirects();
	};
	ForEachPlatformSet(ApplyPlatformSetRedirects);

#if !WITH_EDITOR
	// When running without the editor in a cooked build we run the update immediately in post load since
	// there will be no merging or compiling which makes it safe to do so.
	UpdateSystemAfterLoad();
#endif

#if WITH_EDITORONLY_DATA
	// see the equivalent in NiagaraEmitter for details
	if(bIsTemplateAsset_DEPRECATED)
	{
		TemplateSpecification = bIsTemplateAsset_DEPRECATED ? ENiagaraScriptTemplateSpecification::Template : ENiagaraScriptTemplateSpecification::None;
	}
#endif // WITH_EDITORONLY_DATA

	PrecachePSOs();
}

#if WITH_EDITORONLY_DATA
void UNiagaraSystem::DeclareConstructClasses(TArray<FTopLevelAssetPath>& OutConstructClasses, const UClass* SpecificSubclass)
{
	Super::DeclareConstructClasses(OutConstructClasses, SpecificSubclass);
	OutConstructClasses.Add(FTopLevelAssetPath(UNiagaraScratchPadContainer::StaticClass()));
	OutConstructClasses.Add(FTopLevelAssetPath(TEXT("/Script/NiagaraEditor.NiagaraSystemEditorData")));
	OutConstructClasses.Add(FTopLevelAssetPath(TEXT("/Script/NiagaraEditor.NiagaraEditorParametersAdapter")));
}
#endif

void UNiagaraSystem::PrecachePSOs()
{
	if (!IsComponentPSOPrecachingEnabled() && !IsResourcePSOPrecachingEnabled())
	{
		return;
	}

	struct VFsPerMaterialData
	{
		UMaterialInterface* MaterialInterface;
		bool bDisableBackfaceCulling;
		TArray<const FVertexFactoryType*, TInlineAllocator<2>> VertexFactoryTypes;
	};
	TArray<VFsPerMaterialData, TInlineAllocator<2>> VFsPerMaterials;

	for (const FNiagaraEmitterHandle& EmitterHandle : GetEmitterHandles())
	{
		FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData();
		if (EmitterData && EmitterHandle.GetIsEnabled())
		{
			EmitterData->ForEachEnabledRenderer(
				[&](UNiagaraRendererProperties* Properties)
				{
					const FVertexFactoryType* VFType = Properties->GetVertexFactoryType();
					if (VFType == nullptr)
					{
						return;
					}
					bool bDisableBackfaceCulling = Properties->IsBackfaceCullingDisabled();

					// Don't have an instance yet to retrieve the possible material override data from
					const FNiagaraEmitterInstance* EmitterInstance = nullptr;
					TArray<UMaterialInterface*> Mats;
					Properties->GetUsedMaterials(EmitterInstance, Mats);

					for (UMaterialInterface* MaterialInterface : Mats)
					{
						VFsPerMaterialData* VFsPerMaterial = VFsPerMaterials.FindByPredicate([MaterialInterface, bDisableBackfaceCulling](const VFsPerMaterialData& Other) 
						{ 
							return (Other.MaterialInterface == MaterialInterface &&
									Other.bDisableBackfaceCulling == bDisableBackfaceCulling);
						});
						if (VFsPerMaterial == nullptr)
						{
							VFsPerMaterial = &VFsPerMaterials.AddDefaulted_GetRef();
							VFsPerMaterial->MaterialInterface = MaterialInterface;
							VFsPerMaterial->bDisableBackfaceCulling = bDisableBackfaceCulling;
						}
						VFsPerMaterial->VertexFactoryTypes.AddUnique(VFType);
					}
				}
			);
		}
	}

	FPSOPrecacheParams PreCachePSOParams;
	PreCachePSOParams.SetMobility(EComponentMobility::Movable);

	for (VFsPerMaterialData& VFsPerMaterial : VFsPerMaterials)
	{
		if (VFsPerMaterial.MaterialInterface)
		{
			PreCachePSOParams.bDisableBackFaceCulling = VFsPerMaterial.bDisableBackfaceCulling;
			VFsPerMaterial.MaterialInterface->PrecachePSOs(VFsPerMaterial.VertexFactoryTypes, PreCachePSOParams);
		}
	}
}


#if WITH_EDITORONLY_DATA

UNiagaraEditorDataBase* UNiagaraSystem::GetEditorData()
{
	return EditorData;
}

const UNiagaraEditorDataBase* UNiagaraSystem::GetEditorData() const
{
	return EditorData;
}

UNiagaraEditorParametersAdapterBase* UNiagaraSystem::GetEditorParameters()
{
	return EditorParameters;
}

bool UNiagaraSystem::ReferencesInstanceEmitter(const FVersionedNiagaraEmitter& Emitter) const
{
	if (Emitter.Emitter == nullptr)
	{
		return false;
	}

	for (const FNiagaraEmitterHandle& Handle : EmitterHandles)
	{
		if (Emitter == Handle.GetInstance())
		{
			return true;
		}
	}
	return false;
}

void UNiagaraSystem::RefreshSystemParametersFromEmitter(const FNiagaraEmitterHandle& EmitterHandle)
{
	InitEmitterCompiledData();
	if (ensureMsgf(EmitterHandles.ContainsByPredicate([=](const FNiagaraEmitterHandle& OwnedEmitterHandle) { return OwnedEmitterHandle.GetId() == EmitterHandle.GetId(); }),
		TEXT("Can't refresh parameters from an emitter handle this system doesn't own.")))
	{
		if (FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData())
		{
			EmitterData->EmitterSpawnScriptProps.Script->RapidIterationParameters.CopyParametersTo(SystemSpawnScript->RapidIterationParameters, false, FNiagaraParameterStore::EDataInterfaceCopyMethod::None);
			EmitterData->EmitterUpdateScriptProps.Script->RapidIterationParameters.CopyParametersTo(SystemUpdateScript->RapidIterationParameters, false, FNiagaraParameterStore::EDataInterfaceCopyMethod::None);
		}
	}
}

void UNiagaraSystem::RemoveSystemParametersForEmitter(const FNiagaraEmitterHandle& EmitterHandle)
{
	InitEmitterCompiledData();
	if (ensureMsgf(EmitterHandles.ContainsByPredicate([=](const FNiagaraEmitterHandle& OwnedEmitterHandle) { return OwnedEmitterHandle.GetId() == EmitterHandle.GetId(); }),
		TEXT("Can't remove parameters for an emitter handle this system doesn't own.")))
	{
		if (FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData())
		{
			EmitterData->EmitterSpawnScriptProps.Script->RapidIterationParameters.RemoveParameters(SystemSpawnScript->RapidIterationParameters);
			EmitterData->EmitterUpdateScriptProps.Script->RapidIterationParameters.RemoveParameters(SystemUpdateScript->RapidIterationParameters);
		}
	}
}
#endif


TArray<FNiagaraEmitterHandle>& UNiagaraSystem::GetEmitterHandles()
{
	return EmitterHandles;
}

const TArray<FNiagaraEmitterHandle>& UNiagaraSystem::GetEmitterHandles()const
{
	return EmitterHandles;
}

bool UNiagaraSystem::AllowScalabilityForLocalPlayerFX()const
{
	return bAllowCullingForLocalPlayers;
}

bool UNiagaraSystem::IsReadyToRunInternal() const
{
	//TODO: Ideally we'd never even load Niagara assets on the server but this is a larger issue. Tracked in FORT-342580
	if (!FApp::CanEverRender())
	{
		return false;
	}

	EnsureFullyLoaded();
	if (!SystemSpawnScript || !SystemUpdateScript)
	{
		if (FPlatformProperties::RequiresCookedData())
		{
			UE_LOG(LogNiagara, Warning, TEXT("%s IsReadyToRunInternal() failed due to missing SystemScript.  Spawn[%s] Update[%s]"),
				*GetFullName(),
				SystemSpawnScript ? *SystemSpawnScript->GetName() : TEXT("<none>"),
				SystemUpdateScript ? *SystemUpdateScript->GetName() : TEXT("<none>"));
		}

		return false;
	}

#if WITH_EDITORONLY_DATA
	if (HasOutstandingCompilationRequests())
	{
		return false;
	}

	/* Check that our post compile data is in sync with the current emitter handles count. If we have just added a new emitter handle, we will not have any outstanding compilation requests as the new compile
	 * will not be added to the outstanding compilation requests until the next tick.
	 */
	if (EmitterHandles.Num() != EmitterCompiledData.Num())
	{
		return false;
	}
#endif

	if (SystemSpawnScript->IsScriptCompilationPending(false) || 
		SystemUpdateScript->IsScriptCompilationPending(false))
	{
		return false;
	}

	const int32 EmitterCount = EmitterHandles.Num();
	for (int32 EmitterIt = 0; EmitterIt < EmitterCount; ++EmitterIt)
	{
		const FNiagaraEmitterHandle& Handle = EmitterHandles[EmitterIt];
		FVersionedNiagaraEmitterData* EmitterData = Handle.GetEmitterData();
		if (EmitterData && !EmitterData->IsReadyToRun())
		{
			if (FPlatformProperties::RequiresCookedData())
			{
				UE_LOG(LogNiagara, Warning, TEXT("%s IsReadyToRunInternal() failed due to Emitter not being ready to run.  Emitter #%d - %s"),
					*GetFullName(),
					EmitterIt,
					Handle.GetInstance().Emitter ? *Handle.GetInstance().Emitter->GetUniqueEmitterName() : TEXT("<none>"));
			}

			return false;
		}
	}

	// SystemSpawnScript and SystemUpdateScript needs to agree on the attributes of the datasets
	// Outside of DDC weirdness it's unclear how they can get out of sync, but this is a precaution to make sure that mismatched scripts won't run
	if (SystemSpawnScript->GetVMExecutableData().Attributes != SystemUpdateScript->GetVMExecutableData().Attributes)
	{
		if (FPlatformProperties::RequiresCookedData())
		{
			UE_LOG(LogNiagara, Warning, TEXT("%s IsReadyToRunInternal() failed due to mismatch between System spawn and update script attributes."), *GetFullName());
		}

		return false;
	}

	return true;
}

void UNiagaraSystem::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	if (HasAnyFlags(RF_ClassDefaultObject) == false)
	{
		EnsureFullyLoaded();
	}

#if WITH_EDITOR
	OutTags.Add(FAssetRegistryTag("HasGPUEmitter", HasAnyGPUEmitters() ? TEXT("True") : TEXT("False"), FAssetRegistryTag::TT_Alphabetical));

	const float BoundsSize = FixedBounds.GetSize().GetMax();
	OutTags.Add(FAssetRegistryTag("FixedBoundsSize", bFixedBounds ? FString::Printf(TEXT("%.2f"), BoundsSize) : FString(TEXT("None")), FAssetRegistryTag::TT_Numerical));

	OutTags.Add(FAssetRegistryTag("NumEmitters", LexToString(EmitterHandles.Num()), FAssetRegistryTag::TT_Numerical));

	uint32 GPUSimsMissingFixedBounds = 0;

	// Gather up generic NumActive values
	uint32 NumActiveEmitters = 0;
	uint32 NumActiveRenderers = 0;
	TArray<const UNiagaraRendererProperties*> ActiveRenderers;
	for (const FNiagaraEmitterHandle& Handle : EmitterHandles)
	{
		if (Handle.GetIsEnabled())
		{

			NumActiveEmitters++;
			if (FVersionedNiagaraEmitterData* EmitterData = Handle.GetEmitterData())
			{
				// Only register fixed bounds requirement for GPU if the system itself isn't fixed bounds.
				if (bFixedBounds == false && EmitterData->CalculateBoundsMode == ENiagaraEmitterCalculateBoundMode::Dynamic && EmitterData->SimTarget == ENiagaraSimTarget::GPUComputeSim)
				{
					GPUSimsMissingFixedBounds++;
				}

				for (const UNiagaraRendererProperties* Props : EmitterData->GetRenderers())
				{
					if (Props)
					{
						NumActiveRenderers++;
						ActiveRenderers.Add(Props);
					}
				}
			}
		}
	}

	OutTags.Add(FAssetRegistryTag("ActiveEmitters", LexToString(NumActiveEmitters), FAssetRegistryTag::TT_Numerical));
	OutTags.Add(FAssetRegistryTag("ActiveRenderers", LexToString(NumActiveRenderers), FAssetRegistryTag::TT_Numerical));
	OutTags.Add(FAssetRegistryTag("GPUSimsMissingFixedBounds", LexToString(GPUSimsMissingFixedBounds), FAssetRegistryTag::TT_Numerical));
	OutTags.Add(FAssetRegistryTag("EffectType", EffectType != nullptr ? EffectType->GetName() : FString(TEXT("None")), FAssetRegistryTag::TT_Alphabetical));
	OutTags.Add(FAssetRegistryTag("WarmupTime", LexToString(WarmupTime), FAssetRegistryTag::TT_Numerical));
	OutTags.Add(FAssetRegistryTag("HasOverrideScalabilityForSystem", bOverrideScalabilitySettings ? TEXT("True") : TEXT("False"), FAssetRegistryTag::TT_Alphabetical));
	OutTags.Add(FAssetRegistryTag("HasDIsWithPostSimulateTick", bHasDIsWithPostSimulateTick ? TEXT("True") : TEXT("False"), FAssetRegistryTag::TT_Alphabetical));
	OutTags.Add(FAssetRegistryTag("NeedsSortedSignificanceCull", bNeedsSortedSignificanceCull ? TEXT("True") : TEXT("False"), FAssetRegistryTag::TT_Alphabetical));

	// Gather up NumActive emitters based off of quality level.
	const UNiagaraSettings* Settings = GetDefault<UNiagaraSettings>();
	if (Settings)
	{
		int32 NumQualityLevels = Settings->QualityLevels.Num();
		TArray<int32> QualityLevelsNumActive;
		QualityLevelsNumActive.AddZeroed(NumQualityLevels);

		for (const FNiagaraEmitterHandle& Handle : EmitterHandles)
		{
			if (Handle.GetIsEnabled())
			{
				FVersionedNiagaraEmitter Emitter = Handle.GetInstance();
				FVersionedNiagaraEmitterData* EmitterData = Emitter.GetEmitterData();
				if (EmitterData)
				{
					for (int32 i = 0; i < NumQualityLevels; i++)
					{
						if (EmitterData->Platforms.IsEffectQualityEnabled(i))
						{
							QualityLevelsNumActive[i]++;
						}
					}
				}
			}
		}

		for (int32 i = 0; i < NumQualityLevels; i++)
		{
			FString QualityLevelKey = Settings->QualityLevels[i].ToString() + TEXT("Emitters");
			OutTags.Add(FAssetRegistryTag(*QualityLevelKey, LexToString(QualityLevelsNumActive[i]), FAssetRegistryTag::TT_Numerical));
		}
	}


	TMap<FName, uint32> NumericKeys;
	TMap<FName, FString> StringKeys;

	// Gather up custom asset tags for  RendererProperties
	{
		TArray<UClass*> RendererClasses;
		GetDerivedClasses(UNiagaraRendererProperties::StaticClass(), RendererClasses);

		for (UClass* RendererClass : RendererClasses)
		{
			const UNiagaraRendererProperties* PropDefault = RendererClass->GetDefaultObject< UNiagaraRendererProperties>();
			if (PropDefault)
			{
				PropDefault->GetAssetTagsForContext(this, FGuid(), ActiveRenderers, NumericKeys, StringKeys);
			}
		}
	}

	// Gather up custom asset tags for DataInterfaces
	{
		TArray<const UNiagaraDataInterface*> DataInterfaces;
		auto AddDIs = [&](UNiagaraScript* Script)
		{
			if (Script)
			{
				for (FNiagaraScriptDataInterfaceCompileInfo& Info : Script->GetVMExecutableData().DataInterfaceInfo)
				{
					UNiagaraDataInterface* DefaultDataInterface = Info.GetDefaultDataInterface();
					DataInterfaces.AddUnique(DefaultDataInterface);
				}
			}
		};

		AddDIs(SystemSpawnScript);
		AddDIs(SystemUpdateScript);
		for (const FNiagaraEmitterHandle& Handle : EmitterHandles)
		{
			if (Handle.GetIsEnabled())
			{
				if (FVersionedNiagaraEmitterData* EmitterData = Handle.GetEmitterData())
				{
					TArray<UNiagaraScript*> Scripts;
					EmitterData->GetScripts(Scripts);
					for (UNiagaraScript* Script : Scripts)
					{
						AddDIs(Script);
					}
				}
			}
		}

		TArray<UClass*> DIClasses;
		GetDerivedClasses(UNiagaraDataInterface::StaticClass(), DIClasses);

		for (UClass* DIClass : DIClasses)
		{
			const UNiagaraDataInterface* PropDefault = DIClass->GetDefaultObject< UNiagaraDataInterface>();
			if (PropDefault)
			{
				PropDefault->GetAssetTagsForContext(this, FGuid(), DataInterfaces, NumericKeys, StringKeys);
			}
		}
		OutTags.Add(FAssetRegistryTag("ActiveDIs", LexToString(DataInterfaces.Num()), FAssetRegistryTag::TT_Numerical));
	}


	// Now propagate the custom numeric and string tags from the DataInterfaces and RendererProperties above
	auto NumericIter = NumericKeys.CreateConstIterator();
	while (NumericIter)
	{

		OutTags.Add(FAssetRegistryTag(NumericIter.Key(), LexToString(NumericIter.Value()), FAssetRegistryTag::TT_Numerical));
		++NumericIter;
	}

	auto StringIter = StringKeys.CreateConstIterator();
	while (StringIter)
	{

		OutTags.Add(FAssetRegistryTag(StringIter.Key(), LexToString(StringIter.Value()), FAssetRegistryTag::TT_Alphabetical));
		++StringIter;
	}

	// TemplateSpecialization
	FName TemplateSpecificationName = GET_MEMBER_NAME_CHECKED(UNiagaraSystem, TemplateSpecification);
	FText TemplateSpecializationValueString = StaticEnum<ENiagaraScriptTemplateSpecification>()->GetDisplayNameTextByValue((int64) TemplateSpecification);
	OutTags.Add(FAssetRegistryTag(TemplateSpecificationName, TemplateSpecializationValueString.ToString(), FAssetRegistryTag::TT_Alphabetical));

	/*for (const UNiagaraDataInterface* DI : DataInterfaces)
	{
		FString ClassName;
		DI->GetClass()->GetName(ClassName);
		OutTags.Add(FAssetRegistryTag(*(TEXT("bHas")+ClassName), TEXT("True"), FAssetRegistryTag::TT_Alphabetical));
	}*/


	//OutTags.Add(FAssetRegistryTag("CPUCollision", UsesCPUCollision() ? TEXT("True") : TEXT("False"), FAssetRegistryTag::TT_Alphabetical));
	//OutTags.Add(FAssetRegistryTag("Looping", bAnyEmitterLoopsForever ? TEXT("True") : TEXT("False"), FAssetRegistryTag::TT_Alphabetical));
	//OutTags.Add(FAssetRegistryTag("Immortal", IsImmortal() ? TEXT("True") : TEXT("False"), FAssetRegistryTag::TT_Alphabetical));
	//OutTags.Add(FAssetRegistryTag("Becomes Zombie", WillBecomeZombie() ? TEXT("True") : TEXT("False"), FAssetRegistryTag::TT_Alphabetical));
	//OutTags.Add(FAssetRegistryTag("CanBeOccluded", OcclusionBoundsMethod == EParticleSystemOcclusionBoundsMethod::EPSOBM_None ? TEXT("False") : TEXT("True"), FAssetRegistryTag::TT_Alphabetical));

#endif
	Super::GetAssetRegistryTags(OutTags);
}

#if WITH_EDITORONLY_DATA
bool UNiagaraSystem::HasOutstandingCompilationRequests(bool bIncludingGPUShaders) const
{
	if (ActiveCompilations.Num() > 0)
	{
		return true;
	}

	// the above check only handles the VM script generation, and so GPU compute script compilation can still
	// be underway, so we'll check for that explicitly, only when needed, so that we don't burden the user with excessive compiles
	if (bIncludingGPUShaders)
	{
		for (const FNiagaraEmitterHandle& EmitterHandle : GetEmitterHandles())
		{
			if (FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData())
			{
				if (const UNiagaraScript* GPUComputeScript = EmitterData->GetGPUComputeScript())
				{
					if (const FNiagaraShaderScript* ShaderScript = GPUComputeScript->GetRenderThreadScript())
					{
						if (!ShaderScript->IsCompilationFinished())
						{
							return true;
						}
					}
				}
			}
		}
	}

	return false;
}
#endif

bool UNiagaraSystem::ComputeEmitterPriority(int32 EmitterIdx, TArray<int32, TInlineAllocator<32>>& EmitterPriorities, const TBitArray<TInlineAllocator<32>>& EmitterDependencyGraph)
{
	// Mark this node as being evaluated.
	EmitterPriorities[EmitterIdx] = 0;

	int32 MaxPriority = 0;

	// Examine all the nodes we depend on. We must run after all of them, so our priority
	// will be 1 higher than the maximum priority of all our dependencies.
	const int32 NumEmitters = EmitterHandles.Num();
	int32 DepStartIndex = EmitterIdx * NumEmitters;
	TConstSetBitIterator<TInlineAllocator<32>> DepIt(EmitterDependencyGraph, DepStartIndex);
	while (DepIt.GetIndex() < DepStartIndex + NumEmitters)
	{
		int32 OtherEmitterIdx = DepIt.GetIndex() - DepStartIndex;

		// This can't happen, because we explicitly skip self-dependencies when building the edge table.
		checkSlow(OtherEmitterIdx != EmitterIdx);

		if (EmitterPriorities[OtherEmitterIdx] == 0)
		{
			// This node is currently being evaluated, which means we've found a cycle.
			return false;
		}

		if (EmitterPriorities[OtherEmitterIdx] < 0)
		{
			// Node not evaluated yet, recurse.
			if (!ComputeEmitterPriority(OtherEmitterIdx, EmitterPriorities, EmitterDependencyGraph))
			{
				return false;
			}
		}

		if (MaxPriority < EmitterPriorities[OtherEmitterIdx])
		{
			MaxPriority = EmitterPriorities[OtherEmitterIdx];
		}

		++DepIt;
	}

	EmitterPriorities[EmitterIdx] = MaxPriority + 1;
	return true;
}

void UNiagaraSystem::FindEventDependencies(FVersionedNiagaraEmitterData* EmitterData, TArray<FVersionedNiagaraEmitter>& Dependencies)
{
	if (!EmitterData)
	{
		return;
	}

	const TArray<FNiagaraEventScriptProperties>& EventHandlers = EmitterData->GetEventHandlers();
	for (const FNiagaraEventScriptProperties& Handler : EventHandlers)
	{
		// An empty ID means the event reads from the same emitter, so we don't need to record a dependency.
		if (!Handler.SourceEmitterID.IsValid())
		{
			continue;
		}

		// Look for the ID in the list of emitter handles from the system object.
		FString SourceEmitterIDName = Handler.SourceEmitterID.ToString();
		for (int EmitterIdx = 0; EmitterIdx < EmitterHandles.Num(); ++EmitterIdx)
		{
			FName EmitterIDName = EmitterHandles[EmitterIdx].GetIdName();
			if (EmitterIDName.ToString() == SourceEmitterIDName)
			{
				// The Emitters array is in the same order as the EmitterHandles array.
				Dependencies.Add(EmitterHandles[EmitterIdx].GetInstance());
				break;
			}
		}
	}
}

void UNiagaraSystem::FindDataInterfaceDependencies(FVersionedNiagaraEmitterData* EmitterData, UNiagaraScript* Script, TArray<FVersionedNiagaraEmitter>& Dependencies)
{
	if (const FNiagaraScriptExecutionParameterStore* ParameterStore = Script->GetExecutionReadyParameterStore(EmitterData->SimTarget))
	{
		if (EmitterData->SimTarget == ENiagaraSimTarget::CPUSim)
		{
			for (UNiagaraDataInterface* DataInterface : ParameterStore->GetDataInterfaces())
			{
				DataInterface->GetEmitterDependencies(this, Dependencies);
			}
		}
		else
		{
			const TArray<UNiagaraDataInterface*>& StoreDataInterfaces = ParameterStore->GetDataInterfaces();
			if (StoreDataInterfaces.Num() > 0)
			{
				auto FindCachedDefaultDI =
					[](UNiagaraScript* Script, const FNiagaraVariable& Variable) -> UNiagaraDataInterface*
				{
					if (Script)
					{
						for (FNiagaraScriptDataInterfaceInfo& DataInterfaceInfo : Script->GetCachedDefaultDataInterfaces())
						{
							if ((Variable.GetType() == DataInterfaceInfo.Type) && (Variable.GetName() == DataInterfaceInfo.RegisteredParameterMapWrite))
							{
								return DataInterfaceInfo.DataInterface;
							}
						}
					}
					return nullptr;
				};

				for (const FNiagaraVariableWithOffset& Variable : ParameterStore->ReadParameterVariables())
				{
					if (!Variable.IsDataInterface())
					{
						continue;
					}

					if (UNiagaraDataInterface* DefaultDI = FindCachedDefaultDI(SystemSpawnScript, Variable))
					{
						DefaultDI->GetEmitterDependencies(this, Dependencies);
						continue;
					}

					if (UNiagaraDataInterface* DefaultDI = FindCachedDefaultDI(SystemUpdateScript, Variable))
					{
						DefaultDI->GetEmitterDependencies(this, Dependencies);
						continue;
					}

					StoreDataInterfaces[Variable.Offset]->GetEmitterDependencies(this, Dependencies);
				}
			}
		}
	}
}

void UNiagaraSystem::ComputeEmittersExecutionOrder()
{
	const int32 NumEmitters = EmitterHandles.Num();

	TArray<int32, TInlineAllocator<32>> EmitterPriorities;
	TBitArray<TInlineAllocator<32>> EmitterDependencyGraph;

	EmitterExecutionOrder.SetNum(NumEmitters);
	EmitterPriorities.SetNum(NumEmitters);
	EmitterDependencyGraph.Init(false, NumEmitters * NumEmitters);

	TArray<FVersionedNiagaraEmitter> EmitterDependencies;
	EmitterDependencies.Reserve(3 * NumEmitters);

	RendererPostTickOrder.Reset();
	RendererCompletionOrder.Reset();

	bool bHasEmitterDependencies = false;
	uint32 SystemRendererIndex = 0;
	for (int32 EmitterIdx = 0; EmitterIdx < NumEmitters; ++EmitterIdx)
	{
		const FNiagaraEmitterHandle& EmitterHandle = EmitterHandles[EmitterIdx];
		FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData();

		EmitterExecutionOrder[EmitterIdx].EmitterIndex = EmitterIdx;
		EmitterPriorities[EmitterIdx] = -1;

		if (EmitterData == nullptr)
		{
			continue;
		}

		if (!EmitterHandle.GetIsEnabled())
		{
			EmitterData->ForEachEnabledRenderer([&] (const UNiagaraRendererProperties*) { ++SystemRendererIndex; });
			continue;
		}

		EmitterDependencies.SetNum(0, false);

		if (EmitterData->SimTarget == ENiagaraSimTarget::GPUComputeSim && EmitterData->GetGPUComputeScript())
		{
			// GPU emitters have a combined execution context for spawn and update.
			FindDataInterfaceDependencies(EmitterData, EmitterData->GetGPUComputeScript(), EmitterDependencies);
		}
		else
		{
			// CPU emitters have separate contexts for spawn and update, so we need to gather DIs from both. They also support events,
			// so we need to look at the event sources for extra dependencies.
			FindDataInterfaceDependencies(EmitterData, EmitterData->SpawnScriptProps.Script, EmitterDependencies);
			FindDataInterfaceDependencies(EmitterData, EmitterData->UpdateScriptProps.Script, EmitterDependencies);
			FindEventDependencies(EmitterData, EmitterDependencies);
		}

		// Map the pointers returned by the emitter to indices inside the Emitters array. This is O(N^2), but we expect
		// to have few dependencies, so in practice it should be faster than a TMap. If it gets out of hand, we can also
		// ask the DIs to give us indices directly, since they probably got the pointers by scanning the array we gave them
		// through GetEmitters() anyway.
		for (int32 DepIdx = 0; DepIdx < EmitterDependencies.Num(); ++DepIdx)
		{
			for (int32 OtherEmitterIdx = 0; OtherEmitterIdx < NumEmitters; ++OtherEmitterIdx)
			{
				if (EmitterDependencies[DepIdx] == EmitterHandles[OtherEmitterIdx].GetInstance())
				{
					const bool HasSourceEmitter = EmitterHandles[EmitterIdx].GetInstance().Emitter != nullptr;
					const bool HasDependentEmitter = EmitterHandles[OtherEmitterIdx].GetInstance().Emitter != nullptr;

					// check to see if the emitter we're dependent on may have been culled during the cook
					if (HasSourceEmitter && !HasDependentEmitter)
					{
						UE_LOG(LogNiagara, Error, TEXT("Emitter[%s] depends on Emitter[%s] which is not available (has scalability removed it during a cook?)."),
							*EmitterHandles[EmitterIdx].GetName().ToString(),
							*EmitterHandles[OtherEmitterIdx].GetName().ToString());
					}

					// Some DIs might read from the same emitter they're applied to. We don't care about dependencies on self.
					if (EmitterIdx != OtherEmitterIdx)
					{
						EmitterDependencyGraph.SetRange(EmitterIdx * NumEmitters + OtherEmitterIdx, 1, true);
						bHasEmitterDependencies = true;
					}
					break;
				}
			}
		}

		// Determine renderer execution order for PostTick and Completion for any renderers that opt into it
		for (int32 RendererIndex = 0; RendererIndex < EmitterData->GetRenderers().Num(); ++RendererIndex)
		{
			const UNiagaraRendererProperties* Renderer = EmitterData->GetRenderers()[RendererIndex];
			if (Renderer && Renderer->GetIsEnabled() && Renderer->IsSimTargetSupported(EmitterData->SimTarget))
			{
				FNiagaraRendererExecutionIndex ExecutionIndex;
				ExecutionIndex.EmitterIndex = EmitterIdx;
				ExecutionIndex.EmitterRendererIndex = RendererIndex;
				ExecutionIndex.SystemRendererIndex = SystemRendererIndex;

				if (Renderer->NeedsSystemPostTick())
				{
					RendererPostTickOrder.Add(ExecutionIndex);
				}
				if (Renderer->NeedsSystemCompletion())
				{
					RendererCompletionOrder.Add(ExecutionIndex);
				}
				++SystemRendererIndex;
			}
		}
	}

	if (bHasEmitterDependencies)
	{
		for (int32 EmitterIdx = 0; EmitterIdx < NumEmitters; ++EmitterIdx)
		{
			if (EmitterPriorities[EmitterIdx] < 0)
			{
				if (!ComputeEmitterPriority(EmitterIdx, EmitterPriorities, EmitterDependencyGraph))
				{
					FName EmitterName = EmitterHandles[EmitterIdx].GetName();
					UE_LOG(LogNiagara, Error, TEXT("Found circular dependency involving emitter '%s' in system '%s'. The execution order will be undefined."), *EmitterName.ToString(), *GetName());
					break;
				}
			}
		}

		// Sort the emitter indices in the execution order array so that dependencies are satisfied.
		Algo::Sort(EmitterExecutionOrder, [&EmitterPriorities](FNiagaraEmitterExecutionIndex IdxA, FNiagaraEmitterExecutionIndex IdxB) { return EmitterPriorities[IdxA.EmitterIndex] < EmitterPriorities[IdxB.EmitterIndex]; });

		// Emitters with the same priority value can execute in parallel. Look for the emitters where the priority increases and mark them as needing to start a new
		// overlap group. This informs the execution code about where to insert synchronization points to satisfy data dependencies.
		// Note that we don't want to set the flag on the first emitter, since on the GPU all the systems are bunched together, and we don't mind overlapping the
		// first emitter from a system with the previous emitters from a different system, as we don't have inter-system dependencies.
		int32 PrevIdx = EmitterExecutionOrder[0].EmitterIndex;
		for (int32 i = 1; i < EmitterExecutionOrder.Num(); ++i)
		{
			int32 CurrentIdx = EmitterExecutionOrder[i].EmitterIndex;
			// A bit of paranoia never hurt anyone. Check that the priorities are monotonically increasing.
			checkSlow(EmitterPriorities[PrevIdx] <= EmitterPriorities[CurrentIdx]);
			if (EmitterPriorities[PrevIdx] != EmitterPriorities[CurrentIdx])
			{
				EmitterExecutionOrder[i].bStartNewOverlapGroup = true;
			}
			PrevIdx = CurrentIdx;
		}
	}

	// go through and remove any entries in the EmitterExecutionOrder array for emitters where we don't have a CachedEmitter, they have
	// likely been cooked out because of scalability
	EmitterExecutionOrder.SetNum(Algo::StableRemoveIf(EmitterExecutionOrder, [this](FNiagaraEmitterExecutionIndex EmitterExecIdx)
	{
		return EmitterHandles[EmitterExecIdx.EmitterIndex].GetInstance().Emitter == nullptr;
	}));
}

void UNiagaraSystem::ComputeRenderersDrawOrder()
{
	struct FSortInfo
	{
		FSortInfo(int32 InSortHint, int32 InRendererIdx) : SortHint(InSortHint), RendererIdx(InRendererIdx) {}
		int32 SortHint;
		int32 RendererIdx;
	};
	TArray<FSortInfo, TInlineAllocator<8>> RendererSortInfo;

	for (const FNiagaraEmitterHandle& EmitterHandle : EmitterHandles)
	{
		if (FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData())
		{
			EmitterData->ForEachEnabledRenderer(
				[&](UNiagaraRendererProperties* Properties)
				{
					RendererSortInfo.Emplace(Properties->SortOrderHint, RendererSortInfo.Num());
				}
			);
		}
	}

	// We sort by the sort hint in order to guarantee that we submit according to the preferred sort order..
	RendererSortInfo.Sort([](const FSortInfo& A, const FSortInfo& B) { return A.SortHint < B.SortHint; });

	RendererDrawOrder.Reset(RendererSortInfo.Num());

	for (const FSortInfo& SortInfo : RendererSortInfo)
	{
		RendererDrawOrder.Add(SortInfo.RendererIdx);
	}
}

void UNiagaraSystem::CacheFromCompiledData()
{
	const FNiagaraDataSetCompiledData& SystemDataSet = SystemCompiledData.DataSetCompiledData;

	bNeedsAsyncOptimize = true;

	// Reset static buffers
	StaticBuffers.Reset(new FNiagaraSystemStaticBuffers());

	// Cache system data accessors
	static const FName NAME_System_ExecutionState = "System.ExecutionState";
	SystemExecutionStateAccessor.Init(SystemDataSet, NAME_System_ExecutionState);

	// Cache emitter data set accessors
	EmitterSpawnInfoAccessors.Reset();
	EmitterExecutionStateAccessors.Reset();
	EmitterSpawnInfoAccessors.SetNum(GetNumEmitters());

	// reset the MaxDeltaTime so we get the most up to date values from the emitters
	MaxDeltaTime.Reset();

	TSet<FName> DataInterfaceGpuUsage;
	TStringBuilder<128> ExecutionStateNameBuilder;
	for (int32 i=0; i < EmitterHandles.Num(); ++i)
	{
		FNiagaraEmitterHandle& Handle = EmitterHandles[i];
		FVersionedNiagaraEmitterData* EmitterData = Handle.GetEmitterData();
		if (Handle.GetIsEnabled() && EmitterData)
		{
			// Cache system instance accessors
			ExecutionStateNameBuilder.Reset();
			ExecutionStateNameBuilder << Handle.GetInstance().Emitter->GetUniqueEmitterName();
			ExecutionStateNameBuilder << TEXT(".ExecutionState");
			const FName ExecutionStateName(ExecutionStateNameBuilder.ToString());

			EmitterExecutionStateAccessors.AddDefaulted_GetRef().Init(SystemDataSet, ExecutionStateName);

			// Cache emitter data set accessors, for things like bounds, etc
			const FNiagaraDataSetCompiledData* DataSetCompiledData = nullptr;
			if (EmitterCompiledData.IsValidIndex(i))
			{
				for (const FName& SpawnName : EmitterCompiledData[i]->SpawnAttributes)
				{
					EmitterSpawnInfoAccessors[i].Emplace(SystemDataSet, SpawnName);
				}

				DataSetCompiledData = &EmitterCompiledData[i]->DataSetCompiledData;

				if (EmitterData->bLimitDeltaTime)
				{
					MaxDeltaTime = MaxDeltaTime.IsSet() ? FMath::Min(MaxDeltaTime.GetValue(), EmitterData->MaxDeltaTimePerTick) : EmitterData->MaxDeltaTimePerTick;
				}
			}
			Handle.GetInstance().Emitter->ConditionalPostLoad();
			EmitterData->CacheFromCompiledData(DataSetCompiledData, *Handle.GetInstance().Emitter);

			// Allow data interfaces to cache static buffers
			UNiagaraScript* NiagaraEmitterScripts[] =
			{
				EmitterData->SpawnScriptProps.Script,
				EmitterData->UpdateScriptProps.Script,
				EmitterData->GetGPUComputeScript(),
			};

			for (UNiagaraScript* NiagaraScript : NiagaraEmitterScripts)
			{
				if (NiagaraScript == nullptr)
				{
					continue;
				}

				const bool bUsedByCPU = EmitterData->SimTarget == ENiagaraSimTarget::CPUSim;
				const bool bUsedByGPU = EmitterData->SimTarget == ENiagaraSimTarget::GPUComputeSim;
				for (const FNiagaraScriptDataInterfaceInfo& DataInterfaceInfo : NiagaraScript->GetCachedDefaultDataInterfaces())
				{
					if (UNiagaraDataInterface* DataInterface = DataInterfaceInfo.DataInterface)
					{
						DataInterface->CacheStaticBuffers(*StaticBuffers.Get(), DataInterfaceInfo, bUsedByCPU, bUsedByGPU);

						if ( bUsedByGPU )
						{
							if (!DataInterfaceInfo.RegisteredParameterMapRead.IsNone())
							{
								DataInterfaceGpuUsage.Add(DataInterfaceInfo.RegisteredParameterMapRead);
							}
						}
					}
				}
			}
		}
		else
		{
			EmitterExecutionStateAccessors.AddDefaulted();
		}
	}

	// Loop over system scripts these are more awkward because we need to determine the usage
	for (UNiagaraScript* NiagaraScript : { SystemSpawnScript, SystemUpdateScript })
	{
		if (NiagaraScript == nullptr)
		{
			continue;
		}

		for (const FNiagaraScriptDataInterfaceInfo& DataInterfaceInfo : NiagaraScript->GetCachedDefaultDataInterfaces())
		{
			if (UNiagaraDataInterface* DataInterface = DataInterfaceInfo.DataInterface)
			{
				const bool bUsedByGPU = DataInterfaceGpuUsage.Contains(DataInterfaceInfo.RegisteredParameterMapWrite);
				DataInterface->CacheStaticBuffers(*StaticBuffers.Get(), DataInterfaceInfo, true, bUsedByGPU);
			}
		}
	}

	// Finalize static buffers
	StaticBuffers->Finalize();
}

bool UNiagaraSystem::HasSystemScriptDIsWithPerInstanceData() const
{
	return bHasSystemScriptDIsWithPerInstanceData;
}

const TArray<FName>& UNiagaraSystem::GetUserDINamesReadInSystemScripts() const
{
	return UserDINamesReadInSystemScripts;
}

FBox UNiagaraSystem::GetFixedBounds() const
{
	return FixedBounds;
}

void CheckDICompileInfo(const TArray<FNiagaraScriptDataInterfaceCompileInfo>& ScriptDICompileInfos, bool& bOutbHasSystemDIsWithPerInstanceData, TArray<FName>& OutUserDINamesReadInSystemScripts)
{
	for (const FNiagaraScriptDataInterfaceCompileInfo& ScriptDICompileInfo : ScriptDICompileInfos)
	{
		UNiagaraDataInterface* DefaultDataInterface = ScriptDICompileInfo.GetDefaultDataInterface();
		if (DefaultDataInterface != nullptr && DefaultDataInterface->PerInstanceDataSize() > 0)
		{
			bOutbHasSystemDIsWithPerInstanceData = true;
		}

		if (ScriptDICompileInfo.RegisteredParameterMapRead.ToString().StartsWith(TEXT("User.")))
		{
			OutUserDINamesReadInSystemScripts.AddUnique(ScriptDICompileInfo.RegisteredParameterMapRead);
		}
	}
}

void UNiagaraSystem::UpdatePostCompileDIInfo()
{
	bHasSystemScriptDIsWithPerInstanceData = false;
	UserDINamesReadInSystemScripts.Empty();
	bNeedsGPUContextInitForDataInterfaces = false;

	CheckDICompileInfo(SystemSpawnScript->GetVMExecutableData().DataInterfaceInfo, bHasSystemScriptDIsWithPerInstanceData, UserDINamesReadInSystemScripts);
	CheckDICompileInfo(SystemUpdateScript->GetVMExecutableData().DataInterfaceInfo, bHasSystemScriptDIsWithPerInstanceData, UserDINamesReadInSystemScripts);

	for (const FNiagaraEmitterHandle& EmitterHandle : GetEmitterHandles())
	{
		FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData();
		if (EmitterHandle.GetIsEnabled() == false || EmitterData == nullptr)
		{
			continue;
		}
		if (EmitterData->SimTarget == ENiagaraSimTarget::GPUComputeSim)
		{
			UNiagaraScript* GPUScript = EmitterData->GetGPUComputeScript();
			if (GPUScript)
			{
				FNiagaraVMExecutableData& VMData = GPUScript->GetVMExecutableData();
				if (VMData.IsValid() && VMData.bNeedsGPUContextInit)
				{
					bNeedsGPUContextInitForDataInterfaces = true;
				}
			}
		}
	}
}

void UNiagaraSystem::UpdateDITickFlags()
{
	bHasDIsWithPostSimulateTick = false;
	bAllDIsPostSimulateCanOverlapFrames = true;
	auto CheckPostSimTick = [&](UNiagaraScript* Script)
	{
		if (Script)
		{
			for (FNiagaraScriptDataInterfaceCompileInfo& Info : Script->GetVMExecutableData().DataInterfaceInfo)
			{
				UNiagaraDataInterface* DefaultDataInterface = Info.GetDefaultDataInterface();
				if (DefaultDataInterface && DefaultDataInterface->HasPostSimulateTick())
				{
					bHasDIsWithPostSimulateTick |= true;
					bAllDIsPostSimulateCanOverlapFrames &= DefaultDataInterface->PostSimulateCanOverlapFrames();
				}
			}
		}
	};

	CheckPostSimTick(SystemSpawnScript);
	CheckPostSimTick(SystemUpdateScript);
	for (FNiagaraEmitterHandle& Handle : EmitterHandles)
	{
		if (Handle.GetIsEnabled())
		{
			if (FVersionedNiagaraEmitterData* EmitterData = Handle.GetEmitterData())
			{
				TArray<UNiagaraScript*> Scripts;
				EmitterData->GetScripts(Scripts);
				for (UNiagaraScript* Script : Scripts)
				{
					CheckPostSimTick(Script);
				}
			}
		}
	}
}

void UNiagaraSystem::UpdateHasGPUEmitters()
{
	bHasAnyGPUEmitters = 0;
	for (FNiagaraEmitterHandle& Handle : EmitterHandles)
	{
		if (Handle.GetIsEnabled())
		{
			if (FVersionedNiagaraEmitterData* EmitterData = Handle.GetEmitterData())
			{
				bHasAnyGPUEmitters |= EmitterData->SimTarget == ENiagaraSimTarget::GPUComputeSim;
			}
		}
	}
}

bool UNiagaraSystem::IsValidInternal() const
{
	if (!SystemSpawnScript || !SystemUpdateScript)
	{
		return false;
	}

	if ((!SystemSpawnScript->IsScriptCompilationPending(false) && !SystemSpawnScript->DidScriptCompilationSucceed(false)) ||
		(!SystemUpdateScript->IsScriptCompilationPending(false) && !SystemUpdateScript->DidScriptCompilationSucceed(false)))
	{
		return false;
	}

	if (EmitterHandles.Num() == 0)
	{
		return false;
	}

	for (const FNiagaraEmitterHandle& Handle : EmitterHandles)
	{
		if (Handle.GetIsEnabled())
		{
			if (FVersionedNiagaraEmitterData* EmitterData = Handle.GetEmitterData())
			{
				if (!EmitterData->IsValid() && EmitterData->IsAllowedByScalability())
				{
					return false;
				}
			}
		}
	}

	return true;
}

void UNiagaraSystem::EnsureFullyLoaded() const
{
	UNiagaraSystem* System = const_cast<UNiagaraSystem*>(this);
	System->UpdateSystemAfterLoad();
}

bool UNiagaraSystem::CanObtainEmitterAttribute(const FNiagaraVariableBase& InVarWithUniqueNameNamespace, FNiagaraTypeDefinition& OutBoundType) const
{
	return CanObtainSystemAttribute(InVarWithUniqueNameNamespace, OutBoundType);
}

bool UNiagaraSystem::CanObtainSystemAttribute(const FNiagaraVariableBase& InVar, FNiagaraTypeDefinition& OutBoundType) const
{
	if (SystemSpawnScript)
	{
		OutBoundType = InVar.GetType();
		bool bContainsAttribute = SystemSpawnScript->GetVMExecutableData().Attributes.ContainsByPredicate(FNiagaraVariableMatch(OutBoundType, InVar.GetName()));
		if (!bContainsAttribute && InVar.GetType() == FNiagaraTypeDefinition::GetPositionDef())
		{
			// if we don't find a position type var we check for a vec3 type for backwards compatibility
			OutBoundType = FNiagaraTypeDefinition::GetVec3Def();
			bContainsAttribute = SystemSpawnScript->GetVMExecutableData().Attributes.ContainsByPredicate(FNiagaraVariableMatch(OutBoundType, InVar.GetName()));
		}
		return bContainsAttribute;
	}
	return false;
}

bool UNiagaraSystem::CanObtainUserVariable(const FNiagaraVariableBase& InVar) const
{
	return ExposedParameters.IndexOf(InVar) != INDEX_NONE;
}

#if WITH_EDITORONLY_DATA

FNiagaraEmitterHandle UNiagaraSystem::AddEmitterHandle(UNiagaraEmitter& InEmitter, FName EmitterName, FGuid EmitterVersion)
{
	UNiagaraEmitter* NewEmitter = UNiagaraEmitter::CreateWithParentAndOwner(FVersionedNiagaraEmitter(&InEmitter, EmitterVersion), this, EmitterName, ~(RF_Public | RF_Standalone));
	FNiagaraEmitterHandle EmitterHandle(*NewEmitter, EmitterVersion);
	if (InEmitter.TemplateSpecification == ENiagaraScriptTemplateSpecification::Template || InEmitter.TemplateSpecification == ENiagaraScriptTemplateSpecification::Behavior)
	{
		NewEmitter->DisableVersioning(EmitterVersion);
		NewEmitter->TemplateSpecification = ENiagaraScriptTemplateSpecification::None;
		NewEmitter->TemplateAssetDescription = FText();
		NewEmitter->GetLatestEmitterData()->RemoveParent();
	}
	EmitterHandles.Add(EmitterHandle);
	RefreshSystemParametersFromEmitter(EmitterHandle);
	return EmitterHandle;
}

void UNiagaraSystem::AddEmitterHandleDirect(FNiagaraEmitterHandle& EmitterHandle)
{
	EmitterHandles.Add(EmitterHandle);
	RefreshSystemParametersFromEmitter(EmitterHandle);
}

FNiagaraEmitterHandle UNiagaraSystem::DuplicateEmitterHandle(const FNiagaraEmitterHandle& EmitterHandleToDuplicate, FName EmitterName)
{
	UNiagaraEmitter* DuplicateEmitter = UNiagaraEmitter::CreateAsDuplicate(*EmitterHandleToDuplicate.GetInstance().Emitter, EmitterName, *this);
	FNiagaraEmitterHandle EmitterHandle(*DuplicateEmitter, EmitterHandleToDuplicate.GetInstance().Version);
	EmitterHandle.SetIsEnabled(EmitterHandleToDuplicate.GetIsEnabled(), *this, false);
	EmitterHandles.Add(EmitterHandle);
	RefreshSystemParametersFromEmitter(EmitterHandle);
	return EmitterHandle;
}

void UNiagaraSystem::RemoveEmitterHandle(const FNiagaraEmitterHandle& EmitterHandleToDelete)
{
	RemoveSystemParametersForEmitter(EmitterHandleToDelete);
	auto RemovePredicate = [&](const FNiagaraEmitterHandle& EmitterHandle) { return EmitterHandle.GetId() == EmitterHandleToDelete.GetId(); };
	EmitterHandles.RemoveAll(RemovePredicate);
}

void UNiagaraSystem::RemoveEmitterHandlesById(const TSet<FGuid>& HandlesToRemove)
{
	auto RemovePredicate = [&](const FNiagaraEmitterHandle& EmitterHandle)
	{
		return HandlesToRemove.Contains(EmitterHandle.GetId());
	};
	EmitterHandles.RemoveAll(RemovePredicate);

	InitEmitterCompiledData();
}
#endif


UNiagaraScript* UNiagaraSystem::GetSystemSpawnScript()
{
	return SystemSpawnScript;
}

UNiagaraScript* UNiagaraSystem::GetSystemUpdateScript()
{
	return SystemUpdateScript;
}

const UNiagaraScript* UNiagaraSystem::GetSystemSpawnScript() const
{
	return SystemSpawnScript;
}

const UNiagaraScript* UNiagaraSystem::GetSystemUpdateScript() const
{
	return SystemUpdateScript;
}

#if WITH_EDITORONLY_DATA

void UNiagaraSystem::KillAllActiveCompilations()
{
	InvalidateActiveCompiles();
	for (FNiagaraSystemCompileRequest& Request : ActiveCompilations)
	{
		for (auto& AsyncTask : Request.DDCTasks)
		{
			AsyncTask->AbortTask();
		}
	}
	ActiveCompilations.Empty();
}

bool UNiagaraSystem::GetIsolateEnabled() const
{
	return bIsolateEnabled;
}

void UNiagaraSystem::SetIsolateEnabled(bool bIsolate)
{
	bIsolateEnabled = bIsolate;
}

UNiagaraSystem::FOnSystemCompiled& UNiagaraSystem::OnSystemCompiled()
{
	return OnSystemCompiledDelegate;
}

UNiagaraSystem::FOnSystemPostEditChange& UNiagaraSystem::OnSystemPostEditChange()
{
	return OnSystemPostEditChangeDelegate;
}

UNiagaraSystem::FOnScalabilityChanged& UNiagaraSystem::OnScalabilityChanged()
{
	return OnScalabilityChangedDelegate;
}

void UNiagaraSystem::ForceGraphToRecompileOnNextCheck()
{
	check(SystemSpawnScript->GetLatestSource() == SystemUpdateScript->GetLatestSource());
	SystemSpawnScript->GetLatestSource()->ForceGraphToRecompileOnNextCheck();

	for (FNiagaraEmitterHandle Handle : EmitterHandles)
	{
		if (FVersionedNiagaraEmitterData* EmitterData = Handle.GetEmitterData())
		{
			UNiagaraScriptSourceBase* GraphSource = EmitterData->GraphSource;
			GraphSource->ForceGraphToRecompileOnNextCheck();
		}
	}
}

void UNiagaraSystem::WaitForCompilationComplete(bool bIncludingGPUShaders, bool bShowProgress)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(WaitForNiagaraCompilation);
	TRACE_CPUPROFILER_EVENT_SCOPE_TEXT_ON_CHANNEL(*GetPathName(), NiagaraChannel);

	// Calculate the slow progress for notifying via UI
	TArray<FNiagaraShaderScript*, TInlineAllocator<16>> GPUScripts;
	if (bIncludingGPUShaders)
	{
		for (FNiagaraEmitterHandle& EmitterHandle : EmitterHandles)
		{
			if (FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData())
			{
				if (UNiagaraScript* GPUComputeScript = EmitterData->GetGPUComputeScript())
				{
					if (FNiagaraShaderScript* ShaderScript = GPUComputeScript->GetRenderThreadScript())
					{
						if (!ShaderScript->IsCompilationFinished())
							GPUScripts.Add(ShaderScript);
					}
				}
			}
		}
	}
	
	const int32 TotalCompiles = ActiveCompilations.Num() + GPUScripts.Num();
	FScopedSlowTask Progress(TotalCompiles, LOCTEXT("WaitingForCompile", "Waiting for compilation to complete"));
	if (bShowProgress && TotalCompiles > 0)
	{
		Progress.MakeDialog();
	}

	double StartTime = FPlatformTime::Seconds();
	double TimeLogThreshold = GNiagaraCompileWaitLoggingThreshold;
	uint32 NumLogIterations = GNiagaraCompileWaitLoggingTerminationCap;

	while (ActiveCompilations.Num() > 0)
	{
		if (QueryCompileComplete(true, ActiveCompilations.Num() == 1))
		{
			// make sure to only mark progress if we actually have accomplished something in the QueryCompileComplete
			Progress.EnterProgressFrame();
		}
	
		if (GIsAutomationTesting)
		{
			double CurrentTime = FPlatformTime::Seconds();
			double DeltaTimeSinceLastLog = CurrentTime - StartTime;
			if (DeltaTimeSinceLastLog > TimeLogThreshold)
			{
				StartTime = CurrentTime;
				UE_LOG(LogNiagara, Log, TEXT("Waiting for %f seconds > %s"), (float)DeltaTimeSinceLastLog, *GetPathNameSafe(this));
				NumLogIterations--;
			}

			if (NumLogIterations == 0)
			{
				UE_LOG(LogNiagara, Warning, TEXT("Timed out for compiling > %s"), *GetPathNameSafe(this));
				break;
			}
		}
	}
	
	for (FNiagaraShaderScript* ShaderScript : GPUScripts)
	{
		Progress.EnterProgressFrame();
		ShaderScript->FinishCompilation();
	}
}

void UNiagaraSystem::InvalidateActiveCompiles()
{
	for (FNiagaraSystemCompileRequest& CompileRequest : ActiveCompilations)
	{
		CompileRequest.bIsValid = false;
	}
}

bool UNiagaraSystem::PollForCompilationComplete()
{
	if (ActiveCompilations.Num() > 0)
	{
		return QueryCompileComplete(false, true);
	}
	return true;
}

bool UNiagaraSystem::CompilationResultsValid(FNiagaraSystemCompileRequest& CompileRequest) const
{
	// for now the only thing we're concerned about is if we've got results for SystemSpawn and SystemUpdate scripts
	// then we need to make sure that they agree in terms of the dataset attributes
	TSharedPtr<FNiagaraAsyncCompileTask, ESPMode::ThreadSafe>* SpawnScriptRequest = CompileRequest.DDCTasks.FindByPredicate([this](const auto& Task) { return Task->GetScriptPair().CompiledScript == SystemSpawnScript; });
	TSharedPtr<FNiagaraAsyncCompileTask, ESPMode::ThreadSafe>* UpdateScriptRequest = CompileRequest.DDCTasks.FindByPredicate([this](const auto& Task) { return Task->GetScriptPair().CompiledScript == SystemUpdateScript; });

	const bool SpawnScriptValid = SpawnScriptRequest
		&& SpawnScriptRequest->Get()->GetScriptPair().CompileResults.IsValid()
		&& SpawnScriptRequest->Get()->GetScriptPair().CompileResults->LastCompileStatus != ENiagaraScriptCompileStatus::NCS_Error;

	const bool UpdateScriptValid = UpdateScriptRequest
		&& UpdateScriptRequest->Get()->GetScriptPair().CompileResults.IsValid()
		&& UpdateScriptRequest->Get()->GetScriptPair().CompileResults->LastCompileStatus != ENiagaraScriptCompileStatus::NCS_Error;

	if (SpawnScriptValid && UpdateScriptValid)
	{
		const FEmitterCompiledScriptPair& SpawnScriptPair = SpawnScriptRequest->Get()->GetScriptPair();
		const FEmitterCompiledScriptPair& UpdateScriptPair = UpdateScriptRequest->Get()->GetScriptPair();
		if (SpawnScriptPair.CompileResults->Attributes != UpdateScriptPair.CompileResults->Attributes)
		{
			// if we had requested a full rebuild, then we've got a case where the generated scripts are not compatible.  This indicates
			// a significant issue where we're allowing graphs to generate invalid collections of scripts.  One known example is using
			// the Script.Context static switch that isn't fully processed in all scripts, leading to attributes differing between the
			// SystemSpawnScript and the SystemUpdateScript
			if (CompileRequest.bForced)
			{
				FString MissingAttributes;
				FString AdditionalAttributes;

				for (const auto& SpawnAttrib : SpawnScriptPair.CompileResults->Attributes)
				{
					if (!UpdateScriptPair.CompileResults->Attributes.Contains(SpawnAttrib))
					{
						MissingAttributes.Appendf(TEXT("%s%s"), MissingAttributes.Len() ? TEXT(", ") : TEXT(""), *SpawnAttrib.GetName().ToString());
					}
				}

				for (const auto& UpdateAttrib : UpdateScriptPair.CompileResults->Attributes)
				{
					if (!SpawnScriptPair.CompileResults->Attributes.Contains(UpdateAttrib))
					{
						AdditionalAttributes.Appendf(TEXT("%s%s"), AdditionalAttributes.Len() ? TEXT(", ") : TEXT(""), *UpdateAttrib.GetName().ToString());
					}
				}

				FNiagaraCompileEvent AttributeMismatchEvent(
					FNiagaraCompileEventSeverity::Error,
					FText::Format(LOCTEXT("SystemScriptAttributeMismatchError", "System Spawn/Update scripts have attributes which don't match!\n\tMissing update attributes: {0}\n\tAdditional update attributes: {1}"),
						FText::FromString(MissingAttributes),
						FText::FromString(AdditionalAttributes))
					.ToString());

				SpawnScriptPair.CompileResults->LastCompileStatus = ENiagaraScriptCompileStatus::NCS_Error;
				SpawnScriptPair.CompileResults->LastCompileEvents.Add(AttributeMismatchEvent);
			}
			else
			{
				UE_LOG(LogNiagara, Log, TEXT("Failed to generate consistent results for System spawn and update scripts for system %s."), *GetFullName());
			}

			return false;
		}
	}
	return true;
}

void UNiagaraSystem::EvaluateCompileResultDependencies() const
{
	struct FScriptCompileResultValidationInfo
	{
		UNiagaraEmitter* Emitter = nullptr;
		FNiagaraVMExecutableData* CompileResults = nullptr;
		TArray<int32> ParentIndices;
		bool bCompileResultStatusDirty = false;

		void ReEvaluateCompileStatus()
		{
			// Early out for invalid existing compile statuses.
			if (CompileResults->LastCompileStatus == ENiagaraScriptCompileStatus::NCS_BeingCreated)
			{
				ensureMsgf(false, TEXT("Encountered \"Being Created\" Compile Status when evaluating compile dependencies after compile! Compilation should be complete!"));
				return;
			}
			else if (CompileResults->LastCompileStatus == ENiagaraScriptCompileStatus::NCS_Dirty)
			{
				ensureMsgf(false, TEXT("Encountered \"Dirty\" Compile Status when evaluating compile dependencies after compile! Compilation should be complete!"));
				return;
			}

			// Tally warnings and errors.
			int32 NumErrors = 0;
			int32 NumWarnings = 0;

			for (const FNiagaraCompileEvent& Event : CompileResults->LastCompileEvents)
			{
				if (Event.Severity == FNiagaraCompileEventSeverity::Error)
				{
					NumErrors++;
				}
				else if (Event.Severity == FNiagaraCompileEventSeverity::Warning)
				{
					NumWarnings++;
				}
			}

			// Set last compile status based on error and warning count.
			if (NumErrors > 0)
			{
				CompileResults->LastCompileStatus = ENiagaraScriptCompileStatus::NCS_Error;
			}
			else if (NumWarnings > 0)
			{
				CompileResults->LastCompileStatus = ENiagaraScriptCompileStatus::NCS_UpToDateWithWarnings;
			}
			else
			{
				CompileResults->LastCompileStatus = ENiagaraScriptCompileStatus::NCS_UpToDate;
			}
		};
	};

	TArray<FScriptCompileResultValidationInfo> ScriptCompilesToValidate;

	// Add a compiled script pair to be evaluated for external dependencies to ScriptCompilesToValidate and clear all of its compile events that are from script dependencies.
	auto AddCompiledScriptForValidation = [&ScriptCompilesToValidate](UNiagaraScript* InScript, const TArray<int32>& InParentIndices, UNiagaraEmitter* InEmitter = nullptr)->int32
	{
		FScriptCompileResultValidationInfo ValidationInfo = FScriptCompileResultValidationInfo();
		ValidationInfo.CompileResults = &InScript->GetVMExecutableData();
		TArray<FNiagaraCompileEvent>& CompileEvents = ValidationInfo.CompileResults->LastCompileEvents;
		for (int32 Idx = CompileEvents.Num() - 1; Idx > -1; --Idx)
		{
			const FNiagaraCompileEvent& Event = CompileEvents[Idx];
			if (Event.Source == FNiagaraCompileEventSource::ScriptDependency)
			{
				CompileEvents.RemoveAt(Idx);
				ValidationInfo.bCompileResultStatusDirty = true;
			}
		}

		ValidationInfo.Emitter = InEmitter;
		ValidationInfo.ParentIndices = InParentIndices;
		return ScriptCompilesToValidate.Add(ValidationInfo);
	};

	int32 SystemSpawnScriptIdx = AddCompiledScriptForValidation(SystemSpawnScript, TArray<int32>());
	TArray<int32> SystemSpawnVisitOrder;
	SystemSpawnVisitOrder.Add(SystemSpawnScriptIdx);

	int32 SystemUpdateScriptIdx = AddCompiledScriptForValidation(SystemUpdateScript, SystemSpawnVisitOrder);
	TArray<int32> SystemUpdateVisitOrder;
	SystemUpdateVisitOrder.Add(SystemSpawnScriptIdx);
	SystemUpdateVisitOrder.Add(SystemUpdateScriptIdx);

	// Gather per-emitter scripts to evaluate dependencies. The dependencies need to form a linear chain walking up from the current execution path all the way to SystemSpawn, with 
	// all scripts that execute in between being visited. If there are holes, then something could be marked as not being written even though it was written. The flow is thus:
	// System Spawn
	// Emitter Spawn
	// System Update
	// Emitter Update
	// Particle Spawn/Interpolated
	// Particle Spawn Event
	// Particle Update
	// Particle Non-Spawn Event
	// Particle Sim Stages
	for (const FNiagaraEmitterHandle& Handle : EmitterHandles)
	{
		if (Handle.GetIsEnabled() && Handle.GetEmitterData())
		{
			UNiagaraEmitter* Emitter = Handle.GetInstance().Emitter;
			FVersionedNiagaraEmitterData* EmitterData = Handle.GetEmitterData();

			const int32 EmitterSpawnScriptIdx = AddCompiledScriptForValidation(EmitterData->EmitterSpawnScriptProps.Script, SystemSpawnVisitOrder, Emitter);
			TArray<int32> EmitterSpawnVisitOrder = SystemSpawnVisitOrder;
			TArray<int32> EmitterUpdateVisitOrder = SystemUpdateVisitOrder;
			EmitterSpawnVisitOrder.Add(EmitterSpawnScriptIdx);
			EmitterUpdateVisitOrder.Add(EmitterSpawnScriptIdx);

			const int32 EmitterUpdateScriptIdx = AddCompiledScriptForValidation(EmitterData->EmitterUpdateScriptProps.Script, EmitterUpdateVisitOrder, Emitter);
			EmitterUpdateVisitOrder.Add(EmitterUpdateScriptIdx);

			TArray<int32> ParticleSpawnVisitOrder = EmitterUpdateVisitOrder;
			TArray<int32> ParticleUpdateVisitOrder = ParticleSpawnVisitOrder;
			const int32 ParticleSpawnScriptIdx = AddCompiledScriptForValidation(EmitterData->SpawnScriptProps.Script, ParticleSpawnVisitOrder, Emitter);
			ParticleSpawnVisitOrder.Add(ParticleSpawnScriptIdx);
			ParticleUpdateVisitOrder.Add(ParticleSpawnScriptIdx);

			const int32 ParticleUpdateScriptIdx = AddCompiledScriptForValidation(EmitterData->UpdateScriptProps.Script, ParticleUpdateVisitOrder, Emitter);
			ParticleUpdateVisitOrder.Add(ParticleUpdateScriptIdx);

			// If the spawn script is interpolated, allow the particle update script to resolve dependencies for spawn events and spawn sim stages.
			if (EmitterData->bInterpolatedSpawning)
			{
				ParticleSpawnVisitOrder.Add(ParticleUpdateScriptIdx);
			}
			
			const TArray<FNiagaraEventScriptProperties>& EventHandlerScriptProps = EmitterData->GetEventHandlers();
			for (int32 i = 0; i < EventHandlerScriptProps.Num(); i++)
			{
				if (EventHandlerScriptProps[i].Script)
				{
					if (EventHandlerScriptProps[i].ExecutionMode == EScriptExecutionMode::SpawnedParticles)
					{
						AddCompiledScriptForValidation(EventHandlerScriptProps[i].Script, ParticleSpawnVisitOrder, Emitter);
					}
						
					else
					{
						AddCompiledScriptForValidation(EventHandlerScriptProps[i].Script, ParticleUpdateVisitOrder, Emitter);
					}	
				}
			}
			
			// Gather up all the spawn stages first, since they could write to a value before anything else.
			const TArray<UNiagaraSimulationStageBase*>& SimulationStages = EmitterData->GetSimulationStages();
			TArray<UNiagaraSimulationStageBase*> NonSpawnStages;
			for (int32 i = 0; i < SimulationStages.Num(); i++)
			{
				if (SimulationStages[i] && SimulationStages[i]->Script)
				{
					if (SimulationStages[i]->bEnabled == false)
					{
						continue;
					}

					UNiagaraSimulationStageGeneric* GenericSimStage = Cast<UNiagaraSimulationStageGeneric>(SimulationStages[i]);
					if (GenericSimStage && GenericSimStage->ExecuteBehavior == ENiagaraSimStageExecuteBehavior::OnSimulationReset)
					{
						int32 LastSpawnStageIdx = AddCompiledScriptForValidation(SimulationStages[i]->Script, ParticleSpawnVisitOrder, Emitter);
						ParticleSpawnVisitOrder.Add(LastSpawnStageIdx);
						ParticleUpdateVisitOrder.Add(LastSpawnStageIdx);
					}
					else
					{
						NonSpawnStages.Add(SimulationStages[i]);
					}
				}
			}

			// Now handle all the other stages
			for (int32 i = 0; i < NonSpawnStages.Num(); i++)
			{
				int32 LastStageIdx = AddCompiledScriptForValidation(NonSpawnStages[i]->Script, ParticleUpdateVisitOrder, Emitter);
				ParticleUpdateVisitOrder.Add(LastStageIdx);
			}

			if (EmitterData->SimTarget == ENiagaraSimTarget::GPUComputeSim)
			{
				AddCompiledScriptForValidation(EmitterData->GetGPUComputeScript(), ParticleUpdateVisitOrder, Emitter);
			}
		}
	}

	// Iterate over all ScriptCompilesToValidate and add compile events for dependencies that are not met.
	for (FScriptCompileResultValidationInfo& ValidationInfo : ScriptCompilesToValidate)
	{
		for (const FNiagaraCompileDependency& Dependency : ValidationInfo.CompileResults->ExternalDependencies)
		{
			FNiagaraVariableBase TestVar = Dependency.DependentVariable;
			ensure(TestVar.GetName() != NAME_None);
			if (ValidationInfo.Emitter && Dependency.bDependentVariableFromCustomIterationNamespace == false)
			{
				FName NewName = GetEmitterVariableAliasName(TestVar, ValidationInfo.Emitter);
				TestVar.SetName(NewName);
			}

			auto TestVarNameAndAssignableTypeMatchPred = [&TestVar](const FNiagaraVariableBase& ComparisonVar)->bool {
				return TestVar.GetName() == ComparisonVar.GetName() && FNiagaraUtilities::AreTypesAssignable(TestVar.GetType(), ComparisonVar.GetType());
			};

			bool bDependencyMet = false;
			for (const int32 TestIndex : ValidationInfo.ParentIndices)
			{
				if (ScriptCompilesToValidate.IsValidIndex(TestIndex) == false)
				{
					ensureMsgf(false, TEXT("Encountered invalid index into script compiles to validate when evaluating fail if not set dependencies!"));
				}

				const FScriptCompileResultValidationInfo& TestInfo = ScriptCompilesToValidate[TestIndex];
				if (TestVar.GetType().IsStatic() && TestInfo.CompileResults->StaticVariablesWritten.ContainsByPredicate(TestVarNameAndAssignableTypeMatchPred))
				{
					bDependencyMet = true;
					break;
				}
				else if (TestInfo.CompileResults->AttributesWritten.ContainsByPredicate(TestVarNameAndAssignableTypeMatchPred))
				{
					bDependencyMet = true;
					break;
				}
			}
			if (!bDependencyMet)
			{
				FNiagaraCompileEvent LinkerErrorEvent(
					FNiagaraCVarUtilities::GetCompileEventSeverityForFailIfNotSet(), Dependency.LinkerErrorMessage, FString(), false, Dependency.NodeGuid, Dependency.PinGuid, Dependency.StackGuids, FNiagaraCompileEventSource::ScriptDependency);
				ValidationInfo.CompileResults->LastCompileEvents.Add(LinkerErrorEvent);
				ValidationInfo.bCompileResultStatusDirty = true;
			}
		}

		// If the compile events of the compile results have changed, re-evaluate the compile status as it could have changed.
		if (ValidationInfo.bCompileResultStatusDirty)
		{
			ValidationInfo.ReEvaluateCompileStatus();
		}
	}
}

void UNiagaraSystem::PreProcessWaitingDDCTasks(bool bProcessForWait)
{
	if (!bProcessForWait)
	{
		return;
	}
	for (FNiagaraSystemCompileRequest& CompileRequest : ActiveCompilations)
	{
		for (auto& AsyncTask : CompileRequest.DDCTasks)
		{
			AsyncTask->bWaitForCompileJob = true;
			// before we start to wait for the compile results, we start the compilation of all remaining tasks
			while (!AsyncTask->IsDone() && AsyncTask->CurrentState < ENiagaraCompilationState::AwaitResult)
			{
				AsyncTask->ProcessCurrentState();
			}
		}
	}
}

bool UNiagaraSystem::QueryCompileComplete(bool bWait, bool bDoPost, bool bDoNotApply)
{
	check(IsInGameThread());
	if (ActiveCompilations.Num() > 0 && !bCompilationReentrantGuard)
	{
		bCompilationReentrantGuard = true;
		ON_SCOPE_EXIT { bCompilationReentrantGuard = false; };

		PreProcessWaitingDDCTasks(bWait);
		
		FNiagaraSystemCompileRequest& CompileRequest = ActiveCompilations[0];
		bool bAreWeWaitingForAnyResults = false;
		double StartTime = FPlatformTime::Seconds();
		
		// Check to see if ALL of the sub-requests have resolved.
		for (auto& AsyncTask : CompileRequest.DDCTasks)
		{
			// if a task is very expensive we bail and continue on the next frame  
			if (FPlatformTime::Seconds() - StartTime < 0.125)
			{
				AsyncTask->ProcessCurrentState();
			}

			if (AsyncTask->IsDone())
			{
				continue;
			}

			if (!AsyncTask->bFetchedGCObjects && AsyncTask->CurrentState > ENiagaraCompilationState::Precompile && AsyncTask->CurrentState <= ENiagaraCompilationState::ProcessResult)
			{
				CompileRequest.RootObjects.Append(AsyncTask->PrecompileReference->CompilationRootObjects);
				AsyncTask->bFetchedGCObjects = true;
			}
			
			if (bWait)
			{
				AsyncTask->WaitAndResolveResult();
			}
			else
			{
				bAreWeWaitingForAnyResults = true;
			}
		}

		// Make sure that we aren't waiting for any results to come back.
		if (bAreWeWaitingForAnyResults)
		{
			return false;
		}
		// if we've gotten all the results, run a quick check to see if the data is valid, if it's not then that indicates that
		// we've run into a compatibility issue and so we should see if we should issue a full rebuild
		const bool ResultsValid = CompilationResultsValid(CompileRequest);
		if (!ResultsValid && !CompileRequest.bForced)
		{
			CompileRequest.RootObjects.Empty();
			ActiveCompilations.RemoveAt(0);
			RequestCompile(true, nullptr);
			return false;
		}

		// In the world of do not apply, we're exiting the system completely so let's just kill any active compilations altogether.
		if (bDoNotApply || CompileRequest.bIsValid == false)
		{
			CompileRequest.RootObjects.Empty();
			ActiveCompilations.RemoveAt(0);
			return true;
		}

		// Now that the above code says they are all complete, go ahead and resolve them all at once.
		bool bHasCompiledJobs = false;
		for (auto& AsyncTask : CompileRequest.DDCTasks)
		{
			bHasCompiledJobs |= AsyncTask->bUsedShaderCompilerWorker;
			
			FEmitterCompiledScriptPair& EmitterCompiledScriptPair = AsyncTask->ScriptPair;
			if (EmitterCompiledScriptPair.bResultsReady)
			{
				TSharedPtr<FNiagaraVMExecutableData> ExeData = EmitterCompiledScriptPair.CompileResults;
				CompileRequest.CombinedCompileTime += ExeData->CompileTime;
				UNiagaraScript* CompiledScript = EmitterCompiledScriptPair.CompiledScript;

				// generate the ObjectNameMap from the source (from the duplicated data if available).
				TMap<FName, UNiagaraDataInterface*> ObjectNameMap;
				if (AsyncTask->ComputedPrecompileDuplicateData.IsValid())
				{
					if (const UNiagaraScriptSourceBase* ScriptSource = AsyncTask->ComputedPrecompileDuplicateData->GetScriptSource())
					{
						ObjectNameMap = ScriptSource->ComputeObjectNameMap(*this, CompiledScript->GetUsage(), CompiledScript->GetUsageId(), AsyncTask->UniqueEmitterName);
					}
					else
					{
						checkf(false, TEXT("Failed to get ScriptSource from ComputedPrecompileDuplicateData"));
					}

					// we need to replace the DI in the above generated map with the duplicates that we've created as a part of the duplication process
					TMap<FName, UNiagaraDataInterface*> DuplicatedObjectNameMap = AsyncTask->ComputedPrecompileDuplicateData->GetObjectNameMap();
					for (auto ObjectMapIt = ObjectNameMap.CreateIterator(); ObjectMapIt; ++ObjectMapIt)
					{
						if (UNiagaraDataInterface** Replacement = DuplicatedObjectNameMap.Find(ObjectMapIt.Key()))
						{
							ObjectMapIt.Value() = *Replacement;
						}
						else
						{
							ObjectMapIt.RemoveCurrent();
						}
					}
				}
				else
				{
					const UNiagaraScriptSourceBase* ScriptSource = CompiledScript->GetLatestSource();
					ObjectNameMap = ScriptSource->ComputeObjectNameMap(*this, CompiledScript->GetUsage(), CompiledScript->GetUsageId(), AsyncTask->UniqueEmitterName);
				}

				EmitterCompiledScriptPair.CompiledScript->SetVMCompilationResults(EmitterCompiledScriptPair.CompileId, *ExeData, AsyncTask->UniqueEmitterName, ObjectNameMap);

				// Synchronize the variables that we actually encountered during precompile so that we can expose them to the end user.
				TArray<FNiagaraVariable> OriginalExposedParams;
				ExposedParameters.GetParameters(OriginalExposedParams);
				TArray<FNiagaraVariable>& EncounteredExposedVars = AsyncTask->EncounteredExposedVars;
				for (int32 i = 0; i < EncounteredExposedVars.Num(); i++)
				{
					if (OriginalExposedParams.Contains(EncounteredExposedVars[i]) == false)
					{
						// Just in case it wasn't added previously..
						ExposedParameters.AddParameter(EncounteredExposedVars[i], true, false);
					}
				}
			}
		}

		// clean up the precompile data
		for (auto& AsyncTask : CompileRequest.DDCTasks)
		{
			AsyncTask->ComputedPrecompileData.Reset();
			if (AsyncTask->ComputedPrecompileDuplicateData.IsValid())
			{
				AsyncTask->ComputedPrecompileDuplicateData->ReleaseCompilationCopies();
				AsyncTask->ComputedPrecompileDuplicateData.Reset();
			}
		}

		// Once compile results have been set, check dependencies found during compile and evaluate whether dependency compile events should be added.
		if (FNiagaraCVarUtilities::GetShouldEmitMessagesForFailIfNotSet())
		{
			EvaluateCompileResultDependencies();
		}

		if (bDoPost)
		{
			for (FNiagaraEmitterHandle Handle : EmitterHandles)
			{
				if (Handle.GetInstance().Emitter)
				{
					if (Handle.GetIsEnabled())
					{
						Handle.GetInstance().Emitter->OnPostCompile(Handle.GetInstance().Version);
					}
					else
					{
						Handle.GetEmitterData()->InvalidateCompileResults();
					}
				}
			}
		}

		InitEmitterCompiledData();
		InitSystemCompiledData();

		// HACK: This is a temporary hack to fix an issue where data interfaces used by modules and dynamic inputs in the
		// particle update script aren't being shared by the interpolated spawn script when accessed directly.  This works
		// properly if the data interface is assigned to a named particle parameter and then linked to an input.
		// TODO: Bind these data interfaces the same way parameter data interfaces are bound.
		for (auto& AsyncTask : CompileRequest.DDCTasks)
		{
			FEmitterCompiledScriptPair& EmitterCompiledScriptPair = AsyncTask->ScriptPair;
			FVersionedNiagaraEmitter Emitter = EmitterCompiledScriptPair.VersionedEmitter;
			UNiagaraScript* CompiledScript = EmitterCompiledScriptPair.CompiledScript;

			if (UNiagaraScript::IsEquivalentUsage(CompiledScript->GetUsage(), ENiagaraScriptUsage::ParticleUpdateScript))
			{
				UNiagaraScript* SpawnScript = Emitter.GetEmitterData()->SpawnScriptProps.Script;
				for (const FNiagaraScriptDataInterfaceInfo& UpdateDataInterfaceInfo : CompiledScript->GetCachedDefaultDataInterfaces())
				{
					// If the data interface isn't being written to a parameter map then it won't be bound properly so we
					// assign the update scripts copy of the data interface to the spawn scripts copy by pointer so that they will share
					// the data interface at runtime and will both be updated in the editor.
					for (FNiagaraScriptDataInterfaceInfo& SpawnDataInterfaceInfo : SpawnScript->GetCachedDefaultDataInterfaces())
					{
						if (SpawnDataInterfaceInfo.RegisteredParameterMapWrite == NAME_None && UpdateDataInterfaceInfo.RegisteredParameterMapWrite == NAME_None && UpdateDataInterfaceInfo.Name == SpawnDataInterfaceInfo.Name)
						{
							SpawnDataInterfaceInfo.DataInterface = UpdateDataInterfaceInfo.DataInterface;
							break;
						}
					}
				}
			}
		}

		CompileRequest.RootObjects.Empty();

		UpdatePostCompileDIInfo();
		ComputeEmittersExecutionOrder();
		ComputeRenderersDrawOrder();
		CacheFromCompiledData();
		UpdateHasGPUEmitters();
		UpdateDITickFlags();
		ResolveScalabilitySettings();

		const float ElapsedWallTime = (float)(FPlatformTime::Seconds() - CompileRequest.StartTime);
		
		if (bHasCompiledJobs)
		{
			UE_LOG(LogNiagara, Log, TEXT("Compiling System %s took %f sec (time since issued), %f sec (combined shader worker time)."),
				*GetFullName(), ElapsedWallTime, CompileRequest.CombinedCompileTime);
		}
		else if (CompileRequest.bAllScriptsSynchronized == false)
		{
			UE_LOG(LogNiagara, Log, TEXT("Compiling System %s took %f sec, no shader worker used."), *GetFullName(), ElapsedWallTime);
		}

		ActiveCompilations.RemoveAt(0);

		if (bDoPost)
		{
			SCOPE_CYCLE_COUNTER(STAT_Niagara_System_CompileScriptResetAfter);

			BroadcastOnSystemCompiled();
		}

		return true;
	}

	return false;
}

void UNiagaraSystem::BroadcastOnSystemCompiled()
{
	OnSystemCompiled().Broadcast(this);
#if WITH_EDITOR
	IAssetRegistry::GetChecked().AssetTagsFinalized(*this);
#endif
}

void UNiagaraSystem::InitEmitterVariableAliasNames(FNiagaraEmitterCompiledData& EmitterCompiledDataToInit, const UNiagaraEmitter* InAssociatedEmitter)
{
	EmitterCompiledDataToInit.EmitterSpawnIntervalVar.SetName(GetEmitterVariableAliasName(SYS_PARAM_EMITTER_SPAWN_INTERVAL, InAssociatedEmitter));
	EmitterCompiledDataToInit.EmitterInterpSpawnStartDTVar.SetName(GetEmitterVariableAliasName(SYS_PARAM_EMITTER_INTERP_SPAWN_START_DT, InAssociatedEmitter));
	EmitterCompiledDataToInit.EmitterAgeVar.SetName(GetEmitterVariableAliasName(SYS_PARAM_EMITTER_AGE, InAssociatedEmitter));
	EmitterCompiledDataToInit.EmitterSpawnGroupVar.SetName(GetEmitterVariableAliasName(SYS_PARAM_EMITTER_SPAWN_GROUP, InAssociatedEmitter));
	EmitterCompiledDataToInit.EmitterRandomSeedVar.SetName(GetEmitterVariableAliasName(SYS_PARAM_EMITTER_RANDOM_SEED, InAssociatedEmitter));
	EmitterCompiledDataToInit.EmitterInstanceSeedVar.SetName(GetEmitterVariableAliasName(SYS_PARAM_ENGINE_EMITTER_INSTANCE_SEED, InAssociatedEmitter));
	EmitterCompiledDataToInit.EmitterTotalSpawnedParticlesVar.SetName(GetEmitterVariableAliasName(SYS_PARAM_ENGINE_EMITTER_TOTAL_SPAWNED_PARTICLES, InAssociatedEmitter));
}

const FName UNiagaraSystem::GetEmitterVariableAliasName(const FNiagaraVariable& InEmitterVar, const UNiagaraEmitter* InEmitter) const
{
	return FName(*InEmitterVar.GetName().ToString().Replace(TEXT("Emitter."), *(InEmitter->GetUniqueEmitterName() + TEXT("."))));
}

void UNiagaraSystem::InitEmitterDataSetCompiledData(FNiagaraDataSetCompiledData& DataSetToInit, const FNiagaraEmitterHandle& InAssociatedEmitterHandle)
{
	DataSetToInit.Empty();
	const FVersionedNiagaraEmitterData* AssociatedEmitterData = InAssociatedEmitterHandle.GetEmitterData();
	AssociatedEmitterData->GatherCompiledParticleAttributes(DataSetToInit.Variables);

	DataSetToInit.bRequiresPersistentIDs = AssociatedEmitterData->RequiresPersistentIDs() || DataSetToInit.Variables.Contains(SYS_PARAM_PARTICLES_ID);
	DataSetToInit.ID = FNiagaraDataSetID(InAssociatedEmitterHandle.GetIdName(), ENiagaraDataSetType::ParticleData);
	DataSetToInit.SimTarget = AssociatedEmitterData->SimTarget;

	DataSetToInit.BuildLayout();
}

void UNiagaraSystem::PrepareRapidIterationParametersForCompilation()
{
	// prepare rapid iteration parameters for compilation
	TArray<UNiagaraScript*> Scripts;
	TMap<UNiagaraScript*, FVersionedNiagaraEmitter> ScriptToEmitterMap;
	for (int32 i = 0; i < GetEmitterHandles().Num(); i++)
	{
		const FNiagaraEmitterHandle& Handle = GetEmitterHandle(i);
		if (Handle.GetIsEnabled()) // Don't pull in the emitter if it isn't going to be used.
		{
			TArray<UNiagaraScript*> EmitterScripts;
			Handle.GetEmitterData()->GetScripts(EmitterScripts, false, true);

			for (int32 ScriptIdx = 0; ScriptIdx < EmitterScripts.Num(); ScriptIdx++)
			{
				if (EmitterScripts[ScriptIdx])
				{
					Scripts.AddUnique(EmitterScripts[ScriptIdx]);
					ScriptToEmitterMap.Add(EmitterScripts[ScriptIdx], Handle.GetInstance());
				}
			}
		}
	}
	TArray<TTuple<UNiagaraScript*, FVersionedNiagaraEmitter>> ScriptsToIterate;
	for (const auto& Entry : ScriptToEmitterMap)
	{
		ScriptsToIterate.Add(Entry);
	}
    		
	TMap<UNiagaraScript*, UNiagaraScript*> ScriptDependencyMap;
	for (int Index = 0; Index < ScriptsToIterate.Num(); Index++)
	{
		TTuple<UNiagaraScript*, FVersionedNiagaraEmitter> ScriptEmitterPair = ScriptsToIterate[Index];
		UNiagaraScript* CompiledScript = ScriptEmitterPair.Key;
		FVersionedNiagaraEmitter VersionedEmitter = ScriptEmitterPair.Value;

		if (UNiagaraScript::IsEquivalentUsage(CompiledScript->GetUsage(), ENiagaraScriptUsage::EmitterSpawnScript))
		{
			Scripts.AddUnique(SystemSpawnScript);
			ScriptDependencyMap.Add(CompiledScript, SystemSpawnScript);
			if (!ScriptToEmitterMap.Contains(SystemSpawnScript))
			{
				ScriptToEmitterMap.Add(SystemSpawnScript.Get());
				ScriptsToIterate.Emplace(SystemSpawnScript, FVersionedNiagaraEmitter());
			}
		}

		if (UNiagaraScript::IsEquivalentUsage(CompiledScript->GetUsage(), ENiagaraScriptUsage::EmitterUpdateScript))
		{
			Scripts.AddUnique(SystemUpdateScript);
			ScriptDependencyMap.Add(CompiledScript, SystemUpdateScript);
			if (!ScriptToEmitterMap.Contains(SystemUpdateScript))
			{
				ScriptToEmitterMap.Add(SystemUpdateScript.Get());
				ScriptsToIterate.Emplace(SystemUpdateScript, FVersionedNiagaraEmitter());
			}
		}

		FVersionedNiagaraEmitterData* EmitterData = VersionedEmitter.GetEmitterData();
		if (UNiagaraScript::IsEquivalentUsage(CompiledScript->GetUsage(), ENiagaraScriptUsage::ParticleSpawnScript))
		{
			if (EmitterData && EmitterData->SimTarget == ENiagaraSimTarget::GPUComputeSim)
			{
				UNiagaraScript* ComputeScript = EmitterData->GetGPUComputeScript();

				Scripts.AddUnique(ComputeScript);
				ScriptDependencyMap.Add(CompiledScript, ComputeScript);
				if (!ScriptToEmitterMap.Contains(ComputeScript))
				{
					ScriptToEmitterMap.Add(ComputeScript, VersionedEmitter);
					ScriptsToIterate.Emplace(ComputeScript, VersionedEmitter);
				}
			}
		}

		if (UNiagaraScript::IsEquivalentUsage(CompiledScript->GetUsage(), ENiagaraScriptUsage::ParticleUpdateScript))
		{
			if (EmitterData && EmitterData->SimTarget == ENiagaraSimTarget::GPUComputeSim)
			{
				UNiagaraScript* ComputeScript = EmitterData->GetGPUComputeScript();

				Scripts.AddUnique(ComputeScript);
				ScriptDependencyMap.Add(CompiledScript, ComputeScript);
				if (!ScriptToEmitterMap.Contains(ComputeScript))
				{
					ScriptToEmitterMap.Add(ComputeScript, VersionedEmitter);
					ScriptsToIterate.Emplace(ComputeScript, VersionedEmitter);
				}
			}
			else if (EmitterData && EmitterData->bInterpolatedSpawning)
			{
				Scripts.AddUnique(EmitterData->SpawnScriptProps.Script);
				ScriptDependencyMap.Add(CompiledScript, EmitterData->SpawnScriptProps.Script);
				if (!ScriptToEmitterMap.Contains(EmitterData->SpawnScriptProps.Script))
				{
					ScriptToEmitterMap.Add(EmitterData->SpawnScriptProps.Script, VersionedEmitter);
					ScriptsToIterate.Emplace(EmitterData->SpawnScriptProps.Script, VersionedEmitter);
				}
			}
		}
	}
	FNiagaraUtilities::PrepareRapidIterationParameters(Scripts, ScriptDependencyMap, ScriptToEmitterMap);
}


const TSharedPtr<FNiagaraGraphCachedDataBase, ESPMode::ThreadSafe>& UNiagaraSystem::GetCachedTraversalData() const
{
	if (CachedTraversalData.IsValid())
		return CachedTraversalData;

	INiagaraModule& NiagaraModule = FModuleManager::Get().LoadModuleChecked<INiagaraModule>(TEXT("Niagara"));
	CachedTraversalData = NiagaraModule.CacheGraphTraversal(this, FGuid());
	return CachedTraversalData;
}

void UNiagaraSystem::InvalidateCachedData()
{
	CachedTraversalData.Reset();
}

bool UNiagaraSystem::RequestCompile(bool bForce, FNiagaraSystemUpdateContext* OptionalUpdateContext)
{
	check(IsInGameThread());
	TRACE_CPUPROFILER_EVENT_SCOPE(UNiagaraSystem::RequestCompile)

	// We remove emitters and scripts on dedicated servers, so skip further work.
	const bool bIsDedicatedServer = !GIsClient && GIsServer;
	if (bIsDedicatedServer)
	{
		return false;
	}

	if (!AllowShaderCompiling() || GetOutermost()->bIsCookedForEditor)
	{
		return false;
	}

	if (bForce)
	{
		ForceGraphToRecompileOnNextCheck();
	}

	// we can't compile systems that have been cooked without editor data
	if (RootPackageHasAnyFlags(PKG_FilterEditorOnly))
	{
		return false;
	}

	if (bCompilationReentrantGuard)
	{
		return false;
	}

	// we don't want to stack compilations, so we remove requests that have not started processing yet
	for (int i = ActiveCompilations.Num() - 1; i >= 1; i--)
	{
		FNiagaraSystemCompileRequest& Request = ActiveCompilations[i];
		for (auto& AsyncTask : Request.DDCTasks)
		{
			AsyncTask->AbortTask();
		}
		ActiveCompilations.RemoveAt(i);
	}	

	// Record that we entered this function already.
	SCOPE_CYCLE_COUNTER(STAT_Niagara_System_CompileScript);
	bCompilationReentrantGuard = true;
	ON_SCOPE_EXIT { check(bCompilationReentrantGuard == false); };

	FNiagaraSystemCompileRequest& ActiveCompilation = ActiveCompilations.AddDefaulted_GetRef();
	ActiveCompilation.bForced = bForce;
	ActiveCompilation.StartTime = FPlatformTime::Seconds();

	check(SystemSpawnScript->GetLatestSource() == SystemUpdateScript->GetLatestSource());
	PrepareRapidIterationParametersForCompilation();

	TArray<UNiagaraScript*> ScriptsNeedingCompile;
	bool bAnyCompiled = false;
	{
		COOK_STAT(auto Timer = NiagaraScriptCookStats::UsageStats.TimeSyncWork());
		COOK_STAT(Timer.TrackCyclesOnly());

		//Compile all emitters

		// Pass one... determine if any need to be compiled.
		{
			bool bAnyUnsynchronized = false;
			for (int32 i = 0; i < EmitterHandles.Num(); i++)
			{
				FNiagaraEmitterHandle Handle = EmitterHandles[i];
				FVersionedNiagaraEmitterData* EmitterData = Handle.GetEmitterData();
				if (EmitterData && Handle.GetIsEnabled())
				{
					TArray<UNiagaraScript*> EmitterScripts;
					EmitterData->GetScripts(EmitterScripts, false, true);
					check(EmitterScripts.Num() > 0);
					for (UNiagaraScript* EmitterScript : EmitterScripts)
					{
						FEmitterCompiledScriptPair Pair;
						Pair.VersionedEmitter = Handle.GetInstance();
						Pair.CompiledScript = EmitterScript;

						FNiagaraVMExecutableDataId NewID;
						// we need to compute the vmID here to check later in the ddc task before doing the precompile if anything has changed in the meantime
						Pair.CompiledScript->ComputeVMCompilationId(NewID, FGuid());
						Pair.CompileId = NewID;

						TSharedPtr<FNiagaraAsyncCompileTask, ESPMode::ThreadSafe> AsyncTask = MakeShared<FNiagaraAsyncCompileTask, ESPMode::ThreadSafe>(this, EmitterScript->GetPathName(), Pair);
						if (EmitterScript->IsCompilable() && !EmitterScript->AreScriptAndSourceSynchronized())
						{
							ScriptsNeedingCompile.Add(EmitterScript);
							bAnyUnsynchronized = true;
						}
						else
						{
							AsyncTask->CurrentState = ENiagaraCompilationState::Finished;
						}
						AsyncTask->UniqueEmitterName = Handle.GetInstance().Emitter->GetUniqueEmitterName();
						ActiveCompilation.DDCTasks.Add(AsyncTask);
					}
				}
			}

			bAnyCompiled = bAnyUnsynchronized || bForce;

			// Now add the system scripts for compilation...
			{
				FEmitterCompiledScriptPair Pair;
				Pair.VersionedEmitter = FVersionedNiagaraEmitter();
				Pair.CompiledScript = SystemSpawnScript;
				
				FNiagaraVMExecutableDataId NewID;
				// we need to compute the vmID here to check later in the ddc task before doing the precompile if anything has changed in the meantime
				Pair.CompiledScript->ComputeVMCompilationId(NewID, FGuid());
				Pair.CompileId = NewID;

				TSharedPtr<FNiagaraAsyncCompileTask, ESPMode::ThreadSafe> AsyncTask = MakeShared<FNiagaraAsyncCompileTask, ESPMode::ThreadSafe>(this, SystemSpawnScript->GetPathName(), Pair);
				if (!SystemSpawnScript->AreScriptAndSourceSynchronized())
				{
					ScriptsNeedingCompile.Add(SystemSpawnScript);
					bAnyCompiled = true;
				}
				else
				{
					AsyncTask->CurrentState = ENiagaraCompilationState::Finished;
				}
				ActiveCompilation.DDCTasks.Add(AsyncTask);
			}

			{
				FEmitterCompiledScriptPair Pair;
				Pair.VersionedEmitter = FVersionedNiagaraEmitter();
				Pair.CompiledScript = SystemUpdateScript;
				
				FNiagaraVMExecutableDataId NewID;
				// we need to compute the vmID here to check later in the ddc task before doing the precompile if anything has changed in the meantime
				Pair.CompiledScript->ComputeVMCompilationId(NewID, FGuid());
				Pair.CompileId = NewID;

				TSharedPtr<FNiagaraAsyncCompileTask, ESPMode::ThreadSafe> AsyncTask = MakeShared<FNiagaraAsyncCompileTask, ESPMode::ThreadSafe>(this, SystemUpdateScript->GetPathName(), Pair);
				if (!SystemUpdateScript->AreScriptAndSourceSynchronized())
				{
					ScriptsNeedingCompile.Add(SystemUpdateScript);
					bAnyCompiled = true;
				}
				else
				{
					AsyncTask->CurrentState = ENiagaraCompilationState::Finished;
				}
				ActiveCompilation.DDCTasks.Add(AsyncTask);
			}
		}


		// prepare data for any precompile the ddc tasks need to do
		TSharedPtr<FNiagaraLazyPrecompileReference, ESPMode::ThreadSafe> PrecompileReference = MakeShared<FNiagaraLazyPrecompileReference, ESPMode::ThreadSafe>();
		PrecompileReference->System = this;
		PrecompileReference->Scripts = ScriptsNeedingCompile;
		
		for (int32 i = 0; i < EmitterHandles.Num(); i++)
		{
			FNiagaraEmitterHandle Handle = EmitterHandles[i];
			if (Handle.GetIsEnabled() && Handle.GetEmitterData())
			{
				TArray<UNiagaraScript*> EmitterScripts;
				Handle.GetEmitterData()->GetScripts(EmitterScripts, false, true);
				check(EmitterScripts.Num() > 0);
				for (UNiagaraScript* EmitterScript : EmitterScripts)
				{
					PrecompileReference->EmitterScriptIndex.Add(EmitterScript, i);
				}
			}
		}

		// We have previously duplicated all that is needed for compilation, so let's now issue the compile requests!
		for (UNiagaraScript* CompiledScript : ScriptsNeedingCompile)
		{

			const auto InPairs = [&CompiledScript](const TSharedPtr<FNiagaraAsyncCompileTask, ESPMode::ThreadSafe>& Other) -> bool
			{
				return CompiledScript == Other->GetScriptPair().CompiledScript;
			};

			TSharedPtr<FNiagaraAsyncCompileTask, ESPMode::ThreadSafe> AsyncTask = *ActiveCompilation.DDCTasks.FindByPredicate(InPairs);
			AsyncTask->DDCKey = UNiagaraScript::BuildNiagaraDDCKeyString(AsyncTask->GetScriptPair().CompileId);
			AsyncTask->PrecompileReference = PrecompileReference;
			AsyncTask->StartTaskTime = FPlatformTime::Seconds();

			// fire off all the ddc tasks, which will trigger the compilation if the data is not in the ddc
			UE_LOG(LogNiagara, Verbose, TEXT("Scheduling async get task for %s"), *AsyncTask->AssetPath);
			AsyncTask->TaskHandle = GetDerivedDataCacheRef().GetAsynchronous(*AsyncTask->DDCKey, AsyncTask->AssetPath);
		}
	}

	// Now record that we are done with this function.
	bCompilationReentrantGuard = false;

	// We might be able to just complete compilation right now if nothing needed compilation.
	if (ScriptsNeedingCompile.Num() == 0 && ActiveCompilations.Num() == 1)
	{
		ActiveCompilation.bAllScriptsSynchronized = true;
		PollForCompilationComplete();
	}

	
	if (OptionalUpdateContext)
	{
		OptionalUpdateContext->Add(this, true);
	}
	else
	{
		FNiagaraSystemUpdateContext UpdateCtx(this, true);
	}

	return bAnyCompiled;
}

void UNiagaraSystem::InitEmitterCompiledData()
{
	EmitterCompiledData.Empty();
	if (SystemSpawnScript->GetVMExecutableData().IsValid() && SystemUpdateScript->GetVMExecutableData().IsValid())
	{
		TArray<TSharedRef<FNiagaraEmitterCompiledData>> NewEmitterCompiledData;
		for (int32 EmitterIdx = 0; EmitterIdx < EmitterHandles.Num(); ++EmitterIdx)
		{
			NewEmitterCompiledData.Add(MakeShared<FNiagaraEmitterCompiledData>());
		}

		FNiagaraTypeDefinition SpawnInfoDef = FNiagaraTypeDefinition(FNiagaraSpawnInfo::StaticStruct());

		for (FNiagaraVariable& Var : SystemSpawnScript->GetVMExecutableData().Attributes)
		{
			for (int32 EmitterIdx = 0; EmitterIdx < EmitterHandles.Num(); ++EmitterIdx)
			{
				UNiagaraEmitter* Emitter = EmitterHandles[EmitterIdx].GetInstance().Emitter;
				if (Emitter)
				{
					FString EmitterName = Emitter->GetUniqueEmitterName() + TEXT(".");
					if (Var.GetType() == SpawnInfoDef && Var.GetName().ToString().StartsWith(EmitterName))
					{
						NewEmitterCompiledData[EmitterIdx]->SpawnAttributes.AddUnique(Var.GetName());
					}
				}
			}
		}

		for (FNiagaraVariable& Var : SystemUpdateScript->GetVMExecutableData().Attributes)
		{
			for (int32 EmitterIdx = 0; EmitterIdx < EmitterHandles.Num(); ++EmitterIdx)
			{
				UNiagaraEmitter* Emitter = EmitterHandles[EmitterIdx].GetInstance().Emitter;
				if (Emitter)
				{
					FString EmitterName = Emitter->GetUniqueEmitterName() + TEXT(".");
					if (Var.GetType() == SpawnInfoDef && Var.GetName().ToString().StartsWith(EmitterName))
					{
						NewEmitterCompiledData[EmitterIdx]->SpawnAttributes.AddUnique(Var.GetName());
					}
				}
			}
		}

		for (int32 EmitterIdx = 0; EmitterIdx < EmitterHandles.Num(); ++EmitterIdx)
		{
			const FNiagaraEmitterHandle& EmitterHandle = EmitterHandles[EmitterIdx];
			const UNiagaraEmitter* Emitter = EmitterHandle.GetInstance().Emitter;
			FNiagaraDataSetCompiledData& EmitterDataSetCompiledData = NewEmitterCompiledData[EmitterIdx]->DataSetCompiledData;
			FNiagaraDataSetCompiledData& GPUCaptureCompiledData = NewEmitterCompiledData[EmitterIdx]->GPUCaptureDataSetCompiledData;
			if (ensureMsgf(Emitter != nullptr, TEXT("Failed to get Emitter Instance from Emitter Handle in post compile, please investigate.")))
			{
				static FName GPUCaptureDataSetName = TEXT("GPU Capture Dataset");
				InitEmitterVariableAliasNames(NewEmitterCompiledData[EmitterIdx].Get(), Emitter);
				InitEmitterDataSetCompiledData(EmitterDataSetCompiledData, EmitterHandle);
				GPUCaptureCompiledData.ID = FNiagaraDataSetID(GPUCaptureDataSetName, ENiagaraDataSetType::ParticleData);
				GPUCaptureCompiledData.Variables = EmitterDataSetCompiledData.Variables;
				GPUCaptureCompiledData.SimTarget = ENiagaraSimTarget::CPUSim;
				GPUCaptureCompiledData.BuildLayout();				
			}
		}

		for (int32 EmitterIdx = 0; EmitterIdx < EmitterHandles.Num(); ++EmitterIdx)
		{
			EmitterCompiledData.Add(NewEmitterCompiledData[EmitterIdx]);
		}
	}
}

void UNiagaraSystem::InitSystemCompiledData()
{
	SystemCompiledData.InstanceParamStore.Empty();

	ExposedParameters.CopyParametersTo(SystemCompiledData.InstanceParamStore, false, FNiagaraParameterStore::EDataInterfaceCopyMethod::Reference);

	auto CreateDataSetCompiledData = [&](FNiagaraDataSetCompiledData& CompiledData, TConstArrayView<FNiagaraVariable> Vars)
	{
		CompiledData.Empty();

		CompiledData.Variables.Reset(Vars.Num());
		for (const FNiagaraVariable& Var : Vars)
		{
			CompiledData.Variables.AddUnique(Var);
		}

		CompiledData.bRequiresPersistentIDs = false;
		CompiledData.ID = FNiagaraDataSetID();
		CompiledData.SimTarget = ENiagaraSimTarget::CPUSim;

		CompiledData.BuildLayout();
	};

	const FNiagaraVMExecutableData& SystemSpawnScriptData = GetSystemSpawnScript()->GetVMExecutableData();
	const FNiagaraVMExecutableData& SystemUpdateScriptData = GetSystemUpdateScript()->GetVMExecutableData();

	CreateDataSetCompiledData(SystemCompiledData.DataSetCompiledData, SystemUpdateScriptData.Attributes);

	const FNiagaraParameters* EngineParamsSpawn = SystemSpawnScriptData.DataSetToParameters.Find(TEXT("Engine"));
	CreateDataSetCompiledData(SystemCompiledData.SpawnInstanceParamsDataSetCompiledData, EngineParamsSpawn ? TConstArrayView<FNiagaraVariable>(EngineParamsSpawn->Parameters) : TArrayView<FNiagaraVariable>());
	const FNiagaraParameters* EngineParamsUpdate = SystemUpdateScriptData.DataSetToParameters.Find(TEXT("Engine"));
	CreateDataSetCompiledData(SystemCompiledData.UpdateInstanceParamsDataSetCompiledData, EngineParamsUpdate ? TConstArrayView<FNiagaraVariable>(EngineParamsUpdate->Parameters) : TArrayView<FNiagaraVariable>());

	// create the bindings to be used with our constant buffers; geenrating the offsets to/from the data sets; we need
	// editor data to build these bindings because of the constant buffer structs only having their variable definitions
	// with editor data.
	SystemCompiledData.SpawnInstanceGlobalBinding.Build<FNiagaraGlobalParameters>(SystemCompiledData.SpawnInstanceParamsDataSetCompiledData);
	SystemCompiledData.SpawnInstanceSystemBinding.Build<FNiagaraSystemParameters>(SystemCompiledData.SpawnInstanceParamsDataSetCompiledData);
	SystemCompiledData.SpawnInstanceOwnerBinding.Build<FNiagaraOwnerParameters>(SystemCompiledData.SpawnInstanceParamsDataSetCompiledData);

	SystemCompiledData.UpdateInstanceGlobalBinding.Build<FNiagaraGlobalParameters>(SystemCompiledData.UpdateInstanceParamsDataSetCompiledData);
	SystemCompiledData.UpdateInstanceSystemBinding.Build<FNiagaraSystemParameters>(SystemCompiledData.UpdateInstanceParamsDataSetCompiledData);
	SystemCompiledData.UpdateInstanceOwnerBinding.Build<FNiagaraOwnerParameters>(SystemCompiledData.UpdateInstanceParamsDataSetCompiledData);

	const int32 EmitterCount = EmitterHandles.Num();

	SystemCompiledData.SpawnInstanceEmitterBindings.SetNum(EmitterHandles.Num());
	SystemCompiledData.UpdateInstanceEmitterBindings.SetNum(EmitterHandles.Num());

	const FString EmitterNamespace = TEXT("Emitter");
	for (int32 EmitterIdx = 0; EmitterIdx < EmitterCount; ++EmitterIdx)
	{
		const FNiagaraEmitterHandle& PerEmitterHandle = EmitterHandles[EmitterIdx];
		const UNiagaraEmitter* Emitter = PerEmitterHandle.GetInstance().Emitter;
		if (ensureMsgf(Emitter != nullptr, TEXT("Failed to get Emitter Instance from Emitter Handle when post compiling Niagara System %s!"), *GetPathNameSafe(this)))
		{
			const FString EmitterName = Emitter->GetUniqueEmitterName();

			SystemCompiledData.SpawnInstanceEmitterBindings[EmitterIdx].Build<FNiagaraEmitterParameters>(SystemCompiledData.SpawnInstanceParamsDataSetCompiledData, EmitterNamespace, EmitterName);
			SystemCompiledData.UpdateInstanceEmitterBindings[EmitterIdx].Build<FNiagaraEmitterParameters>(SystemCompiledData.UpdateInstanceParamsDataSetCompiledData, EmitterNamespace, EmitterName);
		}
	}

}

void UNiagaraSystem::ResetToEmptySystem()
{
	// in order to handle resetting transient systems that can be created (FNiagaraScriptMergeManager::UpdateModuleVersions) we make sure
	// to mark the system as having been fully loaded to bypass any future work that might be done (generating compile requests)
	bFullyLoaded = true;

	EffectType = nullptr;
	EmitterHandles.Empty();
	ParameterCollectionOverrides.Empty();
	SystemSpawnScript = nullptr;
	SystemUpdateScript = nullptr;
	EmitterCompiledData.Empty();
	SystemCompiledData = FNiagaraSystemCompiledData();
	UserDINamesReadInSystemScripts.Empty();
	EmitterExecutionOrder.Empty();
	RendererPostTickOrder.Empty();
	RendererCompletionOrder.Empty();

	// while we'd like to remove these as well, BP will generate warnings for missing parameters
	//ExposedParameters = FNiagaraUserRedirectionParameterStore();
}

#endif

TStatId UNiagaraSystem::GetStatID(bool bGameThread, bool bConcurrent)const
{
#if STATS
	if (!StatID_GT.IsValidStat())
	{
		GenerateStatID();
	}

	if (bGameThread)
	{
		if (bConcurrent)
		{
			return StatID_GT_CNC;
		}
		else
		{
			return StatID_GT;
		}
	}
	else
	{
		if(bConcurrent)
		{
			return StatID_RT_CNC;
		}
		else
		{
			return StatID_RT;
		}
	}
#endif
	return static_cast<const UObjectBaseUtility*>(this)->GetStatID();
}

void UNiagaraSystem::AddToInstanceCountStat(int32 NumInstances, bool bSolo)const
{
#if STATS
	if (!StatID_GT.IsValidStat())
	{
		GenerateStatID();
	}

	if (FThreadStats::IsCollectingData())
	{
		if (bSolo)
		{
			FThreadStats::AddMessage(StatID_InstanceCountSolo.GetName(), EStatOperation::Add, int64(NumInstances));
			TRACE_STAT_ADD(StatID_InstanceCountSolo.GetName(), int64(NumInstances));
		}
		else
		{
			FThreadStats::AddMessage(StatID_InstanceCount.GetName(), EStatOperation::Add, int64(NumInstances));
			TRACE_STAT_ADD(StatID_InstanceCount.GetName(), int64(NumInstances));
		}
	}
#endif
}

void UNiagaraSystem::AsyncOptimizeAllScripts()
{
	check(IsInGameThread());

	// Optimize is either in flight or done
	if (bNeedsAsyncOptimize == false)
	{
		return;
	}

	if ( !IsReadyToRun() )
	{
		return;
	}

	FGraphEventArray Prereqs;
	ForEachScript(
		[&](UNiagaraScript* Script)
		{
			// Kick off the async optimize, which we'll wait on when the script is actually needed
			if (Script != nullptr)
			{
				FGraphEventRef CompletionEvent = Script->HandleByteCodeOptimization(false);
				if (CompletionEvent.IsValid())
				{
					Prereqs.Add(CompletionEvent);
				}
			}
		}
	);

	if ( Prereqs.Num() > 0 )
	{
		DECLARE_CYCLE_STAT(TEXT("FNullGraphTask.NiagaraScriptOptimizationCompletion"), STAT_FNullGraphTask_NiagaraScriptOptimizationCompletion, STATGROUP_TaskGraphTasks);
		ScriptOptimizationCompletionEvent = TGraphTask<FNullGraphTask>::CreateTask(&Prereqs, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(GET_STATID(STAT_FNullGraphTask_NiagaraScriptOptimizationCompletion), ENamedThreads::AnyThread);

		// Inject task to clear out the reference to the graph task
		FGraphEventArray CompleteTaskPrereqs({ ScriptOptimizationCompletionEvent });
		TGraphTask<FNiagaraOptimizeCompleteTask>::CreateTask(&CompleteTaskPrereqs, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(this, &ScriptOptimizationCompletionEvent);
	}

	bNeedsAsyncOptimize = false;
}

void UNiagaraSystem::GenerateStatID()const
{
#if STATS
	StatID_GT = FDynamicStats::CreateStatId<FStatGroup_STATGROUP_NiagaraSystems>(GetPathName() + TEXT(" [GT]"));
	StatID_GT_CNC = FDynamicStats::CreateStatId<FStatGroup_STATGROUP_NiagaraSystems>(GetPathName() + TEXT(" [GT_CNC]"));
	StatID_RT = FDynamicStats::CreateStatId<FStatGroup_STATGROUP_NiagaraSystems>(GetPathName() + TEXT(" [RT]"));
	StatID_RT_CNC = FDynamicStats::CreateStatId<FStatGroup_STATGROUP_NiagaraSystems>(GetPathName() + TEXT(" [RT_CNC]"));

	StatID_InstanceCount = FDynamicStats::CreateStatIdInt64<FStatGroup_STATGROUP_NiagaraSystemCounts>(GetPathName());
	StatID_InstanceCountSolo = FDynamicStats::CreateStatIdInt64<FStatGroup_STATGROUP_NiagaraSystemCounts>(GetPathName() + TEXT(" [SOLO]"));

#endif
}

UNiagaraEffectType* UNiagaraSystem::GetEffectType()const
{
	return EffectType;
}

const FNiagaraSystemScalabilityOverride& UNiagaraSystem::GetCurrentOverrideSettings() const
{
	if(bOverrideScalabilitySettings)
	{
		for (const FNiagaraSystemScalabilityOverride& Override : SystemScalabilityOverrides.Overrides)
		{
			if (Override.Platforms.IsActive())
			{
				return Override;
			}
		}
	}

	static FNiagaraSystemScalabilityOverride Dummy;
	return Dummy;
}

#if WITH_EDITOR
void UNiagaraSystem::SetEffectType(UNiagaraEffectType* InEffectType)
{
	if (InEffectType != EffectType)
	{
		Modify();
		EffectType = InEffectType;
		ResolveScalabilitySettings();
		FNiagaraSystemUpdateContext UpdateCtx;
		UpdateCtx.Add(this, true);
	}	
}


void UNiagaraSystem::GatherStaticVariables(TArray<FNiagaraVariable>& OutVars, TArray<FNiagaraVariable>& OutEmitterVars) const
{
	TArray<UNiagaraScript*> OutScripts;
	OutScripts.Add(SystemSpawnScript);
	OutScripts.Add(SystemUpdateScript);

	auto GatherFromScripts = [&](TArray<UNiagaraScript*> Scripts, TArray<FNiagaraVariable>& Vars)
	{
		for (UNiagaraScript* Script : Scripts)
		{
			TArray<FNiagaraVariable> StoreParams;
			Script->RapidIterationParameters.GetParameters(StoreParams);

			for (int32 i = 0; i < StoreParams.Num(); i++)
			{
				if (StoreParams[i].GetType().IsStatic())
				{
					const int32* Index = Script->RapidIterationParameters.FindParameterOffset(StoreParams[i]);
					if (Index != nullptr)
					{
						StoreParams[i].SetData(Script->RapidIterationParameters.GetParameterData(*Index)); // This will memcopy the data in.
						Vars.AddUnique(StoreParams[i]);

						//UE_LOG(LogNiagara, Log, TEXT("UNiagaraSystem::GatherStaticVariables Added %s"), *StoreParams[i].ToString());
					}
				}
			}
		}
	};

	GatherFromScripts(OutScripts, OutVars);

	TArray<UNiagaraScript*> EmitterScripts; 
	//const TArray<FNiagaraEmitterHandle>& EmitterHandles = GetEmitterHandles();
	for (int32 i = 0; i < EmitterHandles.Num(); i++)
	{
		const FNiagaraEmitterHandle& Handle = EmitterHandles[i];
		FVersionedNiagaraEmitterData* EmitterData = Handle.GetEmitterData();
		if (EmitterData && Handle.GetIsEnabled())
		{
			EmitterScripts.Add(EmitterData->EmitterSpawnScriptProps.Script);
			EmitterScripts.Add(EmitterData->EmitterUpdateScriptProps.Script);
		}		
	}

	GatherFromScripts(EmitterScripts, OutEmitterVars);
}
#endif

void UNiagaraSystem::ResolveScalabilitySettings()
{
	CurrentScalabilitySettings.Clear();
	if (UNiagaraEffectType* ActualEffectType = GetEffectType())
	{
		CurrentScalabilitySettings = ActualEffectType->GetActiveSystemScalabilitySettings();
		bAllowCullingForLocalPlayers = ActualEffectType->bAllowCullingForLocalPlayers;
	}

	if (bOverrideScalabilitySettings)
	{
		for (FNiagaraSystemScalabilityOverride& Override : SystemScalabilityOverrides.Overrides)
		{
			if (Override.Platforms.IsActive())
			{
				if (Override.bOverrideDistanceSettings)
				{
					CurrentScalabilitySettings.bCullByDistance = Override.bCullByDistance;
					CurrentScalabilitySettings.MaxDistance = Override.MaxDistance;
				}

				if (Override.bOverrideInstanceCountSettings)
				{
					CurrentScalabilitySettings.bCullMaxInstanceCount = Override.bCullMaxInstanceCount;
					CurrentScalabilitySettings.MaxInstances = Override.MaxInstances;
				}

				if (Override.bOverridePerSystemInstanceCountSettings)
				{
					CurrentScalabilitySettings.bCullPerSystemMaxInstanceCount = Override.bCullPerSystemMaxInstanceCount;
					CurrentScalabilitySettings.MaxSystemInstances = Override.MaxSystemInstances;
				}

				if (Override.bOverrideVisibilitySettings)
				{
					CurrentScalabilitySettings.VisibilityCulling.bCullWhenNotRendered = Override.VisibilityCulling.bCullWhenNotRendered;
					CurrentScalabilitySettings.VisibilityCulling.bCullByViewFrustum = Override.VisibilityCulling.bCullByViewFrustum;
					CurrentScalabilitySettings.VisibilityCulling.bAllowPreCullingByViewFrustum = Override.VisibilityCulling.bAllowPreCullingByViewFrustum;
					CurrentScalabilitySettings.VisibilityCulling.MaxTimeWithoutRender = Override.VisibilityCulling.MaxTimeWithoutRender;
					CurrentScalabilitySettings.VisibilityCulling.MaxTimeOutsideViewFrustum = Override.VisibilityCulling.MaxTimeOutsideViewFrustum;
				}

 				if (Override.bOverrideGlobalBudgetScalingSettings)
				{
 					CurrentScalabilitySettings.BudgetScaling = Override.BudgetScaling; 
 				}

				if (Override.bOverrideCullProxySettings)
				{
					CurrentScalabilitySettings.CullProxyMode = Override.CullProxyMode;
					CurrentScalabilitySettings.MaxSystemProxies = Override.MaxSystemProxies;
				}

				break;//These overrides *should* be for orthogonal platform sets so we can exit after we've found a match.
			}
		}

		if (bOverrideAllowCullingForLocalPlayers)
		{
			bAllowCullingForLocalPlayers = bAllowCullingForLocalPlayersOverride;
		}
	}

	CurrentScalabilitySettings.MaxDistance = FMath::Max(GNiagaraScalabiltiyMinumumMaxDistance, CurrentScalabilitySettings.MaxDistance);

	//Work out if this system needs to have sorted significance culling done.
	bNeedsSortedSignificanceCull = false;

	if (CurrentScalabilitySettings.bCullMaxInstanceCount || CurrentScalabilitySettings.bCullPerSystemMaxInstanceCount || CurrentScalabilitySettings.CullProxyMode != ENiagaraCullProxyMode::None)
	{
		bNeedsSortedSignificanceCull = true;
	}
	else
	{
		//If we're not using it at the system level, maybe one of the emitters is.
		auto ScriptUsesSigIndex = [&](UNiagaraScript* Script)
		{
			if (Script && bNeedsSortedSignificanceCull == false)//Skip if we've already found one using it.
			{
				bNeedsSortedSignificanceCull = Script->GetVMExecutableData().bReadsSignificanceIndex;
			}
		};
		ForEachScript(ScriptUsesSigIndex);
	}

	FNiagaraWorldManager::InvalidateCachedSystemScalabilityDataForAllWorlds();
}

void UNiagaraSystem::UpdateScalability()
{
	CacheFromCompiledData();

	ResolveScalabilitySettings();

	for (FNiagaraEmitterHandle& Handle : EmitterHandles)
	{
		if (UNiagaraEmitter* Emitter = Handle.GetInstance().Emitter)
		{
			Emitter->UpdateScalability();
		}
	}

#if WITH_EDITOR
	OnScalabilityChangedDelegate.Broadcast();
#endif

	// Update components
	{
		FNiagaraSystemUpdateContext UpdateCtx;
		UpdateCtx.SetDestroyOnAdd(true);
		UpdateCtx.SetOnlyActive(true);
		UpdateCtx.Add(this, true);
	}

	// Re-prime the component pool
	if (PoolPrimeSize > 0 && MaxPoolSize > 0)
	{
		FNiagaraWorldManager::PrimePoolForAllWorlds(this);
	}
}

const FString& UNiagaraSystem::GetCrashReporterTag()const
{
	if (CrashReporterTag.IsEmpty())
	{
		CrashReporterTag = FString::Printf(TEXT("| System: %s |"), *GetFullName());
	}
	return CrashReporterTag;
}

FNiagaraEmitterCompiledData::FNiagaraEmitterCompiledData()
{
	EmitterSpawnIntervalVar = SYS_PARAM_EMITTER_SPAWN_INTERVAL;
	EmitterInterpSpawnStartDTVar = SYS_PARAM_EMITTER_INTERP_SPAWN_START_DT;
	EmitterAgeVar = SYS_PARAM_EMITTER_AGE;
	EmitterSpawnGroupVar = SYS_PARAM_EMITTER_SPAWN_GROUP;
	EmitterRandomSeedVar = SYS_PARAM_EMITTER_RANDOM_SEED;
	EmitterInstanceSeedVar = SYS_PARAM_ENGINE_EMITTER_INSTANCE_SEED;
	EmitterTotalSpawnedParticlesVar = SYS_PARAM_ENGINE_EMITTER_TOTAL_SPAWNED_PARTICLES;
}

#if WITH_EDITORONLY_DATA
void UNiagaraSystem::FixupPositionUserParameters()
{
	TArray<FNiagaraVariable> UserParameters;
	ExposedParameters.GetUserParameters(UserParameters);
	UserParameters = UserParameters.FilterByPredicate([](const FNiagaraVariable& Var) { return Var.GetType() == FNiagaraTypeDefinition::GetVec3Def(); });

	if (UserParameters.Num() == 0)
	{
		return;
	}
	for (FNiagaraVariable& UserParameter : UserParameters)
	{
		ExposedParameters.MakeUserVariable(UserParameter);
	}
	
	TSet<FNiagaraVariable> LinkedPositionInputs;
	ForEachScript([&UserParameters, &LinkedPositionInputs](UNiagaraScript* Script)
	{
		if (Script)
		{
			Script->ConditionalPostLoad();
			if (UNiagaraScriptSourceBase* LatestSource = Script->GetLatestSource())
			{
				LatestSource->GetLinkedPositionTypeInputs(UserParameters, LinkedPositionInputs);
			}
		}
	});

	// check if any of the renderers have a user param bound to a position binding
	for (FNiagaraEmitterHandle& EmitterHandle : EmitterHandles)
	{
		if (FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData())
		{
			for (UNiagaraRendererProperties* Renderer : EmitterData->GetRenderers())
			{
				for (const FNiagaraVariableAttributeBinding* AttributeBinding : Renderer->GetAttributeBindings())
				{
					if (AttributeBinding == nullptr)
					{
						continue;
					}
					if (AttributeBinding->GetBindingSourceMode() == ExplicitUser && AttributeBinding->GetType() == FNiagaraTypeDefinition::GetPositionDef())
					{
						for (FNiagaraVariable UserParam : UserParameters)
						{
							if (UserParam.GetName() == AttributeBinding->GetParamMapBindableVariable().GetName() && !LinkedPositionInputs.Contains(UserParam))
							{
								LinkedPositionInputs.Add(UserParam);
								break;
							}
						}
					}
				}
			}
		}
	}

	// looks like we have a few FVector3f user parameters that are linked to position inputs in the stack.
	// Most likely this is because core modules were changed to use position data. To ease the transition of old assets, we auto-convert those inputs to position types as well.
	for (const FNiagaraVariable& LinkedParameter : LinkedPositionInputs)
	{
		// we need to go over the scripts again to fix existing usages. Just because it's linked to a position in one input doesn't mean that's the case for all of them and we don't want to end up with
		// different types of the same user parameter linked throughout the system.
		ForEachScript([&LinkedParameter](UNiagaraScript* Script)
		{
			if (Script && Script->GetLatestSource() != nullptr)
			{
				Script->GetLatestSource()->ChangedLinkedInputTypes(LinkedParameter, FNiagaraTypeDefinition::GetPositionDef());
			}
		});

		ExposedParameters.ConvertParameterType(LinkedParameter, FNiagaraTypeDefinition::GetPositionDef());
		UE_LOG(LogNiagara, Log, TEXT("Converted parameter %s from vec3 to position type in asset %s"), *LinkedParameter.GetName().ToString(), *GetPathName());
	}
}

void FNiagaraParameterDataSetBindingCollection::BuildInternal(const TArray<FNiagaraVariable>& ParameterVars, const FNiagaraDataSetCompiledData& DataSet, const FString& NamespaceBase, const FString& NamespaceReplacement)
{
	// be sure to reset the offsets first
	FloatOffsets.Empty();
	Int32Offsets.Empty();

	const bool DoNameReplacement = !NamespaceBase.IsEmpty() && !NamespaceReplacement.IsEmpty();

	int32 ParameterOffset = 0;
	for (FNiagaraVariable Var : ParameterVars)
	{
		if (DoNameReplacement)
		{
			const FString ParamName = Var.GetName().ToString().Replace(*NamespaceBase, *NamespaceReplacement);
			Var.SetName(*ParamName);
		}

		int32 VariableIndex = DataSet.Variables.IndexOfByKey(Var);

		if (DataSet.VariableLayouts.IsValidIndex(VariableIndex))
		{
			const FNiagaraVariableLayoutInfo& Layout = DataSet.VariableLayouts[VariableIndex];
			int32 NumFloats = 0;
			int32 NumInts = 0;

			for (uint32 CompIdx = 0; CompIdx < Layout.GetNumFloatComponents(); ++CompIdx)
			{
				int32 ParamOffset = ParameterOffset + Layout.LayoutInfo.FloatComponentByteOffsets[CompIdx];
				int32 DataSetOffset = Layout.FloatComponentStart + NumFloats++;
				auto& Binding = FloatOffsets.AddDefaulted_GetRef();
				Binding.ParameterOffset = ParamOffset;
				Binding.DataSetComponentOffset = DataSetOffset;
			}
			for (uint32 CompIdx = 0; CompIdx < Layout.GetNumInt32Components(); ++CompIdx)
			{
				int32 ParamOffset = ParameterOffset + Layout.LayoutInfo.Int32ComponentByteOffsets[CompIdx];
				int32 DataSetOffset = Layout.Int32ComponentStart + NumInts++;
				auto& Binding = Int32Offsets.AddDefaulted_GetRef();
				Binding.ParameterOffset = ParamOffset;
				Binding.DataSetComponentOffset = DataSetOffset;
			}
		}

		const int32 ParameterSize = Var.GetSizeInBytes();
		ParameterOffset += ParameterSize;
	}

	FloatOffsets.Shrink();
	Int32Offsets.Shrink();
}

UNiagaraBakerSettings* UNiagaraSystem::GetBakerSettings()
{
	if ( BakerSettings == nullptr )
	{
		BakerSettings = NewObject<UNiagaraBakerSettings>(this, "BakerSettings", RF_Transactional);
	}
	return BakerSettings;
}
#endif

#undef LOCTEXT_NAMESPACE // NiagaraSystem

