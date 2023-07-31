// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/ThreadSafeBool.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "NiagaraCommon.h"
#include "NiagaraScriptBase.h"
#include "NiagaraShared.h"
#include "NiagaraShader.h"
#include "NiagaraParameters.h"
#include "NiagaraDataSet.h"
#include "NiagaraScriptExecutionParameterStore.h"
#include "NiagaraStackSection.h"
#include "NiagaraParameterDefinitionsSubscriber.h"
#include "NiagaraValidationRule.h"
#include "NiagaraVersionedObject.h"
#include "HAL/CriticalSection.h"
#include "VectorVM.h"

#include "NiagaraScript.generated.h"

class UNiagaraDataInterface;
class FNiagaraCompileRequestDataBase;
class FNiagaraCompileRequestDuplicateDataBase;
class UNiagaraConvertInPlaceUtilityBase;

#define NIAGARA_INVALID_MEMORY (0xBA)

#define NIAGARA_SCRIPT_COMPILE_LOGGING_MEDIUM

DECLARE_STATS_GROUP(TEXT("Niagara Detailed"), STATGROUP_NiagaraDetailed, STATCAT_Advanced);

/** Defines what will happen to unused attributes when a script is run. */
UENUM()
enum class EUnusedAttributeBehaviour : uint8
{
	/** The previous value of the attribute is copied across. */
	Copy,
	/** The attribute is set to zero. */
	Zero,
	/** The attribute is untouched. */
	None,
	/** The memory for the attribute is set to NIAGARA_INVALID_MEMORY. */
	MarkInvalid, 
	/** The attribute is passed through without double buffering */
	PassThrough,
};

UENUM()
enum class ENiagaraModuleDependencyType : uint8
{
	/** The dependency belongs before the module. */
	PreDependency,
	/** The dependency belongs after the module. */
	PostDependency
};

UENUM()
enum class ENiagaraModuleDependencyScriptConstraint : uint8
{
	/** The module providing the dependency must be in the same script e.g. if the module requiring the dependency is in "Particle Spawn" the module providing the dependency must also be in "Particle Spawn". */
	SameScript,
	/** The module providing the dependency can be in any script as long as it satisfies the dependency type, e.g. if the module requiring the dependency is in "Particle Spawn" the module providing the dependency could be in "Emitter Spawn". */
	AllScripts
};

UENUM()
enum class ENiagaraScriptLibraryVisibility : uint8
{
	Invalid = 0 UMETA(Hidden),
	
	/** The script is not visible by default to the user, but can be made visible by disabling the "Library only" filter option. */
	Unexposed UMETA(DisplayName = "Unexposed"),

	/** The script is exposed to the asset library and always visible to the user. */
	Library UMETA(DisplayName = "Exposed"),

	/** The script is never visible to the user. This is useful to "soft deprecate" assets that should not be shown to a user, but should also not generate errors for existing usages. */
	Hidden UMETA(DisplayName = "Hidden")
};

UENUM()
enum class ENiagaraScriptTemplateSpecification : uint8
{
	None,
	Template,
	Behavior UMETA(DisplayName = "Behavior Example")
};

/** Defines different usages for a niagara script's module dependecies. */
UENUM()
enum class ENiagaraModuleDependencyUsage : uint8
{
	/** Default entry to catch invalid usages */
	None UMETA(Hidden),
	/** Evaluate when the script is called during the spawn phase. */
	Spawn,
	/** Evaluate when the script is called during the update phase. */
	Update,
	/** Evaluate when the script is called in an event context. */
	Event,
	/** Evaluate when the script is called in a simulation stage. */
	SimulationStage
};

USTRUCT()
struct FNiagaraModuleDependency
{
	GENERATED_USTRUCT_BODY()
public:
	/** Specifies the provided id of the required dependent module (e.g. 'ProvidesNormalizedAge') */
	UPROPERTY(AssetRegistrySearchable, EditAnywhere, Category = Script)
	FName Id;

	/** Whether the dependency belongs before or after this module */
	UPROPERTY(AssetRegistrySearchable, EditAnywhere, Category = Script)
	ENiagaraModuleDependencyType Type; // e.g. PreDependency

	/** Specifies constraints related to the source script a modules provides as dependency. */
	UPROPERTY(AssetRegistrySearchable, EditAnywhere, Category = Script)
	ENiagaraModuleDependencyScriptConstraint ScriptConstraint;

	// Specifies the version constraint that module providing the dependency must fulfill.
	// Example usages:
	// '1.2' requires the exact version 1.2 of the source script
	// '1.2+' requires at least version 1.2, but any higher version is also ok
	// '1.2-2.0' requires any version between 1.2 and 2.0
	UPROPERTY(AssetRegistrySearchable, EditAnywhere, Category = Script)
	FString RequiredVersion;

	/** This property can limit where the dependency is evaluated. By default, the dependency is enforced in all script usages */
	UPROPERTY(AssetRegistrySearchable, EditAnywhere, Category = Script, meta = (Bitmask, BitmaskEnum = "/Script/Niagara.ENiagaraModuleDependencyUsage"))
	int32 OnlyEvaluateInScriptUsage;
	
	/** Detailed description of the dependency */
	UPROPERTY(AssetRegistrySearchable, EditAnywhere, Category = Script, meta = (MultiLine = true))
	FText Description;

	FNiagaraModuleDependency()
	{
		Type = ENiagaraModuleDependencyType::PreDependency;
		ScriptConstraint = ENiagaraModuleDependencyScriptConstraint::SameScript;
		OnlyEvaluateInScriptUsage = 1 << static_cast<int32>(ENiagaraModuleDependencyUsage::Spawn) |
									1 << static_cast<int32>(ENiagaraModuleDependencyUsage::Update) |
									1 << static_cast<int32>(ENiagaraModuleDependencyUsage::Event) |
									1 << static_cast<int32>(ENiagaraModuleDependencyUsage::SimulationStage);
	}

#if WITH_EDITOR
	bool HasValidVersionDependency() const;
	NIAGARA_API bool IsVersionAllowed(const FNiagaraAssetVersion& Version) const;
	
	void CheckVersionCache() const;
#endif
private:
	
	struct FResolvedVersions
	{
		bool bValid = false;
		FString SourceProperty;
		int32 MinMajorVersion = 0;
		int32 MinMinorVersion = 0;
		int32 MaxMajorVersion = 0;
		int32 MaxMinorVersion = 0;
	};
	mutable FResolvedVersions VersionDependencyCache;
};

USTRUCT()
struct NIAGARA_API FNiagaraCompilerTag
{
	GENERATED_USTRUCT_BODY()
public:
	FNiagaraCompilerTag() {}
	FNiagaraCompilerTag(const FNiagaraVariable& InVariable, const FString& InStringValue) : Variable(InVariable), StringValue(InStringValue) {}
	UPROPERTY()
	FNiagaraVariable Variable;

	UPROPERTY()
	FString StringValue;

	static FNiagaraCompilerTag* FindTag(TArray< FNiagaraCompilerTag>& InTags,  const FNiagaraVariableBase& InSearchVar);
	static const FNiagaraCompilerTag* FindTag(const TArray< FNiagaraCompilerTag>& InTags, const FNiagaraVariableBase& InSearchVar);

};


struct FNiagaraScriptDebuggerInfo
{
	FNiagaraScriptDebuggerInfo();
	FNiagaraScriptDebuggerInfo(FName InName, ENiagaraScriptUsage InUsage, const FGuid& InUsageId);

	bool bWaitForGPU;

	FName HandleName;

	ENiagaraScriptUsage Usage;

	FGuid UsageId;

	int32 FrameLastWriteId;

	FNiagaraDataSet Frame;

	FNiagaraParameterStore Parameters;

	TAtomic<bool> bWritten;
};

/** Struct containing all of the data necessary to look up a NiagaraScript's VM executable results from the Derived Data Cache.*/
USTRUCT()
struct NIAGARA_API FNiagaraVMExecutableDataId
{
	GENERATED_USTRUCT_BODY()
public:
	/** The version of the compiler that this needs to be built against.*/
	UPROPERTY()
	FGuid CompilerVersionID;

	/** The type of script this was used for.*/
	UPROPERTY()
	ENiagaraScriptUsage ScriptUsageType;

	/** The instance id of this script usage type.*/
	UPROPERTY()
	FGuid ScriptUsageTypeID;

#if WITH_EDITORONLY_DATA
	/** Configuration options*/
	UPROPERTY()
	TArray<FString> AdditionalDefines;

	UPROPERTY()
	TArray<FNiagaraVariableBase> AdditionalVariables;

	TArray<FString> GetAdditionalVariableStrings();
#endif

	/** Whether or not we need to bake Rapid Iteration params. True to keep params, false to bake.*/
	UPROPERTY()
	uint32 bUsesRapidIterationParams : 1;

	/** Whether or not we allow debug switches to be used. */
	UPROPERTY()
	uint32 bDisableDebugSwitches : 1;

	/** Do we require interpolated spawning */
	UPROPERTY()
	uint32 bInterpolatedSpawn : 1;

	/** Do we require persistent IDs */
	UPROPERTY()
	uint32 bRequiresPersistentIDs : 1;

	/**
	* The GUID of the subgraph this shader primarily represents.
	*/
	UPROPERTY()
	FGuid BaseScriptID_DEPRECATED;

	/**
	* The hash of the subgraph this shader primarily represents.
	*/
	UPROPERTY()
	FNiagaraCompileHash BaseScriptCompileHash;

#if WITH_EDITORONLY_DATA
	/** Compile hashes of any top level scripts the script was dependent on that might trigger a recompile if they change. */
	UPROPERTY()
	TArray<FNiagaraCompileHash> ReferencedCompileHashes;

	/** Temp storage while generating the Id. This is NOT serialized and shouldn't be used in any comparisons*/
	TArray<FString> DebugReferencedObjects;
#endif

	/** The version of the script that was compiled. If empty then just the latest version. */
	UPROPERTY()
	FGuid ScriptVersionID;

	FNiagaraVMExecutableDataId()
		: CompilerVersionID()
		, ScriptUsageType(ENiagaraScriptUsage::Function)
		, bUsesRapidIterationParams(true)
		, bDisableDebugSwitches(false)
		, bInterpolatedSpawn(false)
		, bRequiresPersistentIDs(false)
		, BaseScriptID_DEPRECATED(0, 0, 0, 0)
	{ }


	~FNiagaraVMExecutableDataId()
	{ }

	bool IsValid() const;
	void Invalidate();
	
	friend uint32 GetTypeHash(const FNiagaraVMExecutableDataId& Ref)
	{
		return Ref.BaseScriptCompileHash.GetTypeHash();
	}

	SIZE_T GetSizeBytes() const
	{
		return sizeof(*this);
	}

	bool HasInterpolatedParameters() const;
	bool RequiresPersistentIDs() const;
#if 0
	/** Hashes the script-specific part of this shader map Id. */
	void GetScriptHash(FSHAHash& OutHash) const;
#endif

	/**
	* Tests this set against another for equality, disregarding override settings.
	*
	* @param ReferenceSet	The set to compare against
	* @return				true if the sets are equal
	*/
	bool operator==(const FNiagaraVMExecutableDataId& ReferenceSet) const;

	bool operator!=(const FNiagaraVMExecutableDataId& ReferenceSet) const
	{
		return !(*this == ReferenceSet);
	}

#if WITH_EDITORONLY_DATA
	/** Appends string representations of this Id to a key string. */
	void AppendKeyString(FString& KeyString, const FString& Delimiter = TEXT("_"), bool bAppendObjectForDebugging = false) const;

#endif
};

USTRUCT()
struct NIAGARA_API FNiagaraVMExecutableByteCode
{
	GENERATED_USTRUCT_BODY()
private:
	UPROPERTY()
	TArray<uint8> Data;

	UPROPERTY()
	int32 UncompressedSize = INDEX_NONE;

public:
	bool HasByteCode() const;
	bool IsCompressed() const;
	bool Compress();
	bool Uncompress();

	void Reset();

	void SetData(const TArray<uint8>& InData);
	void SetData(TArray<uint8>&& InData);

	const TArray<uint8>& GetData() const { return Data; }
	TArray<uint8>& GetData() { return Data; }
	const uint8* GetDataPtr() const { return Data.GetData(); }
	int32 GetLength() const { return Data.Num(); }

	void Reserve(int32 Number) { Data.Reserve(Number); }
	void Shrink() { Data.Shrink(); }

	/** Used to upgrade a serialized property to our own struct */
	bool SerializeFromMismatchedTag(const struct FPropertyTag& Tag, FStructuredArchive::FSlot Slot);
};

template<>
struct TStructOpsTypeTraits<FNiagaraVMExecutableByteCode> : public TStructOpsTypeTraitsBase2<FNiagaraVMExecutableByteCode>
{
	enum
	{
		WithStructuredSerializeFromMismatchedTag = true,
	};
};

// Carrier for uncompressed, and optimmized bytecode returned from optimization task
struct NIAGARA_API FNiagaraScriptAsyncOptimizeTaskState
{
	FNiagaraVMExecutableDataId CachedScriptVMId;
	FNiagaraVMExecutableByteCode SourceByteCode;
	FNiagaraVMExecutableByteCode OptimizedByteCode;

	TArray<uint8, TInlineAllocator<32>> ExternalFunctionRegisterCounts;
	
	bool bShouldOptimizeByteCode = false;
	bool bShouldFreeSourceByteCodeOnCooked = false;
	bool bOptimizationComplete = false;
		
	void OptimizeByteCode();
};
using FNiagaraScriptAsyncOptimizeTaskStatePtr = TSharedPtr<FNiagaraScriptAsyncOptimizeTaskState, ESPMode::ThreadSafe>;


/** Struct containing all of the data needed to run a Niagara VM executable script.*/
USTRUCT()
struct NIAGARA_API FNiagaraVMExecutableData
{
	GENERATED_USTRUCT_BODY()
public:
	FNiagaraVMExecutableData();

	/** Byte code to execute for this system. */
	UPROPERTY()
	FNiagaraVMExecutableByteCode ByteCode;

	/** Optimized version of the byte code to execute for this system */
	UPROPERTY(Transient)
	FNiagaraVMExecutableByteCode OptimizedByteCode;
	// Container for the optimization task that doesn't copy the task over when the executable data is copied
	struct FOptimizationTask
	{
		FOptimizationTask() { }
		FOptimizationTask(const FOptimizationTask&) { }
		FOptimizationTask& operator=(const FOptimizationTask&) { return *this; }

		// Optimization task if one is pending, that should be applied before execution
		FNiagaraScriptAsyncOptimizeTaskStatePtr State;

		// Lock used to apply the optimization results
		FCriticalSection Lock;
	} OptimizationTask;

	/** Number of temp registers used by this script. */
	UPROPERTY()
	int32 NumTempRegisters;

	/** Number of user pointers we must pass to the VM. */
	UPROPERTY()
	int32 NumUserPtrs;

#if WITH_EDITORONLY_DATA
	/** All the data for using external constants in the script, laid out in the order they are expected in the uniform table.*/
	UPROPERTY()
	FNiagaraParameters Parameters;

	/** All the data for using external constants in the script, laid out in the order they are expected in the uniform table.*/
	UPROPERTY()
	FNiagaraParameters InternalParameters;

	/** List of all external dependencies of this script. If not met, linking should result in an error.*/
	UPROPERTY()
	TArray<FNiagaraCompileDependency> ExternalDependencies;

#endif

	UPROPERTY()
	TArray<FNiagaraCompilerTag> CompileTags;

	UPROPERTY()
	TArray<uint8> ScriptLiterals;

	/** Attributes used by this script. */
	UPROPERTY()
	TArray<FNiagaraVariable> Attributes;

	/** Contains various usage information for this script. */
	UPROPERTY()
	FNiagaraScriptDataUsageInfo DataUsage;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TMap<FName, FNiagaraParameters> DataSetToParameters;

	UPROPERTY()
	TArray<FNiagaraFunctionSignature> AdditionalExternalFunctions;
#endif

	/** Information about all data interfaces used by this script. */
	UPROPERTY()
	TArray<FNiagaraScriptDataInterfaceCompileInfo> DataInterfaceInfo;

	/** Array of ordered vm external functions to place in the function table. */
	UPROPERTY()
	TArray<FVMExternalFunctionBindingInfo> CalledVMExternalFunctions;

	TArray<FVMExternalFunction> CalledVMExternalFunctionBindings;

	UPROPERTY()
	TArray<FNiagaraDataSetID> ReadDataSets;

	UPROPERTY()
	TArray<FNiagaraDataSetProperties> WriteDataSets;

	/** Scopes we'll track with stats.*/
	UPROPERTY()
	TArray<FNiagaraStatScope> StatScopes;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FString LastHlslTranslation;

	UPROPERTY()
	FString LastHlslTranslationGPU;

	UPROPERTY()
	FString LastAssemblyTranslation;

	UPROPERTY()
	uint32 LastOpCount;
#endif

	UPROPERTY()
	FNiagaraShaderScriptParametersMetadata ShaderScriptParametersMetadata; //TODO: GPU Param info should not be in the "VM executable data"

#if WITH_EDITORONLY_DATA
	/** The parameter collections used by this script. */
	UPROPERTY()
	TArray<FString> ParameterCollectionPaths;
#endif

	/** Last known compile status. Lets us determine the latest state of the script byte buffer.*/
	UPROPERTY()
	ENiagaraScriptCompileStatus LastCompileStatus;

	UPROPERTY()
	TArray<FSimulationStageMetaData> SimulationStageMetaData;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	bool bReadsAttributeData;

	/** List of all attributes explicitly written by this VM script graph. Used to verify external dependencies.*/
	UPROPERTY()
	TArray<FNiagaraVariableBase> AttributesWritten;

	/** List of all attributes explicitly written by this VM script graph. Used to verify external dependencies.*/
	UPROPERTY()
	TArray<FNiagaraVariable> StaticVariablesWritten;

	UPROPERTY()
	FString ErrorMsg;

	UPROPERTY()
	float CompileTime;

	/** Array of all compile events generated last time the script was compiled.*/
	UPROPERTY()
	TArray<FNiagaraCompileEvent> LastCompileEvents;
#endif

	UPROPERTY()
	TArray<uint8> ExperimentalContextData;

	UPROPERTY()
	uint32 bReadsSignificanceIndex : 1;

	UPROPERTY()
	uint32 bNeedsGPUContextInit : 1;

	void ApplyFinishedOptimization(const FNiagaraVMExecutableDataId& CachedScriptVMId, const FNiagaraScriptAsyncOptimizeTaskStatePtr& Result);

	void SerializeData(FArchive& Ar, bool bDDCData);
	
	bool IsValid() const;

	void Reset();

#if WITH_EDITORONLY_DATA
	void BakeScriptLiterals(TArray<uint8>& OutLiterals) const;
#endif

	bool HasByteCode() const;

#if VECTORVM_SUPPORTS_EXPERIMENTAL
	bool SupportsExperimentalVM() const;
	FVectorVMOptimizeContext BuildExperimentalContext() const;
#endif //VECTORVM_SUPPORTS_EXPERIMENTAL

	void PostSerialize(const FArchive& Ar);
};

template<>
struct TStructOpsTypeTraits<FNiagaraVMExecutableData> : public TStructOpsTypeTraitsBase2<FNiagaraVMExecutableData>
{
	enum
	{
		WithPostSerialize = true,
	};
};

struct NIAGARA_API FNiagaraGraphCachedDataBase
{
	virtual ~FNiagaraGraphCachedDataBase() {}
	virtual void GetStaticVariables(TArray<FNiagaraVariable>& OutVars) {}
};

/** Struct containing all of the data that can be different between different script versions.*/
USTRUCT()
struct NIAGARA_API FVersionedNiagaraScriptData
{
	GENERATED_USTRUCT_BODY()

#if WITH_EDITORONLY_DATA
public:
	FVersionedNiagaraScriptData();

	UPROPERTY()
	FNiagaraAssetVersion Version;
	
	/** What changed in this version compared to the last? Displayed to the user when upgrading to a new script version. */
	UPROPERTY()
	FText VersionChangeDescription;

	/** When used as a module, what are the appropriate script types for referencing this module?*/
	UPROPERTY(EditAnywhere, Category = Script, meta = (Bitmask, BitmaskEnum = "/Script/Niagara.ENiagaraScriptUsage"))
	int32 ModuleUsageBitmask;

	/** Used to break up scripts of the same Usage type in UI display.*/
	UPROPERTY(EditAnywhere, Category = Script)
	FText Category;

	/** If true, this script will be added to a 'Suggested' category at the top of menus during searches */
	UPROPERTY(AssetRegistrySearchable, EditAnywhere, Category = Script)
	bool bSuggested = false;
	
	/** Array of Ids of dependencies provided by this module to other modules on the stack (e.g. 'ProvidesNormalizedAge') */
	UPROPERTY(EditAnywhere, Category = Script)
	TArray<FName> ProvidedDependencies;

	/** Dependencies required by this module from other modules on the stack */
	UPROPERTY(EditAnywhere, Category = Script)
	TArray<FNiagaraModuleDependency> RequiredDependencies;
	
	/* If this script is no longer meant to be used, this option should be set.*/
	UPROPERTY()
	bool bDeprecated;
	
	/* Message to display when the script is deprecated. */
	UPROPERTY()
	FText DeprecationMessage;

	/* Which script to use if this is deprecated.*/
	UPROPERTY(EditAnywhere, Category = Script)
	TObjectPtr<UNiagaraScript> DeprecationRecommendation;

	/* Custom logic to convert the contents of an existing script assignment to this script.*/
	UPROPERTY(EditAnywhere, Category = Script)
	TSubclassOf<UNiagaraConvertInPlaceUtilityBase> ConversionUtility;

	/** Is this script experimental and less supported? */
	UPROPERTY(EditAnywhere, Category = Script)
	uint32 bExperimental : 1;

	/** The message to display when a function is marked experimental. */
	UPROPERTY(EditAnywhere, Category = Script, meta = (EditCondition = "bExperimental", MultiLine = true))
	FText ExperimentalMessage;

	/** A message to display when adding the module to the stack. This is useful to highlight pitfalls or weird behavior of the module. */
	UPROPERTY(EditAnywhere, Category = Script, meta = (MultiLine = true))
	FText NoteMessage;

	/* Defines if this script is visible to the user when searching for modules to add to an emitter.  */
	UPROPERTY(AssetRegistrySearchable, EditAnywhere, Category = Script)
	ENiagaraScriptLibraryVisibility LibraryVisibility;

	/** The mode to use when deducing the type of numeric output pins from the types of the input pins. */
	UPROPERTY(EditAnywhere, Category=Script)
	ENiagaraNumericOutputTypeSelectionMode NumericOutputTypeSelectionMode;

	UPROPERTY(AssetRegistrySearchable, EditAnywhere, Category = Script, meta = (MultiLine = true))
	FText Description;

	/** A list of space separated keywords which can be used to find this script in editor menus. */
	UPROPERTY(AssetRegistrySearchable, EditAnywhere, Category = Script)
	FText Keywords;

	/** The format for the text to display in the stack if the value is collapsed.
	*  This supports formatting placeholders for the function inputs, for example "myfunc({0}, {1})" will be converted to "myfunc(1.23, Particles.Position)". */
	UPROPERTY(EditAnywhere, Category = Script)
	FText CollapsedViewFormat;

	/* If used as a dynamic input with exactly one input and output of different types, setting this to true will auto-insert this script to convert from one type to another when dragging and dropping parameters in the stack.
	   For example, a script with a bool as input and a float as output will be automatically inserted in the stack when dropping a bool parameter into the float input of a module in the stack. */
	UPROPERTY(EditAnywhere, Category = Script)
	uint32 bCanBeUsedForTypeConversions : 1;

	UPROPERTY(EditAnywhere, Category = Script, DisplayName = "Script Metadata", meta = (ToolTip = "Script Metadata"))
	TMap<FName, FString> ScriptMetaData;
	
	UPROPERTY(EditAnywhere, Category = Script)
	TArray<FNiagaraStackSection> InputSections;

	/** Adjusted every time ComputeVMCompilationId is called.*/
	UPROPERTY()
	mutable FNiagaraVMExecutableDataId LastGeneratedVMId;

	/** Reference to a python script that is executed when the user updates from a previous version to this version. */
	UPROPERTY()
	ENiagaraPythonUpdateScriptReference UpdateScriptExecution = ENiagaraPythonUpdateScriptReference::None;

	/** Python script to run when updating to this script version. */
	UPROPERTY()
	FString PythonUpdateScript;

	/** Asset reference to a python script to run when updating to this script version. */
	UPROPERTY()
	FFilePath ScriptAsset;

	/** Subscriptions to parameter definitions for this script version */
	UPROPERTY()
	TArray<FParameterDefinitionsSubscription> ParameterDefinitionsSubscriptions;
	TArray<ENiagaraScriptUsage> GetSupportedUsageContexts() const;

private:
	friend class UNiagaraScript;

	/** 'Source' data/graphs for this script */
	UPROPERTY()
	TObjectPtr<class UNiagaraScriptSourceBase>	Source = nullptr;
#endif	
};

/** Runtime script for a Niagara system */
UCLASS(MinimalAPI)
class UNiagaraScript : public UNiagaraScriptBase, public FNiagaraVersionedObject
{
	GENERATED_UCLASS_BODY()
public:
	UNiagaraScript();

#if WITH_EDITORONLY_DATA
	/** If true then this script asset uses active version control to track changes. */
	NIAGARA_API virtual bool IsVersioningEnabled() const override { return bVersioningEnabled; }

	/** Returns the script data for latest exposed version. */
	NIAGARA_API FVersionedNiagaraScriptData* GetLatestScriptData();
	NIAGARA_API const FVersionedNiagaraScriptData* GetLatestScriptData() const;

	/** Returns the script data for a specific version or nullptr if no such version is found. For the null-Guid it returns the exposed version.  */
	NIAGARA_API FVersionedNiagaraScriptData* GetScriptData(const FGuid& VersionGuid);
	NIAGARA_API const FVersionedNiagaraScriptData* GetScriptData(const FGuid& VersionGuid) const;

	/** Returns all available versions for this script. */
	NIAGARA_API virtual TArray<FNiagaraAssetVersion> GetAllAvailableVersions() const override;

	/** Returns the version of the exposed version data (i.e. the version used when adding a module to the stack) */
	NIAGARA_API virtual FNiagaraAssetVersion GetExposedVersion() const override;

	/** Returns the version data for the given guid, if it exists. Otherwise returns nullptr. */
	NIAGARA_API virtual FNiagaraAssetVersion const* FindVersionData(const FGuid& VersionGuid) const override;
	
	/** Creates a new data entry for the given version number. The version must be > 1.0 and must not collide with an already existing version. The data will be a copy of the previous minor version. */
	NIAGARA_API virtual FGuid AddNewVersion(int32 MajorVersion, int32 MinorVersion) override;

	/** Deletes the version data for an existing version. The exposed version cannot be deleted and will result in an error. Does nothing if the guid does not exist in the script's version data. */
	NIAGARA_API virtual void DeleteVersion(const FGuid& VersionGuid) override;

	/** Changes the exposed version. Does nothing if the guid does not exist in the script's version data. */
	NIAGARA_API virtual void ExposeVersion(const FGuid& VersionGuid) override;

	/** Enables versioning for this script asset. */
	NIAGARA_API virtual void EnableVersioning() override;

	/** Disables versioning and keeps only the data from the given version guid. Note that this breaks ALL references from existing assets and should only be used when creating a copy of a script, as the effect is very destructive.  */
	NIAGARA_API virtual void DisableVersioning(const FGuid& VersionGuidToUse) override;

	NIAGARA_API virtual TSharedPtr<FNiagaraVersionDataAccessor> GetVersionDataAccessor(const FGuid& Version) override; 

	/** Makes sure that the default version data is available and fixes old script assets. */
	NIAGARA_API void CheckVersionDataAvailable();

	/** Creates a shallow transient copy of this script for compilation purposes. */
	NIAGARA_API UNiagaraScript* CreateCompilationCopy();

	/** A set of rules to apply when this script is used in the stack. To create your own rules, write a custom class that extends UNiagaraValidationRule. */
	UPROPERTY(EditAnywhere, Category = "Validation", Instanced)
	TArray<TObjectPtr<UNiagaraValidationRule>> ValidationRules;
#endif

	/** Workaround for emitter versioning because we used a lot of Script->GetOuter() previously. */
	NIAGARA_API FVersionedNiagaraEmitter GetOuterEmitter() const;

	// how this script is to be used. cannot be private due to use of GET_MEMBER_NAME_CHECKED
	UPROPERTY(AssetRegistrySearchable)
	ENiagaraScriptUsage Usage;

#if WITH_EDITOR
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnScriptCompiled, UNiagaraScript*, const FGuid&);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPropertyChanged, FPropertyChangedEvent& /* PropertyChangedEvent */)
#endif

private:
	/** Specifies a unique id for use when there are multiple scripts with the same usage, e.g. events. */
	UPROPERTY()
	FGuid UsageId;

#if WITH_EDITORONLY_DATA
	/** The exposed version is the version that is used by default when a user adds this script somewhere. It is basically the published version and allows a script maintainer to create and test newer versions that are not used by normal users. */
	UPROPERTY()
	FGuid ExposedVersion;

	/** If true then this script asset uses active version control to track changes. */
	UPROPERTY()
	bool bVersioningEnabled = false;

	/** Contains all of the versioned script data. */
	UPROPERTY()
	TArray<FVersionedNiagaraScriptData> VersionData;

	/** Editor time adapters to a specific VersionData and this Script ptr to handle synchronizing changes made by parameter definitions. */
	TArray<struct FVersionedNiagaraScript> VersionedScriptAdapters;
#endif

public:

	/** Contains all of the top-level values that are iterated on in the UI. These are usually "Module" variables in the graph. They don't necessarily have to be in the order that they are expected in the uniform table.*/
	UPROPERTY()
	FNiagaraParameterStore RapidIterationParameters;
	
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FNiagaraParameterStore RapidIterationParametersCookedEditorCache;

	/** This is used as a transient value to open a specific version in the graph editor */
	UPROPERTY(Transient)
	FGuid VersionToOpenInEditor;

	/** Which instance of the usage in the graph to use.  This is now deprecated and is handled by UsageId. */
	UPROPERTY()
	int32 UsageIndex_DEPRECATED;
	
	/** Use property in struct returned from GetScriptData() instead */
	UPROPERTY(meta = (DeprecatedProperty))
	int32 ModuleUsageBitmask_DEPRECATED;

	/** Use property in struct returned from GetScriptData() instead */
	UPROPERTY(meta = (DeprecatedProperty))
	FText Category_DEPRECATED;

	/** Use property in struct returned from GetScriptData() instead */
	UPROPERTY(meta = (DeprecatedProperty))
	TArray<FName> ProvidedDependencies_DEPRECATED;
	
	/** Use property in struct returned from GetScriptData() instead */
	UPROPERTY(meta = (DeprecatedProperty))
	TArray<FNiagaraModuleDependency> RequiredDependencies_DEPRECATED;

	/** Use property in struct returned from GetScriptData() instead */
	UPROPERTY(meta = (DeprecatedProperty))
	uint32 bDeprecated_DEPRECATED : 1;

	/** Use property in struct returned from GetScriptData() instead */
	UPROPERTY(meta = (DeprecatedProperty))
	FText DeprecationMessage_DEPRECATED;

	/** Use property in struct returned from GetScriptData() instead */
	UPROPERTY(meta = (DeprecatedProperty))
	TObjectPtr<UNiagaraScript> DeprecationRecommendation_DEPRECATED;

	/** Use property in struct returned from GetScriptData() instead */
	UPROPERTY(meta = (DeprecatedProperty))
	TSubclassOf<UNiagaraConvertInPlaceUtilityBase> ConversionUtility_DEPRECATED;

	/** Use property in struct returned from GetScriptData() instead */
	UPROPERTY(meta = (DeprecatedProperty))
	uint32 bExperimental_DEPRECATED : 1;

	/** Use property in struct returned from GetScriptData() instead */
	UPROPERTY(meta = (DeprecatedProperty))
	FText ExperimentalMessage_DEPRECATED;

	/** Use property in struct returned from GetScriptData() instead */
	UPROPERTY(meta = (DeprecatedProperty))
	FText NoteMessage_DEPRECATED;

	/* Deprecated, use LibraryVisibility instead. */
	UPROPERTY(meta = (DeprecatedProperty))
	uint32 bExposeToLibrary_DEPRECATED : 1;
	
	/** Use property in struct returned from GetScriptData() instead */
	UPROPERTY(meta = (DeprecatedProperty))
	ENiagaraScriptLibraryVisibility LibraryVisibility_DEPRECATED;

	/** Use property in struct returned from GetScriptData() instead */
	UPROPERTY(meta = (DeprecatedProperty))
	ENiagaraNumericOutputTypeSelectionMode NumericOutputTypeSelectionMode_DEPRECATED;

	/** Use property in struct returned from GetScriptData() instead */
	UPROPERTY(meta = (DeprecatedProperty))
	FText Description_DEPRECATED;

	/** Use property in struct returned from GetScriptData() instead */
	UPROPERTY(meta = (DeprecatedProperty))
	FText Keywords_DEPRECATED;

	/* Deprecated, use LibraryVisibility instead. */
	UPROPERTY(meta = (DeprecatedProperty))
	FText CollapsedViewFormat_DEPRECATED;

	/* Deprecated, use LibraryVisibility instead. */
	UPROPERTY(meta = (DeprecatedProperty))
	TMap<FName, FString> ScriptMetaData_DEPRECATED;

	/** 'Source' data/graphs for this script */
	UPROPERTY(meta = (DeprecatedProperty))
	TObjectPtr<class UNiagaraScriptSourceBase>	Source_DEPRECATED;

	NIAGARA_API static const FName NiagaraCustomVersionTagName;
#endif

	NIAGARA_API void ComputeVMCompilationId(FNiagaraVMExecutableDataId& Id, FGuid VersionGuid) const;
	NIAGARA_API const FNiagaraVMExecutableDataId& GetComputedVMCompilationId() const
	{
#if WITH_EDITORONLY_DATA
		if (!IsCooked)
		{
			return GetLastGeneratedVMId();
		}
#endif
		return CachedScriptVMId;
	}

	void SetUsage(ENiagaraScriptUsage InUsage) { Usage = InUsage; }
	ENiagaraScriptUsage GetUsage() const { return Usage; }

	void SetUsageId(const FGuid& InUsageId) { UsageId = InUsageId; }
	FGuid GetUsageId() const { return UsageId; }

	NIAGARA_API bool ContainsUsage(ENiagaraScriptUsage InUsage) const;
	bool IsEquivalentUsage(ENiagaraScriptUsage InUsage) const {return (InUsage == Usage) || (Usage == ENiagaraScriptUsage::ParticleSpawnScript && InUsage == ENiagaraScriptUsage::ParticleSpawnScriptInterpolated) || (Usage == ENiagaraScriptUsage::ParticleSpawnScriptInterpolated && InUsage == ENiagaraScriptUsage::ParticleSpawnScript);}
	static bool IsEquivalentUsage(ENiagaraScriptUsage InUsageA, ENiagaraScriptUsage InUsageB) { return (InUsageA == InUsageB) || (InUsageB == ENiagaraScriptUsage::ParticleSpawnScript && InUsageA == ENiagaraScriptUsage::ParticleSpawnScriptInterpolated) || (InUsageB == ENiagaraScriptUsage::ParticleSpawnScriptInterpolated && InUsageA == ENiagaraScriptUsage::ParticleSpawnScript); }
	/** Is usage A dependent on Usage B?*/
	NIAGARA_API static bool IsUsageDependentOn(ENiagaraScriptUsage InUsageA, ENiagaraScriptUsage InUsageB);

	bool IsParticleSpawnScript() const { return IsParticleSpawnScript(Usage); }
	bool IsInterpolatedParticleSpawnScript() const { return IsInterpolatedParticleSpawnScript(Usage); }
	bool IsParticleUpdateScript() const { return IsParticleUpdateScript(Usage); }
	bool IsModuleScript() const { return IsModuleScript(Usage); }
	bool IsFunctionScript()	const { return IsFunctionScript(Usage); }
	bool IsDynamicInputScript() const { return IsDynamicInputScript(Usage); }
	bool IsParticleEventScript() const { return IsParticleEventScript(Usage); }
	bool IsParticleScript() const {	return IsParticleScript(Usage);}

	bool IsNonParticleScript() const { return IsNonParticleScript(Usage); }
	
	bool IsSystemSpawnScript() const { return IsSystemSpawnScript(Usage); }
	bool IsSystemUpdateScript() const { return IsSystemUpdateScript(Usage); }
	bool IsEmitterSpawnScript() const { return IsEmitterSpawnScript(Usage); }
	bool IsEmitterUpdateScript() const { return IsEmitterUpdateScript(Usage); }
	bool IsStandaloneScript() const { return IsStandaloneScript(Usage); }

	bool IsSpawnScript() const { return IsParticleSpawnScript() || IsEmitterSpawnScript() || IsSystemSpawnScript(); }

	bool IsCompilable() const { return IsCompilable(Usage); }


	static bool IsGPUScript(ENiagaraScriptUsage Usage) { return Usage == ENiagaraScriptUsage::ParticleGPUComputeScript; }
	static bool IsParticleSpawnScript(ENiagaraScriptUsage Usage)  { return Usage == ENiagaraScriptUsage::ParticleSpawnScript || Usage == ENiagaraScriptUsage::ParticleSpawnScriptInterpolated; }
	static bool IsInterpolatedParticleSpawnScript(ENiagaraScriptUsage Usage) { return Usage == ENiagaraScriptUsage::ParticleSpawnScriptInterpolated; }
	static bool IsParticleUpdateScript(ENiagaraScriptUsage Usage) { return Usage == ENiagaraScriptUsage::ParticleUpdateScript; }
	static bool IsParticleStageScript(ENiagaraScriptUsage Usage) { return Usage == ENiagaraScriptUsage::ParticleSimulationStageScript; }
	static bool IsModuleScript(ENiagaraScriptUsage Usage) { return Usage == ENiagaraScriptUsage::Module; }
	static bool IsFunctionScript(ENiagaraScriptUsage Usage)	 { return Usage == ENiagaraScriptUsage::Function; }
	static bool IsDynamicInputScript(ENiagaraScriptUsage Usage)	 { return Usage == ENiagaraScriptUsage::DynamicInput; }
	static bool IsParticleEventScript(ENiagaraScriptUsage Usage) { return Usage == ENiagaraScriptUsage::ParticleEventScript; }
	static bool IsParticleScript(ENiagaraScriptUsage Usage)  { return Usage >= ENiagaraScriptUsage::ParticleSpawnScript && Usage <= ENiagaraScriptUsage::ParticleGPUComputeScript; }

	static bool IsNonParticleScript(ENiagaraScriptUsage Usage) { return Usage >= ENiagaraScriptUsage::EmitterSpawnScript; }

	static bool IsSystemSpawnScript(ENiagaraScriptUsage Usage) { return Usage == ENiagaraScriptUsage::SystemSpawnScript; }
	static bool IsSystemUpdateScript(ENiagaraScriptUsage Usage) { return Usage == ENiagaraScriptUsage::SystemUpdateScript; }
	static bool IsSystemScript(ENiagaraScriptUsage Usage) { return IsSystemSpawnScript(Usage) || IsSystemUpdateScript(Usage);}

	static bool IsEmitterScript(ENiagaraScriptUsage Usage) { return IsEmitterSpawnScript(Usage) || IsEmitterUpdateScript(Usage); }
	static bool IsEmitterSpawnScript(ENiagaraScriptUsage Usage) { return Usage == ENiagaraScriptUsage::EmitterSpawnScript; }
	static bool IsEmitterUpdateScript(ENiagaraScriptUsage Usage) { return Usage == ENiagaraScriptUsage::EmitterUpdateScript; }
	static bool IsStandaloneScript(ENiagaraScriptUsage Usage)  { return IsDynamicInputScript(Usage) || IsFunctionScript(Usage) || IsModuleScript(Usage); }

	static bool IsSpawnScript(ENiagaraScriptUsage Usage) { return IsParticleSpawnScript(Usage) || IsEmitterSpawnScript(Usage) || IsSystemSpawnScript(Usage); }

	static bool IsCompilable(ENiagaraScriptUsage Usage)  { return !IsEmitterSpawnScript(Usage) && !IsEmitterUpdateScript(Usage); }

	static bool NIAGARA_API ConvertUsageToGroup(ENiagaraScriptUsage InUsage, ENiagaraScriptGroup& OutGroup);

#if WITH_EDITORONLY_DATA
	static NIAGARA_API TArray<ENiagaraScriptUsage> GetSupportedUsageContextsForBitmask(int32 InModuleUsageBitmask, bool bIncludeHiddenUsages = false);
	static NIAGARA_API bool IsSupportedUsageContextForBitmask(int32 InModuleUsageBitmask, ENiagaraScriptUsage InUsageContext, bool bIncludeHiddenUsages = false);
	static NIAGARA_API bool ContainsEquivilentUsage(const TArray<ENiagaraScriptUsage>& Usages, ENiagaraScriptUsage InUsage);
#endif

	NIAGARA_API bool CanBeRunOnGpu() const;
	NIAGARA_API bool IsReadyToRun(ENiagaraSimTarget SimTarget) const;
	NIAGARA_API bool ShouldCacheShadersForCooking(const ITargetPlatform* TargetPlatform) const;

#if WITH_EDITORONLY_DATA
	NIAGARA_API class UNiagaraScriptSourceBase* GetLatestSource();
	NIAGARA_API const class UNiagaraScriptSourceBase* GetLatestSource() const;
	NIAGARA_API class UNiagaraScriptSourceBase* GetSource(const FGuid& VersionGuid);
	NIAGARA_API const class UNiagaraScriptSourceBase* GetSource(const FGuid& VersionGuid) const;
	NIAGARA_API void SetLatestSource(class UNiagaraScriptSourceBase* InSource);
	NIAGARA_API void SetSource(class UNiagaraScriptSourceBase* InSource, const FGuid& VersionGuid);

	NIAGARA_API FGuid GetBaseChangeID(const FGuid& VersionGuid = FGuid()) const;
	NIAGARA_API ENiagaraScriptCompileStatus GetLastCompileStatus() const;

	NIAGARA_API bool HandleVariableRenames(const TMap<FNiagaraVariable, FNiagaraVariable>& OldToNewVars, const FString& UniqueEmitterName);
#endif

	//~ Begin UObject interface
	PRAGMA_DISABLE_DEPRECATION_WARNINGS // Suppress compiler warning on override of deprecated function
	UE_DEPRECATED(5.0, "Use version that takes FObjectPreSaveContext instead.")
	virtual void PreSave(const class ITargetPlatform* TargetPlatform) override;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;
	virtual void Serialize(FArchive& Ar)override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditChangeVersionedProperty(FPropertyChangedEvent& PropertyChangedEvent, const FGuid& Version);
	virtual void PostEditUndo();
#endif
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;

	virtual bool IsEditorOnly() const override;

	virtual NIAGARA_API void BeginDestroy() override;
	virtual NIAGARA_API bool IsReadyForFinishDestroy() override;
	//~ End UObject interface

	//~ Begin UNiagaraScriptBase interface
	virtual bool ShouldCompile(EShaderPlatform Platform) const override;
	virtual void ModifyCompilationEnvironment(EShaderPlatform Platform, struct FShaderCompilerEnvironment& OutEnvironment) const override;
	virtual TConstArrayView<FSimulationStageMetaData> GetSimulationStageMetaData() const override { return MakeArrayView(CachedScriptVM.SimulationStageMetaData); }
	//~ End UNiagaraScriptBase interface

	// Infrastructure for GPU compute Shaders
#if WITH_EDITOR
	NIAGARA_API void CacheResourceShadersForCooking(EShaderPlatform ShaderPlatform, TArray<FNiagaraShaderScript*>& InOutCachedResources, const ITargetPlatform* TargetPlatform = nullptr);

	NIAGARA_API void CacheResourceShadersForRendering(bool bRegenerateId, bool bForceRecompile=false);
	void BeginCacheForCookedPlatformData(const ITargetPlatform *TargetPlatform);
	virtual bool IsCachedCookedPlatformDataLoaded(const ITargetPlatform* TargetPlatform) override;
	void CacheShadersForResources(FNiagaraShaderScript* ResourceToCache, bool bApplyCompletedShaderMapForRendering, bool bForceRecompile = false, bool bCooking=false, const ITargetPlatform* TargetPlatform = nullptr);
	void SaveShaderStableKeys(const class ITargetPlatform* TP);
#endif // WITH_EDITOR
	FNiagaraShaderScript* AllocateResource();
	FNiagaraShaderScript* GetRenderThreadScript()
	{
		return ScriptResource.Get();
	}

	const FNiagaraShaderScript* GetRenderThreadScript() const
	{
		return ScriptResource.Get();
	}

	NIAGARA_API void GenerateStatIDs();

	NIAGARA_API bool IsScriptCompilationPending(bool bGPUScript) const;
	NIAGARA_API bool DidScriptCompilationSucceed(bool bGPUScript) const;

	template<typename T>
	TOptional<T> GetCompilerTag(const FNiagaraVariableBase& InVar, const FNiagaraParameterStore* FallbackParameterStore = nullptr) const
	{
		for (const FNiagaraCompilerTag& Tag : CachedScriptVM.CompileTags)
		{
			if (static_cast<const FNiagaraVariableBase&>(Tag.Variable) == InVar)
			{
				if (Tag.Variable.IsDataAllocated())
				{
					return TOptional<T>(Tag.Variable.GetValue<T>());
				}
				else if (const int32* Offset = RapidIterationParameters.FindParameterOffset(FNiagaraVariableBase(Tag.Variable.GetType(), *Tag.StringValue)))
				{
					return TOptional<T>(*(T*)RapidIterationParameters.GetParameterData(*Offset));
				}
				else if (FallbackParameterStore)
				{
					if (const int32* OffsetAlternate = FallbackParameterStore->FindParameterOffset(FNiagaraVariableBase(Tag.Variable.GetType(), *Tag.StringValue)))
						return TOptional<T>(*(T*)FallbackParameterStore->GetParameterData(*OffsetAlternate));
				}
			}
		}

		return TOptional<T>();
	}

	TOptional<FVector3d> GetCompilerTag(const FNiagaraVariableBase& InVar, const FNiagaraParameterStore* FallbackParameterStore) const
	{
		TOptional<FVector3f> Optional(*GetCompilerTag<FVector3f>(InVar, FallbackParameterStore));
		if (Optional.IsSet())
		{
			return TOptional<FVector3d>((FVector3d)Optional.GetValue());
		}
		return TOptional<FVector3d>();
	}

#if WITH_EDITORONLY_DATA
	NIAGARA_API void InvalidateCompileResults(const FString& Reason);
	NIAGARA_API FText GetDescription(const FGuid& VersionGuid);

	/** Helper to convert the struct from its binary data out of the DDC to it's actual in-memory version.
		Do not call this on anything other than the game thread as it depends on the FObjectAndNameAsStringProxyArchive,
		which calls FindStaticObject which can fail when used in any other thread!*/
	static bool BinaryToExecData(const UNiagaraScript* Script, const TArray<uint8>& InBinaryData, FNiagaraVMExecutableData& OutExecData);

	/** Reverse of the BinaryToExecData() function */
	static bool ExecToBinaryData(const UNiagaraScript* Script, TArray<uint8>& OutBinaryData, FNiagaraVMExecutableData& InExecData);

	/** Determine if the Script and its source graph are in sync.*/
	NIAGARA_API bool AreScriptAndSourceSynchronized(const FGuid& VersionGuid = FGuid()) const;

	/** Ensure that the Script and its source graph are marked out of sync.*/
	NIAGARA_API void MarkScriptAndSourceDesynchronized(FString Reason, const FGuid& VersionGuid);
	
	/** Request a synchronous compile for the script, possibly forcing it to compile.*/
	NIAGARA_API void RequestCompile(const FGuid& ScriptVersion, bool bForceCompile = false);

	/** Request an asynchronous compile for the script, possibly forcing it to compile. The output values are the compilation id of the data as well as the async handle to 
		gather up the results with. The function returns whether or not any compiles were actually issued. */
	NIAGARA_API bool RequestExternallyManagedAsyncCompile(const TSharedPtr<FNiagaraCompileRequestDataBase, ESPMode::ThreadSafe>& RequestData, const TSharedPtr<FNiagaraCompileRequestDuplicateDataBase, ESPMode::ThreadSafe>& RequestDuplicateData, FNiagaraVMExecutableDataId& OutCompileId, uint32& OutAsyncHandle);

	/** Builds the DDC string for the derived data cache using the supplied CompiledId */
	static FString BuildNiagaraDDCKeyString(const FNiagaraVMExecutableDataId& CompileId);

	/** Creates a string key for the derived data cache */
	FString GetNiagaraDDCKeyString(const FGuid& ScriptVersion);

	/** Callback issued whenever a VM script compilation successfully happened (even if the results are a script that cannot be executed due to errors)*/
	NIAGARA_API FOnScriptCompiled& OnVMScriptCompiled();

	/** Callback issued whenever a GPU script compilation successfully happened (even if the results are a script that cannot be executed due to errors)*/
	NIAGARA_API FOnScriptCompiled& OnGPUScriptCompiled();

	/** Callback issues whenever post edit changed is called on this script. */
	NIAGARA_API FOnPropertyChanged& OnPropertyChanged();

	/** External call used to identify the values for a successful VM script compilation. OnVMScriptCompiled will be issued in this case.*/
	void SetVMCompilationResults(const FNiagaraVMExecutableDataId& InCompileId, FNiagaraVMExecutableData& InScriptVM, FString EmitterUniqueName, const TMap<FName, UNiagaraDataInterface*>& ObjectNameMap);

	/** In the event where we "merge" we duplicate the changes of the source script onto the newly cloned copy. This function will synchronize the compiled script 
		results assuming that the scripts themselves are bound to the same key. This saves looking things up in the DDC. It returns true if successfully synchronized and 
		false if not.*/
	NIAGARA_API bool SynchronizeExecutablesWithCompilation(const UNiagaraScript* Script, const TMap<FString, FString>& RenameMap);

	NIAGARA_API FString GetFriendlyName() const;

	NIAGARA_API void SyncAliases(const FNiagaraAliasContext& ResolveAliasesContext);
#endif
	
	UFUNCTION()
	void RaiseOnGPUCompilationComplete();


	NIAGARA_API FORCEINLINE FNiagaraVMExecutableData& GetVMExecutableData() { return CachedScriptVM; }
	NIAGARA_API FORCEINLINE const FNiagaraVMExecutableData& GetVMExecutableData() const { return CachedScriptVM; }
	NIAGARA_API FORCEINLINE const FNiagaraVMExecutableDataId& GetVMExecutableDataCompilationId() const { return CachedScriptVMId; }

	NIAGARA_API TConstArrayView<FNiagaraDataInterfaceGPUParamInfo> GetDataInterfaceGPUParamInfos() const { return CachedScriptVM.ShaderScriptParametersMetadata.DataInterfaceParamInfo; }

	NIAGARA_API TArray<UNiagaraParameterCollection*>& GetCachedParameterCollectionReferences();
	TArray<FNiagaraScriptDataInterfaceInfo>& GetCachedDefaultDataInterfaces() { return CachedDefaultDataInterfaces; }
	TConstArrayView<FNiagaraScriptDataInterfaceInfo> GetCachedDefaultDataInterfaces() const { return MakeArrayView(CachedDefaultDataInterfaces); }

#if STATS
	TArrayView<const TStatId> GetStatScopeIDs() const { return MakeArrayView(StatScopesIDs); }
#elif ENABLE_STATNAMEDEVENTS
	TArrayView<const FString> GetStatNamedEvents() const { return MakeArrayView(StatNamedEvents); }
#endif

	bool UsesCollection(const class UNiagaraParameterCollection* Collection)const;
	
	NIAGARA_API const FNiagaraScriptExecutionParameterStore* GetExecutionReadyParameterStore(ENiagaraSimTarget SimTarget);
	void InvalidateExecutionReadyParameterStores();

	bool IsScriptCooked() const
	{
#if WITH_EDITORONLY_DATA
		return IsCooked;
#else
		return true;
#endif
	}

private:
	bool OwnerCanBeRunOnGpu() const;
	bool LegacyCanBeRunOnGpu()const;

	void ProcessSerializedShaderMaps();
	void SerializeNiagaraShaderMaps(FArchive& Ar, int32 NiagaraVer, bool IsValidShaderScript);

	/** Return the expected SimTarget for this script. Only returns a valid target if there is valid data to run with. */
	TOptional<ENiagaraSimTarget> GetSimTarget() const;

	bool ShouldDecompressByteCode() const;
	bool ShouldOptimizeByteCode() const;
	bool ShouldFreeUnoptimizedByteCode() const;

public:
	/** Kicks off an async job to convert the ByteCode into an optimized version for the platform we are running on. */
	FGraphEventRef HandleByteCodeOptimization(bool bShouldForceNow = false);

	static int32 NIAGARA_API LogCompileStaticVars;

private:
	/** Generates all of the function bindings for DI that don't require user data */
	void GenerateDefaultFunctionBindings();

	/** Returns whether the parameter store bindings are valid */
	bool HasValidParameterBindings() const;

#if WITH_EDITORONLY_DATA
	/* Safely resolves soft object parameter collection references into hard references. */
	void ResolveParameterCollectionReferences();

	UPROPERTY(Transient)
	FNiagaraScriptExecutionParameterStore ScriptExecutionParamStoreCPU;

	UPROPERTY(Transient)
	FNiagaraScriptExecutionParameterStore ScriptExecutionParamStoreGPU;
#endif // WITH_EDITORONLY_DATA

	/** The equivalent of ScriptExecutionParamStoreCPU (or GPU) cooked for the given platform.*/
	UPROPERTY()
	FNiagaraScriptExecutionParameterStore ScriptExecutionParamStore;
	/** The cooked binding data between ScriptExecutionParamStore and RapidIterationParameters.*/
	UPROPERTY()
	TArray<FNiagaraBoundParameter> ScriptExecutionBoundParameters;

#if WITH_EDITORONLY_DATA
	class UNiagaraSystem* FindRootSystem();

	bool HasIdsRequiredForShaderCaching() const;

	/** A multicast delegate which is called whenever the script has been compiled (successfully or not). */
	FOnScriptCompiled OnVMScriptCompiledDelegate;
	FOnScriptCompiled OnGPUScriptCompiledDelegate;
	FOnPropertyChanged OnPropertyChangedDelegate;

	mutable FNiagaraVMExecutableDataId LastReportedVMId;

	mutable TOptional<TMap<FName, FString>> CustomAssetRegistryTagCache;
#endif

	/** Adjusted every time that we compile this script. Lets us know that we might differ from any cached versions.*/
	UPROPERTY()
	FNiagaraVMExecutableDataId CachedScriptVMId;

#if WITH_EDITORONLY_DATA
	FNiagaraVMExecutableDataId& GetLastGeneratedVMId(const FGuid& VersionGuid = FGuid()) const;
#endif

	TUniquePtr<FNiagaraShaderScript> ScriptResource;

#if WITH_EDITORONLY_DATA
	TArray<FNiagaraShaderScript> LoadedScriptResources;
	FNiagaraShaderScript* ScriptResourcesByFeatureLevel[ERHIFeatureLevel::Num];
#endif

	/** Compute shader compiled for this script */
	FComputeShaderRHIRef ScriptShader;

	/** Runtime stat IDs generated from StatScopes. */
#if STATS
	TArray<TStatId> StatScopesIDs;
#elif ENABLE_STATNAMEDEVENTS
	TArray<FString> StatNamedEvents;
#endif

#if WITH_EDITORONLY_DATA
	/* script resources being cached for cooking. */
	TMap<const class ITargetPlatform*, TArray<FNiagaraShaderScript*>> CachedScriptResourcesForCooking;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> ActiveCompileRoots;

	/* Flag set on load based on whether we're loading from a cooked package. */
	bool IsCooked = false;
#endif

	/** Compiled VM bytecode and data necessary to run this script.*/
	UPROPERTY()
	FNiagaraVMExecutableData CachedScriptVM;
	
	UPROPERTY()
	TArray<TObjectPtr<UNiagaraParameterCollection>> CachedParameterCollectionReferences;

	UPROPERTY()
	TArray<FNiagaraScriptDataInterfaceInfo> CachedDefaultDataInterfaces;

	static UNiagaraDataInterface* CopyDataInterface(UNiagaraDataInterface* Src, UObject* Owner);

	/** Flag used to guarantee that the RT isn't accessing the FNiagaraScriptResource before cleanup. */
	FThreadSafeBool ReleasedByRT;

	private :

#if WITH_EDITORONLY_DATA
		void ComputeVMCompilationId_EmitterShared(FNiagaraVMExecutableDataId& Id, const FVersionedNiagaraEmitter& Emitter, ENiagaraRendererSourceDataMode InSourceMode) const;
#endif
};

// Forward decl FVersionedNiagaraScriptWeakPtr to suport FVersionedNiagaraScript::ToWeakPtr().
struct FVersionedNiagaraScriptWeakPtr;

/** Struct combining a script with a specific version.*/
struct NIAGARA_API FVersionedNiagaraScript : public INiagaraParameterDefinitionsSubscriber
{
#if WITH_EDITORONLY_DATA
public:
	FVersionedNiagaraScript() //@todo(ng) refactor to never allow constructing with null script
		: Script(nullptr)
		, Version(FGuid())
	{};

	FVersionedNiagaraScript(UNiagaraScript* InScript)
		: Script(InScript)
		, Version(FGuid())
	{
	};

	FVersionedNiagaraScript(UNiagaraScript* InScript, const FGuid& InVersion)
		: Script(InScript)
		, Version(InVersion)
	{
	};

	//~ Begin INiagaraParameterDefinitionsSubscriber Interface
	virtual const TArray<FParameterDefinitionsSubscription>& GetParameterDefinitionsSubscriptions() const override { return GetScriptData()->ParameterDefinitionsSubscriptions; };
	virtual TArray<FParameterDefinitionsSubscription>& GetParameterDefinitionsSubscriptions() override { return GetScriptData()->ParameterDefinitionsSubscriptions; };

	/** Get all UNiagaraScriptSourceBase of this subscriber. */
	virtual TArray<UNiagaraScriptSourceBase*> GetAllSourceScripts() override;

	/** Get the path to the UObject of this subscriber. */
	virtual FString GetSourceObjectPathName() const override;
	//~ End INiagaraParameterDefinitionsSubscriber Interface

	FVersionedNiagaraScriptWeakPtr ToWeakPtr();
	FVersionedNiagaraScriptData* GetScriptData() const;

public:
	UNiagaraScript* Script = nullptr;

	FGuid Version;
#endif
};

/** Struct combining a script with a specific version.*/
struct NIAGARA_API FVersionedNiagaraScriptWeakPtr : public INiagaraParameterDefinitionsSubscriber
{
#if WITH_EDITORONLY_DATA
public:
	FVersionedNiagaraScriptWeakPtr(UNiagaraScript* InScript, const FGuid& InVersion)
		: Script(InScript)
		, Version(InVersion)
	{
	};

	//~ Begin INiagaraParameterDefinitionsSubscriber Interface
	virtual const TArray<FParameterDefinitionsSubscription>& GetParameterDefinitionsSubscriptions() const override { return Pin().GetScriptData()->ParameterDefinitionsSubscriptions; };
	virtual TArray<FParameterDefinitionsSubscription>& GetParameterDefinitionsSubscriptions() override { return Pin().GetScriptData()->ParameterDefinitionsSubscriptions; };

	/** Get all UNiagaraScriptSourceBase of this subscriber. */
	virtual TArray<UNiagaraScriptSourceBase*> GetAllSourceScripts() override;

	/** Get the path to the UObject of this subscriber. */
	virtual FString GetSourceObjectPathName() const override;
	//~ End INiagaraParameterDefinitionsSubscriber Interface

	const FVersionedNiagaraScript Pin() const;
	FVersionedNiagaraScript Pin();

public:
	TWeakObjectPtr<UNiagaraScript> Script;

	FGuid Version;
#endif
};
