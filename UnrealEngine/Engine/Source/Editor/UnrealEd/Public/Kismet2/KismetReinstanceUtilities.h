// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Templates/SubclassOf.h"
#include "Components/ActorComponent.h"
#include "UObject/GCObject.h"

class FReinstanceFinalizer;
class UBlueprint;

DECLARE_STATS_GROUP(TEXT("Kismet Reinstancer"), STATGROUP_KismetReinstancer, STATCAT_Advanced);

enum class EBlueprintCompileReinstancerFlags
{
	None = 0x0,

	BytecodeOnly			= 0x1,
	AutoInferSaveOnCompile	= 0x2,
	AvoidCDODuplication		= 0x4,
	UseDeltaSerialization	= 0x8,
};

ENUM_CLASS_FLAGS(EBlueprintCompileReinstancerFlags)

class FReinstanceFinalizer;

struct UNREALED_API FRecreateUberGraphFrameScope
{
private:
	TArray<UObject*> Objects;
	UClass* RecompiledClass;
public:
	FRecreateUberGraphFrameScope(UClass* InClass, bool bRecreate);
	~FRecreateUberGraphFrameScope();
};

struct UNREALED_API FReplaceInstancesOfClassParameters
{
	FReplaceInstancesOfClassParameters(UClass* InOldClass, UClass* InNewClass);

	/** The old class, instances of which will be replaced */
	UClass* OldClass;

	/** The new class, used to create new instances to replace instances of the old class */
	UClass* NewClass;

	/** OriginalCDO, use if OldClass->ClassDefaultObject has been overwritten */
	UObject* OriginalCDO; 

	/** Set of objects that should not have their references updated if they refer to instances that are replaced */
	TSet<UObject*>* ObjectsThatShouldUseOldStuff;

	/** Set of objects for which new objects should not be created, useful if client has created new instances themself */
	const TSet<UObject*>* InstancesThatShouldUseOldClass;

	/** Set to true if class object has been replaced*/
	bool bClassObjectReplaced; 

	/** Defaults to true, indicates whether root component should be preserved */
	bool bPreserveRootComponent; 
};

struct UNREALED_API FBatchReplaceInstancesOfClassParameters
{
	TSet<UObject*>* ObjectsThatShouldUseOldStuff = nullptr;
	
	const TSet<UObject*>* InstancesThatShouldUseOldClass = nullptr;

	bool bArchetypesAreUpToDate = false; 
	
	/** 
	 * Blueprints reuses its UClass* from compile to compile, but it's more 
	 * intuitive to just replace a UClass* with a new instance (e.g. from a 
	 * package reload). This flag tells the reinstancer to replace references
	 * to old classes with references to new classes.
	 */
	bool bReplaceReferencesToOldClasses = false;
};

class UNREALED_API FBlueprintCompileReinstancer : public TSharedFromThis<FBlueprintCompileReinstancer>, public FGCObject
{
protected:
	friend struct FBlueprintCompilationManagerImpl;
	friend struct FReinstancingJob;

	static TSet<TWeakObjectPtr<UBlueprint>> DependentBlueprintsToRefresh;
	static TSet<TWeakObjectPtr<UBlueprint>> CompiledBlueprintsToSave;

	static UClass* HotReloadedOldClass;
	static UClass* HotReloadedNewClass;

	/** Reference to the class we're actively reinstancing */
	UClass* ClassToReinstance;

	/** Reference to the duplicate of ClassToReinstance, which all previous instances are now instances of */
	UClass* DuplicatedClass;

	/** The original CDO object for the class being actively reinstanced */
	UObject*	OriginalCDO;

	/** Children of this blueprint, which will need to be recompiled and relinked temporarily to maintain the class layout */
	TArray<UBlueprint*> Children;

	/** Mappings from old fields before recompilation to their new equivalents */
	TMap<FName, FProperty*> PropertyMap;
	TMap<FName, UFunction*> FunctionMap;

	/** Whether or not this resinstancer has already reinstanced  */
	bool bHasReinstanced;

	/** Cached value, mostly used to determine if we're explicitly targeting the skeleton class or not */
	enum EReinstClassType
	{
		RCT_Unknown,
		RCT_BpSkeleton,
		RCT_BpGenerated,
		RCT_Native,
	};
	EReinstClassType ReinstClassType;

	uint32 ClassToReinstanceDefaultValuesCRC;

	/** Objects that should keep reference to old class */
	TSet<UObject*> ObjectsThatShouldUseOldStuff;

	/** TRUE if this is the root reinstancer that all other active reinstancing is spawned from */
	bool bIsRootReinstancer;

	/** TRUE if this reinstancer should resave compiled Blueprints if the user has requested it */
	bool bAllowResaveAtTheEndIfRequested;

	/** TRUE if delta serialization should be forced during FBlueprintCompileReinstancer::CopyPropertiesForUnrelatedObjects */
	bool bUseDeltaSerializationToCopyProperties;

public:
	// FSerializableObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FBlueprintCompileReinstancer");
	}
	// End of FSerializableObject interface

	static void OptionallyRefreshNodes(UBlueprint* BP);

	void ListDependentBlueprintsToRefresh(const TArray<UBlueprint*>& DependentBPs);
	UE_DEPRECATED(5.0, "This method performs no function and isn't invoked by the base class.  Remove all calls to the base class method.")
	virtual void EnlistDependentBlueprintToRecompile(UBlueprint* BP, bool bBytecodeOnly);
	virtual void BlueprintWasRecompiled(UBlueprint* BP, bool bBytecodeOnly);

	static TSharedPtr<FBlueprintCompileReinstancer> Create(UClass* InClassToReinstance, EBlueprintCompileReinstancerFlags Flags = EBlueprintCompileReinstancerFlags::AutoInferSaveOnCompile)
	{
		return MakeShareable(new FBlueprintCompileReinstancer(InClassToReinstance, Flags));
	}

	virtual ~FBlueprintCompileReinstancer();

	/** Saves a mapping of field names to their UField equivalents, so we can remap any bytecode that references them later */
	void SaveClassFieldMapping(UClass* InClassToReinstance);

	/** Helper to gather mappings from the old class's fields to the new class's version */
	void GenerateFieldMappings(TMap<FFieldVariant, FFieldVariant>& FieldMapping);

	/** Reinstances all objects in the ObjectReinstancingMap */
	void ReinstanceObjects(bool bForceAlwaysReinstance = false);

	/** Updates references to properties and functions of the class that has in the bytecode of dependent blueprints */
	void UpdateBytecodeReferences();

	/** Worker function to replace all instances of OldClass with a new instance of NewClass */
	static void ReplaceInstancesOfClass(UClass* OldClass, UClass* NewClass, UObject* OriginalCDO = nullptr, TSet<UObject*>* ObjectsThatShouldUseOldStuff = nullptr, bool bClassObjectReplaced = false, bool bPreserveRootComponent = true);
	static void ReplaceInstancesOfClassEx(const FReplaceInstancesOfClassParameters& Parameters );

	/** Batch replaces a mapping of one or more classes to their new class by leveraging ReplaceInstancesOfClass */
	static void BatchReplaceInstancesOfClass(TMap<UClass*, UClass*>& InOldToNewClassMap, const FBatchReplaceInstancesOfClassParameters& Options = FBatchReplaceInstancesOfClassParameters());
	
	/** Function used to safely discard a CDO, so that the class can have its layout changed, callers must move parent CDOs aside before moving child CDOs aside: */
	static UClass* MoveCDOToNewClass(UClass* OwnerClass, const TMap<UClass*, UClass*>& OldToNewMap, bool bAvoidCDODuplication);

	/**
	* Moves CDOs aside to immutable versions of classes(`REINST`) so that the CDO's can safely be GC'd.
	* These `REINST` classes will be re-parented to a native parent that we know will not be churning
	* through this function again later, so we avoid O(N^2) processing of REINST classes.
	* Maps each given `SKEL` class to its appropriate `REINST` version of itself
	*/
	static void MoveDependentSkelToReinst(UClass* OwnerClass, TMap<UClass*, UClass*>& OldToNewMap);

	/** Gathers the full class Hierarchy of the ClassToSearch, sorted top down (0 index being UObject, n being the subclasses) */
	static void GetSortedClassHierarchy(UClass* ClassToSearch, TArray<UClass*>& OutHierarchy, UClass** OutNativeParent);

	/** Returns true if the given class is a REINST class (starts with the 'REINST_' prefix) */
	static bool IsReinstClass(const UClass* Class);

	/**
	 * When re-instancing a component, we have to make sure all instance owners' 
	 * construction scripts are re-ran (in-case modifying the component alters 
	 * the construction of the actor).
	 * 
	 * @param  ComponentClass    Identifies the component that was altered (used to find all its instances, and thusly all instance owners).
	 */
	static void ReconstructOwnerInstances(TSubclassOf<UActorComponent> ComponentClass);
	
	/** Verify that all instances of the duplicated class have been replaced and collected */
	void VerifyReplacement();

	virtual bool IsClassObjectReplaced() const { return false; }

	void FinalizeFastReinstancing(TArray<UObject*>& ObjectsToReplace);
protected:

	TSharedPtr<FReinstanceFinalizer> ReinstanceInner(bool bForceAlwaysReinstance);

	TSharedPtr<FReinstanceFinalizer> ReinstanceFast();

	void CompileChildren();

	bool IsReinstancingSkeleton() const { return (ReinstClassType == RCT_BpSkeleton); }

	/** Default constructor, can only be used by derived classes */
	FBlueprintCompileReinstancer()
		: ClassToReinstance(NULL)
		, DuplicatedClass(NULL)
		, OriginalCDO(NULL)
		, bHasReinstanced(false)
		, ReinstClassType(RCT_Unknown)
		, ClassToReinstanceDefaultValuesCRC(0)
		, bIsRootReinstancer(false)
	{}

	/** 
	 * Sets the reinstancer up to work on every object of the specified class
	 *
	 * @param InClassToReinstance		Class being reinstanced
	 * @param bIsBytecodeOnly			TRUE if only the bytecode is being recompiled
	 * @param bSkipGC					TRUE if garbage collection should be skipped
	 * @param bAutoInferSaveOnCompile	TRUE if the reinstancer should infer whether or not save on compile should occur, FALSE if it should never save on compile
	 */
	FBlueprintCompileReinstancer(UClass* InClassToReinstance, EBlueprintCompileReinstancerFlags Flags = EBlueprintCompileReinstancerFlags::AutoInferSaveOnCompile);

	/** Reparents the specified blueprint or class to be the duplicated class in order to allow properties to be copied from the previous CDO to the new one */
	void ReparentChild(UBlueprint* ChildBP);
	void ReparentChild(UClass* ChildClass);

	/** Determine whether reinstancing actors should preserve the root component of the new actor */
	virtual bool ShouldPreserveRootComponentOfReinstancedActor() const { return true; }

	/**
	* Attempts to copy as many properties as possible from the old object to the new. 
	* Use during BP compilation to copy properties from the old CDO to the new one.
	* 
	* @param OldObject						The old object to copy properties from
	* @param NewObject						The new Object to copy properties to
	* @param bClearExternalReferences		If true then attempt to replace references to old classes and instances on this object with the corresponding new ones
	* @param bForceDeltaSerialization		If true the delta serialization will be used when copying
	*/
	static void CopyPropertiesForUnrelatedObjects(UObject* OldObject, UObject* NewObject, bool bClearExternalReferences, bool bForceDeltaSerialization = false);

private:
	/** Handles the work of ReplaceInstancesOfClass, handling both normal replacement of instances and batch */
	static void ReplaceInstancesOfClass_Inner(TMap<UClass*, UClass*>& InOldToNewClassMap, UObject* InOriginalCDO, TSet<UObject*>* ObjectsThatShouldUseOldStuff = NULL, bool bClassObjectReplaced = false, bool bPreserveRootComponent = true, bool bArchetypesAreUpToDate = false, const TSet<UObject*>* InstancesThatShouldUseOldClass = nullptr, bool bReplaceReferencesToOldClasses = false);

	/** Returns true if A is higher up the class hierarchy  */
	static bool ReinstancerOrderingFunction(UClass* A, UClass* B);
};
