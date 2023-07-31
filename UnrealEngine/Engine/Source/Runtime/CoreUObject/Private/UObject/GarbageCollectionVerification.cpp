// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnObjGC.cpp: Unreal object garbage collection code.
=============================================================================*/

#include "UObject/GarbageCollectionVerification.h"
#include "UObject/GarbageCollection.h"
#include "HAL/ThreadSafeBool.h"
#include "Misc/TimeGuard.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "UObject/UObjectAllocator.h"
#include "UObject/UObjectBase.h"
#include "UObject/Object.h"
#include "UObject/Class.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "UObject/GCObject.h"
#include "UObject/GCScopeLock.h"
#include "HAL/ExceptionHandling.h"
#include "UObject/UObjectClusters.h"
#include "Async/ParallelFor.h"
#include "UObject/ReferenceChainSearch.h"
#include "UObject/FastReferenceCollector.h"
#include <atomic>

/*-----------------------------------------------------------------------------
   Garbage collection verification code.
-----------------------------------------------------------------------------*/

/**
* If set and VERIFY_DISREGARD_GC_ASSUMPTIONS is true, we verify GC assumptions about "Disregard For GC" objects.
*/
COREUOBJECT_API bool	GShouldVerifyGCAssumptions = !(UE_BUILD_SHIPPING != 0 && WITH_EDITOR != 0);

#if VERIFY_DISREGARD_GC_ASSUMPTIONS

/**
 * Finds only direct references of objects passed to the TFastReferenceCollector and verifies if they meet Disregard for GC assumptions
 */
class FDisregardSetReferenceProcessor : public FSimpleReferenceProcessorBase
{
	FThreadSafeCounter NumErrors;

public:
	FDisregardSetReferenceProcessor()
		: NumErrors(0)
	{
	}
	int32 GetErrorCount() const
	{
		return NumErrors.GetValue();
	}
	FORCEINLINE_DEBUGGABLE void HandleTokenStreamObjectReference(FGCArrayStruct& ObjectsToSerializeStruct, UObject* ReferencingObject, UObject*& Object, const int32 TokenIndex, const EGCTokenType TokenType, bool bAllowReferenceElimination)
	{
		if (Object)
		{
#if ENABLE_GC_OBJECT_CHECKS
			if (
#if DO_POINTER_CHECKS_ON_GC
				!IsPossiblyAllocatedUObjectPointer(Object) ||
#endif
				!Object->IsValidLowLevelFast())
			{
				FString TokenDebugInfo;
				if (UClass *Class = (ReferencingObject ? ReferencingObject->GetClass() : nullptr))
				{
					FTokenInfo TokenInfo = Class->ReferenceTokenStream.GetTokenInfo(TokenIndex);
					TokenDebugInfo = FString::Printf(TEXT("ReferencingObjectClass: %s, Property Name: %s, Offset: %d"),
						*Class->GetFullName(), *TokenInfo.Name.GetPlainNameString(), TokenInfo.Offset);
				}
				else
				{
					// This means this objects is most likely being referenced by AddReferencedObjects
					TokenDebugInfo = TEXT("Native Reference");
				}

				UE_LOG(LogGarbage, Fatal, TEXT("Invalid object while verifying Disregard for GC assumptions: 0x%016llx, ReferencingObject: %s, %s, TokenIndex: %d"),
					(int64)(PTRINT)Object,
					ReferencingObject ? *ReferencingObject->GetFullName() : TEXT("NULL"),
					*TokenDebugInfo, TokenIndex);
				}
#endif // ENABLE_GC_OBJECT_CHECKS

			if (!(Object->IsRooted() ||
					  GUObjectArray.IsDisregardForGC(Object) ||
					  GUObjectArray.ObjectToObjectItem(Object)->GetOwnerIndex() > 0 ||
					  GUObjectArray.ObjectToObjectItem(Object)->HasAnyFlags(EInternalObjectFlags::ClusterRoot)))
			{
				UE_LOG(LogGarbage, Warning, TEXT("Disregard for GC object %s referencing %s which is not part of root set"),
					*ReferencingObject->GetFullName(),
					*Object->GetFullName());
				NumErrors.Increment();
			}
		}
	}
};
typedef TDefaultReferenceCollector<FDisregardSetReferenceProcessor> FDisregardSetReferenceCollector;

void VerifyGCAssumptions()
{	
	int32 MaxNumberOfObjects = GUObjectArray.GetObjectArrayNumPermanent();

	FDisregardSetReferenceProcessor Processor;
	TFastReferenceCollector<
		FDisregardSetReferenceProcessor, 
		FDisregardSetReferenceCollector, 
		FGCArrayPool, 
		EFastReferenceCollectorOptions::AutogenerateTokenStream | EFastReferenceCollectorOptions::ProcessNoOpTokens
	> ReferenceCollector(Processor, FGCArrayPool::Get());

	int32 NumThreads = FMath::Max(1, FTaskGraphInterface::Get().GetNumWorkerThreads());
	int32 NumberOfObjectsPerThread = (MaxNumberOfObjects / NumThreads) + 1;
	FGCArrayStruct* ArrayStructs = new FGCArrayStruct[NumThreads];

	ParallelFor( TEXT("GarbageCollection.PF"),NumThreads,1, [&ReferenceCollector, ArrayStructs, NumberOfObjectsPerThread, NumThreads, MaxNumberOfObjects](int32 ThreadIndex)
	{
		int32 FirstObjectIndex = ThreadIndex * NumberOfObjectsPerThread;
		int32 NumObjects = (ThreadIndex < (NumThreads - 1)) ? NumberOfObjectsPerThread : (MaxNumberOfObjects - (NumThreads - 1)*NumberOfObjectsPerThread);
		FGCArrayStruct& ArrayStruct = ArrayStructs[ThreadIndex];
		ArrayStruct.ObjectsToSerialize.Reserve(NumberOfObjectsPerThread);

		for (int32 ObjectIndex = 0; ObjectIndex < NumObjects && (FirstObjectIndex + ObjectIndex) < MaxNumberOfObjects; ++ObjectIndex)
		{
			FUObjectItem& ObjectItem = GUObjectArray.GetObjectItemArrayUnsafe()[FirstObjectIndex + ObjectIndex];
			if (ObjectItem.Object && ObjectItem.Object != FGCObject::GGCObjectReferencer)
			{
				ArrayStruct.ObjectsToSerialize.Add(static_cast<UObject*>(ObjectItem.Object));
			}
		}
		ReferenceCollector.CollectReferences(ArrayStruct);
	});

	delete[] ArrayStructs;

	UE_CLOG(Processor.GetErrorCount() > 0, LogGarbage, Fatal, TEXT("Encountered %d object(s) breaking Disregard for GC assumptions. Please check log for details."), Processor.GetErrorCount());
}

/**
* Finds only direct references of objects passed to the TFastReferenceCollector and verifies if they meet GC Cluster assumptions
*/
class FClusterVerifyReferenceProcessor : public FSimpleReferenceProcessorBase
{
	FThreadSafeCounter NumErrors;
	UObject* CurrentObject;
	FUObjectCluster* Cluster;
	UObject* ClusterRootObject;

public:
	FClusterVerifyReferenceProcessor()
		: NumErrors(0)
		, CurrentObject(nullptr)
		, Cluster(nullptr)
		, ClusterRootObject(nullptr)
	{
	}
	int32 GetErrorCount() const
	{
		return NumErrors.GetValue();
	}
	void SetCurrentObjectAndCluster(UObject* InRootOrClusterObject)
	{
		check(InRootOrClusterObject);
		CurrentObject = InRootOrClusterObject;
		Cluster = GUObjectClusters.GetObjectCluster(CurrentObject);
		check(Cluster);
		FUObjectItem* RootItem = GUObjectArray.IndexToObject(Cluster->RootIndex);
		check(RootItem && RootItem->Object);
		ClusterRootObject = static_cast<UObject*>(RootItem->Object);
	}
	/**
	* Handles UObject reference from the token stream. Performance is critical here so we're FORCEINLINING this function.
	*
	* @param ObjectsToSerialize An array of remaining objects to serialize (Obj must be added to it if Obj can be added to cluster)
	* @param ReferencingObject Object referencing the object to process.
	* @param TokenIndex Index to the token stream where the reference was found.
	* @param bAllowReferenceElimination True if reference elimination is allowed (ignored when constructing clusters).
	*/
	FORCEINLINE_DEBUGGABLE void HandleTokenStreamObjectReference(FGCArrayStruct& ObjectsToSerializeStruct, UObject* ReferencingObject, UObject*& Object, const int32 TokenIndex, const EGCTokenType TokenType, bool bAllowReferenceElimination)
	{
		if (Object)
		{
			if (ObjectsToSerializeStruct.GetReferencingObject() != CurrentObject)
			{
				SetCurrentObjectAndCluster(ObjectsToSerializeStruct.GetReferencingObject());
			}
			check(CurrentObject);

#if ENABLE_GC_OBJECT_CHECKS
			if (
#if DO_POINTER_CHECKS_ON_GC
				!IsPossiblyAllocatedUObjectPointer(Object) ||
#endif
				!Object->IsValidLowLevelFast())
			{
				FString TokenDebugInfo;
				if (UClass *Class = (ReferencingObject ? ReferencingObject->GetClass() : nullptr))
				{
					FTokenInfo TokenInfo = Class->ReferenceTokenStream.GetTokenInfo(TokenIndex);
					TokenDebugInfo = FString::Printf(TEXT("ReferencingObjectClass: %s, Property Name: %s, Offset: %d"),
						*Class->GetFullName(), *TokenInfo.Name.GetPlainNameString(), TokenInfo.Offset);
				}
				else
				{
					// This means this objects is most likely being referenced by AddReferencedObjects
					TokenDebugInfo = TEXT("Native Reference");
				}

#if UE_GCCLUSTER_VERBOSE_LOGGING
				DumpClusterToLog(*Cluster, true, true);
#endif

				UE_LOG(LogGarbage, Fatal, TEXT("Invalid object while verifying cluster assumptions: 0x%016llx, ReferencingObject: %s, %s, TokenIndex: %d"),
					(int64)(PTRINT)Object,
					ReferencingObject ? *ReferencingObject->GetFullName() : TEXT("NULL"),
					*TokenDebugInfo, TokenIndex);
			}
#endif // ENABLE_GC_OBJECT_CHECKS

			FUObjectItem* ObjectItem = GUObjectArray.ObjectToObjectItem(Object);
			if (ObjectItem->GetOwnerIndex() <= 0)
			{
				// We are allowed to reference other clusters, root set objects and objects from diregard for GC pool
				if (!ObjectItem->HasAnyFlags(EInternalObjectFlags::ClusterRoot | EInternalObjectFlags::RootSet)
					&& !GUObjectArray.IsDisregardForGC(Object) && Object->CanBeInCluster() &&
					!Cluster->MutableObjects.Contains(GUObjectArray.ObjectToIndex(Object))) // This is for objects that had RF_NeedLoad|RF_NeedPostLoad set when creating the cluster
				{
					UE_LOG(LogGarbage, Warning, TEXT("Object %s (0x%016llx) from cluster %s (0x%016llx / 0x%016llx) is referencing 0x%016llx %s which is not part of root set or cluster."),
						*CurrentObject->GetFullName(),
						(int64)(PTRINT)CurrentObject,
						*ClusterRootObject->GetFullName(),
						(int64)(PTRINT)ClusterRootObject,
						(int64)(PTRINT)Cluster,
						(int64)(PTRINT)Object,
						*Object->GetFullName());
					NumErrors.Increment();
#if UE_BUILD_DEBUG
					FReferenceChainSearch RefChainSearch(Object, EReferenceChainSearchMode::Shortest | EReferenceChainSearchMode::PrintResults);
#endif
				}
				else if (ObjectItem->HasAnyFlags(EInternalObjectFlags::ClusterRoot))
				{
					// However, clusters need to be referenced by the current cluster otherwise they can also get GC'd too early.
					const FUObjectItem* ClusterRootObjectItem = GUObjectArray.ObjectToObjectItem(ClusterRootObject);
					const int32 OtherClusterRootIndex = GUObjectArray.ObjectToIndex(Object);
					const FUObjectItem* OtherClusterRootItem = GUObjectArray.IndexToObjectUnsafeForGC(OtherClusterRootIndex);
					check(OtherClusterRootItem && OtherClusterRootItem->Object);
					UObject* OtherClusterRootObject = static_cast<UObject*>(OtherClusterRootItem->Object);
					UE_CLOG(OtherClusterRootIndex != Cluster->RootIndex &&
						!Cluster->ReferencedClusters.Contains(OtherClusterRootIndex) &&
						!Cluster->MutableObjects.Contains(OtherClusterRootIndex), LogGarbage, Warning,
						TEXT("Object %s from source cluster %s (%d) is referencing cluster root object %s (0x%016llx) (%d) which is not referenced by the source cluster."),
						*GetFullNameSafe(ReferencingObject),
						*ClusterRootObject->GetFullName(),
						ClusterRootObjectItem->GetClusterIndex(),
						*Object->GetFullName(),
						(int64)(PTRINT)Object,
						OtherClusterRootItem->GetClusterIndex());
				}
			}
			else if (ObjectItem->GetOwnerIndex() != Cluster->RootIndex)
			{
				// If we're referencing an object from another cluster, make sure the other cluster is actually referenced by this cluster
				const FUObjectItem* ClusterRootObjectItem = GUObjectArray.ObjectToObjectItem(ClusterRootObject);
				const int32 OtherClusterRootIndex = ObjectItem->GetOwnerIndex();
				check(OtherClusterRootIndex > 0);
				const FUObjectItem* OtherClusterRootItem = GUObjectArray.IndexToObjectUnsafeForGC(OtherClusterRootIndex);
				check(OtherClusterRootItem && OtherClusterRootItem->Object);
				UObject* OtherClusterRootObject = static_cast<UObject*>(OtherClusterRootItem->Object);
				UE_CLOG(OtherClusterRootIndex != Cluster->RootIndex &&
					!Cluster->ReferencedClusters.Contains(OtherClusterRootIndex) &&
					!Cluster->MutableObjects.Contains(GUObjectArray.ObjectToIndex(Object)), LogGarbage, Warning,
					TEXT("Object %s from source cluster %s (%d) is referencing object %s (0x%016llx) from cluster %s (%d) which is not referenced by the source cluster."),
					*GetFullNameSafe(ReferencingObject),
					*ClusterRootObject->GetFullName(),
					ClusterRootObjectItem->GetClusterIndex(),
					*Object->GetFullName(),
					(int64)(PTRINT)Object,
					*OtherClusterRootObject->GetFullName(),
					OtherClusterRootItem->GetClusterIndex());
			}
		}
	}
};
typedef TDefaultReferenceCollector<FClusterVerifyReferenceProcessor> FClusterVerifyReferenceCollector;

void VerifyClustersAssumptions()
{
	int32 MaxNumberOfClusters = GUObjectClusters.GetClustersUnsafe().Num();
	int32 NumThreads = FMath::Max(1, FTaskGraphInterface::Get().GetNumWorkerThreads());
	int32 NumberOfClustersPerThread = (MaxNumberOfClusters / NumThreads) + 1;
	FGCArrayStruct* ArrayStructs = new FGCArrayStruct[NumThreads];
	FThreadSafeCounter NumErrors(0);

	ParallelFor( TEXT("GarbageCollection.PF"),NumThreads,1, [&NumErrors, ArrayStructs, NumberOfClustersPerThread, NumThreads, MaxNumberOfClusters](int32 ThreadIndex)
	{
		int32 FirstClusterIndex = ThreadIndex * NumberOfClustersPerThread;
		int32 NumClusters = (ThreadIndex < (NumThreads - 1)) ? NumberOfClustersPerThread : (MaxNumberOfClusters - (NumThreads - 1) * NumberOfClustersPerThread);
		FGCArrayStruct& ArrayStruct = ArrayStructs[ThreadIndex];
		
		FClusterVerifyReferenceProcessor Processor;
		TFastReferenceCollector<
			FClusterVerifyReferenceProcessor, 
			FClusterVerifyReferenceCollector, 
			FGCArrayPool, 
			EFastReferenceCollectorOptions::AutogenerateTokenStream | EFastReferenceCollectorOptions::ProcessNoOpTokens
		> ReferenceCollector(Processor, FGCArrayPool::Get());

		for (int32 ClusterIndex = 0; ClusterIndex < NumClusters && (FirstClusterIndex + ClusterIndex) < MaxNumberOfClusters; ++ClusterIndex)
		{
			FUObjectCluster& Cluster = GUObjectClusters.GetClustersUnsafe()[FirstClusterIndex + ClusterIndex];
			if (Cluster.RootIndex >= 0 && Cluster.Objects.Num())
			{
				ArrayStruct.ObjectsToSerialize.Reset();
				ArrayStruct.ObjectsToSerialize.Reserve(Cluster.Objects.Num() + 1);
				{
					FUObjectItem* RootItem = GUObjectArray.IndexToObject(Cluster.RootIndex);
					check(RootItem);
					check(RootItem->Object);
					ArrayStruct.ObjectsToSerialize.Add(static_cast<UObject*>(RootItem->Object));
				}
				for (int32 ObjectIndex : Cluster.Objects)
				{
					FUObjectItem* ObjectItem = GUObjectArray.IndexToObject(ObjectIndex);
					check(ObjectItem);
					check(ObjectItem->Object);
					ArrayStruct.ObjectsToSerialize.Add(static_cast<UObject*>(ObjectItem->Object));
				}
				ReferenceCollector.CollectReferences(ArrayStruct);
				NumErrors.Add(Processor.GetErrorCount());
			}			
		}		
	});

	delete[] ArrayStructs;

	UE_CLOG(NumErrors.GetValue() > 0, LogGarbage, Fatal, TEXT("Encountered %d object(s) breaking GC Clusters assumptions. Please check log for details."), NumErrors.GetValue());
}

void VerifyObjectFlagMirroring()
{
	int32 MaxNumberOfObjects = GUObjectArray.GetObjectArrayNum();
	int32 NumThreads = FMath::Max(1, FTaskGraphInterface::Get().GetNumWorkerThreads());
	int32 NumberOfObjectsPerThread = (MaxNumberOfObjects / NumThreads) + 1;
	std::atomic<uint32> NumErrors(0);

	ParallelFor( TEXT("GarbageCollection.PF"),NumThreads,1, [&NumErrors, NumberOfObjectsPerThread, NumThreads, MaxNumberOfObjects](int32 ThreadIndex)
	{
		int32 FirstObjectIndex = ThreadIndex * NumberOfObjectsPerThread;
		int32 NumObjects = (ThreadIndex < (NumThreads - 1)) ? NumberOfObjectsPerThread : (MaxNumberOfObjects - (NumThreads - 1) * NumberOfObjectsPerThread);

		for (int32 ObjectIndex = 0; ObjectIndex < NumObjects && (FirstObjectIndex + ObjectIndex) < MaxNumberOfObjects; ++ObjectIndex)
		{
			FUObjectItem& ObjectItem = GUObjectArray.GetObjectItemArrayUnsafe()[FirstObjectIndex + ObjectIndex];
			if (ObjectItem.Object)
			{
				UObjectBaseUtility* Object = (UObjectBaseUtility*)ObjectItem.Object;
				bool bHasObjectFlag = Object->HasAnyFlags(RF_InternalPendingKill);
				PRAGMA_DISABLE_DEPRECATION_WARNINGS
				bool bHasInternalFlag = ObjectItem.HasAnyFlags(EInternalObjectFlags::PendingKill);
				PRAGMA_ENABLE_DEPRECATION_WARNINGS
				if (bHasObjectFlag != bHasInternalFlag)
				{
					UE_LOG(LogGarbage, Warning, TEXT("RF_PendingKill (%d) and EInternalObjectFlags::PendingKill (%d) flag mismatch on %s"),
						(int32)bHasObjectFlag,
						(int32)bHasInternalFlag,
						*Object->GetFullName());

					++NumErrors;
				}

				bHasObjectFlag = Object->HasAnyFlags(RF_InternalGarbage);
				bHasInternalFlag = ObjectItem.HasAnyFlags(EInternalObjectFlags::Garbage);
				if (bHasObjectFlag != bHasInternalFlag)
				{
					UE_LOG(LogGarbage, Warning, TEXT("RF_Garbage (%d) and EInternalObjectFlags::Garbage (%d) flag mismatch on %s"),
						(int32)bHasObjectFlag,
						(int32)bHasInternalFlag,
						*Object->GetFullName());

					++NumErrors;
				}
		}
	}});

	UE_CLOG(NumErrors > 0, LogGarbage, Fatal, TEXT("Encountered %d object(s) breaking Object and Internal flag mirroring assumptions. Please check log for details."), (uint32)NumErrors);
}

#endif // VERIFY_DISREGARD_GC_ASSUMPTIONS
#if PROFILE_GCConditionalBeginDestroy

TMap<FName, FCBDTime> CBDTimings;
TMap<UObject*, FName> CBDNameLookup;

void FScopedCBDProfile::DumpProfile()
{
	CBDTimings.ValueSort(TLess<FCBDTime>());
	int32 NumPrint = 0;
	for (auto& Item : CBDTimings)
	{
		UE_LOG(LogGarbage, Log, TEXT("    %6d cnt %6.2fus per   %6.2fms total  %s"), Item.Value.Items, 1000.0f * 1000.0f * Item.Value.TotalTime / float(Item.Value.Items), 1000.0f * Item.Value.TotalTime, *Item.Key.ToString());
		if (NumPrint++ > 3000000000)
		{
			break;
		}
	}
	CBDTimings.Empty();
	CBDNameLookup.Empty();
}

#endif // PROFILE_GCConditionalBeginDestroy