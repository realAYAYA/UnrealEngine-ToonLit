// Copyright Epic Games, Inc. All Rights Reserved.


#include "NiagaraSystem.h"

#include "EngineAnalytics.h"
#include "Components/PrimitiveComponent.h"
#include "NiagaraSystemImpl.h"

#include "AssetRegistry/IAssetRegistry.h"
#include "INiagaraEditorOnlyDataUtlities.h"
#include "NiagaraAnalytics.h"
#include "Materials/MaterialInterface.h"
#include "Stateless/NiagaraStatelessEmitter.h"
#include "NiagaraAsyncCompile.h"
#include "NiagaraConstants.h"
#include "NiagaraBakerSettings.h"
#include "NiagaraCompilationTypes.h"
#include "NiagaraCustomVersion.h"
#include "NiagaraDataInterfaceUtilities.h"
#include "NiagaraEditorDataBase.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraModule.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraResolveDIHelpers.h"
#include "NiagaraScratchPadContainer.h"
#include "NiagaraScriptSourceBase.h"
#include "NiagaraSettings.h"
#include "NiagaraShared.h"
#include "NiagaraSimulationStageBase.h"
#include "NiagaraStats.h"
#include "NiagaraSystemStaticBuffers.h"
#include "NiagaraTrace.h"
#include "NiagaraTypes.h"
#include "NiagaraWorldManager.h"

#include "Algo/RemoveIf.h"
#include "Algo/StableSort.h"
#include "HAL/LowLevelMemTracker.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/ScopeExit.h"
#include "Modules/ModuleManager.h"
#include "ProfilingDebugging/CookStats.h"
#include "ProfilingDebugging/ScopedTimers.h"
#include "UObject/AssetRegistryTagsContext.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/LinkerLoad.h"
#include "UObject/Package.h"
#include "PipelineStateCache.h"
#include "NiagaraDataChannel.h"
#include "UObject/UObjectIterator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraSystem)

#define LOCTEXT_NAMESPACE "NiagaraSystem"

#if WITH_EDITOR
#endif

DECLARE_CYCLE_STAT(TEXT("Niagara - System - CompileScript"), STAT_Niagara_System_CompileScript, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("Niagara - System - CompileScript_ResetAfter"), STAT_Niagara_System_CompileScriptResetAfter, STATGROUP_Niagara);

#if ENABLE_COOK_STATS
namespace NiagaraScriptCookStats
{
	extern FCookStats::FDDCResourceUsageStats UsageStats;

	static double NiagaraSystemWaitForCompilationCompleteTime = 0.0;
	static FCookStatsManager::FAutoRegisterCallback RegisterNiagaraSystemCookStats([](FCookStatsManager::AddStatFuncRef AddStat)
	{
		AddStat(TEXT("Niagara"), FCookStatsManager::CreateKeyValueArray(TEXT("NiagaraSystemWaitForCompilationCompleteTime"), NiagaraSystemWaitForCompilationCompleteTime));
	});
}
#endif

namespace NiagaraSystemPrivate
{
	static const FName NAME_ActiveEmitters("ActiveEmitters");
	static const FName NAME_ActiveRenderers("ActiveRenderers");
	static const FName NAME_GPUSimsMissingFixedBounds("GPUSimsMissingFixedBounds");
	static const FName NAME_EffectType("EffectType");
	static const FName NAME_WarmupTime("WarmupTime");
	static const FName NAME_HasOverrideScalabilityForSystem("HasOverrideScalabilityForSystem");
	static const FName NAME_HasDIsWithPostSimulateTick("HasDIsWithPostSimulateTick");
	static const FName NAME_NeedsSortedSignificanceCull("NeedsSortedSignificanceCull");
	static const FName NAME_ActiveDIs("ActiveDIs");
	static const FName NAME_HasGPUEmitter("HasGPUEmitter");
	static const FName NAME_FixedBoundsSize("FixedBoundsSize");
	static const FName NAME_NumEmitters("NumEmitters");
}

//Disable for now until we can spend more time on a good method of applying the data gathered.
int32 GEnableNiagaraRuntimeCycleCounts = 0;
static FAutoConsoleVariableRef CVarEnableNiagaraRuntimeCycleCounts(TEXT("fx.EnableNiagaraRuntimeCycleCounts"), GEnableNiagaraRuntimeCycleCounts, TEXT("Toggle for runtime cylce counts tracking Niagara's frame time. \n"), ECVF_ReadOnly);

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

#if WITH_EDITORONLY_DATA
static int GNiagaraOnDemandCompileEnabled = 1;
static FAutoConsoleVariableRef CVarNiagaraOnDemandCompileEnabled(
	TEXT("fx.Niagara.OnDemandCompileEnabled"),
	GNiagaraOnDemandCompileEnabled,
	TEXT("Compiles Niagara Systems on demand rather than on post load."),
	ECVF_Default
);

static int GNiagaraObjectNeedsLoadMode = 1;
static FAutoConsoleVariableRef CVarNiagaraObjectNeedsLoadMode(
	TEXT("fx.Niagara.ObjectNeedsLoadMode"),
	GNiagaraObjectNeedsLoadMode,
	TEXT("How we decide to handle objects that need loading\n")
	TEXT("0 - Do nothing\n")
	TEXT("1 - Validate objects are loaded\n")
	TEXT("2 - Validate objects are loaded and force preload\n"),
	ECVF_Default
);

static int GNiagaraReportOnCook = 1;
static FAutoConsoleVariableRef CVarNiagaraReportOnCook(
	TEXT("fx.Niagara.Analytics.ReportOnCook"),
	GNiagaraReportOnCook,
	TEXT("If true then basic system info will be gathered and reported as part of the editor analytics for every cooked system."),
	ECVF_Default
);
#endif

//////////////////////////////////////////////////////////////////////////

class FNiagaraSystemNullCompletionTask
{
public:
	FNiagaraSystemNullCompletionTask(UNiagaraSystem* Owner, FGraphEventRef* InRefToClear)
		: WeakOwner(Owner)
		, RefToClear(InRefToClear)
		, ReferenceValue(InRefToClear != nullptr ? InRefToClear->GetReference() : nullptr)
	{
	}

	FORCEINLINE TStatId GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(FNiagaraSystemNullCompletionTask, STATGROUP_TaskGraphTasks); }
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

FNiagaraSystemScalabilityOverrides::FNiagaraSystemScalabilityOverrides() = default;
FNiagaraSystemScalabilityOverrides::~FNiagaraSystemScalabilityOverrides() = default;

//////////////////////////////////////////////////////////////////////////

UNiagaraSystem::UNiagaraSystem(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
#if WITH_EDITORONLY_DATA
, LibraryVisibility(ENiagaraScriptLibraryVisibility::Unexposed)
, bBakeOutRapidIteration(false)
, bBakeOutRapidIterationOnCook(true)
, bTrimAttributes(false)
, bTrimAttributesOnCook(true)
, bIgnoreParticleReadsForAttributeTrim(false)
, bDisableDebugSwitches(false)
, bDisableDebugSwitchesOnCook(true)
, bNeedsRequestCompile(false)
, bCompileForEdit(false)
#endif
, bSupportLargeWorldCoordinates(true)
, bDisableExperimentalVM(false)
, CustomDepthStencilWriteMask(ERendererStencilMask::ERSM_Default)
, bFixedBounds(false)
#if WITH_EDITORONLY_DATA
, bIsolateEnabled(false)
#endif
, FixedBounds(FBox(FVector(-100), FVector(100)))
, bNeedsGPUContextInitForDataInterfaces(false)
, bNeedsAsyncOptimize(true)
, CurrentScalabilitySettings(*new FNiagaraSystemScalabilitySettings())
, bHasDIsWithPostSimulateTick(false)
, bAllDIsPostSimulateCanOverlapFrames(true)
, bAllDIsPostStageCanOverlapTickGroups(true)
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
	, CurrentScalabilitySettings(*new FNiagaraSystemScalabilitySettings())
{
}

UNiagaraSystem::~UNiagaraSystem()
{
	delete &CurrentScalabilitySettings;
}

void UNiagaraSystem::FStaticBuffersDeletor::operator()(FNiagaraSystemStaticBuffers* Ptr) const
{
	ENQUEUE_RENDER_COMMAND(ScriptSafeDelete)(
		[RT_Ptr = Ptr](FRHICommandListImmediate& RHICmdList)
		{
			delete RT_Ptr;
		}
	);
}

void UNiagaraSystem::BeginDestroy()
{
	Super::BeginDestroy();

#if WITH_EDITORONLY_DATA
	KillAllActiveCompilations();
#endif

#if WITH_EDITOR
	CleanupDefinitionsSubscriptions();
#endif

	FNiagaraWorldManager::DestroyAllSystemSimulations(this);

	// Ensure we wait for any potential render commands to finish executing before we GC the NiagaraSystem
	WaitRenderCommandsFence.BeginFence();
}

bool UNiagaraSystem::IsReadyForFinishDestroy()
{
	const bool bReady = Super::IsReadyForFinishDestroy();
	return bReady && WaitRenderCommandsFence.IsFenceComplete();
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

	if (bNeedsRequestCompile)
	{
		RequestCompile(false, nullptr, TargetPlatform);
	}

	// check if any of the active compilations requires waiting - note that none should but the original
	// implementation does as it will stall the DDC fill jobs.  Unclear exactly why, but likely because of
	// the need to tick some of the managers
	bool bWaitForCompilationComplete = false;
	for (const TUniquePtr<FNiagaraActiveCompilation>& ActiveCompilation : ActiveCompilations)
	{
		if (ActiveCompilation->BlocksBeginCacheForCooked())
		{
			bWaitForCompilationComplete = true;
		}
	}

	if (bWaitForCompilationComplete)
	{
		WaitForCompilationComplete();
	}
	ReportAnalyticsData(true);
}

bool UNiagaraSystem::IsCachedCookedPlatformDataLoaded(const ITargetPlatform* TargetPlatform)
{
	if (!Super::IsCachedCookedPlatformDataLoaded(TargetPlatform))
	{
		return false;
	}

	if (!ActiveCompilations.IsEmpty())
	{
		return PollForCompilationComplete(false);
	}

	return true;
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
		if ( EmitterHandle.GetInstance().Emitter )
		{
			OutSubscribers.Add(EmitterHandle.GetInstance().Emitter);
		}
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

#if 0
namespace FNiagaraSystemParameterUtilities
{
	void DumpParameterStateToLog(FString Message) const
	{
		UE_LOG(LogNiagara, Log, *Message);
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
	}
}
#endif

void UNiagaraSystem::UpdateSystemAfterLoad()
{
	// guard against deadlocks by having wait called on it during the update
	if (bFullyLoaded)
	{
		return;
	}
	bFullyLoaded = true;

	// Ensure parameter stores have been post loaded inside the script otherwise searching them will be invalid
	if (SystemSpawnScript)
	{
		SystemSpawnScript->ConditionalPostLoad();
	}
	if (SystemUpdateScript)
	{
		SystemUpdateScript->ConditionalPostLoad();
	}

#if WITH_EDITORONLY_DATA
	if (SystemSpawnScript && SystemUpdateScript && !GetOutermost()->bIsCookedForEditor)
	{
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
		bAllowValidation = false;
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
			SystemScriptSource = SystemSpawnScript->GetLatestSource();
		}
		AllSystemScripts.Add(SystemSpawnScript);

		if (SystemUpdateScript == nullptr)
		{
			SystemUpdateScript = NewObject<UNiagaraScript>(this, "SystemUpdateScript", RF_Transactional);
			SystemUpdateScript->SetUsage(ENiagaraScriptUsage::SystemUpdateScript);
			SystemUpdateScript->SetLatestSource(SystemScriptSource);
		}
		AllSystemScripts.Add(SystemUpdateScript);

		// Synchronize with parameter definitions
		PostLoadDefinitionsSubscriptions();

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

		ResolveRequiresScripts();
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
	bNeedsRequestCompile = false;
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
			bNeedsRequestCompile = true;
			if (IsRunningCommandlet())
			{
				// Call modify here so that the system will resave the compile ids and script vm when running the resave
				// commandlet. We don't need it for normal post-loading.
				Modify();
			}
			if ( !GIsEditor || GNiagaraOnDemandCompileEnabled == 0 )
			{
				RequestCompile(false);
			}
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

void UNiagaraSystem::ReportAnalyticsData(bool bIsCooking)
{
	if (FEngineAnalytics::IsAvailable() && IsAsset() && (bIsCooking == false || GNiagaraReportOnCook))
	{
		TArray<FAnalyticsEventAttribute> Attributes;
		Attributes.Emplace(TEXT("FromCook"), bIsCooking);
		Attributes.Emplace(TEXT("EmitterCount"), EmitterHandles.Num());
		Attributes.Emplace(TEXT("UserParameterCount"), GetExposedParameters().Num());
		Attributes.Emplace(TEXT("IsNiagaraAsset"), NiagaraAnalytics::IsPluginAsset(this));
		Attributes.Emplace(TEXT("AssetPathHash"), GetTypeHash(GetPathName()));
		
		// gather data interface data
		TSet<UNiagaraDataInterface*> DataInterfaces;
		TArray<FString> DataInterfaceClasses;
		FNiagaraDataInterfaceUtilities::FDataInterfaceSearchOptions SearchOptions;
		SearchOptions.bIncludeInternal = true;
		ForEachDataInterface(this, [&](const FNiagaraDataInterfaceUtilities::FDataInterfaceUsageContext& UsageContext) -> bool
		{
			if (UsageContext.DataInterface && !DataInterfaces.Contains(UsageContext.DataInterface))
			{
				DataInterfaces.Add(UsageContext.DataInterface);

				if (NiagaraAnalytics::IsPluginClass(UsageContext.DataInterface->GetClass()))
				{
					DataInterfaceClasses.AddUnique(UsageContext.DataInterface->GetClass()->GetName());
				}
			}
			return true;
		}, SearchOptions);
		DataInterfaceClasses.Sort();
		Attributes.Emplace(TEXT("DataInterfaceCount"), DataInterfaces.Num());
		Attributes.Emplace(TEXT("ExposedDataInterfaceCount"), GetExposedParameters().GetDataInterfaces().Num());
		Attributes.Add(FAnalyticsEventAttribute(TEXT("DataInterfaces"), DataInterfaceClasses));

		// gather emitter data
		int32 GpuCount = 0;
		int32 DisabledEmitters = 0;
		int32 StatelessEmitters = 0;
		int32 RendererCount = 0;
		int32 CustomRendererCount = 0;
		int32 DisabledRendererCount = 0;
		int32 ParentEmitterCount = 0;
		int32 ScratchPadCount = ScratchPadScripts.Num();
		TArray<FString> ParentEmitters;
		FNiagaraScriptSourceAnalytics ScriptAnalytics;
		for (const FNiagaraEmitterHandle& Handle : EmitterHandles)
		{
			if (!Handle.GetIsEnabled())
			{
				DisabledEmitters++;
			}
			if (Handle.GetEmitterMode() == ENiagaraEmitterMode::Stateless)
			{
				StatelessEmitters++;
			}
			if (FVersionedNiagaraEmitterData* EmitterData = Handle.GetInstance().GetEmitterData())
			{
				if (EmitterData->SimTarget == ENiagaraSimTarget::GPUComputeSim)
				{
					GpuCount++;
				}
				
				if (EmitterData->GraphSource && Handle.GetIsEnabled())
				{
					EmitterData->GraphSource->ReportAnalyticsData(ScriptAnalytics);
					EmitterData->ForEachRenderer([&](UNiagaraRendererProperties* Renderer)
					{
						RendererCount++;
						if (Renderer && !Renderer->GetIsEnabled())
						{
							DisabledRendererCount++;
						}
						if (Renderer && !NiagaraAnalytics::IsPluginClass(Renderer->GetClass()))
						{
							CustomRendererCount++;
						}
					});
					ScratchPadCount += EmitterData->ScratchPads->Scripts.Num();

					if (EmitterData->GetParent().GetEmitterData())
					{
						ParentEmitterCount++;
						if (NiagaraAnalytics::IsPluginAsset(EmitterData->GetParent().Emitter))
						{
							FNiagaraAssetVersion AssetVersion = EmitterData->GetParent().GetEmitterData()->Version;
							ParentEmitters.AddUnique(FString::Format(TEXT("{0}:{1}.{2}"), {GetPathNameSafe(EmitterData->GetParent().Emitter->GetPackage()), AssetVersion.MajorVersion, AssetVersion.MinorVersion}));
						}
					}
				}
			}
		}
		ParentEmitters.Sort();
		Attributes.Emplace(TEXT("TotalEmitters"), EmitterHandles.Num());
		Attributes.Emplace(TEXT("ActiveGpuEmitters"), GpuCount);
		Attributes.Emplace(TEXT("StatelessEmitters"), StatelessEmitters);
		Attributes.Emplace(TEXT("DisabledEmitters"), DisabledEmitters);
		Attributes.Emplace(TEXT("RendererCount"), RendererCount);
		Attributes.Emplace(TEXT("CustomRendererCount"), CustomRendererCount);
		Attributes.Emplace(TEXT("DisabledRendererCount"), DisabledRendererCount);
		Attributes.Emplace(TEXT("ScratchPadCount"), ScratchPadCount);
		Attributes.Emplace(TEXT("ParentEmitterCount"), ParentEmitterCount);
		Attributes.Emplace(TEXT("ParentEmitters"), ParentEmitters);
		Attributes.Emplace(TEXT("UsesEffectType"), GetEffectType() != nullptr);
		Attributes.Emplace(TEXT("OverrideScalabilitySettings"), GetOverrideScalabilitySettings());
		Attributes.Emplace(TEXT("UsesWarmup"), NeedsWarmup());

		// gather system script module data
		UNiagaraScriptSourceBase* SystemScriptSource = SystemSpawnScript->GetLatestSource();
		if (SystemSpawnScript && SystemScriptSource)
		{
			SystemScriptSource->ReportAnalyticsData(ScriptAnalytics);
		}
		TArray<FString> UsedModules = ScriptAnalytics.UsedNiagaraModules.Array();
		UsedModules.Sort();
		Attributes.Emplace(TEXT("Script.ActiveModules"), ScriptAnalytics.ActiveModules);
		Attributes.Emplace(TEXT("Script.DisabledModules"), ScriptAnalytics.DisabledModules);
		Attributes.Emplace(TEXT("Script.UsedNiagaraModules"), UsedModules);
		
		NiagaraAnalytics::RecordEvent(TEXT("System.Content"), Attributes);
	}
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

bool UNiagaraSystem::SupportsStatScopedPerformanceMode() const
{
	const UNiagaraSettings* Settings = GetDefault<UNiagaraSettings>();
	return bDisableExperimentalVM || !Settings || !Settings->bExperimentalVMEnabled;
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

void UNiagaraSystem::SetWarmupTime(float InWarmupTime)
{
	WarmupTime = InWarmupTime;
	ResolveWarmupTickCount();
}

void UNiagaraSystem::SetWarmupTickDelta(float InWarmupTickDelta)
{
	WarmupTickDelta = InWarmupTickDelta;
	ResolveWarmupTickCount();
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
		WarmupTickCount = FMath::FloorToInt(WarmupTime / WarmupTickDelta);
		WarmupTime = WarmupTickDelta * WarmupTickCount;
	}
}

#if WITH_EDITOR
void UNiagaraSystem::PostRename(UObject* OldOuter, const FName OldName)
{
	Super::PostRename(OldOuter, OldName);

#if STATS
	UpdateStatID();
	for (const FNiagaraEmitterHandle& Handle : EmitterHandles)
	{
		if (UNiagaraEmitter* Emitter = Handle.GetInstance().Emitter)
		{
			Emitter->UpdateStatID();
		}
	}

	// Recreate any scene proxies
	FNiagaraSystemUpdateContext UpdateCtx(this, true);
#endif
}

void UNiagaraSystem::PreEditChange(FProperty* PropertyThatWillChange)
{
	Super::PreEditChange(PropertyThatWillChange);

	if (PropertyThatWillChange && PropertyThatWillChange->GetFName() == GET_MEMBER_NAME_CHECKED(UNiagaraSystem, EffectType))
	{
		UpdateContext.SetDestroyOnAdd(true);
		UpdateContext.Add(this, false);
	}

	////-TODO:Stateless: Merge into emitter handle
	//if (PropertyThatWillChange && PropertyThatWillChange->GetFName() == GET_MEMBER_NAME_CHECKED(UNiagaraSystem, StatelessEmitters))
	//{
	//	UpdateContext.SetDestroyOnAdd(true);
	//	UpdateContext.Add(this, false);
	//}
	////-TODO:Stateless: Merge into emitter handle
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

#if WITH_EDITORONLY_DATA
	// Validate that all our sub-objects have been loaded before we start post loading
	// This is to help track down issues where subobjects have not be loaded and to warn that it's the case
	if (GNiagaraObjectNeedsLoadMode > 0)
	{
		TArray<UObject*> ObjectReferences;
		FReferenceFinder(ObjectReferences, this, false, true, true, true).FindReferences(this);

		for (UObject* Dependency : ObjectReferences)
		{
			if (Dependency->HasAnyFlags(RF_NeedLoad))
			{
				UE_LOG(LogNiagara, Log, TEXT("NiagaraSystem::PostLoad() - SubObject(%s) RF_NeedLoad"), *GetFullNameSafe(Dependency));
				if (GNiagaraObjectNeedsLoadMode == 2)
				{
					Dependency->GetLinker()->Preload(Dependency);
				}
			}
		}
	}
#endif

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

	ExposedParameters.PostLoad(this);
	ExposedParameters.SanityCheckData();

#if WITH_EDITORONLY_DATA
	EditorOnlyAddedParameters.PostLoad(this);
	EditorOnlyAddedParameters.SanityCheckData();
#endif

	SystemCompiledData.InstanceParamStore.PostLoad(this);

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
#if WITH_EDITORONLY_DATA
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
#endif

	if (NiagaraVer < FNiagaraCustomVersion::InitialOwnerVelocityFromActor)
	{
		bInitialOwnerVelocityFromActor = false;
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
	
	if(bExposeToLibrary_DEPRECATED)
	{
		LibraryVisibility = ENiagaraScriptLibraryVisibility::Unexposed;
	}

	if (MessageKeyToMessageMap_DEPRECATED.IsEmpty() == false)
	{
		MessageStore.SetMessages(MessageKeyToMessageMap_DEPRECATED);
		MessageKeyToMessageMap_DEPRECATED.Empty();
	}

#endif // WITH_EDITORONLY_DATA

	//Apply platform set redirectors
	auto ApplyPlatformSetRedirects = [](FNiagaraPlatformSet& PlatformSet)
	{
		PlatformSet.ApplyRedirects();
	};
	ForEachPlatformSet(ApplyPlatformSetRedirects);

#if !WITH_EDITOR
	// When running without the editor in a cooked build we run the update immediately in post load since
	// there will be no merging or compiling which makes it safe to do so.
	UpdateSystemAfterLoad();
#endif

#if WITH_EDITORONLY_DATA
	// see the equivalent in NiagaraEmitter for details
	ENiagaraScriptTemplateSpecification CurrentTemplateSpecification = TemplateSpecification_DEPRECATED;
	if(bIsTemplateAsset_DEPRECATED)
	{
		CurrentTemplateSpecification = ENiagaraScriptTemplateSpecification::Template;
	}

	if(NiagaraVer < FNiagaraCustomVersion::InheritanceUxRefactor)
	{
		if(CurrentTemplateSpecification == ENiagaraScriptTemplateSpecification::Template)
		{
			AssetTags.AddUnique(INiagaraModule::TemplateTagDefinition);
		}
		else if(CurrentTemplateSpecification == ENiagaraScriptTemplateSpecification::Behavior)
		{
			AssetTags.AddUnique(INiagaraModule::LearningContentTagDefinition);
		}
	}
#endif // WITH_EDITORONLY_DATA

	PrecachePSOs();
}

void UNiagaraSystem::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);
#if WITH_EDITOR
	// The assumption here is that whatever is being duplicated was already postloaded - but duplication clears out the non-uproperty fields.
	// Also, this only needs to be done in editor, otherwise postload does everything in one go.
	EnsureFullyLoaded();
#endif
}

void UNiagaraSystem::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);

	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(SystemScalabilityOverrides.Overrides.GetAllocatedSize());
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(EmitterHandles.GetAllocatedSize());
	if (const UScriptStruct* Struct = FVersionedNiagaraEmitterData::StaticStruct())
	{
		CumulativeResourceSize.AddDedicatedSystemMemoryBytes(Struct->GetStructureSize() * EmitterHandles.Num());
	}
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(EmitterCompiledData.GetAllocatedSize());
	if (const UScriptStruct* Struct = FNiagaraEmitterCompiledData::StaticStruct())
	{
		CumulativeResourceSize.AddDedicatedSystemMemoryBytes(Struct->GetStructureSize() * EmitterCompiledData.Num());
	}
	if (const UScriptStruct* Struct = FNiagaraSystemCompiledData::StaticStruct())
	{
		CumulativeResourceSize.AddDedicatedSystemMemoryBytes(Struct->GetStructureSize());
	}
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(EmitterExecutionOrder.GetAllocatedSize());
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(RendererPostTickOrder.GetAllocatedSize());
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(RendererCompletionOrder.GetAllocatedSize());
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(RendererDrawOrder.GetAllocatedSize());
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(EmitterExecutionStateAccessors.GetAllocatedSize());
	if (StaticBuffers != nullptr)
	{
		CumulativeResourceSize.AddDedicatedSystemMemoryBytes(StaticBuffers->GetCpuFloatBuffer().Num() * sizeof(float));
	}
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

	// Don't have an instance yet to retrieve the possible material override data from
	FNiagaraEmitterInstance* EmitterInstance = nullptr;

	FMaterialInterfacePSOPrecacheParamsList MaterialInterfacePSOPrecacheParamsList;
	for (const FNiagaraEmitterHandle& EmitterHandle : GetEmitterHandles())
	{
		FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData();
		if (EmitterData && EmitterHandle.GetIsEnabled())
		{
			EmitterData->ForEachEnabledRenderer(
				[&](UNiagaraRendererProperties* Properties)
				{
					Properties->CollectPSOPrecacheData(EmitterInstance, MaterialInterfacePSOPrecacheParamsList);
				}
			);
		}
	}

	LaunchPSOPrecaching(MaterialInterfacePSOPrecacheParamsList);
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

bool UNiagaraSystem::IsReadyToRun() const
{
	if (PSOPrecacheCompletionEvent.IsValid() && !PSOPrecacheCompletionEvent->IsComplete())
	{
		return false;
	}
	return FPlatformProperties::RequiresCookedData() ? bIsReadyToRunCached : IsReadyToRunInternal();
}

void UNiagaraSystem::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS;
	Super::GetAssetRegistryTags(OutTags);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS;
}

void UNiagaraSystem::GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
	if (HasAnyFlags(RF_ClassDefaultObject) == false)
	{
		EnsureFullyLoaded();
	}

#if WITH_EDITOR
	Context.AddTag(FAssetRegistryTag(NiagaraSystemPrivate::NAME_HasGPUEmitter, HasAnyGPUEmitters() ? TEXT("True") : TEXT("False"), FAssetRegistryTag::TT_Alphabetical));

	const float BoundsSize = float(FixedBounds.GetSize().GetMax());
	Context.AddTag(FAssetRegistryTag(NiagaraSystemPrivate::NAME_FixedBoundsSize, bFixedBounds ? FString::Printf(TEXT("%.2f"), BoundsSize) : FString(TEXT("None")), FAssetRegistryTag::TT_Numerical));

	Context.AddTag(FAssetRegistryTag(NiagaraSystemPrivate::NAME_NumEmitters, LexToString(EmitterHandles.Num()), FAssetRegistryTag::TT_Numerical));

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

	Context.AddTag(FAssetRegistryTag(NiagaraSystemPrivate::NAME_ActiveEmitters, LexToString(NumActiveEmitters), FAssetRegistryTag::TT_Numerical));
	Context.AddTag(FAssetRegistryTag(NiagaraSystemPrivate::NAME_ActiveRenderers, LexToString(NumActiveRenderers), FAssetRegistryTag::TT_Numerical));
	Context.AddTag(FAssetRegistryTag(NiagaraSystemPrivate::NAME_GPUSimsMissingFixedBounds, LexToString(GPUSimsMissingFixedBounds), FAssetRegistryTag::TT_Numerical));
	Context.AddTag(FAssetRegistryTag(NiagaraSystemPrivate::NAME_EffectType, EffectType != nullptr ? EffectType->GetName() : FString(TEXT("None")), FAssetRegistryTag::TT_Alphabetical));
	Context.AddTag(FAssetRegistryTag(NiagaraSystemPrivate::NAME_WarmupTime, LexToString(WarmupTime), FAssetRegistryTag::TT_Numerical));
	Context.AddTag(FAssetRegistryTag(NiagaraSystemPrivate::NAME_HasOverrideScalabilityForSystem, bOverrideScalabilitySettings ? TEXT("True") : TEXT("False"), FAssetRegistryTag::TT_Alphabetical));
	Context.AddTag(FAssetRegistryTag(NiagaraSystemPrivate::NAME_HasDIsWithPostSimulateTick, bHasDIsWithPostSimulateTick ? TEXT("True") : TEXT("False"), FAssetRegistryTag::TT_Alphabetical));
	Context.AddTag(FAssetRegistryTag(NiagaraSystemPrivate::NAME_NeedsSortedSignificanceCull, bNeedsSortedSignificanceCull ? TEXT("True") : TEXT("False"), FAssetRegistryTag::TT_Alphabetical));

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
			Context.AddTag(FAssetRegistryTag(*QualityLevelKey, LexToString(QualityLevelsNumActive[i]), FAssetRegistryTag::TT_Numerical));
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
		Context.AddTag(FAssetRegistryTag(NiagaraSystemPrivate::NAME_ActiveDIs, LexToString(DataInterfaces.Num()), FAssetRegistryTag::TT_Numerical));
	}


	// Now propagate the custom numeric and string tags from the DataInterfaces and RendererProperties above
	auto NumericIter = NumericKeys.CreateConstIterator();
	while (NumericIter)
	{

		Context.AddTag(FAssetRegistryTag(NumericIter.Key(), LexToString(NumericIter.Value()), FAssetRegistryTag::TT_Numerical));
		++NumericIter;
	}

	auto StringIter = StringKeys.CreateConstIterator();
	while (StringIter)
	{

		Context.AddTag(FAssetRegistryTag(StringIter.Key(), LexToString(StringIter.Value()), FAssetRegistryTag::TT_Alphabetical));
		++StringIter;
	}

	/*for (const UNiagaraDataInterface* DI : DataInterfaces)
	{
		FString ClassName;
		DI->GetClass()->GetName(ClassName);
		Context.AddTag(FAssetRegistryTag(*(TEXT("bHas")+ClassName), TEXT("True"), FAssetRegistryTag::TT_Alphabetical));
	}*/

	// Asset Library Tags
	for(const FNiagaraAssetTagDefinitionReference& Tag : AssetTags)
	{
		Tag.AddTagToAssetRegistryTags(Context);
	}
	
	//Context.AddTag(FAssetRegistryTag("CPUCollision", UsesCPUCollision() ? TEXT("True") : TEXT("False"), FAssetRegistryTag::TT_Alphabetical));
	//Context.AddTag(FAssetRegistryTag("Looping", bAnyEmitterLoopsForever ? TEXT("True") : TEXT("False"), FAssetRegistryTag::TT_Alphabetical));
	//Context.AddTag(FAssetRegistryTag("Immortal", IsImmortal() ? TEXT("True") : TEXT("False"), FAssetRegistryTag::TT_Alphabetical));
	//Context.AddTag(FAssetRegistryTag("Becomes Zombie", WillBecomeZombie() ? TEXT("True") : TEXT("False"), FAssetRegistryTag::TT_Alphabetical));
	//Context.AddTag(FAssetRegistryTag("CanBeOccluded", OcclusionBoundsMethod == EParticleSystemOcclusionBoundsMethod::EPSOBM_None ? TEXT("False") : TEXT("True"), FAssetRegistryTag::TT_Alphabetical));

	INiagaraModule& NiagaraModule = FModuleManager::GetModuleChecked<INiagaraModule>("Niagara");
	Context.AddTag(NiagaraModule.GetEditorOnlyDataUtilities().CreateClassUsageAssetRegistryTag(this));
#endif
	Super::GetAssetRegistryTags(Context);
}

#if WITH_EDITORONLY_DATA
void UNiagaraSystem::GetAssetRegistryTagMetadata(TMap<FName, FAssetRegistryTagMetadata>& OutMetadata) const
{
	Super::GetAssetRegistryTagMetadata(OutMetadata);

	OutMetadata.Add(
		NiagaraSystemPrivate::NAME_ActiveEmitters,
		FAssetRegistryTagMetadata()
		.SetDisplayName(LOCTEXT("ActiveEmitters", "Active Emitters"))
		.SetTooltip(LOCTEXT("ActiveEmittersTooltip", "The nunmber of active emitters in the system"))
	);
	OutMetadata.Add(
		NiagaraSystemPrivate::NAME_ActiveRenderers,
		FAssetRegistryTagMetadata()
		.SetDisplayName(LOCTEXT("ActiveRenderers", "Active Renderers"))
	);
	OutMetadata.Add(
		NiagaraSystemPrivate::NAME_GPUSimsMissingFixedBounds,
		FAssetRegistryTagMetadata()
		.SetDisplayName(LOCTEXT("GPUSimsMissingFixedBounds", "GPU Sims Missing Fixed Bounds"))
	);
	OutMetadata.Add(
		NiagaraSystemPrivate::NAME_EffectType,
		FAssetRegistryTagMetadata()
		.SetDisplayName(LOCTEXT("EffectType", "Effect Type"))
	);
	OutMetadata.Add(
		NiagaraSystemPrivate::NAME_WarmupTime,
		FAssetRegistryTagMetadata()
		.SetDisplayName(LOCTEXT("WarmupTime", "Warmup Time"))
	);
	OutMetadata.Add(
		NiagaraSystemPrivate::NAME_HasOverrideScalabilityForSystem,
		FAssetRegistryTagMetadata()
		.SetDisplayName(LOCTEXT("HasOverrideScalabilityForSystem", "Has Override Scalability For System"))
	);
	OutMetadata.Add(
		NiagaraSystemPrivate::NAME_HasDIsWithPostSimulateTick,
		FAssetRegistryTagMetadata()
		.SetDisplayName(LOCTEXT("HasDIsWithPostSimulateTick", "Has DIs With Post Simulate Tick"))
	);
	OutMetadata.Add(
		NiagaraSystemPrivate::NAME_NeedsSortedSignificanceCull,
		FAssetRegistryTagMetadata()
		.SetDisplayName(LOCTEXT("NeedsSortedSignificanceCull", "Needs Sorted Significance Cull"))
	);
	OutMetadata.Add(
		NiagaraSystemPrivate::NAME_ActiveDIs,
		FAssetRegistryTagMetadata()
		.SetDisplayName(LOCTEXT("ActiveDIs", "Active DIs"))
	);
	OutMetadata.Add(
		NiagaraSystemPrivate::NAME_HasGPUEmitter,
		FAssetRegistryTagMetadata()
		.SetDisplayName(LOCTEXT("HasGPUEmitter", "Has GPU Emitter"))
	);
	OutMetadata.Add(
		NiagaraSystemPrivate::NAME_FixedBoundsSize,
		FAssetRegistryTagMetadata()
		.SetDisplayName(LOCTEXT("FixedBoundsSize", "Fixed Bounds Size"))
	);
	OutMetadata.Add(
		NiagaraSystemPrivate::NAME_NumEmitters,
		FAssetRegistryTagMetadata()
		.SetDisplayName(LOCTEXT("NumEmitters", "Num Emitters"))
	);
}

bool UNiagaraSystem::HasOutstandingCompilationRequests(bool bIncludingGPUShaders) const
{
	if (bNeedsRequestCompile)
	{
		return true;
	}

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

bool UNiagaraSystem::CompileRequestsShouldBlockGC() const
{
	for (const TUniquePtr<FNiagaraActiveCompilation>& ActiveCompilation : ActiveCompilations)
	{
		if (ActiveCompilation && ActiveCompilation->BlocksGarbageCollection())
		{
			return true;
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
	const FNiagaraScriptExecutionParameterStore* ParameterStore = Script->GetExecutionReadyParameterStore(EmitterData->SimTarget);
	if (ParameterStore == nullptr)
	{
		return;
	}

	// Note we need to null check here as merging can leave parameter stores with nullptrs to user data interfaces, once the compilation is complete they will be valid.
	if (EmitterData->SimTarget == ENiagaraSimTarget::CPUSim)
	{
		for (UNiagaraDataInterface* DataInterface : ParameterStore->GetDataInterfaces())
		{
			if (DataInterface != nullptr)
			{
				DataInterface->GetEmitterDependencies(this, Dependencies);
			}
		}
	}
	else
	{
		const TArray<UNiagaraDataInterface*>& StoreDataInterfaces = ParameterStore->GetDataInterfaces();
		if (StoreDataInterfaces.Num() > 0)
		{
			auto FindCachedDefaultDI =
				[](UNiagaraScript* Script, const FNiagaraVariableBase& Variable) -> UNiagaraDataInterface*
			{
				if (Script)
				{
					for (const FNiagaraScriptResolvedDataInterfaceInfo& DataInterfaceInfo : Script->GetResolvedDataInterfaces())
					{
						if (DataInterfaceInfo.ParameterStoreVariable == Variable)
						{
							return DataInterfaceInfo.ResolvedDataInterface;
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

				if (StoreDataInterfaces[Variable.Offset] != nullptr)
				{
					StoreDataInterfaces[Variable.Offset]->GetEmitterDependencies(this, Dependencies);
				}
			}
		}
	}
}

#if WITH_EDITORONLY_DATA
const FGuid UNiagaraSystem::ComputeEmitterExecutionOrderMessageId(0x736C2644, 0x3B4F4D0E, 0xADEDE523, 0x2DACC2CF);
#endif

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
			continue;
		}

		EmitterDependencies.SetNum(0, EAllowShrinking::No);

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
				#if WITH_EDITORONLY_DATA
					FName EmitterName = EmitterHandles[EmitterIdx].GetName();
					FText ErrorMessage = FText::Format(LOCTEXT("CircularDependencyError", "Found circular dependency involving emitter '{0}' in system '{1}'. The execution order will be undefined."), FText::FromName(EmitterName), FText::FromString(GetName()));
					INiagaraModule& NiagaraModule = FModuleManager::GetModuleChecked<INiagaraModule>("Niagara");
					bool bAllowDismissal = true;
					UNiagaraMessageDataBase* ComputeEmitterExecutionOrderMessage = NiagaraModule.GetEditorOnlyDataUtilities().CreateWarningMessage(
						this,
						LOCTEXT("ComputeEmitterOrderError", "Error computing emitter execution order"),
						ErrorMessage,
						"Resolve Data Interfaces",
						bAllowDismissal);
					MessageStore.AddMessage(ComputeEmitterExecutionOrderMessageId, ComputeEmitterExecutionOrderMessage);
					if (MessageStore.IsMessageDismissed(ComputeEmitterExecutionOrderMessageId) == false)
					{
						UE_LOG(LogNiagara, Log, TEXT("%s"), *ErrorMessage.ToString());
					}
				#endif
					break;
				}
			}
		}

		// Sort the emitter indices in the execution order array so that dependencies are satisfied.
		Algo::StableSort(EmitterExecutionOrder, [&EmitterPriorities](FNiagaraEmitterExecutionIndex IdxA, FNiagaraEmitterExecutionIndex IdxB) { return EmitterPriorities[IdxA.EmitterIndex] < EmitterPriorities[IdxB.EmitterIndex]; });

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
		if (!EmitterHandle.GetIsEnabled())
		{
			continue;
		}

		if (EmitterHandle.GetEmitterMode() == ENiagaraEmitterMode::Standard)
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
		//-TODO:Stateless:Abstract this away
		else
		{
			ensure(EmitterHandle.GetEmitterMode() == ENiagaraEmitterMode::Stateless);
			const UNiagaraStatelessEmitter* StatelessEmitter = EmitterHandle.GetStatelessEmitter();
			if (StatelessEmitter)
			{
				StatelessEmitter->ForEachEnabledRenderer(
					[&](UNiagaraRendererProperties* Properties)
					{
						RendererSortInfo.Emplace(Properties->SortOrderHint, RendererSortInfo.Num());
					}
				);
			}
		}
		//-TODO:Stateless:Abstract this away
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
	LLM_SCOPE(ELLMTag::Niagara);
	const FNiagaraDataSetCompiledData& SystemDataSet = SystemCompiledData.DataSetCompiledData;
	const UNiagaraSettings* NiagaraSettings = GetDefault<UNiagaraSettings>();

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

	FGraphEventArray PSOPrecacheEvents;

	TSet<FName> DataInterfaceGpuUsage;
	FNameBuilder ExecutionStateNameBuilder;
	for (int32 i=0; i < EmitterHandles.Num(); ++i)
	{
		FNiagaraEmitterHandle& EmitterHandle = EmitterHandles[i];
		FNiagaraDataSetAccessor<ENiagaraExecutionState>& EmitterExecutionState = EmitterExecutionStateAccessors.AddDefaulted_GetRef();
		if (!EmitterHandle.GetIsEnabled())
		{
			continue;
		}

		if (EmitterHandle.GetEmitterMode() == ENiagaraEmitterMode::Standard)
		{
			if (FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData())
			{
				// Cache system instance accessors
				ExecutionStateNameBuilder.Reset();
				ExecutionStateNameBuilder << EmitterHandle.GetInstance().Emitter->GetUniqueEmitterName();
				ExecutionStateNameBuilder << TEXT(".ExecutionState");
				const FName ExecutionStateName(ExecutionStateNameBuilder);

				EmitterExecutionState.Init(SystemDataSet, ExecutionStateName);

				// Cache emitter data set accessors, for things like bounds, etc
				const FNiagaraDataSetCompiledData* DataSetCompiledData = nullptr;
				if (EmitterCompiledData.IsValidIndex(i))
				{
					for (const FName& SpawnName : EmitterCompiledData[i]->SpawnAttributes)
					{
						EmitterSpawnInfoAccessors[i].Emplace(SystemDataSet, SpawnName);
					}

					DataSetCompiledData = &EmitterCompiledData[i]->DataSetCompiledData;

					if (NiagaraSettings->bLimitDeltaTime)
					{
						MaxDeltaTime = MaxDeltaTime.IsSet() ? FMath::Min(MaxDeltaTime.GetValue(), NiagaraSettings->MaxDeltaTimePerTick) : NiagaraSettings->MaxDeltaTimePerTick;
					}
				}
				EmitterHandle.GetInstance().Emitter->ConditionalPostLoad();
				EmitterData->CacheFromCompiledData(DataSetCompiledData, *EmitterHandle.GetInstance().Emitter);

				PSOPrecacheEvents.Append(EmitterData->PrecacheComputePSOs(*EmitterHandle.GetInstance().Emitter));

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
					for (const FNiagaraScriptResolvedDataInterfaceInfo& DataInterfaceInfo : NiagaraScript->GetResolvedDataInterfaces())
					{
						if (UNiagaraDataInterface* DataInterface = DataInterfaceInfo.ResolvedDataInterface)
						{
							DataInterface->CacheStaticBuffers(*StaticBuffers.Get(), DataInterfaceInfo.ResolvedVariable, bUsedByCPU, bUsedByGPU);

							if ( bUsedByGPU )
							{
								if (DataInterfaceInfo.bIsInternal == false)
								{
									DataInterfaceGpuUsage.Add(DataInterfaceInfo.ParameterStoreVariable.GetName()); 
								}
							}
						}
					}
				}
			}
		}
		//-TODO:Stateless:Abstract this away
		else
		{
			ensure(EmitterHandle.GetEmitterMode() == ENiagaraEmitterMode::Stateless);
			if (UNiagaraStatelessEmitter* StatelessEmitter = EmitterHandle.GetStatelessEmitter())
			{
				StatelessEmitter->CacheFromCompiledData();
			}
		}
		//-TODO:Stateless:Abstract this away
	}

	// If we had any precache events to wait on add a task to ensure the system does not start until they are ready
	if (PSOPrecacheEvents.Num() > 0)
	{
		DECLARE_CYCLE_STAT(TEXT("FNullGraphTask.NiagaraPSOPrecacheCompletion"), STAT_FNullGraphTask_NiagaraPSOPrecacheCompletion, STATGROUP_TaskGraphTasks);
		PSOPrecacheCompletionEvent = TGraphTask<FNullGraphTask>::CreateTask(&PSOPrecacheEvents, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(GET_STATID(STAT_FNullGraphTask_NiagaraPSOPrecacheCompletion), ENamedThreads::AnyThread);

		// Inject task to clear out the reference to the graph task as soon as possible
		FGraphEventArray CompleteTaskPrereqs({ PSOPrecacheCompletionEvent });
		TGraphTask<FNiagaraSystemNullCompletionTask>::CreateTask(&CompleteTaskPrereqs, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(this, &PSOPrecacheCompletionEvent);
	}

	// Loop over system scripts these are more awkward because we need to determine the usage
	for (UNiagaraScript* NiagaraScript : { SystemSpawnScript, SystemUpdateScript })
	{
		if (NiagaraScript == nullptr)
		{
			continue;
		}

 		for (const FNiagaraScriptResolvedDataInterfaceInfo& DataInterfaceInfo : NiagaraScript->GetResolvedDataInterfaces())
		{
			if (UNiagaraDataInterface* DataInterface = DataInterfaceInfo.ResolvedDataInterface)
			{
				const bool bUsedByGPU = DataInterfaceGpuUsage.Contains(DataInterfaceInfo.ParameterStoreVariable.GetName());
				DataInterface->CacheStaticBuffers(*StaticBuffers.Get(), DataInterfaceInfo.ResolvedVariable, true, bUsedByGPU);
			}
		}
	}

	// Finalize static buffers
	StaticBuffers->Finalize();
}

FBox UNiagaraSystem::GetFixedBounds() const
{
	return FixedBounds;
}

#if WITH_EDITORONLY_DATA

const FGuid UNiagaraSystem::ResolveDIsMessageId(0xB6ACDD97, 0xA2C04B02, 0xAB9FA4E4, 0xDBC73E51);

void UNiagaraSystem::ResolveParameterStoreBindings()
{
	TArray<FText> ErrorMessages;
	FNiagaraResolveDIHelpers::ResolveDIs(this, ErrorMessages);
	if (ErrorMessages.Num() > 0)
	{
		INiagaraModule& NiagaraModule = FModuleManager::GetModuleChecked<INiagaraModule>("Niagara");
		bool bAllowDismissal = true;
		UNiagaraMessageDataBase* ResolveDIsMessage = NiagaraModule.GetEditorOnlyDataUtilities().CreateWarningMessage(
			this,
			LOCTEXT("ResolveDIsError", "Resolving data interfaces generated errors"),
			FText::Join(FText::FromString("\n"), ErrorMessages),
			"Resolve Data Interfaces",
			bAllowDismissal);
		MessageStore.AddMessage(ResolveDIsMessageId, ResolveDIsMessage);
		if (MessageStore.IsMessageDismissed(ResolveDIsMessageId) == false)
		{
			UE_LOG(LogNiagara, Warning, TEXT("Issues reported while resolving data interfaces for system %s"), *this->GetPathName());
			for (const FText& ErrorMessage : ErrorMessages)
			{
				UE_LOG(LogNiagara, Warning, TEXT("%s"), *ErrorMessage.ToString());
			}
		}
	}
	else
	{
		MessageStore.RemoveMessage(ResolveDIsMessageId);
	}


	// Resolve Object bindings
	{
		ForEachScript(
			[&](UNiagaraScript* Script)
			{
				if (Script->GetCachedDefaultUObjects().Num() == 0)
				{
					return;
				}

				const FNiagaraParameterStore& InstanceParamStore = GetSystemCompiledData().InstanceParamStore;

				TArray<FNiagaraResolvedUObjectInfo> ResolvedUObjects;
				ResolvedUObjects.Reserve(Script->GetCachedDefaultUObjects().Num());

				for (const FNiagaraScriptUObjectCompileInfo& DefaultInfo : Script->GetCachedDefaultUObjects())
				{
					const FNiagaraVariableBase ReadVariable(DefaultInfo.Variable.GetType(), DefaultInfo.RegisteredParameterMapRead);
					const bool bIsUserVariable = ReadVariable.IsInNameSpace(FNiagaraConstants::UserNamespaceString);

					for (const FName WriteName : DefaultInfo.RegisteredParameterMapWrites)
					{
						FNiagaraResolvedUObjectInfo& ResolvedInfo = ResolvedUObjects.AddDefaulted_GetRef();
						const FNiagaraVariableBase WriteVariable(DefaultInfo.Variable.GetType(), WriteName);
						ResolvedInfo.ReadVariableName = ReadVariable.GetName();
						ResolvedInfo.ResolvedVariable = WriteVariable;
						if (bIsUserVariable)
						{
							ResolvedInfo.Object = InstanceParamStore.GetUObject(ReadVariable);
						}
						else
						{
							ResolvedInfo.Object = DefaultInfo.Object;
						}
					}
				}

				ResolvedUObjects.Shrink();
				Script->SetResolvedUObjects(ResolvedUObjects);
			}
		);
	}
}

#endif // WITH_EDITORONLY_DATA

void UNiagaraSystem::UpdatePostCompileDIInfo()
{
	bNeedsGPUContextInitForDataInterfaces = false;

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

	ForEachScript([&](UNiagaraScript* Script) 
	{
		for(const FNiagaraScriptResolvedDataInterfaceInfo& ResolvedDIInfo : Script->GetResolvedDataInterfaces())
		{
			if(ResolvedDIInfo.ResolvedDataInterface)
			{
				ResolvedDIInfo.ResolvedDataInterface->PostCompile();
			}
		}		
	});
}

void UNiagaraSystem::UpdateDITickFlags()
{
	bHasDIsWithPostSimulateTick = false;
	bAllDIsPostSimulateCanOverlapFrames = true;
	bAllDIsPostStageCanOverlapTickGroups = true;
	auto CheckPostSimTick = [&](UNiagaraScript* Script)
	{
		if (Script)
		{
			for (FNiagaraScriptDataInterfaceCompileInfo& Info : Script->GetVMExecutableData().DataInterfaceInfo)
			{
				if(UNiagaraDataInterface* DefaultDataInterface = Info.GetDefaultDataInterface())
				{
					if (DefaultDataInterface->HasPostSimulateTick())
					{
						bHasDIsWithPostSimulateTick |= true;
						bAllDIsPostSimulateCanOverlapFrames &= DefaultDataInterface->PostSimulateCanOverlapFrames();
					}
					bAllDIsPostStageCanOverlapTickGroups &= DefaultDataInterface->PostStageCanOverlapTickGroups();
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

#if WITH_EDITORONLY_DATA

void UNiagaraSystem::OnCompiledDataInterfaceChanged()
{
	// We need to make sure our execution ready parameter stores are invalidated so that the DI's will rebind functions
	ForEachScript([&](UNiagaraScript* Script) { Script->InvalidateExecutionReadyParameterStores(); });
	UpdatePostCompileDIInfo();
	UpdateDITickFlags();
}

void UNiagaraSystem::OnCompiledUObjectChanged()
{
	ForEachScript([&](UNiagaraScript* Script) { Script->InvalidateExecutionReadyParameterStores(); });
}

#endif

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
	if (InEmitter.bIsInheritable == false)
	{
		NewEmitter->DisableVersioning(EmitterVersion);
		NewEmitter->TemplateAssetDescription = FText();
		NewEmitter->GetLatestEmitterData()->RemoveParent();
	}
	EmitterHandles.Add(EmitterHandle);
	RefreshSystemParametersFromEmitter(EmitterHandle);
	InvalidateCachedData();
	return EmitterHandle;
}

void UNiagaraSystem::AddEmitterHandleDirect(FNiagaraEmitterHandle& EmitterHandle)
{
	EmitterHandles.Add(EmitterHandle);
	RefreshSystemParametersFromEmitter(EmitterHandle);
	InvalidateCachedData();
}

FNiagaraEmitterHandle UNiagaraSystem::DuplicateEmitterHandle(const FNiagaraEmitterHandle& EmitterHandleToDuplicate, FName EmitterName)
{
	UNiagaraEmitter* DuplicateEmitter = EmitterHandleToDuplicate.GetInstance().Emitter ? UNiagaraEmitter::CreateAsDuplicate(*EmitterHandleToDuplicate.GetInstance().Emitter, EmitterName, *this) : nullptr;
	UNiagaraStatelessEmitter* DuplicateStatelessEmitter = EmitterHandleToDuplicate.GetStatelessEmitter() ? EmitterHandleToDuplicate.GetStatelessEmitter()->CreateAsDuplicate(EmitterName, *this) : nullptr;
	if (DuplicateEmitter)
	{
		EmitterHandles.Emplace_GetRef(*DuplicateEmitter, EmitterHandleToDuplicate.GetInstance().Version);
	}
	else
	{
		check(DuplicateStatelessEmitter);
		EmitterHandles.Emplace(*DuplicateStatelessEmitter);
	}

	FNiagaraEmitterHandle& NewHandle = EmitterHandles.Last();
	NewHandle.SetIsEnabled(EmitterHandleToDuplicate.GetIsEnabled(), *this, false);
	NewHandle.SetStatelessEmitter(DuplicateStatelessEmitter);
	NewHandle.SetEmitterMode(*this, EmitterHandleToDuplicate.GetEmitterMode());
	RefreshSystemParametersFromEmitter(NewHandle);
	InvalidateCachedData();
	return NewHandle;
}

void UNiagaraSystem::RemoveEmitterHandle(const FNiagaraEmitterHandle& EmitterHandleToDelete)
{
	RemoveSystemParametersForEmitter(EmitterHandleToDelete);
	auto RemovePredicate = [&](const FNiagaraEmitterHandle& EmitterHandle) { return EmitterHandle.GetId() == EmitterHandleToDelete.GetId(); };
	EmitterHandles.RemoveAll(RemovePredicate);
	InvalidateCachedData();
}

void UNiagaraSystem::RemoveEmitterHandlesById(const TSet<FGuid>& HandlesToRemove)
{
	auto RemovePredicate = [&](const FNiagaraEmitterHandle& EmitterHandle)
	{
		return HandlesToRemove.Contains(EmitterHandle.GetId());
	};
	EmitterHandles.RemoveAll(RemovePredicate);

	InitEmitterCompiledData();
	InvalidateCachedData();
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
	for (TUniquePtr<FNiagaraActiveCompilation>& ActiveCompilation : ActiveCompilations)
	{
		ActiveCompilation->Abort();
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

void UNiagaraSystem::SetCompileForEdit(bool bNewCompileForEdit)
{
	if (bNewCompileForEdit != bCompileForEdit)
	{
		bCompileForEdit = bNewCompileForEdit;
		bNeedsRequestCompile = true;
	}
}

void UNiagaraSystem::WaitForCompilationComplete(bool bIncludingGPUShaders, bool bShowProgress)
{
	LLM_SCOPE(ELLMTag::Niagara);
	TRACE_CPUPROFILER_EVENT_SCOPE(WaitForNiagaraCompilation);
	TRACE_CPUPROFILER_EVENT_SCOPE_TEXT_ON_CHANNEL(*GetPathName(), NiagaraChannel);
	COOK_STAT(FScopedDurationTimer DurationTimer(NiagaraScriptCookStats::NiagaraSystemWaitForCompilationCompleteTime));

	if (bNeedsRequestCompile)
	{
		RequestCompile(false);
	}

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
	FScopedSlowTask Progress(float(TotalCompiles), LOCTEXT("WaitingForCompile", "Waiting for compilation to complete"));
	if (bShowProgress && TotalCompiles > 0)
	{
		Progress.MakeDialog();
	}

	double StartTime = FPlatformTime::Seconds();
	double TimeLogThreshold = GNiagaraCompileWaitLoggingThreshold;
	uint32 NumLogIterations = GNiagaraCompileWaitLoggingTerminationCap;

	while (!ActiveCompilations.IsEmpty())
	{
		if (QueryCompileComplete(true))
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
	for (TUniquePtr<FNiagaraActiveCompilation>& ActiveCompilation : ActiveCompilations)
	{
		ActiveCompilation->Invalidate();
	}
}

bool UNiagaraSystem::PollForCompilationComplete(bool bFlushRequestCompile)
{
	if (bNeedsRequestCompile && bFlushRequestCompile)
	{
		RequestCompile(false);
	}

	return QueryCompileComplete(false);
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
					FNiagaraCVarUtilities::GetCompileEventSeverityForFailIfNotSet(), Dependency.LinkerErrorMessage, FString(), Dependency.NodeGuid, Dependency.PinGuid, Dependency.StackGuids, FNiagaraCompileEventSource::ScriptDependency);
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

bool UNiagaraSystem::QueryCompileComplete(bool bWait)
{
	check(IsInGameThread());

	if (ActiveCompilations.IsEmpty() || bCompilationReentrantGuard)
	{
		return false;
	}

	TGuardValue<bool> ReentrantGuard(bCompilationReentrantGuard, true);

	FNiagaraQueryCompilationOptions Options;
	Options.System = this;
	Options.bWait = bWait;
	Options.MaxWaitDuration = 0.125;

	const bool bCompileComplete = ActiveCompilations[0]->QueryCompileComplete(Options);

	// Make sure that we aren't waiting for any results to come back.
	if (!bCompileComplete)
	{
		return false;
	}

	// now that the request is complete we can pull it from the array of ActiveCompilations and
	// finish processing it
	TUniquePtr<FNiagaraActiveCompilation> CurrentCompilation = MoveTemp(ActiveCompilations[0]);
	ActiveCompilations.RemoveAt(0);

	// In the world of do not apply, we're exiting the system completely so let's just kill any active compilations altogether.
	if (!CurrentCompilation->ShouldApply())
	{
		return true;
	}

	// if we've gotten all the results, run a quick check to see if the data is valid, if it's not then that indicates that
	// we've run into a compatibility issue and so we should see if we should issue a full rebuild
	const bool ResultsConsistent = CurrentCompilation->ValidateConsistentResults(Options);
	if (!ResultsConsistent && !CurrentCompilation->WasForced())
	{
		RequestCompile(true, nullptr);
		return false;
	}

	CurrentCompilation->Apply(Options);

	// Once compile results have been set, check dependencies found during compile and evaluate whether dependency compile events should be added.
	if (FNiagaraCVarUtilities::GetShouldEmitMessagesForFailIfNotSet())
	{
		EvaluateCompileResultDependencies();
	}

	// we want to run the post compile steps (specifically broadcasting compilations completing) unless
	// we're in the middle of waiting and we still have more compilations to get through
	const bool bDoPostCompile = !bWait || ActiveCompilations.IsEmpty();

	if (bDoPostCompile)
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

	ResolveParameterStoreBindings();
	UpdatePostCompileDIInfo();
	ComputeEmittersExecutionOrder();
	ComputeRenderersDrawOrder();
	CacheFromCompiledData();
	UpdateHasGPUEmitters();
	UpdateDITickFlags();
	ResolveScalabilitySettings();
	ResolveRequiresScripts();

	CurrentCompilation->ReportResults(Options);

	if (bDoPostCompile)
	{
		BroadcastOnSystemCompiled();
	}

	return true;
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

const FName UNiagaraSystem::GetEmitterVariableAliasName(const FNiagaraVariableBase& InEmitterVar, const UNiagaraEmitter* InEmitter) const
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
		FVersionedNiagaraEmitterData* EmitterData = Handle.GetEmitterData();
		if (Handle.GetIsEnabled() && EmitterData) // Don't pull in the emitter if it isn't going to be used.
		{
			TArray<UNiagaraScript*> EmitterScripts;
			EmitterData->GetScripts(EmitterScripts, false, true);

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
	if (CachedTraversalData.IsValid() && CachedTraversalData->IsValidForSystem(this))
		return CachedTraversalData;

	INiagaraModule& NiagaraModule = FModuleManager::Get().LoadModuleChecked<INiagaraModule>(TEXT("Niagara"));
	CachedTraversalData = NiagaraModule.CacheGraphTraversal(this, FGuid());
	return CachedTraversalData;
}

void UNiagaraSystem::InvalidateCachedData()
{
	CachedTraversalData.Reset();
}

bool UNiagaraSystem::RequestCompile(bool bForce, FNiagaraSystemUpdateContext* OptionalUpdateContext, const ITargetPlatform* TargetPlatform)
{
	check(IsInGameThread());
	TRACE_CPUPROFILER_EVENT_SCOPE(UNiagaraSystem::RequestCompile)

	bNeedsRequestCompile = false;

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
		ActiveCompilations[i]->Abort();
		ActiveCompilations.RemoveAt(i);
	}

	SCOPE_CYCLE_COUNTER(STAT_Niagara_System_CompileScript);

	bool bLaunchedCompilations = false;

	// Record that we entered this function already.
	{
		TGuardValue<bool> ReentrantGuard(bCompilationReentrantGuard, true);

		COOK_STAT(auto Timer = NiagaraScriptCookStats::UsageStats.TimeSyncWork());
		COOK_STAT(Timer.TrackCyclesOnly());

		TUniquePtr<FNiagaraActiveCompilation>& CurrentCompilation = ActiveCompilations.Emplace_GetRef(
			FNiagaraActiveCompilation::CreateCompilation());

		FNiagaraCompilationOptions Options;
		Options.System = this;
		Options.TargetPlatform = TargetPlatform;
		Options.bForced = bForce;

		bLaunchedCompilations = CurrentCompilation->Launch(Options);
	}

	// We might be able to just complete compilation right now if nothing needed compilation.
	if (!bLaunchedCompilations && ActiveCompilations.Num() == 1)
	{
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

	return bLaunchedCompilations;
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

		for (const FNiagaraVariableBase& Var : SystemSpawnScript->GetVMExecutableData().Attributes)
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

		for (const FNiagaraVariableBase& Var : SystemUpdateScript->GetVMExecutableData().Attributes)
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
			//-TODO:Stateless: Do we need stateless support here?
			//if (ensureMsgf(Emitter != nullptr, TEXT("Failed to get Emitter Instance from Emitter Handle in post compile, please investigate.")))
			if (Emitter != nullptr)
			{
				InitEmitterVariableAliasNames(NewEmitterCompiledData[EmitterIdx].Get(), Emitter);
				InitEmitterDataSetCompiledData(EmitterDataSetCompiledData, EmitterHandle);			
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

	//-TODO:FNiagaraVariableBase Verify if EngineParamsSpawn->Parameters can also be moved to base
	auto CreateDataSetCompiledData = [&](FNiagaraDataSetCompiledData& CompiledData, TConstStridedView<FNiagaraVariableBase> Vars)
	{
		CompiledData.Empty();

		CompiledData.Variables.Reset(Vars.Num());
		for (const FNiagaraVariableBase& Var : Vars)
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

	CreateDataSetCompiledData(SystemCompiledData.DataSetCompiledData, MakeStridedView(SystemUpdateScriptData.Attributes));

	const FNiagaraParameters* EngineParamsSpawn = SystemSpawnScriptData.DataSetToParameters.Find(TEXT("Engine"));
	CreateDataSetCompiledData(SystemCompiledData.SpawnInstanceParamsDataSetCompiledData, EngineParamsSpawn ? MakeConstStridedViewOfBase<FNiagaraVariableBase, FNiagaraVariable>(EngineParamsSpawn->Parameters) : MakeConstStridedView<FNiagaraVariableBase>(0, nullptr, 0));
	const FNiagaraParameters* EngineParamsUpdate = SystemUpdateScriptData.DataSetToParameters.Find(TEXT("Engine"));
	CreateDataSetCompiledData(SystemCompiledData.UpdateInstanceParamsDataSetCompiledData, EngineParamsUpdate ? MakeConstStridedViewOfBase<FNiagaraVariableBase, FNiagaraVariable>(EngineParamsUpdate->Parameters) : MakeConstStridedView<FNiagaraVariableBase>(0, nullptr, 0));

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
		//if (ensureMsgf(Emitter != nullptr, TEXT("Failed to get Emitter Instance from Emitter Handle when post compiling Niagara System %s!"), *GetPathNameSafe(this)))
		if (Emitter != nullptr)
		//-TODO:Stateless:
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
	EmitterExecutionOrder.Empty();
	RendererPostTickOrder.Empty();
	RendererCompletionOrder.Empty();

	// while we'd like to remove these as well, BP will generate warnings for missing parameters
	//ExposedParameters = FNiagaraUserRedirectionParameterStore();

	// in the case of dedicated servers, clearing out the system is sufficient to remove unnecessary data as the emitters and scripts
	// will not be loaded on the target platform.  This is not strictly true for all platforms that don't support AV data and so we
	// also want to be a bit more invasive with resetting the underlying objects (Emitters & Scripts).
	ForEachScript([&](UNiagaraScript* Script)
	{
		if (Script)
		{
			Script->InvalidateCompileResults(TEXT("Parent system ResetToEmptySystem"));
		}
	});
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
#else
	return static_cast<const UObjectBaseUtility*>(this)->GetStatID();
#endif
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
		TGraphTask<FNiagaraSystemNullCompletionTask>::CreateTask(&CompleteTaskPrereqs, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(this, &ScriptOptimizationCompletionEvent);
	}

	bNeedsAsyncOptimize = false;
}

FGraphEventRef UNiagaraSystem::GetScriptOptimizationCompletionEvent()
{
	if (ScriptOptimizationCompletionEvent.IsValid())
	{
		if (!ScriptOptimizationCompletionEvent->IsComplete())
		{
			return ScriptOptimizationCompletionEvent;
		}
		ScriptOptimizationCompletionEvent = nullptr;
	}
	return nullptr;
}

void UNiagaraSystem::UpdateStatID() const
{
#if STATS
	if (StatID_GT.IsValidStat())
	{
		GenerateStatID();
	}
#endif
}

void UNiagaraSystem::GenerateStatID() const
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
	TArray<UNiagaraScript*> ScriptsToProcess;
	ScriptsToProcess.Reserve(2 * (EmitterHandles.Num() + 1));
	ScriptsToProcess.Add(SystemSpawnScript);
	ScriptsToProcess.Add(SystemUpdateScript);

	for (int32 i = 0; i < EmitterHandles.Num(); i++)
	{
		const FNiagaraEmitterHandle& Handle = EmitterHandles[i];
		FVersionedNiagaraEmitterData* EmitterData = Handle.GetEmitterData();
		if (EmitterData && Handle.GetIsEnabled())
		{
			ScriptsToProcess.Add(EmitterData->EmitterSpawnScriptProps.Script);
			ScriptsToProcess.Add(EmitterData->EmitterUpdateScriptProps.Script);
		}		
	}

	for (const UNiagaraScript* ScriptToProcess : ScriptsToProcess)
	{
		if (ScriptToProcess)
		{
			ScriptToProcess->GatherScriptStaticVariables(OutEmitterVars);
		}
	}
}
#endif

void UNiagaraSystem::ResolveRequiresScripts()
{
#if WITH_EDITORONLY_DATA
	TOptional<FNiagaraSystemStateData> NewSystemStateData;

	if (bAllowSystemStateFastPath)
	{
		INiagaraModule& NiagaraModule = FModuleManager::GetModuleChecked<INiagaraModule>("Niagara");
		const INiagaraEditorOnlyDataUtilities& EditorOnlyDataUtilities = NiagaraModule.GetEditorOnlyDataUtilities();
		NewSystemStateData = EditorOnlyDataUtilities.TryGetSystemStateData(*this);
	}

	bSystemStateFastPathEnabled = NewSystemStateData.IsSet();
	SystemStateData = NewSystemStateData.Get(FNiagaraSystemStateData());
#endif //WITH_EDITORONLY_DATA
}

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

	FNiagaraWorldManager::RequestInvalidateCachedSystemScalabilityDataForAllWorlds();
}

bool UNiagaraSystem::IsAllowedByScalability() const
{
	return Platforms.IsActive();
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

ENiagaraCullProxyMode UNiagaraSystem::GetCullProxyMode()const
{
	return GetScalabilitySettings().CullProxyMode;
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
				int32 ParamOffset = ParameterOffset + Layout.LayoutInfo.GetFloatComponentByteOffset(CompIdx);
				int32 DataSetOffset = Layout.GetFloatComponentStart() + NumFloats++;
				auto& Binding = FloatOffsets.AddDefaulted_GetRef();
				Binding.ParameterOffset = ParamOffset;
				Binding.DataSetComponentOffset = DataSetOffset;
			}
			for (uint32 CompIdx = 0; CompIdx < Layout.GetNumInt32Components(); ++CompIdx)
			{
				int32 ParamOffset = ParameterOffset + Layout.LayoutInfo.GetInt32ComponentByteOffset(CompIdx);
				int32 DataSetOffset = Layout.GetInt32ComponentStart() + NumInts++;
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
		Modify();
		BakerSettings = NewObject<UNiagaraBakerSettings>(this, "BakerSettings", RF_Transactional);
		//PostEditChange();
	}
	return BakerSettings;
}

void UNiagaraSystem::SetBakerGeneratedSettings(UNiagaraBakerSettings* Settings)
{
	if (BakerGeneratedSettings != Settings)
	{
		Modify();
		BakerGeneratedSettings = Settings;
		PostEditChange();
	}
}

#endif


#if NIAGARA_SYSTEM_CAPTURE
const FNiagaraDataSetCompiledData& FNiagaraEmitterCompiledData::GetGPUCaptureDataSetCompiledData()const
{
	if (GPUCaptureDataSetCompiledData.Variables.Num() == 0)
	{
		static FName GPUCaptureDataSetName = TEXT("GPU Capture Dataset");
		GPUCaptureDataSetCompiledData.ID = FNiagaraDataSetID(GPUCaptureDataSetName, ENiagaraDataSetType::ParticleData);
		GPUCaptureDataSetCompiledData.Variables = DataSetCompiledData.Variables;
		GPUCaptureDataSetCompiledData.SimTarget = ENiagaraSimTarget::CPUSim;
		GPUCaptureDataSetCompiledData.BuildLayout();
	}

	return GPUCaptureDataSetCompiledData;
}
#endif

#undef LOCTEXT_NAMESPACE // NiagaraSystem

