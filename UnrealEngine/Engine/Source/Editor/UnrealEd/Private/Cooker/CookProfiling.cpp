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
#include "Misc/OutputDevice.h"
#include "Misc/StringBuilder.h"
#include "Serialization/ArchiveUObject.h"
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

static FHierarchicalTimerInfo RootTimerInfo("Root", 0);
static FHierarchicalTimerInfo* CurrentTimerInfo = &RootTimerInfo;
static TAllocatorFixedSizeFreeList<sizeof(FHierarchicalTimerInfo), 256> TimerInfoAllocator;

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

	UE_LOG(LogCook, Display, TEXT("  %s%s: %.3fs (%u)"), &LeftPad[PadOffset], *TimerName, TimerInfo->Length, TimerInfo->HitCount);

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
	UE_LOG(LogCook, Display, TEXT("Hierarchy Timer Information:"));

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
	double TickCookOnTheSideTimeSec = 0.0;
	double TickCookOnTheSideLoadPackagesTimeSec = 0.0;
	double TickCookOnTheSideResolveRedirectorsTimeSec = 0.0;
	double TickCookOnTheSideSaveCookedPackageTimeSec = 0.0;
	double TickCookOnTheSidePrepareSaveTimeSec = 0.0;
	double BlockOnAssetRegistryTimeSec = 0.0;
	double GameCookModificationDelegateTimeSec = 0.0;

	// Stats tracked through FAutoRegisterCallback
	int32 PeakRequestQueueSize = 0;
	int32 PeakLoadQueueSize = 0;
	int32 PeakSaveQueueSize = 0;
	uint32 NumPreloadedDependencies = 0;
	uint32 NumPackagesIterativelySkipped = 0;
	uint32 NumPackagesSavedForCook = 0;
	FCookStatsManager::FAutoRegisterCallback RegisterCookOnTheFlyServerStats([](FCookStatsManager::AddStatFuncRef AddStat)
		{
			AddStat(TEXT("Package.Load"), FCookStatsManager::CreateKeyValueArray(TEXT("NumPreloadedDependencies"), NumPreloadedDependencies));
			AddStat(TEXT("Package.Save"), FCookStatsManager::CreateKeyValueArray(TEXT("NumPackagesIterativelySkipped"), NumPackagesIterativelySkipped));
			AddStat(TEXT("CookOnTheFlyServer"), FCookStatsManager::CreateKeyValueArray(TEXT("PeakRequestQueueSize"), PeakRequestQueueSize));
			AddStat(TEXT("CookOnTheFlyServer"), FCookStatsManager::CreateKeyValueArray(TEXT("PeakLoadQueueSize"), PeakLoadQueueSize));
			AddStat(TEXT("CookOnTheFlyServer"), FCookStatsManager::CreateKeyValueArray(TEXT("PeakSaveQueueSize"), PeakSaveQueueSize));
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
	void ToString(FStringBuilderBase& Builder, TConstArrayView<UObject*> VertexToObject)
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
			UObject* Object = VertexToObject[VertexArgument];
			FString ReferencerName;
			if (!Object || !FGCObject::GGCObjectReferencer->GetReferencerName(Object, ReferencerName))
			{
				ReferencerName = TEXT("<Unknown>");
			}
			Builder << TEXT("FGCObject ") << ReferencerName;
			break;
		}
		case EObjectReferencerType::Referenced:
		{
			check(VertexArgument != Algo::Graph::InvalidVertex);
			UObject* Object = VertexToObject[VertexArgument];
			if (Object)
			{
				Object->GetPathName(nullptr, Builder);
			}
			else
			{
				Builder << TEXT("<UnknownObject>");
			}
			break;
		}
		default:
			checkNoEntry();
			break;
		}
	}

private:
	Algo::Graph::FVertex VertexArgument = Algo::Graph::InvalidVertex;
	EObjectReferencerType LinkType = EObjectReferencerType::Unknown;
};

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
 * Given the list of AllObjects from e.g. a TObjectIterator, use serialization and other methods from Garbage Collection
 * to find all the dependencies of each Object.
 * Return the dependencies as a normalized graph in the style of GraphConvert.h, with the vertex of each object defined
 * by AllObjects and ObjectToVertex.
 */
void ConstructObjectGraph(TConstArrayView<UObject*> AllObjects,
	const TMap<UObject*, Algo::Graph::FVertex>& ObjectToVertex, TArray64<Algo::Graph::FVertex>& OutGraphBuffer,
	TArray<TConstArrayView<Algo::Graph::FVertex>>& OutGraph)
{
	using namespace Algo::Graph;

	TArray<TArray<FVertex>> LooseEdges;
	int32 NumVertices = AllObjects.Num();
	LooseEdges.SetNum(NumVertices);
	TArray<UObject*> TargetObjects;
	int32 NumEdges = 0;

	for (FVertex SourceVertex = 0; SourceVertex < NumVertices; ++SourceVertex)
	{
		UObject* SourceObject = AllObjects[SourceVertex];
		TargetObjects.Reset();
		{
			FReferenceFinder Collector(TargetObjects);
			if (SourceObject == FGCObject::GGCObjectReferencer)
			{
				UGCObjectReferencer::AddReferencedObjects(FGCObject::GGCObjectReferencer, Collector);
			}
			else
			{
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
			TargetObjects.SetNum(Algo::Unique(TargetObjects), false /* bAllowShrinking */);
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

void DumpObjClassList(TConstArrayView<FWeakObjectPtr> InitialObjects)
{
	using namespace Algo::Graph;

	FOutputDevice& LogAr = *(GLog);

	// Get the list of Objects
	TArray<UObject*> AllObjects;
	for (FThreadSafeObjectIterator Iter; Iter; ++Iter)
	{
		UObject* Object = *Iter;
		if (!Object)
		{
			continue;
		}
		AllObjects.Add(Object);
	}

	// Convert Objects to Algo::Graph::FVertex to reduce graph search memory
	int32 NumVertices = AllObjects.Num();
	TMap<UObject*, FVertex> VertexOfObject;
	for (FVertex Vertex = 0; Vertex < NumVertices; ++Vertex)
	{
		VertexOfObject.Add(AllObjects[Vertex], Vertex);
	}

	// Store for each vertex whether the vertex is new - not in InitialObjects
	TBitArray<> IsNew(true, NumVertices);
	for (const FWeakObjectPtr& InitialObjectWeak : InitialObjects)
	{
		UObject* InitialObject = InitialObjectWeak.Get();
		if (InitialObject)
		{
			FVertex* Vertex = VertexOfObject.Find(InitialObject);
			if (Vertex)
			{
				IsNew[*Vertex] = false;
			}
		}
	}

	// Serialize objects to get dependencies and use them to create the ObjectGraph
	TArray64<FVertex> ObjectGraphBuffer;
	TArray<TConstArrayView<FVertex>> ObjectGraph;
	ConstructObjectGraph(AllObjects, VertexOfObject, ObjectGraphBuffer, ObjectGraph);

	// Mark the objects that are rooted by IsRooted, and find any special vertices
	FVertex GCObjectReferencerVertex = InvalidVertex;
	TArray<FObjectReferencer> AliveReason;
	AliveReason.SetNum(NumVertices);
	for (FVertex Vertex = 0; Vertex < NumVertices; ++Vertex)
	{
		UObject* Object = AllObjects[Vertex];
		if (Object->IsRooted())
		{
			AliveReason[Vertex].Set(EObjectReferencerType::Rooted);
		}
		if (Object == FGCObject::GGCObjectReferencer)
		{
			GCObjectReferencerVertex = Vertex;
		}
	}

	// Mark the objects that are rooted by GCObjectReferencerVertex
	for (FVertex Vertex : ObjectGraph[GCObjectReferencerVertex])
	{
		if (AliveReason[Vertex].GetLinkType() == EObjectReferencerType::Unknown)
		{
			AliveReason[Vertex].Set(EObjectReferencerType::GCObjectRef, Vertex);
		}
	}
	check(GCObjectReferencerVertex != InvalidVertex);

	// Do a DFS to mark the referencer and root of all non-rooted objects
	TArray<FVertex> RootOfVertex;
	RootOfVertex.SetNumUninitialized(NumVertices);
	for (FVertex& Root : RootOfVertex)
	{
		Root = InvalidVertex;
	}

	TArray<FVertex> Stack;
	for (FVertex RootedVertex = 0; RootedVertex < NumVertices; ++RootedVertex)
	{
		if (AliveReason[RootedVertex].GetLinkType() == EObjectReferencerType::Unknown ||
			RootedVertex == GCObjectReferencerVertex)
		{
			continue;
		}

		RootOfVertex[RootedVertex] = RootedVertex;
		Stack.Reset();
		Stack.Add(RootedVertex);
		while (!Stack.IsEmpty())
		{
			FVertex SourceVertex = Stack.Pop(false /* bAllowShrinking */);
			for (FVertex TargetVertex : ObjectGraph[SourceVertex])
			{
				if (AliveReason[TargetVertex].GetLinkType() == EObjectReferencerType::Unknown)
				{
					AliveReason[TargetVertex].Set(EObjectReferencerType::Referenced, SourceVertex);
					RootOfVertex[TargetVertex] = RootedVertex;
					Stack.Add(TargetVertex);
				}
			}
		}
	}

	// Count how many new objects of each class there are, and store all root objects that keep them in memory
	struct FClassInfo
	{
		TMap<FVertex, int32> Roots;
		int32 Count = 0;
		UClass* Class = nullptr;
	};
	TMap<UClass*, FClassInfo> ClassInfos;
	for (FVertex Vertex = 0; Vertex < NumVertices; ++Vertex)
	{
		// Ignore non-new objects
		if (!IsNew[Vertex] || Vertex == GCObjectReferencerVertex)
		{
			continue;
		}
		FObjectReferencer Link = AliveReason[Vertex];
		EObjectReferencerType LinkType = Link.GetLinkType();
		// Ignore objects that have AliveReason unknown. This can occur if the objects were rooted during garbage
		// collection but then asynchronous work RemovedThemFromRoot in between GC finishing and our call to IsRooted.
		if (LinkType == EObjectReferencerType::Unknown)
		{
			continue;
		}
		UClass* Class = AllObjects[Vertex]->GetClass();
		if (!Class || !Class->IsNative())
		{
			continue;
		}
		FClassInfo& ClassInfo = ClassInfos.FindOrAdd(Class);
		ClassInfo.Class = Class;
		ClassInfo.Roots.FindOrAdd(RootOfVertex[Vertex], 0)++;
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


	LogAr.Logf(TEXT("New Objects of each class and the top roots keeping them alive:"));
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
				MaxRoots.Pop(false /* bAllowShrinking */);
			}
		}
		LogAr.Logf(TEXT("\t%6d %s"), ClassInfo.Count, *ClassInfo.Class->GetPathName());
		for (TPair<FVertex, int32>& RootPair : MaxRoots)
		{
			RootObjectString.Reset();
			RootObjectString.Appendf(TEXT("\t\t%6d: "), RootPair.Value);
			AllObjects[RootPair.Key]->GetFullName(RootObjectString);
			FObjectReferencer Link = AliveReason[RootPair.Key];
			RootObjectString << TEXT(" <- ");
			Link.ToString(RootObjectString, AllObjects);
			while (Link.GetLinkType() == EObjectReferencerType::Referenced)
			{
				Link = AliveReason[Link.GetVertexArgument()];
				RootObjectString << TEXT(" <- ");
				Link.ToString(RootObjectString, AllObjects);
			}
			LogAr.Logf(TEXT("%s"), *RootObjectString);
		}
	}
}

} // namespace UE::Cook
