// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "NiagaraBakerSettings.h"
#include "NiagaraCommon.h"
#include "NiagaraDataSet.h"
#include "NiagaraDataSetAccessor.h"
#include "NiagaraEffectType.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraParameterCollection.h"
#include "NiagaraParameterDefinitionsSubscriber.h"
#include "NiagaraSystemStaticBuffers.h"
#include "NiagaraUserRedirectionParameterStore.h"
#include "Particles/ParticlePerfStats.h"
#include "Particles/ParticleSystem.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "Components/PrimitiveComponent.h"

#include "NiagaraDataInterfacePlatformSet.h"


#include "NiagaraSystem.generated.h"

class FNiagaraAsyncCompileTask;

#if WITH_EDITORONLY_DATA
class UNiagaraEditorDataBase;
#endif

USTRUCT()
struct FNiagaraEmitterCompiledData
{
	GENERATED_USTRUCT_BODY()

	FNiagaraEmitterCompiledData();
	
	/** Attribute names in the data set that are driving each emitter's spawning. */
	UPROPERTY()
	TArray<FName> SpawnAttributes;

	/** Explicit list of Niagara Variables to bind to Emitter instances. */
	UPROPERTY()
	FNiagaraVariable EmitterSpawnIntervalVar;

	UPROPERTY()
	FNiagaraVariable EmitterInterpSpawnStartDTVar;

	UPROPERTY()
	FNiagaraVariable EmitterSpawnGroupVar;

	UPROPERTY()
	FNiagaraVariable EmitterAgeVar;

	UPROPERTY()
	FNiagaraVariable EmitterRandomSeedVar;

	UPROPERTY()
	FNiagaraVariable EmitterInstanceSeedVar;

	UPROPERTY()
	FNiagaraVariable EmitterTotalSpawnedParticlesVar;

	/** Per-Emitter DataSet Data. */
	UPROPERTY()
	FNiagaraDataSetCompiledData DataSetCompiledData;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FNiagaraDataSetCompiledData GPUCaptureDataSetCompiledData;
#endif
};

USTRUCT()
struct FNiagaraParameterDataSetBinding
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	int32 ParameterOffset = 0;

	UPROPERTY()
	int32 DataSetComponentOffset = 0;
};

USTRUCT()
struct FNiagaraParameterDataSetBindingCollection
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<FNiagaraParameterDataSetBinding> FloatOffsets;

	UPROPERTY()
	TArray<FNiagaraParameterDataSetBinding> Int32Offsets;

#if WITH_EDITORONLY_DATA
	template<typename BufferType>
	void Build(const FNiagaraDataSetCompiledData& DataSet)
	{
		BuildInternal(BufferType::GetVariables(), DataSet, TEXT(""), TEXT(""));
	}

	template<typename BufferType>
	void Build(const FNiagaraDataSetCompiledData& DataSet, const FString& NamespaceBase, const FString& NamespaceReplacement)
	{
		BuildInternal(BufferType::GetVariables(), DataSet, NamespaceBase, NamespaceReplacement);
	}

protected:
	void BuildInternal(const TArray<FNiagaraVariable>& ParameterVars, const FNiagaraDataSetCompiledData& DataSet, const FString& NamespaceBase, const FString& NamespaceReplacement);

#endif
};

USTRUCT()
struct FNiagaraSystemCompiledData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FNiagaraParameterStore InstanceParamStore;

	UPROPERTY()
	FNiagaraDataSetCompiledData DataSetCompiledData;

	UPROPERTY()
	FNiagaraDataSetCompiledData SpawnInstanceParamsDataSetCompiledData;

	UPROPERTY()
	FNiagaraDataSetCompiledData UpdateInstanceParamsDataSetCompiledData;

	UPROPERTY()
	FNiagaraParameterDataSetBindingCollection SpawnInstanceGlobalBinding;
	UPROPERTY()
	FNiagaraParameterDataSetBindingCollection SpawnInstanceSystemBinding;
	UPROPERTY()
	FNiagaraParameterDataSetBindingCollection SpawnInstanceOwnerBinding;
	UPROPERTY()
	TArray<FNiagaraParameterDataSetBindingCollection> SpawnInstanceEmitterBindings;

	UPROPERTY()
	FNiagaraParameterDataSetBindingCollection UpdateInstanceGlobalBinding;
	UPROPERTY()
	FNiagaraParameterDataSetBindingCollection UpdateInstanceSystemBinding;
	UPROPERTY()
	FNiagaraParameterDataSetBindingCollection UpdateInstanceOwnerBinding;
	UPROPERTY()
	TArray<FNiagaraParameterDataSetBindingCollection> UpdateInstanceEmitterBindings;
};

USTRUCT()
struct FNiagaraSystemCompileRequest
{
	GENERATED_USTRUCT_BODY()

	double StartTime = 0.0;

	UPROPERTY()
	TArray<TObjectPtr<UObject>> RootObjects;

	TArray<TSharedPtr<FNiagaraAsyncCompileTask, ESPMode::ThreadSafe>> DDCTasks;
	
	bool bIsValid = true;
	bool bForced = false;
	bool bAllScriptsSynchronized = false;

	float CombinedCompileTime = 0.0f;
};

struct FNiagaraEmitterExecutionIndex
{
	FNiagaraEmitterExecutionIndex() { bStartNewOverlapGroup = false; EmitterIndex = 0; }

	/** Flag to denote if the batcher should start a new overlap group, i.e. when we have a dependency ensure we don't overlap with the emitter we depend on. */
	uint32 bStartNewOverlapGroup : 1;
	/** Emitter index to use */
	uint32 EmitterIndex : 31;
};

struct FNiagaraRendererExecutionIndex
{
	/** The index of the emitter */
	uint32 EmitterIndex = INDEX_NONE;
	/** The index of the renderer in the emitter's list */
	uint32 EmitterRendererIndex = INDEX_NONE;
	/** The index of the renderer in the entire system */
	uint32 SystemRendererIndex = INDEX_NONE;
};

/** Container for multiple emitters that combine together to create a particle system effect.*/
UCLASS(BlueprintType, meta= (LoadBehavior = "LazyOnDemand"))
class NIAGARA_API UNiagaraSystem : public UFXSystemAsset, public INiagaraParameterDefinitionsSubscriber
{
	GENERATED_UCLASS_BODY()

public:
#if WITH_EDITOR
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnSystemCompiled, UNiagaraSystem*);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnSystemPostEditChange, UNiagaraSystem*);
	DECLARE_MULTICAST_DELEGATE(FOnScalabilityChanged)
#endif
	//TestChange

	UNiagaraSystem(FVTableHelper& Helper);

	//~ UObject interface
	void PostInitProperties();
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override; 
#if WITH_EDITORONLY_DATA
	static void DeclareConstructClasses(TArray<FTopLevelAssetPath>& OutConstructClasses, const UClass* SpecificSubclass);
#endif
	virtual void BeginDestroy() override;
	PRAGMA_DISABLE_DEPRECATION_WARNINGS // Suppress compiler warning on override of deprecated function
	UE_DEPRECATED(5.0, "Use version that takes FObjectPreSaveContext instead.")
	virtual void PreSave(const class ITargetPlatform* TargetPlatform) override;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;
#if WITH_EDITOR
	virtual void PreEditChange(FProperty* PropertyThatWillChange)override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override; 
	virtual void BeginCacheForCookedPlatformData(const ITargetPlatform *TargetPlatform) override;
	//~ End UObject interface

	/** Helper method to handle when an internal variable has been renamed. Renames any downstream dependencies in the emitters or exposed variables.*/
	void HandleVariableRenamed(const FNiagaraVariable& InOldVariable, const FNiagaraVariable& InNewVariable, bool bUpdateContexts);
	/** Helper method to handle when an internal variable has been removed. Resets any downstream dependencies in the emitters or exposed variables.*/
	void HandleVariableRemoved(const FNiagaraVariable& InOldVariable, bool bUpdateContexts);
#endif

#if WITH_EDITORONLY_DATA
	//~ Begin INiagaraParameterDefinitionsSubscriber interface
	virtual const TArray<FParameterDefinitionsSubscription>& GetParameterDefinitionsSubscriptions() const override { return ParameterDefinitionsSubscriptions; }
	
	virtual TArray<FParameterDefinitionsSubscription>& GetParameterDefinitionsSubscriptions() override { return ParameterDefinitionsSubscriptions; };

	/** Get all UNiagaraScriptSourceBase of this subscriber. */
	virtual TArray<UNiagaraScriptSourceBase*> GetAllSourceScripts() override;

	/** Get the path to the UObject of this subscriber. */
	virtual FString GetSourceObjectPathName() const override;

	/** Get All adapters to editor only script vars owned directly by this subscriber. */
	virtual TArray<UNiagaraEditorParametersAdapterBase*> GetEditorOnlyParametersAdapters() override;

	/** Get all subscribers that are owned by this subscriber.
	 *  Note: Implemented for synchronizing UNiagaraSystem. UNiagaraSystem returns all UNiagaraEmitters it owns to call SynchronizeWithParameterDefinitions for each.
	 */
	virtual TArray<INiagaraParameterDefinitionsSubscriber*> GetOwnedParameterDefinitionsSubscribers() override;
	//~ End INiagaraParameterDefinitionsSubscriber interface

	virtual bool ChangeEmitterVersion(const FVersionedNiagaraEmitter& Emitter, const FGuid& NewVersion);
#endif 

	/** Gets an array of the emitter handles. */
	TArray<FNiagaraEmitterHandle>& GetEmitterHandles();
	const TArray<FNiagaraEmitterHandle>& GetEmitterHandles()const;
	
	FNiagaraSystemScalabilityOverrides& GetScalabilityOverrides(){return SystemScalabilityOverrides; }

private:
	bool IsValidInternal() const;

#if WITH_EDITORONLY_DATA
	void FixupPositionUserParameters();
#endif
	
public:
	/** Returns true if this system is valid and can be instanced. False otherwise. */
	bool IsValid() const { return FPlatformProperties::RequiresCookedData() ? bIsValidCached : IsValidInternal(); }

#if WITH_EDITORONLY_DATA
	/** Adds a new emitter handle to this System.  The new handle exposes an Instance value which is a copy of the
		original asset. */
	FNiagaraEmitterHandle AddEmitterHandle(UNiagaraEmitter& SourceEmitter, FName EmitterName, FGuid EmitterVersion);

	/** Adds a new emitter handle to this system without copying the original asset. This should only be used for temporary systems and never for live assets. */
	void AddEmitterHandleDirect(FNiagaraEmitterHandle& EmitterHandleToAdd);

	/** Duplicates an existing emitter handle and adds it to the System.  The new handle will reference the same source asset,
		but will have a copy of the duplicated Instance value. */
	FNiagaraEmitterHandle DuplicateEmitterHandle(const FNiagaraEmitterHandle& EmitterHandleToDuplicate, FName EmitterName);

	/** Removes the provided emitter handle. */
	void RemoveEmitterHandle(const FNiagaraEmitterHandle& EmitterHandleToDelete);

	/** Removes the emitter handles which have an Id in the supplied set. */
	void RemoveEmitterHandlesById(const TSet<FGuid>& HandlesToRemove);

#endif


	FNiagaraEmitterHandle& GetEmitterHandle(int Idx)
	{
		check(Idx < EmitterHandles.Num());
		return EmitterHandles[Idx];
	};

	const FNiagaraEmitterHandle& GetEmitterHandle(int Idx) const
	{
		check(Idx < EmitterHandles.Num());
		return EmitterHandles[Idx];
	};

	int GetNumEmitters()
	{
		return EmitterHandles.Num();
	}

	/** From the last compile, what are the variables that were exported out of the system for external use?*/
	const FNiagaraUserRedirectionParameterStore& GetExposedParameters() const {	return ExposedParameters; }
	FNiagaraUserRedirectionParameterStore& GetExposedParameters()  { return ExposedParameters; }

	/** Gets the System script which is used to populate the System parameters and parameter bindings. */
	UNiagaraScript* GetSystemSpawnScript();
	UNiagaraScript* GetSystemUpdateScript();
	const UNiagaraScript* GetSystemSpawnScript() const;
	const UNiagaraScript* GetSystemUpdateScript() const;

	TOptional<float> GetMaxDeltaTime() const { return MaxDeltaTime; }
	const FNiagaraDataSetAccessor<ENiagaraExecutionState>& GetSystemExecutionStateAccessor() const { return SystemExecutionStateAccessor; }
	TConstArrayView<FNiagaraDataSetAccessor<ENiagaraExecutionState>> GetEmitterExecutionStateAccessors() const { return MakeArrayView(EmitterExecutionStateAccessors); }
	TConstArrayView<FNiagaraDataSetAccessor<FNiagaraSpawnInfo>> GetEmitterSpawnInfoAccessors(int32 EmitterIndex) const { return MakeArrayView(EmitterSpawnInfoAccessors[EmitterIndex]);  }
	
	/** Performs the passed action for all scripts in this system. */
	template<typename TAction>
	void ForEachScript(TAction Func) const;

	/** Performs the passed action for all FNiagaraPlatformSets used by this system. Some may not be owned by this system. */
	template<typename TAction>
	void ForEachPlatformSet(TAction Func);

	bool AllowScalabilityForLocalPlayerFX()const;

	void PrecachePSOs();

private:
	bool IsReadyToRunInternal() const;

public:
	bool IsReadyToRun() const { return FPlatformProperties::RequiresCookedData() ? bIsReadyToRunCached : IsReadyToRunInternal(); }

	FORCEINLINE bool NeedsWarmup()const { return WarmupTickCount > 0 && WarmupTickDelta > SMALL_NUMBER; }
	FORCEINLINE float GetWarmupTime()const { return WarmupTime; }
	FORCEINLINE int32 GetWarmupTickCount()const { return WarmupTickCount; }
	FORCEINLINE float GetWarmupTickDelta()const { return WarmupTickDelta; }
	FORCEINLINE bool HasFixedTickDelta() const { return bFixedTickDelta; }
	FORCEINLINE float GetFixedTickDeltaTime()const { return FixedTickDeltaTime; }
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags)  const override;

	FORCEINLINE bool NeedsDeterminism() const { return bDeterminism; }
	FORCEINLINE int32 GetRandomSeed() const { return RandomSeed; }

	FORCEINLINE void SetWarmupTime(float InWarmupTime) { WarmupTime = InWarmupTime; ResolveWarmupTickCount(); }
	FORCEINLINE void SetWarmupTickDelta(float InWarmupTickDelta) { WarmupTickDelta = InWarmupTickDelta; ResolveWarmupTickCount(); }
	void ResolveWarmupTickCount();

#if STATS
	FNiagaraStatDatabase& GetStatData() { return StatDatabase; }
#endif

#if WITH_EDITORONLY_DATA
	/** Are there any pending compile requests?*/
	bool HasOutstandingCompilationRequests(bool bIncludingGPUShaders = false) const;

	/** Determines if this system has the supplied emitter as an editable and simulating emitter instance. */
	bool ReferencesInstanceEmitter(const FVersionedNiagaraEmitter& Emitter) const;

	/** Updates the system's rapid iteration parameters from a specific emitter. */
	void RefreshSystemParametersFromEmitter(const FNiagaraEmitterHandle& EmitterHandle);

	/** Removes the system's rapid iteration parameters for a specific emitter. */
	void RemoveSystemParametersForEmitter(const FNiagaraEmitterHandle& EmitterHandle);

	/** Request that any dirty scripts referenced by this system be compiled.*/
	bool RequestCompile(bool bForce, FNiagaraSystemUpdateContext* OptionalUpdateContext = nullptr);

	/** If we have a pending compile request, is it done with yet? */
	bool PollForCompilationComplete();

	/** Blocks until all active compile jobs have finished */
	void WaitForCompilationComplete(bool bIncludingGPUShaders = false, bool bShowProgress = true);

	/** Tries to abort all running shader compilations */
	void KillAllActiveCompilations();

	/** Invalidates any active compilation requests which will ignore their results. */
	void InvalidateActiveCompiles();

	/** Delegate called when the system's dependencies have all been compiled.*/
	FOnSystemCompiled& OnSystemCompiled();

	/** Delegate called on PostEditChange.*/
	FOnSystemPostEditChange& OnSystemPostEditChange();

	/** Delegate called on effect type or effect type value change */
	FOnScalabilityChanged& OnScalabilityChanged();

	/** Gets editor specific data stored with this system. */
	UNiagaraEditorDataBase* GetEditorData();

	/** Gets editor specific parameters stored with this system */
	UNiagaraEditorParametersAdapterBase* GetEditorParameters();

	/** Gets editor specific data stored with this system. */
	const UNiagaraEditorDataBase* GetEditorData() const;

	/** Internal: The thumbnail image.*/
	UPROPERTY()
	TObjectPtr<class UTexture2D> ThumbnailImage;

	/** Deprecated library exposure bool. Use the LibraryVisibility enum instead. FNiagaraEditorUtilities has accessor functions that takes deprecation into account */
	UPROPERTY()
	bool bExposeToLibrary_DEPRECATED;

	/** If this system is exposed to the library, or should be explicitly hidden. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Asset Options", AssetRegistrySearchable, meta = (SkipSystemResetOnChange = "true"))
	ENiagaraScriptLibraryVisibility LibraryVisibility = ENiagaraScriptLibraryVisibility::Unexposed;
	
	/** Deprecated template asset bool. Use the TemplateSpecification enum instead. */
	UPROPERTY()
	bool bIsTemplateAsset_DEPRECATED;

	/** If this system is a regular system, a template or a behavior example. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Asset Options", AssetRegistrySearchable, meta = (SkipSystemResetOnChange = "true"))
	ENiagaraScriptTemplateSpecification TemplateSpecification = ENiagaraScriptTemplateSpecification::None;

	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Asset Options", AssetRegistrySearchable, meta = (SkipSystemResetOnChange = "true"))
	FText TemplateAssetDescription;

	/** Category of this system. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Asset Options", AssetRegistrySearchable, meta = (SkipSystemResetOnChange = "true"))
	FText Category;

	UPROPERTY()
	TArray<TObjectPtr<UNiagaraScript>> ScratchPadScripts;

	UPROPERTY(transient)
	FNiagaraParameterStore EditorOnlyAddedParameters;

	bool GetIsolateEnabled() const;
	void SetIsolateEnabled(bool bIsolate);
	
	UPROPERTY(transient)
	FNiagaraSystemUpdateContext UpdateContext;
#endif

	void UpdateSystemAfterLoad();
	void EnsureFullyLoaded() const;

	bool ShouldAutoDeactivate() const { return bAutoDeactivate; }
	bool IsLooping() const;

	const TArray<TSharedRef<const FNiagaraEmitterCompiledData>>& GetEmitterCompiledData() const { return EmitterCompiledData; };

	const FNiagaraSystemCompiledData& GetSystemCompiledData() const { return SystemCompiledData; };

	bool UsesCollection(const UNiagaraParameterCollection* Collection)const;

	bool SupportsLargeWorldCoordinates() const { return bSupportLargeWorldCoordinates && bLwcEnabledSettingCached; }
	FORCEINLINE bool ShouldDisableExperimentalVM() const { return bDisableExperimentalVM; }

#if WITH_EDITORONLY_DATA
	bool UsesEmitter(UNiagaraEmitter* Emitter) const;
	bool UsesEmitter(const FVersionedNiagaraEmitter& VersionedEmitter) const;
	bool UsesScript(const UNiagaraScript* Script)const; 
	void ForceGraphToRecompileOnNextCheck();

	static void RequestCompileForEmitter(const FVersionedNiagaraEmitter& InEmitter);
	static void RecomputeExecutionOrderForEmitter(const FVersionedNiagaraEmitter& InEmitter);
	static void RecomputeExecutionOrderForDataInterface(class UNiagaraDataInterface* DataInterface);

	FORCEINLINE bool ShouldUseRapidIterationParameters() const { return bCompileForEdit ? !bBakeOutRapidIteration : !bBakeOutRapidIterationOnCook; }
	FORCEINLINE bool ShouldTrimAttributes() const { return bCompileForEdit ? bTrimAttributes : bTrimAttributesOnCook; }
	FORCEINLINE bool ShouldIgnoreParticleReadsForAttributeTrim() const { return bIgnoreParticleReadsForAttributeTrim; }
	FORCEINLINE bool ShouldDisableDebugSwitches() const { return bCompileForEdit ? bDisableDebugSwitches : bDisableDebugSwitchesOnCook; }
	FORCEINLINE bool ShouldCompressAttributes() const { return bCompressAttributes; }

	FORCEINLINE void SetBakeOutRapidIterationOnCook(bool bBakeOut) { bBakeOutRapidIteration = bBakeOut; bBakeOutRapidIterationOnCook = bBakeOut; }
	FORCEINLINE void SetTrimAttributesOnCook(bool bTrim) { bTrimAttributes = bTrim; bTrimAttributesOnCook = bTrim; }

	/** When enabled we compile for the edit path, which can result in slower system performance but faster editor responsiveness. */
	UPROPERTY(transient)
	uint32 bCompileForEdit : 1;

protected:
	/** When enable constant values are baked into the scripts while editing the system, this will increase iteration times but improve performance. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Performance", meta=(DisplayName="Bake Rapid Iteration Parameters During Edit"))
	uint32 bBakeOutRapidIteration : 1;

	/** When enabled constant values are baked into scripts to improve performance. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Performance", meta=(DisplayName="Bake Rapid Iteration Parameters"))
	uint32 bBakeOutRapidIterationOnCook : 1;

	/** Toggles whether or not emitters within this system will try and compress their particle attributes.
	In some cases, this precision change can lead to perceivable differences, but memory costs and or performance (especially true for GPU emitters) can improve. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Performance")
	uint32 bCompressAttributes : 1;

	/** When enabled we trim particle attributes while editing the system. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Performance", meta=(DisplayName="Trim Attributes During Edit"))
	uint32 bTrimAttributes : 1;
	/** If true Particle attributes will be removed from the DataSet if they are unnecessary (are never read by ParameterMap) */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Performance", meta = (DisplayName="Trim Attributes"))
	uint32 bTrimAttributesOnCook : 1;

	/** If true ParticleReads will not absolutely prevent attribute trimming - User must ensure that the appropriate attributes are preserved on the source emitter! */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Performance")
	uint32 bIgnoreParticleReadsForAttributeTrim : 1;

	/** When enable debug switches are disabled while editing the system. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Performance", meta = (DisplayName="Disable Debug Switches During Edit"))
	uint32 bDisableDebugSwitches : 1;
	/** When enabled debug switches are disabled when compiling the system. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Performance", meta = (DisplayName="Disable Debug Switches"))
	uint32 bDisableDebugSwitchesOnCook : 1;

public:
	/** Subscriptions to definitions of parameters. */
	UPROPERTY()
	TArray<FParameterDefinitionsSubscription> ParameterDefinitionsSubscriptions;
#endif

	/** If true then position type values will be rebased on system activation to fit into a float precision vector. This needs to be turned off when using a custom data interface or renderer that does not support the rebasing. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Rendering")
	uint8 bSupportLargeWorldCoordinates : 1;

	//////////////////////////////////////////////////////////////////////////
	// Various optional overrides for component properties when spawning a system
	UPROPERTY(EditAnywhere, Category="Rendering", meta=(InlineEditConditionToggle="bCastShadow"))
	uint8 bOverrideCastShadow : 1;

	UPROPERTY(EditAnywhere, Category="Rendering", meta=(InlineEditConditionToggle="bReceivesDecals"))
	uint8 bOverrideReceivesDecals : 1;

	UPROPERTY(EditAnywhere, Category="Rendering", meta=(InlineEditConditionToggle="bRenderCustomDepth"))
	uint8 bOverrideRenderCustomDepth : 1;

	UPROPERTY(EditAnywhere, Category="Rendering", meta=(InlineEditConditionToggle="CustomDepthStencilValue"))
	uint8 bOverrideCustomDepthStencilValue : 1;

	UPROPERTY(EditAnywhere, Category="Rendering", meta=(InlineEditConditionToggle="CustomDepthStencilWriteMask"))
	uint8 bOverrideCustomDepthStencilWriteMask : 1;

	UPROPERTY(EditAnywhere, Category="Rendering", meta=(InlineEditConditionToggle="TranslucencySortPriority"))
	uint8 bOverrideTranslucencySortPriority : 1;

	UPROPERTY(EditAnywhere, Category="Rendering", meta=(InlineEditConditionToggle="TranslucencySortDistanceOffset"))
	uint8 bOverrideTranslucencySortDistanceOffset : 1;

	/**
	When enabled this is the default value set on the component.
	Controls whether the primitive component should cast a shadow or not.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rendering", meta=(DisplayName="Default Cast Shadows", EditCondition="bOverrideCastShadow"))
	uint8 bCastShadow : 1;

	/**
	When enabled this is the default value set on the component.
	Whether the primitive receives decals.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rendering", meta=(DisplayName="Default Receives Decals", EditCondition="bOverrideReceivesDecals"))
	uint8 bReceivesDecals : 1;

	/**
	When enabled this is the default value set on the component.
	This primitive has bRenderCustomDepth enabled.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rendering", meta=(DisplayName="Default Render CustomDepth Pass", EditCondition="bOverrideRenderCustomDepth"))
	uint8 bRenderCustomDepth : 1;

	/** If true, disables experimental VM, if available */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Performance", meta = (DisplayName = "Disable Experimental VM"))
	uint8 bDisableExperimentalVM : 1;

	/**
	When enabled this is the default value set on the component.
	Mask used for stencil buffer writes.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category="Rendering", meta=(DisplayName="Default CustomDepthStencil Write Mask", editcondition="bOverrideCustomDepthStencilValue"))
	ERendererStencilMask CustomDepthStencilWriteMask = ERendererStencilMask::ERSM_Default;

	/**
	When enabled this is the default value set on the component.
	Optionally write this 0-255 value to the stencil buffer in CustomDepth pass (Requires project setting or r.CustomDepth == 3)
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category="Rendering", meta=(DisplayName="Default CustomDepthStencil Value", editcondition="bOverrideCustomDepthStencilWriteMask", UIMin = "0", UIMax = "255"))
	int32 CustomDepthStencilValue = 0;

	/**
	When enabled this is the default value set on the component.
	Adjusts the translucent object sorting priority, see PrimitiveComponent description for more details.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category="Rendering", meta=(editcondition="bOverrideTranslucencySortPriority"))
	int32 TranslucencySortPriority = 0;

	/**
	When enabled this is the default value set on the component.
	Modifies the sort distance for translucent objects, see PrimitiveComponent description for more details.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category="Rendering", meta=(editcondition="bOverrideTranslucencySortDistanceOffset"))
	float TranslucencySortDistanceOffset = 0.0f;

	/** Computes emitter priorities based on the dependency information. */
	bool ComputeEmitterPriority(int32 EmitterIdx, TArray<int32, TInlineAllocator<32>>& EmitterPriorities, const TBitArray<TInlineAllocator<32>>& EmitterDependencyGraph);

	/** Queries all the data interfaces in the array for emitter dependencies. */
	void FindDataInterfaceDependencies(FVersionedNiagaraEmitterData* EmitterData, UNiagaraScript* Script, TArray<FVersionedNiagaraEmitter>& Dependencies);

	/** Looks at all the event handlers in the emitter to determine which other emitters it depends on. */
	void FindEventDependencies(FVersionedNiagaraEmitterData* EmitterData, TArray<FVersionedNiagaraEmitter>& Dependencies);

	/** Computes the order in which the emitters in the Emitters array will be ticked and stores the results in EmitterExecutionOrder. */
	void ComputeEmittersExecutionOrder();

	/** Computes the order in which renderers will render */
	void ComputeRenderersDrawOrder();

	/** Cache data & accessors from the compiled data, allows us to avoid per instance. */
	void CacheFromCompiledData();

	FORCEINLINE TConstArrayView<FNiagaraEmitterExecutionIndex> GetEmitterExecutionOrder() const { return MakeArrayView(EmitterExecutionOrder); }
	FORCEINLINE TConstArrayView<FNiagaraRendererExecutionIndex> GetRendererPostTickOrder() const { return MakeArrayView(RendererPostTickOrder); }
	FORCEINLINE TConstArrayView<FNiagaraRendererExecutionIndex> GetRendererCompletionOrder() const { return MakeArrayView(RendererCompletionOrder); }

	FORCEINLINE TConstArrayView<int32> GetRendererDrawOrder() const { return MakeArrayView(RendererDrawOrder); }

	/** When an index inside the EmitterExecutionOrder array has this bit set, it means the corresponding emitter cannot execute in parallel with the previous emitters due to a data dependency. */
	static constexpr int32 kStartNewOverlapGroupBit = (1 << 31);

	FORCEINLINE UNiagaraParameterCollectionInstance* GetParameterCollectionOverride(UNiagaraParameterCollection* Collection)
	{
		auto* Found = ParameterCollectionOverrides.FindByPredicate(
			[&](const UNiagaraParameterCollectionInstance* CheckInst)
		{
			return CheckInst && Collection == CheckInst->Collection;
		});

		return Found ? *Found : nullptr;
	}
	
	UPROPERTY(EditAnywhere, Category = "Debug", Transient)
	bool bDumpDebugSystemInfo = false;

	UPROPERTY(EditAnywhere, Category = "Debug", Transient)
	bool bDumpDebugEmitterInfo = false;

	bool bFullyLoaded = false;

	/** When enabled, we follow the settings on the UNiagaraComponent for tick order. When this option is disabled, we ignore any dependencies from data interfaces or other variables and instead fire off the simulation as early in the frame as possible. This greatly
	reduces overhead and allows the game thread to run faster, but comes at a tradeoff if the dependencies might leave gaps or other visual artifacts.*/
	UPROPERTY(EditAnywhere, Category = "Performance")
	bool bRequireCurrentFrameData = true;

	bool HasSystemScriptDIsWithPerInstanceData() const;
	FORCEINLINE bool HasDIsWithPostSimulateTick() const { return bHasDIsWithPostSimulateTick; }
	FORCEINLINE bool AllDIsPostSimulateCanOverlapFrames() const { return bAllDIsPostSimulateCanOverlapFrames; }
	FORCEINLINE bool HasAnyGPUEmitters()const{ return bHasAnyGPUEmitters; }
	FORCEINLINE bool NeedsGPUContextInitForDataInterfaces() const { return bNeedsGPUContextInitForDataInterfaces; }

	const TArray<FName>& GetUserDINamesReadInSystemScripts() const;

	FBox GetFixedBounds() const;
	FORCEINLINE void SetFixedBounds(const FBox& Box) { FixedBounds = Box;  }

#if WITH_EDITOR
	void SetEffectType(UNiagaraEffectType* EffectType);

	FNiagaraSystemScalabilityOverrides& GetSystemScalabilityOverrides() { return SystemScalabilityOverrides; }
	FORCEINLINE bool GetOverrideScalabilitySettings()const { return bOverrideScalabilitySettings; }
	FORCEINLINE void SetOverrideScalabilitySettings(bool bOverride) { bOverrideScalabilitySettings = bOverride; }


	void GatherStaticVariables(TArray<FNiagaraVariable>& OutVars, TArray<FNiagaraVariable>& OutEmitterVars) const;
#endif
	UNiagaraEffectType* GetEffectType()const;
	FORCEINLINE const FNiagaraSystemScalabilitySettings& GetScalabilitySettings()const { return CurrentScalabilitySettings; }
	const FNiagaraSystemScalabilityOverride& GetCurrentOverrideSettings() const;
	FORCEINLINE bool NeedsSortedSignificanceCull()const{ return bNeedsSortedSignificanceCull; }
	
	void UpdateScalability();

	FORCEINLINE ENiagaraCullProxyMode GetCullProxyMode()const { return GetScalabilitySettings().CullProxyMode; }

	/** Whether or not fixed bounds are enabled. */
	UPROPERTY(EditAnywhere, Category = "System", meta = (SkipSystemResetOnChange = "true", InlineEditConditionToggle))
	uint32 bFixedBounds : 1;

	TStatId GetStatID(bool bGameThread, bool bConcurrent)const;
	void AddToInstanceCountStat(int32 NumInstances, bool bSolo)const;

	const FString& GetCrashReporterTag()const;
	bool CanObtainEmitterAttribute(const FNiagaraVariableBase& InVarWithUniqueNameNamespace, FNiagaraTypeDefinition& OutBoundType) const;
	bool CanObtainSystemAttribute(const FNiagaraVariableBase& InVar, FNiagaraTypeDefinition& OutBoundType) const;
	bool CanObtainUserVariable(const FNiagaraVariableBase& InVar) const;

#if WITH_EDITORONLY_DATA
	const TMap<FGuid, TObjectPtr<UNiagaraMessageDataBase>>& GetMessages() const { return MessageKeyToMessageMap; };
	void AddMessage(const FGuid& MessageKey, UNiagaraMessageDataBase* NewMessage) { MessageKeyToMessageMap.Add(MessageKey, NewMessage); };
	void RemoveMessage(const FGuid& MessageKey) { MessageKeyToMessageMap.Remove(MessageKey); };
	void RemoveMessageDelegateable(const FGuid MessageKey) { MessageKeyToMessageMap.Remove(MessageKey); };
	const FGuid& GetAssetGuid() const {return AssetGuid;};
#endif

	FORCEINLINE void RegisterActiveInstance();
	FORCEINLINE void UnregisterActiveInstance();
	FORCEINLINE int32& GetActiveInstancesCount() { return ActiveInstances; }

#if WITH_EDITORONLY_DATA
	UNiagaraBakerSettings* GetBakerSettings();
	const UNiagaraBakerSettings* GetBakerGeneratedSettings() const { return BakerGeneratedSettings; }
	void SetBakerGeneratedSettings(UNiagaraBakerSettings* Settings) { BakerGeneratedSettings = Settings; }


	/** Get the cached parameter map traversal for this emitter.  */
	const TSharedPtr<FNiagaraGraphCachedDataBase, ESPMode::ThreadSafe>& GetCachedTraversalData() const;
	void InvalidateCachedData();
	void GraphSourceChanged();

	/** Resets internal data leaving it in a state which would have minimal cost to exist in headless builds (servers) */
	void ResetToEmptySystem();
#endif

private:
#if WITH_EDITORONLY_DATA


	/** Since the shader compilation is done in another process, this is used to check if the result for any ongoing compilations is done.
	*   If bWait is true then this *blocks* the game thread (and ui) until all shader compilations are finished.
	*/
	bool QueryCompileComplete(bool bWait, bool bDoPost, bool bDoNotApply = false);

	void BroadcastOnSystemCompiled();

	void PreProcessWaitingDDCTasks(bool bProcessForWait);

	bool CompilationResultsValid(FNiagaraSystemCompileRequest& CompileRequest) const;

	void EvaluateCompileResultDependencies() const;

	void InitEmitterCompiledData();

	void InitSystemCompiledData();

	/** Helper for filling in precomputed variable names per emitter. Converts an emitter paramter "Emitter.XXXX" into it's real parameter name. */
	void InitEmitterVariableAliasNames(FNiagaraEmitterCompiledData& EmitterCompiledDataToInit, const UNiagaraEmitter* InAssociatedEmitter);

	/** Helper for generating aliased FNiagaraVariable names for the Emitter they are associated with. */
	const FName GetEmitterVariableAliasName(const FNiagaraVariable& InEmitterVar, const UNiagaraEmitter* InEmitter) const;

	/** Helper for filling in attribute datasets per emitter. */
	void InitEmitterDataSetCompiledData(FNiagaraDataSetCompiledData& DataSetToInit, const FNiagaraEmitterHandle& InAssociatedEmitterHandle);
	void PrepareRapidIterationParametersForCompilation();
#endif

	void ResolveScalabilitySettings();
	void UpdatePostCompileDIInfo();
	void UpdateDITickFlags();
	void UpdateHasGPUEmitters();

protected:
	UPROPERTY(EditAnywhere, Category = "System")
	TObjectPtr<UNiagaraEffectType> EffectType;

	UPROPERTY(EditAnywhere, Category = "Scalability", meta=(DisplayInScalabilityContext))
	bool bOverrideScalabilitySettings;

	/** Controls whether we should override the Effect Type value for bAllowCullingForLocalPlayers. */
	UPROPERTY(EditAnywhere, Category = "Scalability", meta = (InlineEditConditionToggle, EditCondition = bOverrideScalabilitySettings))
	uint32 bOverrideAllowCullingForLocalPlayers : 1;
	
	/** The override value for bAllowCullingForLocalPlayers from the Effect Type. */
	UPROPERTY(EditAnywhere, Category = "Scalability", meta = (EditCondition = bOverrideAllowCullingForLocalPlayers))
	uint32 bAllowCullingForLocalPlayersOverride : 1;

	UPROPERTY()
	TArray<FNiagaraSystemScalabilityOverride> ScalabilityOverrides_DEPRECATED;

	UPROPERTY(EditAnywhere, Category = "Scalability", meta = (EditCondition="bOverrideScalabilitySettings", DisplayInScalabilityContext))
	FNiagaraSystemScalabilityOverrides SystemScalabilityOverrides;

	/** Handles to the emitter this System will simulate. */
	UPROPERTY()
	TArray<FNiagaraEmitterHandle> EmitterHandles;

	UPROPERTY(EditAnywhere, Category="System")
	TArray<TObjectPtr<UNiagaraParameterCollectionInstance>> ParameterCollectionOverrides;

#if WITH_EDITORONLY_DATA
	bool bCompilationReentrantGuard = false;

	UPROPERTY(Transient)
	TArray<FNiagaraSystemCompileRequest> ActiveCompilations;

	mutable TSharedPtr<FNiagaraGraphCachedDataBase, ESPMode::ThreadSafe> CachedTraversalData;
#endif

	/** The script which defines the System parameters, and which generates the bindings from System
		parameter to emitter parameter. */
	UPROPERTY()
	TObjectPtr<UNiagaraScript> SystemSpawnScript;

	/** The script which defines the System parameters, and which generates the bindings from System
	parameter to emitter parameter. */
	UPROPERTY()
	TObjectPtr<UNiagaraScript> SystemUpdateScript;

	//** Post compile generated data used for initializing Emitter Instances during runtime. */
	TArray<TSharedRef<const FNiagaraEmitterCompiledData>> EmitterCompiledData;

	//** Post compile generated data used for initializing System Instances during runtime. */
	UPROPERTY()
	FNiagaraSystemCompiledData SystemCompiledData;

	/** Variables exposed to the outside work for tweaking*/
	UPROPERTY()
	FNiagaraUserRedirectionParameterStore ExposedParameters;

#if WITH_EDITORONLY_DATA
	/** Data used by the editor to maintain UI state etc.. */
	UPROPERTY()
	TObjectPtr<UNiagaraEditorDataBase> EditorData;

	/** Wrapper for editor only parameters. */
	UPROPERTY()
	TObjectPtr<UNiagaraEditorParametersAdapterBase> EditorParameters;

	bool bIsolateEnabled;

	/** A multicast delegate which is called whenever the script has been compiled (successfully or not). */
	FOnSystemCompiled OnSystemCompiledDelegate;

	/** A multicast delegate which is called whenever this system's properties are changed. */
	FOnSystemPostEditChange OnSystemPostEditChangeDelegate;

	/** A multicast delegate that is called whenever the effect type or the effect type values are changed */
	FOnScalabilityChanged OnScalabilityChangedDelegate;
#endif

	/** The fixed bounding box value. bFixedBounds is the condition whether the fixed bounds can be edited. */
	UPROPERTY(EditAnywhere, Category = "System", meta = (SkipSystemResetOnChange = "true", EditCondition = "bFixedBounds"))
	FBox FixedBounds;

	UPROPERTY(EditAnywhere, Category = Performance, meta = (ToolTip = "Auto-deactivate system if all emitters are determined to not spawn particles again, regardless of lifetime."))
	bool bAutoDeactivate;

	/**
	When disabled we will generate a RandomSeed per instance on reset which is not deterministic.
	When enabled we will always use the RandomSeed from the system plus the components RandomSeedOffset, this allows for determinism but variance between components.
	*/
	UPROPERTY(EditAnywhere, Category = "Random")
	bool bDeterminism = false;

	/** Seed used for system script random number generator. */
	UPROPERTY(EditAnywhere, Category = "Random", meta = (EditCondition = "bDeterminism"))
	int32 RandomSeed = 0;

	/** Warm up time in seconds. Used to calculate WarmupTickCount. Rounds down to the nearest multiple of WarmupTickDelta. */
	UPROPERTY(EditAnywhere, Category = Warmup, meta = (ForceUnits=s))
	float WarmupTime;

	/** Number of ticks to process for warmup. You can set by this or by time via WarmupTime. */
	UPROPERTY(EditAnywhere, Category = Warmup)
	int32 WarmupTickCount;

	/** Delta time to use for warmup ticks. */
	UPROPERTY(EditAnywhere, Category = Warmup, meta = (ForceUnits=s))
	float WarmupTickDelta;

	UPROPERTY(EditAnywhere, Category = "System", meta = (InlineEditConditionToggle))
	bool bFixedTickDelta = false;

	/**
	If activated, the system ticks with a fixed delta time instead of the varying game thread delta time. This leads to much more stable simulations.
	When the fixed tick delta is smaller than the game thread tick time the simulation is substepping by executing multiple ticks per frame.
	Note that activating this feature forces the system to tick on the game thread instead of an async task in parallel.

	The max number of substeps per frame can be set via fx.Niagara.SystemSimulation.MaxTickSubsteps
	*/
	UPROPERTY(EditAnywhere, Category = "System", meta = (EditCondition = "bFixedTickDelta", ForceUnits=s))
	float FixedTickDeltaTime = 0.01667;

#if WITH_EDITORONLY_DATA
	/** Settings used inside the baker */
	UPROPERTY(Export)
	TObjectPtr<UNiagaraBakerSettings> BakerSettings;

	/** Generated data baker settings, will be null until we have generated at least once. */
	UPROPERTY(Export)
	TObjectPtr<UNiagaraBakerSettings> BakerGeneratedSettings;
#endif

	UPROPERTY()
	bool bHasSystemScriptDIsWithPerInstanceData;

	UPROPERTY()
	bool bNeedsGPUContextInitForDataInterfaces;


	UPROPERTY()
	TArray<FName> UserDINamesReadInSystemScripts;

	/** Array of emitter indices sorted by execution priority. The emitters will be ticked in this order. Please note that some indices may have the top bit set (kStartNewOverlapGroupBit)
	* to indicate synchronization points in parallel execution, so mask it out before using the values as indices in the emitters array.
	*/
	TArray<FNiagaraEmitterExecutionIndex> EmitterExecutionOrder;

	/** Array of renderer indices to notify system PostTick, in order of execution */
	TArray<FNiagaraRendererExecutionIndex> RendererPostTickOrder;
	/** Array of renderer indices to notify system Completion, in order of execution */
	TArray<FNiagaraRendererExecutionIndex> RendererCompletionOrder;

	/** Precomputed emitter renderer draw order, since emitters & renderers are not dynamic we can do this. */
	TArray<int32> RendererDrawOrder;

	uint32 bIsValidCached : 1;
	uint32 bIsReadyToRunCached : 1;
	uint32 bNeedsAsyncOptimize : 1;
	uint32 bLwcEnabledSettingCached : 1;

	TOptional<float> MaxDeltaTime;
	FNiagaraDataSetAccessor<ENiagaraExecutionState> SystemExecutionStateAccessor;
	TArray<FNiagaraDataSetAccessor<ENiagaraExecutionState>> EmitterExecutionStateAccessors;
	TArray<TArray<FNiagaraDataSetAccessor<FNiagaraSpawnInfo>>> EmitterSpawnInfoAccessors;

	FGraphEventRef ScriptOptimizationCompletionEvent;

public:
	void AsyncOptimizeAllScripts();

	FGraphEventRef GetScriptOptimizationCompletionEvent()
	{
		if (ScriptOptimizationCompletionEvent.IsValid())
		{
			if ( !ScriptOptimizationCompletionEvent->IsComplete() )
			{
				return ScriptOptimizationCompletionEvent;
			}
			ScriptOptimizationCompletionEvent = nullptr;
		}
		return nullptr;
	}

	bool AllowCullingForLocalPlayers() const { return bAllowCullingForLocalPlayers; }

	const FNiagaraSystemStaticBuffers* GetStaticBuffers() const { return StaticBuffers.Get(); }

protected:
	void GenerateStatID()const;
#if STATS
	mutable TStatId StatID_GT;
	mutable TStatId StatID_GT_CNC;
	mutable TStatId StatID_RT;
	mutable TStatId StatID_RT_CNC;

	mutable TStatId StatID_InstanceCount;
	mutable TStatId StatID_InstanceCountSolo;
	FNiagaraStatDatabase StatDatabase;
#endif

	//Scalability settings
	FNiagaraSystemScalabilitySettings CurrentScalabilitySettings;
	bool bAllowCullingForLocalPlayers = false;

	mutable FString CrashReporterTag;

	uint32 bHasDIsWithPostSimulateTick : 1;
	uint32 bAllDIsPostSimulateCanOverlapFrames : 1;
	uint32 bHasAnyGPUEmitters : 1;
	uint32 bNeedsSortedSignificanceCull : 1;

#if WITH_EDITORONLY_DATA
	/** Messages associated with the System asset. */
	UPROPERTY()
	TMap<FGuid, TObjectPtr<UNiagaraMessageDataBase>> MessageKeyToMessageMap;

	FGuid AssetGuid;
#endif

	/** Total active instances of this system. */
	int32 ActiveInstances;

	TUniquePtr<FNiagaraSystemStaticBuffers, FNiagaraSystemStaticBuffers::FDeletor> StaticBuffers;
};

FORCEINLINE void UNiagaraSystem::RegisterActiveInstance()
{
	++ActiveInstances;
}

FORCEINLINE void UNiagaraSystem::UnregisterActiveInstance()
{
	--ActiveInstances;
}

template<typename TAction>
void UNiagaraSystem::ForEachScript(TAction Func) const
{
	Func(SystemSpawnScript);
	Func(SystemUpdateScript);

	for (const FNiagaraEmitterHandle& Handle : EmitterHandles)
	{
		if (FVersionedNiagaraEmitterData* EmitterData = Handle.GetEmitterData())
		{
			EmitterData->ForEachScript(Func);
		}
	}
}

/** Performs the passed action for all FNiagaraPlatformSets in this system. */
template<typename TAction>
void UNiagaraSystem::ForEachPlatformSet(TAction Func)
{
	//Handle our scalability overrides
	for (FNiagaraSystemScalabilityOverride& Override : SystemScalabilityOverrides.Overrides)
	{
		Func(Override.Platforms);
	}

	//Handle and platform set User DIs.
	for (UNiagaraDataInterface* DI : GetExposedParameters().GetDataInterfaces())
	{
		if (UNiagaraDataInterfacePlatformSet* PlatformSetDI = Cast<UNiagaraDataInterfacePlatformSet>(DI))
		{
			Func(PlatformSetDI->Platforms);
		}
	}

	//Handle all platform set DIs held in scripts for this system.
	auto HandleScript = [Func](UNiagaraScript* NiagaraScript)
	{
		if (NiagaraScript)
		{
			for (const FNiagaraScriptDataInterfaceInfo& DataInterfaceInfo : NiagaraScript->GetCachedDefaultDataInterfaces())
			{
				if (UNiagaraDataInterfacePlatformSet* PlatformSetDI = Cast<UNiagaraDataInterfacePlatformSet>(DataInterfaceInfo.DataInterface))
				{
					Func(PlatformSetDI->Platforms);
				}
			}
		}
	};
	HandleScript(SystemSpawnScript);
	HandleScript(SystemUpdateScript);

	//Finally handle all our emitters.
	for (FNiagaraEmitterHandle& Handle : EmitterHandles)
	{
		if (FVersionedNiagaraEmitterData* Emitter = Handle.GetEmitterData())
		{
			Emitter->ForEachPlatformSet(Func);
		}
	}
}