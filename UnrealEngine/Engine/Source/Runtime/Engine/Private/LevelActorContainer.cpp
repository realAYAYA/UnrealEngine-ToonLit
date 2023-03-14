// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Level.cpp: Level-related functions
=============================================================================*/

#include "Engine/LevelActorContainer.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "UObject/FastReferenceCollector.h"
#include "UObject/UObjectArray.h"
#include "UObject/Package.h"
#include "UObject/UObjectClusters.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LevelActorContainer)

DEFINE_LOG_CATEGORY_STATIC(LogLevelActorContainer, Log, All);

/**
* Handles UObject references found by TFastReferenceCollector
*/
class FActorClusterReferenceProcessor : public FSimpleReferenceProcessorBase
{
	int32 ClusterRootIndex;
	FUObjectCluster& Cluster;
	ULevel* ParentLevel;
	UPackage* ParentLevelPackage;

public:

	FActorClusterReferenceProcessor(int32 InClusterRootIndex, FUObjectCluster& InCluster, ULevel* InParentLevel)
		: ClusterRootIndex(InClusterRootIndex)
		, Cluster(InCluster)
		, ParentLevel(InParentLevel)
	{
		ParentLevelPackage = ParentLevel->GetOutermost();
	}

	FORCENOINLINE bool CanAddToCluster(UObject* Object)
	{
		if (!Object->IsIn(ParentLevelPackage))
		{
			// No external references are allowed in level clusters
			return false;
		}
		else if (!Object->IsIn(ParentLevel))
		{
			// If the object is in the same package but is not in the level we don't want it either.
			return false;
		}
		if (Object->IsA(ULevel::StaticClass()) || Object->IsA(UWorld::StaticClass()))
		{
			// And generally, no levels or worlds
			return false;
		}
		return Object->CanBeInCluster();
	}

	/**
	* Adds an object to cluster (if possible)
	*
	* @param ObjectIndex UObject index in GUObjectArray
	* @param ObjectItem UObject's entry in GUObjectArray
	* @param Obj The object to add to cluster
	* @param ObjectsToSerialize An array of remaining objects to serialize (Obj must be added to it if Obj can be added to cluster)
	* @param bOuterAndClass If true, the Obj's Outer and Class will also be added to the cluster
	*/
	void AddObjectToCluster(int32 ObjectIndex, FUObjectItem* ObjectItem, UObject* Obj, FGCArrayStruct& ObjectsToSerializeStruct, bool bOuterAndClass)
	{
		// If we haven't finished loading, we can't be sure we know all the references
		check(!Obj->HasAnyFlags(RF_NeedLoad));
		check(ObjectItem->GetOwnerIndex() == 0 || ObjectItem->GetOwnerIndex() == ClusterRootIndex || ObjectIndex == ClusterRootIndex);
		check(Obj->CanBeInCluster());
		if (ObjectIndex != ClusterRootIndex && ObjectItem->GetOwnerIndex() == 0 && !GUObjectArray.IsDisregardForGC(Obj) && !Obj->IsRooted())
		{
			ObjectsToSerializeStruct.ObjectsToSerialize.Add(Obj);
			check(!ObjectItem->HasAnyFlags(EInternalObjectFlags::ClusterRoot));
			ObjectItem->SetOwnerIndex(ClusterRootIndex);
			Cluster.Objects.Add(ObjectIndex);

			if (bOuterAndClass)
			{
				UObject* ObjOuter = Obj->GetOuter();
				if (CanAddToCluster(ObjOuter))
				{
					HandleTokenStreamObjectReference(ObjectsToSerializeStruct, Obj, ObjOuter, INDEX_NONE, EGCTokenType::Native, true);
				}
				else
				{
					Cluster.MutableObjects.AddUnique(GUObjectArray.ObjectToIndex(ObjOuter));
				}
				if (!Obj->GetClass()->HasAllClassFlags(CLASS_Native))
				{
					UObject* ObjectClass = Obj->GetClass();
					HandleTokenStreamObjectReference(ObjectsToSerializeStruct, Obj, ObjectClass, INDEX_NONE, EGCTokenType::Native, true);
					UObject* ObjectClassOuter = Obj->GetClass()->GetOuter();
					HandleTokenStreamObjectReference(ObjectsToSerializeStruct, Obj, ObjectClassOuter, INDEX_NONE, EGCTokenType::Native, true);
				}
			}
		}
	}

	/**
	* Handles UObject reference from the token stream. Performance is critical here so we're FORCEINLINING this function.
	*
	* @param ObjectsToSerialize An array of remaining objects to serialize (Obj must be added to it if Obj can be added to cluster)
	* @param ReferencingObject Object referencing the object to process.
	* @param TokenIndex Index to the token stream where the reference was found.
	* @param bAllowReferenceElimination True if reference elimination is allowed (ignored when constructing clusters).
	*/
	FORCEINLINE void HandleTokenStreamObjectReference(FGCArrayStruct& ObjectsToSerializeStruct, UObject* ReferencingObject, UObject*& Object, const int32 TokenIndex, const EGCTokenType TokenType, bool bAllowReferenceElimination)
	{
		if (Object)
		{
			// If we haven't finished loading, we can't be sure we know all the references
			check(!Object->HasAnyFlags(RF_NeedLoad));

			FUObjectItem* ObjectItem = GUObjectArray.ObjectToObjectItem(Object);

			// Add encountered object reference to list of to be serialized objects if it hasn't already been added.
			if (ObjectItem->GetOwnerIndex() != ClusterRootIndex)
			{
				if (ObjectItem->HasAnyFlags(EInternalObjectFlags::ClusterRoot) || ObjectItem->GetOwnerIndex() != 0)
				{
					// Simply reference this cluster and all clusters it's referencing
					const int32 OtherClusterRootIndex = ObjectItem->HasAnyFlags(EInternalObjectFlags::ClusterRoot) ? GUObjectArray.ObjectToIndex(Object) : ObjectItem->GetOwnerIndex();
					FUObjectItem* OtherClusterRootItem = GUObjectArray.IndexToObject(OtherClusterRootIndex);
					FUObjectCluster& OtherCluster = GUObjectClusters[OtherClusterRootItem->GetClusterIndex()];
					Cluster.ReferencedClusters.AddUnique(OtherClusterRootIndex);

					OtherCluster.ReferencedByClusters.AddUnique(ClusterRootIndex);

					for (int32 OtherClusterReferencedCluster : OtherCluster.ReferencedClusters)
					{
						if (OtherClusterReferencedCluster != ClusterRootIndex)
						{
							Cluster.ReferencedClusters.AddUnique(OtherClusterReferencedCluster);
						}
					}
					for (int32 OtherClusterReferencedMutableObjectIndex : OtherCluster.MutableObjects)
					{
						Cluster.MutableObjects.AddUnique(OtherClusterReferencedMutableObjectIndex);
					}
				}
				else if (!GUObjectArray.IsDisregardForGC(Object)) // Objects that are in disregard for GC set can be safely skipped
				{
					check(ObjectItem->GetOwnerIndex() == 0);

					// New object, add it to the cluster.
					if (CanAddToCluster(Object) && !Object->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad) && !Object->IsRooted())
					{
						AddObjectToCluster(GUObjectArray.ObjectToIndex(Object), ObjectItem, Object, ObjectsToSerializeStruct, true);
					}
					else
					{
						UE_CLOG(Object->HasAnyFlags(RF_NeedLoad), LogLevelActorContainer, Log, TEXT("%s is being added to %s's cluster but hasn't finished loading yet"), *ParentLevel->GetFullName(), *Object->GetFullName());
						Cluster.MutableObjects.AddUnique(GUObjectArray.ObjectToIndex(Object));
					}
				}
			}
		}
	}
};

void ULevelActorContainer::CreateCluster()
{

	int32 ContainerInternalIndex = GUObjectArray.ObjectToIndex(this);
	FUObjectItem* RootItem = GUObjectArray.IndexToObject(ContainerInternalIndex);
	if (RootItem->GetOwnerIndex() != 0 || RootItem->HasAnyFlags(EInternalObjectFlags::ClusterRoot))
	{
		return;
	}

	// If we haven't finished loading, we can't be sure we know all the references
	check(!HasAnyFlags(RF_NeedLoad));

	// Create a new cluster, reserve an arbitrary amount of memory for it.
	const int32 ClusterIndex = GUObjectClusters.AllocateCluster(GUObjectArray.ObjectToIndex(this));
	FUObjectCluster& Cluster = GUObjectClusters[ClusterIndex];
	Cluster.Objects.Reserve(64);

	// Collect all objects referenced by cluster root and by all objects it's referencing
	FActorClusterReferenceProcessor Processor(ContainerInternalIndex, Cluster, CastChecked<ULevel>(GetOuter()));
	TFastReferenceCollector<
		FActorClusterReferenceProcessor, 
		TDefaultReferenceCollector<FActorClusterReferenceProcessor>, 
		FGCArrayPool, 
		EFastReferenceCollectorOptions::AutogenerateTokenStream | EFastReferenceCollectorOptions::ProcessNoOpTokens
	> ReferenceCollector(Processor, FGCArrayPool::Get());
	FGCArrayStruct ArrayStruct;
	TArray<UObject*>& ObjectsToProcess = ArrayStruct.ObjectsToSerialize;
	ObjectsToProcess.Add(static_cast<UObject*>(this));
	ReferenceCollector.CollectReferences(ArrayStruct);
#if UE_BUILD_DEBUG
	FGCArrayPool::Get().CheckLeaks();
#endif

	check(RootItem->GetOwnerIndex() == 0);
	RootItem->SetClusterIndex(ClusterIndex);
	RootItem->SetFlags(EInternalObjectFlags::ClusterRoot);

	if (Cluster.Objects.Num() >= GUObjectClusters.GetMinClusterSize())
	{
		// Sort all objects and set up the cluster root
		Cluster.Objects.Sort();
		Cluster.ReferencedClusters.Sort();
		Cluster.MutableObjects.Sort();

		UE_LOG(LogLevelActorContainer, Verbose, TEXT("Created LevelActorCluster (%d) for %s with %d objects, %d referenced clusters and %d mutable objects."),
			ClusterIndex, *GetOuter()->GetPathName(), Cluster.Objects.Num(), Cluster.ReferencedClusters.Num(), Cluster.MutableObjects.Num());

#if UE_GCCLUSTER_VERBOSE_LOGGING
		DumpClusterToLog(Cluster, true, false);
#endif
	}
	else
	{
		for (int32 ClusterObjectIndex : Cluster.Objects)
		{
			FUObjectItem* ClusterObjectItem = GUObjectArray.IndexToObjectUnsafeForGC(ClusterObjectIndex);
			ClusterObjectItem->SetOwnerIndex(0);
		}
		GUObjectClusters.FreeCluster(ClusterIndex);
		check(RootItem->GetOwnerIndex() == 0);
		check(!RootItem->HasAnyFlags(EInternalObjectFlags::ClusterRoot));
	}
}

void ULevelActorContainer::OnClusterMarkedAsPendingKill()
{
	ULevel* Level = CastChecked<ULevel>(GetOuter());
	Level->ActorsForGC.Append(Actors);
	Actors.Reset();

	Super::OnClusterMarkedAsPendingKill();
}

