// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "NiagaraTypes.h"
#include "Templates/SharedPointer.h"
#include "NiagaraCompilationTypes.h"
#include "NiagaraPerfBaseline.h"
#include "NiagaraDebuggerCommon.h"
#include "NiagaraScript.h"
#include "Templates/PimplPtr.h"

class FNiagaraWorldManager;
class UNiagaraEmitter;
struct FNiagaraVMExecutableData;
class UNiagaraScript;
class FNiagaraCompileOptions;
class FNiagaraCompileRequestDataBase;
class FNiagaraCompileRequestDuplicateDataBase;
class INiagaraMergeManager;
class INiagaraEditorOnlyDataUtilities;
struct FNiagaraParameterStore;
class FCommonViewportClient;
class FNiagaraDebuggerClient;
struct FNiagaraSystemAsyncCompileResults;

extern NIAGARA_API int32 GEnableVerboseNiagaraChangeIdLogging;

/**
* Niagara module interface
*/
class INiagaraModule : public IModuleInterface
{
public:
#if WITH_EDITOR
	typedef TSharedPtr<FNiagaraCompileRequestDataBase, ESPMode::ThreadSafe> CompileRequestPtr;
	typedef TSharedPtr<FNiagaraCompileRequestDuplicateDataBase, ESPMode::ThreadSafe> CompileRequestDuplicatePtr;
	typedef TSharedPtr<FNiagaraGraphCachedDataBase, ESPMode::ThreadSafe> GraphCachedDataPtr;

	DECLARE_DELEGATE_RetVal_ThreeParams(int32, FScriptCompiler, const FNiagaraCompileRequestDataBase*, const FNiagaraCompileRequestDuplicateDataBase*, const FNiagaraCompileOptions&);
	DECLARE_DELEGATE_RetVal_ThreeParams(TSharedPtr<FNiagaraVMExecutableData>, FCheckCompilationResult, int32, bool, FNiagaraScriptCompileMetrics&);
	DECLARE_DELEGATE_RetVal_TwoParams(CompileRequestPtr, FOnPrecompile, UObject*, FGuid);
	DECLARE_DELEGATE_RetVal_FiveParams(CompileRequestDuplicatePtr, FOnPrecompileDuplicate, const FNiagaraCompileRequestDataBase* /*OwningSystemRequestData*/, UNiagaraSystem* /*OwningSystem*/, UNiagaraEmitter* /*OwningEmitter*/, UNiagaraScript* /*TargetScript*/, FGuid /*Version*/);
	DECLARE_DELEGATE_RetVal_TwoParams(GraphCachedDataPtr, FOnCacheGraphTraversal, const UObject*, FGuid);

	DECLARE_DELEGATE_RetVal_ThreeParams(FNiagaraCompilationTaskHandle, FOnRequestCompileSystem, UNiagaraSystem*, bool, const ITargetPlatform*);
	DECLARE_DELEGATE_RetVal_FourParams(bool, FOnPollSystemCompile, FNiagaraCompilationTaskHandle, FNiagaraSystemAsyncCompileResults&, bool /*bWait*/, bool /*bPeek*/);
	DECLARE_DELEGATE_OneParam(FOnAbortSystemCompile, FNiagaraCompilationTaskHandle);

#endif
	DECLARE_DELEGATE_RetVal(void, FOnProcessQueue);

public:
	NIAGARA_API virtual void StartupModule()override;
	NIAGARA_API virtual void ShutdownModule()override;
	
	/** Get the instance of this module. */
	NIAGARA_API static INiagaraModule& Get();
	
	NIAGARA_API void ShutdownRenderingResources();
	
	NIAGARA_API void OnPostEngineInit();
	NIAGARA_API void OnPreExit();

	NIAGARA_API void OnWorldTickStart(UWorld* World, ELevelTick TickType, float DeltaSeconds);
	NIAGARA_API void OnBeginFrame();
	NIAGARA_API void OnPostGarbageCollect();
	NIAGARA_API void OnWorldBeginTearDown(UWorld* World);

	NIAGARA_API FDelegateHandle SetOnProcessShaderCompilationQueue(FOnProcessQueue InOnProcessQueue);
	NIAGARA_API void ResetOnProcessShaderCompilationQueue(FDelegateHandle DelegateHandle);
	NIAGARA_API void ProcessShaderCompilationQueue();

#if WITH_NIAGARA_DEBUGGER
	FNiagaraDebuggerClient* GetDebuggerClient() { return DebuggerClient.Get(); }
#endif

#if WITH_EDITOR
	NIAGARA_API const INiagaraMergeManager& GetMergeManager() const;

	NIAGARA_API void RegisterMergeManager(TSharedRef<INiagaraMergeManager> InMergeManager);

	NIAGARA_API void UnregisterMergeManager(TSharedRef<INiagaraMergeManager> InMergeManager);

	NIAGARA_API const INiagaraEditorOnlyDataUtilities& GetEditorOnlyDataUtilities() const;

	NIAGARA_API void RegisterEditorOnlyDataUtilities(TSharedRef<INiagaraEditorOnlyDataUtilities> InEditorOnlyDataUtilities);

	NIAGARA_API void UnregisterEditorOnlyDataUtilities(TSharedRef<INiagaraEditorOnlyDataUtilities> InEditorOnlyDataUtilities);

	NIAGARA_API int32 StartScriptCompileJob(const FNiagaraCompileRequestDataBase* InCompileData, const FNiagaraCompileRequestDuplicateDataBase* InCompileDuplicateData, const FNiagaraCompileOptions& InCompileOptions);
	NIAGARA_API TSharedPtr<FNiagaraVMExecutableData> GetCompileJobResult(int32 JobID, bool bWait, FNiagaraScriptCompileMetrics& Metrics);

	NIAGARA_API FDelegateHandle RegisterScriptCompiler(FScriptCompiler ScriptCompiler);
	NIAGARA_API void UnregisterScriptCompiler(FDelegateHandle DelegateHandle);

	NIAGARA_API FDelegateHandle RegisterCompileResultDelegate(FCheckCompilationResult ResultDelegate);
	NIAGARA_API void UnregisterCompileResultDelegate(FDelegateHandle DelegateHandle);

	NIAGARA_API TSharedPtr<FNiagaraCompileRequestDataBase, ESPMode::ThreadSafe> Precompile(UObject* InObj, FGuid Version);
	NIAGARA_API TSharedPtr<FNiagaraCompileRequestDuplicateDataBase, ESPMode::ThreadSafe> PrecompileDuplicate(
		const FNiagaraCompileRequestDataBase* OwningSystemRequestData,
		UNiagaraSystem* OwningSystem,
		UNiagaraEmitter* OwningEmitter,
		UNiagaraScript* TargetScript,
		FGuid TargetVersion);
	NIAGARA_API FDelegateHandle RegisterPrecompiler(FOnPrecompile PreCompiler);
	NIAGARA_API void UnregisterPrecompiler(FDelegateHandle DelegateHandle);
	NIAGARA_API FDelegateHandle RegisterPrecompileDuplicator(FOnPrecompileDuplicate PreCompileDuplicator);
	NIAGARA_API void UnregisterPrecompileDuplicator(FDelegateHandle DelegateHandle);

	NIAGARA_API TSharedPtr<FNiagaraGraphCachedDataBase, ESPMode::ThreadSafe> CacheGraphTraversal(const UObject* InObj, FGuid Version);
	NIAGARA_API FDelegateHandle RegisterGraphTraversalCacher(FOnCacheGraphTraversal PreCompiler);
	NIAGARA_API void UnregisterGraphTraversalCacher(FDelegateHandle DelegateHandle);

	NIAGARA_API FNiagaraCompilationTaskHandle RequestCompileSystem(UNiagaraSystem* System, bool bForce, const ITargetPlatform* TargetPlatform);
	NIAGARA_API FDelegateHandle RegisterRequestCompileSystem(FOnRequestCompileSystem RequestCompileSystemCallback);
	NIAGARA_API void UnregisterRequestCompileSystem(FDelegateHandle DelegateHandle);

	NIAGARA_API bool PollSystemCompile(FNiagaraCompilationTaskHandle, FNiagaraSystemAsyncCompileResults&, bool /*bWait*/, bool /*bPeek*/);
	NIAGARA_API FDelegateHandle RegisterPollSystemCompile(FOnPollSystemCompile PollSystemCompileCallback);
	NIAGARA_API void UnregisterPollSystemCompile(FDelegateHandle DelegateHandle);

	NIAGARA_API void AbortSystemCompile(FNiagaraCompilationTaskHandle);
	NIAGARA_API FDelegateHandle RegisterAbortSystemCompile(FOnAbortSystemCompile AbortSystemCompileCallback);
	NIAGARA_API void UnregisterAbortSystemCompile(FDelegateHandle DelegateHandle);

	NIAGARA_API void OnAssetLoaded(UObject* Asset);
#endif

	static NIAGARA_API void RequestRefreshDataChannels() { bDataChannelRefreshRequested = true; }
	static NIAGARA_API void RefreshDataChannels();

	const TArray<const FNiagaraAssetTagDefinition*>& GetInternalAssetTagDefinitions() { return InternalAssetTagDefinitions; }
	void RegisterInternalAssetTagDefinitions();
	
#if NIAGARA_PERF_BASELINES
	NIAGARA_API void GeneratePerfBaselines(TArray<UNiagaraEffectType*>& BaselinesToGenerate);

	NIAGARA_API bool ToggleStatPerfBaselines(UWorld* World, FCommonViewportClient* ViewportClient, const TCHAR* Stream = nullptr);
	NIAGARA_API int32 RenderStatPerfBaselines(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation = nullptr, const FRotator* ViewRotation = nullptr);
#endif

	FORCEINLINE static bool UseGlobalFXBudget() { return bUseGlobalFXBudget; }
	static NIAGARA_API void OnUseGlobalFXBudgetChanged(IConsoleVariable* Variable);

	FORCEINLINE static bool DataChannelsEnabled() { return bDataChannelsEnabled; }
	static NIAGARA_API void OnDataChannelsEnabledChanged(IConsoleVariable* Variable);

	FORCEINLINE static float GetGlobalSpawnCountScale() { return EngineGlobalSpawnCountScale; }
	FORCEINLINE static float GetGlobalSystemCountScale() { return EngineGlobalSystemCountScale; }

	static NIAGARA_API float EngineGlobalSpawnCountScale;
	static NIAGARA_API float EngineGlobalSystemCountScale;

	FORCEINLINE static const FNiagaraVariable&  GetVar_Engine_WorldDeltaTime() { return Engine_WorldDeltaTime; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Engine_DeltaTime() { return Engine_DeltaTime; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Engine_InvDeltaTime() { return Engine_InvDeltaTime; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Engine_Time() { return Engine_Time; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Engine_RealTime() { return Engine_RealTime; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Engine_QualityLevel() { return Engine_QualityLevel; }

	FORCEINLINE static const FNiagaraVariable&  GetVar_Engine_Owner_Position() { return Engine_Owner_Position; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Engine_Owner_Velocity() { return Engine_Owner_Velocity; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Engine_Owner_XAxis() { return Engine_Owner_XAxis; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Engine_Owner_YAxis() { return Engine_Owner_YAxis; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Engine_Owner_ZAxis() { return Engine_Owner_ZAxis; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Engine_Owner_Scale() { return Engine_Owner_Scale; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Engine_Owner_Rotation() { return Engine_Owner_Rotation; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Engine_Owner_LWC_Tile() { return Engine_Owner_LWC_Tile; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Engine_ExecIndex() { return Engine_ExecIndex; }

	FORCEINLINE static const FNiagaraVariable&  GetVar_Engine_Owner_SystemLocalToWorld() { return Engine_Owner_SystemLocalToWorld; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Engine_Owner_SystemWorldToLocal() { return Engine_Owner_SystemWorldToLocal; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Engine_Owner_SystemLocalToWorldTransposed() { return Engine_Owner_SystemLocalToWorldTransposed; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Engine_Owner_SystemWorldToLocalTransposed() { return Engine_Owner_SystemWorldToLocalTransposed; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Engine_Owner_SystemLocalToWorldNoScale() { return Engine_Owner_SystemLocalToWorldNoScale; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Engine_Owner_SystemWorldToLocalNoScale() { return Engine_Owner_SystemWorldToLocalNoScale; }

	FORCEINLINE static const FNiagaraVariable&  GetVar_Engine_Owner_TimeSinceRendered() { return Engine_Owner_TimeSinceRendered; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Engine_Owner_LODDistance() { return Engine_Owner_LODDistance; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Engine_Owner_LODDistanceFraction() { return Engine_Owner_LODDistanceFraction; }

	FORCEINLINE static const FNiagaraVariable&  GetVar_Engine_Owner_ExecutionState() { return Engine_Owner_ExecutionState; }

	FORCEINLINE static const FNiagaraVariable&  GetVar_Engine_ExecutionCount() { return Engine_ExecutionCount; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Engine_Emitter_NumParticles() { return Engine_Emitter_NumParticles; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Engine_Emitter_SimulationPosition() { return Engine_Emitter_SimulationPosition; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Engine_Emitter_TotalSpawnedParticles() { return Engine_Emitter_TotalSpawnedParticles; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Engine_Emitter_SpawnCountScale() { return Engine_Emitter_SpawnCountScale; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Engine_Emitter_InstanceSeed() { return Engine_Emitter_InstanceSeed; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Engine_Emitter_ID() { return Engine_Emitter_ID; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Engine_System_TickCount() { return Engine_System_TickCount; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Engine_System_NumEmittersAlive() { return Engine_System_NumEmittersAlive; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Engine_System_SignificanceIndex() { return Engine_System_SignificanceIndex; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Engine_System_RandomSeed() { return Engine_System_RandomSeed; }

	FORCEINLINE static const FNiagaraVariable& GetVar_Engine_System_CurrentTimeStep() { return Engine_System_CurrentTimeStep; }
	FORCEINLINE static const FNiagaraVariable& GetVar_Engine_System_NumTimeSteps() { return Engine_System_NumTimeSteps; }
	FORCEINLINE static const FNiagaraVariable& GetVar_Engine_System_TimeStepFraction() { return Engine_System_TimeStepFraction; }
	FORCEINLINE static const FNiagaraVariable& GetVar_Engine_System_NumParticles() { return Engine_System_NumParticles; }

	FORCEINLINE static const FNiagaraVariable&  GetVar_Engine_System_NumEmitters() { return Engine_System_NumEmitters; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Engine_NumSystemInstances() { return Engine_NumSystemInstances; }

	FORCEINLINE static const FNiagaraVariable&  GetVar_Engine_GlobalSpawnCountScale() { return Engine_GlobalSpawnCountScale; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Engine_GlobalSystemScale() { return Engine_GlobalSystemScale; }

	FORCEINLINE static const FNiagaraVariable&  GetVar_Engine_System_Age() { return Engine_System_Age; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Emitter_Age() { return Emitter_Age; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Emitter_LocalSpace() { return Emitter_LocalSpace; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Emitter_Determinism() { return Emitter_Determinism; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Emitter_InterpolatedSpawn() { return Emitter_InterpolatedSpawn; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Emitter_OverrideGlobalSpawnCountScale() { return Emitter_OverrideGlobalSpawnCountScale; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Emitter_RandomSeed() { return Emitter_RandomSeed; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Emitter_SpawnRate() { return Emitter_SpawnRate; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Emitter_SpawnInterval() { return Emitter_SpawnInterval; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Emitter_SimulationTarget() { return Emitter_SimulationTarget; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_ScriptUsage() { return ScriptUsage; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_ScriptContext() { return ScriptContext; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_FunctionDebugState() { return FunctionDebugState; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Emitter_InterpSpawnStartDt() { return Emitter_InterpSpawnStartDt; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Emitter_SpawnGroup() { return Emitter_SpawnGroup; }

	FORCEINLINE static const FNiagaraVariable&  GetVar_Particles_UniqueID() { return Particles_UniqueID; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Particles_ID() { return Particles_ID; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Particles_Position() { return Particles_Position; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Particles_Velocity() { return Particles_Velocity; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Particles_Color() { return Particles_Color; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Particles_SpriteRotation() { return Particles_SpriteRotation; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Particles_NormalizedAge() { return Particles_NormalizedAge; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Particles_SpriteSize() { return Particles_SpriteSize; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Particles_SpriteFacing() { return Particles_SpriteFacing; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Particles_SpriteAlignment() { return Particles_SpriteAlignment; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Particles_SubImageIndex() { return Particles_SubImageIndex; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Particles_DynamicMaterialParameter() { return Particles_DynamicMaterialParameter; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Particles_DynamicMaterialParameter1() { return Particles_DynamicMaterialParameter1; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Particles_DynamicMaterialParameter2() { return Particles_DynamicMaterialParameter2; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Particles_DynamicMaterialParameter3() { return Particles_DynamicMaterialParameter3; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Particles_Scale() { return Particles_Scale; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Particles_Lifetime() { return Particles_Lifetime; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Particles_MeshOrientation() { return Particles_MeshOrientation; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Particles_UVScale() { return Particles_UVScale; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Particles_PivotOffset() { return Particles_PivotOffset; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Particles_CameraOffset() { return Particles_CameraOffset; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Particles_MaterialRandom() { return Particles_MaterialRandom; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Particles_LightRadius() { return Particles_LightRadius; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Particles_LightExponent() { return Particles_LightExponent; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Particles_LightEnabled() { return Particles_LightEnabled; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Particles_LightVolumetricScattering() { return Particles_LightVolumetricScattering; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Particles_LightSpecularScale() { return Particles_LightSpecularScale; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Particles_RibbonID() { return Particles_RibbonID; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Particles_RibbonWidth() { return Particles_RibbonWidth; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Particles_RibbonTwist() { return Particles_RibbonTwist; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Particles_RibbonFacing() { return Particles_RibbonFacing; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Particles_RibbonLinkOrder() { return Particles_RibbonLinkOrder; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Particles_RibbonUVDistance() { return Particles_RibbonUVDistance; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Particles_RibbonU0Override() { return Particles_RibbonU0Override; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Particles_RibbonV0RangeOverride() { return Particles_RibbonV0RangeOverride; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Particles_RibbonU1Override() { return Particles_RibbonU1Override; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Particles_RibbonV1RangeOverride() { return Particles_RibbonV1RangeOverride; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Particles_VisibilityTag() { return Particles_VisibilityTag; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Particles_MeshIndex() { return Particles_MeshIndex; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_Particles_ComponentsEnabled() { return Particles_ComponentsEnabled; }
	
	FORCEINLINE static const FNiagaraVariable&  GetVar_DataInstance_Alive() { return DataInstance_Alive; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_BeginDefaults() { return Translator_BeginDefaults; }
	FORCEINLINE static const FNiagaraVariable&  GetVar_CallID() { return Translator_CallID; }
	
	FOnProcessQueue OnProcessQueue;
	
#if WITH_EDITORONLY_DATA
	TSharedPtr<INiagaraMergeManager> MergeManager;
	TSharedPtr<INiagaraEditorOnlyDataUtilities> EditorOnlyDataUtilities;

	FScriptCompiler ScriptCompilerDelegate;
	FCheckCompilationResult CompilationResultDelegate;
	FOnPrecompile PrecompileDelegate;
	FOnPrecompileDuplicate PrecompileDuplicateDelegate;
	FOnCacheGraphTraversal GraphTraversalCacheDelegate;
	FOnRequestCompileSystem RequestCompileSystemDelegate;
	FOnPollSystemCompile PollSystemCompileDelegate;
	FOnAbortSystemCompile AbortSystemCompileDelegate;
#endif

	static NIAGARA_API int32 EngineEffectsQuality;

	static NIAGARA_API bool bUseGlobalFXBudget;
	static NIAGARA_API bool bDataChannelsEnabled;

	static const FNiagaraAssetTagDefinition TemplateTagDefinition;
	static const FNiagaraAssetTagDefinition LearningContentTagDefinition;
	TArray<const FNiagaraAssetTagDefinition*> InternalAssetTagDefinitions;
private:
	static NIAGARA_API FNiagaraVariable Engine_WorldDeltaTime;
	static NIAGARA_API FNiagaraVariable Engine_DeltaTime;
	static NIAGARA_API FNiagaraVariable Engine_InvDeltaTime;
	static NIAGARA_API FNiagaraVariable Engine_Time; 
	static NIAGARA_API FNiagaraVariable Engine_RealTime; 
	static NIAGARA_API FNiagaraVariable Engine_QualityLevel; 

	static NIAGARA_API FNiagaraVariable Engine_Owner_Position;
	static NIAGARA_API FNiagaraVariable Engine_Owner_Velocity;
	static NIAGARA_API FNiagaraVariable Engine_Owner_XAxis;
	static NIAGARA_API FNiagaraVariable Engine_Owner_YAxis;
	static NIAGARA_API FNiagaraVariable Engine_Owner_ZAxis;
	static NIAGARA_API FNiagaraVariable Engine_Owner_Scale;
	static NIAGARA_API FNiagaraVariable Engine_Owner_Rotation;
	static NIAGARA_API FNiagaraVariable Engine_Owner_LWC_Tile;
	static NIAGARA_API FNiagaraVariable Engine_ExecIndex;

	static NIAGARA_API FNiagaraVariable Engine_Owner_SystemLocalToWorld;
	static NIAGARA_API FNiagaraVariable Engine_Owner_SystemWorldToLocal;
	static NIAGARA_API FNiagaraVariable Engine_Owner_SystemLocalToWorldTransposed;
	static NIAGARA_API FNiagaraVariable Engine_Owner_SystemWorldToLocalTransposed;
	static NIAGARA_API FNiagaraVariable Engine_Owner_SystemLocalToWorldNoScale;
	static NIAGARA_API FNiagaraVariable Engine_Owner_SystemWorldToLocalNoScale;

	static NIAGARA_API FNiagaraVariable Engine_Owner_TimeSinceRendered;
	static NIAGARA_API FNiagaraVariable Engine_Owner_LODDistance;
	static NIAGARA_API FNiagaraVariable Engine_Owner_LODDistanceFraction;
	
	static NIAGARA_API FNiagaraVariable Engine_Owner_ExecutionState;

	static NIAGARA_API FNiagaraVariable Engine_ExecutionCount;
	static NIAGARA_API FNiagaraVariable Engine_Emitter_NumParticles;
	static NIAGARA_API FNiagaraVariable Engine_Emitter_SimulationPosition;
	static NIAGARA_API FNiagaraVariable Engine_Emitter_TotalSpawnedParticles;
	static NIAGARA_API FNiagaraVariable Engine_Emitter_SpawnCountScale;
	static NIAGARA_API FNiagaraVariable Engine_System_TickCount;
	static NIAGARA_API FNiagaraVariable Engine_System_NumEmittersAlive;
	static NIAGARA_API FNiagaraVariable Engine_System_SignificanceIndex;
	static NIAGARA_API FNiagaraVariable Engine_System_RandomSeed;
	
	static NIAGARA_API FNiagaraVariable Engine_System_CurrentTimeStep;
	static NIAGARA_API FNiagaraVariable Engine_System_NumTimeSteps;
	static NIAGARA_API FNiagaraVariable Engine_System_TimeStepFraction;
	static NIAGARA_API FNiagaraVariable Engine_System_NumParticles;
	
	static NIAGARA_API FNiagaraVariable Engine_System_NumEmitters;
	static NIAGARA_API FNiagaraVariable Engine_NumSystemInstances;

	static NIAGARA_API FNiagaraVariable Engine_GlobalSpawnCountScale;
	static NIAGARA_API FNiagaraVariable Engine_GlobalSystemScale;

	static NIAGARA_API FNiagaraVariable Engine_System_Age;
	static NIAGARA_API FNiagaraVariable Emitter_Age;
	static NIAGARA_API FNiagaraVariable Emitter_LocalSpace;
	static NIAGARA_API FNiagaraVariable Emitter_Determinism;
	static NIAGARA_API FNiagaraVariable Emitter_InterpolatedSpawn;
	static NIAGARA_API FNiagaraVariable Emitter_OverrideGlobalSpawnCountScale;
	static NIAGARA_API FNiagaraVariable Emitter_SimulationTarget;
	static NIAGARA_API FNiagaraVariable Emitter_RandomSeed;
	static NIAGARA_API FNiagaraVariable Engine_Emitter_InstanceSeed;
	static NIAGARA_API FNiagaraVariable Engine_Emitter_ID;
	static NIAGARA_API FNiagaraVariable Emitter_SpawnRate;
	static NIAGARA_API FNiagaraVariable Emitter_SpawnInterval;
	static NIAGARA_API FNiagaraVariable Emitter_InterpSpawnStartDt;
	static NIAGARA_API FNiagaraVariable Emitter_SpawnGroup;

	static NIAGARA_API FNiagaraVariable Particles_UniqueID;
	static NIAGARA_API FNiagaraVariable Particles_ID;
	static NIAGARA_API FNiagaraVariable Particles_Position;
	static NIAGARA_API FNiagaraVariable Particles_Velocity;
	static NIAGARA_API FNiagaraVariable Particles_Color;
	static NIAGARA_API FNiagaraVariable Particles_SpriteRotation;
	static NIAGARA_API FNiagaraVariable Particles_NormalizedAge;
	static NIAGARA_API FNiagaraVariable Particles_SpriteSize;
	static NIAGARA_API FNiagaraVariable Particles_SpriteFacing;
	static NIAGARA_API FNiagaraVariable Particles_SpriteAlignment;
	static NIAGARA_API FNiagaraVariable Particles_SubImageIndex;
	static NIAGARA_API FNiagaraVariable Particles_DynamicMaterialParameter;
	static NIAGARA_API FNiagaraVariable Particles_DynamicMaterialParameter1;
	static NIAGARA_API FNiagaraVariable Particles_DynamicMaterialParameter2;
	static NIAGARA_API FNiagaraVariable Particles_DynamicMaterialParameter3;
	static NIAGARA_API FNiagaraVariable Particles_Scale;
	static NIAGARA_API FNiagaraVariable Particles_Lifetime;
	static NIAGARA_API FNiagaraVariable Particles_MeshOrientation;
	static NIAGARA_API FNiagaraVariable Particles_VisibilityTag;
	static NIAGARA_API FNiagaraVariable Particles_MeshIndex;
	static NIAGARA_API FNiagaraVariable Particles_UVScale;
	static NIAGARA_API FNiagaraVariable Particles_PivotOffset;
	static NIAGARA_API FNiagaraVariable Particles_CameraOffset;
	static NIAGARA_API FNiagaraVariable Particles_MaterialRandom;
	static NIAGARA_API FNiagaraVariable Particles_LightRadius;
	static NIAGARA_API FNiagaraVariable Particles_LightExponent;
	static NIAGARA_API FNiagaraVariable Particles_LightEnabled;
	static NIAGARA_API FNiagaraVariable Particles_LightVolumetricScattering;
	static NIAGARA_API FNiagaraVariable Particles_LightSpecularScale;
	static NIAGARA_API FNiagaraVariable Particles_RibbonID;
	static NIAGARA_API FNiagaraVariable Particles_RibbonWidth;
	static NIAGARA_API FNiagaraVariable Particles_RibbonTwist;
	static NIAGARA_API FNiagaraVariable Particles_RibbonFacing;
	static NIAGARA_API FNiagaraVariable Particles_RibbonLinkOrder;
	static NIAGARA_API FNiagaraVariable Particles_ComponentsEnabled;
	static NIAGARA_API FNiagaraVariable Particles_RibbonUVDistance;
	static NIAGARA_API FNiagaraVariable Particles_RibbonU0Override;
	static NIAGARA_API FNiagaraVariable Particles_RibbonV0RangeOverride;
	static NIAGARA_API FNiagaraVariable Particles_RibbonU1Override;
	static NIAGARA_API FNiagaraVariable Particles_RibbonV1RangeOverride;

	static NIAGARA_API FNiagaraVariable ScriptUsage;
	static NIAGARA_API FNiagaraVariable ScriptContext;
	static NIAGARA_API FNiagaraVariable FunctionDebugState;
	static NIAGARA_API FNiagaraVariable DataInstance_Alive;
	static NIAGARA_API FNiagaraVariable Translator_BeginDefaults;
	static NIAGARA_API FNiagaraVariable Translator_CallID;

#if NIAGARA_PERF_BASELINES
	TUniquePtr<FNiagaraPerfBaselineHandler> BaselineHandler;
#endif

#if WITH_NIAGARA_DEBUGGER
	TPimplPtr<FNiagaraDebuggerClient> DebuggerClient;
#endif

	FDelegateHandle OnCVarUnregisteredHandle;

	static std::atomic<bool> bDataChannelRefreshRequested;
};

