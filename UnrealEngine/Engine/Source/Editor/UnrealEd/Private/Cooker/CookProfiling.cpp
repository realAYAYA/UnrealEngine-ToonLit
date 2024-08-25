// Copyright Epic Games, Inc. All Rights Reserved.

#include "CookProfiling.h"

#include "Algo/GraphConvert.h"
#include "Algo/Sort.h"
#include "Algo/Unique.h"
#include "Containers/Array.h"
#include "Containers/BitArray.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "CookOnTheSide/CookLog.h"
#include "CoreGlobals.h"
#include "DerivedDataBuildRemoteExecutor.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/OutputDevice.h"
#include "Misc/PathViews.h"
#include "Misc/StringBuilder.h"
#include "PackageBuildDependencyTracker.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/ArchiveUObject.h"
#include "Serialization/JsonWriter.h"
#include "Templates/Casts.h"
#include "UObject/GCObject.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectIterator.h"
#include "UObject/WeakObjectPtr.h"

#if OUTPUT_COOKTIMING || ENABLE_COOK_STATS
#include "ProfilingDebugging/ScopedTimers.h"
#endif

#if OUTPUT_COOKTIMING
#include <Containers/AllocatorFixedSizeFreeList.h>
#endif

#if ENABLE_COOK_STATS
#include "AnalyticsET.h"
#include "AnalyticsEventAttribute.h"
#include "IAnalyticsProviderET.h"
#include "StudioAnalytics.h"
#include "DerivedDataCacheInterface.h"
#include "DerivedDataCacheUsageStats.h"
#include "Virtualization/VirtualizationSystem.h"
#include "Experimental/ZenServerInterface.h"
#endif

#if OUTPUT_COOKTIMING
struct FHierarchicalTimerInfo
{
public:
	FHierarchicalTimerInfo(const FHierarchicalTimerInfo& InTimerInfo) = delete;
	FHierarchicalTimerInfo(FHierarchicalTimerInfo&& InTimerInfo) = delete;

	explicit FHierarchicalTimerInfo(const char* InName, uint16 InId)
	:	Id(InId)
	,	Name(InName)
	{
	}

	~FHierarchicalTimerInfo()
	{
		ClearChildren();
	}

	void ClearChildren()
	{
		for (FHierarchicalTimerInfo* Child = FirstChild; Child;)
		{
			FHierarchicalTimerInfo* NextChild = Child->NextSibling;

			DestroyAndFree(Child);

			Child = NextChild;
		}
		FirstChild = nullptr;
	}
	FHierarchicalTimerInfo* GetChild(uint16 InId, const char* InName)
	{
		for (FHierarchicalTimerInfo* Child = FirstChild; Child;)
		{
			if (Child->Id == InId)
				return Child;

			Child = Child->NextSibling;
		}

		FHierarchicalTimerInfo* Child = AllocNew(InName, InId);

		Child->NextSibling	= FirstChild;
		FirstChild			= Child;

		return Child;
	}
	

	uint32							HitCount = 0;
	uint16							Id = 0;
	bool							IncrementDepth = true;
	double							Length = 0;
	const char*						Name;

	FHierarchicalTimerInfo*			FirstChild = nullptr;
	FHierarchicalTimerInfo*			NextSibling = nullptr;

private:
	static FHierarchicalTimerInfo*	AllocNew(const char* InName, uint16 InId);
	static void						DestroyAndFree(FHierarchicalTimerInfo* InPtr);
};

static TAllocatorFixedSizeFreeList<sizeof(FHierarchicalTimerInfo), 256> TimerInfoAllocator;
static FHierarchicalTimerInfo RootTimerInfo("Root", 0);
static FHierarchicalTimerInfo* CurrentTimerInfo = &RootTimerInfo;

FHierarchicalTimerInfo* FHierarchicalTimerInfo::AllocNew(const char* InName, uint16 InId)
{
	return new(TimerInfoAllocator.Allocate()) FHierarchicalTimerInfo(InName, InId);
}

void FHierarchicalTimerInfo::DestroyAndFree(FHierarchicalTimerInfo* InPtr)
{
	InPtr->~FHierarchicalTimerInfo();
	TimerInfoAllocator.Free(InPtr);
}

FScopeTimer::FScopeTimer(int InId, const char* InName, bool IncrementScope /*= false*/ )
{
	checkSlow(IsInGameThread());

	HierarchyTimerInfo = CurrentTimerInfo->GetChild(static_cast<uint16>(InId), InName);

	HierarchyTimerInfo->IncrementDepth = IncrementScope;

	PrevTimerInfo		= CurrentTimerInfo;
	CurrentTimerInfo	= HierarchyTimerInfo;
}

void FScopeTimer::Start()
{
	if (StartTime)
	{
		return;
	}

	StartTime = FPlatformTime::Cycles64();
}

void FScopeTimer::Stop()
{
	if (!StartTime)
	{
		return;
	}

	HierarchyTimerInfo->Length += FPlatformTime::ToSeconds64(FPlatformTime::Cycles64() - StartTime);
	++HierarchyTimerInfo->HitCount;

	StartTime = 0;
}

FScopeTimer::~FScopeTimer()
{
	Stop();

	check(CurrentTimerInfo == HierarchyTimerInfo);
	CurrentTimerInfo = PrevTimerInfo;
}

void OutputHierarchyTimers(const FHierarchicalTimerInfo* TimerInfo, int32 Depth)
{
	FString TimerName(TimerInfo->Name);

	static const TCHAR LeftPad[] = TEXT("                                ");
	const SIZE_T PadOffset = FMath::Max<int>(UE_ARRAY_COUNT(LeftPad) - 1 - Depth * 2, 0);

	UE_LOG(LogCookStats, Display, TEXT("  %s%s: %.3fs (%u)"), &LeftPad[PadOffset], *TimerName, TimerInfo->Length, TimerInfo->HitCount);

	// We need to print in reverse order since the child list begins with the most recently added child

	TArray<const FHierarchicalTimerInfo*> Stack;

	for (const FHierarchicalTimerInfo* Child = TimerInfo->FirstChild; Child; Child = Child->NextSibling)
	{
		Stack.Add(Child);
	}

	const int32 ChildDepth = Depth + TimerInfo->IncrementDepth;

	for (size_t i = Stack.Num(); i > 0; --i)
	{
		OutputHierarchyTimers(Stack[i - 1], ChildDepth);
	}
}

void OutputHierarchyTimers()
{
	UE_LOG(LogCookStats, Display, TEXT("Hierarchy Timer Information:"));

	OutputHierarchyTimers(&RootTimerInfo, 0);
}

void ClearHierarchyTimers()
{
	RootTimerInfo.ClearChildren();
}

#endif

#if ENABLE_COOK_STATS
namespace DetailedCookStats
{
	FString CookProject;
	FString CookCultures;
	FString CookLabel;
	FString TargetPlatforms;
	double CookStartTime = 0.0;
	double CookWallTimeSec = 0.0;
	double StartupWallTimeSec = 0.0;
	double StartCookByTheBookTimeSec = 0.0;
	double TickCookOnTheSideTimeSec = 0.0;
	double TickCookOnTheSideLoadPackagesTimeSec = 0.0;
	double TickCookOnTheSideResolveRedirectorsTimeSec = 0.0;
	double TickCookOnTheSideSaveCookedPackageTimeSec = 0.0;
	double TickCookOnTheSidePrepareSaveTimeSec = 0.0;
	double BlockOnAssetRegistryTimeSec = 0.0;
	double GameCookModificationDelegateTimeSec = 0.0;
	double TickLoopGCTimeSec = 0.0;
	double TickLoopRecompileShaderRequestsTimeSec = 0.0;
	double TickLoopShaderProcessAsyncResultsTimeSec = 0.0;
	double TickLoopProcessDeferredCommandsTimeSec = 0.0;
	double TickLoopTickCommandletStatsTimeSec = 0.0;
	double TickLoopFlushRenderingCommandsTimeSec = 0.0;
	bool IsCookAll = false;
	bool IsCookOnTheFly = false;
	bool IsIterativeCook = false;
	bool IsFastCook = false;
	bool IsUnversioned = false;


	// Stats tracked through FAutoRegisterCallback
	int32 PeakRequestQueueSize = 0;
	int32 PeakLoadQueueSize = 0;
	int32 PeakSaveQueueSize = 0;
	std::atomic<int32> NumDetectedLoads{ 0 };
	int32 NumRequestedLoads = 0;
	uint32 NumPackagesIterativelySkipped = 0;
	FCookStatsManager::FAutoRegisterCallback RegisterCookOnTheFlyServerStats([](FCookStatsManager::AddStatFuncRef AddStat)
		{
			AddStat(TEXT("Package.Load"), FCookStatsManager::CreateKeyValueArray(
				TEXT("NumRequestedLoads"), NumRequestedLoads,
				TEXT("NumPackagesLoaded"), NumDetectedLoads.load(),
				TEXT("NumInlineLoads"), NumDetectedLoads - NumRequestedLoads));
			AddStat(TEXT("Package.Save"), FCookStatsManager::CreateKeyValueArray(TEXT("NumPackagesIterativelySkipped"), NumPackagesIterativelySkipped));
			AddStat(TEXT("CookOnTheFlyServer"), FCookStatsManager::CreateKeyValueArray(
				TEXT("PeakRequestQueueSize"), PeakRequestQueueSize,
				TEXT("PeakLoadQueueSize"), PeakLoadQueueSize,
				TEXT("PeakSaveQueueSize"), PeakSaveQueueSize));
		});
}
#endif

namespace UE::Cook
{

/** The various ways objects can be referenced that keeps them in memory. */
enum class EObjectReferencerType : uint8
{
	Unknown = 0,
	Rooted,
	GCObjectRef,
	Referenced,
};

struct FObjectGraphProfileData;

/**
 * Data for how an object is referenced in the DumpObjClassList graph search,
 * including the type of reference and the vertex of the referencer.
 */
struct FObjectReferencer
{
	FObjectReferencer() = default;
	explicit FObjectReferencer(EObjectReferencerType InLinkType, Algo::Graph::FVertex InVertexArgument = Algo::Graph::InvalidVertex)
	{
		Set(InLinkType, InVertexArgument);
	}

	Algo::Graph::FVertex GetVertexArgument() const
	{
		return VertexArgument;
	}
	EObjectReferencerType GetLinkType()
	{
		return LinkType;
	}
	void Set(EObjectReferencerType InLinkType, Algo::Graph::FVertex InVertexArgument = Algo::Graph::InvalidVertex)
	{
		switch (InLinkType)
		{
		case EObjectReferencerType::GCObjectRef:
			check(InVertexArgument != Algo::Graph::InvalidVertex);
			break;
		case EObjectReferencerType::Referenced:
			check(InVertexArgument != Algo::Graph::InvalidVertex);
			break;
		default:
			break;
		}

		VertexArgument = InVertexArgument;
		LinkType = InLinkType;
	}
	void ToString(FStringBuilderBase& Builder, FObjectGraphProfileData& ProfileData);

private:
	Algo::Graph::FVertex VertexArgument = Algo::Graph::InvalidVertex;
	EObjectReferencerType LinkType = EObjectReferencerType::Unknown;
};

struct FObjectGraphProfileData
{
	/** The list of UObjects found from a TObectIterator */
	TArray<UObject*> AllObjects;
	/** We assign FVertex N <-> AllObjects[N]; this field records the reverse map. */
	TMap<UObject*, Algo::Graph::FVertex> VertexOfObject;
	/** Element N records whether AllObjects[N] is not one of InitialObjects */
	TBitArray<> IsNew;
	/** The first reason found that AllObjects[n] is still referenced. */
	TArray<FObjectReferencer> AliveReason;
	/** The first rooted vertex found that has a reference chain to AllObjects[n]. */
	TArray<Algo::Graph::FVertex> RootOfVertex;
	/** The referencenames reported by FGCObject::GGCObjectReferencer for why it refers to objects. */
	TArray<FString> AllGCObjectNames;
	/** We assign (FVertex NumObjects+N) <-> AllGCObjectNames[N]; this field records the reverse map. */
	TMap<FString, Algo::Graph::FVertex> GCObjectNameToVertex;
	/** Buffer of edges used for ObjectGraph */
	TArray64<Algo::Graph::FVertex> ObjectGraphBuffer;
	/** ObjectGraph constructed from the edges between vertices defined by serialization references between objects. */
	TArray<TConstArrayView<Algo::Graph::FVertex>> ObjectGraph;
	/** Total number of vertices, both Objects and GCObjectNames */
	int32 NumVertices;
	/** Number of object vertices. The first GCObjectName vertex starts after this number. */
	int32 NumObjectVertices;
	/** Number of GCObjectName vertices. */
	int32 NumGCObjectNameVertices;
	/** The vertex that is assigned to FGCObject::GGCObjectReferencer. */
	Algo::Graph::FVertex GCObjectReferencerVertex;

	void AppendVertexName(Algo::Graph::FVertex Vertex, FStringBuilderBase& Builder)
	{
		if (Vertex < 0)
		{
			Builder << TEXT("InvalidVertex");
		}
		else if (Vertex < NumObjectVertices)
		{
			AllObjects[Vertex]->GetPathName(nullptr, Builder);
		}
		else if (Vertex - NumObjectVertices < NumGCObjectNameVertices)
		{
			Builder << TEXT("FGCObject ") << AllGCObjectNames[Vertex - NumObjectVertices];
		}
		else
		{
			Builder << TEXT("InvalidVertex");
		}
	}
};

void FObjectReferencer::ToString(FStringBuilderBase& Builder, FObjectGraphProfileData& ProfileData)
{
	switch (GetLinkType())
	{
	case EObjectReferencerType::Unknown:
		Builder << TEXT("<Unknown>");
		break;
	case EObjectReferencerType::Rooted:
		Builder << TEXT("<Rooted>");
		break;
	case EObjectReferencerType::GCObjectRef:
	{
		check(VertexArgument != Algo::Graph::InvalidVertex);
		check(ProfileData.NumObjectVertices <= VertexArgument && VertexArgument < ProfileData.NumObjectVertices + ProfileData.NumGCObjectNameVertices);
		ProfileData.AppendVertexName(VertexArgument, Builder);
		break;
	}
	case EObjectReferencerType::Referenced:
	{
		check(VertexArgument != Algo::Graph::InvalidVertex);
		check(VertexArgument < ProfileData.NumObjectVertices);
		ProfileData.AppendVertexName(VertexArgument, Builder);
		break;
	}
	default:
		checkNoEntry();
		break;
	}
}
/** An ObjectReferenceCollector to pass to Object->Serialize to collect references into an array. */
class FArchiveGetReferences : public FArchiveUObject
{
public:
	FArchiveGetReferences(UObject* Object, TArray<UObject*>& OutReferencedObjects)
		:ReferencedObjects(OutReferencedObjects)
	{
		ArIsObjectReferenceCollector = true;
		ArIgnoreOuterRef = false;
		SetShouldSkipCompilingAssets(false);
		Object->Serialize(*this);
	}

	FArchive& operator<<(UObject*& Object)
	{
		if (Object)
		{
			ReferencedObjects.Add(Object);
		}

		return *this;
	}
private:
	TArray<UObject*>& ReferencedObjects;
};

/**
 *  A ReferenceFinder used only when serializing FGCObject::GGCObjectReferencer.
 * It captures the referencerName from GGCObjectReferencer for each UObject passed to it.
 */
class FGCObjectReferencerFinder : public FReferenceFinder
{
public:

	FGCObjectReferencerFinder(TArray<UObject*>& InObjectArray, TMap<UObject*, FString>& InObjectReferencerNames)
		: FReferenceFinder(InObjectArray)
		, ObjectReferencerNames(InObjectReferencerNames)
	{
	}

	virtual void HandleObjectReference(UObject*& InObject, const UObject* InReferencingObject, const FProperty* InReferencingProperty) override
	{
		// Avoid duplicate entries.
		if (InObject != NULL)
		{
			// Many places that use FReferenceFinder expect the object to not be const.
			UObject* Object = const_cast<UObject*>(InObject);
			// do not add or recursively serialize objects that have already been added
			bool bAlreadyExists;
			ObjectSet.Add(Object, &bAlreadyExists);
			if (!bAlreadyExists)
			{
				check(Object->IsValidLowLevel());
				ObjectArray.Add(Object);
				FString ReferencerName;
				FGCObject::GGCObjectReferencer->GetReferencerName(Object, ReferencerName, true /* bOnlyIfAddingReferenced */);
				if (!ReferencerName.IsEmpty())
				{
					ObjectReferencerNames.Add(Object, MoveTemp(ReferencerName));
				}
			}
		}
	}

private:
	TMap<UObject*, FString>& ObjectReferencerNames;
	FGCObject* CurrentlySerializingObject;
};

/**
 * Given the list of AllObjects from e.g. a TObjectIterator, use serialization and other methods from Garbage Collection
 * to find all the dependencies of each Object.
 * Return the dependencies as a normalized graph in the style of GraphConvert.h, with the vertex of each object defined
 * by AllObjects and ObjectToVertex.
 */
void ConstructObjectGraph(TConstArrayView<UObject*> AllObjects,
	const TMap<UObject*, Algo::Graph::FVertex>& ObjectToVertex, TArray64<Algo::Graph::FVertex>& OutGraphBuffer,
	TArray<TConstArrayView<Algo::Graph::FVertex>>& OutGraph, TMap<UObject*, FString>& OutGCObjectReferencerNames)
{
	using namespace Algo::Graph;

	TArray<TArray<FVertex>> LooseEdges;
	int32 NumVertices = AllObjects.Num();
	LooseEdges.SetNum(NumVertices);
	TArray<UObject*> TargetObjects;
	int32 NumEdges = 0;
	OutGCObjectReferencerNames.Reset();

	for (FVertex SourceVertex = 0; SourceVertex < NumVertices; ++SourceVertex)
	{
		UObject* SourceObject = AllObjects[SourceVertex];
		TargetObjects.Reset();
		{
			if (SourceObject == FGCObject::GGCObjectReferencer)
			{
				FGCObjectReferencerFinder Collector(TargetObjects, OutGCObjectReferencerNames);
				UGCObjectReferencer::AddReferencedObjects(FGCObject::GGCObjectReferencer, Collector);
			}
			else
			{
				FReferenceFinder Collector(TargetObjects);
				FArchiveGetReferences Ar(SourceObject, TargetObjects);
				if (SourceObject->GetClass())
				{
					SourceObject->GetClass()->CallAddReferencedObjects(SourceObject, Collector);
				}
				// TODO: Handle elements in the token stream not covered by serialize, such as UPackage's
				// Class->EmitObjectReference(STRUCT_OFFSET(UPackage, MetaData), TEXT("MetaData"));
				// In the meantime we handle MetaData explicitly.
				if (UPackage* AsPackage = Cast<UPackage>(SourceObject))
				{
					TargetObjects.Add(AsPackage->GetMetaData());
				}
			}
		}

		if (TargetObjects.Num())
		{
			Algo::Sort(TargetObjects);
			TargetObjects.SetNum(Algo::Unique(TargetObjects), EAllowShrinking::No);
			TArray<FVertex>& TargetVertices = LooseEdges[SourceVertex];
			TargetVertices.Reserve(TargetObjects.Num());
			for (UObject* TargetObject : TargetObjects)
			{
				const FVertex* TargetVertex = ObjectToVertex.Find(TargetObject);
				if (TargetVertex && *TargetVertex != SourceVertex)
				{
					TargetVertices.Add(*TargetVertex);
				}
			}
			NumEdges += TargetVertices.Num();
		}
	}
	OutGraphBuffer.Empty(NumEdges);
	OutGraph.Empty(NumVertices);
	for (FVertex SourceVertex = 0; SourceVertex < NumVertices; ++SourceVertex)
	{
		TArray<FVertex>& InEdges = LooseEdges[SourceVertex];
		TConstArrayView<FVertex>& OutEdges = OutGraph.Emplace_GetRef();
		OutEdges = TConstArrayView<FVertex>(OutGraphBuffer.GetData() + OutGraphBuffer.Num(), InEdges.Num());
		OutGraphBuffer.Append(InEdges);
	}
}

void ConstructObjectGraphProfileData(TConstArrayView<FWeakObjectPtr> InitialObjects, FObjectGraphProfileData& OutProfileData)
{
	using namespace Algo::Graph;
	// Get the list of Objects
	OutProfileData.AllObjects.Reset();
	for (FThreadSafeObjectIterator Iter; Iter; ++Iter)
	{
		UObject* Object = *Iter;
		if (!Object)
		{
			continue;
		}
		OutProfileData.AllObjects.Add(Object);
	}

	// Convert Objects to Algo::Graph::FVertex to reduce graph search memory
	OutProfileData.NumObjectVertices = OutProfileData.AllObjects.Num();
	OutProfileData.NumVertices = OutProfileData.NumObjectVertices;
	OutProfileData.VertexOfObject.Reset();
	for (FVertex Vertex = 0; Vertex < OutProfileData.NumObjectVertices; ++Vertex)
	{
		OutProfileData.VertexOfObject.Add(OutProfileData.AllObjects[Vertex], Vertex);
	}

	// Store for each vertex whether the vertex is new - not in InitialObjects
	OutProfileData.IsNew.Init(true, OutProfileData.NumObjectVertices);
	for (const FWeakObjectPtr& InitialObjectWeak : InitialObjects)
	{
		UObject* InitialObject = InitialObjectWeak.Get();
		if (InitialObject)
		{
			FVertex* Vertex = OutProfileData.VertexOfObject.Find(InitialObject);
			if (Vertex)
			{
				OutProfileData.IsNew[*Vertex] = false;
			}
		}
	}

	// Serialize objects to get dependencies and use them to create the ObjectGraph
	TMap<UObject*, FString> GCObjectReferencerNames;
	ConstructObjectGraph(OutProfileData.AllObjects, OutProfileData.VertexOfObject,
		OutProfileData.ObjectGraphBuffer, OutProfileData.ObjectGraph,
		GCObjectReferencerNames);

	OutProfileData.GCObjectReferencerVertex = InvalidVertex;
	OutProfileData.AliveReason.SetNum(OutProfileData.NumObjectVertices);
	OutProfileData.RootOfVertex.SetNumUninitialized(OutProfileData.NumObjectVertices);
	for (FVertex& Root : OutProfileData.RootOfVertex)
	{
		Root = InvalidVertex;
	}

	// Mark the objects that are rooted by IsRooted, and find the special GCObjectReferencerVertex
	for (FVertex Vertex = 0; Vertex < OutProfileData.NumObjectVertices; ++Vertex)
	{
		UObject* Object = OutProfileData.AllObjects[Vertex];
		if (Object->IsRooted())
		{
			OutProfileData.AliveReason[Vertex].Set(EObjectReferencerType::Rooted);
			OutProfileData.RootOfVertex[Vertex] = Vertex;
		}
		if (Object == FGCObject::GGCObjectReferencer)
		{
			OutProfileData.GCObjectReferencerVertex = Vertex;
		}
	}
	check(OutProfileData.GCObjectReferencerVertex != InvalidVertex);

	// Mark the objects that are rooted by GCObjectReferencerVertex, and construct a synthetic vertex
	// for each of the referencer names reported by GCObjectReferencerVertex.
	OutProfileData.GCObjectNameToVertex.Reset();
	OutProfileData.AllGCObjectNames.Reset();
	FString UnknownReferencer(TEXT("<Unknown>"));
	for (FVertex Vertex : OutProfileData.ObjectGraph[OutProfileData.GCObjectReferencerVertex])
	{
		if (OutProfileData.AliveReason[Vertex].GetLinkType() == EObjectReferencerType::Unknown)
		{
			UObject* Object = OutProfileData.AllObjects[Vertex];
			FString* ReferencerName = &UnknownReferencer;
			if (Object)
			{
				ReferencerName = GCObjectReferencerNames.Find(Object);
				if (!ReferencerName)
				{
					ReferencerName = &UnknownReferencer;
				}
			}
			FVertex& ReferencerVertex = OutProfileData.GCObjectNameToVertex.FindOrAdd(*ReferencerName);
			if (ReferencerVertex == (FVertex)0) // Having value 0 means it was newly added by FindOrAdd
			{
				ReferencerVertex = OutProfileData.NumVertices++;
				OutProfileData.AllGCObjectNames.Add(*ReferencerName);
				check(OutProfileData.NumVertices == OutProfileData.AllObjects.Num() + OutProfileData.AllGCObjectNames.Num());
			}
			OutProfileData.AliveReason[Vertex].Set(EObjectReferencerType::GCObjectRef, ReferencerVertex);
			OutProfileData.RootOfVertex[Vertex] = ReferencerVertex;
		}
	}
	OutProfileData.NumObjectVertices = OutProfileData.AllObjects.Num();
	OutProfileData.NumGCObjectNameVertices = OutProfileData.AllGCObjectNames.Num();

	// Do a DFS to mark the referencer and root of all non-rooted objects
	TArray<FVertex> Stack;
	for (FVertex PotentialRoot = 0; PotentialRoot < OutProfileData.NumObjectVertices; ++PotentialRoot)
	{
		if (PotentialRoot == OutProfileData.GCObjectReferencerVertex ||
			(OutProfileData.AliveReason[PotentialRoot].GetLinkType() != EObjectReferencerType::Rooted &&
				OutProfileData.AliveReason[PotentialRoot].GetLinkType() != EObjectReferencerType::GCObjectRef))
		{
			continue;
		}
		FVertex RootVertex = OutProfileData.RootOfVertex[PotentialRoot];

		Stack.Reset();
		Stack.Add(PotentialRoot);
		while (!Stack.IsEmpty())
		{
			FVertex SourceVertex = Stack.Pop(EAllowShrinking::No);
			for (FVertex TargetVertex : OutProfileData.ObjectGraph[SourceVertex])
			{
				if (OutProfileData.AliveReason[TargetVertex].GetLinkType() == EObjectReferencerType::Unknown)
				{
					OutProfileData.AliveReason[TargetVertex].Set(EObjectReferencerType::Referenced, SourceVertex);
					OutProfileData.RootOfVertex[TargetVertex] = RootVertex;
					Stack.Add(TargetVertex);
				}
			}
		}
	}
}

void DumpObjClassList(TConstArrayView<FWeakObjectPtr> InitialObjects)
{
	using namespace Algo::Graph;

	FOutputDevice& LogAr = *(GLog);
	FObjectGraphProfileData ProfileData;
	ConstructObjectGraphProfileData(InitialObjects, ProfileData);

	// Count how many new objects of each class there are, and store all root objects that keep them in memory
	struct FClassInfo
	{
		TMap<FVertex, int32> Roots;
		int32 Count = 0;
		UClass* Class = nullptr;
	};
	TMap<UClass*, FClassInfo> ClassInfos;
	for (FVertex Vertex = 0; Vertex < ProfileData.NumObjectVertices; ++Vertex)
	{
		// Ignore non-new objects
		if (!ProfileData.IsNew[Vertex] || Vertex == ProfileData.GCObjectReferencerVertex)
		{
			continue;
		}
		FObjectReferencer Link = ProfileData.AliveReason[Vertex];
		EObjectReferencerType LinkType = Link.GetLinkType();
		// Ignore objects that have AliveReason unknown. This can occur if the objects were rooted during garbage
		// collection but then asynchronous work RemovedThemFromRoot in between GC finishing and our call to IsRooted.
		if (LinkType == EObjectReferencerType::Unknown)
		{
			continue;
		}
		UClass* Class = ProfileData.AllObjects[Vertex]->GetClass();
		if (!Class || !Class->IsNative())
		{
			continue;
		}
		FClassInfo& ClassInfo = ClassInfos.FindOrAdd(Class);
		ClassInfo.Class = Class;
		ClassInfo.Roots.FindOrAdd(ProfileData.RootOfVertex[Vertex], 0)++;
		ClassInfo.Count++;
	}

	TArray<FClassInfo> ClassInfoArray;
	ClassInfoArray.Reserve(ClassInfos.Num());
	for (TPair<UClass*, FClassInfo>& Pair : ClassInfos)
	{
		ClassInfoArray.Add(MoveTemp(Pair.Value));
	}
	Algo::Sort(ClassInfoArray, [](const FClassInfo& A, const FClassInfo& B)
		{
			return FTopLevelAssetPath(A.Class).Compare(FTopLevelAssetPath(B.Class)) < 0;
		});


	LogAr.Logf(TEXT("Memory Analysis: New Objects of each class and the top roots keeping them alive:"));
	LogAr.Logf(TEXT("\t%6s %s"), TEXT("Count"), TEXT("ClassPath"));
	LogAr.Logf(TEXT("\t\t%6s %s"), TEXT("Count"), TEXT("RootObjectAndChain"));
	TStringBuilder<1024> RootObjectString;
	constexpr int32 MaxRootCount = 2;
	TArray<TPair<FVertex, int32>, TInlineAllocator<MaxRootCount + 1>> MaxRoots;
	for (FClassInfo& ClassInfo : ClassInfoArray)
	{
		MaxRoots.Reset();
		for (TPair<FVertex, int32>& RootPair : ClassInfo.Roots)
		{
			for (int32 IndexFromMax = 0; IndexFromMax < MaxRootCount; ++IndexFromMax)
			{
				if (MaxRoots.Num() <= IndexFromMax || MaxRoots[IndexFromMax].Value < RootPair.Value)
				{
					MaxRoots.Insert(RootPair, IndexFromMax);
					break;
				}
			}
			if (MaxRoots.Num() > MaxRootCount)
			{
				MaxRoots.Pop(EAllowShrinking::No);
			}
		}
		LogAr.Logf(TEXT("\t%6d %s"), ClassInfo.Count, *ClassInfo.Class->GetPathName());
		for (TPair<FVertex, int32>& RootPair : MaxRoots)
		{
			RootObjectString.Reset();
			RootObjectString.Appendf(TEXT("\t\t%6d: "), RootPair.Value);
			ProfileData.AppendVertexName(RootPair.Key, RootObjectString);
			if (RootPair.Key < ProfileData.NumObjectVertices)
			{
				FObjectReferencer Link = ProfileData.AliveReason[RootPair.Key];
				while (Link.GetLinkType() == EObjectReferencerType::Referenced)
				{
					RootObjectString << TEXT(" <- ");
					Link.ToString(RootObjectString, ProfileData);
					Link = ProfileData.AliveReason[Link.GetVertexArgument()];
				}
				RootObjectString << TEXT(" <- ");
				Link.ToString(RootObjectString, ProfileData);
			}
			LogAr.Logf(TEXT("%s"), *RootObjectString);
		}
	}
}

void DumpPackageReferencers(TConstArrayView<UPackage*> Packages)
{
	using namespace Algo::Graph;

	FOutputDevice& LogAr = *(GLog);
	FObjectGraphProfileData ProfileData;
	ConstructObjectGraphProfileData(TConstArrayView<FWeakObjectPtr>(), ProfileData);

	// List all roots that cause any of the Packages to remain alive, and count how many packages each one causes
	TMap<FVertex, int32> Roots;
	TMap<FVertex, FVertex> RootExamples;
	int32 Unexpected = 0;
	for (UPackage* Package : Packages)
	{
		FVertex* FoundVertex = ProfileData.VertexOfObject.Find(Package);
		if (!FoundVertex)
		{
			++Unexpected;
			continue;
		}
		FVertex PackageVertex = *FoundVertex;
		FVertex RootOfThisVertex = ProfileData.RootOfVertex[PackageVertex];
		if (RootOfThisVertex == InvalidVertex)
		{
			++Unexpected;
			continue;
		}
		int32& RootCount = Roots.FindOrAdd(RootOfThisVertex);
		if (RootCount == 0)
		{
			RootExamples.Add(RootOfThisVertex, PackageVertex);
		}
		++RootCount;
	}

	LogAr.Logf(TEXT("Memory Analysis: Referencers of SoftGCPackages:"));
	Roots.ValueSort([](int32 A, int32 B) { return A > B; });
	for (TPair<FVertex, int32>& Pair : Roots)
	{
		TStringBuilder<256> ReferencerName;
		ProfileData.AppendVertexName(Pair.Key, ReferencerName);
		LogAr.Logf(TEXT("\t%5d: %s"), Pair.Value, *ReferencerName);
		FVertex* ExampleVertexPtr = RootExamples.Find(Pair.Key);
		if (ExampleVertexPtr)
		{
			TStringBuilder<256> Chain;
			FObjectReferencer Link = ProfileData.AliveReason[*ExampleVertexPtr];
			ProfileData.AllObjects[*ExampleVertexPtr]->GetFullName(Chain);
			while (Link.GetLinkType() == EObjectReferencerType::Referenced)
			{
				Chain << TEXT(" <- ");
				Link.ToString(Chain, ProfileData);
				Link = ProfileData.AliveReason[Link.GetVertexArgument()];
			}
			Chain << TEXT(" <- ");
			Link.ToString(Chain, ProfileData);
			LogAr.Logf(TEXT("\t\t     Ex: %s"), *Chain);
		}
	}

	if (Unexpected > 0)
	{
		LogAr.Logf(TEXT("Memory Analysis: Unknown referenced SoftGCPackages:"));
		for (UPackage* Package : Packages)
		{
			FVertex* FoundVertex = ProfileData.VertexOfObject.Find(Package);
			if (!FoundVertex)
			{
				LogAr.Logf(TEXT("%s: unknown, we did not create a vertex for the package"), *Package->GetName());
				continue;
			}
			FVertex PackageVertex = *FoundVertex;

			if (ProfileData.AliveReason[PackageVertex].GetLinkType() == EObjectReferencerType::Unknown)
			{
				LogAr.Logf(TEXT("%s: no reference found"), *Package->GetName());
				continue;
			}
		}
	}
}

} // namespace UE::Cook

#if ENABLE_COOK_STATS

namespace DetailedCookStats
{

FCookStatsManager::FAutoRegisterCallback RegisterCookStats([](FCookStatsManager::AddStatFuncRef AddStat)
{
	const FString StatName(TEXT("Cook.Profile"));
	#define ADD_COOK_STAT_FLT(Path, Name) AddStat(StatName, FCookStatsManager::CreateKeyValueArray(TEXT("Path"), TEXT(Path), TEXT(#Name), Name))
	ADD_COOK_STAT_FLT(" 0", CookWallTimeSec);
	ADD_COOK_STAT_FLT(" 0. 0", StartupWallTimeSec);
	ADD_COOK_STAT_FLT(" 0. 1", StartCookByTheBookTimeSec);
	ADD_COOK_STAT_FLT(" 0. 1. 0", BlockOnAssetRegistryTimeSec);
	ADD_COOK_STAT_FLT(" 0. 1. 1", GameCookModificationDelegateTimeSec);
	ADD_COOK_STAT_FLT(" 0. 2", TickCookOnTheSideTimeSec);
	ADD_COOK_STAT_FLT(" 0. 2. 0", TickCookOnTheSideLoadPackagesTimeSec);
	ADD_COOK_STAT_FLT(" 0. 2. 1", TickCookOnTheSideSaveCookedPackageTimeSec);
	ADD_COOK_STAT_FLT(" 0. 2. 1. 0", TickCookOnTheSideResolveRedirectorsTimeSec);
	ADD_COOK_STAT_FLT(" 0. 2. 2", TickCookOnTheSidePrepareSaveTimeSec);
	ADD_COOK_STAT_FLT(" 0. 3", TickLoopGCTimeSec);
	ADD_COOK_STAT_FLT(" 0. 4", TickLoopRecompileShaderRequestsTimeSec);
	ADD_COOK_STAT_FLT(" 0. 5", TickLoopShaderProcessAsyncResultsTimeSec);
	ADD_COOK_STAT_FLT(" 0. 6", TickLoopProcessDeferredCommandsTimeSec);
	ADD_COOK_STAT_FLT(" 0. 7", TickLoopTickCommandletStatsTimeSec);
	ADD_COOK_STAT_FLT(" 0. 8", TickLoopFlushRenderingCommandsTimeSec);
	FString CookParameters; // Empty value to write a header with name "CookParameters"
	ADD_COOK_STAT_FLT(" 1", CookParameters);
	ADD_COOK_STAT_FLT(" 1. 0", TargetPlatforms);
	ADD_COOK_STAT_FLT(" 1. 1", CookProject);
	ADD_COOK_STAT_FLT(" 1. 2", CookCultures);
	ADD_COOK_STAT_FLT(" 1. 3", IsCookAll);
	ADD_COOK_STAT_FLT(" 1. 4", IsCookOnTheFly);
	ADD_COOK_STAT_FLT(" 1. 5", IsIterativeCook);
	ADD_COOK_STAT_FLT(" 1. 6", IsUnversioned);
	ADD_COOK_STAT_FLT(" 1. 7", CookLabel);
	ADD_COOK_STAT_FLT(" 1. 8", IsFastCook);
		
	#undef ADD_COOK_STAT_FLT
});

void SendLogCookStats(ECookMode::Type CookMode)
{
	if (IsCookingInEditor(CookMode))
	{
		return;
	}

	/** Used to store profile data for custom logging. */
	struct FCookProfileData
	{
	public:
		FCookProfileData(FString InPath, FString InKey, FString InValue) : Path(MoveTemp(InPath)), Key(MoveTemp(InKey)), Value(MoveTemp(InValue)) {}
		FString Path;
		FString Key;
		FString Value;
	};

	// instead of printing the usage stats generically, we capture them so we can log a subset of them in an easy-to-read way.
	TArray<FDerivedDataCacheResourceStat> DDCResourceUsageStats;
	TArray<FCookStatsManager::StringKeyValue> DDCSummaryStats;
	TArray<FCookProfileData> CookProfileData;
	TArray<FString> StatCategories;
	TMap<FString, TArray<FCookStatsManager::StringKeyValue>> StatsInCategories;

	/** this functor will take a collected cooker stat and log it out using some custom formatting based on known stats that are collected.. */
	auto LogStatsFunc = [&DDCResourceUsageStats, &DDCSummaryStats, &CookProfileData, &StatCategories, &StatsInCategories]
	(const FString& StatName, const TArray<FCookStatsManager::StringKeyValue>& StatAttributes)
	{
		// Some stats will use custom formatting to make a visibly pleasing summary.
		bool bStatUsedCustomFormatting = false;

		if (StatName == TEXT("DDC.Usage"))
		{
			// Don't even log this detailed DDC data. It's mostly only consumable by ingestion into pivot tools.
			bStatUsedCustomFormatting = true;
		}
		else if (StatName.EndsWith(TEXT(".Usage"), ESearchCase::IgnoreCase))
		{
			// These are gathered through GatherResourceStats.
			bStatUsedCustomFormatting = true;
		}
		else if (StatName == TEXT("DDC.Summary"))
		{
			DDCSummaryStats.Append(StatAttributes);
			bStatUsedCustomFormatting = true;
		}
		else if (StatName == TEXT("Cook.Profile"))
		{
			if (StatAttributes.Num() >= 2)
			{
				CookProfileData.Emplace(StatAttributes[0].Value, StatAttributes[1].Key, StatAttributes[1].Value);
			}
			bStatUsedCustomFormatting = true;
		}

		// if a stat doesn't use custom formatting, just spit out the raw info.
		if (!bStatUsedCustomFormatting)
		{
			TArray<FCookStatsManager::StringKeyValue>& StatsInCategory = StatsInCategories.FindOrAdd(StatName);
			if (StatsInCategory.Num() == 0)
			{
				StatCategories.Add(StatName);
			}
			StatsInCategory.Append(StatAttributes);
		}
	};

	GetDerivedDataCacheRef().GatherResourceStats(DDCResourceUsageStats);

	FCookStatsManager::LogCookStats(LogStatsFunc);

	UE_LOG(LogCookStats, Display, TEXT("Misc Cook Stats"));
	UE_LOG(LogCookStats, Display, TEXT("==============="));
	for (FString& StatCategory : StatCategories)
	{
		UE_LOG(LogCookStats, Display, TEXT("%s"), *StatCategory);
		TArray<FCookStatsManager::StringKeyValue>& StatsInCategory = StatsInCategories.FindOrAdd(StatCategory);

		// log each key/value pair, with the equal signs lined up.
		for (const FCookStatsManager::StringKeyValue& StatKeyValue : StatsInCategory)
		{
			UE_LOG(LogCookStats, Display, TEXT("    %s=%s"), *StatKeyValue.Key, *StatKeyValue.Value);
		}
	}

	// DDC Usage stats are custom formatted, and the above code just accumulated them into a TSet. Now log it with our special formatting for readability.
	if (CookProfileData.Num() > 0)
	{
		UE_LOG(LogCookStats, Display, TEXT(""));
		UE_LOG(LogCookStats, Display, TEXT("Cook Profile"));
		UE_LOG(LogCookStats, Display, TEXT("============"));
		for (const auto& ProfileEntry : CookProfileData)
		{
			UE_LOG(LogCookStats, Display, TEXT("%s.%s=%s"), *ProfileEntry.Path, *ProfileEntry.Key, *ProfileEntry.Value);
		}

		FString CookStatsFileName;
		if (FParse::Value(FCommandLine::Get(), TEXT("-CookStatsFile="), CookStatsFileName))
		{
			uint32 MultiprocessId = UE::GetMultiprocessId();
			if (MultiprocessId != 0)
			{
				// Suppress the file creation on CookWorkers
				// TODO: Replicate the information back to the CookDirector instead, UE-185774
				CookStatsFileName.Empty();
			}
		}

		if (!CookStatsFileName.IsEmpty())
		{
			FString JsonString;
			TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> JsonWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR> >::Create(&JsonString);
			JsonWriter->WriteObjectStart();
			for (const auto& ProfileEntry : CookProfileData)
			{
				JsonWriter->WriteObjectStart(ProfileEntry.Key);
				JsonWriter->WriteValue(TEXT("Path"), ProfileEntry.Path);
				JsonWriter->WriteValue(TEXT("Value"), ProfileEntry.Value);
				JsonWriter->WriteObjectEnd();
			}
			JsonWriter->WriteObjectEnd();
			JsonWriter->Close();
			TUniquePtr<FArchive> JsonFile(IFileManager::Get().CreateFileWriter(*CookStatsFileName));
			if (!JsonFile)
			{
				UE_LOG(LogCookStats, Warning, TEXT("Could not write to CookStatsFile %s."), *CookStatsFileName);
			}
			else
			{
				JsonFile->Serialize(TCHAR_TO_ANSI(*JsonString), JsonString.Len());
				JsonFile->Close();
			}
		}

	}
	if (DDCSummaryStats.Num() > 0)
	{
		UE_LOG(LogCookStats, Display, TEXT(""));
		UE_LOG(LogCookStats, Display, TEXT("DDC Summary Stats"));
		UE_LOG(LogCookStats, Display, TEXT("================="));
		for (const auto& Attr : DDCSummaryStats)
		{
			UE_LOG(LogCookStats, Display, TEXT("%-16s=%10s"), *Attr.Key, *Attr.Value);
		}
	}

	DumpDerivedDataBuildRemoteExecutorStats();

	if (!DDCResourceUsageStats.IsEmpty())
	{
		Algo::SortBy(DDCResourceUsageStats, [](const FDerivedDataCacheResourceStat& Stat) { return Stat.BuildTimeSec + Stat.LoadTimeSec; }, TGreater());

		UE_LOG(LogCookStats, Display, TEXT(""));
		UE_LOG(LogCookStats, Display, TEXT("DDC Resource Stats"));
		UE_LOG(LogCookStats, Display, TEXT("======================================================================================================="));
		UE_LOG(LogCookStats, Display, TEXT("Asset Type                          Total Time (Sec)  GameThread Time (Sec)  Assets Built  MB Processed"));
		UE_LOG(LogCookStats, Display, TEXT("----------------------------------  ----------------  ---------------------  ------------  ------------"));
		for (const FDerivedDataCacheResourceStat& Stat : DDCResourceUsageStats)
		{
			UE_LOG(LogCookStats, Display, TEXT("%-34s  %16.2f  %21.2f  %12d  %12.2f"),
				*Stat.AssetType, Stat.LoadTimeSec + Stat.BuildTimeSec, Stat.GameThreadTimeSec,
				Stat.BuildCount, Stat.LoadSizeMB + Stat.BuildSizeMB);
		}
	}

	DumpBuildDependencyTrackerStats();

	if (UE::Virtualization::IVirtualizationSystem::IsInitialized())
	{
		UE::Virtualization::IVirtualizationSystem::Get().DumpStats();
	}
}

}
#endif