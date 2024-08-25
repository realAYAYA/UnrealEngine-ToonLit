// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlueprintCompilationManager.h"

#include "Async/Async.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "BlueprintCompilerExtension.h"
#include "BlueprintEditorSettings.h"
#include "Blueprint/BlueprintSupport.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Components/TimelineComponent.h"
#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "Engine/Engine.h"
#include "Editor/Transactor.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/TimelineTemplate.h"
#include "FileHelpers.h"
#include "FindInBlueprintManager.h"
#include "HAL/LowLevelMemTracker.h"
#include "IMessageLogListing.h"
#include "INotifyFieldValueChanged.h"
#include "K2Node_CreateDelegate.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/KismetReinstanceUtilities.h"
#include "KismetCompiler.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/DataValidation.h"
#include "Misc/PackageAccessTrackingOps.h"
#include "ProfilingDebugging/ScopedTimers.h"
#include "Serialization/ArchiveHasReferences.h"
#include "Serialization/ArchiveReplaceObjectRef.h"
#include "ProfilingDebugging/LoadTimeTracker.h"
#include "TickableEditorObject.h"
#include "UObject/FortniteMainBranchObjectVersion.h"
#include "UObject/MetaData.h"
#include "UObject/ReferenceChainSearch.h"
#include "UObject/UObjectHash.h"
#include "Kismet2/KismetDebugUtilities.h"
#include "BlueprintEditorModule.h"
#include "Algo/TopologicalSort.h"
#include "Animation/AnimBlueprint.h"
#include "Stats/StatsHierarchical.h"
#include "UObject/PropertyBagRepository.h"
#include "UObject/OverridableManager.h"

extern UNREALED_API UUnrealEdEngine* GUnrealEd;

#define LOCTEXT_NAMESPACE "BlueprintCompilationManager"

/*
	BLUEPRINT COMPILATION MANAGER IMPLEMENTATION NOTES

	INPUTS: UBlueprint, UEdGraph, UEdGraphNode, UEdGraphPin, references to UClass, UProperties
	INTERMEDIATES: Cloned Graph, Nodes, Pins
	OUPUTS: UClass, UProperties

	The blueprint compilation manager addresses shortcomings of compilation 
	behavior (performance, correctness) that occur when compiling blueprints 
	that are inter-dependent. If you are using blueprints and there are no dependencies
	between blueprint compilation outputs and inputs, then this code is completely
	unnecessary and you can directly interface with FKismetCompilerContext and its
	derivatives.

	In order to handle compilation correctly the manager splits compilation into
	the following stages (implemented below in FlushCompilationQueueImpl):

	STAGE I: GATHER
	STAGE II: FILTER
	STAGE III: SORT
	STAGE IV: SET TEMPORARY BLUEPRINT FLAGS
	STAGE V: VALIDATE
	STAGE VI: PURGE (LOAD ONLY)
	STAGE VII: DISCARD SKELETON CDO
	STAGE VIII: RECOMPILE SKELETON
	STAGE IX: RECONSTRUCT NODES, REPLACE DEPRECATED NODES (LOAD ONLY)
	STAGE X: CREATE REINSTANCER (DISCARD 'OLD' CLASS)
	STAGE XI: CREATE UPDATED CLASS HIERARCHY
	STAGE XII: COMPILE CLASS LAYOUT
	STAGE XIII: COMPILE CLASS FUNCTIONS
	STAGE XIV: REINSTANCE
	STAGE XV: POST CDO COMPILED 
	STAGE XVI: CLEAR TEMPORARY FLAGS

	The code that implements these stages are labeled below. At some later point a final
	reinstancing operation will occur, unless the client is using CompileSynchronously, 
	in which case the expensive object graph find and replace will occur immediately
*/

// Debugging switches:
#define VERIFY_NO_STALE_CLASS_REFERENCES 0
#define VERIFY_NO_BAD_SKELETON_REFERENCES 0

struct FReinstancingJob;
struct FSkeletonFixupData;
struct FCompilerData;

struct FBPCompileRequestInternal
{
	FBPCompileRequestInternal(FBPCompileRequest InUserData)
		: UserData(InUserData)
	{
	}

	FBPCompileRequest UserData;
};

enum class EReparentClassOptions
{
	None = 0x0,

	ReplaceReferencesToOldClasses = 0x1,
};
ENUM_CLASS_FLAGS(EReparentClassOptions)

struct FBlueprintCompilationManagerImpl : public FGCObject
{
	FBlueprintCompilationManagerImpl();
	virtual ~FBlueprintCompilationManagerImpl();

	// FGCObject:
	virtual void AddReferencedObjects(FReferenceCollector& Collector);
	virtual FString GetReferencerName() const override;

	void RegisterCompilerExtension(TSubclassOf<UBlueprint> BlueprintType, UBlueprintCompilerExtension* Extension);

	void QueueForCompilation(const FBPCompileRequestInternal& CompileJob);
	void CompileSynchronouslyImpl(const FBPCompileRequestInternal& Request);
	void FlushCompilationQueueImpl(bool bSuppressBroadcastCompiled, TArray<UBlueprint*>* BlueprintsCompiled, TArray<UBlueprint*>* BlueprintsCompiledOrSkeletonCompiled, FUObjectSerializeContext* InLoadContext, TMap<UClass*, TMap<UObject*, UObject*>>* OldToNewTemplates = nullptr);
	void FixupDelegateProperties(const TArray<FCompilerData>& CurrentlyCompilingBPs);
	void ProcessExtensions(const TArray<FCompilerData>& InCurrentlyCompilingBPs);
	void FlushReinstancingQueueImpl(bool bFindAndReplaceCDOReferences = false, TMap<UClass*, TMap<UObject*, UObject*>>* OldToNewTemplates = nullptr);
	bool HasBlueprintsToCompile() const;
	bool IsGeneratedClassLayoutReady() const;
	void GetDefaultValue(const UClass* ForClass, const FProperty* Property, FString& OutDefaultValueAsString) const;
	void VerifyNoQueuedRequests(const TArray<FCompilerData>& CurrentlyCompilingBPs);

	static void ReparentHierarchies(const TMap<UClass*, UClass*>& OldClassToNewClass, EReparentClassOptions Options);
	static void BuildDSOMap(UObject* OldObject, UObject* NewObject, TMap<UObject*, UObject*>& OutOldToNewDSO);
	static void ReinstanceBatch(TArray<FReinstancingJob>& Reinstancers, TMap< UClass*, UClass* >& InOutOldToNewClassMap, FUObjectSerializeContext* InLoadContext, TMap<UClass*, TMap<UObject*, UObject*>>* OldToNewTemplates = nullptr);
	static UClass* FastGenerateSkeletonClass(UBlueprint* BP, FKismetCompilerContext& CompilerContext, bool bIsSkeletonOnly, TArray<FSkeletonFixupData>& OutSkeletonFixupData);
	static bool IsQueuedForCompilation(UBlueprint* BP);
	static void ConformToParentAndInterfaces(UBlueprint* BP);
	static void RelinkSkeleton(UClass* SkeletonToRelink);

	// Declaration of archive to fix up bytecode references of blueprints that are actively compiled:
	class FFixupBytecodeReferences : public FArchiveUObject
	{
	public:
		FFixupBytecodeReferences(UObject* InObject);

	private:
		virtual FArchive& operator<<(UObject*& Obj) override;
		virtual FArchive& operator<<(FField*& Field) override;
	};

	// Extension data, could be organized in many ways, but this provides an easy way
	// to extend blueprint compilation after the graph has been pruned and functions
	// have been generated (but before code is generated):
	TMap<TObjectPtr<UClass>, TArray<TObjectPtr<UBlueprintCompilerExtension>> > CompilerExtensions;

	// Queued requests to be processed in the next FlushCompilationQueueImpl call:
	TArray<FBPCompileRequestInternal> QueuedRequests;
	
	// Data stored for reinstancing, which finishes much later than compilation,
	// populated by FlushCompilationQueueImpl, cleared by FlushReinstancingQueueImpl:
	TMap<TObjectPtr<UClass>, TObjectPtr<UClass>> ClassesToReinstance;
	
	// Map to old default values, useful for providing access to this data throughout
	// the compilation process:
	TMap<TObjectPtr<UBlueprint>, TObjectPtr<UObject>> OldCDOs;
	
	// Blueprints that should be saved after the compilation pass is complete:
	TArray<UBlueprint*> CompiledBlueprintsToSave;

	// State stored so that we can check what stage of compilation we're in:
	bool bGeneratedClassLayoutReady;

#if WITH_EDITOR
	// Used to avoid reinstanciation on the GT while compiling on the loading thread
	FCriticalSection Lock;
#endif
};

// free function that we use to cross a module boundary (from CoreUObject to here)
void FlushReinstancingQueueImplWrapper();
void MoveSkelCDOAside(UClass* Class, TMap<UClass*, UClass*>& OldToNewMap);
void ReparentHierarchiesWrapper(const TMap<UClass*, UClass*>& OldToNewMap)
{
	FBlueprintCompilationManagerImpl::ReparentHierarchies(OldToNewMap, EReparentClassOptions::ReplaceReferencesToOldClasses);
}

FBlueprintCompilationManagerImpl::FBlueprintCompilationManagerImpl()
{
	FBlueprintSupport::SetFlushReinstancingQueueFPtr(&FlushReinstancingQueueImplWrapper);
	FBlueprintSupport::SetClassReparentingFPtr(&ReparentHierarchiesWrapper);
	bGeneratedClassLayoutReady = true;
}

FBlueprintCompilationManagerImpl::~FBlueprintCompilationManagerImpl() 
{ 
	FBlueprintSupport::SetFlushReinstancingQueueFPtr(nullptr); 
	FBlueprintSupport::SetClassReparentingFPtr(nullptr);
}

void FBlueprintCompilationManagerImpl::AddReferencedObjects(FReferenceCollector& Collector)
{
	for(auto& Extensions : CompilerExtensions)
	{
		Collector.AddReferencedObject(Extensions.Key);
		Collector.AddReferencedObjects(Extensions.Value);
	}

	for( FBPCompileRequestInternal& Job : QueuedRequests )
	{
		Collector.AddReferencedObject(Job.UserData.BPToCompile);
	}

	Collector.AddReferencedObjects(ClassesToReinstance);
	Collector.AddReferencedObjects(OldCDOs);
}

FString FBlueprintCompilationManagerImpl::GetReferencerName() const
{
	return TEXT("FBlueprintCompilationManagerImpl");
}

void FBlueprintCompilationManagerImpl::RegisterCompilerExtension(TSubclassOf<UBlueprint> BlueprintType, UBlueprintCompilerExtension* Extension)
{
	CompilerExtensions.FindOrAdd(BlueprintType).Emplace(Extension);
}

void FBlueprintCompilationManagerImpl::QueueForCompilation(const FBPCompileRequestInternal& CompileJob)
{
#if WITH_EDITOR
	FScopeLock ScopeLock(&Lock);
#endif
	if(!CompileJob.UserData.BPToCompile->bQueuedForCompilation)
	{
		if(GCompilingBlueprint)
		{
			FString CurrentlyCompiling;
			for (const TPair<TObjectPtr<UClass>, TObjectPtr<UClass>>& CompilerData : ClassesToReinstance)
			{
				if (!CompilerData.Value)
				{
					continue;
				}
				CurrentlyCompiling += CompilerData.Value->GetName() + TEXT(" ");
			}
			ensureMsgf(false, 
				TEXT("Attempting to enqueue %s for compile while compiling: %s"), 
				*CompileJob.UserData.BPToCompile->GetName(),
				*CurrentlyCompiling);
		}

		CompileJob.UserData.BPToCompile->bQueuedForCompilation = true;
		QueuedRequests.Add(CompileJob);
	}
}

void FBlueprintCompilationManagerImpl::CompileSynchronouslyImpl(const FBPCompileRequestInternal& Request)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CompileSynchronouslyImpl);

#if WITH_EDITOR
	FScopeLock ScopeLock(&Lock);
#endif
	Request.UserData.BPToCompile->bQueuedForCompilation = true;

	const bool bIsRegeneratingOnLoad		= (Request.UserData.CompileOptions & EBlueprintCompileOptions::IsRegeneratingOnLoad				) != EBlueprintCompileOptions::None;
	const bool bRegenerateSkeletonOnly		= (Request.UserData.CompileOptions & EBlueprintCompileOptions::RegenerateSkeletonOnly			) != EBlueprintCompileOptions::None;
	const bool bSkipGarbageCollection		= (Request.UserData.CompileOptions & EBlueprintCompileOptions::SkipGarbageCollection			) != EBlueprintCompileOptions::None
		|| bRegenerateSkeletonOnly;
	const bool bBatchCompile				= (Request.UserData.CompileOptions & EBlueprintCompileOptions::BatchCompile						) != EBlueprintCompileOptions::None;
	const bool bSkipReinstancing			= (Request.UserData.CompileOptions & EBlueprintCompileOptions::SkipReinstancing					) != EBlueprintCompileOptions::None;
	const bool bSkipSaving					= (Request.UserData.CompileOptions & EBlueprintCompileOptions::SkipSave							) != EBlueprintCompileOptions::None;
	const bool bFindAndReplaceCDOReferences	= (Request.UserData.CompileOptions & EBlueprintCompileOptions::IncludeCDOInReferenceReplacement	) != EBlueprintCompileOptions::None;

	ensure(!bIsRegeneratingOnLoad); // unexpected code path, compile on load handled with different function call
	ensure(!bSkipReinstancing); // This is an internal option, should not go through CompileSynchronouslyImpl
	ensure(QueuedRequests.Num() == 0);

	// Wipe the PreCompile log, any generated messages are now irrelevant
	Request.UserData.BPToCompile->PreCompileLog.Reset();

	// Reset the flag, so if the user tries to use PIE it will warn them if the BP did not compile
	Request.UserData.BPToCompile->bDisplayCompilePIEWarning = true;
	
	// Do not want to run this code without the editor present nor when running commandlets.
	// We do not want to regenerate a search Guid during loads, nothing has changed in the Blueprint
	// and it is cached elsewhere.
	// We would like to regenerated it when a skeleton changes, but it is too expensive:
	if (GEditor && GIsEditor && !bIsRegeneratingOnLoad && !bRegenerateSkeletonOnly)
	{
		FFindInBlueprintSearchManager::Get().AddOrUpdateBlueprintSearchMetadata(Request.UserData.BPToCompile);
	}

	QueuedRequests.Add(Request);

	// We suppress normal compilation broadcasts because the old code path 
	// did this after GC and we want to match the old behavior:
	const bool bSuppressBroadcastCompiled = true;
	TMap<UClass*, TMap<UObject*, UObject*>> OldToNewTemplates;
	TArray<UBlueprint*> CompiledBlueprints;
	TArray<UBlueprint*> SkeletonCompiledBlueprints;
	FlushCompilationQueueImpl(bSuppressBroadcastCompiled, &CompiledBlueprints, &SkeletonCompiledBlueprints, nullptr, bFindAndReplaceCDOReferences ? &OldToNewTemplates : nullptr);
	FlushReinstancingQueueImpl(bFindAndReplaceCDOReferences, bFindAndReplaceCDOReferences ? &OldToNewTemplates : nullptr);
	
	if (FBlueprintEditorUtils::IsLevelScriptBlueprint(Request.UserData.BPToCompile) && !bRegenerateSkeletonOnly)
	{
		// When the Blueprint is recompiled, then update the bound events for level scripting
		ULevelScriptBlueprint* LevelScriptBP = CastChecked<ULevelScriptBlueprint>(Request.UserData.BPToCompile);

		// ULevel::OnLevelScriptBlueprintChanged needs to be run after the CDO has
		// been updated as it respawns the actor:
		if (ULevel* BPLevel = LevelScriptBP->GetLevel())
		{
			// Newly created levels don't need this notification:
			if (BPLevel->GetLevelScriptBlueprint(true))
			{
				BPLevel->OnLevelScriptBlueprintChanged(LevelScriptBP);
			}
		}
	}

	if ( GEditor && !bRegenerateSkeletonOnly)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(BroadcastBlueprintReinstanced)

		// Make sure clients know they're being reinstanced as part of blueprint compilation. After this point
		// compilation is completely done:
		TGuardValue<bool> GuardTemplateNameFlag(GCompilingBlueprint, true);
		GEditor->BroadcastBlueprintReinstanced();
	}
	
	ensure(Request.UserData.BPToCompile->bQueuedForCompilation == false);

	if(!bSkipGarbageCollection)
	{
		TGuardValue<bool> GuardTemplateNameFlag(GIsGCingAfterBlueprintCompile, true);
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	}

	if (!bRegenerateSkeletonOnly)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(BroadcastChanged);

		for(UBlueprint* BP : SkeletonCompiledBlueprints)
		{
			BP->BroadcastChanged();
		}
	}
	else
	{
		ensure(SkeletonCompiledBlueprints.Num() == 1);
	}

	if (!bBatchCompile && !bRegenerateSkeletonOnly)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(BroadCastCompiled);

		for(UBlueprint* BP : SkeletonCompiledBlueprints)
		{
			BP->BroadcastCompiled();
		}

		if(GEditor)
		{
			GEditor->BroadcastBlueprintCompiled();	
		}
	}

	if (CompiledBlueprintsToSave.Num() > 0 && !bRegenerateSkeletonOnly)
	{
		if (!bSkipSaving)
		{
			TArray<UPackage*> PackagesToSave;
			for (UBlueprint* BP : CompiledBlueprintsToSave)
			{
				PackagesToSave.Add(BP->GetOutermost());
			}

			FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, /*bCheckDirty =*/true, /*bPromptToSave =*/false);
		}
		CompiledBlueprintsToSave.Empty();
	}

	// We've done our GC, so release old CDO references
	OldCDOs.Empty();
}

static double GTimeCompiling = 0.f;
static double GTimeReinstancing = 0.f;

enum class ECompilationManagerJobType
{
	Normal,
	SkeletonOnly,
	RelinkOnly,
};

// Currently only used to fix up delegate parameters on skeleton ufunctions, resolving the cyclical dependency,
// could be augmented if similar cases arise:
struct FSkeletonFixupData
{
	FSimpleMemberReference MemberReference;
	FProperty* DelegateProperty;

	template<typename T>
	static void FixUpDelegateProperty(T* DelegateProperty, const FSimpleMemberReference& MemberReference, UClass* SkeletonGeneratedClass)
	{
		check(DelegateProperty);
	
		UFunction* OldFunction = DelegateProperty->SignatureFunction;
		DelegateProperty->SignatureFunction = FMemberReference::ResolveSimpleMemberReference<UFunction>(MemberReference, SkeletonGeneratedClass);
		UStruct* Owner = DelegateProperty->template GetOwnerChecked<UStruct>();

		int32 Index = Owner->ScriptAndPropertyObjectReferences.Find(OldFunction);
		if (Index != INDEX_NONE)
		{
			Owner->ScriptAndPropertyObjectReferences[Index] = DelegateProperty->SignatureFunction.Get();
		}
	}
};

struct FCompilerData
{
	explicit FCompilerData(
		UBlueprint* InBP, 
		ECompilationManagerJobType InJobType, 
		FCompilerResultsLog* InResultsLogOverride, 
		EBlueprintCompileOptions UserOptions, 
		bool bBytecodeOnly)
	{
		check(InBP);
		BP = InBP;
		JobType = InJobType;
		UPackage* Package = BP->GetOutermost();
		bPackageWasDirty = Package ? Package->IsDirty() : false;
		OriginalBPStatus = BP->Status;

		ActiveResultsLog = InResultsLogOverride;
		if(InResultsLogOverride == nullptr)
		{
			ResultsLog = MakeUnique<FCompilerResultsLog>();
			ResultsLog->BeginEvent(TEXT("BlueprintCompilationManager Compile"));
			ResultsLog->SetSourcePath(InBP->GetPathName());
			ActiveResultsLog = ResultsLog.Get();
		}
		
		static const FBoolConfigValueHelper IgnoreCompileOnLoadErrorsOnBuildMachine(TEXT("Kismet"), TEXT("bIgnoreCompileOnLoadErrorsOnBuildMachine"), GEngineIni);
		ActiveResultsLog->bLogInfoOnly = !BP->bHasBeenRegenerated && GIsBuildMachine && IgnoreCompileOnLoadErrorsOnBuildMachine;

		InternalOptions.bRegenerateSkelton = false;
		InternalOptions.bReinstanceAndStubOnFailure = false;
		InternalOptions.bSaveIntermediateProducts = (UserOptions & EBlueprintCompileOptions::SaveIntermediateProducts) != EBlueprintCompileOptions::None;
		InternalOptions.bSkipDefaultObjectValidation = (UserOptions & EBlueprintCompileOptions::SkipDefaultObjectValidation) != EBlueprintCompileOptions::None;
		InternalOptions.bSkipFiBSearchMetaUpdate = (UserOptions & EBlueprintCompileOptions::SkipFiBSearchMetaUpdate) != EBlueprintCompileOptions::None;
		InternalOptions.bUseDeltaSerializationDuringReinstancing = (UserOptions & EBlueprintCompileOptions::UseDeltaSerializationDuringReinstancing) != EBlueprintCompileOptions::None;
		InternalOptions.bSkipNewVariableDefaultsDetection = (UserOptions & EBlueprintCompileOptions::SkipNewVariableDefaultsDetection) != EBlueprintCompileOptions::None;
		InternalOptions.CompileType = bBytecodeOnly ? EKismetCompileType::BytecodeOnly : EKismetCompileType::Full;

		Compiler = FKismetCompilerContext::GetCompilerForBP(BP, *ActiveResultsLog, InternalOptions);
	}

	bool IsSkeletonOnly() const { return JobType == ECompilationManagerJobType::SkeletonOnly; }
	bool ShouldSetTemporaryBlueprintFlags() const { return JobType != ECompilationManagerJobType::RelinkOnly; }
	bool ShouldResetErrorState() const { return JobType == ECompilationManagerJobType::Normal && InternalOptions.CompileType != EKismetCompileType::BytecodeOnly; }
	bool ShouldValidate() const { return JobType == ECompilationManagerJobType::Normal; }
	bool ShouldRegenerateSkeleton() const { return JobType != ECompilationManagerJobType::RelinkOnly; }
	bool ShouldMarkUpToDateAfterSkeletonStage() const { return IsSkeletonOnly(); }
	bool ShouldReconstructNodes() const { return JobType == ECompilationManagerJobType::Normal || (!IsSkeletonOnly() && BP->bIsRegeneratingOnLoad); }
	bool ShouldSkipReinstancerCreation() const { return (IsSkeletonOnly() && (!BP->ParentClass || BP->ParentClass->IsNative())); }
	bool ShouldInitiateReinstancing() const { return JobType == ECompilationManagerJobType::Normal || BP->bIsRegeneratingOnLoad; }
	bool ShouldCompileClassLayout() const { return JobType == ECompilationManagerJobType::Normal; }
	bool ShouldCompileClassFunctions() const { return JobType == ECompilationManagerJobType::Normal; }
	bool ShouldRegisterCompilerResults() const { return JobType == ECompilationManagerJobType::Normal; }
	bool ShouldSkipIfDependenciesAreUnchanged() const { return InternalOptions.CompileType == EKismetCompileType::BytecodeOnly || JobType == ECompilationManagerJobType::RelinkOnly; }
	bool ShouldValidateClassDefaultObject() const { return JobType == ECompilationManagerJobType::Normal && !InternalOptions.bSkipDefaultObjectValidation; }
	bool ShouldUpdateBlueprintSearchMetadata() const { return JobType == ECompilationManagerJobType::Normal && !InternalOptions.bSkipFiBSearchMetaUpdate; }
	bool UseDeltaSerializationDuringReinstancing() const { return InternalOptions.bUseDeltaSerializationDuringReinstancing; }
	bool ShouldSkipNewVariableDefaultsDetection() const { return InternalOptions.bSkipNewVariableDefaultsDetection; }
	
	UBlueprint* BP;
	FCompilerResultsLog* ActiveResultsLog;
	TUniquePtr<FCompilerResultsLog> ResultsLog;
	TSharedPtr<FKismetCompilerContext> Compiler;
	FKismetCompilerOptions InternalOptions;
	TSharedPtr<FBlueprintCompileReinstancer> Reinstancer;
	TArray<FSkeletonFixupData> SkeletonFixupData;
	/** variables that are new to the generated class and will need their default set (can occur when a new variable is added to a BP's ancestor class) */
	TArray<FBPVariableDescription> NewDefaultVariables;	

	ECompilationManagerJobType JobType;
	bool bPackageWasDirty;
	EBlueprintStatus OriginalBPStatus;
};

struct FReinstancingJob
{
	FReinstancingJob(TSharedPtr<FBlueprintCompileReinstancer> InReinstancer);
	FReinstancingJob(TSharedPtr<FBlueprintCompileReinstancer> InReinstancer, TSharedPtr<FKismetCompilerContext> InCompiler);
	FReinstancingJob(TPair<UClass*, UClass*> InOldToNew);

	// optional:
	TSharedPtr<FBlueprintCompileReinstancer> Reinstancer;
	TSharedPtr<FKismetCompilerContext> Compiler;

	// always set:
	TPair<UClass*, UClass*> OldToNew;

	struct FArchetypeInfo
	{
		FArchetypeInfo(UObject* Archetype)
			: Archetype(Archetype)
			, ArchetypeTemplate(Archetype ? Archetype->GetArchetype() : nullptr)
		{}

		bool operator==(const FArchetypeInfo& Other) const
		{
			return Archetype == Other.Archetype;
		}

		friend int32 GetTypeHash(const FArchetypeInfo& Info)
		{
			return GetTypeHash(Info.Archetype);
		}

		UObject* Archetype = nullptr;
		UObject* ArchetypeTemplate;
	};


	// Old archetype to re-instantiate and its old associated template
	TArray<FArchetypeInfo> OldArchetypeObjects;
};

FReinstancingJob::FReinstancingJob(TSharedPtr<FBlueprintCompileReinstancer> InReinstancer)
	: Reinstancer(InReinstancer)
	, Compiler()
	, OldToNew(nullptr, nullptr)
{
	check(InReinstancer.IsValid());
	OldToNew.Key = InReinstancer->DuplicatedClass;
	OldToNew.Value = InReinstancer->ClassToReinstance;
}

FReinstancingJob::FReinstancingJob(TSharedPtr<FBlueprintCompileReinstancer> InReinstancer, TSharedPtr<FKismetCompilerContext> InCompiler)
	: Reinstancer(InReinstancer)
	, Compiler(InCompiler)
	, OldToNew(nullptr, nullptr)
{
	check(InReinstancer.IsValid());
	OldToNew.Key = InReinstancer->DuplicatedClass;
	OldToNew.Value = InReinstancer->ClassToReinstance;
}

FReinstancingJob::FReinstancingJob(TPair<UClass*, UClass*> InOldToNew)
	: Reinstancer()
	, Compiler()
	, OldToNew(InOldToNew)
{
}

namespace UE::Kismet::BlueprintCompilationManager::Private
{
	namespace ConsoleVariables
	{
		/** Flag to use the new FBlueprintCompileReinstancer::MoveDependentSkelToReinst and avoid problematic recursion during reinstancing */
		static bool bEnableSkelReinstUpdate = true;
		static FAutoConsoleVariableRef CVarEnableSkelReinstUpdate(
			TEXT("BP.bEnableSkelReinstUpdate"), bEnableSkelReinstUpdate,
			TEXT("If true the Reinstancing of SKEL classes will use the new FBlueprintCompileReinstancer::MoveDependentSkelToReinst(o(n)) instead of the old MoveSkelCDOAside (o(n^2))"),
			ECVF_Default);

		/** Flag to disable faster compiles for individual blueprints if they have no function signature changes */
		static bool bForceAllDependenciesToRecompile = false;
		static FAutoConsoleVariableRef CVarForceAllDependenciesToRecompile(
			TEXT("BP.bForceAllDependenciesToRecompile"), bForceAllDependenciesToRecompile,
			TEXT("If true all dependencies will be bytecode-compiled even when all referenced functions have no signature changes. Intended for compiler development/debugging purposes."),
			ECVF_Default);
	}
}

void FBlueprintCompilationManagerImpl::FlushCompilationQueueImpl(bool bSuppressBroadcastCompiled, TArray<UBlueprint*>* BlueprintsCompiled, TArray<UBlueprint*>* BlueprintsCompiledOrSkeletonCompiled, FUObjectSerializeContext* InLoadContext, TMap<UClass*, TMap<UObject*, UObject*>>* OldToNewTemplates /* = nullptr*/)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FlushCompilationQueueImpl);

#if WITH_EDITOR
	FScopeLock ScopeLock(&Lock);
#endif

	TGuardValue<bool> GuardTemplateNameFlag(GCompilingBlueprint, true);
	ensure(bGeneratedClassLayoutReady);

	if( QueuedRequests.Num() == 0 )
	{
		return;
	}

	FScopedSlowTask SlowTask(17.f /* Number of steps */, LOCTEXT("FlushCompilationQueue", "Compiling blueprints..."));
	SlowTask.MakeDialogDelayed(1.0f);

	TArray<FCompilerData> CurrentlyCompilingBPs;
	{ // begin GTimeCompiling scope 
		FScopedDurationTimer SetupTimer(GTimeCompiling); 

		// STAGE I: Add any related blueprints that were not compiled, then add any children so that they will be relinked:
		TArray<UBlueprint*> BlueprintsToRecompile;

		// First add any dependents of macro libraries that are being compiled:
		for(const FBPCompileRequestInternal& CompileJob : QueuedRequests)
		{
			if ((CompileJob.UserData.CompileOptions & 
				(	EBlueprintCompileOptions::RegenerateSkeletonOnly)
				) != EBlueprintCompileOptions::None)
			{
				continue;
			}

			UBlueprint* BP = CompileJob.UserData.BPToCompile;

			if(!BP->bHasBeenRegenerated && BP->GetLinker())
			{
				// we may have cached dependencies before being fully loaded:
				BP->bCachedDependenciesUpToDate = false;
			}

			const bool bWasDependencyCacheOutOfDate = !BP->bCachedDependenciesUpToDate;

			FBlueprintEditorUtils::EnsureCachedDependenciesUpToDate(BP);

			if ((CompileJob.UserData.CompileOptions & 
				(	EBlueprintCompileOptions::IsRegeneratingOnLoad)
				) != EBlueprintCompileOptions::None)
			{
				continue;
			}
			
			if(BP->BlueprintType == BPTYPE_MacroLibrary)
			{
				TArray<UBlueprint*> DependentBlueprints;
				FBlueprintEditorUtils::GetDependentBlueprints(BP, DependentBlueprints);
				for(UBlueprint* DependentBlueprint : DependentBlueprints)
				{
					if(!IsQueuedForCompilation(DependentBlueprint))
					{
						// The macro may have updated its dependency cache above; if so, we'll need to regenerate the dependent's set as well.
						DependentBlueprint->bCachedDependenciesUpToDate &= !bWasDependencyCacheOutOfDate;

						DependentBlueprint->bQueuedForCompilation = true;
						CurrentlyCompilingBPs.Emplace(
							FCompilerData(
								DependentBlueprint, 
								ECompilationManagerJobType::Normal, 
								nullptr, 
								EBlueprintCompileOptions::None,
								false // full compile
							)
						);
						BlueprintsToRecompile.Add(DependentBlueprint);
					}
				}
			}
		}

		SlowTask.EnterProgressFrame();

		// then make sure any normal blueprints have their bytecode dependents recompiled, this is in case a function signature changes:
		for(const FBPCompileRequestInternal& CompileJob : QueuedRequests)
		{
			if ((CompileJob.UserData.CompileOptions & EBlueprintCompileOptions::RegenerateSkeletonOnly) != EBlueprintCompileOptions::None)
			{
				continue;
			}

			// Add any dependent blueprints for a bytecode compile, this is needed because we 
			// have no way to keep bytecode safe when a function is renamed or parameters are
			// added or removed. Below (Stage VIII) we skip further compilation for blueprints 
			// that are being bytecode compiled, but their dependencies have not changed:
			TArray<UBlueprint*> DependentBlueprints;
			FBlueprintEditorUtils::GetDependentBlueprints(CompileJob.UserData.BPToCompile, DependentBlueprints);
			for(UBlueprint* DependentBlueprint : DependentBlueprints)
			{
				if(!IsQueuedForCompilation(DependentBlueprint))
				{
					DependentBlueprint->bQueuedForCompilation = true;
					// Because we're adding this as a bytecode only blueprint compile we don't need to 
					// recursively recompile dependencies. The assumption is that a bytecode only compile
					// will not change the class layout. @todo: add an ensure to detect class layout changes
					CurrentlyCompilingBPs.Emplace(
						FCompilerData(
							DependentBlueprint, 
							ECompilationManagerJobType::Normal, 
							nullptr, 
							EBlueprintCompileOptions::None,
							true
						)
					);
					BlueprintsToRecompile.Add(DependentBlueprint);
				}
			}
		}

		SlowTask.EnterProgressFrame();

		// STAGE II: Filter out data only and interface blueprints:
		for(int32 I = 0; I < QueuedRequests.Num(); ++I)
		{
			FBPCompileRequestInternal& QueuedJob = QueuedRequests[I];
			UBlueprint* QueuedBP = QueuedJob.UserData.BPToCompile;

			ensure(!QueuedBP->GeneratedClass ||
				!QueuedBP->GeneratedClass->ClassDefaultObject ||
				!(QueuedBP->GeneratedClass->ClassDefaultObject->HasAnyFlags(RF_NeedLoad)));
			bool bDefaultComponentMustBeAdded = false;
			bool bHasPendingUberGraphFrame = false;
			UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(QueuedBP->GeneratedClass);

			if(BPGC)
			{
				if( BPGC->SimpleConstructionScript &&
					BPGC->SimpleConstructionScript->GetSceneRootComponentTemplate(true) == nullptr)
				{
					bDefaultComponentMustBeAdded = true;
				}

				bHasPendingUberGraphFrame = BPGC->UberGraphFramePointerProperty || BPGC->UberGraphFunction;
			}

			bool bSkipCompile = false;
			const UClass* ParentClass = QueuedBP->ParentClass;
			const bool bHasClassAndMatchesParent = BPGC && BPGC->GetSuperClass() == ParentClass;
			if( bHasClassAndMatchesParent &&
				FBlueprintEditorUtils::IsDataOnlyBlueprint(QueuedBP) && !QueuedBP->bHasBeenRegenerated && 
				QueuedBP->GetLinker() && !bDefaultComponentMustBeAdded && !bHasPendingUberGraphFrame )
			{
				// consider skipping the compile operation for this DOB:
				if (ParentClass && ParentClass->HasAllClassFlags(CLASS_Native))
				{
					bSkipCompile = true;
				}
				else if (const UClass* CurrentClass = QueuedBP->GeneratedClass)
				{
					if(FStructUtils::TheSameLayout(CurrentClass, CurrentClass->GetSuperStruct()))
					{
						bSkipCompile = true;
					}
				}
			}

			if(bSkipCompile)
			{
				CurrentlyCompilingBPs.Emplace(
					FCompilerData(
						QueuedBP, 
						ECompilationManagerJobType::SkeletonOnly, 
						QueuedJob.UserData.ClientResultsLog, 
						QueuedJob.UserData.CompileOptions,
						false
					)
				);
				if (QueuedBP->GeneratedClass != nullptr)
				{
					// set bIsRegeneratingOnLoad so that we don't reset loaders:
					QueuedBP->bIsRegeneratingOnLoad = true;
					FBlueprintEditorUtils::RemoveStaleFunctions(Cast<UBlueprintGeneratedClass>(QueuedBP->GeneratedClass), QueuedBP);
					QueuedBP->bIsRegeneratingOnLoad = false;
				}

				// No actual compilation work to be done, but try to conform the class and fix up anything that might need to be updated if the native base class has changed in any way
				FKismetEditorUtilities::ConformBlueprintFlagsAndComponents(QueuedBP);

				if (QueuedBP->GeneratedClass)
				{
					FBlueprintEditorUtils::RecreateClassMetaData(QueuedBP, QueuedBP->GeneratedClass, true);
				}

				QueuedRequests.RemoveAtSwap(I);
				--I;
			}
			else
			{
				ECompilationManagerJobType JobType = ECompilationManagerJobType::Normal;
				if ((QueuedJob.UserData.CompileOptions & EBlueprintCompileOptions::RegenerateSkeletonOnly) != EBlueprintCompileOptions::None)
				{
					JobType = ECompilationManagerJobType::SkeletonOnly;
				}

				CurrentlyCompilingBPs.Emplace(
					FCompilerData(
						QueuedBP, 
						JobType, 
						QueuedJob.UserData.ClientResultsLog, 
						QueuedJob.UserData.CompileOptions, 
						false
					)
				);

				BlueprintsToRecompile.Add(QueuedBP);
			}
		}

		SlowTask.EnterProgressFrame();

		for(UBlueprint* BP : BlueprintsToRecompile)
		{
			// make sure all children are at least re-linked:
			if(UClass* OldSkeletonClass = BP->SkeletonGeneratedClass)
			{
				TArray<UClass*> SkeletonClassesToReparentList;
				// Has to be recursive gather of children because instances of a UClass will cache information about
				// classes that are above their immediate parent (e.g. ClassConstructor):
				GetDerivedClasses(OldSkeletonClass, SkeletonClassesToReparentList);
		
				for(UClass* ChildClass : SkeletonClassesToReparentList)
				{
					if(UBlueprint* ChildBlueprint = UBlueprint::GetBlueprintFromClass(ChildClass))
					{
						if(!IsQueuedForCompilation(ChildBlueprint))
						{
							ChildBlueprint->bQueuedForCompilation = true;
							ensure(ChildBlueprint->bHasBeenRegenerated);
							CurrentlyCompilingBPs.Emplace(
								FCompilerData(
									ChildBlueprint, 
									ECompilationManagerJobType::RelinkOnly, 
									nullptr, 
									EBlueprintCompileOptions::None,
									false
								)
							);
						}
					}
				}
			}

			// Don't report progress for substep but gives a chance to tick slate to improve the responsiveness of the 
			// progress bar being shown. We expect slate to be ticked at regular intervals throughout the loading.
			SlowTask.TickProgress();
		}

		/*	Prevent 'pending kill' blueprints from being recompiled. Dependency
			gathering is currently done for the following reasons:
			 * Update a caller's called functions when they are recreated
			 * Update a child type's cached information about its superclass
			 * Update a child type's class layout when a parent type layout changes
			 * Update a reader/writers references to member variables when member variables are recreated
		
			Pending kill objects do not need these updates and StaticDuplicateObject
			cannot duplicate them - so they cannot be updated as normal, anyway.

			Ultimately pending kill UBlueprintGeneratedClass instances rely on the GetDerivedClasses/ReparentChild
			calls in FBlueprintCompileReinstancer() to maintain accurate class layouts so that we 
			don't leak or scribble memory.
		*/
		CurrentlyCompilingBPs.RemoveAll(
			[](FCompilerData& Data)
			{ 
				if(!IsValid(Data.BP))
				{
					check(!Data.BP->bBeingCompiled);
					check(Data.BP->CurrentMessageLog == nullptr);
					if(UPackage* Package = Data.BP->GetOutermost())
					{
						Package->SetDirtyFlag(Data.bPackageWasDirty);
					}
					if(Data.ResultsLog)
					{
						Data.ResultsLog->EndEvent();
					}
					Data.BP->bQueuedForCompilation = false;
					return true;
				}
				return false;
			}
		);

		BlueprintsToRecompile.Empty();
		QueuedRequests.Empty();

		SlowTask.EnterProgressFrame();

		// STAGE III: Sort into correct compilation order. We want to compile root types before their derived (child) types:
		auto HierarchyDepthSortFn = [](const FCompilerData& CompilerDataA, const FCompilerData& CompilerDataB)
		{
			UBlueprint& A = *(CompilerDataA.BP);
			UBlueprint& B = *(CompilerDataB.BP);

			bool bAIsInterface = FBlueprintEditorUtils::IsInterfaceBlueprint(&A);
			bool bBIsInterface = FBlueprintEditorUtils::IsInterfaceBlueprint(&B);

			if(bAIsInterface && !bBIsInterface)
			{
				return true;
			}
			else if(bBIsInterface && !bAIsInterface)
			{
				return false;
			}

			return FBlueprintCompileReinstancer::ReinstancerOrderingFunction(A.GeneratedClass, B.GeneratedClass);
		};
		CurrentlyCompilingBPs.Sort( HierarchyDepthSortFn );

		SlowTask.EnterProgressFrame();

		// STAGE IV: Set UBlueprint flags (bBeingCompiled, bIsRegeneratingOnLoad)
		for (FCompilerData& CompilerData : CurrentlyCompilingBPs)
		{
			if (!CompilerData.ShouldSetTemporaryBlueprintFlags())
			{
				continue;
			}

			UBlueprint* BP = CompilerData.BP;
			BP->bBeingCompiled = true;
			BP->CurrentMessageLog = CompilerData.ActiveResultsLog;
			BP->bIsRegeneratingOnLoad = !BP->bHasBeenRegenerated && BP->GetLinker();

			if(CompilerData.ShouldResetErrorState())
			{
				TArray<UEdGraph*> AllGraphs;
				BP->GetAllGraphs(AllGraphs);
				for (UEdGraph* Graph : AllGraphs )
				{
					for (UEdGraphNode* GraphNode : Graph->Nodes)
					{
						if (GraphNode)
						{
							GraphNode->ClearCompilerMessage();
						}
					}
				}
			}
		}

		SlowTask.EnterProgressFrame();

		// STAGE V: Validate
		for (FCompilerData& CompilerData : CurrentlyCompilingBPs)
		{
			if(!CompilerData.ShouldValidate())
			{
				continue;
			}

			CompilerData.Compiler->ValidateVariableNames();
			CompilerData.Compiler->ValidateClassPropertyDefaults();
		}

		SlowTask.EnterProgressFrame();

		// STAGE V (phase 2): Give the blueprint the possibility for edits
		for (FCompilerData& CompilerData : CurrentlyCompilingBPs)
		{
			UBlueprint* BP = CompilerData.BP;
			if (BP->bIsRegeneratingOnLoad)
			{
				FKismetCompilerContext& CompilerContext = *(CompilerData.Compiler);
				CompilerContext.PreCompileUpdateBlueprintOnLoad(BP);
			}
		}

		SlowTask.EnterProgressFrame();

		// STAGE VI: Purge null graphs, misc. data fixup
		for (FCompilerData& CompilerData : CurrentlyCompilingBPs)
		{
			UBlueprint* BP = CompilerData.BP;
			if(BP->bIsRegeneratingOnLoad)
			{
				FBlueprintEditorUtils::PurgeNullGraphs(BP);
				BP->ConformNativeComponents();
				if (FLinkerLoad* Linker = BP->GetLinker())
				{
					if (Linker->UEVer() < VER_UE4_EDITORONLY_BLUEPRINTS)
					{
						BP->ChangeOwnerOfTemplates();
					}
				}
			}
		}

		SlowTask.EnterProgressFrame();

		// STAGE VII: safely throw away old skeleton CDOs:
		{
			using namespace UE::Kismet::BlueprintCompilationManager;

			TMap<UClass*, UClass*> NewSkeletonToOldSkeleton;
			for (FCompilerData& CompilerData : CurrentlyCompilingBPs)
			{
				UBlueprint* BP = CompilerData.BP;
				UClass* OldSkeletonClass = BP->SkeletonGeneratedClass;
				if(OldSkeletonClass)
				{
					if (Private::ConsoleVariables::bEnableSkelReinstUpdate)
					{
						TRACE_CPUPROFILER_EVENT_SCOPE(MoveDependentSkelToReinst);

						FBlueprintCompileReinstancer::MoveDependentSkelToReinst(OldSkeletonClass, NewSkeletonToOldSkeleton);
					}
					else
					{
						// Old code path
						MoveSkelCDOAside(OldSkeletonClass, NewSkeletonToOldSkeleton);
					}
				}
			}
		
		
			// STAGE VIII: recompile skeleton

			// if any function signatures have changed in this skeleton class we will need to recompile all dependencies, but if not
			// then we can avoid dependency recompilation:
			bool bSkipUnneededDependencyCompilation = !Private::ConsoleVariables::bForceAllDependenciesToRecompile;
			TSet<UObject*> OldFunctionsWithSignatureChanges;

			for (FCompilerData& CompilerData : CurrentlyCompilingBPs)
			{
				UBlueprint* BP = CompilerData.BP;
		
				if(CompilerData.ShouldRegenerateSkeleton())
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(RecompileSkeleton);
					SCOPED_LOADTIMER_ASSET_TEXT(*BP->GetPathName());

					if(BlueprintsCompiledOrSkeletonCompiled)
					{
						BlueprintsCompiledOrSkeletonCompiled->Add(BP);
					}

					BP->SkeletonGeneratedClass = FastGenerateSkeletonClass(BP, *(CompilerData.Compiler), CompilerData.IsSkeletonOnly(), CompilerData.SkeletonFixupData);
					UBlueprintGeneratedClass* AuthoritativeClass = Cast<UBlueprintGeneratedClass>(BP->GeneratedClass);
					if(AuthoritativeClass && bSkipUnneededDependencyCompilation)
					{
						if(CompilerData.InternalOptions.CompileType == EKismetCompileType::Full )
						{
							for (TFieldIterator<UFunction> FuncIt(AuthoritativeClass, EFieldIteratorFlags::ExcludeSuper); FuncIt; ++FuncIt)
							{
								UFunction* OldFunction = *FuncIt;

								if(!OldFunction->HasAnyFunctionFlags(EFunctionFlags::FUNC_BlueprintCallable))
								{
									continue;
								}

								// We assume that if the func is FUNC_BlueprintCallable that it will be present in the Skeleton class.
								// If it is not in the skeleton class we will always think that this blueprints public interface has 
								// changed. Not a huge deal, but will mean we recompile dependencies more often than necessary.
								UFunction* NewFunction = BP->SkeletonGeneratedClass->FindFunctionByName((OldFunction)->GetFName());
								if(	NewFunction == nullptr || 
									!NewFunction->IsSignatureCompatibleWith(OldFunction) || 
									// If a function changes its net flags, callers may now need to do a full EX_FinalFunction/EX_VirtualFunction 
									// instead of a EX_LocalFinalFunction/EX_LocalVirtualFunction:
									NewFunction->HasAnyFunctionFlags(FUNC_NetFuncFlags) != OldFunction->HasAnyFunctionFlags(FUNC_NetFuncFlags))
								{
									OldFunctionsWithSignatureChanges.Add(OldFunction);
									break;
								}
							}
						}
					}
				}
				else
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(RelinkSkeleton);

					// Just relink, note that UProperties that reference *other* types may be stale until
					// we fixup below:
					RelinkSkeleton(BP->SkeletonGeneratedClass);
				}

				if(CompilerData.ShouldMarkUpToDateAfterSkeletonStage())
				{
					// Flag data only blueprints as being up-to-date
					BP->Status = BP->bHasBeenRegenerated ? CompilerData.OriginalBPStatus : BS_UpToDate;
					BP->bHasBeenRegenerated = true;
					if (BP->GeneratedClass)
					{
						BP->GeneratedClass->ClearFunctionMapsCaches();
					}
				}
			}

			// Fix up delegate parameters on skeleton class UFunctions, as they have a direct reference to a UFunction*
			// that may have been created as part of skeleton generation:
			for (FCompilerData& CompilerData : CurrentlyCompilingBPs)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FixUpDelegateParameters);

				UBlueprint* BP = CompilerData.BP;
				TArray<FSkeletonFixupData>& ParamsToFix = CompilerData.SkeletonFixupData;
				for( const FSkeletonFixupData& FixupData : ParamsToFix )
				{
					if(FDelegateProperty* DelegateProperty = CastField<FDelegateProperty>(FixupData.DelegateProperty))
					{
						FSkeletonFixupData::FixUpDelegateProperty(DelegateProperty, FixupData.MemberReference, BP->SkeletonGeneratedClass);
					}
					else if(FMulticastDelegateProperty* MCDelegateProperty = CastField<FMulticastDelegateProperty>(FixupData.DelegateProperty))
					{
						FSkeletonFixupData::FixUpDelegateProperty(MCDelegateProperty, FixupData.MemberReference, BP->SkeletonGeneratedClass);
					}
				}
			}

			// Skip further compilation for blueprints that are being bytecode compiled as a dependency of something that has
			// not had a change in its function parameters:
			if(bSkipUnneededDependencyCompilation)
			{
				const auto HasNoReferencesToChangedFunctions = [&OldFunctionsWithSignatureChanges](FCompilerData& Data)
				{
					if(!Data.ShouldSkipIfDependenciesAreUnchanged())
					{
						return false;
					}

					// Anim BPs cannot skip un-needed dependency compilation as their property access bytecode
					// will need refreshing due to external class layouts changing (they require at least a bytecode recompile or a relink)
					const bool bIsAnimBlueprintClass = !!Cast<UAnimBlueprint>(Data.BP);
					if(bIsAnimBlueprintClass)
					{
						return false;
					}
					
					// if our parent is still being compiled, then we still need to be compiled:
					UClass* Iter = Data.BP->ParentClass;
					while(Iter)
					{
						if(UBlueprint* BP = Cast<UBlueprint>(Iter->ClassGeneratedBy))
						{
							if(BP->bBeingCompiled)
							{
								return false;
							}
						}
						Iter = Iter->GetSuperClass();
					}

					// look for references to a function with a signature change
					// in the old class, if it has none, we can skip bytecode recompile:
					bool bHasNoReferencesToChangedFunctions = true;
					UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Data.BP->GeneratedClass);
					if(BPGC)
					{
						for(UFunction* CalledFunction : BPGC->CalledFunctions)
						{
							if(OldFunctionsWithSignatureChanges.Contains(CalledFunction))
							{
								bHasNoReferencesToChangedFunctions = false;
								break;
							}
						}
					}

					if(bHasNoReferencesToChangedFunctions)
					{
						// This BP is not actually going to be compiled, clean it up:
						Data.BP->bBeingCompiled = false;
						Data.BP->CurrentMessageLog = nullptr;
						if(UPackage* Package = Data.BP->GetOutermost())
						{
							Package->SetDirtyFlag(Data.bPackageWasDirty);
						}
						if(Data.ResultsLog)
						{
							Data.ResultsLog->EndEvent();
						}
						Data.BP->bQueuedForCompilation = false;
					}

					return bHasNoReferencesToChangedFunctions;
				};

				// Order very much matters, but we could RemoveAllSwap and re-sort:
				CurrentlyCompilingBPs.RemoveAll(HasNoReferencesToChangedFunctions);
			}
		}
		
		// Detect any variable-based properties that are not in the old generated class, save them for after reinstancing. This can occur 
		//    when a new variable is introduced in an ancestor class, and we'll need to use its default as our generated class's initial value.
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(DetectNewDefaultVariables);

			for (FCompilerData& CompilerData : CurrentlyCompilingBPs)
			{
				if (CompilerData.JobType == ECompilationManagerJobType::Normal &&
					 CompilerData.BP->bHasBeenRegenerated &&		// Note: This ensures that we'll only do this after the Blueprint has been loaded/created; otherwise the class may not contain any properties to find.
					 CompilerData.BP->GeneratedClass &&
					 !CompilerData.ShouldSkipNewVariableDefaultsDetection())
				{
					const UClass* ParentClass = CompilerData.BP->ParentClass;
					while (const UBlueprint* ParentBP = UBlueprint::GetBlueprintFromClass(ParentClass))
					{
						for (const FBPVariableDescription& ParentBPVarDesc : ParentBP->NewVariables)
						{
							if (!CompilerData.BP->GeneratedClass->FindPropertyByName(ParentBPVarDesc.VarName))
							{
								CompilerData.NewDefaultVariables.Add(ParentBPVarDesc);
							}
						}

						ParentClass = ParentBP->ParentClass;
					}
				}
			}
		}

		SlowTask.EnterProgressFrame();

		// STAGE IX: Reconstruct nodes and replace deprecated nodes, then broadcast 'precompile
		for (FCompilerData& CompilerData : CurrentlyCompilingBPs)
		{
			if(!CompilerData.ShouldReconstructNodes())
			{
				continue;
			}

			UBlueprint* BP = CompilerData.BP;
			TRACE_CPUPROFILER_EVENT_SCOPE(ReconstructNodes);
			SCOPED_LOADTIMER_ASSET_TEXT(*BP->GetPathName());
			UE_TRACK_REFERENCING_PACKAGE_SCOPED(BP, PackageAccessTrackingOps::NAME_CookerBuildObject);

			ConformToParentAndInterfaces(BP);

			// Some nodes are set up to do things during reconstruction only when this flag is NOT set.
			if(BP->bIsRegeneratingOnLoad)
			{
				FBlueprintEditorUtils::ReconstructAllNodes(BP);
				FBlueprintEditorUtils::ReplaceDeprecatedNodes(BP);
			}
			else
			{
				// matching existing behavior, when compiling a BP not on load we refresh nodes
				// before compiling:
				FBlueprintCompileReinstancer::OptionallyRefreshNodes(BP);
				TArray<UBlueprint*> DependentBlueprints;
				FBlueprintEditorUtils::GetDependentBlueprints(BP, DependentBlueprints);

				for (UBlueprint* CurrentBP : DependentBlueprints)
				{
					const EBlueprintStatus OriginalStatus = CurrentBP->Status;
					UPackage* const Package = CurrentBP->GetOutermost();
					const bool bStartedWithUnsavedChanges = Package != nullptr ? Package->IsDirty() : true;

					FBlueprintEditorUtils::RefreshExternalBlueprintDependencyNodes(CurrentBP, BP->GeneratedClass);

					CurrentBP->Status = OriginalStatus;
					if(Package != nullptr && Package->IsDirty() && !bStartedWithUnsavedChanges)
					{
						Package->SetDirtyFlag(false);
					}
				}
			}
			
			// Broadcast pre-compile
			{
				if(GEditor && GIsEditor)
				{
					GEditor->BroadcastBlueprintPreCompile(BP);
				}
			}

			if (CompilerData.ShouldUpdateBlueprintSearchMetadata())
			{
				// Do not want to run this code without the editor present nor when running commandlets.
				if (GEditor && GIsEditor)
				{
					// We do not want to regenerate a search Guid during loads, nothing has changed in the Blueprint and it is cached elsewhere
					if (!BP->bIsRegeneratingOnLoad)
					{
						FFindInBlueprintSearchManager::Get().AddOrUpdateBlueprintSearchMetadata(BP);
					}
				}
			}

			// we are regenerated, tag ourself as such so that
			// old logic to 'fix' circular dependencies doesn't
			// cause redundant regeneration (e.g. bForceRegenNodes
			// in ExpandTunnelsAndMacros):
			BP->bHasBeenRegenerated = true;
		}
	
		SlowTask.EnterProgressFrame();

		// STAGE X: reinstance every blueprint that is queued, note that this means classes in the hierarchy that are *not* being 
		// compiled will be parented to REINST versions of the class, so type checks (IsA, etc) involving those types
		// will be incoherent!
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(ReinstanceQueued);
			for (FCompilerData& CompilerData : CurrentlyCompilingBPs)
			{
				// we including skeleton only compilation jobs for reinstancing because we need UpdateCustomPropertyListForPostConstruction
				// to happen (at the right time) for those generated classes as well. This means we *don't* need to reinstance if 
				// the parent is a native type (unless we hot reload, but that should not need to be handled here):
				if(CompilerData.ShouldSkipReinstancerCreation())
				{
					continue;
				}

				// no need to reinstance skeleton or relink jobs that are not in a hierarchy that has had reinstancing initiated:
				bool bRequiresReinstance = CompilerData.ShouldInitiateReinstancing();
				if (!bRequiresReinstance)
				{
					UClass* Iter = CompilerData.BP->GeneratedClass;
					if (!Iter)
					{
						bRequiresReinstance = true;
					}
					while (Iter)
					{
						if (Iter->HasAnyClassFlags(CLASS_NewerVersionExists))
						{
							bRequiresReinstance = true;
							break;
						}

						Iter = Iter->GetSuperClass();
					}
				}

				if (!bRequiresReinstance)
				{
					continue;
				}

				UBlueprint* BP = CompilerData.BP;
				SCOPED_LOADTIMER_ASSET_TEXT(*BP->GetPathName());

				if(BP->GeneratedClass)
				{
					OldCDOs.Add(BP, BP->GeneratedClass->ClassDefaultObject);
				}

				EBlueprintCompileReinstancerFlags CompileReinstancerFlags =
					EBlueprintCompileReinstancerFlags::AutoInferSaveOnCompile
					| EBlueprintCompileReinstancerFlags::AvoidCDODuplication;

				if (CompilerData.UseDeltaSerializationDuringReinstancing())
				{
					CompileReinstancerFlags |= EBlueprintCompileReinstancerFlags::UseDeltaSerialization;
				}

				CompilerData.Reinstancer = TSharedPtr<FBlueprintCompileReinstancer>(
					new FBlueprintCompileReinstancer(
						BP->GeneratedClass,
						CompileReinstancerFlags
					)
				);

				if(CompilerData.Compiler.IsValid())
				{
					CompilerData.Compiler->OldClass = Cast<UBlueprintGeneratedClass>(CompilerData.Reinstancer->DuplicatedClass);
				}

				if(BP->GeneratedClass)
				{
					BP->GeneratedClass->bLayoutChanging = true;
					CompilerData.Reinstancer->SaveSparseClassData(BP->GeneratedClass);
				}
			}
		}

		SlowTask.EnterProgressFrame();

		// STAGE XI: Reinstancing done, lets fix up child->parent pointers and take ownership of SCD:
		for (FCompilerData& CompilerData : CurrentlyCompilingBPs)
		{
			UBlueprint* BP = CompilerData.BP;
			if(BP->GeneratedClass && BP->GeneratedClass->GetSuperClass()->HasAnyClassFlags(CLASS_NewerVersionExists))
			{
				BP->GeneratedClass->SetSuperStruct(BP->GeneratedClass->GetSuperClass()->GetAuthoritativeClass());
			}
			if(BP->GeneratedClass && CompilerData.Reinstancer.IsValid())
			{
				CompilerData.Reinstancer->TakeOwnershipOfSparseClassData(BP->GeneratedClass);
			}
		}

		SlowTask.EnterProgressFrame();

		// STAGE XII: Recompile every blueprint
		bGeneratedClassLayoutReady = false;
		for (FCompilerData& CompilerData : CurrentlyCompilingBPs)
		{
			UBlueprint* BP = CompilerData.BP;
			if(CompilerData.ShouldCompileClassLayout())
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(CompileClassLayout);
				SCOPED_LOADTIMER_ASSET_TEXT(*BP->GetPathName());

				ensure( BP->GeneratedClass == nullptr ||
						BP->GeneratedClass->ClassDefaultObject == nullptr || 
						BP->GeneratedClass->ClassDefaultObject->GetClass() != BP->GeneratedClass);
				// default value propagation occurs in ReinstaneBatch, CDO will be created via CompileFunctions call:
				if(BP->ParentClass)
				{
					if(BP->GeneratedClass)
					{
						BP->GeneratedClass->ClassDefaultObject = nullptr;
					}

					// Reset the flag, so if the user tries to use PIE it will warn them if the BP did not compile
					BP->bDisplayCompilePIEWarning = true;
		
					// this will create FProperties for the UClass and generate the sparse class data
					// if the compiler in question wants to:
					FKismetCompilerContext& CompilerContext = *(CompilerData.Compiler);
					CompilerContext.CompileClassLayout(EInternalCompilerFlags::PostponeLocalsGenerationUntilPhaseTwo);

					// We immediately relink children so that iterative compilation logic has an easier time:
					TArray<UClass*> ClassesToRelink;
					GetDerivedClasses(BP->GeneratedClass, ClassesToRelink, false);
					for (UClass* ChildClass : ClassesToRelink)
					{
						ChildClass->Bind();
						ChildClass->StaticLink();
						ensure(ChildClass->ClassDefaultObject == nullptr);
					}
				}
				else
				{
					CompilerData.ActiveResultsLog->Error(*LOCTEXT("KismetCompileError_MalformedParentClasss", "Blueprint @@ has missing or NULL parent class.").ToString(), BP);
				}
			}
			else if(CompilerData.Compiler.IsValid() && BP->GeneratedClass)
			{
				CompilerData.Compiler->SetNewClass( CastChecked<UBlueprintGeneratedClass>(BP->GeneratedClass) );
			}
		}

		// If we're compiling more than one Blueprint as part of a batch operation, we may have generated
		// delegate properties (e.g. function parameters) that reference a function dependency in one of
		// the other Blueprints in the batched set. Depending on the compile order, those functions may get
		// regenerated in a subsequent pass above, invalidating the delegate property references as a result.
		// Invalidating references that exist outside of the class layout (e.g. in bytecode) is fine, as
		// those will be fixed up after we've finished compiling. However, the next stage (compiling functions)
		// will attempt to validate generated fields for function parameters against their source pin types,
		// and that logic will throw an error for generated delegate properties if they reference a signature
		// function that has not yet been replaced. As a result, we run a small pass here to find/fix those up.
		// 
		// Note: It is *not* necessary to also verify ScriptAndPropertyObjectReferences here, as that will be
		// updated in the next stage via reference collection (@see FKismetCompilerContext::CompileFunctions).
		FixupDelegateProperties(CurrentlyCompilingBPs);

		bGeneratedClassLayoutReady = true;
		
		ProcessExtensions(CurrentlyCompilingBPs);

		SlowTask.EnterProgressFrame();

		// STAGE XIII: Compile functions
		UBlueprintEditorSettings* Settings = GetMutableDefault<UBlueprintEditorSettings>();
		
		const bool bSaveBlueprintsAfterCompile = Settings->SaveOnCompile == SoC_Always;
		const bool bSaveBlueprintAfterCompileSucceeded = Settings->SaveOnCompile == SoC_SuccessOnly;

		for (FCompilerData& CompilerData : CurrentlyCompilingBPs)
		{
			UBlueprint* BP = CompilerData.BP;
			UClass* BPGC = BP->GeneratedClass;

			if(!CompilerData.ShouldCompileClassFunctions())
			{
				if( BPGC &&
					(	BPGC->ClassDefaultObject == nullptr || 
						BPGC->ClassDefaultObject->GetClass() != BPGC) )
				{
					if (CompilerData.Reinstancer.IsValid())
					{
						CompilerData.Reinstancer->PropagateSparseClassDataToNewClass(BPGC);
					}
					// relink, generate CDO:
					BPGC->bLayoutChanging = false;
					BPGC->Bind();
					BPGC->StaticLink(true);
					BPGC->ClassDefaultObject = nullptr;
					BPGC->GetDefaultObject(true);
				}
			}
			else
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(CompileClassFunctions);
				SCOPED_LOADTIMER_ASSET_TEXT(*BP->GetPathName());

				// default value propagation occurs below:
				if(BPGC)
				{
					if (CompilerData.Reinstancer.IsValid())
					{
						CompilerData.Reinstancer->PropagateSparseClassDataToNewClass(BPGC);
					}

					if( BPGC->ClassDefaultObject && 
						BPGC->ClassDefaultObject->GetClass() == BPGC)
					{
						// the CDO has been created early, it is possible that the reflection data was still
						// being mutated by CompileClassLayout. Warn the user and and move the CDO aside:
						ensureAlwaysMsgf(false, 
							TEXT("ClassDefaultObject for %s created at the wrong time - it may be corrupt. It is recommended that you save all data and restart the editor session"), 
							*BP->GetName()
						);

						BPGC->ClassDefaultObject->Rename(
							nullptr,
							// destination - this is the important part of this call. Moving the object 
							// out of the way so we can reuse its name:
							GetTransientPackage(), 
							// Rename options:
							REN_DoNotDirty | REN_DontCreateRedirectors | REN_ForceNoResetLoaders
						);
					}
					BPGC->ClassDefaultObject = nullptr;
		
					// class layout is ready, we can clear bLayoutChanging and CompileFunctions can create the CDO:
					BPGC->bLayoutChanging = false;

					FKismetCompilerContext& CompilerContext = *(CompilerData.Compiler);
					CompilerContext.CompileFunctions(
						EInternalCompilerFlags::PostponeLocalsGenerationUntilPhaseTwo
						|EInternalCompilerFlags::PostponeDefaultObjectAssignmentUntilReinstancing
						|EInternalCompilerFlags::SkipRefreshExternalBlueprintDependencyNodes
					); 
				}

				if (CompilerData.ActiveResultsLog->NumErrors == 0)
				{
					// Blueprint is error free.  Go ahead and fix up debug info
					BP->Status = (0 == CompilerData.ActiveResultsLog->NumWarnings) ? BS_UpToDate : BS_UpToDateWithWarnings;

					BP->BlueprintSystemVersion = UBlueprint::GetCurrentBlueprintSystemVersion();

					// Reapply breakpoints to the bytecode of the new class
					FKismetDebugUtilities::ForeachBreakpoint(
						BP,
						[](FBlueprintBreakpoint& Breakpoint)
						{
							FKismetDebugUtilities::ReapplyBreakpoint(Breakpoint);
						}
					);
				}
				else
				{
					BP->Status = BS_Error; // do we still have the old version of the class?
				}

				// SOC settings only apply after compile on load:
				if(!BP->bIsRegeneratingOnLoad)
				{
					if(bSaveBlueprintsAfterCompile || (bSaveBlueprintAfterCompileSucceeded && BP->Status == BS_UpToDate))
					{
						CompiledBlueprintsToSave.Add(BP);
					}
				}
			}

			if(BPGC)
			{
				BPGC->ClassFlags &= ~CLASS_ReplicationDataIsSetUp;
				BPGC->SetUpRuntimeReplicationData();
			}
			
			FKismetCompilerUtilities::UpdateDependentBlueprints(BP);

			ensure(BPGC == nullptr || BPGC->ClassDefaultObject->GetClass() == BPGC);
		}
	} // end GTimeCompiling scope

	SlowTask.EnterProgressFrame();

	// STAGE XIV: Now we can finish the first stage of the reinstancing operation, moving old classes to new classes:
	{
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(MoveOldClassesToNewClasses);

			TArray<FReinstancingJob> Reinstancers;
			// Set up reinstancing jobs - we need a reference to the compiler in order to honor 
			// CopyTermDefaultsToDefaultObject
			for (FCompilerData& CompilerData : CurrentlyCompilingBPs)
			{
				if(CompilerData.Reinstancer.IsValid() && CompilerData.Reinstancer->ClassToReinstance)
				{
					Reinstancers.Push(
						FReinstancingJob( CompilerData.Reinstancer, CompilerData.Compiler )
					);
				}
			}

			FScopedDurationTimer ReinstTimer(GTimeReinstancing);
			ReinstanceBatch(Reinstancers, MutableView(ClassesToReinstance), InLoadContext, OldToNewTemplates);

			// We purposefully do not remove the OldCDOs yet, need to keep them in memory past first GC
		}

		// Set default values on any newly-introduced variables (from ancestor BPs)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(SetNewDefaultVariables);

			for (FCompilerData& CompilerData : CurrentlyCompilingBPs)
			{
				if (!CompilerData.NewDefaultVariables.Num())
				{
					continue;
				}

				const UBlueprintGeneratedClass* GenClass = Cast<UBlueprintGeneratedClass>(CompilerData.BP->GeneratedClass);

				if (GenClass && GenClass->ClassDefaultObject)
				{
					for (const FBPVariableDescription& NewInheritedVar : CompilerData.NewDefaultVariables)
					{
						if (const FProperty* MatchingProperty = GenClass->FindPropertyByName(NewInheritedVar.VarName))
						{
							FBlueprintEditorUtils::PropertyValueFromString(MatchingProperty, NewInheritedVar.DefaultValue, reinterpret_cast<uint8*>(GenClass->ClassDefaultObject.Get()));
						}
					}
				}
			}
		}
		
		// STAGE XV: POST CDO COMPILED
		for (FCompilerData& CompilerData : CurrentlyCompilingBPs)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(PostCDOCompiled);

			if (CompilerData.Compiler.IsValid())
			{
				SCOPED_LOADTIMER_ASSET_TEXT(*CompilerData.BP->GetPathName());
				UObject::FPostCDOCompiledContext PostCDOCompiledContext;
				PostCDOCompiledContext.bIsRegeneratingOnLoad = CompilerData.BP->bIsRegeneratingOnLoad;
				PostCDOCompiledContext.bIsSkeletonOnly = CompilerData.IsSkeletonOnly();

				CompilerData.Compiler->PostCDOCompiled(PostCDOCompiledContext);
			}
		}

		// STAGE XVI: CLEAR TEMPORARY FLAGS
		for (FCompilerData& CompilerData : CurrentlyCompilingBPs)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(ClearTemporaryFlags);

			UBlueprint* BP = CompilerData.BP;

			// Ensure that delegate nodes/pins conform to compiled function names/signatures. In general,
			// these will rely on the skeleton class, so we must also include it for a skeleton-only pass.
			FBlueprintEditorUtils::UpdateDelegatesInBlueprint(BP);

			// These are post-compile validations that generally rely on a fully-generated up-to-date class,
			// so we defer it on skeleton-only passes as well as during compile-on-load, where it's generally
			// going to be handled during the PostLoad() phase after all dependencies have been loaded/compiled.
			if (!CompilerData.IsSkeletonOnly() && !BP->bIsRegeneratingOnLoad && BP->GeneratedClass)
			{
				FKismetEditorUtilities::StripExternalComponents(BP);

				if (BP->SimpleConstructionScript)
				{
					BP->SimpleConstructionScript->FixupRootNodeParentReferences();
				}

				if (BP->Status != BS_Error)
				{
					if (CompilerData.Compiler.IsValid())
					{
						// Route through the compiler context in order to perform type-specific Blueprint class validation.
						CompilerData.Compiler->ValidateGeneratedClass(CastChecked<UBlueprintGeneratedClass>(BP->GeneratedClass));

						if (CompilerData.ShouldValidateClassDefaultObject())
						{
							// Our CDO should be properly constructed by this point and should always exist
							const UObject* ClassDefaultObject = BP->GeneratedClass->GetDefaultObject(false);
							if (ensureAlways(ClassDefaultObject))
							{
								FKismetCompilerUtilities::ValidateEnumProperties(ClassDefaultObject, *CompilerData.ActiveResultsLog);

								// Make sure any class-specific validation passes on the CDO
								FDataValidationContext Context;
								EDataValidationResult ValidateCDOResult = ClassDefaultObject->IsDataValid(/*out*/ Context);
								if (Context.GetNumErrors() + Context.GetNumWarnings() > 0)
								{
									TArray<FText> Warnings, Errors;
									Context.SplitIssues(Warnings, Errors);

									for (const FText& Warning : Warnings)
									{
										CompilerData.ActiveResultsLog->Warning(*Warning.ToString());
									}

									for (const FText& Error : Errors)
									{
										CompilerData.ActiveResultsLog->Error(*Error.ToString());
									}
								}

								// Adjust Blueprint status to match anything new that was found during validation.
								if (CompilerData.ActiveResultsLog->NumErrors > 0)
								{
									BP->Status = BS_Error;
								}
								else if (BP->Status == BS_UpToDate && CompilerData.ActiveResultsLog->NumWarnings > 0)
								{
									BP->Status = BS_UpToDateWithWarnings;
								}
							}
						}
					}
					else
					{
						UBlueprint::ValidateGeneratedClass(BP->GeneratedClass);
					}
				}
			}

			if(CompilerData.ShouldRegisterCompilerResults())
			{
				if (IsInGameThread())
				{
					// This helper structure registers the results log messages with the UI control that displays them:
					FScopedBlueprintMessageLog MessageLog(BP);
					MessageLog.Log->ClearMessages();
					MessageLog.Log->AddMessages(CompilerData.ActiveResultsLog->Messages, false);
				}
				else
				{
					Async(EAsyncExecution::TaskGraphMainThread,
						[Messages = CompilerData.ActiveResultsLog->Messages, WeakBP = TWeakObjectPtr<UBlueprint>(BP)]()
						{
							if (WeakBP.IsValid())
							{
								FScopedBlueprintMessageLog MessageLog(WeakBP.Get());
								MessageLog.Log->ClearMessages();
								MessageLog.Log->AddMessages(Messages, false);
							}
						}
					);
				}
				
			}

			if(CompilerData.ShouldSetTemporaryBlueprintFlags())
			{
				BP->bBeingCompiled = false;
				BP->CurrentMessageLog = nullptr;
				BP->bIsRegeneratingOnLoad = false;
			}

			if(UPackage* Package = BP->GetOutermost())
			{
				Package->SetDirtyFlag(CompilerData.bPackageWasDirty);
			}
		}

		// Make sure no junk in bytecode, this can happen only for blueprints that were in CurrentlyCompilingBPs because
		// the reinstancer can detect all other references (see UpdateBytecodeReferences):
		for (FCompilerData& CompilerData : CurrentlyCompilingBPs)
		{
			if(CompilerData.ShouldCompileClassFunctions())
			{
				if(BlueprintsCompiled)
				{
					BlueprintsCompiled->Add(CompilerData.BP);
				}
				
				if(!bSuppressBroadcastCompiled)
				{
					// Some logic (e.g. UObject::ProcessInternal) uses this flag to suppress warnings:
					TGuardValue<bool> ReinstancingGuard(GIsReinstancing, true);
					CompilerData.BP->BroadcastCompiled();
				}

				continue;
			}

			UBlueprint* BP = CompilerData.BP;
			for( TFieldIterator<UFunction> FuncIter(BP->GeneratedClass, EFieldIteratorFlags::ExcludeSuper); FuncIter; ++FuncIter )
			{
				UFunction* CurrentFunction = *FuncIter;
				if( CurrentFunction->Script.Num() > 0 )
				{
					FFixupBytecodeReferences ValidateAr(CurrentFunction);
				}
			}
		}

		if (!bSuppressBroadcastCompiled)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(BroadcastBlueprintCompiled);

			if(GEditor)
			{
				GEditor->BroadcastBlueprintCompiled();	
			}
		}
	}

	SlowTask.EnterProgressFrame();

	for (FCompilerData& CompilerData : CurrentlyCompilingBPs)
	{
		if(CompilerData.ResultsLog)
		{
			CompilerData.ResultsLog->EndEvent();
		}
		CompilerData.BP->bQueuedForCompilation = false;
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UEdGraphPin::Purge);
		UEdGraphPin::Purge();
	}

	SlowTask.EnterProgressFrame();

	UE_LOG(LogBlueprint, Display, TEXT("Time Compiling: %f, Time Reinstancing: %f"),  GTimeCompiling, GTimeReinstancing);
	//GTimeCompiling = 0.0;
	//GTimeReinstancing = 0.0;
	VerifyNoQueuedRequests(CurrentlyCompilingBPs);
}

void FBlueprintCompilationManagerImpl::FixupDelegateProperties(const TArray<FCompilerData>& InCurrentlyCompilingBPs)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FixupDelegateProperties);

	if (InCurrentlyCompilingBPs.Num() <= 1)
	{
		// It's safe to bypass the fixup logic in this case.
		return;
	}

	// Gather old->new field mappings for each regenerated class.
	TMap<FFieldVariant, FFieldVariant> AllFieldMappings;
	for (const FCompilerData& CompilerData : InCurrentlyCompilingBPs)
	{
		// If there's no reinstancer, then we don't need to include it (e.g. skeleton-only compiles and/or when the layout wasn't regenerated).
		if (CompilerData.Reinstancer.IsValid())
		{
			TMap<FFieldVariant, FFieldVariant> FieldMappings;
			CompilerData.Reinstancer->GenerateFieldMappings(FieldMappings);
			AllFieldMappings.Append(MoveTemp(FieldMappings));
		}
	}

	auto FixupDelegateProperty = [&AllFieldMappings](FDelegateProperty* DelegateProperty)
	{
		// See if the current signature references a stale function (i.e. one that has been regenerated).
		auto* PossiblyStaleFunctionPtr = &DelegateProperty->SignatureFunction;
		if (FFieldVariant* GeneratedFunctionMapping = AllFieldMappings.Find(*PossiblyStaleFunctionPtr))
		{
			// Update the stale reference to point to the regenerated function instead.
			*PossiblyStaleFunctionPtr = CastChecked<UFunction>(GeneratedFunctionMapping->ToUObject());
		}
	};

	for (const TPair<FFieldVariant, FFieldVariant>& FieldMapping : AllFieldMappings)
	{
		// Check signatures for any delegate properties owned by the regenerated class.
		if (FDelegateProperty* ClassDelegateProperty = CastField<FDelegateProperty>(FieldMapping.Value.ToField()))
		{
			FixupDelegateProperty(ClassDelegateProperty);
		}
		else if (UFunction* Function = Cast<UFunction>(FieldMapping.Value.ToUObject()))
		{
			// Check signatures for any delegate properties owned by a regenerated class function (e.g. parameters).
			for (FDelegateProperty* LocalDelegateProperty : TFieldRange<FDelegateProperty>(Function, EFieldIterationFlags::IncludeDeprecated))
			{
				FixupDelegateProperty(LocalDelegateProperty);
			}
		}
	}
}

void FBlueprintCompilationManagerImpl::ProcessExtensions(const TArray<FCompilerData>& InCurrentlyCompilingBPs)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ProcessExtensions);

	if(CompilerExtensions.Num() == 0)
	{
		return;
	}

	for (const FCompilerData& CompilerData : InCurrentlyCompilingBPs)
	{
		UBlueprint* BP = CompilerData.BP;
		FBlueprintCompiledData CompiledData; // populate only if we find an extension:

		if(CompilerData.ShouldCompileClassLayout())
		{
			// give extension a chance to raise errors, or save off data:
			UClass* Iter = BP->GetClass();
			while(Iter != UBlueprint::StaticClass()->GetSuperClass())
			{
				auto* Extensions = CompilerExtensions.Find(Iter);
				if(Extensions)
				{
					// extension found, store off data from compiler that we want to expose to extensions:
					if(CompiledData.IntermediateGraphs.Num() == 0)
					{
						if(CompilerData.Compiler->ConsolidatedEventGraph)
						{
							CompiledData.IntermediateGraphs.Emplace(CompilerData.Compiler->ConsolidatedEventGraph);
						}
								
						for(const FKismetFunctionContext& Fn : CompilerData.Compiler->FunctionList)
						{
							if(Fn.SourceGraph == CompilerData.Compiler->ConsolidatedEventGraph)
							{
								continue;
							}

							CompiledData.IntermediateGraphs.Emplace(Fn.SourceGraph);
						}
					}

					for(UBlueprintCompilerExtension* Extension : *Extensions)
					{
						Extension->BlueprintCompiled(*CompilerData.Compiler, CompiledData);
					}
				}
				Iter = Iter->GetSuperClass();
			}
		}
	}
}

void FBlueprintCompilationManagerImpl::FlushReinstancingQueueImpl(bool bFindAndReplaceCDOReferences, TMap<UClass*, TMap<UObject*, UObject*>>* OldToNewTemplates /* = nullptr*/)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FlushReinstancingQueueImpl);

#if WITH_EDITOR
	FScopeLock ScopeLock(&Lock);
#endif

	if(GCompilingBlueprint)
	{
		return;
	}

	TGuardValue<bool> GuardTemplateNameFlag(GCompilingBlueprint, true);
	// we can finalize reinstancing now:
	if(ClassesToReinstance.Num() == 0)
	{
		return;
	}

	{
		FScopedDurationTimer ReinstTimer(GTimeReinstancing);
		
		TGuardValue<bool> ReinstancingGuard(GIsReinstancing, true);
		
		TMap<UClass*, UClass*> ClassesToReinstanceOwned = ObjectPtrDecay(MoveTemp(ClassesToReinstance));
		ClassesToReinstance = {};

		FReplaceInstancesOfClassParameters Options;
		Options.bArchetypesAreUpToDate = true;
		Options.bReplaceReferencesToOldCDOs = bFindAndReplaceCDOReferences;
		Options.OldToNewTemplates = OldToNewTemplates;
		FBlueprintCompileReinstancer::BatchReplaceInstancesOfClass(ClassesToReinstanceOwned, Options);

		// Special case when we run on ALT, we want to cleanup all classes flagged for reinstanciation right away.
		const bool bIsInActualAsyncLoadingThread = IsInAsyncLoadingThread() && !IsInGameThread();
		if (IsAsyncLoading() && (!IsAsyncLoadingMultithreaded() || !bIsInActualAsyncLoadingThread))
		{
			// While async loading we only remove classes that have no instances being
			// async loaded. Those instances will need to be reinstanced once they finish
			// loading, there's no race here because if any instances are created after
			// we check ClassHasInstancesAsyncLoading they will be created with the new class:
			for( TMap<UClass*, UClass*>::TIterator It(ClassesToReinstanceOwned); It; ++It )
			{
				if (!ClassHasInstancesAsyncLoading(It->Key))
				{
					// Make sure to cleanup all properties that couldn't be destroyed in PurgeClass
					It->Key->DestroyPropertiesPendingDestruction();
					It->Value->DestroyPropertiesPendingDestruction();
					It.RemoveCurrent();
				}
			}
			// preserve any pairs that are currently loading:
			ClassesToReinstance = ObjectPtrWrap(MoveTemp(ClassesToReinstanceOwned));
		}
		else
		{
			// Make sure to cleanup all properties that couldn't be destroyed in PurgeClass
			for (decltype(ClassesToReinstanceOwned)::TIterator It(ClassesToReinstanceOwned); It; ++It)
			{
				It->Key->DestroyPropertiesPendingDestruction();
				It->Value->DestroyPropertiesPendingDestruction();
			}
		}
	}
	
#if VERIFY_NO_STALE_CLASS_REFERENCES
	FBlueprintSupport::ValidateNoRefsToOutOfDateClasses();
#endif

#if VERIFY_NO_BAD_SKELETON_REFERENCES
	FBlueprintSupport::ValidateNoExternalRefsToSkeletons();
#endif

	UE_LOG(LogBlueprint, Display, TEXT("Time Compiling: %f, Time Reinstancing: %f"),  GTimeCompiling, GTimeReinstancing);
}

bool FBlueprintCompilationManagerImpl::HasBlueprintsToCompile() const
{
	return QueuedRequests.Num() != 0;
}

bool FBlueprintCompilationManagerImpl::IsGeneratedClassLayoutReady() const
{
	return bGeneratedClassLayoutReady;
}

void FBlueprintCompilationManagerImpl::GetDefaultValue(const UClass* ForClass, const FProperty* Property, FString& OutDefaultValueAsString) const
{
	if(!ForClass || !Property)
	{
		return;
	}

	if (ForClass->ClassDefaultObject)
	{
		FBlueprintEditorUtils::PropertyValueToString(Property, (uint8*)ForClass->ClassDefaultObject.Get(), OutDefaultValueAsString);
	}
	else
	{
		UBlueprint* BP = Cast<UBlueprint>(ForClass->ClassGeneratedBy);
		if(ensure(BP))
		{
			const auto* OldCDO = OldCDOs.Find(BP);
			if(OldCDO && *OldCDO)
			{
				const UClass* OldClass = (*OldCDO)->GetClass();
				const FProperty* OldProperty = OldClass->FindPropertyByName(Property->GetFName());
				if(OldProperty)
				{
					FBlueprintEditorUtils::PropertyValueToString(OldProperty, (uint8*)OldCDO->Get(), OutDefaultValueAsString);
				}
			}
		}
	}
}

void FBlueprintCompilationManagerImpl::VerifyNoQueuedRequests(const TArray<FCompilerData>& CurrentlyCompilingBPs)
{
	if (QueuedRequests.Num() != 0)
	{
		FString QueuedBlueprints = TEXT("");
		for (const FBPCompileRequestInternal& Request: QueuedRequests)
		{
			QueuedBlueprints += Request.UserData.BPToCompile->GetName() + TEXT(" ");
		}
		FString CompilingBlueprints = TEXT("");
		for (const FCompilerData& CompilerData : CurrentlyCompilingBPs)
		{
			CompilingBlueprints += CompilerData.BP->GetName() + TEXT(" ");
		}

		ensureMsgf(false, 
			TEXT("Blueprints requested compilation while compiling other blueprints: %s\nWhile Compiling: %s"),
			*QueuedBlueprints, *CompilingBlueprints);
	}
}

void FBlueprintCompilationManagerImpl::ReparentHierarchies(const TMap<UClass*, UClass*>& OldToNewClasses, EReparentClassOptions Options)
{
	const bool bReplaceReferencesToOldClasses = (Options & EReparentClassOptions::ReplaceReferencesToOldClasses) != EReparentClassOptions::None;
	TRACE_CPUPROFILER_EVENT_SCOPE(ReparentHierarchies);

	// something has decided to replace instances of a class. We need to update all the children of those types:
	TArray< UClass* > ClassesOrdered;
	// Map used to distinguish between new classes and classes that need to be reinstanced (reparented) via a new reinstancer:
	TMap<UClass*, UClass*> NewToOldClasses;
	{
		TSet<UClass*> Classes;
		for(const TPair<UClass*, UClass*>& OldToNewClass : OldToNewClasses)
		{
			// classes with no CDO do not need to be reinstanced:
			if(OldToNewClass.Key->ClassDefaultObject == nullptr)
			{
				continue;
			}

			Classes.Add(OldToNewClass.Value);
			NewToOldClasses.Add(OldToNewClass.Value, OldToNewClass.Key);

			TArray<UClass*> DerivedClasses;
			// Just like when compiling we have to gather all children, not just immediate children. This is so that we can
			// update things like the ClassConstructor pointer in case it changed:
			GetDerivedClasses(OldToNewClass.Key, DerivedClasses);

			for(UClass* DerivedClass : DerivedClasses)
			{
				if (DerivedClass->ClassDefaultObject == nullptr && 
					DerivedClass->GetSparseClassData(EGetSparseClassDataMethod::ReturnIfNull) == nullptr)
				{
					// no CDO->no other instances->no need to reinstance..
					// and we have no sparse classs data to manage...
					continue;
				}

				if (DerivedClass->HasAnyClassFlags(CLASS_NewerVersionExists))
				{
					// Don't reinstance artifacts from classes that have been previously been reinstanced/trashed but aren't cleaned up yet.
					continue;
				}

				if (OldToNewClasses.Find(DerivedClass) == nullptr)
				{
					Classes.Add(DerivedClass);
				}
			}
		}

		ClassesOrdered = Classes.Array();
	}

	// Order the classes we're about to reinstance by hierarchy depth. This will improve determinism
	// and is as logical an order as I can come up with:
	ClassesOrdered.Sort( [](UClass& A, UClass& B)->bool { return FBlueprintCompileReinstancer::ReinstancerOrderingFunction(&A, &B); } );

	// create reinstancing jobs, no need to create a reinstancer when there is a new UClass* available (e.g. asset reload, hot reload):
	TArray<FReinstancingJob> Reinstancers;
	for(UClass* Class : ClassesOrdered)
	{
		UClass* const* OldClass = NewToOldClasses.Find(Class);
		if(OldClass)
		{
			Reinstancers.Push( FReinstancingJob( TPair<UClass*, UClass*>(*OldClass, Class) ) );
		}
		else
		{
			Reinstancers.Push( FReinstancingJob( 
				TSharedPtr<FBlueprintCompileReinstancer>(
					new FBlueprintCompileReinstancer(
						Class,
						EBlueprintCompileReinstancerFlags::AutoInferSaveOnCompile | EBlueprintCompileReinstancerFlags::AvoidCDODuplication
					)
				),
				TSharedPtr<FKismetCompilerContext>()
			) );

			FReinstancingJob& ReinstancingJob = Reinstancers.Last();
			ReinstancingJob.Reinstancer->SaveSparseClassData(Class);
			ensure(ReinstancingJob.Reinstancer->DuplicatedClass && ReinstancingJob.Reinstancer->ClassToReinstance);
		}
	}

	for (const FReinstancingJob& ReinstancingJob : Reinstancers)
	{
		if (ReinstancingJob.Reinstancer.IsValid())
		{
			ReinstancingJob.Reinstancer->TakeOwnershipOfSparseClassData(ReinstancingJob.OldToNew.Value);
		}
	}

	// Reparent and Link - this is .. kind of pointless.. ReinstanceBatch should
	// be doing this
	TMap<UClass*, UClass*> OldClassToNewClassIncludingChildren = OldToNewClasses;
	for(const FReinstancingJob& ReinstancingJob : Reinstancers)
	{
		UClass* ClassToReinstance = ReinstancingJob.OldToNew.Value;

		// We only need to relink if we've reparented the class to a new type:
		if (ReinstancingJob.Reinstancer.IsValid())
		{
			ClassToReinstance->ClassConstructor = nullptr;
			ClassToReinstance->ClassVTableHelperCtorCaller = nullptr;
			ClassToReinstance->CppClassStaticFunctions.Reset();
		
			// check to see if we're a direct parent of one of the new classes:
			UClass* const* NewParent = OldToNewClasses.Find(ClassToReinstance->GetSuperClass());
			if(NewParent)
			{
				ClassToReinstance->SetSuperStruct(*NewParent);
			}

			ClassToReinstance->Bind();
			ClassToReinstance->ClearFunctionMapsCaches();
			ClassToReinstance->StaticLink(true);
		}

		OldClassToNewClassIncludingChildren.Add(ReinstancingJob.OldToNew.Key, ClassToReinstance);
	}
	
	// Reparenting done, reinstance the hierarchy and update archetypes:
	TMap<UClass*, TMap<UObject*, UObject*>> OldToNewTemplates;
	ReinstanceBatch(Reinstancers, OldClassToNewClassIncludingChildren, nullptr, &OldToNewTemplates);

	// Reinstance (non archetype) instances
	TMap<UClass*, UClass*> OldClassToNewClassDerivedTypes;
	
	if (bReplaceReferencesToOldClasses)
	{
		OldClassToNewClassDerivedTypes = OldClassToNewClassIncludingChildren;
	}
	
	for(const FReinstancingJob& ReinstancingJob : Reinstancers)
	{
		OldClassToNewClassDerivedTypes.Add(ReinstancingJob.OldToNew);
	}
	TGuardValue<bool> ReinstancingGuard(GIsReinstancing, true);
	FReplaceInstancesOfClassParameters BatchOptions;
	BatchOptions.bArchetypesAreUpToDate = true;
	BatchOptions.bReplaceReferencesToOldClasses = bReplaceReferencesToOldClasses;

	// Make sure we don't replace old instances that are in the *callers* old to new TMap!
	TSet<UObject*> OldObjects;
	for(TPair<UClass*, UClass*> OldToNew : OldClassToNewClassDerivedTypes)
	{
		ensure(OldToNew.Value->HasAnyClassFlags(CLASS_TokenStreamAssembled));

		TArray< UObject* > OldObjectsOfType;
		GetObjectsOfClass(OldToNew.Key, OldObjectsOfType);

		for(UObject* Obj : OldObjectsOfType)
		{
			if(Obj->HasAnyFlags(RF_NewerVersionExists))
			{
				OldObjects.Add(Obj);
			}
		}
	}
	BatchOptions.ObjectsThatShouldUseOldStuff = &OldObjects;
	BatchOptions.InstancesThatShouldUseOldClass = &OldObjects;
	BatchOptions.bReplaceReferencesToOldCDOs = true;
	BatchOptions.OldToNewTemplates = &OldToNewTemplates;

	FBlueprintCompileReinstancer::BatchReplaceInstancesOfClass(OldClassToNewClassDerivedTypes, BatchOptions );
}


void FBlueprintCompilationManagerImpl::BuildDSOMap(UObject* OldObject, UObject* NewObject, TMap<UObject*, UObject*>& OutOldToNewDSO)
{
	// IsDefaultSubObject() unfortunately cannot be relied upon for archetypes, so we explicitly search for the flag:
	TArray<UObject*> OldSubobjects;
	ForEachObjectWithOuter(OldObject, [&OldSubobjects](UObject* Object)
	{
		if (Object->HasAnyFlags(RF_DefaultSubObject|RF_ArchetypeObject))
		{
			OldSubobjects.Add(Object);
		}
	}, false);

	for(UObject* OldSubobject : OldSubobjects)
	{
		UObject* NewSubobject = NewObject ? StaticFindObjectFast(UObject::StaticClass(), NewObject, OldSubobject->GetFName()) : nullptr;
		// It may seem aggressive to ensure here, but we should always have a new version of the DSO. If that's
		// not the case then some testing will need to be done on client of OutOldToNewDSO
		OutOldToNewDSO.Add(OldSubobject, NewSubobject);
		BuildDSOMap(OldSubobject, NewSubobject, OutOldToNewDSO);
	}
}

void FBlueprintCompilationManagerImpl::ReinstanceBatch(TArray<FReinstancingJob>& Reinstancers, TMap< UClass*, UClass* >& InOutOldToNewClassMap, FUObjectSerializeContext* InLoadContext, TMap<UClass*, TMap<UObject*, UObject*>>* OldToNewTemplates /* = nullptr*/)
{
	TGuardValue<bool> ReinstancingGuard(GIsReinstancing, true);

	const auto FilterOutOfDateClasses = [](TArray<UClass*>& ClassList)
	{
		// Old versions of classes can be abandoned, classes without CDOs have no instances and don't require reinstancing
		// but they may still require reparenting..
		ClassList.RemoveAllSwap( [](UClass* Class) { return Class->HasAnyClassFlags(CLASS_NewerVersionExists); } );
	};

	const auto HasChildren = [FilterOutOfDateClasses](UClass* InClass) -> bool
	{
		TArray<UClass*> ChildTypes;
		GetDerivedClasses(InClass, ChildTypes, false);
		FilterOutOfDateClasses(ChildTypes);
		return ChildTypes.Num() > 0;
	};

	TSet<UClass*> ClassesToReparent;
	TSet<UClass*> ClassesToReinstance;

	// Reinstancers may contain *part* of a class hierarchy, so we first need to reparent any child types that 
	// haven't already been reinstanced:
	for (const FReinstancingJob& ReinstancingJob : Reinstancers)
	{
		UClass* OldClass = ReinstancingJob.OldToNew.Key;
		if(!OldClass)
		{
			continue;
		}

		UClass* NewClass = ReinstancingJob.OldToNew.Value;

		InOutOldToNewClassMap.Add(OldClass, NewClass);

		if(!HasChildren(OldClass))
		{
			continue;
		}

		bool bParentLayoutChanged = !FStructUtils::TheSameLayout(OldClass, NewClass);

		if(!bParentLayoutChanged)
		{
			// make sure uber graph didn't change, if present:
			UBlueprintGeneratedClass* OldParent = Cast<UBlueprintGeneratedClass>(OldClass);
			UBlueprintGeneratedClass* NewBPGC = Cast<UBlueprintGeneratedClass>(NewClass);

			if(OldParent && NewBPGC && OldParent->UberGraphFunction)
			{
				bParentLayoutChanged = !FStructUtils::TheSameLayout(OldParent->UberGraphFunction, NewBPGC->UberGraphFunction);
			}
		}

		if(bParentLayoutChanged)
		{
			// we need *all* derived types:
			TArray<UClass*> ClassesToReinstanceList;
			GetDerivedClasses(OldClass, ClassesToReinstanceList);
			FilterOutOfDateClasses(ClassesToReinstanceList);
			
			for(UClass* ClassToReinstance : ClassesToReinstanceList)
			{
				if(IsValid(ClassToReinstance))
				{
					if (ClassToReinstance->ClassDefaultObject)
					{
						ClassesToReinstance.Add(ClassToReinstance);
					}
					else
					{
						ClassesToReparent.Add(ClassToReinstance);
					}
				}
			}
		}
		else
		{
			// parent layout did not change, we can just relink the direct children:
			TArray<UClass*> ClassesToReparentList;
			GetDerivedClasses(OldClass, ClassesToReparentList, false);
			FilterOutOfDateClasses(ClassesToReparentList);
			
			for(UClass* ClassToReparent : ClassesToReparentList)
			{
				if(IsValid(ClassToReparent))
				{
					ClassesToReparent.Add(ClassToReparent);
				}
			}
		}
	}

	for(UClass* Class : ClassesToReparent)
	{
		UClass** NewParent = InOutOldToNewClassMap.Find(Class->GetSuperClass());
		check(NewParent && *NewParent);
		Class->SetSuperStruct(*NewParent);
		Class->Bind();
		Class->StaticLink(true);

		Class->ClassFlags &= ~CLASS_ReplicationDataIsSetUp;
		Class->SetUpRuntimeReplicationData();
	}

	// make new hierarchy
	for(UClass* Class : ClassesToReinstance)
	{
		UObject* OriginalCDO = Class->ClassDefaultObject;
		Reinstancers.Emplace(
			FReinstancingJob ( 
				TSharedPtr<FBlueprintCompileReinstancer>( 
					new FBlueprintCompileReinstancer(
						Class, 
						EBlueprintCompileReinstancerFlags::AutoInferSaveOnCompile|EBlueprintCompileReinstancerFlags::AvoidCDODuplication
					)
				),
				TSharedPtr<FKismetCompilerContext>()
			)
		);

		// make sure we have the newest parent now that CDO has been moved to duplicate class:
		TSharedPtr<FBlueprintCompileReinstancer>& NewestReinstancer = Reinstancers.Last().Reinstancer;
		ensure(NewestReinstancer->DuplicatedClass && NewestReinstancer->ClassToReinstance);

		UClass* SuperClass = NewestReinstancer->ClassToReinstance->GetSuperClass();
		if(ensure(SuperClass))
		{
			if(SuperClass->HasAnyClassFlags(CLASS_NewerVersionExists))
			{
				NewestReinstancer->ClassToReinstance->SetSuperStruct(SuperClass->GetAuthoritativeClass());
			}
		}
		
		// relink the new class:
		NewestReinstancer->ClassToReinstance->Bind();
		NewestReinstancer->ClassToReinstance->StaticLink(true);

		NewestReinstancer->ClassToReinstance->ClassFlags &= ~CLASS_ReplicationDataIsSetUp;
		NewestReinstancer->ClassToReinstance->SetUpRuntimeReplicationData();
	}

	// run UpdateBytecodeReferences:
	{
		TSet<UBlueprint*> DependentBPs;
		TMap<FFieldVariant, FFieldVariant> FieldMappings;
		for (FReinstancingJob& ReinstancingJob : Reinstancers)
		{
			if (ReinstancingJob.OldToNew.Key)
			{
				InOutOldToNewClassMap.Add(ReinstancingJob.OldToNew.Key, ReinstancingJob.OldToNew.Value);
			}

			if (ReinstancingJob.Reinstancer.IsValid())
			{
				UBlueprint* CompiledBlueprint = UBlueprint::GetBlueprintFromClass(ReinstancingJob.OldToNew.Value);
				ReinstancingJob.Reinstancer->UpdateBytecodeReferences(DependentBPs, FieldMappings);
			}
		}

		FBlueprintCompileReinstancer::FinishUpdateBytecodeReferences(DependentBPs, FieldMappings);
	}
	
	// Now we can update templates and archetypes - note that we don't look for direct references to archetypes - doing
	// so is very expensive and it will be much faster to directly update anything that cares to cache direct references
	// to an archetype here (e.g. a UClass::ClassDefaultObject member):
	TArray<FReinstancingJob*> ReinstancersPtr;
	ReinstancersPtr.Reset(Reinstancers.Num());
	for (FReinstancingJob& ReinstancingJob : Reinstancers)
	{
		ReinstancersPtr.Add(&ReinstancingJob);
	}

	Algo::TopologicalSort(ReinstancersPtr, [&Reinstancers](FReinstancingJob* ReinstancingJob)
	{
		TArray<FReinstancingJob*> Dependencies;
		auto AddDependentClass = [&Dependencies,&Reinstancers](UClass* DependentClass)
		{
			if (FReinstancingJob* ReinstancingJob = Reinstancers.FindByPredicate([DependentClass](FReinstancingJob& ReinstancingJob) { return ReinstancingJob.OldToNew.Key == DependentClass;}))
			{
				Dependencies.Add(ReinstancingJob);
			}
		};

		if (UClass* OldClass = ReinstancingJob->OldToNew.Key)
		{
			AddDependentClass(OldClass->GetSuperClass());

			if (const UObject* CDO = OldClass->ClassDefaultObject)
			{
				TArray<UObject*> ContainedOldObjects;
				GetObjectsWithOuter(CDO, ContainedOldObjects);
				for (const UObject* OldObject : ContainedOldObjects)
				{
					AddDependentClass(OldObject->GetClass());
				}
			}
		}

		return Dependencies;
	});

	// 2. Update Sparse Class Data
	for (const FReinstancingJob* ReinstancingJobPtr : ReinstancersPtr)
	{
		TSharedPtr<FBlueprintCompileReinstancer> const& Reinstancer = ReinstancingJobPtr->Reinstancer;
		if (!Reinstancer.IsValid())
		{
			continue; // no reinstancer, we're not responsible for recreating this class (e.g. it came from verse compile or asset reload)
		}

		// if the new class already has a sparse class data, that indicates
		// it was generated by its compiler and we can discard the old data:
		UClass* NewClass = ReinstancingJobPtr->OldToNew.Value;
		if (!NewClass || NewClass->GetSparseClassData(EGetSparseClassDataMethod::ReturnIfNull) != nullptr)
		{
			continue;
		}

		// New class has not created a sparse class data, if the old class had one copy it over:
		Reinstancer->PropagateSparseClassDataToNewClass(NewClass);
	}

	// 3. Copy defaults from old CDO - CDO may be missing if this class was reinstanced and relinked here,
	// so use GetDefaultObject(true):
	for (const FReinstancingJob* ReinstancingJobPtr : ReinstancersPtr)
	{
		const FReinstancingJob& ReinstancingJob = *ReinstancingJobPtr;
		UObject* OldCDO = nullptr;
		UClass* OldClass = ReinstancingJob.OldToNew.Key;

		if (OldClass)
		{
			OldCDO = OldClass->ClassDefaultObject;
			if (OldCDO && ReinstancingJob.Reinstancer.IsValid())
			{
				// Object using overridable serialization need to use delta serialization for it do work appropriatly
				// Eventually we should do this to all BP classes CDO no matter what
				const bool bUsingOverrideSerialization = FOverridableManager::Get().IsEnabled(*OldCDO);
				const bool bUseDeltaSerialization = bUsingOverrideSerialization ? true : (ReinstancingJob.Reinstancer.IsValid() ? ReinstancingJob.Reinstancer->bUseDeltaSerializationToCopyProperties : false);
				UClass* NewClass = ReinstancingJob.OldToNew.Value;

				// We do not expect the CDO to be already created at this point. 
				// Who ever bother to create it before didn't look if the parent was ready or not.
				if (NewClass->ClassDefaultObject != nullptr && bUsingOverrideSerialization)
				{
					// Override serialization does not currently want to use any CDO created before this
					// point. I do not have a test case justifying this need, but renaming the cdo here 
					// causes various regressions for existing blueprints, because their compilers have put
					// needed data on the generated CDO. If we want to use override serialization broadly
					// we will have to address these shortcomings (likely by ordering compilation itself,
					// rather than reinstancing, more carefully). Discarding the CDO here is also wasteful
					// - if we cannot use the objects, why create them at all?
					NewClass->ClassDefaultObject->Rename(nullptr, GetTransientPackage(), REN_DoNotDirty | REN_DontCreateRedirectors | REN_ForceNoResetLoaders | REN_NonTransactional);
					NewClass->ClassDefaultObject = nullptr;
				}
				UObject* NewCDO = NewClass->GetDefaultObject(true);

				UBlueprint* CompiledBlueprint = UBlueprint::GetBlueprintFromClass(OldClass);
				if (CompiledBlueprint && CompiledBlueprint->bIsRegeneratingOnLoad)
				{
					// This is a catch-all for any deferred dependencies that didn't get resolved during loading/linking (that system is not bulletproof for complex circular dependencies)
					FBlueprintSupport::RepairDeferredDependenciesInObject(OldCDO);
				}

				TMap<UObject*, UObject*> CreatedInstanceMap;
				FBlueprintCompileReinstancer::PreCreateSubObjectsForReinstantiation(InOutOldToNewClassMap, OldCDO, NewCDO, CreatedInstanceMap);

				// We only need to copy properties of the pre-created instances, the rest of the default sub object is done inside the UEditorEngine::CopyPropertiesForUnrelatedObjects
				TMap<UObject*, UObject*> OldToNewInstanceMap(CreatedInstanceMap);
				if (TMap<UObject*, UObject*>* OldToNewTemplateMap = OldToNewTemplates ? OldToNewTemplates->Find(OldClass->GetSuperClass()) : nullptr)
				{
					OldToNewInstanceMap.Append(*OldToNewTemplateMap);
				}
				for(const auto& Pair : CreatedInstanceMap)
				{
					FBlueprintCompileReinstancer::CopyPropertiesForUnrelatedObjects(Pair.Key, Pair.Value, /*bClearExternalReferences*/true, bUseDeltaSerialization, /*bOnlyHandleDirectSubObjects*/true, &OldToNewInstanceMap, &InOutOldToNewClassMap);
				}

				if (OldToNewTemplates && !OldToNewInstanceMap.IsEmpty())
				{
					OldToNewTemplates->FindOrAdd(OldClass).Append(OldToNewInstanceMap);
				}

				if (ReinstancingJob.Compiler.IsValid())
				{
					ReinstancingJob.Compiler->PropagateValuesToCDO(NewCDO, OldCDO);
				}
			}
		}

		if (UBlueprintGeneratedClass* BPGClass = Cast<UBlueprintGeneratedClass>(ReinstancingJob.OldToNew.Value))
		{
			BPGClass->UpdateCustomPropertyListForPostConstruction();

			// patch new cdo into linker table:
			if(OldCDO && ReinstancingJob.Reinstancer.IsValid())
			{
				UBlueprint* CurrentBP = CastChecked<UBlueprint>(BPGClass->ClassGeneratedBy);
				if(FLinkerLoad* CurrentLinker = CurrentBP->GetLinker())
				{
					int32 OldCDOIndex = INDEX_NONE;

					for (int32 i = 0; i < CurrentLinker->ExportMap.Num(); i++)
					{
						FObjectExport& ThisExport = CurrentLinker->ExportMap[i];
						// In addition to checking for RF_ClassDefaultObject, we also need to check if the object names
						// match to for Control Rig BP use case
						// 
						// In a normal Blueprint, there is usually just 1 CDO in the package, so name checking was not necessary.
						// 
						// But in a Control Rig BP, multiple CDO can be in a package because custom UClasses are used to describe 
						// the layout of RigVM Memory Storage. These UClasses and their CDO are serialzed with the package as well.
						// Thus, we have to ensure that the export is not just a CDO, but also a CDO with the matching name
						if ((ThisExport.ObjectFlags & RF_ClassDefaultObject) && (ThisExport.ObjectName == CurrentBP->GeneratedClass->ClassDefaultObject->GetFName()))
						{
							OldCDOIndex = i;
							break;
						}
					}

					if(OldCDOIndex != INDEX_NONE)
					{
						FBlueprintEditorUtils::PatchNewCDOIntoLinker(CurrentBP->GeneratedClass->ClassDefaultObject, CurrentLinker, OldCDOIndex, InLoadContext);
						FBlueprintEditorUtils::PatchCDOSubobjectsIntoExport(OldCDO, CurrentBP->GeneratedClass->ClassDefaultObject);
					}
				}
			}
		}
	}

	TMap<UObject*, UObject*> OldArchetypeToNewArchetype;

	// 4. Update any remaining instances that are tagged as RF_ArchetypeObject or RF_InheritableComponentTemplate - 
	// we may need to do further sorting to ensure that interdependent archetypes are initialized correctly:
	TSet<UObject*> ArchetypeReferencers;

	// The transaction buffer could reference archetypes, and tag serialization
	// will be simpler if we update the instance:
	if (IsInGameThread() && GUnrealEd && GUnrealEd->Trans)
	{
		ArchetypeReferencers.Add(GUnrealEd->Trans);
	}

	for (FReinstancingJob* ReinstancingJobPtr : ReinstancersPtr)
	{
		FReinstancingJob& ReinstancingJob = *ReinstancingJobPtr;
		UClass* OldClass = ReinstancingJob.OldToNew.Key;
		if (OldClass)
		{
			SCOPED_LOADTIMER_ASSET_TEXT(*WriteToString<256>(TEXT("Reinstancing "), *GetPathNameSafe(ReinstancingJob.OldToNew.Value)));

			UClass* NewClass = ReinstancingJob.OldToNew.Value;
			if (NewClass && 
				OldClass->ClassDefaultObject && 
				NewClass->ClassDefaultObject &&
				OldClass->ClassDefaultObject != NewClass->ClassDefaultObject)
			{
				OldArchetypeToNewArchetype.Add(OldClass->ClassDefaultObject, NewClass->ClassDefaultObject);
				// also map old *default* subobjects to new default subobjects:
				BuildDSOMap(OldClass->ClassDefaultObject, NewClass->ClassDefaultObject, OldArchetypeToNewArchetype);
			}

			TArray<UObject*> ArchetypeObjects;
			GetObjectsOfClass(OldClass, ArchetypeObjects, false);
			
			// filter out non-archetype instances, note that WidgetTrees and some component
			// archetypes do not have RF_ArchetypeObject or RF_InheritableComponentTemplate so
			// we simply detect that they are outered to a UBPGC or UBlueprint and assume that 
			// they are archetype objects in practice:
			ArchetypeObjects.RemoveAllSwap(
				[&InOutOldToNewClassMap](UObject* Obj) 
				{ 
					bool bIsArchetype = 
						Obj->HasAnyFlags(RF_ArchetypeObject|RF_InheritableComponentTemplate)
						|| Obj->GetTypedOuter<UBlueprintGeneratedClass>()
						|| Obj->GetTypedOuter<UBlueprint>();
					// remove if this is not an archetype or its already in the transient package, note
					// that things that are not directly outered to the transient package will be 
					// 'reinst'd', this is specifically to handle components, which need to be up to date
					// on the REINST_ actor class:
					// Also no need to reinstantiate if our outer is also being reinstantiated as an archetype.
					return !bIsArchetype || 
							Obj->GetOutermost() == GetTransientPackage() || 
							Obj->HasAnyFlags(RF_NewerVersionExists) ||
							InOutOldToNewClassMap.Find(Obj->GetOuter()->GetClass()); 
				}
			);

			// for each archetype:
			for (UObject* Archetype : ArchetypeObjects)
			{
				ReinstancingJob.OldArchetypeObjects.Add(Archetype);

				// make sure we fix up references in the owner:
				{
					UObject* Iter = Archetype->GetOuter();
					while(Iter)
					{
						UBlueprintGeneratedClass* IterAsBPGC = Cast<UBlueprintGeneratedClass>(Iter);
						UBlueprint* IterAsBP = Cast<UBlueprint>(Iter);
						if(Iter->HasAnyFlags(RF_ClassDefaultObject)
							|| (IterAsBPGC && !IterAsBPGC->HasAnyClassFlags(CLASS_NewerVersionExists))
							|| IterAsBP)
						{
							ArchetypeReferencers.Add(Iter);

							// Component templates are referenced by the UBlueprint, but are outered to the UBPGC. Both
							// will need to be updated. Realistically there is no reason to refernce these in the 
							// UBlueprint, so there is no reason to generalize this behavior:
							if(IterAsBPGC)
							{
								ArchetypeReferencers.Add(IterAsBPGC->ClassGeneratedBy);
								IterAsBP = Cast<UBlueprint>(IterAsBPGC->ClassGeneratedBy);
							}
							if (IterAsBP)
							{
								if (IterAsBP->SkeletonGeneratedClass)
								{
									ArchetypeReferencers.Add(IterAsBP->SkeletonGeneratedClass);
								}
							}

							// this handles nested subobjects:
							TArray<UObject*> ContainedObjects;
							GetObjectsWithOuter(Iter, ContainedObjects);
							ArchetypeReferencers.Append(ContainedObjects);
						}
						Iter = Iter->GetOuter();
					}
				}
			}

			// Sort all archetype between themselves, as one might depends on an other
			// This happens when the class containing the archetype has derived classes.
			Algo::TopologicalSort(ReinstancingJob.OldArchetypeObjects, [&ArchetypeObjects](const FReinstancingJob::FArchetypeInfo& OldArchetypeInfo)
			{
				TArray<UObject*> Dependencies;
				if (OldArchetypeInfo.ArchetypeTemplate && !OldArchetypeInfo.ArchetypeTemplate->HasAnyFlags(RF_ClassDefaultObject))
				{
					if(ArchetypeObjects.Contains(OldArchetypeInfo.ArchetypeTemplate))
					{
						Dependencies.Add(OldArchetypeInfo.ArchetypeTemplate);
					}
					else
					{
						UE_LOG(LogBlueprint, Warning, TEXT("Expecting the template object (%s) of archetype (%s) to already be in the list of archetypes"), *GetNameSafe(OldArchetypeInfo.ArchetypeTemplate), *GetNameSafe(OldArchetypeInfo.Archetype));
					}
				}

				return Dependencies;
			});
		}
	}

	// This loop finishes the reinstancing of archetypes after the entire Outer hierarchy has been updated with new instances:
	for (const FReinstancingJob* ReinstancingJobPtr : ReinstancersPtr)
	{
		const FReinstancingJob& ReinstancingJob = *ReinstancingJobPtr;
		UClass* OldClass = ReinstancingJob.OldToNew.Key;
		if(OldClass)
		{
			SCOPED_LOADTIMER_ASSET_TEXT(*WriteToString<256>(TEXT("FinishReinstancing "), *GetPathNameSafe(ReinstancingJob.OldToNew.Value)));

			UClass* NewClass = ReinstancingJob.OldToNew.Value;

			TMap<UObject*, UObject*>* OldToNewTemplatesForClass = OldToNewTemplates ? &OldToNewTemplates->FindOrAdd(OldClass) : nullptr;

			for(const FReinstancingJob::FArchetypeInfo& OldArchetypeInfo : ReinstancingJob.OldArchetypeObjects)
			{
				UObject* OldInstance = OldArchetypeInfo.Archetype;

				// move aside:
				FName OriginalName = OldInstance->GetFName();
				UObject* OriginalOuter = OldInstance->GetOuter();
				EObjectFlags OriginalFlags = OldInstance->GetFlags();

				UObject* Destination = GetTransientOuterForRename(OldInstance->GetClass());
				OldInstance->Rename(
					nullptr,
					// destination - this is the important part of this call. Moving the object 
					// out of the way so we can reuse its name:
					Destination, 
					// Rename options:
					REN_DoNotDirty | REN_DontCreateRedirectors | REN_ForceNoResetLoaders );

				// reconstruct
				FMakeClassSpawnableOnScope TemporarilySpawnable(NewClass);
				const EObjectFlags FlagMask = RF_Public | RF_ArchetypeObject | RF_Transactional | RF_Transient | RF_TextExportTransient | RF_InheritableComponentTemplate | RF_Standalone; //TODO: what about RF_RootSet?

				UObject* Template = nullptr;
				if (UObject** NewArchetypeTemplate = OldArchetypeInfo.ArchetypeTemplate ? OldArchetypeToNewArchetype.Find(OldArchetypeInfo.ArchetypeTemplate) : nullptr)
				{
					Template = *NewArchetypeTemplate;
				}
				UObject* NewArchetype = NewObject<UObject>(OriginalOuter, NewClass, OriginalName, OriginalFlags & FlagMask, Template);

				OldArchetypeToNewArchetype.Add(OldInstance, NewArchetype);
				if (OldToNewTemplatesForClass)
				{
					OldToNewTemplatesForClass->Add(OldInstance, NewArchetype);
				}

				// also map old *default* subobjects to new default subobjects:
				BuildDSOMap(OldInstance, NewArchetype, OldArchetypeToNewArchetype);

				ArchetypeReferencers.Add(NewArchetype);

				FLinkerLoad::PRIVATE_PatchNewObjectIntoExport(OldInstance, NewArchetype);

				// NewArchetype may be null in the case of deleted or EditorOnly (in -game) DSOs:
				if(NewArchetype)
				{
					// The new object hierarchy has been created, all of the old instances are in the transient package and new
					// ones have taken their place. Reference members will mostly be pointing at *old* instances, and will get fixed
					// up below:			
					const bool bUseDeltaSerialization = ReinstancingJob.Reinstancer.IsValid() ? ReinstancingJob.Reinstancer->bUseDeltaSerializationToCopyProperties : false;

					TMap<UObject*, UObject*> CreatedInstanceMap;
					FBlueprintCompileReinstancer::PreCreateSubObjectsForReinstantiation(InOutOldToNewClassMap, OldInstance, NewArchetype, CreatedInstanceMap);

					// We only need to copy properties of the pre-created instances, the rest of the default sub object is done inside the UEditorEngine::CopyPropertiesForUnrelatedObjects
					TMap<UObject*, UObject*> OldToNewInstanceMap(CreatedInstanceMap);
					if (TMap<UObject*, UObject*>* OldToNewTemplateMap = OldToNewTemplates ? OldToNewTemplates->Find(OldClass->GetSuperClass()) : nullptr)
					{
						OldToNewInstanceMap.Append(*OldToNewTemplateMap);
					}
					for (const auto& Pair : CreatedInstanceMap)
					{
						FBlueprintCompileReinstancer::CopyPropertiesForUnrelatedObjects(Pair.Key, Pair.Value, /*bClearExternalReferences*/true, bUseDeltaSerialization, /*bOnlyHandleDirectSubObjects*/true, &OldToNewInstanceMap, &InOutOldToNewClassMap);
					}

					if (OldToNewTemplates && !OldToNewInstanceMap.IsEmpty())
					{
						OldToNewTemplates->FindOrAdd(OldClass).Append(OldToNewInstanceMap);
					}

					OldInstance->RemoveFromRoot();
					OldInstance->MarkAsGarbage();
				}
			}
		}
	}

	// Reassociate relevant property bags
	UE::FPropertyBagRepository::Get().ReassociateObjects(OldArchetypeToNewArchetype);
	
	// 5. update known references to archetypes (e.g. component templates, WidgetTree). We don't want to run the normal 
	// reference finder to update these because searching the entire object graph is time consuming. Instead we just replace
	// all references in our UBlueprint and its generated class:
	for (const FReinstancingJob* ReinstancingJobPtr : ReinstancersPtr)
	{
		const FReinstancingJob& ReinstancingJob = *ReinstancingJobPtr;
		ArchetypeReferencers.Add(ReinstancingJob.OldToNew.Value);
		ArchetypeReferencers.Add(ReinstancingJob.OldToNew.Value->ClassGeneratedBy);

		if(!ReinstancingJob.Reinstancer.IsValid())
		{
			continue;
		}

		if(UBlueprint* BP = Cast<UBlueprint>(ReinstancingJob.OldToNew.Value->ClassGeneratedBy))
		{
			// The only known way to cause this ensure to trip is to enqueue blueprints for compilation
			// while blueprints are already compiling:
			if( ensure(BP->SkeletonGeneratedClass) )
			{
				ArchetypeReferencers.Add(BP->SkeletonGeneratedClass);
			}
			for(const TWeakObjectPtr<UBlueprint>& Dependency : BP->CachedDependencies)
			{
				if (UBlueprint* DependencyBP = Dependency.Get())
				{
					ArchetypeReferencers.Add(DependencyBP);
				}
			}
		}
	}

	// Data loading may be in flight, lets immediately patch existing redirectors - 
	// we may want to search the entire graph some day, but that would be expensive:
	for (TObjectIterator<UObjectRedirector> Itr; Itr; ++Itr)
	{
		if (Itr->DestinationObject && 
			Itr->DestinationObject->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject))
		{
			ArchetypeReferencers.Add(*Itr);
		}
	}

	for(UObject* ArchetypeReferencer : ArchetypeReferencers)
	{
		// Do not bother trying to replace references in referencers that are not valid
		if (IsValid(ArchetypeReferencer))
		{
			FArchiveReplaceObjectRef<UObject> ReplaceInCDOAr(ArchetypeReferencer, OldArchetypeToNewArchetype);
		}
	}
}

/*
	This function completely replaces the 'skeleton only' compilation pass in the Kismet compiler. Long
	term that code path will be removed and clients will be redirected to this function.

	Notes to maintainers: any UObject created here and outered to the resulting class must be marked as transient
	or you will create a cook error!
*/
UClass* FBlueprintCompilationManagerImpl::FastGenerateSkeletonClass(UBlueprint* BP, FKismetCompilerContext& CompilerContext, bool bIsSkeletonOnly, TArray<FSkeletonFixupData>& OutSkeletonFixupData)
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	FCompilerResultsLog MessageLog;

	UClass* ParentClass = BP->ParentClass;
	if(ParentClass == nullptr)
	{
		return nullptr;
	}

	if(ParentClass->ClassGeneratedBy)
	{
		if(UBlueprint* ParentBP = Cast<UBlueprint>(ParentClass->ClassGeneratedBy))
		{
			if(ParentBP->SkeletonGeneratedClass)
			{
				ParentClass = ParentBP->SkeletonGeneratedClass;
			}
		}
	}

	UBlueprintGeneratedClass* Ret = nullptr;
	UBlueprintGeneratedClass* OriginalNewClass = CompilerContext.NewClass;
	FString SkelClassName = FString::Printf(TEXT("SKEL_%s_C"), *BP->GetName());

	// Temporarily set the compile type to indicate that we're generating the skeleton class.
	TGuardValue<EKismetCompileType::Type> GuardCompileType(CompilerContext.CompileOptions.CompileType, EKismetCompileType::SkeletonOnly);

	if (BP->SkeletonGeneratedClass == nullptr)
	{
		// This might exist in the package because we are being reloaded in place
		BP->SkeletonGeneratedClass = FindObject<UBlueprintGeneratedClass>(BP->GetOutermost(), *SkelClassName);
	}

	if (BP->SkeletonGeneratedClass == nullptr)
	{
		CompilerContext.SpawnNewClass(SkelClassName);
		Ret = CompilerContext.NewClass;
		Ret->SetFlags(RF_Transient);
		CompilerContext.NewClass = OriginalNewClass;
	}
	else
	{
		Ret = CastChecked<UBlueprintGeneratedClass>(*(BP->SkeletonGeneratedClass));

		// If we're changing the parent class, first validate variable names against the inherited set to avoid collisions when we create properties.
		if (Ret->GetSuperClass() != ParentClass)
		{
			CompilerContext.ValidateVariableNames();
		}

		CompilerContext.CleanAndSanitizeClass(Ret, MutableView(Ret->ClassDefaultObject));
	}
	
	Ret->ClassGeneratedBy = BP;

	// This is a version of PrecompileFunction that does not require 'terms' and graph cloning:
	const auto MakeFunction = [Ret, ParentClass, Schema, BP, &MessageLog, &OutSkeletonFixupData]
		(	FName FunctionNameFName, 
			UField**& InCurrentFieldStorageLocation, 
			FField**& InCurrentParamStorageLocation, 
			EFunctionFlags InFunctionFlags, 
			const TArray<UK2Node_FunctionResult*>& ReturnNodes, 
			const TArray<UEdGraphPin*>& InputPins,
			bool bIsStaticFunction, 
			bool bForceArrayStructRefsConst, 
			UFunction* SignatureOverride) -> UFunction*
	{
		if(!ensure(FunctionNameFName != FName())
			|| FindObjectFast<UField>(Ret, FunctionNameFName))
		{
			return nullptr;
		}
		
		UFunction* NewFunction = NewObject<UFunction>(Ret, FunctionNameFName, RF_Public|RF_Transient);
					
		Ret->AddFunctionToFunctionMap(NewFunction, NewFunction->GetFName());

		*InCurrentFieldStorageLocation = NewFunction;
		InCurrentFieldStorageLocation = &NewFunction->Next;

		if(bIsStaticFunction)
		{
			NewFunction->FunctionFlags |= FUNC_Static;
		}

		UFunction* ParentFn = ParentClass->FindFunctionByName(NewFunction->GetFName());

		// Set the parent function prior to searching for a corresponding interface class function. This matches what
		// is done in the full path (@see FKismetCompilerContext::PrecompileFunction). It is ok for this to be NULL.
		NewFunction->SetSuperStruct(ParentFn);

		if(ParentFn == nullptr)
		{
			// check for function in implemented interfaces:
			for(const FBPInterfaceDescription& BPID : BP->ImplementedInterfaces)
			{
				// we only want the *skeleton* version of the function:
				UClass* InterfaceClass = BPID.Interface;
				// We need to null check because FBlueprintEditorUtils::ConformImplementedInterfaces won't run until 
				// after the skeleton classes have been generated:
				if(InterfaceClass)
				{
					if(UBlueprint* Owner = Cast<UBlueprint>(InterfaceClass->ClassGeneratedBy))
					{
						if( ensure(Owner->SkeletonGeneratedClass) )
						{
							InterfaceClass = Owner->SkeletonGeneratedClass;
						}
					}

					if(UFunction* ParentInterfaceFn = InterfaceClass->FindFunctionByName(NewFunction->GetFName()))
					{
						ParentFn = ParentInterfaceFn;
						break;
					}
				}
			}
		}
		
		InCurrentParamStorageLocation = &NewFunction->ChildProperties;

		// params:
		if(ParentFn || SignatureOverride)
		{
			UFunction* SignatureFn = ParentFn ? ParentFn : SignatureOverride;
			NewFunction->FunctionFlags |= (SignatureFn->FunctionFlags & (FUNC_FuncInherit | FUNC_Public | FUNC_Protected | FUNC_Private | FUNC_BlueprintPure));
			for (TFieldIterator<FProperty> PropIt(SignatureFn); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
			{
				FProperty* ClonedParam = CastField<FProperty>(FField::Duplicate(*PropIt, NewFunction, PropIt->GetFName(), RF_AllFlags, EInternalObjectFlags_AllFlags & ~(EInternalObjectFlags::Native)));
				check(ClonedParam);
				ClonedParam->PropertyFlags |= CPF_BlueprintVisible|CPF_BlueprintReadOnly;
				ClonedParam->Next = nullptr;
				*InCurrentParamStorageLocation = ClonedParam;
				InCurrentParamStorageLocation = &ClonedParam->Next;
			}
			UMetaData::CopyMetadata(SignatureFn, NewFunction);
		}
		else
		{
			NewFunction->FunctionFlags |= InFunctionFlags;
			for(UEdGraphPin* Pin : InputPins)
			{
				if(Pin->Direction == EEdGraphPinDirection::EGPD_Output && !Schema->IsExecPin(*Pin) && Pin->ParentPin == nullptr && Pin->GetFName() != UK2Node_Event::DelegateOutputName)
				{
					// Reimplementation of FKismetCompilerContext::CreatePropertiesFromList without dependence on 'terms'
					FProperty* Param = FKismetCompilerUtilities::CreatePropertyOnScope(NewFunction, Pin->PinName, Pin->PinType, Ret, CPF_BlueprintVisible|CPF_BlueprintReadOnly, Schema, MessageLog);
					if(Param)
					{
						Param->SetFlags(RF_Transient);
						Param->PropertyFlags |= CPF_Parm;
						if(Pin->PinType.bIsReference)
						{
							Param->PropertyFlags |= CPF_ReferenceParm | CPF_OutParm;
						}

						if(Pin->PinType.bIsConst || (bForceArrayStructRefsConst && (Pin->PinType.IsArray() || Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct) && Pin->PinType.bIsReference))
						{
							Param->PropertyFlags |= CPF_ConstParm;
						}

						if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Param))
						{
							UClass* EffectiveClass = nullptr;
							if (ObjProp->PropertyClass != nullptr)
							{
								EffectiveClass = ObjProp->PropertyClass;
							}
							else if (FClassProperty* ClassProp = CastField<FClassProperty>(ObjProp))
							{
								EffectiveClass = ClassProp->MetaClass;
							}

							if ((EffectiveClass != nullptr) && (EffectiveClass->HasAnyClassFlags(CLASS_Const)))
							{
								Param->PropertyFlags |= CPF_ConstParm;
							}
						}
						else if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Param))
						{
							Param->PropertyFlags |= CPF_ReferenceParm;

							// ALWAYS pass array parameters as out params, so they're set up as passed by ref
							Param->PropertyFlags |= CPF_OutParm;
						}
						// Delegate properties have a direct reference to a UFunction that we may currently be generating, so we're going
						// to track them and fix them after all UFunctions have been generated. As you can tell we're tightly coupled
						// to the implementation of CreatePropertyOnScope
						else if( FDelegateProperty* DelegateProp = CastField<FDelegateProperty>(Param))
						{
							OutSkeletonFixupData.Add( {
								Pin->PinType.PinSubCategoryMemberReference,
								DelegateProp
							} );
						}
						else if( FMulticastDelegateProperty* MCDelegateProp = CastField<FMulticastDelegateProperty>(Param))
						{
							OutSkeletonFixupData.Add( {
								Pin->PinType.PinSubCategoryMemberReference,
								MCDelegateProp
							} );
						}

						*InCurrentParamStorageLocation = Param;
						InCurrentParamStorageLocation = &Param->Next;
					}
				}
			}
			
			if(ReturnNodes.Num() > 0)
			{
				// Gather all input pins on these nodes, these are 
				// the outputs of the function:
				TSet<FName> UsedPinNames;
				for(UK2Node_FunctionResult* Node : ReturnNodes)
				{
					for(UEdGraphPin* Pin : Node->Pins)
					{
						if(!Schema->IsExecPin(*Pin) && Pin->ParentPin == nullptr)
						{								
							if(!UsedPinNames.Contains(Pin->PinName))
							{
								UsedPinNames.Add(Pin->PinName);
							
								FProperty* Param = FKismetCompilerUtilities::CreatePropertyOnScope(NewFunction, Pin->PinName, Pin->PinType, Ret, CPF_None, Schema, MessageLog);
								if(Param)
								{
									Param->SetFlags(RF_Transient);
									// we only tag things as CPF_ReturnParm if the value is named ReturnValue.... this is *terrible* behavior:
									if(Param->GetFName() == UEdGraphSchema_K2::PN_ReturnValue)
									{
										Param->PropertyFlags |= CPF_ReturnParm;
									}
									Param->PropertyFlags |= CPF_Parm|CPF_OutParm;
									*InCurrentParamStorageLocation = Param;
									InCurrentParamStorageLocation = &Param->Next;
								}
							}
						}
					}
				}
			}
		}

		// We're linking the skeleton function because TProperty::LinkInternal
		// will assign add TTypeFundamentals::GetComputedFlagsPropertyFlags()
		// to PropertyFlags. PropertyFlags must (mostly) match in order for 
		// functions to be compatible:
		NewFunction->StaticLink(true);
		return NewFunction;
	};


	// helpers:
	const auto AddFunctionForGraphs = [Schema, &MessageLog, ParentClass, Ret, BP, MakeFunction, &CompilerContext](const TCHAR* FunctionNamePostfix, const TArray<UEdGraph*>& Graphs, UField**& InCurrentFieldStorageLocation, bool bIsStaticFunction, bool bAreDelegateGraphs)
	{
		for( const UEdGraph* Graph : Graphs )
		{
			TArray<UK2Node_FunctionEntry*> EntryNodes;
			Graph->GetNodesOfClass(EntryNodes);
			if(EntryNodes.Num() > 0)
			{
				TArray<UK2Node_FunctionResult*> ReturnNodes;
				Graph->GetNodesOfClass(ReturnNodes);
				UK2Node_FunctionEntry* EntryNode = EntryNodes[0];
				FName NewFunctionName = (EntryNode->CustomGeneratedFunctionName != NAME_None) ? EntryNode->CustomGeneratedFunctionName : Graph->GetFName();

				FField** CurrentParamStorageLocation = nullptr;
				UFunction* NewFunction = MakeFunction(
					FName(*(NewFunctionName.ToString() + FunctionNamePostfix)), 
					InCurrentFieldStorageLocation, 
					CurrentParamStorageLocation, 
					(EFunctionFlags)(EntryNode->GetFunctionFlags() & ~FUNC_Native),
					ReturnNodes, 
					EntryNode->Pins,
					bIsStaticFunction, 
					false,
					nullptr
				);

				if(NewFunction)
				{
					if(bAreDelegateGraphs)
					{
						NewFunction->FunctionFlags |= FUNC_Delegate;
					}

					// locals:
					for( const FBPVariableDescription& BPVD : EntryNode->LocalVariables )
					{
						if(FProperty* LocalVariable = FKismetCompilerContext::CreateUserDefinedLocalVariableForFunction(BPVD, NewFunction, Ret, CurrentParamStorageLocation, Schema, MessageLog) )
						{
							LocalVariable->SetFlags(RF_Transient);
						}
					}

					// __WorldContext:
					if(bIsStaticFunction)
					{
						if( FindFProperty<FObjectProperty>(NewFunction, TEXT("__WorldContext")) == nullptr )
						{
							FEdGraphPinType WorldContextPinType(UEdGraphSchema_K2::PC_Object, NAME_None, UObject::StaticClass(), EPinContainerType::None, false, FEdGraphTerminalType());
							FProperty* Param = FKismetCompilerUtilities::CreatePropertyOnScope(NewFunction, TEXT("__WorldContext"), WorldContextPinType, Ret, CPF_None, Schema, MessageLog);
							if(Param)
							{
								Param->SetFlags(RF_Transient);
								Param->PropertyFlags |= CPF_Parm;
								*CurrentParamStorageLocation = Param;
								CurrentParamStorageLocation = &Param->Next;
							}
						}
						
						// set the metdata:
						NewFunction->SetMetaData(FBlueprintMetadata::MD_WorldContext, TEXT("__WorldContext"));
					}

					CompilerContext.SetCalculatedMetaDataAndFlags(NewFunction, EntryNode, Schema);

					if (EntryNode->MetaData.HasMetaData(FBlueprintMetadata::MD_FieldNotify) && Ret->ImplementsInterface(UNotifyFieldValueChanged::StaticClass()))
					{
						ensure(!Ret->FieldNotifies.Contains(FFieldNotificationId(NewFunction->GetFName())));
						Ret->FieldNotifies.Add(FFieldNotificationId(NewFunction->GetFName()));
					}
				}

				if (BP->bIsRegeneratingOnLoad)
				{
					// Ensure that the function's variable cache is up-to-date after property creation. Note that
					// this may incur a load if the variable's default value (a string) refers to an external asset;
					// that won't result in a package import until after the BP is compiled, and sometimes users will
					// save the Blueprint after having set the variable's default value without also recompiling it.
					// In that case we want to ensure these assets are loaded as part of regenerating classes on load.
					EntryNode->RefreshFunctionVariableCache();
				}
			}
		}
	};


	auto RetChildrenScope = MutableView(Ret->Children);
	UField** CurrentFieldStorageLocation = ToRawPtr(RetChildrenScope);
	
	// Helper function for making UFunctions generated for 'event' nodes, e.g. custom event and timelines
	const auto MakeEventFunction = [&CurrentFieldStorageLocation, MakeFunction, Schema]( FName InName, EFunctionFlags ExtraFnFlags, const TArray<UEdGraphPin*>& InputPins, const TArray< TSharedPtr<FUserPinInfo> >& UserPins, UFunction* InSourceFN, bool bInCallInEditor, bool bIsDeprecated, const FString& DeprecationMessage, FKismetUserDeclaredFunctionMetadata* UserDefinedMetaData = nullptr)
	{
		FField** CurrentParamStorageLocation = nullptr;

		UFunction* NewFunction = MakeFunction(
			InName, 
			CurrentFieldStorageLocation, 
			CurrentParamStorageLocation, 
			ExtraFnFlags|FUNC_BlueprintCallable|FUNC_BlueprintEvent,
			TArray<UK2Node_FunctionResult*>(), 
			InputPins,
			false, 
			true,
			InSourceFN
		);

		if(NewFunction)
		{
			FKismetCompilerContext::SetDefaultInputValueMetaData(NewFunction, UserPins);

			if (bIsDeprecated)
			{
				NewFunction->SetMetaData(FBlueprintMetadata::MD_DeprecatedFunction, TEXT("true"));
				if (!DeprecationMessage.IsEmpty())
				{
					NewFunction->SetMetaData(FBlueprintMetadata::MD_DeprecationMessage, *DeprecationMessage);
				}
			}

			if(bInCallInEditor)
			{
				NewFunction->SetMetaData(FBlueprintMetadata::MD_CallInEditor, TEXT( "true" ));
			}

			if (UserDefinedMetaData)
			{
				NewFunction->SetMetaData(FBlueprintMetadata::MD_FunctionKeywords, *(UserDefinedMetaData->Keywords).ToString());
			}

			NewFunction->Bind();
			NewFunction->StaticLink(true);
		}
	};

	const auto CreateDelegateProxyFunctions = [Ret, &CompilerContext](UField**& InCurrentFieldStorageLocation)
	{
		for (const auto& DelegateInfo : CompilerContext.ConvertibleDelegates)
		{
			check(DelegateInfo.Key);

			const UFunction* DelegateSignature = DelegateInfo.Key->GetDelegateSignature();

			// The original signature function is actually a UDelegateFunction type.
			// When we create our own function from the original, we need to ensure that it's a standard UFunction.

			UObject* NewObject =
				StaticDuplicateObject(DelegateSignature, Ret, DelegateInfo.Value.ProxyFunctionName, RF_AllFlags, UFunction::StaticClass());
			UFunction* NewFunction = CastChecked<UFunction>(NewObject);

			NewFunction->FunctionFlags &= ~(FUNC_Delegate | FUNC_MulticastDelegate);

			*InCurrentFieldStorageLocation = NewFunction;
			InCurrentFieldStorageLocation = &NewFunction->Next;
		}
	};

	Ret->SetSuperStruct(ParentClass);
	
	Ret->ClassFlags |= (ParentClass->ClassFlags & CLASS_Inherit);
	Ret->ClassCastFlags |= ParentClass->ClassCastFlags;
	
	if (FBlueprintEditorUtils::IsInterfaceBlueprint(BP))
	{
		Ret->ClassFlags |= CLASS_Interface;
	}

	// link in delegate signatures, variables will reference these 
	AddFunctionForGraphs(HEADER_GENERATED_DELEGATE_SIGNATURE_SUFFIX, BP->DelegateSignatureGraphs, CurrentFieldStorageLocation, false, true);

	// handle event entry ponts (mostly custom events) - this replaces
	// the skeleton compile pass CreateFunctionStubForEvent call:
	TArray<UEdGraph*> AllEventGraphs;
	
	for (UEdGraph* UberGraph : BP->UbergraphPages)
	{
		AllEventGraphs.Add(UberGraph);
		UberGraph->GetAllChildrenGraphs(AllEventGraphs);
	}

	for( const UEdGraph* Graph : AllEventGraphs )
	{
		TArray<UK2Node_Event*> EventNodes;
		Graph->GetNodesOfClass(EventNodes);
		for( UK2Node_Event* Event : EventNodes )
		{
			FString DeprecationMessage;
			bool bIsDeprecated = false;
			bool bCallInEditor = false;
			FKismetUserDeclaredFunctionMetadata* UserMetaData = nullptr;
			if(UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Event))
			{
				bCallInEditor = CustomEvent->bCallInEditor;
				bIsDeprecated = CustomEvent->bIsDeprecated;
				if (bIsDeprecated)
				{
					DeprecationMessage = CustomEvent->DeprecationMessage;
				}
				UserMetaData = &(CustomEvent->GetUserDefinedMetaData());
			}
			MakeEventFunction(
				CompilerContext.GetEventStubFunctionName(Event), 
				(EFunctionFlags)Event->FunctionFlags, 
				Event->Pins, 
				Event->UserDefinedPins,
				Event->FindEventSignatureFunction(),
				bCallInEditor,
				bIsDeprecated,
				DeprecationMessage,
				UserMetaData
			);
		}
	}
	
	for (UTimelineTemplate* Timeline : BP->Timelines)
	{
		if(Timeline)
		{
			// If the timeline hasn't gone through post load that means that the cache isn't up to date, so force an update on it
			if (Timeline->HasAllFlags(RF_NeedPostLoad) && Timeline->GetLinkerCustomVersion(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::StoreTimelineNamesInTemplate)
			{
				FUpdateTimelineCachedNames::Execute(Timeline);
			}

			for (const FTTEventTrack& EventTrack : Timeline->EventTracks)
			{
				MakeEventFunction(EventTrack.GetFunctionName(), EFunctionFlags::FUNC_None, TArray<UEdGraphPin*>(), TArray< TSharedPtr<FUserPinInfo> >(), nullptr, false, false, FString());
			}
		
			MakeEventFunction(Timeline->GetUpdateFunctionName(), EFunctionFlags::FUNC_None, TArray<UEdGraphPin*>(), TArray< TSharedPtr<FUserPinInfo> >(), nullptr, false, false, FString());
			MakeEventFunction(Timeline->GetFinishedFunctionName(), EFunctionFlags::FUNC_None, TArray<UEdGraphPin*>(), TArray< TSharedPtr<FUserPinInfo> >(), nullptr, false, false, FString());
		}
	}

	{
		CompilerContext.NewClass = Ret;
		CompilerContext.RegisterClassDelegateProxiesFromBlueprint();
		CompilerContext.NewClass = OriginalNewClass;
	}

	{
		CompilerContext.NewClass = Ret;
		TGuardValue<bool> GuardAssignDelegateSignatureFunction( CompilerContext.bAssignDelegateSignatureFunction, true);
		CompilerContext.CreateClassVariablesFromBlueprint();
		CompilerContext.NewClass = OriginalNewClass;
	}

	UField* Iter = Ret->Children;
	while(Iter)
	{
		CurrentFieldStorageLocation = &Iter->Next;
		Iter = Iter->Next;
	}
	
	CreateDelegateProxyFunctions(CurrentFieldStorageLocation);

	AddFunctionForGraphs(TEXT(""), BP->FunctionGraphs, CurrentFieldStorageLocation, BPTYPE_FunctionLibrary == BP->BlueprintType, false);
	AddFunctionForGraphs(TEXT(""), CompilerContext.GeneratedFunctionGraphs, CurrentFieldStorageLocation, BPTYPE_FunctionLibrary == BP->BlueprintType, false);

	// Add interface functions, often these are added by normal detection of implemented functions, but they won't be
	// if the interface is added but the function is not implemented:
	for(const FBPInterfaceDescription& BPID : BP->ImplementedInterfaces)
	{
		UClass* InterfaceClass = BPID.Interface;
		// Again, once the skeleton has been created we will purge null ImplementedInterfaces entries,
		// but not yet:
		if(InterfaceClass)
		{
			if(UBlueprint* Owner = Cast<UBlueprint>(InterfaceClass->ClassGeneratedBy))
			{
				if( ensure(Owner->SkeletonGeneratedClass) )
				{
					InterfaceClass = Owner->SkeletonGeneratedClass;
				}
			}

			AddFunctionForGraphs(TEXT(""), BPID.Graphs, CurrentFieldStorageLocation, BPTYPE_FunctionLibrary == BP->BlueprintType, false);

			for (TFieldIterator<UFunction> FunctionIt(InterfaceClass, EFieldIteratorFlags::ExcludeSuper); FunctionIt; ++FunctionIt)
			{
				UFunction* Fn = *FunctionIt;
			
				FField** CurrentParamStorageLocation = nullptr;

				// Note that MakeFunction will early out if the function was created above:
				MakeFunction(
					Fn->GetFName(), 
					CurrentFieldStorageLocation, 
					CurrentParamStorageLocation, 
					Fn->FunctionFlags & ~FUNC_Native, 
					TArray<UK2Node_FunctionResult*>(), 
					TArray<UEdGraphPin*>(),
					false, 
					false,
					nullptr
				);
			}
		}
	}

	// Add the uber graph frame just so that we match the old skeleton class's layout. This will be removed in 4.20:
	if (CompilerContext.UsePersistentUberGraphFrame() && AllEventGraphs.Num() != 0)
	{
		//UBER GRAPH PERSISTENT FRAME
		FEdGraphPinType Type(TEXT("struct"), NAME_None, FPointerToUberGraphFrame::StaticStruct(), EPinContainerType::None, false, FEdGraphTerminalType());
		CompilerContext.NewClass = Ret;
		FProperty* Property = CompilerContext.CreateVariable(UBlueprintGeneratedClass::GetUberGraphFrameName(), Type);
		CompilerContext.NewClass = OriginalNewClass;
		Property->SetPropertyFlags(CPF_DuplicateTransient | CPF_Transient);
	}

	CompilerContext.NewClass = Ret;
	CompilerContext.bAssignDelegateSignatureFunction = true;
	CompilerContext.FinishCompilingClass(Ret);
	CompilerContext.bAssignDelegateSignatureFunction = false;
	CompilerContext.NewClass = OriginalNewClass;

	Ret->GetDefaultObject()->SetFlags(RF_Transient);

	return Ret;
}

bool FBlueprintCompilationManagerImpl::IsQueuedForCompilation(UBlueprint* BP)
{
	return BP->bQueuedForCompilation;
}

void FBlueprintCompilationManagerImpl::ConformToParentAndInterfaces(UBlueprint* BP)
{
	// If graphs are renamed the blueprint will be marked as not 'bCachedDependenciesUpToDate', but 
	// because we're conforming an existing dependency we don't need to change bCachedDependenciesUpToDate:
	TGuardValue<bool> LockDependenciesUpToDate(BP->bCachedDependenciesUpToDate, BP->bCachedDependenciesUpToDate);

	// Make sure that this blueprint is up-to-date with regards to its parent functions
	FBlueprintEditorUtils::ConformCallsToParentFunctions(BP);

	// Conform implemented events here, to ensure we generate custom events if necessary after reparenting
	FBlueprintEditorUtils::ConformImplementedEvents(BP);

	// Conform implemented interfaces here, to ensure we generate all functions required by the interface as stubs
	FBlueprintEditorUtils::ConformImplementedInterfaces(BP);

	// Make sure we don't have any signature graphs with no corresponding variable - some assets have
	// managed to get into this state - the UI does not provide a way to fix these objects manually
	FBlueprintEditorUtils::ConformDelegateSignatureGraphs(BP);
}

void FBlueprintCompilationManagerImpl::RelinkSkeleton(UClass* SkeletonToRelink)
{
	// CDO needs to be moved aside already:
	ensure(SkeletonToRelink->ClassDefaultObject == nullptr);
	ensure(!SkeletonToRelink->GetSuperClass()->HasAnyClassFlags(CLASS_NewerVersionExists));

	SkeletonToRelink->ClassConstructor = nullptr;
	SkeletonToRelink->ClassVTableHelperCtorCaller = nullptr;
	SkeletonToRelink->CppClassStaticFunctions.Reset();
	SkeletonToRelink->Bind();
	SkeletonToRelink->ClearFunctionMapsCaches();
	SkeletonToRelink->StaticLink(true);
	SkeletonToRelink->GetDefaultObject()->SetFlags(RF_Transient);

	// Update UFunction SuperStruct pointers, which are typically set to their overridden function.
	// For non-skeleton classes these are reassigned by the 'bytecode' recompile that we run on all
	// referencing classes...
	for (TFieldIterator<UFunction> FuncIter(SkeletonToRelink, EFieldIteratorFlags::ExcludeSuper); FuncIter; ++FuncIter)
	{
		if (UFunction* SuperFunction = SkeletonToRelink->GetSuperClass()->FindFunctionByName(FuncIter->GetFName()))
		{
			FuncIter->SetSuperStruct(SuperFunction);
		}
	}
}

// FFixupBytecodeReferences Implementation:
FBlueprintCompilationManagerImpl::FFixupBytecodeReferences::FFixupBytecodeReferences(UObject* InObject)
{
	ArIsObjectReferenceCollector = true;
		
	InObject->Serialize(*this);
	class FArchiveProxyCollector : public FReferenceCollector
	{
		/** Archive we are a proxy for */
		FArchive& Archive;
	public:
		FArchiveProxyCollector(FArchive& InArchive)
			: Archive(InArchive)
		{
		}
		virtual void HandleObjectReference(UObject*& Object, const UObject* ReferencingObject, const FProperty* ReferencingProperty) override
		{
			Archive << Object;
		}
		virtual void HandleObjectReferences(UObject** InObjects, const int32 ObjectNum, const UObject* InReferencingObject, const FProperty* InReferencingProperty) override
		{
			for (int32 ObjectIndex = 0; ObjectIndex < ObjectNum; ++ObjectIndex)
			{
				UObject*& Object = InObjects[ObjectIndex];
				Archive << Object;
			}
		}
		virtual bool IsIgnoringArchetypeRef() const override
		{
			return false;
		}
		virtual bool IsIgnoringTransient() const override
		{
			return false;
		}
	} ArchiveProxyCollector(*this);
		
	InObject->GetClass()->CallAddReferencedObjects(InObject, ArchiveProxyCollector);
}

FArchive& FBlueprintCompilationManagerImpl::FFixupBytecodeReferences::operator<<( UObject*& Obj )
{
	if (Obj != NULL)
	{
		if(UClass* RelatedClass = Cast<UClass>(Obj))
		{
			UClass* NewClass = RelatedClass->GetAuthoritativeClass();
			ensure(NewClass);
			if(NewClass != RelatedClass)
			{
				Obj = NewClass;
			}
		}
		else if(UField* AsField = Cast<UField>(Obj))
		{
			UClass* OwningClass = AsField->GetOwnerClass();
			if(OwningClass)
			{
				UClass* NewClass = OwningClass->GetAuthoritativeClass();
				ensure(NewClass);
				if(NewClass != OwningClass)
				{
					// drill into new class finding equivalent object:
					TArray<FName> Names;
					UObject* Iter = Obj;
					while (Iter && Iter != OwningClass)
					{
						Names.Add(Iter->GetFName());
						Iter = Iter->GetOuter();
					}

					UObject* Owner = NewClass;
					UObject* Match = nullptr;
					for(int32 I = Names.Num() - 1; I >= 0; --I)
					{
						UObject* Next = StaticFindObjectFast( UObject::StaticClass(), Owner, Names[I]);
						if( Next )
						{
							if(I == 0)
							{
								Match = Next;
							}
							else
							{
								Owner = Match;
							}
						}
						else
						{
							break;
						}
					}
						
					if(Match)
					{
						Obj = Match;
					}
				}
			}
		}
	}
	return *this;
}

FArchive& FBlueprintCompilationManagerImpl::FFixupBytecodeReferences::operator<<(FField*& Field)
{
	if (Field != nullptr)
	{
		UClass* OwningClass = Field->GetOwnerClass();
		if (OwningClass)
		{
			UClass* NewClass = OwningClass->GetAuthoritativeClass();
			ensure(NewClass);
			if (NewClass && NewClass != OwningClass)
			{
				// drill into new class finding equivalent object:
				TArray<FFieldVariant> OwnerChain;
				for (FFieldVariant Iter = Field; Iter.IsValid() && Iter.Get<UObject>(); Iter = Iter.GetOwnerVariant())
				{
					OwnerChain.Add(Iter);
				}
				FString MatchPath = NewClass->GetPathName();
				for (int32 ChainIndex = OwnerChain.Num() - 1; ChainIndex >= 0; --ChainIndex)
				{
					MatchPath += SUBOBJECT_DELIMITER_CHAR;
					MatchPath += OwnerChain[ChainIndex].GetName();
				}
				TFieldPath<FField> Match;
				Match.Generate(*MatchPath);
				if (Match.Get())
				{
					Field = Match.Get();
				}
			}
		}
	}
	return *this;
}

// Singleton boilerplate, simply forwarding to the implementation above:
FBlueprintCompilationManagerImpl* BPCMImpl = nullptr;

void FlushReinstancingQueueImplWrapper()
{
	BPCMImpl->FlushReinstancingQueueImpl();
}

// Recursive function to move CDOs aside to immutable versions of classes
// so that CDOs can be safely GC'd. Recursion is necessary to find REINST_ classes
// that are still parented to a valid SKEL (e.g. from MarkBlueprintAsStructurallyModified)
// and therefore need to be REINST_'d again before the SKEL is mutated... Normally
// these old REINST_ classes are GC'd but, there is no guarantee of that:
void MoveSkelCDOAside(UClass* Class, TMap<UClass*, UClass*>& OutOldToNewMap)
{
	UClass* CopyOfOldClass = FBlueprintCompileReinstancer::MoveCDOToNewClass(Class, OutOldToNewMap, true);
	OutOldToNewMap.Add(Class, CopyOfOldClass);

	// Child types that are associated with a BP will be compiled by the compilation
	// manager, but old REINST_ or TRASH_ types need to be handled explicitly:
	TArray<UClass*> Children;
	GetDerivedClasses(Class, Children);
	for(UClass* Child : Children)
	{
		if(UBlueprint* BP = Cast<UBlueprint>(Child->ClassGeneratedBy))
		{
			if(BP->SkeletonGeneratedClass != Child)
			{
				if(	ensureMsgf ( 
					BP->GeneratedClass != Child, 
					TEXT("Class in skeleton hierarchy is cached as GeneratedClass"))
				)
				{
					MoveSkelCDOAside(Child, OutOldToNewMap);
				}
			}
		}
	}
};

void FBlueprintCompilationManager::Initialize()
{
	if(!BPCMImpl)
	{
		BPCMImpl = new FBlueprintCompilationManagerImpl();
	}
}

void FBlueprintCompilationManager::Shutdown()
{
	delete BPCMImpl;
	BPCMImpl = nullptr;
}

// Forward to impl:
void FBlueprintCompilationManager::FlushCompilationQueue(FUObjectSerializeContext* InLoadContext)
{
	if(BPCMImpl)
	{
		LLM_SCOPE_BYNAME(TEXT("Blueprints"));
		BPCMImpl->FlushCompilationQueueImpl(false, nullptr, nullptr, InLoadContext);

		// We can't support save on compile or keeping old CDOs from GCing when reinstancing is deferred:
		BPCMImpl->CompiledBlueprintsToSave.Empty();
		BPCMImpl->OldCDOs.Empty();
	}
}

void FBlueprintCompilationManager::FlushCompilationQueueAndReinstance()
{
	if(BPCMImpl)
	{
		BPCMImpl->FlushCompilationQueueImpl(false, nullptr, nullptr, nullptr);
		BPCMImpl->FlushReinstancingQueueImpl();

		BPCMImpl->OldCDOs.Empty();
	}
}

void FBlueprintCompilationManager::CompileSynchronously(const FBPCompileRequest& Request)
{
	if(BPCMImpl)
	{
		BPCMImpl->CompileSynchronouslyImpl(Request);
	}
}

void FBlueprintCompilationManager::NotifyBlueprintLoaded(UBlueprint* BPLoaded)
{
	// Blueprints can be loaded before editor modules are on line:
	if(!BPCMImpl)
	{
		FBlueprintCompilationManager::Initialize();
	}

	if(FBlueprintEditorUtils::IsCompileOnLoadDisabled(BPLoaded))
	{
		return;
	}

	check(BPLoaded->GetLinker());
	BPCMImpl->QueueForCompilation(FBPCompileRequest(BPLoaded, EBlueprintCompileOptions::IsRegeneratingOnLoad, nullptr));
}

void FBlueprintCompilationManager::QueueForCompilation(UBlueprint* BPLoaded)
{
	BPCMImpl->QueueForCompilation(FBPCompileRequest(BPLoaded, EBlueprintCompileOptions::None, nullptr));
}

bool FBlueprintCompilationManager::IsGeneratedClassLayoutReady()
{
	if(!BPCMImpl)
	{
		// legacy behavior: always assume generated class layout is good:
		return true;
	}
	return BPCMImpl->IsGeneratedClassLayoutReady();
}

bool FBlueprintCompilationManager::GetDefaultValue(const UClass* ForClass, const FProperty* Property, FString& OutDefaultValueAsString)
{
	if(!BPCMImpl)
	{
		// legacy behavior: can't provide CDO for classes currently being compiled
		return false;
	}

	BPCMImpl->GetDefaultValue(ForClass, Property, OutDefaultValueAsString);
	return true;
}


void FBlueprintCompilationManager::ReparentHierarchies(const TMap<UClass*, UClass*>& OldClassToNewClass)
{
	FBlueprintCompilationManagerImpl::ReparentHierarchies(OldClassToNewClass, EReparentClassOptions::None);
}

void FBlueprintCompilationManager::RegisterCompilerExtension(TSubclassOf<UBlueprint> BlueprintType, UBlueprintCompilerExtension* Extension)
{
	Initialize();
	BPCMImpl->RegisterCompilerExtension(BlueprintType, Extension);
}

#undef LOCTEXT_NAMESPACE

