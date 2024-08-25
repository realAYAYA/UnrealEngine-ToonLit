// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rigs/RigHierarchy.h"
#include "ControlRig.h"

#include "Rigs/RigHierarchyElements.h"
#include "Rigs/RigHierarchyController.h"
#include "ModularRigRuleManager.h"
#include "Units/RigUnitContext.h"
#include "Math/ControlRigMathLibrary.h"
#include "UObject/AnimObjectVersion.h"
#include "ControlRigObjectVersion.h"
#include "ModularRig.h"
#include "Algo/Count.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "HAL/LowLevelMemTracker.h"

LLM_DEFINE_TAG(Animation_ControlRig);

#if WITH_EDITOR
#include "RigVMPythonUtils.h"
#include "ScopedTransaction.h"
#include "HAL/PlatformStackWalk.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/TransactionObjectEvent.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Units/Execution/RigUnit_BeginExecution.h"
#include "Algo/Transform.h"

static FCriticalSection GRigHierarchyStackTraceMutex;
static char GRigHierarchyStackTrace[65536];
static void RigHierarchyCaptureCallStack(FString& OutCallstack, uint32 NumCallsToIgnore)
{
	FScopeLock ScopeLock(&GRigHierarchyStackTraceMutex);
	GRigHierarchyStackTrace[0] = 0;
	FPlatformStackWalk::StackWalkAndDump(GRigHierarchyStackTrace, 65535, 1 + NumCallsToIgnore);
	OutCallstack = ANSI_TO_TCHAR(GRigHierarchyStackTrace);
}

// CVar to record all transform changes 
static TAutoConsoleVariable<int32> CVarControlRigHierarchyTraceAlways(TEXT("ControlRig.Hierarchy.TraceAlways"), 0, TEXT("if nonzero we will record all transform changes."));
static TAutoConsoleVariable<int32> CVarControlRigHierarchyTraceCallstack(TEXT("ControlRig.Hierarchy.TraceCallstack"), 0, TEXT("if nonzero we will record the callstack for any trace entry.\nOnly works if(ControlRig.Hierarchy.TraceEnabled != 0)"));
static TAutoConsoleVariable<int32> CVarControlRigHierarchyTracePrecision(TEXT("ControlRig.Hierarchy.TracePrecision"), 3, TEXT("sets the number digits in a float when tracing hierarchies."));
static TAutoConsoleVariable<int32> CVarControlRigHierarchyTraceOnSpawn(TEXT("ControlRig.Hierarchy.TraceOnSpawn"), 0, TEXT("sets the number of frames to trace when a new hierarchy is spawned"));
TAutoConsoleVariable<bool> CVarControlRigHierarchyEnableRotationOrder(TEXT("ControlRig.Hierarchy.EnableRotationOrder"), true, TEXT("enables the rotation order for controls"));
TAutoConsoleVariable<bool> CVarControlRigHierarchyEnableModules(TEXT("ControlRig.Hierarchy.Modules"), true, TEXT("enables the modular rigging functionality"));
static int32 sRigHierarchyLastTrace = INDEX_NONE;
static TCHAR sRigHierarchyTraceFormat[16];

// A console command to trace a single frame / single execution for a control rig anim node / control rig component
FAutoConsoleCommandWithWorldAndArgs FCmdControlRigHierarchyTraceFrames
(
	TEXT("ControlRig.Hierarchy.Trace"),
	TEXT("Traces changes in a hierarchy for a provided number of executions (defaults to 1).\nYou can use ControlRig.Hierarchy.TraceCallstack to enable callstack tracing as part of this."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& InParams, UWorld* InWorld)
	{
		int32 NumFrames = 1;
		if(InParams.Num() > 0)
		{
			NumFrames = FCString::Atoi(*InParams[0]);
		}
		
		TArray<UObject*> Instances;
		URigHierarchy::StaticClass()->GetDefaultObject()->GetArchetypeInstances(Instances);

		for(UObject* Instance : Instances)
		{
			if (Instance->HasAnyFlags(RF_ClassDefaultObject))
			{
				continue;
			}
			
			// we'll just trace all of them for now
			//if(Instance->GetWorld() == InWorld)
			if(Instance->GetTypedOuter<UControlRig>() != nullptr)
			{
				CastChecked<URigHierarchy>(Instance)->TraceFrames(NumFrames);
			}
		}
	})
);

#endif

////////////////////////////////////////////////////////////////////////////////
// URigHierarchy
////////////////////////////////////////////////////////////////////////////////

#if URIGHIERARCHY_ENSURE_CACHE_VALIDITY
bool URigHierarchy::bEnableValidityCheckbyDefault = true;
#else
bool URigHierarchy::bEnableValidityCheckbyDefault = false;
#endif

URigHierarchy::URigHierarchy()
: TopologyVersion(0)
, MetadataVersion(0)
, MetadataTagVersion(0)
, bEnableDirtyPropagation(true)
, Elements()
, IndexLookup()
, TransformStackIndex(0)
, bTransactingForTransformChange(false)
, bIsInteracting(false)
, LastInteractedKey()
, bSuspendNotifications(false)
, bSuspendMetadataNotifications(false)
, HierarchyController(nullptr)
, bIsControllerAvailable(true)
, ResetPoseHash(INDEX_NONE)
, bIsCopyingHierarchy(false)
#if WITH_EDITOR
, bPropagatingChange(false)
, bForcePropagation(false)
, TraceFramesLeft(0)
, TraceFramesCaptured(0)
#endif
, bEnableCacheValidityCheck(bEnableValidityCheckbyDefault)
, HierarchyForCacheValidation()
, bUsePreferredEulerAngles(true)
, bAllowNameSpaceWhenSanitizingName(false)
, ExecuteContext(nullptr)
#if WITH_EDITOR
, bRecordTransformsAtRuntime(true)
#endif
, ElementKeyRedirector(nullptr)
, ElementBeingDestroyed(nullptr)
{
	Reset();
#if WITH_EDITOR
	TraceFrames(CVarControlRigHierarchyTraceOnSpawn->GetInt());
#endif
}

void URigHierarchy::BeginDestroy()
{
	// reset needs to be in begin destroy since
	// reset touches a UObject member of this hierarchy,
	// which will be GCed by the time this hierarchy reaches destructor
	Reset();
	Super::BeginDestroy();
}

void URigHierarchy::Serialize(FArchive& Ar)
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));
	
	Ar.UsingCustomVersion(FAnimObjectVersion::GUID);
	Ar.UsingCustomVersion(FControlRigObjectVersion::GUID);

	if (Ar.IsSaving() || Ar.IsObjectReferenceCollector() || Ar.IsCountingMemory())
	{
		Save(Ar);
	}
	else if (Ar.IsLoading())
	{
		Load(Ar);
	}
	else
	{
		// remove due to FPIEFixupSerializer hitting this checkNoEntry();
	}
}

void URigHierarchy::AddReferencedObjects(UObject* InpThis, FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(InpThis, Collector);

	URigHierarchy* pThis = static_cast<URigHierarchy*>(InpThis);
	FScopeLock Lock(&pThis->ElementsLock);
	for (FRigBaseElement* Element : pThis->Elements)
	{
		Collector.AddPropertyReferencesWithStructARO(Element->GetElementStruct(), Element, pThis);
	}
}

void URigHierarchy::Save(FArchive& Ar)
{
	FScopeLock Lock(&ElementsLock);
	
	if(Ar.IsTransacting())
	{
		Ar << TransformStackIndex;
		Ar << bTransactingForTransformChange;
		
		if(bTransactingForTransformChange)
		{
			return;
		}

		TArray<FRigElementKey> SelectedKeys = GetSelectedKeys();
		Ar << SelectedKeys;
	}

	// make sure all parts of pose are valid.
	// this ensures cache validity.
	EnsureCacheValidity();

	ComputeAllTransforms();

	int32 ElementCount = Elements.Num();
	Ar << ElementCount;

	for(int32 ElementIndex = 0; ElementIndex < ElementCount; ElementIndex++)
	{
		FRigBaseElement* Element = Elements[ElementIndex];

		// store the key
		FRigElementKey Key = Element->GetKey();
		Ar << Key;

		// allow the element to store more information
		Element->Serialize(Ar, FRigBaseElement::StaticData);
	}

	for(int32 ElementIndex = 0; ElementIndex < ElementCount; ElementIndex++)
	{
		FRigBaseElement* Element = Elements[ElementIndex];
		Element->Serialize(Ar, FRigBaseElement::InterElementData);
	}

	Ar << PreviousNameMap;
	Ar << PreviousParentMap;

	{
		TMap<FRigElementKey, FMetadataStorage> ElementMetadataToSave;
		
		for (const FRigBaseElement* Element: Elements)
		{
			if (ElementMetadata.IsValidIndex(Element->MetadataStorageIndex))
			{
				ElementMetadataToSave.Add(Element->Key, ElementMetadata[Element->MetadataStorageIndex]);
			}
		}
		
		Ar << ElementMetadataToSave;
	}
	
}

void URigHierarchy::Load(FArchive& Ar)
{
	FScopeLock Lock(&ElementsLock);
	
	TArray<FRigElementKey> SelectedKeys;
	if(Ar.IsTransacting())
	{
		bool bOnlySerializedTransformStackIndex = false;
		Ar << TransformStackIndex;
		Ar << bOnlySerializedTransformStackIndex;
		
		if(bOnlySerializedTransformStackIndex)
		{
			return;
		}

		Ar << SelectedKeys;
	}

	// If a controller is found where the outer is this hierarchy, make sure it is configured correctly
	{
		TArray<UObject*> ChildObjects;
		GetObjectsWithOuter(this, ChildObjects, false);
		ChildObjects = ChildObjects.FilterByPredicate([](UObject* Object)
			{ return Object->IsA<URigHierarchyController>();});
		if (!ChildObjects.IsEmpty())
		{
			ensure(ChildObjects.Num() == 1); // there should only be one controller
			bIsControllerAvailable = true;
			HierarchyController = Cast<URigHierarchyController>(ChildObjects[0]);
			HierarchyController->SetHierarchy(this);
		}
	}
	
	Reset();

	int32 ElementCount = 0;
	Ar << ElementCount;

	PoseVersionPerElement.Reset();

	for(int32 ElementIndex = 0; ElementIndex < ElementCount; ElementIndex++)
	{
		FRigElementKey Key;
		Ar << Key;

		FRigBaseElement* Element = MakeElement(Key.Type);
		check(Element);

		Element->SubIndex = Num(Key.Type);
		Element->Index = Elements.Add(Element);
		ElementsPerType[RigElementTypeToFlatIndex(Key.Type)].Add(Element);
		IndexLookup.Add(Key, Element->Index);
		
		Element->Load(Ar, FRigBaseElement::StaticData);
	}

	IncrementTopologyVersion();

	for(int32 ElementIndex = 0; ElementIndex < ElementCount; ElementIndex++)
	{
		FRigBaseElement* Element = Elements[ElementIndex];
		Element->Load(Ar, FRigBaseElement::InterElementData);
	}

	IncrementTopologyVersion();

	for(int32 ElementIndex = 0; ElementIndex < ElementCount; ElementIndex++)
	{
		if(FRigTransformElement* TransformElement = Cast<FRigTransformElement>(Elements[ElementIndex]))
		{
			FRigBaseElementParentArray CurrentParents = GetParents(TransformElement, false);
			for (FRigBaseElement* CurrentParent : CurrentParents)
			{
				if(FRigTransformElement* TransformParent = Cast<FRigTransformElement>(CurrentParent))
				{
					TransformParent->ElementsToDirty.AddUnique(TransformElement);
				}
			}
		}
	}

	if(Ar.IsTransacting())
	{
		for(const FRigElementKey& SelectedKey : SelectedKeys)
		{
			if(FRigBaseElement* Element = Find<FRigBaseElement>(SelectedKey))
			{
				Element->bSelected = true;
				OrderedSelection.Add(SelectedKey);
			}
		}
	}

	if (Ar.CustomVer(FControlRigObjectVersion::GUID) >= FControlRigObjectVersion::RigHierarchyStoringPreviousNames)
	{
		Ar << PreviousNameMap;
		Ar << PreviousParentMap;
	}
	else
	{
		PreviousNameMap.Reset();
		PreviousParentMap.Reset();
	}

	if (Ar.CustomVer(FControlRigObjectVersion::GUID) >= FControlRigObjectVersion::RigHierarchyStoresElementMetadata)
	{
		ElementMetadata.Reset();
		ElementMetadataFreeList.Reset();
		TMap<FRigElementKey, FMetadataStorage> LoadedElementMetadata;
		
		Ar << LoadedElementMetadata;
		for (TPair<FRigElementKey, FMetadataStorage>& Entry: LoadedElementMetadata)
		{
			FRigBaseElement* Element = Find(Entry.Key);
			Element->MetadataStorageIndex = ElementMetadata.Num();
			ElementMetadata.Add(MoveTemp(Entry.Value));
		}
	}
}

void URigHierarchy::PostLoad()
{
	UObject::PostLoad();

	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));

	CleanupInvalidCaches();

	Notify(ERigHierarchyNotification::HierarchyReset, nullptr);
}

#if WITH_EDITORONLY_DATA
void URigHierarchy::DeclareConstructClasses(TArray<FTopLevelAssetPath>& OutConstructClasses, const UClass* SpecificSubclass)
{
	Super::DeclareConstructClasses(OutConstructClasses, SpecificSubclass);
	OutConstructClasses.Add(FTopLevelAssetPath(URigHierarchyController::StaticClass()));
}
#endif

void URigHierarchy::Reset()
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));

	Reset_Impl(true);
}

void URigHierarchy::Reset_Impl(bool bResetElements)
{
	TopologyVersion = 0;
	MetadataVersion = 0;
	bEnableDirtyPropagation = true;

	if(bResetElements)
	{
		FScopeLock Lock(&ElementsLock);
		
		// walk in reverse since certain elements might not have been allocated themselves
		for(int32 ElementIndex = Elements.Num() - 1; ElementIndex >= 0; ElementIndex--)
		{
			DestroyElement(Elements[ElementIndex]);
		}
		Elements.Reset();
		ElementsPerType.Reset();
		const UEnum* ElementTypeEnum = StaticEnum<ERigElementType>();
		for(int32 TypeIndex=0; ;TypeIndex++)
		{
			if (static_cast<ERigElementType>(ElementTypeEnum->GetValueByIndex(TypeIndex)) == ERigElementType::All)
			{
				break;
			}
			ElementsPerType.Add(TArray<FRigBaseElement*>());
		}
		IndexLookup.Reset();

		for (FMetadataStorage& MetadataStorage: ElementMetadata)
		{
			for (TTuple<FName, FRigBaseMetadata*>& Item: MetadataStorage.MetadataMap)
			{
				FRigBaseMetadata::DestroyMetadata(&Item.Value);
			} 
		}
		ElementMetadata.Reset();
		ElementMetadataFreeList.Reset();
	}

	ResetPoseHash = INDEX_NONE;
	ResetPoseIsFilteredOut.Reset();
	ElementsToRetainLocalTransform.Reset();
	DefaultParentPerElement.Reset();
	OrderedSelection.Reset();
	PoseVersionPerElement.Reset();
	ElementDependencyCache.Reset();

	ChildElementOffsetAndCountCache.Reset();
	ChildElementCache.Reset();
	ChildElementCacheTopologyVersion = std::numeric_limits<uint32>::max();


	{
		FGCScopeGuard Guard;
		Notify(ERigHierarchyNotification::HierarchyReset, nullptr);
	}

	if(HierarchyForCacheValidation)
	{
		HierarchyForCacheValidation->Reset();
	}
}

#if WITH_EDITOR

void URigHierarchy::ForEachListeningHierarchy(TFunctionRef<void(const FRigHierarchyListener&)> PerListeningHierarchyFunction)
{
	for(int32 Index = 0; Index < ListeningHierarchies.Num(); Index++)
	{
		PerListeningHierarchyFunction(ListeningHierarchies[Index]);
	}
}

#endif

void URigHierarchy::ResetToDefault()
{
	FScopeLock Lock(&ElementsLock);
	
	if(DefaultHierarchyPtr.IsValid())
	{
		if(URigHierarchy* DefaultHierarchy = DefaultHierarchyPtr.Get())
		{
			CopyHierarchy(DefaultHierarchy);
			return;
		}
	}
	Reset();
}

void URigHierarchy::CopyHierarchy(URigHierarchy* InHierarchy)
{
	check(InHierarchy);

	const TGuardValue<bool> MarkCopyingHierarchy(bIsCopyingHierarchy, true);
	
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));

	FScopeLock Lock(&ElementsLock);
	if(Elements.Num() == 0 && InHierarchy->Elements.Num () == 0)
	{
		return;
	}

	// check if we really need to do a deep copy all over again.
	// for rigs which contain more elements (likely procedural elements)
	// we'll assume we can just remove superfluous elements (from the end of the lists).
	bool bReallocateElements = Elements.Num() < InHierarchy->Elements.Num();
	if(!bReallocateElements)
	{
		const UEnum* ElementTypeEnum = StaticEnum<ERigElementType>();
		for(int32 ElementTypeIndex = 0; ; ElementTypeIndex++)
		{
			if (static_cast<ERigElementType>(ElementTypeEnum->GetValueByIndex(ElementTypeIndex)) == ERigElementType::All)
			{
				break;
			}
			check(ElementsPerType.IsValidIndex(ElementTypeIndex));
			check(InHierarchy->ElementsPerType.IsValidIndex(ElementTypeIndex));
			if(ElementsPerType[ElementTypeIndex].Num() < InHierarchy->ElementsPerType[ElementTypeIndex].Num())
			{
				bReallocateElements = true;
				break;
			}
		}

		// make sure that we have the elements in the right order / type
		if(!bReallocateElements)
		{
			for(int32 Index = 0; Index < InHierarchy->Elements.Num(); Index++)
			{
				if((Elements[Index]->GetKey().Type != InHierarchy->Elements[Index]->GetKey().Type) ||
					(Elements[Index]->SubIndex != InHierarchy->Elements[Index]->SubIndex))
				{
					bReallocateElements = true;
					break;
				}
			}
		}
	}

	{
		TGuardValue<bool> SuspendMetadataNotifications(bSuspendMetadataNotifications, true);
		Reset_Impl(bReallocateElements);

		static const TArray<int32> StructureSizePerType = {
			sizeof(FRigBoneElement),
			sizeof(FRigNullElement),
			sizeof(FRigControlElement),
			sizeof(FRigCurveElement),
			sizeof(FRigRigidBodyElement),
			sizeof(FRigReferenceElement),
			sizeof(FRigConnectorElement),
			sizeof(FRigSocketElement),
		}; 

		if(bReallocateElements)
		{
			// Allocate the elements in batches to improve performance
			TArray<uint8*> NewElementsPerType;
			for(int32 ElementTypeIndex = 0; ElementTypeIndex < InHierarchy->ElementsPerType.Num(); ElementTypeIndex++)
			{
				const ERigElementType ElementType = FlatIndexToRigElementType(ElementTypeIndex);
				int32 StructureSize = 0;

				const int32 Count = InHierarchy->ElementsPerType[ElementTypeIndex].Num();
				if(Count)
				{
					FRigBaseElement* ElementMemory = MakeElement(ElementType, Count, &StructureSize);
					verify(StructureSize == StructureSizePerType[ElementTypeIndex]);
					NewElementsPerType.Add(reinterpret_cast<uint8*>(ElementMemory));
				}
				else
				{
					NewElementsPerType.Add(nullptr);
				}
			
				ElementsPerType[ElementTypeIndex].Reserve(Count);
			}

			Elements.Reserve(InHierarchy->Elements.Num());
			IndexLookup.Reserve(InHierarchy->IndexLookup.Num());

			for(int32 Index = 0; Index < InHierarchy->Num(); Index++)
			{
				const FRigBaseElement* Source = InHierarchy->Get(Index);
				const FRigElementKey& Key = Source->Key;

				const int32 ElementTypeIndex = RigElementTypeToFlatIndex(Key.Type);
		
				const int32 SubIndex = Num(Key.Type);

				const int32 StructureSize = StructureSizePerType[ElementTypeIndex];
				check(NewElementsPerType[ElementTypeIndex] != nullptr);
				FRigBaseElement* Target = reinterpret_cast<FRigBaseElement*>(&NewElementsPerType[ElementTypeIndex][StructureSize * SubIndex]);

				Target->InitializeFrom(Source);
			
				Target->SubIndex = SubIndex;
				Target->Index = Elements.Add(Target);

				ElementsPerType[ElementTypeIndex].Add(Target);
				IndexLookup.Add(Key, Target->Index);
			
				IncrementPoseVersion(Index);

				check(Source->Index == Index);
				check(Target->Index == Index);
			}
		}
		else
		{
			// remove the superfluous elements
			for(int32 ElementIndex = Elements.Num() - 1; ElementIndex >= InHierarchy->Elements.Num(); ElementIndex--)
			{
				DestroyElement(Elements[ElementIndex]);
			}

			// shrink the containers accordingly
			Elements.SetNum(InHierarchy->Elements.Num());
			const UEnum* ElementTypeEnum = StaticEnum<ERigElementType>();
			for(int32 ElementTypeIndex = 0; ; ElementTypeIndex++)
			{
				if ((ERigElementType)ElementTypeEnum->GetValueByIndex(ElementTypeIndex) == ERigElementType::All)
				{
					break;
				}
				ElementsPerType[ElementTypeIndex].SetNum(InHierarchy->ElementsPerType[ElementTypeIndex].Num());
			}

			for(int32 Index = 0; Index < InHierarchy->Num(); Index++)
			{
				const FRigBaseElement* Source = InHierarchy->Get(Index);
				FRigBaseElement* Target = Elements[Index];

				check(Target->Key.Type == Source->Key.Type);
				Target->InitializeFrom(Source);

				IncrementPoseVersion(Index);
			}

			IndexLookup = InHierarchy->IndexLookup;
		}

		// Copy all the element subclass data and all elements' metadata over.
		for(int32 Index = 0; Index < InHierarchy->Num(); Index++)
		{
			const FRigBaseElement* Source = InHierarchy->Get(Index);
			FRigBaseElement* Target = Elements[Index];

			Target->CopyFrom(Source);

			CopyAllMetadataFromElement(Target, Source);
		}

		PreviousNameMap.Append(InHierarchy->PreviousNameMap);

		// Increment the topology version to invalidate our cached children.
		IncrementTopologyVersion();

		// Keep incrementing the metadata version so that the UI can refresh.
		MetadataVersion += InHierarchy->GetMetadataVersion();
		MetadataTagVersion += InHierarchy->GetMetadataTagVersion();
	}

	if (MetadataChangedDelegate.IsBound())
	{
		MetadataChangedDelegate.Broadcast(FRigElementKey(ERigElementType::All), NAME_None);
	}

	EnsureCacheValidity();

	bIsCopyingHierarchy = false;
	Notify(ERigHierarchyNotification::HierarchyCopied, nullptr);
}

uint32 URigHierarchy::GetNameHash() const
{
	FScopeLock Lock(&ElementsLock);
	
	uint32 Hash = GetTypeHash(GetTopologyVersion());
	for (int32 ElementIndex = 0; ElementIndex < Elements.Num(); ElementIndex++)
	{
		const FRigBaseElement* Element = Elements[ElementIndex];
		Hash = HashCombine(Hash, GetTypeHash(Element->GetFName()));
	}
	return Hash;	
}

uint32 URigHierarchy::GetTopologyHash(bool bIncludeTopologyVersion, bool bIncludeTransientControls) const
{
	FScopeLock Lock(&ElementsLock);
	
	uint32 Hash = bIncludeTopologyVersion ? TopologyVersion : 0;

	for (int32 ElementIndex = 0; ElementIndex < Elements.Num(); ElementIndex++)
	{
		const FRigBaseElement* Element = Elements[ElementIndex];
		
		// skip transient controls
		if(!bIncludeTransientControls)
		{
			if(const FRigControlElement* ControlElement = Cast<FRigControlElement>(Element))
			{
				if(ControlElement->Settings.bIsTransientControl)
				{
					continue;
				}
			}
		}
		
		Hash = HashCombine(Hash, GetTypeHash(Element->GetKey()));

		if(const FRigSingleParentElement* SingleParentElement = Cast<FRigSingleParentElement>(Element))
		{
			if(SingleParentElement->ParentElement)
			{
				Hash = HashCombine(Hash, GetTypeHash(SingleParentElement->ParentElement->GetKey()));
			}
		}
		if(const FRigMultiParentElement* MultiParentElement = Cast<FRigMultiParentElement>(Element))
		{
			for(const FRigElementParentConstraint& ParentConstraint : MultiParentElement->ParentConstraints)
			{
				Hash = HashCombine(Hash, GetTypeHash(ParentConstraint.ParentElement->GetKey()));
			}
		}
		if(const FRigBoneElement* BoneElement = Cast<FRigBoneElement>(Element))
		{
			Hash = HashCombine(Hash, GetTypeHash(BoneElement->BoneType));
		}
		if(const FRigControlElement* ControlElement = Cast<FRigControlElement>(Element))
		{
			Hash = HashCombine(Hash, GetTypeHash(ControlElement->Settings));
		}
		if(const FRigConnectorElement* ConnectorElement = Cast<FRigConnectorElement>(Element))
		{
			Hash = HashCombine(Hash, GetTypeHash(ConnectorElement->Settings));
		}
	}

	return Hash;
}

#if WITH_EDITOR
void URigHierarchy::RegisterListeningHierarchy(URigHierarchy* InHierarchy)
{
	if (InHierarchy)
	{
		bool bFoundListener = false;
		for(int32 ListenerIndex = ListeningHierarchies.Num() - 1; ListenerIndex >= 0; ListenerIndex--)
		{
			const URigHierarchy::FRigHierarchyListener& Listener = ListeningHierarchies[ListenerIndex];
			if(Listener.Hierarchy.IsValid())
			{
				if(Listener.Hierarchy.Get() == InHierarchy)
				{
					bFoundListener = true;
					break;
				}
			}
		}

		if(!bFoundListener)
		{
			URigHierarchy::FRigHierarchyListener Listener;
			Listener.Hierarchy = InHierarchy; 
			ListeningHierarchies.Add(Listener);
		}
	}
}

void URigHierarchy::UnregisterListeningHierarchy(URigHierarchy* InHierarchy)
{
	if (InHierarchy)
	{
		for(int32 ListenerIndex = ListeningHierarchies.Num() - 1; ListenerIndex >= 0; ListenerIndex--)
		{
			const URigHierarchy::FRigHierarchyListener& Listener = ListeningHierarchies[ListenerIndex];
			if(Listener.Hierarchy.IsValid())
			{
				if(Listener.Hierarchy.Get() == InHierarchy)
				{
					ListeningHierarchies.RemoveAt(ListenerIndex);
				}
			}
		}
	}
}

void URigHierarchy::ClearListeningHierarchy()
{
	ListeningHierarchies.Reset();
}
#endif

void URigHierarchy::CopyPose(URigHierarchy* InHierarchy, bool bCurrent, bool bInitial, bool bWeights, bool bMatchPoseInGlobalIfNeeded)
{
	check(InHierarchy);
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));

	// if we need to copy the weights but the hierarchies are topologically
	// different we need to reset the topology. this is expensive and should
	// only happen during construction of the hierarchy itself.
	if(bWeights && (GetTopologyVersion() != InHierarchy->GetTopologyVersion()))
	{
		CopyHierarchy(InHierarchy);
	}

	const bool bPerformTopologyCheck = GetTopologyVersion() != InHierarchy->GetTopologyVersion();
	for (int32 ElementIndex = 0; ElementIndex < Elements.Num(); ElementIndex++)
	{
		FRigBaseElement* Element = Elements[ElementIndex];
		if(FRigBaseElement* OtherElement = InHierarchy->Find(Element->GetKey()))
		{
			Element->CopyPose(OtherElement, bCurrent, bInitial, bWeights);
			IncrementPoseVersion(Element->Index);

			// if the topologies don't match and we are supposed to match
			// elements in global space...
			if(bMatchPoseInGlobalIfNeeded && bPerformTopologyCheck)
			{
				FRigMultiParentElement* MultiParentElementA = Cast<FRigMultiParentElement>(Element);
				FRigMultiParentElement* MultiParentElementB = Cast<FRigMultiParentElement>(OtherElement);
				if(MultiParentElementA && MultiParentElementB)
				{
					if(MultiParentElementA->ParentConstraints.Num() != MultiParentElementB->ParentConstraints.Num())
					{
						FRigControlElement* ControlElementA = Cast<FRigControlElement>(Element);
						FRigControlElement* ControlElementB = Cast<FRigControlElement>(OtherElement);
						if(ControlElementA && ControlElementB)
						{
							if(bCurrent)
							{
								ControlElementA->Offset.Set(ERigTransformType::CurrentGlobal, InHierarchy->GetControlOffsetTransform(ControlElementB, ERigTransformType::CurrentGlobal));
								ControlElementA->Offset.MarkDirty(ERigTransformType::CurrentLocal);
								ControlElementA->Pose.MarkDirty(ERigTransformType::CurrentGlobal);
								ControlElementA->Shape.MarkDirty(ERigTransformType::CurrentGlobal);
								IncrementPoseVersion(ControlElementA->Index);
							}
							if(bInitial)
							{
								ControlElementA->Offset.Set(ERigTransformType::InitialGlobal, InHierarchy->GetControlOffsetTransform(ControlElementB, ERigTransformType::InitialGlobal));
								ControlElementA->Offset.MarkDirty(ERigTransformType::InitialLocal);
								ControlElementA->Pose.MarkDirty(ERigTransformType::InitialGlobal);
								ControlElementA->Shape.MarkDirty(ERigTransformType::InitialGlobal);
								IncrementPoseVersion(ControlElementA->Index);
							}
						}
						else
						{
							if(bCurrent)
							{
								MultiParentElementA->Pose.Set(ERigTransformType::CurrentGlobal, InHierarchy->GetTransform(MultiParentElementB, ERigTransformType::CurrentGlobal));
								MultiParentElementA->Pose.MarkDirty(ERigTransformType::CurrentLocal);
								IncrementPoseVersion(MultiParentElementA->Index);
							}
							if(bInitial)
							{
								MultiParentElementA->Pose.Set(ERigTransformType::InitialGlobal, InHierarchy->GetTransform(MultiParentElementB, ERigTransformType::InitialGlobal));
								MultiParentElementA->Pose.MarkDirty(ERigTransformType::InitialLocal);
								IncrementPoseVersion(MultiParentElementA->Index);
							}
						}
					}
				}
			}
		}
	}

	EnsureCacheValidity();
}

void URigHierarchy::UpdateReferences(const FRigVMExecuteContext* InContext)
{
	check(InContext);
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));
	
	for (int32 ElementIndex = 0; ElementIndex < Elements.Num(); ElementIndex++)
	{
		if(FRigReferenceElement* Reference = Cast<FRigReferenceElement>(Elements[ElementIndex]))
		{
			const FTransform InitialWorldTransform = Reference->GetReferenceWorldTransform(InContext, true);
			const FTransform CurrentWorldTransform = Reference->GetReferenceWorldTransform(InContext, false);

			const FTransform InitialGlobalTransform = InitialWorldTransform.GetRelativeTransform(InContext->GetToWorldSpaceTransform());
			const FTransform CurrentGlobalTransform = CurrentWorldTransform.GetRelativeTransform(InContext->GetToWorldSpaceTransform());

			const FTransform InitialParentTransform = GetParentTransform(Reference, ERigTransformType::InitialGlobal); 
			const FTransform CurrentParentTransform = GetParentTransform(Reference, ERigTransformType::CurrentGlobal);

			const FTransform InitialLocalTransform = InitialGlobalTransform.GetRelativeTransform(InitialParentTransform);
			const FTransform CurrentLocalTransform = CurrentGlobalTransform.GetRelativeTransform(CurrentParentTransform);

			SetTransform(Reference, InitialLocalTransform, ERigTransformType::InitialLocal, true, false);
			SetTransform(Reference, CurrentLocalTransform, ERigTransformType::CurrentLocal, true, false);
		}
	}
}

void URigHierarchy::ResetPoseToInitial(ERigElementType InTypeFilter)
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));
	bool bPerformFiltering = InTypeFilter != ERigElementType::All;
	
	FScopeLock Lock(&ElementsLock);
	
	// if we are resetting the pose on some elements, we need to check if
	// any of affected elements has any children that would not be affected
	// by resetting the pose. if all children are affected we can use the
	// fast path.
	if(bPerformFiltering)
	{
		const int32 Hash = HashCombine(GetTopologyVersion(), (int32)InTypeFilter);
		if(Hash != ResetPoseHash)
		{
			ResetPoseIsFilteredOut.Reset();
			ElementsToRetainLocalTransform.Reset();
			ResetPoseHash = Hash;

			// let's look at all elements and mark all parent of unaffected children
			ResetPoseIsFilteredOut.AddZeroed(Elements.Num());

			Traverse([this, InTypeFilter](FRigBaseElement* InElement, bool& bContinue)
			{
				bContinue = true;
				ResetPoseIsFilteredOut[InElement->GetIndex()] = !InElement->IsTypeOf(InTypeFilter);

				// make sure to distribute the filtering options from
				// the parent to the children of the part of the tree
				const FRigBaseElementParentArray Parents = GetParents(InElement);
				for(const FRigBaseElement* Parent : Parents)
				{
					if(!ResetPoseIsFilteredOut[Parent->GetIndex()])
					{
						if(InElement->IsA<FRigNullElement>() || InElement->IsA<FRigControlElement>())
						{
							ElementsToRetainLocalTransform.Add(InElement->GetIndex());
						}
						else
						{
							ResetPoseIsFilteredOut[InElement->GetIndex()] = false;
						}
					}
				}
			});
		}

		// if the per element state is empty
		// it means that the filter doesn't affect 
		if(ResetPoseIsFilteredOut.IsEmpty())
		{
			bPerformFiltering = false;
		}
	}

	if(bPerformFiltering)
	{
		for(const int32 ElementIndex : ElementsToRetainLocalTransform)
		{
			if(FRigTransformElement* TransformElement = Get<FRigTransformElement>(ElementIndex))
			{
				// compute the local value if necessary
				GetTransform(TransformElement, ERigTransformType::CurrentLocal);

				if(FRigControlElement* ControlElement = Cast<FRigControlElement>(TransformElement))
				{
					// compute the local offset if necessary
					GetControlOffsetTransform(ControlElement, ERigTransformType::CurrentLocal);
					GetControlShapeTransform(ControlElement, ERigTransformType::CurrentLocal);
				}

				PropagateDirtyFlags(TransformElement, false, true, true, false);
			}
		}
		
		for(const int32 ElementIndex : ElementsToRetainLocalTransform)
		{
			if(FRigTransformElement* TransformElement = Get<FRigTransformElement>(ElementIndex))
			{
				if(TransformElement->Pose.IsDirty(ERigTransformType::CurrentGlobal))
				{
					continue;
				}
				
				TransformElement->Pose.MarkDirty(ERigTransformType::CurrentGlobal);

				if(FRigControlElement* ControlElement = Cast<FRigControlElement>(TransformElement))
				{
					ControlElement->Offset.MarkDirty(ERigTransformType::CurrentGlobal);
					ControlElement->Shape.MarkDirty(ERigTransformType::CurrentGlobal);
				}

				PropagateDirtyFlags(TransformElement, false, true, false, true);
			}
		}
	}

	for(int32 ElementIndex=0; ElementIndex<Elements.Num(); ElementIndex++)
	{
		if(!ResetPoseIsFilteredOut.IsEmpty() && bPerformFiltering)
		{
			if(ResetPoseIsFilteredOut[ElementIndex])
			{
				continue;
			}
		}

		// reset the weights to the initial values as well
		if(FRigMultiParentElement* MultiParentElement = Cast<FRigMultiParentElement>(Elements[ElementIndex]))
		{
			for(FRigElementParentConstraint& ParentConstraint : MultiParentElement->ParentConstraints)
			{
				ParentConstraint.Weight = ParentConstraint.InitialWeight;
			}
		}

		if(FRigControlElement* ControlElement = Cast<FRigControlElement>(Elements[ElementIndex]))
		{
			ControlElement->Offset.Current = ControlElement->Offset.Initial;
			ControlElement->Shape.Current = ControlElement->Shape.Initial;
			ControlElement->PreferredEulerAngles.Current = ControlElement->PreferredEulerAngles.Initial;
		}

		if(FRigTransformElement* TransformElement = Cast<FRigTransformElement>(Elements[ElementIndex]))
		{
			TransformElement->Pose.Current = TransformElement->Pose.Initial;
		}
	}
	
	EnsureCacheValidity();
}

void URigHierarchy::ResetCurveValues()
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));
	for(int32 ElementIndex=0; ElementIndex<Elements.Num(); ElementIndex++)
	{
		if(FRigCurveElement* CurveElement = Cast<FRigCurveElement>(Elements[ElementIndex]))
		{
			SetCurveValue(CurveElement, 0.f);
		}
	}
}

void URigHierarchy::UnsetCurveValues(bool bSetupUndo)
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));
	for(int32 ElementIndex=0; ElementIndex<Elements.Num(); ElementIndex++)
	{
		if(FRigCurveElement* CurveElement = Cast<FRigCurveElement>(Elements[ElementIndex]))
		{
			UnsetCurveValue(CurveElement, bSetupUndo);
		}
	}
}

int32 URigHierarchy::Num(ERigElementType InElementType) const
{
	return ElementsPerType[RigElementTypeToFlatIndex(InElementType)].Num();
}

bool URigHierarchy::IsProcedural(const FRigElementKey& InKey) const
{
	return IsProcedural(Find(InKey));
}

bool URigHierarchy::IsProcedural(const FRigBaseElement* InElement) const
{
	if(InElement == nullptr)
	{
		return false;
	}
	return InElement->IsProcedural();
}

TArray<FRigSocketState> URigHierarchy::GetSocketStates() const
{
	const TArray<FRigElementKey> Keys = GetSocketKeys(true);
	TArray<FRigSocketState> States;
	States.Reserve(Keys.Num());
	for(const FRigElementKey& Key : Keys)
	{
		const FRigSocketElement* Socket = FindChecked<FRigSocketElement>(Key);
		if(!Socket->IsProcedural())
		{
			States.Add(Socket->GetSocketState(this));
		}
	}
	return States;
}

TArray<FRigElementKey> URigHierarchy::RestoreSocketsFromStates(TArray<FRigSocketState> InStates, bool bSetupUndoRedo)
{
	TArray<FRigElementKey> Keys;
	for(const FRigSocketState& State : InStates)
	{
		FRigElementKey Key(State.Name, ERigElementType::Socket);

		if(FRigSocketElement* Socket = Find<FRigSocketElement>(Key))
		{
			(void)GetController()->SetParent(Key, State.Parent);
			Socket->SetColor(State.Color, this);
			Socket->SetDescription(State.Description, this);
			SetInitialLocalTransform(Key, State.InitialLocalTransform);
			SetLocalTransform(Key, State.InitialLocalTransform);
		}
		else
		{
			Key = GetController()->AddSocket(State.Name, State.Parent, State.InitialLocalTransform, false, State.Color, State.Description, bSetupUndoRedo, false);
		}

		Keys.Add(Key);
	}
	return Keys;
}

TArray<FRigConnectorState> URigHierarchy::GetConnectorStates() const
{
	const TArray<FRigElementKey> Keys = GetConnectorKeys(true);
	TArray<FRigConnectorState> States;
	States.Reserve(Keys.Num());
	for(const FRigElementKey& Key : Keys)
	{
		const FRigConnectorElement* Connector = FindChecked<FRigConnectorElement>(Key);
		if(!Connector->IsProcedural())
		{
			States.Add(Connector->GetConnectorState(this));
		}
	}
	return States;
}

TArray<FRigElementKey> URigHierarchy::RestoreConnectorsFromStates(TArray<FRigConnectorState> InStates, bool bSetupUndoRedo)
{
	TArray<FRigElementKey> Keys;
	for(const FRigConnectorState& State : InStates)
	{
		FRigElementKey Key(State.Name, ERigElementType::Connector);

		if(const FRigConnectorElement* Connector = Find<FRigConnectorElement>(Key))
		{
			SetConnectorSettings(Key, State.Settings, bSetupUndoRedo, false, false);
		}
		else
		{
			Key = GetController()->AddConnector(State.Name, State.Settings, bSetupUndoRedo, false);
		}

		Keys.Add(Key);
	}
	return Keys;
}

TArray<FName> URigHierarchy::GetMetadataNames(FRigElementKey InItem) const
{
	TArray<FName> Names;
	if (const FRigBaseElement* Element = Find(InItem))
	{
		if (Element->MetadataStorageIndex != INDEX_NONE)
		{
			ElementMetadata[Element->MetadataStorageIndex].MetadataMap.GetKeys(Names);
		}
	}
	return Names;
}

ERigMetadataType URigHierarchy::GetMetadataType(FRigElementKey InItem, FName InMetadataName) const
{
	if (const FRigBaseElement* Element = Find(InItem))
	{
		if (Element->MetadataStorageIndex != INDEX_NONE)
		{
			if (const FRigBaseMetadata* const* MetadataPtrPtr = ElementMetadata[Element->MetadataStorageIndex].MetadataMap.Find(InMetadataName))
			{
				return (*MetadataPtrPtr)->GetType();
			}
		}
	}
	
	return ERigMetadataType::Invalid;
}

bool URigHierarchy::RemoveMetadata(FRigElementKey InItem, FName InMetadataName)
{
	return RemoveMetadataForElement(Find(InItem), InMetadataName);
}

bool URigHierarchy::RemoveAllMetadata(FRigElementKey InItem)
{
	return RemoveAllMetadataForElement(Find(InItem));
}

FName URigHierarchy::GetModulePathFName(FRigElementKey InItem) const
{
	if(!InItem.IsValid())
	{
		return NAME_None;
	}
	
	const FName Result = GetNameMetadata(InItem, ModuleMetadataName, NAME_None);
	if(!Result.IsNone())
	{
		return Result;
	}

	// fall back on the name of the item
	const FString NameString = InItem.Name.ToString();
	FString ModulePathFromName;
	if(SplitNameSpace(NameString, &ModulePathFromName, nullptr))
	{
		if(!ModulePathFromName.IsEmpty())
		{
			return *ModulePathFromName;
		}
	}

	return NAME_None;
}

FString URigHierarchy::GetModulePath(FRigElementKey InItem) const
{
	const FName ModulePathName = GetModulePathFName(InItem);
	if(!ModulePathName.IsNone())
	{
		return ModulePathName.ToString();
	}
	return FString();
}

FName URigHierarchy::GetNameSpaceFName(FRigElementKey InItem) const
{
	if(!InItem.IsValid())
	{
		return NAME_None;
	}
	
	const FName Result = GetNameMetadata(InItem, NameSpaceMetadataName, NAME_None);
	if(!Result.IsNone())
	{
		return Result;
	}

	// fall back on the name of the item
	const FString NameString = InItem.Name.ToString();
	FString NameSpaceFromName;
	if(SplitNameSpace(NameString, &NameSpaceFromName, nullptr))
	{
		if(!NameSpaceFromName.IsEmpty())
		{
			return *(NameSpaceFromName + UModularRig::NamespaceSeparator);
		}
	}
	
	return NAME_None;
}

FString URigHierarchy::GetNameSpace(FRigElementKey InItem) const
{
	const FName NameSpaceName = GetNameSpaceFName(InItem);
	if(!NameSpaceName.IsNone())
	{
		return NameSpaceName.ToString();
	}
	return FString();
}

TArray<const FRigBaseElement*> URigHierarchy::GetSelectedElements(ERigElementType InTypeFilter) const
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));
	TArray<const FRigBaseElement*> Selection;

	if(URigHierarchy* HierarchyForSelection = HierarchyForSelectionPtr.Get())
	{
		TArray<FRigElementKey> SelectedKeys = HierarchyForSelection->GetSelectedKeys(InTypeFilter);
		for(const FRigElementKey& SelectedKey : SelectedKeys)
		{
			if(const FRigBaseElement* Element = Find(SelectedKey))
			{
				Selection.Add((FRigBaseElement*)Element);
			}
		}
		return Selection;
	}

	for (const FRigElementKey& SelectedKey : OrderedSelection)
	{
		if(SelectedKey.IsTypeOf(InTypeFilter))
		{
			if(const FRigBaseElement* Element = FindChecked(SelectedKey))
			{
				ensure(Element->IsSelected());
				Selection.Add(Element);
			}
		}
	}
	return Selection;
}

TArray<FRigElementKey> URigHierarchy::GetSelectedKeys(ERigElementType InTypeFilter) const
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));
	
	if(URigHierarchy* HierarchyForSelection = HierarchyForSelectionPtr.Get())
	{
		return HierarchyForSelection->GetSelectedKeys(InTypeFilter);
	}

	TArray<FRigElementKey> Selection;
	for (const FRigElementKey& SelectedKey : OrderedSelection)
	{
		if(SelectedKey.IsTypeOf(InTypeFilter))
		{
			Selection.Add(SelectedKey);
		}
	}
	
	return Selection;
}

FRigName URigHierarchy::JoinNameSpace(const FRigName& InLeft, const FRigName& InRight)
{
	return FRigName(JoinNameSpace(InLeft.ToString(), InRight.ToString()));
}

FString URigHierarchy::JoinNameSpace(const FString& InLeft, const FString& InRight)
{
	if(InLeft.EndsWith(UModularRig::NamespaceSeparator))
	{
		return InLeft + InRight;
	}
	return InLeft + UModularRig::NamespaceSeparator + InRight;
}

TPair<FString, FString> URigHierarchy::SplitNameSpace(const FString& InNameSpacedPath, bool bFromEnd)
{
	TPair<FString, FString> Result;
	(void)SplitNameSpace(InNameSpacedPath, &Result.Key, &Result.Value, bFromEnd);
	return Result;
}

TPair<FRigName, FRigName> URigHierarchy::SplitNameSpace(const FRigName& InNameSpacedPath, bool bFromEnd)
{
	const TPair<FString, FString> Result = SplitNameSpace(InNameSpacedPath.GetName(), bFromEnd);
	return {FRigName(Result.Key), FRigName(Result.Value)};
}

bool URigHierarchy::SplitNameSpace(const FString& InNameSpacedPath, FString* OutNameSpace, FString* OutName, bool bFromEnd)
{
	return InNameSpacedPath.Split(UModularRig::NamespaceSeparator, OutNameSpace, OutName, ESearchCase::CaseSensitive, bFromEnd ? ESearchDir::FromEnd : ESearchDir::FromStart);
}

bool URigHierarchy::SplitNameSpace(const FRigName& InNameSpacedPath, FRigName* OutNameSpace, FRigName* OutName, bool bFromEnd)
{
	FString NameSpace, Name;
	if(SplitNameSpace(InNameSpacedPath.GetName(), &NameSpace, &Name, bFromEnd))
	{
		if(OutNameSpace)
		{
			OutNameSpace->SetName(NameSpace);
		}
		if(OutName)
		{
			OutName->SetName(Name);
		}
		return true;
	}
	return false;
}

void URigHierarchy::SanitizeName(FRigName& InOutName, bool bAllowNameSpaces)
{
	// Sanitize the name
	FString SanitizedNameString = InOutName.GetName();
	bool bChangedSomething = false;
	for (int32 i = 0; i < SanitizedNameString.Len(); ++i)
	{
		TCHAR& C = SanitizedNameString[i];

		const bool bGoodChar = FChar::IsAlpha(C) ||					 // Any letter
			(C == '_') || (C == '-') || (C == '.') || (C == '|') ||	 // _  - .  | anytime
			(FChar::IsDigit(C)) ||									 // 0-9 anytime
			((i > 0) && (C== ' '));									 // Space after the first character to support virtual bones

		if (!bGoodChar)
		{
			if(bAllowNameSpaces && C == ':')
			{
				continue;
			}
			
			C = '_';
			bChangedSomething = true;
		}
	}

	if (SanitizedNameString.Len() > GetMaxNameLength())
	{
		SanitizedNameString.LeftChopInline(SanitizedNameString.Len() - GetMaxNameLength());
		bChangedSomething = true;
	}

	if(bChangedSomething)
	{
		InOutName.SetName(SanitizedNameString);
	}
}

FRigName URigHierarchy::GetSanitizedName(const FRigName& InName, bool bAllowNameSpaces)
{
	FRigName Name = InName;
	SanitizeName(Name, bAllowNameSpaces);
	return Name;
}

bool URigHierarchy::IsNameAvailable(const FRigName& InPotentialNewName, ERigElementType InType, FString* OutErrorMessage) const
{
	// check for fixed keywords
	const FRigElementKey PotentialKey(InPotentialNewName.GetFName(), InType);
	if(PotentialKey == URigHierarchy::GetDefaultParentKey())
	{
		return false;
	}

	if (GetIndex(PotentialKey) != INDEX_NONE)
	{
		if (OutErrorMessage)
		{
			*OutErrorMessage = TEXT("Name already used.");
		}
		return false;
	}

	const FRigName UnsanitizedName = InPotentialNewName;
	if (UnsanitizedName.Len() > GetMaxNameLength())
	{
		if (OutErrorMessage)
		{
			*OutErrorMessage = TEXT("Name too long.");
		}
		return false;
	}

	if (UnsanitizedName.IsNone())
	{
		if (OutErrorMessage)
		{
			*OutErrorMessage = TEXT("None is not a valid name.");
		}
		return false;
	}

	bool bAllowNameSpaces = bAllowNameSpaceWhenSanitizingName;

	// try to find a control rig this belongs to
	const UControlRig* ControlRig = Cast<UControlRig>(GetOuter());
	if(ControlRig == nullptr)
	{
		if(const UBlueprint* Blueprint = GetTypedOuter<UBlueprint>())
		{
			if(const UClass* Class = Blueprint->GeneratedClass)
			{
				ControlRig = Cast<UControlRig>(Class->GetDefaultObject());
			}
		}
	}

	// allow namespaces on default control rigs (non-module and non-modular)
	if(ControlRig)
	{
		if(!ControlRig->IsRigModule() &&
			!ControlRig->GetClass()->IsChildOf(UModularRig::StaticClass()))
		{
			bAllowNameSpaces = true;
		}
	}
	else
	{
		bAllowNameSpaces = true;
	}

	FRigName SanitizedName = UnsanitizedName;
	SanitizeName(SanitizedName, bAllowNameSpaces);

	if (SanitizedName != UnsanitizedName)
	{
		if (OutErrorMessage)
		{
			*OutErrorMessage = TEXT("Name contains invalid characters.");
		}
		return false;
	}

	return true;
}

bool URigHierarchy::IsDisplayNameAvailable(const FRigElementKey& InParentElement,
	const FRigName& InPotentialNewDisplayName, FString* OutErrorMessage) const
{
	if(InParentElement.IsValid())
	{
		const TArray<FRigElementKey> ChildKeys = GetChildren(InParentElement);
		if(ChildKeys.ContainsByPredicate([&InPotentialNewDisplayName, this](const FRigElementKey& InChildKey) -> bool
		{
			if(const FRigBaseElement* BaseElement = Find(InChildKey))
			{
				if(BaseElement->GetDisplayName() == InPotentialNewDisplayName.GetFName())
				{
					return true;
				}
			}
			return false;
		}))
		{
			if (OutErrorMessage)
			{
				*OutErrorMessage = TEXT("Name already used.");
			}
			return false;
		}
	}

	const FRigName UnsanitizedName = InPotentialNewDisplayName;
	if (UnsanitizedName.Len() > GetMaxNameLength())
	{
		if (OutErrorMessage)
		{
			*OutErrorMessage = TEXT("Name too long.");
		}
		return false;
	}

	if (UnsanitizedName.IsNone())
	{
		if (OutErrorMessage)
		{
			*OutErrorMessage = TEXT("None is not a valid name.");
		}
		return false;
	}

	FRigName SanitizedName = UnsanitizedName;
	SanitizeName(SanitizedName, true);

	if (SanitizedName != UnsanitizedName)
	{
		if (OutErrorMessage)
		{
			*OutErrorMessage = TEXT("Name contains invalid characters.");
		}
		return false;
	}

	return true;
}

FRigName URigHierarchy::GetSafeNewName(const FRigName& InPotentialNewName, ERigElementType InType, bool bAllowNameSpace) const
{
	FRigName SanitizedName = InPotentialNewName;
	SanitizeName(SanitizedName, bAllowNameSpace);

	bAllowNameSpaceWhenSanitizingName = bAllowNameSpace;
	if(ExecuteContext)
	{
		const FControlRigExecuteContext& CRContext = ExecuteContext->GetPublicData<FControlRigExecuteContext>();
		if(CRContext.IsRigModule())
		{
			FRigName LastSegmentName;
			if(SplitNameSpace(SanitizedName, nullptr, &LastSegmentName, true))
			{
				SanitizedName = LastSegmentName;
			}
			SanitizedName = CRContext.GetRigModuleNameSpace() + SanitizedName.GetName();
			bAllowNameSpaceWhenSanitizingName = true;
		}
	}

	FRigName Name = SanitizedName;

	int32 Suffix = 1;
	while (!IsNameAvailable(Name, InType))
	{
		FString BaseString = SanitizedName.GetName();
		if (BaseString.Len() > GetMaxNameLength() - 4)
		{
			BaseString.LeftChopInline(BaseString.Len() - (GetMaxNameLength() - 4));
		}
		Name.SetName(FString::Printf(TEXT("%s_%d"), *BaseString, ++Suffix));
	}

	bAllowNameSpaceWhenSanitizingName = false;
	return Name;
}

FRigName URigHierarchy::GetSafeNewDisplayName(const FRigElementKey& InParentElement, const FRigName& InPotentialNewDisplayName) const
{
	if(InPotentialNewDisplayName.IsNone())
	{
		return FRigName();
	}

	TArray<FRigElementKey> KeysToCheck;
	if(InParentElement.IsValid())
	{
		KeysToCheck = GetChildren(InParentElement);
	}
	else
	{
		// get all of the root elements
		for(const FRigBaseElement* Element : Elements)
		{
			if(!Element->IsA<FRigTransformElement>())
			{
				continue;
			}
			
			if(GetNumberOfParents(Element) == 0)
			{
				KeysToCheck.Add(Element->GetKey());
			}
		}
	}

	FRigName SanitizedName = InPotentialNewDisplayName;
	SanitizeName(SanitizedName);
	FRigName Name = SanitizedName;

	TArray<FString> DisplayNames;
	Algo::Transform(KeysToCheck, DisplayNames, [this](const FRigElementKey& InKey) -> FString
	{
		if(const FRigBaseElement* BaseElement = Find(InKey))
		{
			return BaseElement->GetDisplayName().ToString();
		}
		return FString();
	});

	int32 Suffix = 1;
	while (DisplayNames.Contains(Name.GetName()))
	{
		FString BaseString = SanitizedName.GetName();
		if (BaseString.Len() > GetMaxNameLength() - 4)
		{
			BaseString.LeftChopInline(BaseString.Len() - (GetMaxNameLength() - 4));
		}
		Name.SetName(FString::Printf(TEXT("%s_%d"), *BaseString, ++Suffix));
	}

	return Name;
}

FText URigHierarchy::GetDisplayNameForUI(const FRigBaseElement* InElement, bool bIncludeNameSpace) const
{
	check(InElement);

	if(const UModularRig* ModularRig = GetTypedOuter<UModularRig>())
	{
		const FString ShortestPath = ModularRig->GetShortestDisplayPathForElement(InElement->GetKey(), false);
		if(!ShortestPath.IsEmpty())
		{
			return FText::FromString(ShortestPath);
		}
	}

	const FName& DisplayName = InElement->GetDisplayName();
	FString DisplayNameString = DisplayName.ToString();
	(void)SplitNameSpace(DisplayNameString, nullptr, &DisplayNameString);
	
	if(bIncludeNameSpace)
	{
		const FName ModuleShortName = GetNameMetadata(InElement->Key, ShortModuleNameMetadataName, NAME_None);
		if(!ModuleShortName.IsNone())
		{
			const FString ModuleShortNameString = ModuleShortName.ToString();
			const FString ModuleDisplayName = JoinNameSpace(ModuleShortNameString, DisplayNameString);
			return FText::FromString(ModuleDisplayName);
		}
	}

	return FText::FromString(*DisplayNameString);
}

FText URigHierarchy::GetDisplayNameForUI(const FRigElementKey& InKey, bool bIncludeNameSpace) const
{
	if(const FRigBaseElement* Element = Find(InKey))
	{
		return GetDisplayNameForUI(Element, bIncludeNameSpace);
	}
	return FText();
}

int32 URigHierarchy::GetPoseVersion(const FRigElementKey& InKey) const
{
	if(const FRigTransformElement* TransformElement = Find<FRigTransformElement>(InKey))
	{
		return GetPoseVersion(TransformElement->Index);
	}
	return INDEX_NONE;
}

FEdGraphPinType URigHierarchy::GetControlPinType(FRigControlElement* InControlElement) const
{
	check(InControlElement);
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));

	// local copy of UEdGraphSchema_K2::PC_ ... static members
	static const FName PC_Boolean(TEXT("bool"));
	static const FName PC_Float(TEXT("float"));
	static const FName PC_Int(TEXT("int"));
	static const FName PC_Struct(TEXT("struct"));
	static const FName PC_Real(TEXT("real"));

	FEdGraphPinType PinType;

	switch(InControlElement->Settings.ControlType)
	{
		case ERigControlType::Bool:
		{
			PinType.PinCategory = PC_Boolean;
			break;
		}
		case ERigControlType::Float:
		case ERigControlType::ScaleFloat:
		{
			PinType.PinCategory = PC_Real;
			PinType.PinSubCategory = PC_Float;
			break;
		}
		case ERigControlType::Integer:
		{
			PinType.PinCategory = PC_Int;
			break;
		}
		case ERigControlType::Vector2D:
		{
			PinType.PinCategory = PC_Struct;
			PinType.PinSubCategoryObject = TBaseStructure<FVector2D>::Get();
			break;
		}
		case ERigControlType::Position:
		case ERigControlType::Scale:
		{
			PinType.PinCategory = PC_Struct;
			PinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
			break;
		}
		case ERigControlType::Rotator:
		{
			PinType.PinCategory = PC_Struct;
			PinType.PinSubCategoryObject = TBaseStructure<FRotator>::Get();
			break;
		}
		case ERigControlType::Transform:
		case ERigControlType::TransformNoScale:
		case ERigControlType::EulerTransform:
		{
			PinType.PinCategory = PC_Struct;
			PinType.PinSubCategoryObject = TBaseStructure<FTransform>::Get();
			break;
		}
	}

	return PinType;
}

FString URigHierarchy::GetControlPinDefaultValue(FRigControlElement* InControlElement, bool bForEdGraph, ERigControlValueType InValueType) const
{
	check(InControlElement);
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));

	FRigControlValue Value = GetControlValue(InControlElement, InValueType);
	switch(InControlElement->Settings.ControlType)
	{
		case ERigControlType::Bool:
		{
			return Value.ToString<bool>();
		}
		case ERigControlType::Float:
		case ERigControlType::ScaleFloat:
		{
			return Value.ToString<float>();
		}
		case ERigControlType::Integer:
		{
			return Value.ToString<int32>();
		}
		case ERigControlType::Vector2D:
		{
			const FVector3f Vector = Value.Get<FVector3f>();
			const FVector2D Vector2D(Vector.X, Vector.Y);

			if(bForEdGraph)
			{
				return Vector2D.ToString();
			}

			FString Result;
			TBaseStructure<FVector2D>::Get()->ExportText(Result, &Vector2D, nullptr, nullptr, PPF_None, nullptr);
			return Result;
		}
		case ERigControlType::Position:
		case ERigControlType::Scale:
		{
			if(bForEdGraph)
			{
				// NOTE: We can not use ToString() here since the FDefaultValueHelper::IsStringValidVector used in
				// EdGraphSchema_K2 expects a string with format '#,#,#', while FVector:ToString is returning the value
				// with format 'X=# Y=# Z=#'				
				const FVector Vector(Value.Get<FVector3f>());
				return FString::Printf(TEXT("%3.3f,%3.3f,%3.3f"), Vector.X, Vector.Y, Vector.Z);
			}
			return Value.ToString<FVector>();
		}
		case ERigControlType::Rotator:
		{
				if(bForEdGraph)
				{
					// NOTE: se explanations above for Position/Scale
					const FRotator Rotator = FRotator::MakeFromEuler((FVector)Value.GetRef<FVector3f>());
					return FString::Printf(TEXT("%3.3f,%3.3f,%3.3f"), Rotator.Pitch, Rotator.Yaw, Rotator.Roll);
				}
				return Value.ToString<FRotator>();
		}
		case ERigControlType::Transform:
		case ERigControlType::TransformNoScale:
		case ERigControlType::EulerTransform:
		{
			const FTransform Transform = Value.GetAsTransform(
				InControlElement->Settings.ControlType,
				InControlElement->Settings.PrimaryAxis);
				
			if(bForEdGraph)
			{
				return Transform.ToString();
			}

			FString Result;
			TBaseStructure<FTransform>::Get()->ExportText(Result, &Transform, nullptr, nullptr, PPF_None, nullptr);
			return Result;
		}
	}
	return FString();
}

TArray<FRigElementKey> URigHierarchy::GetChildren(FRigElementKey InKey, bool bRecursive) const
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));

	auto ConvertElementsToKeys = [this](TConstArrayView<FRigBaseElement*> InElements) -> TArray<FRigElementKey>
	{
		TArray<FRigElementKey> ElementKeys;
		ElementKeys.Reserve(InElements.Num());
		for (const FRigBaseElement* Element: InElements)
		{
			ElementKeys.Add(Element->Key);
		}
		return ElementKeys;
	};

	if (bRecursive)
	{
		return ConvertElementsToKeys(GetChildren(Find(InKey), true));
	}
	else
	{
		return ConvertElementsToKeys(GetChildren(Find(InKey)));
	}
}

TArray<int32> URigHierarchy::GetChildren(int32 InIndex, bool bRecursive) const
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));

	if (!ensure(Elements.IsValidIndex(InIndex)))
	{
		return {};
	}
	
	TArray<int32> ChildIndexes;

	auto AddIndexesFromElements = [this, &ChildIndexes](TConstArrayView<FRigBaseElement*> InElements) -> void
	{
		ChildIndexes.Reserve(ChildIndexes.Num() + InElements.Num());
		for (const FRigBaseElement* Element: InElements)
		{
			ChildIndexes.Add(Element->Index);
		}
	};
	
	AddIndexesFromElements(GetChildren(Elements[InIndex]));

	if (bRecursive)
	{
		// Go along the children array and add all children. Once we stop adding children, the traversal index
		// will reach the end and we're done.
		for(int32 TraversalIndex = 0; TraversalIndex != ChildIndexes.Num(); TraversalIndex++)
		{
			AddIndexesFromElements(GetChildren(Elements[ChildIndexes[TraversalIndex]]));
		}
	}

	return ChildIndexes;
}

TConstArrayView<FRigBaseElement*> URigHierarchy::GetChildren(const FRigBaseElement* InElement) const
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));
	if(InElement)
	{
		EnsureCachedChildrenAreCurrent();

		if (InElement->ChildCacheIndex != INDEX_NONE)
		{
			const FChildElementOffsetAndCount& OffsetAndCount = ChildElementOffsetAndCountCache[InElement->ChildCacheIndex];
			return TConstArrayView<FRigBaseElement*>(&ChildElementCache[OffsetAndCount.Offset], OffsetAndCount.Count);
		}
	}
	return {};
}

TArrayView<FRigBaseElement*> URigHierarchy::GetChildren(const FRigBaseElement* InElement)
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));
	if(InElement)
	{
		EnsureCachedChildrenAreCurrent();

		if (InElement->ChildCacheIndex != INDEX_NONE)
		{
			const FChildElementOffsetAndCount& OffsetAndCount = ChildElementOffsetAndCountCache[InElement->ChildCacheIndex];
			return TArrayView<FRigBaseElement*>(&ChildElementCache[OffsetAndCount.Offset], OffsetAndCount.Count);
		}
	}
	return {};
}



FRigBaseElementChildrenArray URigHierarchy::GetChildren(const FRigBaseElement* InElement, bool bRecursive) const
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));

	FRigBaseElementChildrenArray Children;

	Children.Append(GetChildren(InElement));

	if (bRecursive)
	{
		// Go along the children array and add all children. Once we stop adding children, the traversal index
		// will reach the end and we're done.
		for(int32 TraversalIndex = 0; TraversalIndex != Children.Num(); TraversalIndex++)
		{
			Children.Append(GetChildren(Children[TraversalIndex]));
		}
	}
	
	return Children;
}

TArray<FRigElementKey> URigHierarchy::GetParents(FRigElementKey InKey, bool bRecursive) const
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));
	const FRigBaseElementParentArray& Parents = GetParents(Find(InKey), bRecursive);
	TArray<FRigElementKey> Keys;
	for(const FRigBaseElement* Parent : Parents)
	{
		Keys.Add(Parent->Key);
	}
	return Keys;
}

TArray<int32> URigHierarchy::GetParents(int32 InIndex, bool bRecursive) const
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));
	const FRigBaseElementParentArray& Parents = GetParents(Get(InIndex), bRecursive);
	TArray<int32> Indices;
	for(const FRigBaseElement* Parent : Parents)
	{
		Indices.Add(Parent->Index);
	}
	return Indices;
}

FRigBaseElementParentArray URigHierarchy::GetParents(const FRigBaseElement* InElement, bool bRecursive) const
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));
	FRigBaseElementParentArray Parents;

	if(const FRigSingleParentElement* SingleParentElement = Cast<FRigSingleParentElement>(InElement))
	{
		if(SingleParentElement->ParentElement)
		{
			Parents.Add(SingleParentElement->ParentElement);
		}
	}
	else if(const FRigMultiParentElement* MultiParentElement = Cast<FRigMultiParentElement>(InElement))
	{
		Parents.Reserve(MultiParentElement->ParentConstraints.Num());
		for(const FRigElementParentConstraint& ParentConstraint : MultiParentElement->ParentConstraints)
		{
			Parents.Add(ParentConstraint.ParentElement);
		}
	}

	if(bRecursive)
	{
		const int32 CurrentNumberParents = Parents.Num();
		for(int32 ParentIndex = 0;ParentIndex < CurrentNumberParents; ParentIndex++)
		{
			const FRigBaseElementParentArray GrandParents = GetParents(Parents[ParentIndex], bRecursive);
			for (FRigBaseElement* GrandParent : GrandParents)
			{
				Parents.AddUnique(GrandParent);
			}
		}
	}

	return Parents;
}

FRigElementKey URigHierarchy::GetDefaultParent(FRigElementKey InKey) const
{
	if (DefaultParentCacheTopologyVersion != GetTopologyVersion())
	{
		DefaultParentPerElement.Reset();
		DefaultParentCacheTopologyVersion = GetTopologyVersion();
	}
	
	FRigElementKey DefaultParent;
	if(const FRigElementKey* DefaultParentPtr = DefaultParentPerElement.Find(InKey))
	{
		DefaultParent = *DefaultParentPtr;
	}
	else
	{
		DefaultParent = GetFirstParent(InKey);
		DefaultParentPerElement.Add(InKey, DefaultParent);
	}
	return DefaultParent;
}

FRigElementKey URigHierarchy::GetFirstParent(FRigElementKey InKey) const
{
	if(FRigBaseElement* FirstParent = GetFirstParent(Find(InKey)))
	{
		return FirstParent->Key;
	}
	return FRigElementKey();
}

int32 URigHierarchy::GetFirstParent(int32 InIndex) const
{
	if(FRigBaseElement* FirstParent = GetFirstParent(Get(InIndex)))
	{
		return FirstParent->Index;
	}
	return INDEX_NONE;
}

FRigBaseElement* URigHierarchy::GetFirstParent(const FRigBaseElement* InElement) const
{
	if(const FRigSingleParentElement* SingleParentElement = Cast<FRigSingleParentElement>(InElement))
	{
		return SingleParentElement->ParentElement;
	}
	else if(const FRigMultiParentElement* MultiParentElement = Cast<FRigMultiParentElement>(InElement))
	{
		if(MultiParentElement->ParentConstraints.Num() > 0)
		{
			return MultiParentElement->ParentConstraints[0].ParentElement;
		}
	}
	
	return nullptr;
}

int32 URigHierarchy::GetNumberOfParents(FRigElementKey InKey) const
{
	return GetNumberOfParents(Find(InKey));
}

int32 URigHierarchy::GetNumberOfParents(int32 InIndex) const
{
	return GetNumberOfParents(Get(InIndex));
}

int32 URigHierarchy::GetNumberOfParents(const FRigBaseElement* InElement) const
{
	if(InElement == nullptr)
	{
		return 0;
	}

	if(const FRigSingleParentElement* SingleParentElement = Cast<FRigSingleParentElement>(InElement))
	{
		return SingleParentElement->ParentElement == nullptr ? 0 : 1;
	}
	else if(const FRigMultiParentElement* MultiParentElement = Cast<FRigMultiParentElement>(InElement))
	{
		return MultiParentElement->ParentConstraints.Num();
	}

	return 0;
}

FRigElementWeight URigHierarchy::GetParentWeight(FRigElementKey InChild, FRigElementKey InParent, bool bInitial) const
{
	return GetParentWeight(Find(InChild), Find(InParent), bInitial);
}

FRigElementWeight URigHierarchy::GetParentWeight(const FRigBaseElement* InChild, const FRigBaseElement* InParent, bool bInitial) const
{
	if(const FRigMultiParentElement* MultiParentElement = Cast<FRigMultiParentElement>(InChild))
	{
		if(const int32* ParentIndexPtr = MultiParentElement->IndexLookup.Find(InParent->GetKey()))
		{
			return GetParentWeight(InChild, *ParentIndexPtr, bInitial);
		}
	}
	return FRigElementWeight(FLT_MAX);
}

FRigElementWeight URigHierarchy::GetParentWeight(const FRigBaseElement* InChild, int32 InParentIndex, bool bInitial) const
{
	if(const FRigMultiParentElement* MultiParentElement = Cast<FRigMultiParentElement>(InChild))
	{
		if(MultiParentElement->ParentConstraints.IsValidIndex(InParentIndex))
		{
			if(bInitial)
			{
				return MultiParentElement->ParentConstraints[InParentIndex].InitialWeight;
			}
			else
			{
				return MultiParentElement->ParentConstraints[InParentIndex].Weight;
			}
		}
	}
	return FRigElementWeight(FLT_MAX);
}

TArray<FRigElementWeight> URigHierarchy::GetParentWeightArray(FRigElementKey InChild, bool bInitial) const
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));
	return GetParentWeightArray(Find(InChild), bInitial);
}

TArray<FRigElementWeight> URigHierarchy::GetParentWeightArray(const FRigBaseElement* InChild, bool bInitial) const
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));
	
	TArray<FRigElementWeight> Weights;
	if(const FRigMultiParentElement* MultiParentElement = Cast<FRigMultiParentElement>(InChild))
	{
		for(int32 ParentIndex = 0; ParentIndex < MultiParentElement->ParentConstraints.Num(); ParentIndex++)
		{
			if(bInitial)
			{
				Weights.Add(MultiParentElement->ParentConstraints[ParentIndex].InitialWeight);
			}
			else
			{
				Weights.Add(MultiParentElement->ParentConstraints[ParentIndex].Weight);
			}
		}
	}
	return Weights;
}

FRigElementKey URigHierarchy::GetActiveParent(const FRigElementKey& InKey) const
{
	const TArray<FRigElementWeight> ParentWeights = GetParentWeightArray(InKey);
	if (ParentWeights.Num() > 0)
	{
		const TArray<FRigElementKey> ParentKeys = GetParents(InKey);
		check(ParentKeys.Num() == ParentWeights.Num());
		for (int32 ParentIndex = 0; ParentIndex < ParentKeys.Num(); ParentIndex++)
		{
			if (ParentWeights[ParentIndex].IsAlmostZero())
			{
				continue;
			}
			if (ParentIndex == 0)
			{
				if (!(ParentKeys[ParentIndex] == URigHierarchy::GetDefaultParentKey() || ParentKeys[ParentIndex] == URigHierarchy::GetWorldSpaceReferenceKey()))
				{
					if(ParentKeys[ParentIndex] == GetDefaultParent(InKey))
					{
						return URigHierarchy::GetDefaultParentKey();
					}
				}
			}
			return ParentKeys[ParentIndex];
		}
	}
	return URigHierarchy::GetDefaultParentKey();
}


bool URigHierarchy::SetParentWeight(FRigElementKey InChild, FRigElementKey InParent, FRigElementWeight InWeight, bool bInitial, bool bAffectChildren)
{
	return SetParentWeight(Find(InChild), Find(InParent), InWeight, bInitial, bAffectChildren);
}

bool URigHierarchy::SetParentWeight(FRigBaseElement* InChild, const FRigBaseElement* InParent, FRigElementWeight InWeight, bool bInitial, bool bAffectChildren)
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));
	if(const FRigMultiParentElement* MultiParentElement = Cast<FRigMultiParentElement>(InChild))
	{
		if(const int32* ParentIndexPtr = MultiParentElement->IndexLookup.Find(InParent->GetKey()))
		{
			return SetParentWeight(InChild, *ParentIndexPtr, InWeight, bInitial, bAffectChildren);
		}
	}
	return false;
}

bool URigHierarchy::SetParentWeight(FRigBaseElement* InChild, int32 InParentIndex, FRigElementWeight InWeight, bool bInitial, bool bAffectChildren)
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));
	using namespace ERigTransformType;

	if(FRigMultiParentElement* MultiParentElement = Cast<FRigMultiParentElement>(InChild))
	{
		if(MultiParentElement->ParentConstraints.IsValidIndex(InParentIndex))
		{
			InWeight.Location = FMath::Max(InWeight.Location, 0.f);
			InWeight.Rotation = FMath::Max(InWeight.Rotation, 0.f);
			InWeight.Scale = FMath::Max(InWeight.Scale, 0.f);

			FRigElementWeight& TargetWeight = bInitial?
				MultiParentElement->ParentConstraints[InParentIndex].InitialWeight :
				MultiParentElement->ParentConstraints[InParentIndex].Weight;

			if(FMath::IsNearlyZero(InWeight.Location - TargetWeight.Location) &&
				FMath::IsNearlyZero(InWeight.Rotation - TargetWeight.Rotation) &&
				FMath::IsNearlyZero(InWeight.Scale - TargetWeight.Scale))
			{
				return false;
			}
			
			const ERigTransformType::Type LocalType = bInitial ? InitialLocal : CurrentLocal;
			const ERigTransformType::Type GlobalType = SwapLocalAndGlobal(LocalType);

			if(bAffectChildren)
			{
				GetParentTransform(MultiParentElement, LocalType);
				if(FRigControlElement* ControlElement = Cast<FRigControlElement>(MultiParentElement))
				{
					GetControlOffsetTransform(ControlElement, LocalType);
				}
				GetTransform(MultiParentElement, LocalType);
				MultiParentElement->Pose.MarkDirty(GlobalType);
			}
			else
			{
				GetParentTransform(MultiParentElement, GlobalType);
				if(FRigControlElement* ControlElement = Cast<FRigControlElement>(MultiParentElement))
				{
					GetControlOffsetTransform(ControlElement, GlobalType);
				}
				GetTransform(MultiParentElement, GlobalType);
				MultiParentElement->Pose.MarkDirty(LocalType);
			}

			TargetWeight = InWeight;

			if(FRigControlElement* ControlElement = Cast<FRigControlElement>(MultiParentElement))
			{
				ControlElement->Offset.MarkDirty(GlobalType);
			}

			PropagateDirtyFlags(MultiParentElement, ERigTransformType::IsInitial(LocalType), bAffectChildren);
			EnsureCacheValidity();
			
#if WITH_EDITOR
			if (!bPropagatingChange)
			{
				TGuardValue<bool> bPropagatingChangeGuardValue(bPropagatingChange, true);

				ForEachListeningHierarchy([this, LocalType, InChild, InParentIndex, InWeight, bInitial, bAffectChildren](const FRigHierarchyListener& Listener)
				{
					if(!bForcePropagation && !Listener.ShouldReactToChange(LocalType))
					{
						return;
					}

					if (URigHierarchy* ListeningHierarchy = Listener.Hierarchy.Get())
					{
						if(FRigBaseElement* ListeningElement = ListeningHierarchy->Find(InChild->GetKey()))
						{
							ListeningHierarchy->SetParentWeight(ListeningElement, InParentIndex, InWeight, bInitial, bAffectChildren);
						}
					}
				});
			}
#endif

			Notify(ERigHierarchyNotification::ParentWeightsChanged, MultiParentElement);
			return true;
		}
	}
	return false;
}

bool URigHierarchy::SetParentWeightArray(FRigElementKey InChild, TArray<FRigElementWeight> InWeights, bool bInitial,
	bool bAffectChildren)
{
	return SetParentWeightArray(Find(InChild), InWeights, bInitial, bAffectChildren);
}

bool URigHierarchy::SetParentWeightArray(FRigBaseElement* InChild, const TArray<FRigElementWeight>& InWeights,
	bool bInitial, bool bAffectChildren)
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));
	if(InWeights.Num() == 0)
	{
		return false;
	}
	
	TArrayView<const FRigElementWeight> View(InWeights.GetData(), InWeights.Num());
	return SetParentWeightArray(InChild, View, bInitial, bAffectChildren);
}

bool URigHierarchy::SetParentWeightArray(FRigBaseElement* InChild,  const TArrayView<const FRigElementWeight>& InWeights,
	bool bInitial, bool bAffectChildren)
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));
	using namespace ERigTransformType;

	if(FRigMultiParentElement* MultiParentElement = Cast<FRigMultiParentElement>(InChild))
	{
		if(MultiParentElement->ParentConstraints.Num() == InWeights.Num())
		{
			TArray<FRigElementWeight> InputWeights;
			InputWeights.Reserve(InWeights.Num());

			bool bFoundDifference = false;
			for(int32 WeightIndex=0; WeightIndex < InWeights.Num(); WeightIndex++)
			{
				FRigElementWeight InputWeight = InWeights[WeightIndex];
				InputWeight.Location = FMath::Max(InputWeight.Location, 0.f);
				InputWeight.Rotation = FMath::Max(InputWeight.Rotation, 0.f);
				InputWeight.Scale = FMath::Max(InputWeight.Scale, 0.f);
				InputWeights.Add(InputWeight);

				FRigElementWeight& TargetWeight = bInitial?
					MultiParentElement->ParentConstraints[WeightIndex].InitialWeight :
					MultiParentElement->ParentConstraints[WeightIndex].Weight;

				if(!FMath::IsNearlyZero(InputWeight.Location - TargetWeight.Location) ||
					!FMath::IsNearlyZero(InputWeight.Rotation - TargetWeight.Rotation) ||
					!FMath::IsNearlyZero(InputWeight.Scale - TargetWeight.Scale))
				{
					bFoundDifference = true;
				}
			}

			if(!bFoundDifference)
			{
				return false;
			}
			
			const ERigTransformType::Type LocalType = bInitial ? InitialLocal : CurrentLocal;
			const ERigTransformType::Type GlobalType = SwapLocalAndGlobal(LocalType);

			if(bAffectChildren)
			{
				GetTransform(MultiParentElement, LocalType);
				MultiParentElement->Pose.MarkDirty(GlobalType);
			}
			else
			{
				GetTransform(MultiParentElement, GlobalType);
				MultiParentElement->Pose.MarkDirty(LocalType);
			}

			for(int32 WeightIndex=0; WeightIndex < InWeights.Num(); WeightIndex++)
			{
				if(bInitial)
				{
					MultiParentElement->ParentConstraints[WeightIndex].InitialWeight = InputWeights[WeightIndex];
				}
				else
				{
					MultiParentElement->ParentConstraints[WeightIndex].Weight = InputWeights[WeightIndex];
				}
			}

			if(FRigControlElement* ControlElement = Cast<FRigControlElement>(MultiParentElement))
			{
				ControlElement->Offset.MarkDirty(GlobalType);
				ControlElement->Shape.MarkDirty(GlobalType);
			}

			PropagateDirtyFlags(MultiParentElement, ERigTransformType::IsInitial(LocalType), bAffectChildren);
			EnsureCacheValidity();
			
#if WITH_EDITOR
			if (!bPropagatingChange)
			{
				TGuardValue<bool> bPropagatingChangeGuardValue(bPropagatingChange, true);
			
				ForEachListeningHierarchy([this, LocalType, InChild, InWeights, bInitial, bAffectChildren](const FRigHierarchyListener& Listener)
     			{
					if(!bForcePropagation && !Listener.ShouldReactToChange(LocalType))
					{
						return;
					}

					if (URigHierarchy* ListeningHierarchy = Listener.Hierarchy.Get())
					{
						if(FRigBaseElement* ListeningElement = ListeningHierarchy->Find(InChild->GetKey()))
						{
							ListeningHierarchy->SetParentWeightArray(ListeningElement, InWeights, bInitial, bAffectChildren);
						}
					}
				});
			}
#endif

			Notify(ERigHierarchyNotification::ParentWeightsChanged, MultiParentElement);

			return true;
		}
	}
	return false;
}

bool URigHierarchy::CanSwitchToParent(FRigElementKey InChild, FRigElementKey InParent, const TElementDependencyMap& InDependencyMap, FString* OutFailureReason)
{
	InParent = PreprocessParentElementKeyForSpaceSwitching(InChild, InParent);

	FRigBaseElement* Child = Find(InChild);
	if(Child == nullptr)
	{
		if(OutFailureReason)
		{
			OutFailureReason->Appendf(TEXT("Child Element %s cannot be found."), *InChild.ToString());
		}
		return false;
	}

	FRigBaseElement* Parent = Find(InParent);
	if(Parent == nullptr)
	{
		// if we don't specify anything and the element is parented directly to the world,
		// perfomring this switch means unparenting it from world (since there is no default parent)
		if(!InParent.IsValid() && GetFirstParent(InChild) == GetWorldSpaceReferenceKey())
		{
			return true;
		}
		
		if(OutFailureReason)
		{
			OutFailureReason->Appendf(TEXT("Parent Element %s cannot be found."), *InParent.ToString());
		}
		return false;
	}

	// see if this is already parented to the target parent
	if(GetFirstParent(Child) == Parent)
	{
		return true;
	}

	const FRigMultiParentElement* MultiParentChild = Cast<FRigMultiParentElement>(Child);
	if(MultiParentChild == nullptr)
	{
		if(OutFailureReason)
		{
			OutFailureReason->Appendf(TEXT("Child Element %s does not allow space switching (it's not a multi parent element)."), *InChild.ToString());
		}
	}

	const FRigTransformElement* TransformParent = Cast<FRigMultiParentElement>(Parent);
	if(TransformParent == nullptr)
	{
		if(OutFailureReason)
		{
			OutFailureReason->Appendf(TEXT("Parent Element %s is not a transform element"), *InParent.ToString());
		}
	}

	if(IsParentedTo(Parent, Child, InDependencyMap))
	{
		if(OutFailureReason)
		{
			OutFailureReason->Appendf(TEXT("Cannot switch '%s' to '%s' - would cause a cycle."), *InChild.ToString(), *InParent.ToString());
		}
		return false;
	}

	return true;
}

bool URigHierarchy::SwitchToParent(FRigElementKey InChild, FRigElementKey InParent, bool bInitial, bool bAffectChildren, const TElementDependencyMap& InDependencyMap, FString* OutFailureReason)
{
	InParent = PreprocessParentElementKeyForSpaceSwitching(InChild, InParent);
	return SwitchToParent(Find(InChild), Find(InParent), bInitial, bAffectChildren, InDependencyMap, OutFailureReason);
}

bool URigHierarchy::SwitchToParent(FRigBaseElement* InChild, FRigBaseElement* InParent, bool bInitial,
	bool bAffectChildren, const TElementDependencyMap& InDependencyMap, FString* OutFailureReason)
{
	FRigHierarchyEnableControllerBracket EnableController(this, true);

	// rely on the VM's dependency map if there's currently an available context.
	const TElementDependencyMap* DependencyMapPtr = &InDependencyMap;

#if WITH_EDITOR
	// Keep this in function scope, since we might be taking a pointer to it.
	TElementDependencyMap DependencyMapFromVM;
	{
		FScopeLock Lock(&ExecuteContextLock);
		if(ExecuteContext != nullptr && DependencyMapPtr->IsEmpty())
		{
			if(ExecuteContext->VM)
			{
				DependencyMapFromVM = GetDependenciesForVM(ExecuteContext->VM);
				DependencyMapPtr = &DependencyMapFromVM;
			}
		}
	}
	
#endif
	
	if(InChild && InParent)
	{
		if(!CanSwitchToParent(InChild->GetKey(), InParent->GetKey(), *DependencyMapPtr, OutFailureReason))
		{
			return false;
		}
	}

	// Exit early if switching to the same parent 
	if (InChild)
	{
		const FRigElementKey ChildKey = InChild->GetKey();
		const FRigElementKey ParentKey = InParent ? InParent->GetKey() : GetDefaultParent(ChildKey);
		const FRigElementKey ActiveParentKey = GetActiveParent(ChildKey);
		if(ActiveParentKey == ParentKey ||
			(ActiveParentKey == URigHierarchy::GetDefaultParentKey() && GetDefaultParent(ChildKey) == ParentKey))
		{
			return true;
		}
	}
	
	if(const FRigMultiParentElement* MultiParentElement = Cast<FRigMultiParentElement>(InChild))
	{
		int32 ParentIndex = INDEX_NONE;
		if(InParent)
		{
			if(const int32* ParentIndexPtr = MultiParentElement->IndexLookup.Find(InParent->GetKey()))
			{
				ParentIndex = *ParentIndexPtr;
			}
			else
			{
				if(URigHierarchyController* Controller = GetController(true))
				{
					if(Controller->AddParent(InChild, InParent, 0.f, true, false))
					{
						ParentIndex = MultiParentElement->IndexLookup.FindChecked(InParent->GetKey());
					}
				}
			}
		}
		return SwitchToParent(InChild, ParentIndex, bInitial, bAffectChildren);
	}
	return false;
}

bool URigHierarchy::SwitchToParent(FRigBaseElement* InChild, int32 InParentIndex, bool bInitial, bool bAffectChildren)
{
	TArray<FRigElementWeight> Weights = GetParentWeightArray(InChild, bInitial);
	FMemory::Memzero(Weights.GetData(), Weights.GetAllocatedSize());
	if(Weights.IsValidIndex(InParentIndex))
	{
		Weights[InParentIndex] = 1.f;
	}
	return SetParentWeightArray(InChild, Weights, bInitial, bAffectChildren);
}

bool URigHierarchy::SwitchToDefaultParent(FRigElementKey InChild, bool bInitial, bool bAffectChildren)
{
	return SwitchToParent(InChild, GetDefaultParentKey(), bInitial, bAffectChildren);
}

bool URigHierarchy::SwitchToDefaultParent(FRigBaseElement* InChild, bool bInitial, bool bAffectChildren)
{
	// we assume that the first stored parent is the default parent
	check(InChild);
	return SwitchToParent(InChild->GetKey(), GetDefaultParentKey(), bInitial, bAffectChildren);
}

bool URigHierarchy::SwitchToWorldSpace(FRigElementKey InChild, bool bInitial, bool bAffectChildren)
{
	return SwitchToParent(InChild, GetWorldSpaceReferenceKey(), bInitial, bAffectChildren);
}

bool URigHierarchy::SwitchToWorldSpace(FRigBaseElement* InChild, bool bInitial, bool bAffectChildren)
{
	check(InChild);
	return SwitchToParent(InChild->GetKey(), GetWorldSpaceReferenceKey(), bInitial, bAffectChildren);
}

FRigElementKey URigHierarchy::GetOrAddWorldSpaceReference()
{
	FRigHierarchyEnableControllerBracket EnableController(this, true);

	const FRigElementKey WorldSpaceReferenceKey = GetWorldSpaceReferenceKey();

	FRigBaseElement* Parent = Find(WorldSpaceReferenceKey);
	if(Parent)
	{
		return Parent->GetKey();
	}

	if(URigHierarchyController* Controller = GetController(true))
	{
		return Controller->AddReference(
			WorldSpaceReferenceKey.Name,
			FRigElementKey(),
			FRigReferenceGetWorldTransformDelegate::CreateUObject(this, &URigHierarchy::GetWorldTransformForReference),
			false);
	}

	return FRigElementKey();
}

FRigElementKey URigHierarchy::GetDefaultParentKey()
{
	static const FName DefaultParentName = TEXT("DefaultParent");
	return FRigElementKey(DefaultParentName, ERigElementType::Reference); 
}

FRigElementKey URigHierarchy::GetWorldSpaceReferenceKey()
{
	static const FName WorldSpaceReferenceName = TEXT("WorldSpace");
	return FRigElementKey(WorldSpaceReferenceName, ERigElementType::Reference); 
}

TArray<FRigElementKey> URigHierarchy::GetAllKeys(bool bTraverse, ERigElementType InElementType) const
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"))

	return GetKeysByPredicate([InElementType](const FRigBaseElement& InElement)
	{
		return InElement.IsTypeOf(InElementType);
	}, bTraverse);
}

TArray<FRigElementKey> URigHierarchy::GetKeysByPredicate(
	TFunctionRef<bool(const FRigBaseElement&)> InPredicateFunc,
	bool bTraverse
	) const
{
	auto ElementTraverser = [&](TFunctionRef<void(const FRigBaseElement&)> InProcessFunc)
	{
		if(bTraverse)
		{
			// TBitArray reserves 4, we'll do 16 so we can remember at least 512 elements before
			// we need to hit the heap.
			TBitArray<TInlineAllocator<16>> ElementVisited(false, Elements.Num());

			const TArray<FRigBaseElement*> RootElements = GetRootElements();
			for (FRigBaseElement* Element : RootElements)
			{
				const int32 ElementIndex = Element->GetIndex();
				Traverse(Element, true, [&ElementVisited, InProcessFunc, InPredicateFunc](FRigBaseElement* InElement, bool& bContinue)
				{
					bContinue = !ElementVisited[InElement->GetIndex()];

					if(bContinue)
					{
						if(InPredicateFunc(*InElement))
						{
							InProcessFunc(*InElement);
						}
						ElementVisited[InElement->GetIndex()] = true;
					}
				});
			}
		}
		else
		{
			for (int32 ElementIndex = 0; ElementIndex < Elements.Num(); ElementIndex++)
			{
				const FRigBaseElement* Element = Elements[ElementIndex];
				if(InPredicateFunc(*Element))
				{
					InProcessFunc(*Element);
				}
			}
		}
	};
	
	// First count up how many elements we matched and only reserve that amount. There's very little overhead
	// since we're just running over the same data, so it should still be hot when we do the second pass.
	int32 NbElements = 0;
	ElementTraverser([&NbElements](const FRigBaseElement&) { NbElements++; });
	
	TArray<FRigElementKey> Keys;
	Keys.Reserve(NbElements);
	ElementTraverser([&Keys](const FRigBaseElement& InElement) { Keys.Add(InElement.GetKey()); });

	return Keys;
}

void URigHierarchy::Traverse(FRigBaseElement* InElement, bool bTowardsChildren,
                             TFunction<void(FRigBaseElement*, bool&)> PerElementFunction) const
{
	bool bContinue = true;
	PerElementFunction(InElement, bContinue);

	if(bContinue)
	{
		if(bTowardsChildren)
		{
			for (FRigBaseElement* Child : GetChildren(InElement))
			{
				Traverse(Child, true, PerElementFunction);
			}
		}
		else
		{
			FRigBaseElementParentArray Parents = GetParents(InElement);
			for (FRigBaseElement* Parent : Parents)
			{
				Traverse(Parent, false, PerElementFunction);
			}
		}
	}
}

void URigHierarchy::Traverse(TFunction<void(FRigBaseElement*, bool& /* continue */)> PerElementFunction, bool bTowardsChildren) const
{
	if(bTowardsChildren)
	{
		for (int32 ElementIndex = 0; ElementIndex < Elements.Num(); ElementIndex++)
		{
			FRigBaseElement* Element = Elements[ElementIndex];
			if(GetNumberOfParents(Element) == 0)
			{
				Traverse(Element, bTowardsChildren, PerElementFunction);
			}
        }
	}
	else
	{
		for (int32 ElementIndex = 0; ElementIndex < Elements.Num(); ElementIndex++)
		{
			FRigBaseElement* Element = Elements[ElementIndex];
			if(GetChildren(Element).Num() == 0)
			{
				Traverse(Element, bTowardsChildren, PerElementFunction);
			}
		}
	}
}

const FRigElementKey& URigHierarchy::GetResolvedTarget(const FRigElementKey& InConnectorKey) const
{
	if (InConnectorKey.Type == ERigElementType::Connector)
	{
		if(ElementKeyRedirector)
		{
			if(const FCachedRigElement* Target = ElementKeyRedirector->Find(InConnectorKey))
			{
				return Target->GetKey();
			}
		}
	}
	return InConnectorKey;
}

bool URigHierarchy::Undo()
{
#if WITH_EDITOR
	
	if(TransformUndoStack.IsEmpty())
	{
		return false;
	}

	const FRigTransformStackEntry Entry = TransformUndoStack.Pop();
	ApplyTransformFromStack(Entry, true);
	UndoRedoEvent.Broadcast(this, Entry.Key, Entry.TransformType, Entry.OldTransform, true);
	TransformRedoStack.Push(Entry);
	TransformStackIndex = TransformUndoStack.Num();
	return true;
	
#else
	
	return false;
	
#endif
}

bool URigHierarchy::Redo()
{
#if WITH_EDITOR

	if(TransformRedoStack.IsEmpty())
	{
		return false;
	}

	const FRigTransformStackEntry Entry = TransformRedoStack.Pop();
	ApplyTransformFromStack(Entry, false);
	UndoRedoEvent.Broadcast(this, Entry.Key, Entry.TransformType, Entry.NewTransform, false);
	TransformUndoStack.Push(Entry);
	TransformStackIndex = TransformUndoStack.Num();
	return true;
	
#else
	
	return false;
	
#endif
}

bool URigHierarchy::SetTransformStackIndex(int32 InTransformStackIndex)
{
#if WITH_EDITOR

	while(TransformUndoStack.Num() > InTransformStackIndex)
	{
		if(TransformUndoStack.Num() == 0)
		{
			return false;
		}

		if(!Undo())
		{
			return false;
		}
	}
	
	while(TransformUndoStack.Num() < InTransformStackIndex)
	{
		if(TransformRedoStack.Num() == 0)
		{
			return false;
		}

		if(!Redo())
		{
			return false;
		}
	}

	return InTransformStackIndex == TransformStackIndex;

#else
	
	return false;
	
#endif
}

#if WITH_EDITOR

void URigHierarchy::PostEditUndo()
{
	Super::PostEditUndo();

	const int32 DesiredStackIndex = TransformStackIndex;
	TransformStackIndex = TransformUndoStack.Num();
	if (DesiredStackIndex != TransformStackIndex)
	{
		SetTransformStackIndex(DesiredStackIndex);
	}

	if(URigHierarchyController* Controller = GetController(false))
	{
		Controller->SetHierarchy(this);
	}
}

#endif

void URigHierarchy::SendEvent(const FRigEventContext& InEvent, bool bAsynchronous)
{
	if(EventDelegate.IsBound())
	{
		TWeakObjectPtr<URigHierarchy> WeakThis = this;
		FRigEventDelegate& Delegate = EventDelegate;

		if (bAsynchronous)
		{
			FFunctionGraphTask::CreateAndDispatchWhenReady([WeakThis, Delegate, InEvent]()
            {
                Delegate.Broadcast(WeakThis.Get(), InEvent);
            }, TStatId(), NULL, ENamedThreads::GameThread);
		}
		else
		{
			Delegate.Broadcast(this, InEvent);
		}
	}

}

void URigHierarchy::SendAutoKeyEvent(FRigElementKey InElement, float InOffsetInSeconds, bool bAsynchronous)
{
	FRigEventContext Context;
	Context.Event = ERigEvent::RequestAutoKey;
	Context.Key = InElement;
	Context.LocalTime = InOffsetInSeconds;
	if(UControlRig* Rig = Cast<UControlRig>(GetOuter()))
	{
		Context.LocalTime += Rig->AbsoluteTime;
	}
	SendEvent(Context, bAsynchronous);
}

bool URigHierarchy::IsControllerAvailable() const
{
	return bIsControllerAvailable;
}

URigHierarchyController* URigHierarchy::GetController(bool bCreateIfNeeded)
{
	if(!IsControllerAvailable())
	{
		return nullptr;
	}
	if(HierarchyController)
	{
		return HierarchyController;
	}
	else if(bCreateIfNeeded)
	{
		 {
			 FGCScopeGuard Guard;
			 HierarchyController = NewObject<URigHierarchyController>(this, TEXT("HierarchyController"), RF_Transient);
			 // In case we create this object from async loading thread
			 HierarchyController->ClearInternalFlags(EInternalObjectFlags::Async);

			 HierarchyController->SetHierarchy(this);
			 return HierarchyController;
		 }
	}
	return nullptr;
}

UModularRigRuleManager* URigHierarchy::GetRuleManager(bool bCreateIfNeeded)
{
	if(RuleManager)
	{
		return RuleManager;
	}
	else if(bCreateIfNeeded)
	{
		{
			FGCScopeGuard Guard;
			RuleManager = NewObject<UModularRigRuleManager>(this, TEXT("RuleManager"), RF_Transient);
			// In case we create this object from async loading thread
			RuleManager->ClearInternalFlags(EInternalObjectFlags::Async);

			RuleManager->SetHierarchy(this);
			return RuleManager;
		}
	}
	return nullptr;
}

void URigHierarchy::IncrementTopologyVersion()
{
	TopologyVersion++;
	KeyCollectionCache.Reset();
}

FRigPose URigHierarchy::GetPose(
	bool bInitial,
	ERigElementType InElementType,
	const FRigElementKeyCollection& InItems,
	bool bIncludeTransientControls
) const
{
	return GetPose(bInitial, InElementType, TArrayView<const FRigElementKey>(InItems.Keys.GetData(), InItems.Num()), bIncludeTransientControls);
}

FRigPose URigHierarchy::GetPose(bool bInitial, ERigElementType InElementType,
	const TArrayView<const FRigElementKey>& InItems, bool bIncludeTransientControls) const
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));
	FRigPose Pose;
	Pose.HierarchyTopologyVersion = GetTopologyVersion();
	Pose.PoseHash = Pose.HierarchyTopologyVersion;

	for (int32 ElementIndex = 0; ElementIndex < Elements.Num(); ElementIndex++)
	{
		FRigBaseElement* Element = Elements[ElementIndex];

		// filter by type
		if (((uint8)InElementType & (uint8)Element->GetType()) == 0)
		{
			continue;
		}

		// filter by optional collection
		if(InItems.Num() > 0)
		{
			if(!InItems.Contains(Element->GetKey()))
			{
				continue;
			}
		}
		
		FRigPoseElement PoseElement;
		PoseElement.Index.UpdateCache(Element->GetKey(), this);
		
		if(FRigTransformElement* TransformElement = Cast<FRigTransformElement>(Element))
		{
			PoseElement.LocalTransform = GetTransform(TransformElement, bInitial ? ERigTransformType::InitialLocal : ERigTransformType::CurrentLocal);
			PoseElement.GlobalTransform = GetTransform(TransformElement, bInitial ? ERigTransformType::InitialGlobal : ERigTransformType::CurrentGlobal);
			PoseElement.ActiveParent = GetActiveParent(Element->GetKey());

			if(FRigControlElement* ControlElement = Cast<FRigControlElement>(Element))
			{
				if (bUsePreferredEulerAngles)
				{
					PoseElement.PreferredEulerAngle = GetControlPreferredEulerAngles(ControlElement,
					   GetControlPreferredEulerRotationOrder(ControlElement), bInitial);
				}

				if(!bIncludeTransientControls && ControlElement->Settings.bIsTransientControl)
				{
					continue;
				}
			}
		}
		else if(FRigCurveElement* CurveElement = Cast<FRigCurveElement>(Element))
		{
			PoseElement.CurveValue = GetCurveValue(CurveElement);
		}
		else
		{
			continue;
		}
		Pose.Elements.Add(PoseElement);
		Pose.PoseHash = HashCombine(Pose.PoseHash, GetTypeHash(PoseElement.Index.GetKey()));
	}
	return Pose;
}

void URigHierarchy::SetPose(
	const FRigPose& InPose,
	ERigTransformType::Type InTransformType,
	ERigElementType InElementType,
	const FRigElementKeyCollection& InItems,
	float InWeight
)
{
	SetPose(InPose, InTransformType, InElementType, TArrayView<const FRigElementKey>(InItems.Keys.GetData(), InItems.Num()), InWeight);
}

void URigHierarchy::SetPose(const FRigPose& InPose, ERigTransformType::Type InTransformType,
	ERigElementType InElementType, const TArrayView<const FRigElementKey>& InItems, float InWeight)
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));
	const float U = FMath::Clamp(InWeight, 0.f, 1.f);
	if(U < SMALL_NUMBER)
	{
		return;
	}

	for(const FRigPoseElement& PoseElement : InPose)
	{
		FCachedRigElement Index = PoseElement.Index;

		// filter by type
		if (((uint8)InElementType & (uint8)Index.GetKey().Type) == 0)
		{
			continue;
		}

		// filter by optional collection
		if(InItems.Num() > 0)
		{
			if(!InItems.Contains(Index.GetKey()))
			{
				continue;
			}
		}

		if(Index.UpdateCache(this))
		{
			FRigBaseElement* Element = Get(Index.GetIndex());
			if(FRigTransformElement* TransformElement = Cast<FRigTransformElement>(Element))
			{
				FTransform TransformToSet =
					ERigTransformType::IsLocal(InTransformType) ?
						PoseElement.LocalTransform :
						PoseElement.GlobalTransform;
				
				if(U < 1.f - SMALL_NUMBER)
				{
					const FTransform PreviousTransform = GetTransform(TransformElement, InTransformType);
					TransformToSet = FControlRigMathLibrary::LerpTransform(PreviousTransform, TransformToSet, U);
				}

				if (PoseElement.ActiveParent.IsValid())
				{
					SwitchToParent(Element->GetKey(), PoseElement.ActiveParent);
				}
				SetTransform(TransformElement, TransformToSet, InTransformType, true);
			}
			else if(FRigCurveElement* CurveElement = Cast<FRigCurveElement>(Element))
			{
				SetCurveValue(CurveElement, PoseElement.CurveValue);
			}
		}
	}
}

void URigHierarchy::Notify(ERigHierarchyNotification InNotifType, const FRigBaseElement* InElement)
{
	if(bSuspendNotifications)
	{
		return;
	}

	if (!IsValid(this))
	{
		return;
	}

	// if we are running a VM right now
	{
		FScopeLock Lock(&ExecuteContextLock);
		if(ExecuteContext != nullptr)
		{
			QueueNotification(InNotifType, InElement);
			return;
		}
	}
	

	if(QueuedNotifications.IsEmpty())
	{
		ModifiedEvent.Broadcast(InNotifType, this, InElement);
		if(ModifiedEventDynamic.IsBound())
		{
			FRigElementKey Key;
			if(InElement)
			{
				Key = InElement->GetKey();
			}
			ModifiedEventDynamic.Broadcast(InNotifType, this, Key);
		}
	}
	else
	{
		QueueNotification(InNotifType, InElement);
		SendQueuedNotifications();
	}

#if WITH_EDITOR

	// certain events needs to be forwarded to the listening hierarchies.
	// this mainly has to do with topological change within the hierarchy.
	switch (InNotifType)
	{
		case ERigHierarchyNotification::ElementAdded:
		case ERigHierarchyNotification::ElementRemoved:
		case ERigHierarchyNotification::ElementRenamed:
		case ERigHierarchyNotification::ParentChanged:
		case ERigHierarchyNotification::ParentWeightsChanged:
		{
			if (ensure(InElement != nullptr))
			{
				ForEachListeningHierarchy([this, InNotifType, InElement](const FRigHierarchyListener& Listener)
				{
					if (URigHierarchy* ListeningHierarchy = Listener.Hierarchy.Get())
					{			
						if(const FRigBaseElement* ListeningElement = ListeningHierarchy->Find( InElement->GetKey()))
						{
							ListeningHierarchy->Notify(InNotifType, ListeningElement);
						}
					}
				});
			}
			break;
		}
		default:
		{
			break;
		}
	}

#endif
}

void URigHierarchy::QueueNotification(ERigHierarchyNotification InNotification, const FRigBaseElement* InElement)
{
	FQueuedNotification Entry;
	Entry.Type = InNotification;
	Entry.Key = InElement ? InElement->GetKey() : FRigElementKey();
	QueuedNotifications.Enqueue(Entry);
}

void URigHierarchy::SendQueuedNotifications()
{
	if(bSuspendNotifications)
	{
		QueuedNotifications.Empty();
		return;
	}

	if(QueuedNotifications.IsEmpty())
	{
		return;
	}

	{
		FScopeLock Lock(&ExecuteContextLock);
    	if(ExecuteContext != nullptr)
    	{
    		return;
    	}
	}
	
	// enable access to the controller during this method
	FRigHierarchyEnableControllerBracket EnableController(this, true);

	// we'll collect all notifications and will clean them up
	// to guard against notification storms.
	TArray<FQueuedNotification> AllNotifications;
	FQueuedNotification EntryFromQueue;
	while(QueuedNotifications.Dequeue(EntryFromQueue))
	{
		AllNotifications.Add(EntryFromQueue);
	}
	QueuedNotifications.Empty();

	// now we'll filter the notifications. we'll go through them in
	// reverse and will skip any aggregates (like change color multiple times on the same thing)
	// as well as collapse pairs such as select and deselect
	TArray<FQueuedNotification> FilteredNotifications;
	TArray<FQueuedNotification> UniqueNotifications;
	for(int32 Index = AllNotifications.Num() - 1; Index >= 0; Index--)
	{
		const FQueuedNotification& Entry = AllNotifications[Index];

		bool bSkipNotification = false;
		switch(Entry.Type)
		{
			case ERigHierarchyNotification::HierarchyReset:
			case ERigHierarchyNotification::ElementRemoved:
			case ERigHierarchyNotification::ElementRenamed:
			{
				// we don't allow these to happen during the run of a VM
				if(const URigHierarchyController* Controller = GetController())
				{
					static constexpr TCHAR InvalidNotificationReceivedMessage[] =
						TEXT("Found invalid queued notification %s - %s. Skipping notification.");
					const FString NotificationText = StaticEnum<ERigHierarchyNotification>()->GetNameStringByValue((int64)Entry.Type);
					
					Controller->ReportErrorf(InvalidNotificationReceivedMessage, *NotificationText, *Entry.Key.ToString());	
				}
				bSkipNotification = true;
				break;
			}
			case ERigHierarchyNotification::ControlSettingChanged:
			case ERigHierarchyNotification::ControlVisibilityChanged:
			case ERigHierarchyNotification::ControlDrivenListChanged:
			case ERigHierarchyNotification::ControlShapeTransformChanged:
			case ERigHierarchyNotification::ParentChanged:
			case ERigHierarchyNotification::ParentWeightsChanged:
			{
				// these notifications are aggregates - they don't need to happen
				// more than once during an update
				bSkipNotification = UniqueNotifications.Contains(Entry);
				break;
			}
			case ERigHierarchyNotification::ElementSelected:
			case ERigHierarchyNotification::ElementDeselected:
			{
				FQueuedNotification OppositeEntry;
				OppositeEntry.Type = (Entry.Type == ERigHierarchyNotification::ElementSelected) ?
					ERigHierarchyNotification::ElementDeselected :
					ERigHierarchyNotification::ElementSelected;
				OppositeEntry.Key = Entry.Key;

				// we don't need to add this if we already performed the selection or
				// deselection of the same item before
				bSkipNotification = UniqueNotifications.Contains(Entry) ||
					UniqueNotifications.Contains(OppositeEntry);
				break;
			}
			case ERigHierarchyNotification::Max:
			{
				bSkipNotification = true;
				break;
			}
			case ERigHierarchyNotification::ElementAdded:
			case ERigHierarchyNotification::InteractionBracketOpened:
			case ERigHierarchyNotification::InteractionBracketClosed:
			default:
			{
				break;
			}
		}

		UniqueNotifications.AddUnique(Entry);
		if(!bSkipNotification)
		{
			FilteredNotifications.Add(Entry);
		}

		// if we ever hit a reset then we don't need to deal with
		// any previous notifications.
		if(Entry.Type == ERigHierarchyNotification::HierarchyReset)
		{
			break;
		}
	}

	if(FilteredNotifications.IsEmpty())
	{
		return;
	}

	ModifiedEvent.Broadcast(ERigHierarchyNotification::InteractionBracketOpened, this, nullptr);
	if(ModifiedEventDynamic.IsBound())
	{
		ModifiedEventDynamic.Broadcast(ERigHierarchyNotification::InteractionBracketOpened, this, FRigElementKey());
	}

	// finally send all of the notifications
	// (they have been added in the reverse order to the array)
	for(int32 Index = FilteredNotifications.Num() - 1; Index >= 0; Index--)
	{
		const FQueuedNotification& Entry = FilteredNotifications[Index];
		const FRigBaseElement* Element = Find(Entry.Key);
		if(Element)
		{
			ModifiedEvent.Broadcast(Entry.Type, this, Element);
			if(ModifiedEventDynamic.IsBound())
			{
				ModifiedEventDynamic.Broadcast(Entry.Type, this, Element->GetKey());
			}
		}
	}

	ModifiedEvent.Broadcast(ERigHierarchyNotification::InteractionBracketClosed, this, nullptr);
	if(ModifiedEventDynamic.IsBound())
	{
		ModifiedEventDynamic.Broadcast(ERigHierarchyNotification::InteractionBracketClosed, this, FRigElementKey());
	}
}

FTransform URigHierarchy::GetTransform(FRigTransformElement* InTransformElement,
	const ERigTransformType::Type InTransformType) const
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));
	if(InTransformElement == nullptr)
	{
		return FTransform::Identity;
	}

#if WITH_EDITOR
	{
		FScopeLock Lock(&ExecuteContextLock);
		if(bRecordTransformsAtRuntime && ExecuteContext != nullptr)
		{
			ReadTransformsAtRuntime.Emplace(
				ExecuteContext->GetPublicData<>().GetInstructionIndex(),
				ExecuteContext->GetSlice().GetIndex(),
				InTransformElement->GetIndex(),
				InTransformType
			);
		}
	}
	
	TGuardValue<bool> RecordTransformsPerInstructionGuard(bRecordTransformsAtRuntime, false);
	
#endif
	
	if(InTransformElement->Pose.IsDirty(InTransformType))
	{
		const ERigTransformType::Type OpposedType = SwapLocalAndGlobal(InTransformType);
		const ERigTransformType::Type GlobalType = MakeGlobal(InTransformType);
		ensure(!InTransformElement->Pose.IsDirty(OpposedType));

		FTransform ParentTransform;
		if(IsLocal(InTransformType))
		{
			// if we have a zero scale provided - and the parent also contains a zero scale,
			// we'll keep the local translation and scale since otherwise we'll loose the values.
			// we cannot compute the local from the global if the scale is 0 - since the local scale
			// may be anything - any translation or scale multiplied with the parent's zero scale is zero. 
			auto CompensateZeroScale = [this, InTransformElement, InTransformType](FTransform& Transform)
			{
				const FVector Scale = Transform.GetScale3D();
				if(FMath::IsNearlyZero(Scale.X) || FMath::IsNearlyZero(Scale.Y) || FMath::IsNearlyZero(Scale.Z))
				{
					const FTransform ParentTransform =
						GetParentTransform(InTransformElement, ERigTransformType::SwapLocalAndGlobal(InTransformType));
					const FVector ParentScale = ParentTransform.GetScale3D();
					if(FMath::IsNearlyZero(ParentScale.X) || FMath::IsNearlyZero(ParentScale.Y) || FMath::IsNearlyZero(ParentScale.Z))
					{
						Transform.SetTranslation(InTransformElement->Pose.Get(InTransformType).GetTranslation());
						Transform.SetScale3D(InTransformElement->Pose.Get(InTransformType).GetScale3D());
					}
				}
			};
			
			if(FRigControlElement* ControlElement = Cast<FRigControlElement>(InTransformElement))
			{
				FTransform NewTransform = ComputeLocalControlValue(ControlElement, ControlElement->Pose.Get(OpposedType), GlobalType);
				CompensateZeroScale(NewTransform);
				ControlElement->Pose.Set(InTransformType, NewTransform);
				/** from mikez we do not want geting a pose to set these preferred angles
				switch(ControlElement->Settings.ControlType)
				{
					case ERigControlType::Rotator:
					case ERigControlType::EulerTransform:
					case ERigControlType::Transform:
					case ERigControlType::TransformNoScale:
					{
						ControlElement->PreferredEulerAngles.SetRotator(NewTransform.Rotator(), IsInitial(InTransformType), true);
						break;
					}
					default:
					{
						break;
					}
					
				}*/
			}
			else if(FRigMultiParentElement* MultiParentElement = Cast<FRigMultiParentElement>(InTransformElement))
			{
				// this is done for nulls and any element that can have more than one parent which 
				// is not a control
				const FTransform& GlobalTransform = MultiParentElement->Pose.Get(GlobalType);
				FTransform LocalTransform = InverseSolveParentConstraints(
					GlobalTransform, 
					MultiParentElement->ParentConstraints, GlobalType, FTransform::Identity);
				CompensateZeroScale(LocalTransform);
				MultiParentElement->Pose.Set(InTransformType, LocalTransform);
			}
			else
			{
				ParentTransform = GetParentTransform(InTransformElement, GlobalType);

				FTransform NewTransform = InTransformElement->Pose.Get(OpposedType).GetRelativeTransform(ParentTransform);
				NewTransform.NormalizeRotation();
				CompensateZeroScale(NewTransform);
				InTransformElement->Pose.Set(InTransformType, NewTransform);
			}
		}
		else
		{
			if(FRigControlElement* ControlElement = Cast<FRigControlElement>(InTransformElement))
			{
				// using GetControlOffsetTransform to check dirty flag before accessing the transform
				// note: no need to do the same for Pose.Local because there is already an ensure:
				// "ensure(!InTransformElement->Pose.IsDirty(OpposedType));" above
				const FTransform NewTransform = SolveParentConstraints(
					ControlElement->ParentConstraints, InTransformType,
					GetControlOffsetTransform(ControlElement, OpposedType), true,
					ControlElement->Pose.Get(OpposedType), true);
				ControlElement->Pose.Set(InTransformType, NewTransform);
			}
			else if(FRigMultiParentElement* MultiParentElement = Cast<FRigMultiParentElement>(InTransformElement))
			{
				// this is done for nulls and any element that can have more than one parent which 
				// is not a control
				const FTransform NewTransform = SolveParentConstraints(
					MultiParentElement->ParentConstraints, InTransformType,
					FTransform::Identity, false,
					MultiParentElement->Pose.Get(OpposedType), true);
				MultiParentElement->Pose.Set(InTransformType, NewTransform);
			}
			else
			{
				ParentTransform = GetParentTransform(InTransformElement, GlobalType);

				FTransform NewTransform = InTransformElement->Pose.Get(OpposedType) * ParentTransform;
				NewTransform.NormalizeRotation();
				InTransformElement->Pose.Set(InTransformType, NewTransform);
			}
		}

		EnsureCacheValidity();
	}
	return InTransformElement->Pose.Get(InTransformType);
}

void URigHierarchy::SetTransform(FRigTransformElement* InTransformElement, const FTransform& InTransform, const ERigTransformType::Type InTransformType, bool bAffectChildren, bool bSetupUndo, bool bForce, bool bPrintPythonCommands)
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));
	if(InTransformElement == nullptr)
	{
		return;
	}

	if(IsGlobal(InTransformType))
	{
		if(FRigControlElement* ControlElement = Cast<FRigControlElement>(InTransformElement))
		{
			FTransform LocalTransform = ComputeLocalControlValue(ControlElement, InTransform, InTransformType);
			ControlElement->Settings.ApplyLimits(LocalTransform);
			SetTransform(ControlElement, LocalTransform, MakeLocal(InTransformType), bAffectChildren, false, false, bPrintPythonCommands);
			return;
		}
	}

#if WITH_EDITOR

	// lock execute context scope
	{
		FScopeLock Lock(&ExecuteContextLock);
	
		if(bRecordTransformsAtRuntime && ExecuteContext)
		{
			const FRigVMExecuteContext& PublicData = ExecuteContext->GetPublicData<>();
			const FRigVMSlice& Slice = ExecuteContext->GetSlice();
			WrittenTransformsAtRuntime.Emplace(
				PublicData.GetInstructionIndex(),
				Slice.GetIndex(),
				InTransformElement->GetIndex(),
				InTransformType
			);

			// if we are setting a control / null parent after a child here - let's let the user know
			if(InTransformElement->IsA<FRigControlElement>() || InTransformElement->IsA<FRigNullElement>())
			{
				if(const UWorld* World = GetWorld())
				{
					// only fire these notes if we are inside the asset editor
					if(World->WorldType == EWorldType::EditorPreview)
					{
						for (FRigBaseElement* Child : GetChildren(InTransformElement))
						{
							const bool bChildFound = WrittenTransformsAtRuntime.ContainsByPredicate([Child](const TInstructionSliceElement& Entry) -> bool
							{
								return Entry.Get<2>() == Child->GetIndex();
							});

							if(bChildFound)
							{
								const FControlRigExecuteContext& CRContext = ExecuteContext->GetPublicData<FControlRigExecuteContext>();
								if(CRContext.GetLog())
								{
									static constexpr TCHAR MessageFormat[] = TEXT("Setting transform of parent (%s) after setting child (%s).\nThis may lead to unexpected results.");
									const FString& Message = FString::Printf(
										MessageFormat,
										*InTransformElement->GetName(),
										*Child->GetName());
									CRContext.Report(
										EMessageSeverity::Info,
										ExecuteContext->GetPublicData<>().GetFunctionName(),
										ExecuteContext->GetPublicData<>().GetInstructionIndex(),
										Message);
								}
							}
						}
					}
				}
			}
		}
	}
	
	TGuardValue<bool> RecordTransformsPerInstructionGuard(bRecordTransformsAtRuntime, false);
	
#endif

	if(!InTransformElement->Pose.IsDirty(InTransformType))
	{
		const FTransform PreviousTransform = InTransformElement->Pose.Get(InTransformType);
		if(!bForce && FRigComputedTransform::Equals(PreviousTransform, InTransform))
		{
			return;
		}
	}

	const FTransform PreviousTransform = GetTransform(InTransformElement, InTransformType);
	PropagateDirtyFlags(InTransformElement, ERigTransformType::IsInitial(InTransformType), bAffectChildren);

	const ERigTransformType::Type OpposedType = SwapLocalAndGlobal(InTransformType);
	InTransformElement->Pose.Set(InTransformType, InTransform);
	InTransformElement->Pose.MarkDirty(OpposedType);
	IncrementPoseVersion(InTransformElement->Index);

	if(FRigControlElement* ControlElement = Cast<FRigControlElement>(InTransformElement))
	{
		ControlElement->Shape.MarkDirty(MakeGlobal(InTransformType));

		if(bUsePreferredEulerAngles && ERigTransformType::IsLocal(InTransformType))
		{
			const bool bInitial = ERigTransformType::IsInitial(InTransformType);
			ControlElement->PreferredEulerAngles.SetRotator(InTransform.Rotator(), bInitial, true);
		}
	}

	EnsureCacheValidity();
	
#if WITH_EDITOR
	if(bSetupUndo || IsTracingChanges())
	{
		PushTransformToStack(
			InTransformElement->GetKey(),
			ERigTransformStackEntryType::TransformPose,
			InTransformType,
			PreviousTransform,
			InTransformElement->Pose.Get(InTransformType),
			bAffectChildren,
			bSetupUndo);
	}

	if (!bPropagatingChange)
	{
		TGuardValue<bool> bPropagatingChangeGuardValue(bPropagatingChange, true);

		ForEachListeningHierarchy([this, InTransformElement, InTransform, InTransformType, bAffectChildren, bForce](const FRigHierarchyListener& Listener)
		{
			if(!bForcePropagation && !Listener.ShouldReactToChange(InTransformType))
			{
				return;
			}

			if (URigHierarchy* ListeningHierarchy = Listener.Hierarchy.Get())
			{			
				if(FRigTransformElement* ListeningElement = Cast<FRigTransformElement>(ListeningHierarchy->Find(InTransformElement->GetKey())))
				{
					// bSetupUndo = false such that all listening hierarchies performs undo at the same time the root hierachy undos
					ListeningHierarchy->SetTransform(ListeningElement, InTransform, InTransformType, bAffectChildren, false, bForce);
				}
			}
		});
	}

	if (bPrintPythonCommands)
	{
		FString BlueprintName;
		if (UBlueprint* Blueprint = GetTypedOuter<UBlueprint>())
		{
			BlueprintName = Blueprint->GetFName().ToString();
		}
		else if (UControlRig* Rig = Cast<UControlRig>(GetOuter()))
		{
			if (UBlueprint* BlueprintCR = Cast<UBlueprint>(Rig->GetClass()->ClassGeneratedBy))
			{
				BlueprintName = BlueprintCR->GetFName().ToString();
			}
		}
		if (!BlueprintName.IsEmpty())
		{
			FString MethodName;
			switch (InTransformType)
			{
				case ERigTransformType::InitialLocal: 
				case ERigTransformType::CurrentLocal:
				{
					MethodName = TEXT("set_local_transform");
					break;
				}
				case ERigTransformType::InitialGlobal: 
				case ERigTransformType::CurrentGlobal:
				{
					MethodName = TEXT("set_global_transform");
					break;
				}
			}

			RigVMPythonUtils::Print(BlueprintName,
				FString::Printf(TEXT("hierarchy.%s(%s, %s, %s, %s)"),
				*MethodName,
				*InTransformElement->GetKey().ToPythonString(),
				*RigVMPythonUtils::TransformToPythonString(InTransform),
				(InTransformType == ERigTransformType::InitialGlobal || InTransformType == ERigTransformType::InitialLocal) ? TEXT("True") : TEXT("False"),
				(bAffectChildren) ? TEXT("True") : TEXT("False")));
		}
	}
#endif
}

FTransform URigHierarchy::GetControlOffsetTransform(FRigControlElement* InControlElement,
	const ERigTransformType::Type InTransformType) const
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));
	if(InControlElement == nullptr)
	{
		return FTransform::Identity;
	}

#if WITH_EDITOR

	// lock execute context scope
	{
		FScopeLock Lock(&ExecuteContextLock);
		if(bRecordTransformsAtRuntime && ExecuteContext)
		{
			ReadTransformsAtRuntime.Emplace(
				ExecuteContext->GetPublicData<>().GetInstructionIndex(),
				ExecuteContext->GetSlice().GetIndex(),
				InControlElement->GetIndex(),
				InTransformType
			);
		}
	}
	
	TGuardValue<bool> RecordTransformsPerInstructionGuard(bRecordTransformsAtRuntime, false);
	
#endif

	if(InControlElement->Offset.IsDirty(InTransformType))
	{
		const ERigTransformType::Type OpposedType = SwapLocalAndGlobal(InTransformType);
		const ERigTransformType::Type GlobalType = MakeGlobal(InTransformType);
		ensure(!InControlElement->Offset.IsDirty(OpposedType));

		if(IsLocal(InTransformType))
		{
			const FTransform& GlobalTransform = InControlElement->Offset.Get(GlobalType);
			const FTransform LocalTransform = InverseSolveParentConstraints(
				GlobalTransform, 
				InControlElement->ParentConstraints, GlobalType, FTransform::Identity);
			InControlElement->Offset.Set(InTransformType, LocalTransform);

			if(bEnableCacheValidityCheck)
			{
				const FTransform ComputedTransform = SolveParentConstraints(
					InControlElement->ParentConstraints, MakeGlobal(InTransformType),
					LocalTransform, true,
					FTransform::Identity, false);

				const TArray<FString>& TransformTypeStrings = GetTransformTypeStrings();

				checkf(FRigComputedTransform::Equals(GlobalTransform, ComputedTransform),
					TEXT("Element '%s' Offset %s Cached vs Computed doesn't match. ('%s' <-> '%s')"),
					*InControlElement->GetName(),
					*TransformTypeStrings[(int32)InTransformType],
					*GlobalTransform.ToString(), *ComputedTransform.ToString());
			}
		}
		else
		{
			const FTransform& LocalTransform = InControlElement->Offset.Get(OpposedType); 
			const FTransform GlobalTransform = SolveParentConstraints(
				InControlElement->ParentConstraints, InTransformType,
				LocalTransform, true,
				FTransform::Identity, false);
			InControlElement->Offset.Set(InTransformType, GlobalTransform);

			if(bEnableCacheValidityCheck)
			{
				const FTransform ComputedTransform = InverseSolveParentConstraints(
					GlobalTransform, 
					InControlElement->ParentConstraints, GlobalType, FTransform::Identity);

				const TArray<FString>& TransformTypeStrings = GetTransformTypeStrings();

				checkf(FRigComputedTransform::Equals(LocalTransform, ComputedTransform),
					TEXT("Element '%s' Offset %s Cached vs Computed doesn't match. ('%s' <-> '%s')"),
					*InControlElement->GetName(),
					*TransformTypeStrings[(int32)InTransformType],
					*LocalTransform.ToString(), *ComputedTransform.ToString());
			}
		}

		EnsureCacheValidity();
	}
	return InControlElement->Offset.Get(InTransformType);
}

void URigHierarchy::SetControlOffsetTransform(FRigControlElement* InControlElement, const FTransform& InTransform,
                                              const ERigTransformType::Type InTransformType, bool bAffectChildren, bool bSetupUndo, bool bForce, bool bPrintPythonCommands)
{
	if(InControlElement == nullptr)
	{
		return;
	}

#if WITH_EDITOR

	// lock execute context scope
	{
		FScopeLock Lock(&ExecuteContextLock);
		if(bRecordTransformsAtRuntime && ExecuteContext)
		{
			WrittenTransformsAtRuntime.Emplace(
				ExecuteContext->GetPublicData<>().GetInstructionIndex(),
				ExecuteContext->GetSlice().GetIndex(),
				InControlElement->GetIndex(),
				InTransformType
			);
		}
	}
	
	TGuardValue<bool> RecordTransformsPerInstructionGuard(bRecordTransformsAtRuntime, false);
	
#endif

	if(!InControlElement->Offset.IsDirty(InTransformType))
	{
		const FTransform PreviousTransform = InControlElement->Offset.Get(InTransformType);
		if(!bForce && FRigComputedTransform::Equals(PreviousTransform, InTransform))
		{
			return;
		}
	}
	
	const FTransform PreviousTransform = GetControlOffsetTransform(InControlElement, InTransformType);
	PropagateDirtyFlags(InControlElement, ERigTransformType::IsInitial(InTransformType), bAffectChildren);

	GetTransform(InControlElement, MakeLocal(InTransformType));
	InControlElement->Pose.MarkDirty(MakeGlobal(InTransformType));

	const ERigTransformType::Type OpposedType = SwapLocalAndGlobal(InTransformType);
	InControlElement->Offset.Set(InTransformType, InTransform);
	InControlElement->Offset.MarkDirty(OpposedType);
	InControlElement->Shape.MarkDirty(MakeGlobal(InTransformType));

	EnsureCacheValidity();

	if (ERigTransformType::IsInitial(InTransformType))
	{
		// control's offset transform is considered a special type of transform
		// whenever its initial value is changed, we want to make sure the current is kept in sync
		// such that the viewport can reflect this change
		SetControlOffsetTransform(InControlElement, InTransform, ERigTransformType::MakeCurrent(InTransformType), bAffectChildren, false, bForce);
	}
	

#if WITH_EDITOR
	if(bSetupUndo || IsTracingChanges())
	{
		PushTransformToStack(
            InControlElement->GetKey(),
            ERigTransformStackEntryType::ControlOffset,
            InTransformType,
            PreviousTransform,
            InControlElement->Offset.Get(InTransformType),
            bAffectChildren,
            bSetupUndo);
	}

	if (!bPropagatingChange)
	{
		TGuardValue<bool> bPropagatingChangeGuardValue(bPropagatingChange, true);
			
		ForEachListeningHierarchy([this, InControlElement, InTransform, InTransformType, bAffectChildren, bForce](const FRigHierarchyListener& Listener)
		{
			if (URigHierarchy* ListeningHierarchy = Listener.Hierarchy.Get())
			{	
				if(FRigControlElement* ListeningElement = Cast<FRigControlElement>(ListeningHierarchy->Find(InControlElement->GetKey())))
				{
					// bSetupUndo = false such that all listening hierarchies performs undo at the same time the root hierachy undos
					ListeningHierarchy->SetControlOffsetTransform(ListeningElement, InTransform, InTransformType, bAffectChildren, false, bForce);
				}
			}
		});
	}

	if (bPrintPythonCommands)
	{
		FString BlueprintName;
		if (UBlueprint* Blueprint = GetTypedOuter<UBlueprint>())
		{
			BlueprintName = Blueprint->GetFName().ToString();
		}
		else if (UControlRig* Rig = Cast<UControlRig>(GetOuter()))
		{
			if (UBlueprint* BlueprintCR = Cast<UBlueprint>(Rig->GetClass()->ClassGeneratedBy))
			{
				BlueprintName = BlueprintCR->GetFName().ToString();
			}
		}
		if (!BlueprintName.IsEmpty())
		{
			RigVMPythonUtils::Print(BlueprintName,
				FString::Printf(TEXT("hierarchy.set_control_offset_transform(%s, %s, %s, %s)"),
				*InControlElement->GetKey().ToPythonString(),
				*RigVMPythonUtils::TransformToPythonString(InTransform),
				(ERigTransformType::IsInitial(InTransformType)) ? TEXT("True") : TEXT("False"),
				(bAffectChildren) ? TEXT("True") : TEXT("False")));
		}
	}
#endif
}

FTransform URigHierarchy::GetControlShapeTransform(FRigControlElement* InControlElement,
	const ERigTransformType::Type InTransformType) const
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));
	if(InControlElement == nullptr)
	{
		return FTransform::Identity;
	}
	
	if(InControlElement->Shape.IsDirty(InTransformType))
	{
		const ERigTransformType::Type OpposedType = SwapLocalAndGlobal(InTransformType);
		const ERigTransformType::Type GlobalType = MakeGlobal(InTransformType);
		ensure(!InControlElement->Shape.IsDirty(OpposedType));

		const FTransform ParentTransform = GetTransform(InControlElement, GlobalType);
		if(IsLocal(InTransformType))
		{
			FTransform LocalTransform = InControlElement->Shape.Get(OpposedType).GetRelativeTransform(ParentTransform);
			LocalTransform.NormalizeRotation();
			InControlElement->Shape.Set(InTransformType, LocalTransform);
		}
		else
		{
			FTransform GlobalTransform = InControlElement->Shape.Get(OpposedType) * ParentTransform;
			GlobalTransform.NormalizeRotation();
			InControlElement->Shape.Set(InTransformType, GlobalTransform);
		}

		EnsureCacheValidity();
	}
	return InControlElement->Shape.Get(InTransformType);
}

void URigHierarchy::SetControlShapeTransform(FRigControlElement* InControlElement, const FTransform& InTransform,
	const ERigTransformType::Type InTransformType, bool bSetupUndo, bool bForce, bool bPrintPythonCommands)
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));
	if(InControlElement == nullptr)
	{
		return;
	}

	if(!InControlElement->Shape.IsDirty(InTransformType))
	{
		const FTransform PreviousTransform = InControlElement->Shape.Get(InTransformType);
		if(!bForce && FRigComputedTransform::Equals(PreviousTransform, InTransform))
		{
			return;
		}
	}

	const FTransform PreviousTransform = GetControlShapeTransform(InControlElement, InTransformType);
	const ERigTransformType::Type OpposedType = SwapLocalAndGlobal(InTransformType);
	InControlElement->Shape.Set(InTransformType, InTransform);
	InControlElement->Shape.MarkDirty(OpposedType);

	if (IsInitial(InTransformType))
	{
		// control's shape transform, similar to offset transform, is considered a special type of transform
		// whenever its initial value is changed, we want to make sure the current is kept in sync
		// such that the viewport can reflect this change
		SetControlShapeTransform(InControlElement, InTransform, ERigTransformType::MakeCurrent(InTransformType), false, bForce);
	}
	
	EnsureCacheValidity();
	
#if WITH_EDITOR
	if(bSetupUndo || IsTracingChanges())
	{
		PushTransformToStack(
            InControlElement->GetKey(),
            ERigTransformStackEntryType::ControlShape,
            InTransformType,
            PreviousTransform,
            InControlElement->Shape.Get(InTransformType),
            false,
            bSetupUndo);
	}
#endif

	if(IsLocal(InTransformType))
	{
		Notify(ERigHierarchyNotification::ControlShapeTransformChanged, InControlElement);
	}

#if WITH_EDITOR
	if (!bPropagatingChange)
	{
		TGuardValue<bool> bPropagatingChangeGuardValue(bPropagatingChange, true);
			
		ForEachListeningHierarchy([this, InControlElement, InTransform, InTransformType, bForce](const FRigHierarchyListener& Listener)
		{
			if (URigHierarchy* ListeningHierarchy = Listener.Hierarchy.Get())
			{	
				if(FRigControlElement* ListeningElement = Cast<FRigControlElement>(ListeningHierarchy->Find(InControlElement->GetKey())))
				{
					// bSetupUndo = false such that all listening hierarchies performs undo at the same time the root hierachy undos
					ListeningHierarchy->SetControlShapeTransform(ListeningElement, InTransform, InTransformType, false, bForce);
				}
			}
		});
	}

	if (bPrintPythonCommands)
	{
		FString BlueprintName;
		if (UBlueprint* Blueprint = GetTypedOuter<UBlueprint>())
		{
			BlueprintName = Blueprint->GetFName().ToString();
		}
		else if (UControlRig* Rig = Cast<UControlRig>(GetOuter()))
		{
			if (UBlueprint* BlueprintCR = Cast<UBlueprint>(Rig->GetClass()->ClassGeneratedBy))
			{
				BlueprintName = BlueprintCR->GetFName().ToString();
			}
		}
		if (!BlueprintName.IsEmpty())
		{
			RigVMPythonUtils::Print(BlueprintName,
				FString::Printf(TEXT("hierarchy.set_control_shape_transform(%s, %s, %s)"),
				*InControlElement->GetKey().ToPythonString(),
				*RigVMPythonUtils::TransformToPythonString(InTransform),
				ERigTransformType::IsInitial(InTransformType) ? TEXT("True") : TEXT("False")));
		}
	
	}
#endif
}

void URigHierarchy::SetControlSettings(FRigControlElement* InControlElement, FRigControlSettings InSettings, bool bSetupUndo, bool bForce, bool bPrintPythonCommands)
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));
	if(InControlElement == nullptr)
	{
		return;
	}

	const FRigControlSettings PreviousSettings = InControlElement->Settings;
	if(!bForce && PreviousSettings == InSettings)
	{
		return;
	}

	if(bSetupUndo && !HasAnyFlags(RF_Transient))
	{
		Modify();
	}

	InControlElement->Settings = InSettings;
	Notify(ERigHierarchyNotification::ControlSettingChanged, InControlElement);
	
#if WITH_EDITOR
	if (!bPropagatingChange)
	{
		TGuardValue<bool> bPropagatingChangeGuardValue(bPropagatingChange, true);
			
		ForEachListeningHierarchy([this, InControlElement, InSettings, bForce](const FRigHierarchyListener& Listener)
		{
			if (URigHierarchy* ListeningHierarchy = Listener.Hierarchy.Get())
			{	
				if(FRigControlElement* ListeningElement = Cast<FRigControlElement>(ListeningHierarchy->Find(InControlElement->GetKey())))
				{
					// bSetupUndo = false such that all listening hierarchies performs undo at the same time the root hierachy undos
					ListeningHierarchy->SetControlSettings(ListeningElement, InSettings, false, bForce);
				}
			}
		});
	}

	if (bPrintPythonCommands)
	{
		FString BlueprintName;
		if (UBlueprint* Blueprint = GetTypedOuter<UBlueprint>())
		{
			BlueprintName = Blueprint->GetFName().ToString();
		}
		else if (UControlRig* Rig = Cast<UControlRig>(GetOuter()))
		{
			if (UBlueprint* BlueprintCR = Cast<UBlueprint>(Rig->GetClass()->ClassGeneratedBy))
			{
				BlueprintName = BlueprintCR->GetFName().ToString();
			}
		}
		if (!BlueprintName.IsEmpty())
		{
			FString ControlNamePythonized = RigVMPythonUtils::PythonizeName(InControlElement->GetName());
			FString SettingsName = FString::Printf(TEXT("control_settings_%s"),
				*ControlNamePythonized);
			TArray<FString> Commands = ControlSettingsToPythonCommands(InControlElement->Settings, SettingsName);

			for (const FString& Command : Commands)
			{
				RigVMPythonUtils::Print(BlueprintName, Command);
			}
			
			RigVMPythonUtils::Print(BlueprintName,
				FString::Printf(TEXT("hierarchy.set_control_settings(%s, %s)"),
				*InControlElement->GetKey().ToPythonString(),
				*SettingsName));
		}
	}
#endif
}

FTransform URigHierarchy::GetParentTransform(FRigBaseElement* InElement, const ERigTransformType::Type InTransformType) const
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));
	if(FRigSingleParentElement* SingleParentElement = Cast<FRigSingleParentElement>(InElement))
	{
		return GetTransform(SingleParentElement->ParentElement, InTransformType);
	}
	else if(FRigMultiParentElement* MultiParentElement = Cast<FRigMultiParentElement>(InElement))
	{
		const FTransform OutputTransform = SolveParentConstraints(
			MultiParentElement->ParentConstraints,
			InTransformType,
			FTransform::Identity,
			false,
			FTransform::Identity,
			false
		);
		EnsureCacheValidity();
		return OutputTransform;
	}
	return FTransform::Identity;
}

FRigControlValue URigHierarchy::GetControlValue(FRigControlElement* InControlElement, ERigControlValueType InValueType, bool bUsePreferredAngles) const
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));
	using namespace ERigTransformType;

	FRigControlValue Value;

	if(InControlElement != nullptr)
	{
		auto GetValueFromPreferredEulerAngles = [this, InControlElement, &Value, InValueType, bUsePreferredAngles]() -> bool
		{
			if (!bUsePreferredAngles)
			{
				return false;
			}
			
			const bool bInitial = InValueType == ERigControlValueType::Initial;
			switch(InControlElement->Settings.ControlType)
			{
				case ERigControlType::Rotator:
				{
					Value = MakeControlValueFromRotator(InControlElement->PreferredEulerAngles.GetRotator(bInitial)); 
					return true;
				}
				case ERigControlType::EulerTransform:
				{
					FEulerTransform EulerTransform(GetTransform(InControlElement, CurrentLocal));
					EulerTransform.Rotation = InControlElement->PreferredEulerAngles.GetRotator(bInitial);
					Value = MakeControlValueFromEulerTransform(EulerTransform); 
					return true;
				}
				default:
				{
					break;
				}
			}
			return false;
		};
		
		switch(InValueType)
		{
			case ERigControlValueType::Current:
			{
				if(GetValueFromPreferredEulerAngles())
				{
					break;
				}
					
				Value.SetFromTransform(
                    GetTransform(InControlElement, CurrentLocal),
                    InControlElement->Settings.ControlType,
                    InControlElement->Settings.PrimaryAxis
                );
				break;
			}
			case ERigControlValueType::Initial:
			{
				if(GetValueFromPreferredEulerAngles())
				{
					break;
				}

				Value.SetFromTransform(
                    GetTransform(InControlElement, InitialLocal),
                    InControlElement->Settings.ControlType,
                    InControlElement->Settings.PrimaryAxis
                );
				break;
			}
			case ERigControlValueType::Minimum:
			{
				return InControlElement->Settings.MinimumValue;
			}
			case ERigControlValueType::Maximum:
			{
				return InControlElement->Settings.MaximumValue;
			}
		}
	}
	return Value;
}

void URigHierarchy::SetPreferredEulerAnglesFromValue(FRigControlElement* InControlElement, const FRigControlValue& InValue, const ERigControlValueType& InValueType, const bool bFixEulerFlips)
{
	const bool bInitial = InValueType == ERigControlValueType::Initial;
	switch(InControlElement->Settings.ControlType)
	{
	case ERigControlType::Rotator:
		{
			InControlElement->PreferredEulerAngles.SetRotator(GetRotatorFromControlValue(InValue), bInitial, bFixEulerFlips);
			break;
		}
	case ERigControlType::EulerTransform:
		{
			FEulerTransform EulerTransform = GetEulerTransformFromControlValue(InValue);
			FQuat Quat = EulerTransform.GetRotation();
			const FVector Angle = GetControlAnglesFromQuat(InControlElement, Quat);
			InControlElement->PreferredEulerAngles.SetAngles(Angle, bInitial, InControlElement->PreferredEulerAngles.RotationOrder, bFixEulerFlips);
			break;
		}
	case ERigControlType::Transform:
		{
			FTransform Transform = GetTransformFromControlValue(InValue);
			FQuat Quat = Transform.GetRotation();
			const FVector Angle = GetControlAnglesFromQuat(InControlElement, Quat);
			InControlElement->PreferredEulerAngles.SetAngles(Angle, bInitial, InControlElement->PreferredEulerAngles.RotationOrder, bFixEulerFlips);
			break;
		}
	case ERigControlType::TransformNoScale:
		{
			FTransform Transform = GetTransformNoScaleFromControlValue(InValue);
			FQuat Quat = Transform.GetRotation();
			const FVector Angle = GetControlAnglesFromQuat(InControlElement, Quat);
			InControlElement->PreferredEulerAngles.SetAngles(Angle, bInitial, InControlElement->PreferredEulerAngles.RotationOrder, bFixEulerFlips);
			break;
		}
	default:
		{
			break;
		}
	}
};

void URigHierarchy::SetControlValue(FRigControlElement* InControlElement, const FRigControlValue& InValue, ERigControlValueType InValueType, bool bSetupUndo, bool bForce, bool bPrintPythonCommands, bool bFixEulerFlips)
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));
	
	using namespace ERigTransformType;

	if(InControlElement != nullptr)
	{
		switch(InValueType)
		{
			case ERigControlValueType::Current:
			{
				FRigControlValue Value = InValue;
				InControlElement->Settings.ApplyLimits(Value);

				TGuardValue<bool> DontSetPreferredEulerAngle(bUsePreferredEulerAngles, false);
				SetTransform(
					InControlElement,
					Value.GetAsTransform(
						InControlElement->Settings.ControlType,
						InControlElement->Settings.PrimaryAxis
					),
					CurrentLocal,
					true,
					bSetupUndo,
					bForce,
					bPrintPythonCommands
				);
				if (bFixEulerFlips)
				{
					SetPreferredEulerAnglesFromValue(InControlElement, Value, InValueType, bFixEulerFlips);
				}
				break;
			}
			case ERigControlValueType::Initial:
			{
				FRigControlValue Value = InValue;
				InControlElement->Settings.ApplyLimits(Value);

				TGuardValue<bool> DontSetPreferredEulerAngle(bUsePreferredEulerAngles, false);
				SetTransform(
					InControlElement,
					Value.GetAsTransform(
						InControlElement->Settings.ControlType,
						InControlElement->Settings.PrimaryAxis
					),
					InitialLocal,
					true,
					bSetupUndo,
					bForce,
					bPrintPythonCommands
				);

				if (bFixEulerFlips)
				{
					SetPreferredEulerAnglesFromValue(InControlElement, Value, InValueType, bFixEulerFlips);
				}
				break;
			}
			case ERigControlValueType::Minimum:
			case ERigControlValueType::Maximum:
			{
				if(bSetupUndo)
				{
					Modify();
				}

				if(InValueType == ERigControlValueType::Minimum)
				{
					InControlElement->Settings.MinimumValue = InValue;
				}
				else
				{
					InControlElement->Settings.MaximumValue = InValue;
				}
				
				Notify(ERigHierarchyNotification::ControlSettingChanged, InControlElement);

#if WITH_EDITOR
				if (!bPropagatingChange)
				{
					TGuardValue<bool> bPropagatingChangeGuardValue(bPropagatingChange, true);
			
					ForEachListeningHierarchy([this, InControlElement, InValue, InValueType, bForce](const FRigHierarchyListener& Listener)
					{
						if (URigHierarchy* ListeningHierarchy = Listener.Hierarchy.Get())
						{
							if(FRigControlElement* ListeningElement = Cast<FRigControlElement>(ListeningHierarchy->Find(InControlElement->GetKey())))
							{
								ListeningHierarchy->SetControlValue(ListeningElement, InValue, InValueType, false, bForce);
							}
						}
					});
				}

				if (bPrintPythonCommands)
				{
					FString BlueprintName;
					if (UBlueprint* Blueprint = GetTypedOuter<UBlueprint>())
					{
						BlueprintName = Blueprint->GetFName().ToString();
					}
					else if (UControlRig* Rig = Cast<UControlRig>(GetOuter()))
					{
						if (UBlueprint* BlueprintCR = Cast<UBlueprint>(Rig->GetClass()->ClassGeneratedBy))
						{
							BlueprintName = BlueprintCR->GetFName().ToString();
						}
					}
					if (!BlueprintName.IsEmpty())
					{
						RigVMPythonUtils::Print(BlueprintName,
							FString::Printf(TEXT("hierarchy.set_control_value(%s, %s, %s)"),
							*InControlElement->GetKey().ToPythonString(),
							*InValue.ToPythonString(InControlElement->Settings.ControlType),
							*RigVMPythonUtils::EnumValueToPythonString<ERigControlValueType>((int64)InValueType)));
					}
				}
#endif
				break;
			}
		}	
	}
}

void URigHierarchy::SetControlVisibility(FRigControlElement* InControlElement, bool bVisibility)
{
	if(InControlElement == nullptr)
	{
		return;
	}

	if(InControlElement->Settings.SetVisible(bVisibility))
	{
		Notify(ERigHierarchyNotification::ControlVisibilityChanged, InControlElement);
	}

#if WITH_EDITOR
	if (!bPropagatingChange)
	{
		TGuardValue<bool> bPropagatingChangeGuardValue(bPropagatingChange, true);
			
		ForEachListeningHierarchy([this, InControlElement, bVisibility](const FRigHierarchyListener& Listener)
		{
			if (URigHierarchy* ListeningHierarchy = Listener.Hierarchy.Get())
			{
				if(FRigControlElement* ListeningElement = Cast<FRigControlElement>(ListeningHierarchy->Find(InControlElement->GetKey())))
				{
					ListeningHierarchy->SetControlVisibility(ListeningElement, bVisibility);
				}
			}
		});
	}
#endif
}

void URigHierarchy::SetConnectorSettings(FRigConnectorElement* InConnectorElement, FRigConnectorSettings InSettings,
	bool bSetupUndo, bool bForce, bool bPrintPythonCommands)
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));
	if(InConnectorElement == nullptr)
	{
		return;
	}

	const FRigConnectorSettings PreviousSettings = InConnectorElement->Settings;
	if(!bForce && PreviousSettings == InSettings)
	{
		return;
	}

	if (InSettings.Type == EConnectorType::Primary)
	{
		if (InSettings.bOptional)
		{
			return;
		}
	}

	if(bSetupUndo && !HasAnyFlags(RF_Transient))
	{
		Modify();
	}

	InConnectorElement->Settings = InSettings;
	Notify(ERigHierarchyNotification::ConnectorSettingChanged, InConnectorElement);
	
#if WITH_EDITOR
	if (!bPropagatingChange)
	{
		TGuardValue<bool> bPropagatingChangeGuardValue(bPropagatingChange, true);
			
		ForEachListeningHierarchy([this, InConnectorElement, InSettings, bForce](const FRigHierarchyListener& Listener)
		{
			if (URigHierarchy* ListeningHierarchy = Listener.Hierarchy.Get())
			{	
				if(FRigConnectorElement* ListeningElement = Cast<FRigConnectorElement>(ListeningHierarchy->Find(InConnectorElement->GetKey())))
				{
					// bSetupUndo = false such that all listening hierarchies performs undo at the same time the root hierachy undos
					ListeningHierarchy->SetConnectorSettings(ListeningElement, InSettings, false, bForce);
				}
			}
		});
	}

	if (bPrintPythonCommands)
	{
		FString BlueprintName;
		if (UBlueprint* Blueprint = GetTypedOuter<UBlueprint>())
		{
			BlueprintName = Blueprint->GetFName().ToString();
		}
		else if (UControlRig* Rig = Cast<UControlRig>(GetOuter()))
		{
			if (UBlueprint* BlueprintCR = Cast<UBlueprint>(Rig->GetClass()->ClassGeneratedBy))
			{
				BlueprintName = BlueprintCR->GetFName().ToString();
			}
		}
		if (!BlueprintName.IsEmpty())
		{
			FString ControlNamePythonized = RigVMPythonUtils::PythonizeName(InConnectorElement->GetName());
			FString SettingsName = FString::Printf(TEXT("connector_settings_%s"),
				*ControlNamePythonized);
			TArray<FString> Commands = ConnectorSettingsToPythonCommands(InConnectorElement->Settings, SettingsName);

			for (const FString& Command : Commands)
			{
				RigVMPythonUtils::Print(BlueprintName, Command);
			}
			
			RigVMPythonUtils::Print(BlueprintName,
				FString::Printf(TEXT("hierarchy.set_connector_settings(%s, %s)"),
				*InConnectorElement->GetKey().ToPythonString(),
				*SettingsName));
		}
	}
#endif
}


float URigHierarchy::GetCurveValue(FRigCurveElement* InCurveElement) const
{
	if(InCurveElement == nullptr)
	{
		return 0.f;
	}
	return InCurveElement->bIsValueSet ? InCurveElement->Value : 0.f;
}


bool URigHierarchy::IsCurveValueSet(FRigCurveElement* InCurveElement) const
{
	return InCurveElement && InCurveElement->bIsValueSet;
}


void URigHierarchy::SetCurveValue(FRigCurveElement* InCurveElement, float InValue, bool bSetupUndo, bool bForce)
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));
	if(InCurveElement == nullptr)
	{
		return;
	}

	const bool bPreviousIsValueSet = InCurveElement->bIsValueSet; 
	const float PreviousValue = InCurveElement->Value;
	if(!bForce && InCurveElement->bIsValueSet && FMath::IsNearlyZero(PreviousValue - InValue))
	{
		return;
	}

	InCurveElement->bIsValueSet = true;
	InCurveElement->Value = InValue;

#if WITH_EDITOR
	if(bSetupUndo || IsTracingChanges())
	{
		PushCurveToStack(InCurveElement->GetKey(), PreviousValue, InCurveElement->Value, bPreviousIsValueSet, true, bSetupUndo);
	}

	if (!bPropagatingChange)
	{
		TGuardValue<bool> bPropagatingChangeGuardValue(bPropagatingChange, true);
			
		ForEachListeningHierarchy([this, InCurveElement, InValue, bForce](const FRigHierarchyListener& Listener)
		{
			if(!Listener.Hierarchy.IsValid())
			{
				return;
			}

			if (URigHierarchy* ListeningHierarchy = Listener.Hierarchy.Get())
			{
				if(FRigCurveElement* ListeningElement = Cast<FRigCurveElement>(ListeningHierarchy->Find(InCurveElement->GetKey())))
				{
					// bSetupUndo = false such that all listening hierarchies performs undo at the same time the root hierarchy undoes
					ListeningHierarchy->SetCurveValue(ListeningElement, InValue, false, bForce);
				}
			}
		});
	}
#endif
}


void URigHierarchy::UnsetCurveValue(FRigCurveElement* InCurveElement, bool bSetupUndo, bool bForce)
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));
	if(InCurveElement == nullptr)
	{
		return;
	}

	const bool bPreviousIsValueSet = InCurveElement->bIsValueSet; 
	if(!bForce && !InCurveElement->bIsValueSet)
	{
		return;
	}

	InCurveElement->bIsValueSet = false;

#if WITH_EDITOR
	if(bSetupUndo || IsTracingChanges())
	{
		PushCurveToStack(InCurveElement->GetKey(), InCurveElement->Value, InCurveElement->Value, bPreviousIsValueSet, false, bSetupUndo);
	}

	if (!bPropagatingChange)
	{
		TGuardValue<bool> bPropagatingChangeGuardValue(bPropagatingChange, true);
			
		ForEachListeningHierarchy([this, InCurveElement, bForce](const FRigHierarchyListener& Listener)
		{
			if(!Listener.Hierarchy.IsValid())
			{
				return;
			}

			if (URigHierarchy* ListeningHierarchy = Listener.Hierarchy.Get())
			{
				if(FRigCurveElement* ListeningElement = Cast<FRigCurveElement>(ListeningHierarchy->Find(InCurveElement->GetKey())))
				{
					// bSetupUndo = false such that all listening hierarchies performs undo at the same time the root hierarchy undoes
					ListeningHierarchy->UnsetCurveValue(ListeningElement, false, bForce);
				}
			}
		});
	}
#endif
}


FName URigHierarchy::GetPreviousName(const FRigElementKey& InKey) const
{
	if(const FRigElementKey* OldKeyPtr = PreviousNameMap.Find(InKey))
	{
		return OldKeyPtr->Name;
	}
	return NAME_None;
}

FRigElementKey URigHierarchy::GetPreviousParent(const FRigElementKey& InKey) const
{
	if(const FRigElementKey* OldParentPtr = PreviousParentMap.Find(InKey))
	{
		return *OldParentPtr;
	}
	return FRigElementKey();
}

bool URigHierarchy::IsParentedTo(FRigBaseElement* InChild, FRigBaseElement* InParent, const TElementDependencyMap& InDependencyMap) const
{
	ElementDependencyVisited.Reset();
	if(!InDependencyMap.IsEmpty())
	{
		ElementDependencyVisited.SetNumZeroed(Elements.Num());
	}
	return IsDependentOn(InChild, InParent, InDependencyMap, true);
}

bool URigHierarchy::IsDependentOn(FRigBaseElement* InDependent, FRigBaseElement* InDependency, const TElementDependencyMap& InDependencyMap, bool bIsOnActualTopology) const
{
	if((InDependent == nullptr) || (InDependency == nullptr))
	{
		return false;
	}

	if(InDependent == InDependency)
	{
		return true;
	}

	const int32 DependentElementIndex = InDependent->GetIndex();
	const int32 DependencyElementIndex = InDependency->GetIndex();
	const TTuple<int32,int32> CacheKey(DependentElementIndex, DependencyElementIndex);

		if(!ElementDependencyCache.IsValid(GetTopologyVersion()))
		{
			ElementDependencyCache.Set(TMap<TTuple<int32, int32>, bool>(), GetTopologyVersion());
		}

	// we'll only update the caches if we are following edges on the actual topology
	if(const bool* bCachedResult = ElementDependencyCache.Get().Find(CacheKey))
	{
		return *bCachedResult;
	}

	// check if the reverse dependency check has been stored before - if the dependency is dependent
	// then we don't need to recurse any further.
	const TTuple<int32,int32> ReverseCacheKey(DependencyElementIndex, DependentElementIndex);
	if(const bool* bReverseCachedResult = ElementDependencyCache.Get().Find(ReverseCacheKey))
	{
		if(*bReverseCachedResult)
		{
			return false;
		}
	}

	if(!ElementDependencyVisited.IsEmpty())
	{
		// when running this with a provided dependency map
		// we may run into a cycle / infinite recursion. this array
		// keeps track of all elements being visited before.
		if(!ElementDependencyVisited.IsValidIndex(DependentElementIndex))
		{
			return false;
		}
		if (ElementDependencyVisited[DependentElementIndex])
		{
			return false;
		}
		ElementDependencyVisited[DependentElementIndex] = true;
	}

	// collect all possible parents of the dependent
	if(const FRigSingleParentElement* SingleParentElement = Cast<FRigSingleParentElement>(InDependent))
	{
		if(IsDependentOn(SingleParentElement->ParentElement, InDependency, InDependencyMap, true))
		{
			// we'll only update the caches if we are following edges on the actual topology
			if(bIsOnActualTopology)
			{
				ElementDependencyCache.Get().FindOrAdd(CacheKey, true);
			}
			return true;
		}
	}
	else if(const FRigMultiParentElement* MultiParentElement = Cast<FRigMultiParentElement>(InDependent))
	{
		for(const FRigElementParentConstraint& ParentConstraint : MultiParentElement->ParentConstraints)
		{
			if(IsDependentOn(ParentConstraint.ParentElement, InDependency, InDependencyMap, true))
			{
				// we'll only update the caches if we are following edges on the actual topology
				if(bIsOnActualTopology)
				{
					ElementDependencyCache.Get().FindOrAdd(CacheKey, true);
				}
				return true;
			}
		}
	}

	// check the optional dependency map
	if(const TArray<int32>* DependentIndicesPtr = InDependencyMap.Find(InDependent->GetIndex()))
	{
		const TArray<int32>& DependentIndices = *DependentIndicesPtr;
		for(const int32 DependentIndex : DependentIndices)
		{
			ensure(Elements.IsValidIndex(DependentIndex));
			if(IsDependentOn(Elements[DependentIndex], InDependency, InDependencyMap, false))
			{
				// we'll only update the caches if we are following edges on the actual topology
				if(bIsOnActualTopology)
				{
					ElementDependencyCache.Get().FindOrAdd(CacheKey, true);
				}
				return true;
			}
		}
	}

	// we'll only update the caches if we are following edges on the actual topology
	if(bIsOnActualTopology)
	{
		ElementDependencyCache.Get().FindOrAdd(CacheKey, false);
	}
	return false;
}

int32 URigHierarchy::GetLocalIndex(const FRigBaseElement* InElement) const
{
	if(InElement == nullptr)
	{
		return INDEX_NONE;
	}
	
	if(const FRigBaseElement* ParentElement = GetFirstParent(InElement))
	{
		TConstArrayView<FRigBaseElement*> Children = GetChildren(ParentElement);
		return Children.Find(const_cast<FRigBaseElement*>(InElement));
	}

	return GetRootElements().Find(const_cast<FRigBaseElement*>(InElement));
}

bool URigHierarchy::IsTracingChanges() const
{
#if WITH_EDITOR
	return (CVarControlRigHierarchyTraceAlways->GetInt() != 0) || (TraceFramesLeft > 0);
#else
	return false;
#endif
}

#if WITH_EDITOR

void URigHierarchy::ResetTransformStack()
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));
	TransformUndoStack.Reset();
	TransformRedoStack.Reset();
	TransformStackIndex = TransformUndoStack.Num();

	if(IsTracingChanges())
	{
		TracePoses.Reset();
		StorePoseForTrace(TEXT("BeginOfFrame"));
	}
}

void URigHierarchy::StorePoseForTrace(const FString& InPrefix)
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));
	check(!InPrefix.IsEmpty());
	
	FName InitialKey = *FString::Printf(TEXT("%s_Initial"), *InPrefix);
	FName CurrentKey = *FString::Printf(TEXT("%s_Current"), *InPrefix);
	TracePoses.FindOrAdd(InitialKey) = GetPose(true);
	TracePoses.FindOrAdd(CurrentKey) = GetPose(false);
}

void URigHierarchy::CheckTraceFormatIfRequired()
{
	if(sRigHierarchyLastTrace != CVarControlRigHierarchyTracePrecision->GetInt())
	{
		sRigHierarchyLastTrace = CVarControlRigHierarchyTracePrecision->GetInt();
		const FString Format = FString::Printf(TEXT("%%.%df"), sRigHierarchyLastTrace);
		check(Format.Len() < 16);
		sRigHierarchyTraceFormat[Format.Len()] = '\0';
		FMemory::Memcpy(sRigHierarchyTraceFormat, *Format, Format.Len() * sizeof(TCHAR));
	}
}

template <class CharType>
struct TRigHierarchyJsonPrintPolicy
	: public TPrettyJsonPrintPolicy<CharType>
{
	static inline void WriteDouble(  FArchive* Stream, double Value )
	{
		URigHierarchy::CheckTraceFormatIfRequired();
		TJsonPrintPolicy<CharType>::WriteString(Stream, FString::Printf(sRigHierarchyTraceFormat, Value));
	}
};

void URigHierarchy::DumpTransformStackToFile(FString* OutFilePath)
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));
	if(IsTracingChanges())
	{
		StorePoseForTrace(TEXT("EndOfFrame"));
	}

	FString PathName = GetPathName();
	PathName.Split(TEXT(":"), nullptr, &PathName);
	PathName.ReplaceCharInline('.', '/');

	FString Suffix;
	if(TraceFramesLeft > 0)
	{
		Suffix = FString::Printf(TEXT("_Trace_%03d"), TraceFramesCaptured);
	}

	FString FileName = FString::Printf(TEXT("%sControlRig/%s%s.json"), *FPaths::ProjectLogDir(), *PathName, *Suffix);
	FString FullFilename = FPlatformFileManager::Get().GetPlatformFile().ConvertToAbsolutePathForExternalAppForWrite(*FileName);

	TSharedPtr<FJsonObject> JsonData = MakeShareable(new FJsonObject);
	JsonData->SetStringField(TEXT("PathName"), GetPathName());

	TSharedRef<FJsonObject> JsonTracedPoses = MakeShareable(new FJsonObject);
	for(const TPair<FName, FRigPose>& Pair : TracePoses)
	{
		TSharedRef<FJsonObject> JsonTracedPose = MakeShareable(new FJsonObject);
		if (FJsonObjectConverter::UStructToJsonObject(FRigPose::StaticStruct(), &Pair.Value, JsonTracedPose, 0, 0))
		{
			JsonTracedPoses->SetObjectField(Pair.Key.ToString(), JsonTracedPose);
		}
	}
	JsonData->SetObjectField(TEXT("TracedPoses"), JsonTracedPoses);

	TArray<TSharedPtr<FJsonValue>> JsonTransformStack;
	for (const FRigTransformStackEntry& TransformStackEntry : TransformUndoStack)
	{
		TSharedRef<FJsonObject> JsonTransformStackEntry = MakeShareable(new FJsonObject);
		if (FJsonObjectConverter::UStructToJsonObject(FRigTransformStackEntry::StaticStruct(), &TransformStackEntry, JsonTransformStackEntry, 0, 0))
		{
			JsonTransformStack.Add(MakeShareable(new FJsonValueObject(JsonTransformStackEntry)));
		}
	}
	JsonData->SetArrayField(TEXT("TransformStack"), JsonTransformStack);

	FString JsonText;
	const TSharedRef< TJsonWriter< TCHAR, TRigHierarchyJsonPrintPolicy<TCHAR> > > JsonWriter = TJsonWriterFactory< TCHAR, TRigHierarchyJsonPrintPolicy<TCHAR> >::Create(&JsonText);
	if (FJsonSerializer::Serialize(JsonData.ToSharedRef(), JsonWriter))
	{
		if ( FFileHelper::SaveStringToFile(JsonText, *FullFilename) )
		{
			UE_LOG(LogControlRig, Display, TEXT("Saved hierarchy trace to %s"), *FullFilename);

			if(OutFilePath)
			{
				*OutFilePath = FullFilename;
			}
		}
	}

	TraceFramesLeft = FMath::Max(0, TraceFramesLeft - 1);
	TraceFramesCaptured++;
}

void URigHierarchy::TraceFrames(int32 InNumFramesToTrace)
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));
	
	TraceFramesLeft = InNumFramesToTrace;
	TraceFramesCaptured = 0;
	ResetTransformStack();
}

#endif

bool URigHierarchy::IsSelected(const FRigBaseElement* InElement) const
{
	if(InElement == nullptr)
	{
		return false;
	}
	if(const URigHierarchy* HierarchyForSelection = HierarchyForSelectionPtr.Get())
	{
		return HierarchyForSelection->IsSelected(InElement->GetKey());
	}

	const bool bIsSelected = OrderedSelection.Contains(InElement->GetKey());
	ensure(bIsSelected == InElement->IsSelected());
	return bIsSelected;
}


void URigHierarchy::EnsureCachedChildrenAreCurrent() const
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));

	if(ChildElementCacheTopologyVersion != TopologyVersion)
	{
		const_cast<URigHierarchy*>(this)->UpdateCachedChildren();
	}
}
	
void URigHierarchy::UpdateCachedChildren()
{
	FScopeLock Lock(&ElementsLock);
	
	// First we tally up how many children each element has, then we allocate for the total
	// count and do the same loop again.
	TArray<int32> ChildrenCount;
	ChildrenCount.SetNumZeroed(Elements.Num());

	// Bit array that denotes elements that have parents. We'll use this to quickly iterate
	// for the second pass.
	TBitArray<> ElementHasParent(false, Elements.Num());

	for (int32 ElementIndex = 0; ElementIndex < Elements.Num(); ElementIndex++)
	{
		const FRigBaseElement* Element = Elements[ElementIndex];
		
		if(const FRigSingleParentElement* SingleParentElement = Cast<FRigSingleParentElement>(Element))
		{
			if(const FRigTransformElement* ParentElement = SingleParentElement->ParentElement)
			{
				ChildrenCount[ParentElement->Index]++;
				ElementHasParent[ElementIndex] = true;
			}
		}
		else if(const FRigMultiParentElement* MultiParentElement = Cast<FRigMultiParentElement>(Element))
		{
			for(const FRigElementParentConstraint& ParentConstraint : MultiParentElement->ParentConstraints)
			{
				if(const FRigTransformElement* ParentElement = ParentConstraint.ParentElement)
				{
					ChildrenCount[ParentElement->Index]++;
					ElementHasParent[ElementIndex] = true;
				}
			}
		}
	}

	// Tally up how many elements have children.
	const int32 NumElementsWithChildren = Algo::CountIf(ChildrenCount, [](int32 InCount) { return InCount > 0; });

	ChildElementOffsetAndCountCache.Reset(NumElementsWithChildren);

	// Tally up how many children there are in total and set the index on each of the elements as we go.
	int32 TotalChildren = 0;
	for (int32 ElementIndex = 0; ElementIndex < Elements.Num(); ElementIndex++)
	{
		if (ChildrenCount[ElementIndex])
		{
			Elements[ElementIndex]->ChildCacheIndex = ChildElementOffsetAndCountCache.Num();
			ChildElementOffsetAndCountCache.Add({TotalChildren, ChildrenCount[ElementIndex]});
			TotalChildren += ChildrenCount[ElementIndex];
		}
		else
		{
			// This element has no children, mark it as having no entry in the child table.
			Elements[ElementIndex]->ChildCacheIndex = INDEX_NONE;
		}
	}

	// Now run through all elements that are known to have parents and start filling up the children array.
	ChildElementCache.Reset();
	ChildElementCache.SetNumZeroed(TotalChildren);

	// Recycle this array to indicate where we are with each set of children, as a local offset into each
	// element's children sub-array.
	ChildrenCount.Reset();
	ChildrenCount.SetNumZeroed(Elements.Num());	

	auto SetChildElement = [this, &ChildrenCount](
		const FRigTransformElement* InParentElement,
		FRigBaseElement* InChildElement)
	{
		const int32 ParentElementIndex = InParentElement->Index;
		const int32 CacheOffset = ChildElementOffsetAndCountCache[InParentElement->ChildCacheIndex].Offset;

		ChildElementCache[CacheOffset + ChildrenCount[ParentElementIndex]] = InChildElement;
		ChildrenCount[ParentElementIndex]++;
	};
	
	for (TConstSetBitIterator<> ElementIt(ElementHasParent); ElementIt; ++ElementIt)
	{
		FRigBaseElement* Element = Elements[ElementIt.GetIndex()];
		
		if(const FRigSingleParentElement* SingleParentElement = Cast<FRigSingleParentElement>(Element))
		{
			if(const FRigTransformElement* ParentElement = SingleParentElement->ParentElement)
			{
				SetChildElement(ParentElement, Element);
			}
		}
		else if(const FRigMultiParentElement* MultiParentElement = Cast<FRigMultiParentElement>(Element))
		{
			for(const FRigElementParentConstraint& ParentConstraint : MultiParentElement->ParentConstraints)
			{
				if(const FRigTransformElement* ParentElement = ParentConstraint.ParentElement)
				{
					SetChildElement(ParentElement, Element);
				}
			}
		}
	}

	// Mark the cache up-to-date.
	ChildElementCacheTopologyVersion = TopologyVersion;
}

	
FRigElementKey URigHierarchy::PreprocessParentElementKeyForSpaceSwitching(const FRigElementKey& InChildKey, const FRigElementKey& InParentKey)
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));
	
	if(InParentKey == GetWorldSpaceReferenceKey())
	{
		return GetOrAddWorldSpaceReference();
	}
	else if(InParentKey == GetDefaultParentKey())
	{
		const FRigElementKey DefaultParent = GetDefaultParent(InChildKey);
		if(DefaultParent == GetWorldSpaceReferenceKey())
		{
			return FRigElementKey();
		}
		else
		{
			return DefaultParent;
		}
	}

	return InParentKey;
}

FRigBaseElement* URigHierarchy::MakeElement(ERigElementType InElementType, int32 InCount, int32* OutStructureSize)
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));
	check(InCount > 0);
	
	FRigBaseElement* Element = nullptr;
	switch(InElementType)
	{
		case ERigElementType::Bone:
		{
			if(OutStructureSize)
			{
				*OutStructureSize = sizeof(FRigBoneElement);
			}
			Element = NewElement<FRigBoneElement>(InCount);
			break;
		}
		case ERigElementType::Null:
		{
			if(OutStructureSize)
			{
				*OutStructureSize = sizeof(FRigNullElement);
			}
			Element = NewElement<FRigNullElement>(InCount);
			break;
		}
		case ERigElementType::Control:
		{
			if(OutStructureSize)
			{
				*OutStructureSize = sizeof(FRigControlElement);
			}
			Element = NewElement<FRigControlElement>(InCount);
			break;
		}
		case ERigElementType::Curve:
		{
			if(OutStructureSize)
			{
				*OutStructureSize = sizeof(FRigCurveElement);
			}
			Element = NewElement<FRigCurveElement>(InCount);
			break;
		}
		case ERigElementType::RigidBody:
		{
			if(OutStructureSize)
			{
				*OutStructureSize = sizeof(FRigRigidBodyElement);
			}
			Element = NewElement<FRigRigidBodyElement>(InCount);
			break;
		}
		case ERigElementType::Reference:
		{
			if(OutStructureSize)
			{
				*OutStructureSize = sizeof(FRigReferenceElement);
			}
			Element = NewElement<FRigReferenceElement>(InCount);
			break;
		}
		case ERigElementType::Connector:
		{
			if(OutStructureSize)
			{
				*OutStructureSize = sizeof(FRigConnectorElement);
			}
			Element = NewElement<FRigConnectorElement>(InCount);
			break;
		}
		case ERigElementType::Socket:
		{
			if(OutStructureSize)
			{
				*OutStructureSize = sizeof(FRigSocketElement);
			}
			Element = NewElement<FRigSocketElement>(InCount);
			break;
		}
		default:
		{
			ensure(false);
		}
	}

	return Element;
}

void URigHierarchy::DestroyElement(FRigBaseElement*& InElement)
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));
	check(InElement != nullptr);

	if(InElement->OwnedInstances == 0)
	{
		return;
	}

	const int32 Count = InElement->OwnedInstances;
	switch(InElement->GetType())
	{
		case ERigElementType::Bone:
		{
			FRigBoneElement* ExistingElements = Cast<FRigBoneElement>(InElement);
			for(int32 Index=0;Index<Count;Index++)
			{
				TGuardValue<const FRigBaseElement*> DestroyGuard(ElementBeingDestroyed, &ExistingElements[Index]);
				ExistingElements[Index].~FRigBoneElement(); 
			}
			break;
		}
		case ERigElementType::Null:
		{
			FRigNullElement* ExistingElements = Cast<FRigNullElement>(InElement);
			for(int32 Index=0;Index<Count;Index++)
			{
				TGuardValue<const FRigBaseElement*> DestroyGuard(ElementBeingDestroyed, &ExistingElements[Index]);
				ExistingElements[Index].~FRigNullElement(); 
			}
			break;
		}
		case ERigElementType::Control:
		{
			FRigControlElement* ExistingElements = Cast<FRigControlElement>(InElement);
			for(int32 Index=0;Index<Count;Index++)
			{
				TGuardValue<const FRigBaseElement*> DestroyGuard(ElementBeingDestroyed, &ExistingElements[Index]);
				ExistingElements[Index].~FRigControlElement(); 
			}
			break;
		}
		case ERigElementType::Curve:
		{
			FRigCurveElement* ExistingElements = Cast<FRigCurveElement>(InElement);
			for(int32 Index=0;Index<Count;Index++)
			{
				TGuardValue<const FRigBaseElement*> DestroyGuard(ElementBeingDestroyed, &ExistingElements[Index]);
				ExistingElements[Index].~FRigCurveElement(); 
			}
			break;
		}
		case ERigElementType::RigidBody:
		{
			FRigRigidBodyElement* ExistingElements = Cast<FRigRigidBodyElement>(InElement);
			for(int32 Index=0;Index<Count;Index++)
			{
				TGuardValue<const FRigBaseElement*> DestroyGuard(ElementBeingDestroyed, &ExistingElements[Index]);
				ExistingElements[Index].~FRigRigidBodyElement(); 
			}
			break;
		}
		case ERigElementType::Reference:
		{
			FRigReferenceElement* ExistingElements = Cast<FRigReferenceElement>(InElement);
			for(int32 Index=0;Index<Count;Index++)
			{
				TGuardValue<const FRigBaseElement*> DestroyGuard(ElementBeingDestroyed, &ExistingElements[Index]);
				ExistingElements[Index].~FRigReferenceElement(); 
			}
			break;
		}
		case ERigElementType::Connector:
		{
			FRigConnectorElement* ExistingElements = Cast<FRigConnectorElement>(InElement);
			for(int32 Index=0;Index<Count;Index++)
			{
				TGuardValue<const FRigBaseElement*> DestroyGuard(ElementBeingDestroyed, &ExistingElements[Index]);
				ExistingElements[Index].~FRigConnectorElement(); 
			}
			break;
		}
		case ERigElementType::Socket:
		{
			FRigSocketElement* ExistingElements = Cast<FRigSocketElement>(InElement);
			for(int32 Index=0;Index<Count;Index++)
			{
				TGuardValue<const FRigBaseElement*> DestroyGuard(ElementBeingDestroyed, &ExistingElements[Index]);
				ExistingElements[Index].~FRigSocketElement(); 
			}
			break;
		}
		default:
		{
			ensure(false);
			return;
		}
	}

	FMemory::Free(InElement);
	InElement = nullptr;
}

void URigHierarchy::PropagateDirtyFlags(FRigTransformElement* InTransformElement, bool bInitial, bool bAffectChildren, bool bComputeOpposed, bool bMarkDirty) const
{
	if(!bEnableDirtyPropagation)
	{
		return;
	}
	
	check(InTransformElement);

	const ERigTransformType::Type LocalType = bInitial ? ERigTransformType::InitialLocal : ERigTransformType::CurrentLocal;
	const ERigTransformType::Type GlobalType = bInitial ? ERigTransformType::InitialGlobal : ERigTransformType::CurrentGlobal;

	if(bComputeOpposed)
	{
		for(const FRigTransformElement::FElementToDirty& ElementToDirty : InTransformElement->ElementsToDirty)
		{
			ERigTransformType::Type TypeToCompute = bAffectChildren ? LocalType : GlobalType;
			ERigTransformType::Type TypeToDirty = SwapLocalAndGlobal(TypeToCompute);
			
			if(FRigControlElement* ControlElement = Cast<FRigControlElement>(ElementToDirty.Element))
			{
				// animation channels never dirty their local value
				if(ControlElement->IsAnimationChannel())
				{
					if(ERigTransformType::IsLocal(TypeToDirty))
					{
						Swap(TypeToDirty, TypeToCompute);
					}
				}
				
				if(ERigTransformType::IsGlobal(TypeToDirty))
				{
					if(ControlElement->Offset.IsDirty(TypeToDirty) &&
						ControlElement->Pose.IsDirty(TypeToDirty) &&
						ControlElement->Shape.IsDirty(TypeToDirty))
					{
						continue;
					}
				}
			}
			else if(FRigMultiParentElement* MultiParentElement = Cast<FRigMultiParentElement>(ElementToDirty.Element))
			{
				if(ERigTransformType::IsGlobal(TypeToDirty))
				{
					if(MultiParentElement->Pose.IsDirty(TypeToDirty))
					{
						continue;
					}
				}
			}
			else
			{
				if(ElementToDirty.Element->Pose.IsDirty(TypeToDirty))
				{
					continue;
				}
			}

			if(FRigControlElement* ControlElement = Cast<FRigControlElement>(ElementToDirty.Element))
			{
				GetControlOffsetTransform(ControlElement, LocalType);
			}
			GetTransform(ElementToDirty.Element, TypeToCompute); // make sure the local / global transform is up 2 date

			PropagateDirtyFlags(ElementToDirty.Element, bInitial, bAffectChildren, true, false);
		}
	}

	if(bMarkDirty)
	{
		for(const FRigTransformElement::FElementToDirty& ElementToDirty : InTransformElement->ElementsToDirty)
		{
			ERigTransformType::Type TypeToCompute = bAffectChildren ? LocalType : GlobalType;
			ERigTransformType::Type TypeToDirty = SwapLocalAndGlobal(TypeToCompute);

			if(FRigControlElement* ControlElement = Cast<FRigControlElement>(ElementToDirty.Element))
			{
				// animation channels never dirty their local value
				if(ControlElement->IsAnimationChannel())
				{
					if(ERigTransformType::IsLocal(TypeToDirty))
					{
						Swap(TypeToDirty, TypeToCompute);
					}
				}

				if(ERigTransformType::IsGlobal(TypeToDirty))
				{
					if(ControlElement->Offset.IsDirty(TypeToDirty) &&
						ControlElement->Pose.IsDirty(TypeToDirty) &&
					    ControlElement->Shape.IsDirty(TypeToDirty))
					{
						continue;
					}
				}
			}
			else if(FRigMultiParentElement* MultiParentElement = Cast<FRigMultiParentElement>(ElementToDirty.Element))
			{
				if(ERigTransformType::IsGlobal(TypeToDirty))
				{
					if(MultiParentElement->Pose.IsDirty(TypeToDirty))
					{
						continue;
					}
				}
			}
			else
			{
				if(ElementToDirty.Element->Pose.IsDirty(TypeToDirty))
				{
					continue;
				}
			}
						
			ElementToDirty.Element->Pose.MarkDirty(TypeToDirty);
		
			if(FRigControlElement* ControlElement = Cast<FRigControlElement>(ElementToDirty.Element))
			{
				ControlElement->Offset.MarkDirty(GlobalType);
				ControlElement->Shape.MarkDirty(GlobalType);
			}

			if(bAffectChildren)
			{
				PropagateDirtyFlags(ElementToDirty.Element, bInitial, bAffectChildren, false, true);
			}
		}
	}
}

void URigHierarchy::CleanupInvalidCaches()
{
	// create a copy of this hierarchy and pre compute all transforms
	if(HierarchyForCacheValidation == nullptr)
	{
		HierarchyForCacheValidation = NewObject<URigHierarchy>(this, NAME_None, RF_Transient);
		HierarchyForCacheValidation->bEnableCacheValidityCheck = false;
	}
	HierarchyForCacheValidation->CopyHierarchy(this);

	struct Local
	{
		static bool NeedsCheck(bool bDirty[FRigLocalAndGlobalTransform::EDirtyMax])
		{
			return !bDirty[FRigLocalAndGlobalTransform::ELocal] && !bDirty[FRigLocalAndGlobalTransform::EGlobal];
		}
	};

	// mark all elements' initial as dirty where needed
	for(int32 ElementIndex = 0; ElementIndex < HierarchyForCacheValidation->Elements.Num(); ElementIndex++)
	{
		FRigBaseElement* BaseElement = HierarchyForCacheValidation->Elements[ElementIndex];
		if(FRigControlElement* ControlElement = Cast<FRigControlElement>(BaseElement))
		{
			if(Local::NeedsCheck(ControlElement->Offset.Initial.bDirty))
			{
				ControlElement->Offset.MarkDirty(ERigTransformType::InitialGlobal);
			}

			if(Local::NeedsCheck(ControlElement->Pose.Initial.bDirty))
			{
				ControlElement->Pose.MarkDirty(ERigTransformType::InitialGlobal);
			}

			if(Local::NeedsCheck(ControlElement->Shape.Initial.bDirty))
			{
				ControlElement->Shape.MarkDirty(ERigTransformType::InitialGlobal);
			}
			continue;
		}

		if(FRigMultiParentElement* MultiParentElement = Cast<FRigMultiParentElement>(BaseElement))
		{
			if(Local::NeedsCheck(MultiParentElement->Pose.Initial.bDirty))
			{
				MultiParentElement->Pose.MarkDirty(ERigTransformType::InitialLocal);
			}
			continue;
		}

		if(FRigTransformElement* TransformElement = Cast<FRigTransformElement>(BaseElement))
		{
			if(Local::NeedsCheck(TransformElement->Pose.Initial.bDirty))
			{
				TransformElement->Pose.MarkDirty(ERigTransformType::InitialGlobal);
			}
		}
	}

	// recompute 
	HierarchyForCacheValidation->ComputeAllTransforms();

	// we need to check the elements for integrity (global vs local) to be correct.
	for(int32 ElementIndex = 0; ElementIndex < Elements.Num(); ElementIndex++)
	{
		FRigBaseElement* BaseElement = Elements[ElementIndex];

		if(FRigControlElement* ControlElement = Cast<FRigControlElement>(BaseElement))
		{
			FRigControlElement* OtherControlElement = HierarchyForCacheValidation->FindChecked<FRigControlElement>(ControlElement->GetKey());
			
			if(Local::NeedsCheck(ControlElement->Offset.Initial.bDirty))
			{
				const FTransform CachedGlobalTransform = OtherControlElement->Offset.Get(ERigTransformType::InitialGlobal);
				const FTransform ComputedGlobalTransform = HierarchyForCacheValidation->GetControlOffsetTransform(OtherControlElement, ERigTransformType::InitialGlobal);

				if(!FRigComputedTransform::Equals(ComputedGlobalTransform, CachedGlobalTransform, 0.01))
				{
					ControlElement->Offset.MarkDirty(ERigTransformType::InitialGlobal);
				}
			}

			if(Local::NeedsCheck(ControlElement->Pose.Initial.bDirty))
			{
				const FTransform CachedGlobalTransform = ControlElement->Pose.Get(ERigTransformType::InitialGlobal);
				const FTransform ComputedGlobalTransform = HierarchyForCacheValidation->GetTransform(OtherControlElement, ERigTransformType::InitialGlobal);
				
				if(!FRigComputedTransform::Equals(ComputedGlobalTransform, CachedGlobalTransform, 0.01))
				{
					ControlElement->Pose.MarkDirty(ERigTransformType::InitialGlobal);
				}
			}

			if(Local::NeedsCheck(ControlElement->Shape.Initial.bDirty))
			{
				const FTransform CachedGlobalTransform = ControlElement->Shape.Get(ERigTransformType::InitialGlobal);
				const FTransform ComputedGlobalTransform = HierarchyForCacheValidation->GetControlShapeTransform(OtherControlElement, ERigTransformType::InitialGlobal);
				
				if(!FRigComputedTransform::Equals(ComputedGlobalTransform, CachedGlobalTransform, 0.01))
				{
					ControlElement->Shape.MarkDirty(ERigTransformType::InitialGlobal);
				}
			}
			continue;
		}

		if(FRigMultiParentElement* MultiParentElement = Cast<FRigMultiParentElement>(BaseElement))
		{
			FRigMultiParentElement* OtherMultiParentElement = HierarchyForCacheValidation->FindChecked<FRigMultiParentElement>(MultiParentElement->GetKey());

			if(Local::NeedsCheck(MultiParentElement->Pose.Initial.bDirty))
			{
				const FTransform CachedGlobalTransform = MultiParentElement->Pose.Get(ERigTransformType::InitialGlobal);
				const FTransform ComputedGlobalTransform = HierarchyForCacheValidation->GetTransform(OtherMultiParentElement, ERigTransformType::InitialGlobal);
				
				if(!FRigComputedTransform::Equals(ComputedGlobalTransform, CachedGlobalTransform, 0.01))
				{
					// for nulls we perceive the local transform as less relevant
					MultiParentElement->Pose.MarkDirty(ERigTransformType::InitialLocal);
				}
			}
			continue;
		}

		if(FRigTransformElement* TransformElement = Cast<FRigTransformElement>(BaseElement))
		{
			FRigTransformElement* OtherTransformElement = HierarchyForCacheValidation->FindChecked<FRigTransformElement>(TransformElement->GetKey());

			if(Local::NeedsCheck(TransformElement->Pose.Initial.bDirty))
			{
				const FTransform CachedGlobalTransform = TransformElement->Pose.Get(ERigTransformType::InitialGlobal);
				const FTransform ComputedGlobalTransform = HierarchyForCacheValidation->GetTransform(OtherTransformElement, ERigTransformType::InitialGlobal);
				
				if(!FRigComputedTransform::Equals(ComputedGlobalTransform, CachedGlobalTransform, 0.01))
				{
					TransformElement->Pose.MarkDirty(ERigTransformType::InitialGlobal);
				}
			}
		}
	}

	ResetPoseToInitial(ERigElementType::All);
	EnsureCacheValidity();
}

void URigHierarchy::FMetadataStorage::Reset()
{
	for (TTuple<FName, FRigBaseMetadata*>& Item: MetadataMap)
	{
		FRigBaseMetadata::DestroyMetadata(&Item.Value);
	}
	MetadataMap.Reset();
	LastAccessName = NAME_None;
	LastAccessMetadata = nullptr;
}

void URigHierarchy::FMetadataStorage::Serialize(FArchive& Ar)
{
	static const UEnum* MetadataTypeEnum = StaticEnum<ERigMetadataType>();
	
	if (Ar.IsLoading())
	{
		Reset();
		
		int32 NumEntries = 0;
		Ar << NumEntries;
		MetadataMap.Reserve(NumEntries);
		for (int32 Index = 0; Index < NumEntries; Index++)
		{
			FName ItemName, TypeName;
			Ar << ItemName;
			Ar << TypeName;

			const ERigMetadataType Type = static_cast<ERigMetadataType>(MetadataTypeEnum->GetValueByName(TypeName));
			FRigBaseMetadata* Metadata = FRigBaseMetadata::MakeMetadata(ItemName, Type);
			Metadata->Serialize(Ar);

			MetadataMap.Add(ItemName, Metadata);
		}
	}
	else
	{
		int32 NumEntries = MetadataMap.Num();
		Ar << NumEntries;
		for (const TTuple<FName, FRigBaseMetadata*>& Item: MetadataMap)
		{
			FName ItemName = Item.Key;
			FName TypeName = MetadataTypeEnum->GetNameByValue(static_cast<int64>(Item.Value->GetType()));
			Ar << ItemName;
			Ar << TypeName;
			Item.Value->Serialize(Ar);
		}
	}
}

void URigHierarchy::PropagateMetadata(const FRigElementKey& InKey, const FName& InName, bool bNotify)
{
	if(const FRigBaseElement* Element = Find(InKey))
	{
		PropagateMetadata(Element, InName, bNotify);
	}
}

void URigHierarchy::PropagateMetadata(const FRigBaseElement* InElement, const FName& InName, bool bNotify)
{
#if WITH_EDITOR
	check(InElement);

	if (!ElementMetadata.IsValidIndex(InElement->MetadataStorageIndex))
	{
		return;
	}

	FMetadataStorage& Storage = ElementMetadata[InElement->MetadataStorageIndex];
	FRigBaseMetadata** MetadataPtrPtr = Storage.MetadataMap.Find(InName);
	if (!MetadataPtrPtr)
	{
		return;
	}

	FRigBaseMetadata* MetadataPtr = *MetadataPtrPtr;
	if(MetadataPtr == nullptr)
	{
		return;
	}

	ForEachListeningHierarchy([InElement, MetadataPtr, bNotify](const FRigHierarchyListener& Listener)
	{
		if(URigHierarchy* ListeningHierarchy = Listener.Hierarchy.Get())
		{
			if(FRigBaseElement* Element = ListeningHierarchy->Find(InElement->GetKey()))
			{
				if (FRigBaseMetadata* Metadata = ListeningHierarchy->GetMetadataForElement(Element, MetadataPtr->GetName(), MetadataPtr->GetType(), bNotify))
				{
					Metadata->SetValueData(MetadataPtr->GetValueData(), MetadataPtr->GetValueSize());
					ListeningHierarchy->PropagateMetadata(Element, Metadata->GetName(), bNotify);
				}
			}
		}
	});
#endif
}

void URigHierarchy::OnMetadataChanged(const FRigElementKey& InKey, const FName& InName)
{
	MetadataVersion++;

	if (!bSuspendMetadataNotifications)
	{
		if(MetadataChangedDelegate.IsBound())
		{
			MetadataChangedDelegate.Broadcast(InKey, InName);
		}
	}
}

void URigHierarchy::OnMetadataTagChanged(const FRigElementKey& InKey, const FName& InTag, bool bAdded)
{
	MetadataTagVersion++;

	if (!bSuspendMetadataNotifications)
	{
		if(MetadataTagChangedDelegate.IsBound())
		{
			MetadataTagChangedDelegate.Broadcast(InKey, InTag, bAdded);
		}
	}
}

FRigBaseMetadata* URigHierarchy::GetMetadataForElement(FRigBaseElement* InElement, const FName& InName, ERigMetadataType InType, bool bInNotify)
{
	if (!ElementMetadata.IsValidIndex(InElement->MetadataStorageIndex))
	{
		// Do we have entries in the freelist we can recycle?
		if (!ElementMetadataFreeList.IsEmpty())
		{
			InElement->MetadataStorageIndex = ElementMetadataFreeList.Pop(EAllowShrinking::No);
		}
		else
		{
			InElement->MetadataStorageIndex = ElementMetadata.Num();
			ElementMetadata.AddDefaulted();
		}
	}
	
	FMetadataStorage& Storage = ElementMetadata[InElement->MetadataStorageIndex];

	// If repeatedly accessing the same element, store it here for faster access to avoid map lookups.
	if (Storage.LastAccessMetadata && Storage.LastAccessName == InName && Storage.LastAccessMetadata->GetType() == InType)
	{
		return Storage.LastAccessMetadata;
	}

	FRigBaseMetadata* Metadata;
	if (FRigBaseMetadata** MetadataPtrPtr = Storage.MetadataMap.Find(InName))
	{
		if ((*MetadataPtrPtr)->GetType() == InType)
		{
			Metadata = *MetadataPtrPtr;
		}
		else
		{
			// The type changed, replace the existing metadata with a new one of the correct type.
			FRigBaseMetadata::DestroyMetadata(MetadataPtrPtr);
			Metadata = *MetadataPtrPtr = FRigBaseMetadata::MakeMetadata(InName, InType);
			
			if (bInNotify)
			{
				OnMetadataChanged(InElement->Key, InName);
			}
		}
	}
	else
	{
		// No metadata with that name existed on the element, create one from scratch.
		Metadata = FRigBaseMetadata::MakeMetadata(InName, InType);
		Storage.MetadataMap.Add(InName, Metadata);

		if (bInNotify)
		{
			OnMetadataChanged(InElement->Key, InName);
		}
	}
	
	Storage.LastAccessName = InName;
	Storage.LastAccessMetadata = Metadata;
	return Metadata;
}


FRigBaseMetadata* URigHierarchy::FindMetadataForElement(const FRigBaseElement* InElement, const FName& InName, ERigMetadataType InType)
{
	if (!ElementMetadata.IsValidIndex(InElement->MetadataStorageIndex))
	{
		return nullptr;
	}

	FMetadataStorage& Storage = ElementMetadata[InElement->MetadataStorageIndex];
	if (InName == Storage.LastAccessName && (InType == ERigMetadataType::Invalid || Storage.LastAccessMetadata->GetType() == InType))
	{
		return Storage.LastAccessMetadata;
	}
	
	FRigBaseMetadata** MetadataPtrPtr = Storage.MetadataMap.Find(InName);
	if (!MetadataPtrPtr)
	{
		Storage.LastAccessName = NAME_None;
		Storage.LastAccessMetadata = nullptr;
		return nullptr;
	}

	if (InType != ERigMetadataType::Invalid && (*MetadataPtrPtr)->GetType() != InType)
	{
		Storage.LastAccessName = NAME_None;
		Storage.LastAccessMetadata = nullptr;
		return nullptr;
	}

	Storage.LastAccessName = InName;
	Storage.LastAccessMetadata = *MetadataPtrPtr;

	return *MetadataPtrPtr;
}

const FRigBaseMetadata* URigHierarchy::FindMetadataForElement(const FRigBaseElement* InElement, const FName& InName, ERigMetadataType InType) const
{
	return const_cast<URigHierarchy*>(this)->FindMetadataForElement(InElement, InName, InType);
}


bool URigHierarchy::RemoveMetadataForElement(FRigBaseElement* InElement, const FName& InName)
{
	if (!ElementMetadata.IsValidIndex(InElement->MetadataStorageIndex))
	{
		return false;
	}

	FMetadataStorage& Storage = ElementMetadata[InElement->MetadataStorageIndex];
	FRigBaseMetadata** MetadataPtrPtr = Storage.MetadataMap.Find(InName);
	if (!MetadataPtrPtr)
	{
		return false;
	}
	
	FRigBaseMetadata::DestroyMetadata(MetadataPtrPtr);
	Storage.MetadataMap.Remove(InName);

	// If the storage is now empty, remove the element's storage, so we're not lugging it around
	// unnecessarily. Add the storage slot to the freelist so that the next element to add a new
	// metadata storage can just recycle that.
	if (Storage.MetadataMap.IsEmpty())
	{
		ElementMetadataFreeList.Add(InElement->MetadataStorageIndex);
		InElement->MetadataStorageIndex = INDEX_NONE;
	}
	else if (Storage.LastAccessName == InName)
	{
		Storage.LastAccessMetadata = nullptr;
	}

	if(ElementBeingDestroyed != InElement)
	{
		OnMetadataChanged(InElement->Key, InName);
	}
	return true;
}


bool URigHierarchy::RemoveAllMetadataForElement(FRigBaseElement* InElement)
{
	if (!ElementMetadata.IsValidIndex(InElement->MetadataStorageIndex))
	{
		return false;
	}

	
	FMetadataStorage& Storage = ElementMetadata[InElement->MetadataStorageIndex];
	TArray<FName> Names;
	Storage.MetadataMap.GetKeys(Names);
	
	// Clear the storage for the next user.
	Storage.Reset();
	
	ElementMetadataFreeList.Push(InElement->MetadataStorageIndex);
	InElement->MetadataStorageIndex = INDEX_NONE;

	if(ElementBeingDestroyed != InElement)
	{
		for (FName Name: Names)
		{
			OnMetadataChanged(InElement->Key, Name);
		}
	}
	
	return true; 
}


void URigHierarchy::CopyAllMetadataFromElement(FRigBaseElement* InTargetElement, const FRigBaseElement* InSourceElement)
{
	if (!ensure(InSourceElement->Owner))
	{
		return;
	}

	if (!InSourceElement->Owner->ElementMetadata.IsValidIndex(InSourceElement->MetadataStorageIndex))
	{
		return;
	}
	
	const FMetadataStorage& SourceStorage = InSourceElement->Owner->ElementMetadata[InSourceElement->MetadataStorageIndex];
	
	for (const TTuple<FName, FRigBaseMetadata*>& SourceItem: SourceStorage.MetadataMap)
	{
		const FRigBaseMetadata* SourceMetadata = SourceItem.Value; 	

		constexpr bool bNotify = false;
		FRigBaseMetadata* TargetMetadata = GetMetadataForElement(InTargetElement, SourceItem.Key, SourceMetadata->GetType(), bNotify);
		TargetMetadata->SetValueData(SourceMetadata->GetValueData(), SourceMetadata->GetValueSize());
	}
}


void URigHierarchy::EnsureCacheValidityImpl()
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));
	if(!bEnableCacheValidityCheck)
	{
		return;
	}
	TGuardValue<bool> Guard(bEnableCacheValidityCheck, false);

	static const TArray<FString>& TransformTypeStrings = GetTransformTypeStrings();

	// make sure that elements which are marked as dirty don't have fully cached children
	ForEach<FRigTransformElement>([](FRigTransformElement* TransformElement)
    {
		for(int32 TransformTypeIndex = 0; TransformTypeIndex < (int32) ERigTransformType::NumTransformTypes; TransformTypeIndex++)
		{
			const ERigTransformType::Type GlobalType = (ERigTransformType::Type)TransformTypeIndex;
			const ERigTransformType::Type LocalType = ERigTransformType::SwapLocalAndGlobal(GlobalType);
			const FString& TransformTypeString = TransformTypeStrings[TransformTypeIndex];

			if(ERigTransformType::IsLocal(GlobalType))
			{
				continue;
			}

			if(!TransformElement->Pose.IsDirty(GlobalType))
			{
				continue;
			}

			for(const FRigTransformElement::FElementToDirty& ElementToDirty : TransformElement->ElementsToDirty)
			{
				if(FRigMultiParentElement* MultiParentElementToDirty = Cast<FRigMultiParentElement>(ElementToDirty.Element))
				{
                    if(FRigControlElement* ControlElementToDirty = Cast<FRigControlElement>(ElementToDirty.Element))
                    {
                        if(ControlElementToDirty->Offset.IsDirty(GlobalType))
                        {
                            checkf(ControlElementToDirty->Pose.IsDirty(GlobalType) ||
                                    ControlElementToDirty->Pose.IsDirty(LocalType),
                                    TEXT("Control '%s' %s Offset Cache is dirty, but the Pose is not."),
									*ControlElementToDirty->GetKey().ToString(),
									*TransformTypeString);
						}

                        if(ControlElementToDirty->Pose.IsDirty(GlobalType))
                        {
                            checkf(ControlElementToDirty->Shape.IsDirty(GlobalType) ||
                                    ControlElementToDirty->Shape.IsDirty(LocalType),
                                    TEXT("Control '%s' %s Pose Cache is dirty, but the Shape is not."),
									*ControlElementToDirty->GetKey().ToString(),
									*TransformTypeString);
						}
                    }
                    else
                    {
                        checkf(MultiParentElementToDirty->Pose.IsDirty(GlobalType) ||
                                MultiParentElementToDirty->Pose.IsDirty(LocalType),
                                TEXT("MultiParent '%s' %s Parent Cache is dirty, but the Pose is not."),
								*MultiParentElementToDirty->GetKey().ToString(),
								*TransformTypeString);
                    }
				}
				else
				{
					checkf(ElementToDirty.Element->Pose.IsDirty(GlobalType) ||
						ElementToDirty.Element->Pose.IsDirty(LocalType),
						TEXT("SingleParent '%s' %s Pose is not dirty in Local or Global"),
						*ElementToDirty.Element->GetKey().ToString(),
						*TransformTypeString);
				}
			}
		}
		
        return true;
    });

	// store our own pose in a transient hierarchy used for cache validation
	if(HierarchyForCacheValidation == nullptr)
	{
		HierarchyForCacheValidation = NewObject<URigHierarchy>(this, NAME_None, RF_Transient);
		HierarchyForCacheValidation->bEnableCacheValidityCheck = false;
	}
	if(HierarchyForCacheValidation->GetTopologyVersion() != GetTopologyVersion())
	{
		HierarchyForCacheValidation->CopyHierarchy(this);
	}
	HierarchyForCacheValidation->CopyPose(this, true, true, true);

	// traverse the copied hierarchy and compare cached vs computed values
	URigHierarchy* HierarchyForLambda = HierarchyForCacheValidation;
	HierarchyForLambda->Traverse([HierarchyForLambda](FRigBaseElement* Element, bool& bContinue)
	{
		bContinue = true;

		if(FRigControlElement* ControlElement = Cast<FRigControlElement>(Element))
		{
			for(int32 TransformTypeIndex = 0; TransformTypeIndex < (int32) ERigTransformType::NumTransformTypes; TransformTypeIndex++)
			{
				const ERigTransformType::Type TransformType = (ERigTransformType::Type)TransformTypeIndex;
				const ERigTransformType::Type OpposedType = ERigTransformType::SwapLocalAndGlobal(TransformType);
				const FString& TransformTypeString = TransformTypeStrings[TransformTypeIndex];

				if(!ControlElement->Offset.IsDirty(TransformType) && !ControlElement->Offset.IsDirty(OpposedType))
				{
					const FTransform CachedTransform = HierarchyForLambda->GetControlOffsetTransform(ControlElement, TransformType);
					ControlElement->Offset.MarkDirty(TransformType);
					const FTransform ComputedTransform = HierarchyForLambda->GetControlOffsetTransform(ControlElement, TransformType);
					checkf(FRigComputedTransform::Equals(CachedTransform, ComputedTransform),
						TEXT("Element '%s' Offset %s Cached vs Computed doesn't match. ('%s' <-> '%s')"),
						*Element->GetName(),
						*TransformTypeString,
						*CachedTransform.ToString(),
						*ComputedTransform.ToString());
				}
			}
		}

		if(FRigTransformElement* TransformElement = Cast<FRigTransformElement>(Element))
		{
			for(int32 TransformTypeIndex = 0; TransformTypeIndex < (int32) ERigTransformType::NumTransformTypes; TransformTypeIndex++)
			{
				const ERigTransformType::Type TransformType = (ERigTransformType::Type)TransformTypeIndex;
				const ERigTransformType::Type OpposedType = ERigTransformType::SwapLocalAndGlobal(TransformType);
				const FString& TransformTypeString = TransformTypeStrings[TransformTypeIndex];

				if(!TransformElement->Pose.IsDirty(TransformType) && !TransformElement->Pose.IsDirty(OpposedType))
				{
					const FTransform CachedTransform = HierarchyForLambda->GetTransform(TransformElement, TransformType);
					TransformElement->Pose.MarkDirty(TransformType);
					const FTransform ComputedTransform = HierarchyForLambda->GetTransform(TransformElement, TransformType);
					checkf(FRigComputedTransform::Equals(CachedTransform, ComputedTransform),
						TEXT("Element '%s' Pose %s Cached vs Computed doesn't match. ('%s' <-> '%s')"),
						*Element->GetName(),
						*TransformTypeString,
						*CachedTransform.ToString(), *ComputedTransform.ToString());
				}
			}
		}

		if(FRigControlElement* ControlElement = Cast<FRigControlElement>(Element))
		{
			for(int32 TransformTypeIndex = 0; TransformTypeIndex < (int32) ERigTransformType::NumTransformTypes; TransformTypeIndex++)
			{
				const ERigTransformType::Type TransformType = (ERigTransformType::Type)TransformTypeIndex;
				const ERigTransformType::Type OpposedType = ERigTransformType::SwapLocalAndGlobal(TransformType);
				const FString& TransformTypeString = TransformTypeStrings[TransformTypeIndex];

				if(!ControlElement->Shape.IsDirty(TransformType) && !ControlElement->Shape.IsDirty(OpposedType))
				{
					const FTransform CachedTransform = HierarchyForLambda->GetControlShapeTransform(ControlElement, TransformType);
					ControlElement->Shape.MarkDirty(TransformType);
					const FTransform ComputedTransform = HierarchyForLambda->GetControlShapeTransform(ControlElement, TransformType);
					checkf(FRigComputedTransform::Equals(CachedTransform, ComputedTransform),
						TEXT("Element '%s' Shape %s Cached vs Computed doesn't match. ('%s' <-> '%s')"),
						*Element->GetName(),
						*TransformTypeString,
						*CachedTransform.ToString(), *ComputedTransform.ToString());
				}
			}
		}
	});
}

#if WITH_EDITOR

URigHierarchy::TElementDependencyMap URigHierarchy::GetDependenciesForVM(const URigVM* InVM, FName InEventName) const
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));
	check(InVM);

	if(InEventName.IsNone())
	{
		InEventName = FRigUnit_BeginExecution().GetEventName();
	}

	URigHierarchy::TElementDependencyMap Dependencies;
	const FRigVMInstructionArray Instructions = InVM->GetByteCode().GetInstructions();

	// if the VM doesn't implement the given event
	if(!InVM->ContainsEntry(InEventName))
	{
		return Dependencies;
	}

	// only represent instruction for a given event
	const int32 EntryIndex = InVM->GetByteCode().FindEntryIndex(InEventName);
	const int32 EntryInstructionIndex = InVM->GetByteCode().GetEntry(EntryIndex).InstructionIndex;

	TMap<FRigVMOperand, TArray<int32>> OperandToInstructions;

	for(int32 InstructionIndex = EntryInstructionIndex; InstructionIndex < Instructions.Num(); InstructionIndex++)
	{
		// early exit since following instructions belong to another event
		if(Instructions[InstructionIndex].OpCode == ERigVMOpCode::Exit)
		{
			break;
		}

		const FRigVMOperandArray InputOperands = InVM->GetByteCode().GetInputOperands(InstructionIndex);
		for(const FRigVMOperand InputOperand : InputOperands)
		{
			const FRigVMOperand InputOperandNoRegisterOffset(InputOperand.GetMemoryType(), InputOperand.GetRegisterIndex());
			OperandToInstructions.FindOrAdd(InputOperandNoRegisterOffset).Add(InstructionIndex);
		}
	}

	typedef TTuple<int32, int32> TInt32Tuple;
	TArray<TArray<TInt32Tuple>> ReadTransformPerInstruction, WrittenTransformsPerInstruction;
	
	// Find the max instruction index
	{
		/** TODO: UE-207320
		 * This function needs to be rewritten to account for modular rigs. Rig Hierarchies will have reads/writes coming from
		 * different VMs, so we cannot depend on the algorithm used in this function to find dependencies. */
		
		int32 MaxInstructionIndex = Instructions.Num();
		for(int32 RecordType = 0; RecordType < 2; RecordType++)
		{
			const TArray<TInstructionSliceElement>& Records = RecordType == 0 ? ReadTransformsAtRuntime : WrittenTransformsAtRuntime; 
			for(int32 RecordIndex = 0; RecordIndex < Records.Num(); RecordIndex++)
			{
				const TInstructionSliceElement& Record = Records[RecordIndex];
				const int32& InstructionIndex = Record.Get<0>();
				MaxInstructionIndex = FMath::Max(MaxInstructionIndex, InstructionIndex);
			}
		}
		ReadTransformPerInstruction.AddZeroed(MaxInstructionIndex+1);
		WrittenTransformsPerInstruction.AddZeroed(MaxInstructionIndex+1);
	}

	// fill lookup tables per instruction / element
	for(int32 RecordType = 0; RecordType < 2; RecordType++)
	{
		const TArray<TInstructionSliceElement>& Records = RecordType == 0 ? ReadTransformsAtRuntime : WrittenTransformsAtRuntime; 
		TArray<TArray<TInt32Tuple>>& PerInstruction = RecordType == 0 ? ReadTransformPerInstruction : WrittenTransformsPerInstruction;

		for(int32 RecordIndex = 0; RecordIndex < Records.Num(); RecordIndex++)
		{
			const TInstructionSliceElement& Record = Records[RecordIndex];
			const int32& InstructionIndex = Record.Get<0>();
			const int32& SliceIndex = Record.Get<1>();
			const int32& TransformIndex = Record.Get<2>();
			PerInstruction[InstructionIndex].Emplace(SliceIndex, TransformIndex);
		}
	}

	// for each read transform on an instruction
	// follow the operands to the next instruction affected by it
	TArray<TInt32Tuple> FilteredTransforms;
	TArray<int32> InstructionsToVisit;

	for(int32 InstructionIndex = EntryInstructionIndex; InstructionIndex < Instructions.Num(); InstructionIndex++)
	{
		// early exit since following instructions belong to another event
		if(Instructions[InstructionIndex].OpCode == ERigVMOpCode::Exit)
		{
			break;
		}

		const TArray<TInt32Tuple>& ReadTransforms = ReadTransformPerInstruction[InstructionIndex];
		if(ReadTransforms.IsEmpty())
		{
			continue;
		}

		FilteredTransforms.Reset();

		for(int32 ReadTransformIndex = 0; ReadTransformIndex < ReadTransforms.Num(); ReadTransformIndex++)
		{
			const TInt32Tuple& ReadTransform = ReadTransforms[ReadTransformIndex];
			
			InstructionsToVisit.Reset();
			InstructionsToVisit.Add(InstructionIndex);

			for(int32 InstructionToVisitIndex = 0; InstructionToVisitIndex < InstructionsToVisit.Num(); InstructionToVisitIndex++)
			{
				const int32 InstructionToVisit = InstructionsToVisit[InstructionToVisitIndex];

				const TArray<TInt32Tuple>& WrittenTransforms = WrittenTransformsPerInstruction[InstructionToVisit];
				for(int32 WrittenTransformIndex = 0; WrittenTransformIndex < WrittenTransforms.Num(); WrittenTransformIndex++)
				{
					const TInt32Tuple& WrittenTransform = WrittenTransforms[WrittenTransformIndex];

					// for the first instruction in this pass let's only care about
					// written transforms which have not been read before
					if(InstructionToVisitIndex == 0)
					{
						if(ReadTransforms.Contains(WrittenTransform))
						{
							continue;
						}
					}
					
					FilteredTransforms.AddUnique(WrittenTransform);
				}

				FRigVMOperandArray OutputOperands = InVM->GetByteCode().GetOutputOperands(InstructionToVisit);
				for(const FRigVMOperand OutputOperand : OutputOperands)
				{
					const FRigVMOperand OutputOperandNoRegisterOffset(OutputOperand.GetMemoryType(), OutputOperand.GetRegisterIndex());

					if(const TArray<int32>* InstructionsWithInputOperand = OperandToInstructions.Find(OutputOperandNoRegisterOffset))
					{
						for(int32 InstructionWithInputOperand : *InstructionsWithInputOperand)
						{
							InstructionsToVisit.AddUnique(InstructionWithInputOperand);
						}
					}
				}
			}
		}
		
		for(const TInt32Tuple& ReadTransform : ReadTransforms)
		{
			for(const TInt32Tuple& FilteredTransform : FilteredTransforms)
			{
				// only create dependencies for reads and writes that are on the same slice
				if(ReadTransform != FilteredTransform && ReadTransform.Get<0>() == FilteredTransform.Get<0>())
				{
					Dependencies.FindOrAdd(FilteredTransform.Get<1>()).AddUnique(ReadTransform.Get<1>());
				}
			}
		}
	}

	return Dependencies;
}

#endif

void URigHierarchy::UpdateVisibilityOnProxyControls()
{
	URigHierarchy* HierarchyForSelection = HierarchyForSelectionPtr.Get();
	if(HierarchyForSelection == nullptr)
	{
		HierarchyForSelection = this;
	}

	const UWorld* World = GetWorld();
	if(World == nullptr)
	{
		return;
	}
	if(World->IsPreviewWorld())
	{
		return;
	}

	// create a local map of visible things, starting with the selection
	TSet<FRigElementKey> VisibleElements;
	VisibleElements.Append(HierarchyForSelection->OrderedSelection);

	// if the control is visible - we should consider the
	// driven controls visible as well - so that other proxies
	// assigned to the same driven control also show up
	for(const FRigBaseElement* Element : Elements)
	{
		if(const FRigControlElement* ControlElement = Cast<FRigControlElement>(Element))
		{
			if(ControlElement->Settings.AnimationType == ERigControlAnimationType::ProxyControl)
			{
				if(VisibleElements.Contains(ControlElement->GetKey()))
				{
					VisibleElements.Append(ControlElement->Settings.DrivenControls);
				}
			}
		}
	}

	for(FRigBaseElement* Element : Elements)
	{
		if(FRigControlElement* ControlElement = Cast<FRigControlElement>(Element))
		{
			if(ControlElement->Settings.AnimationType == ERigControlAnimationType::ProxyControl)
			{
				if(ControlElement->Settings.ShapeVisibility == ERigControlVisibility::BasedOnSelection)
				{
					if(HierarchyForSelection->OrderedSelection.IsEmpty())
					{
						if(ControlElement->Settings.SetVisible(false, true))
						{
							Notify(ERigHierarchyNotification::ControlVisibilityChanged, ControlElement);
						}
					}
					else
					{
						// a proxy control should be visible if itself or a driven control is selected / visible
						bool bVisible = VisibleElements.Contains(ControlElement->GetKey());
						if(!bVisible)
						{
							if(ControlElement->Settings.DrivenControls.FindByPredicate([VisibleElements](const FRigElementKey& AffectedControl) -> bool
							{
								return VisibleElements.Contains(AffectedControl);
							}) != nullptr)
							{
								bVisible = true;
							}
						}

						if(bVisible)
						{
							VisibleElements.Add(ControlElement->GetKey());
						}

						if(ControlElement->Settings.SetVisible(bVisible, true))
						{
							Notify(ERigHierarchyNotification::ControlVisibilityChanged, ControlElement);
						}
					}
				}
			}
		}
	}
}

const TArray<FString>& URigHierarchy::GetTransformTypeStrings()
{
	static TArray<FString> TransformTypeStrings;
	if(TransformTypeStrings.IsEmpty())
	{
		for(int32 TransformTypeIndex = 0; TransformTypeIndex < (int32) ERigTransformType::NumTransformTypes; TransformTypeIndex++)
		{
			TransformTypeStrings.Add(StaticEnum<ERigTransformType::Type>()->GetDisplayNameTextByValue((int64)TransformTypeIndex).ToString());
		}
	}
	return TransformTypeStrings;
}

void URigHierarchy::PushTransformToStack(const FRigElementKey& InKey, ERigTransformStackEntryType InEntryType,
                                         ERigTransformType::Type InTransformType, const FTransform& InOldTransform, const FTransform& InNewTransform,
                                         bool bAffectChildren, bool bModify)
{
#if WITH_EDITOR

	if(GIsTransacting)
	{
		return;
	}

	static const FText TransformPoseTitle = NSLOCTEXT("RigHierarchy", "Set Pose Transform", "Set Pose Transform");
	static const FText ControlOffsetTitle = NSLOCTEXT("RigHierarchy", "Set Control Offset", "Set Control Offset");
	static const FText ControlShapeTitle = NSLOCTEXT("RigHierarchy", "Set Control Shape", "Set Control Shape");
	static const FText CurveValueTitle = NSLOCTEXT("RigHierarchy", "Set Curve Value", "Set Curve Value");
	
	FText Title;
	switch(InEntryType)
	{
		case ERigTransformStackEntryType::TransformPose:
		{
			Title = TransformPoseTitle;
			break;
		}
		case ERigTransformStackEntryType::ControlOffset:
		{
			Title = TransformPoseTitle;
			break;
		}
		case ERigTransformStackEntryType::ControlShape:
		{
			Title = TransformPoseTitle;
			break;
		}
		case ERigTransformStackEntryType::CurveValue:
		{
			Title = TransformPoseTitle;
			break;
		}
	}

	TGuardValue<bool> TransactingGuard(bTransactingForTransformChange, true);

	TSharedPtr<FScopedTransaction> TransactionPtr;
	if(bModify)
	{
		TransactionPtr = MakeShareable(new FScopedTransaction(Title));
	}

	if(bIsInteracting)
	{
		bool bCanMerge = LastInteractedKey == InKey;

		FRigTransformStackEntry LastEntry;
		if(!TransformUndoStack.IsEmpty())
		{
			LastEntry = TransformUndoStack.Last();
		}

		if(bCanMerge && LastEntry.Key == InKey && LastEntry.EntryType == InEntryType && LastEntry.bAffectChildren == bAffectChildren)
		{
			// merge the entries on the stack
			TransformUndoStack.Last() = 
                FRigTransformStackEntry(InKey, InEntryType, InTransformType, LastEntry.OldTransform, InNewTransform, bAffectChildren);
		}
		else
		{
			Modify();

			TransformUndoStack.Add(
                FRigTransformStackEntry(InKey, InEntryType, InTransformType, InOldTransform, InNewTransform, bAffectChildren));
			TransformStackIndex = TransformUndoStack.Num();
		}

		TransformRedoStack.Reset();
		LastInteractedKey = InKey;
		return;
	}

	if(bModify)
	{
		Modify();
	}

	TArray<FString> Callstack;
	if(IsTracingChanges() && (CVarControlRigHierarchyTraceCallstack->GetInt() != 0))
	{
		FString JoinedCallStack;
		RigHierarchyCaptureCallStack(JoinedCallStack, 1);
		JoinedCallStack.ReplaceInline(TEXT("\r"), TEXT(""));

		FString Left, Right;
		do
		{
			if(!JoinedCallStack.Split(TEXT("\n"), &Left, &Right))
			{
				Left = JoinedCallStack;
				Right.Empty();
			}

			Left.TrimStartAndEndInline();
			if(Left.StartsWith(TEXT("0x")))
			{
				Left.Split(TEXT(" "), nullptr, &Left);
			}
			Callstack.Add(Left);
			JoinedCallStack = Right;
		}
		while(!JoinedCallStack.IsEmpty());
	}

	TransformUndoStack.Add(
		FRigTransformStackEntry(InKey, InEntryType, InTransformType, InOldTransform, InNewTransform, bAffectChildren, Callstack));
	TransformStackIndex = TransformUndoStack.Num();

	TransformRedoStack.Reset();
	
#endif
}

void URigHierarchy::PushCurveToStack(const FRigElementKey& InKey, float InOldCurveValue, float InNewCurveValue, bool bInOldIsCurveValueSet, bool bInNewIsCurveValueSet, bool bModify)
{
#if WITH_EDITOR

	FTransform OldTransform = FTransform::Identity;
	FTransform NewTransform = FTransform::Identity;

	OldTransform.SetTranslation(FVector(InOldCurveValue, bInOldIsCurveValueSet ? 1.f : 0.f, 0.f));
	NewTransform.SetTranslation(FVector(InNewCurveValue, bInNewIsCurveValueSet ? 1.f : 0.f, 0.f));

	PushTransformToStack(InKey, ERigTransformStackEntryType::CurveValue, ERigTransformType::CurrentLocal, OldTransform, NewTransform, false, bModify);

#endif
}

bool URigHierarchy::ApplyTransformFromStack(const FRigTransformStackEntry& InEntry, bool bUndo)
{
#if WITH_EDITOR

	bool bApplyInitialForCurrent = false;
	FRigBaseElement* Element = Find(InEntry.Key);
	if(Element == nullptr)
	{
		// this might be a transient control which had been removed.
		if(InEntry.Key.Type == ERigElementType::Control)
		{
			const FRigElementKey TargetKey = UControlRig::GetElementKeyFromTransientControl(InEntry.Key);
			Element = Find(TargetKey);
			bApplyInitialForCurrent = Element != nullptr;
		}

		if(Element == nullptr)
		{
			return false;
		}
	}

	const FTransform& Transform = bUndo ? InEntry.OldTransform : InEntry.NewTransform;
	
	switch(InEntry.EntryType)
	{
		case ERigTransformStackEntryType::TransformPose:
		{
			SetTransform(Cast<FRigTransformElement>(Element), Transform, InEntry.TransformType, InEntry.bAffectChildren, false);

			if(ERigTransformType::IsCurrent(InEntry.TransformType) && bApplyInitialForCurrent)
			{
				SetTransform(Cast<FRigTransformElement>(Element), Transform, ERigTransformType::MakeInitial(InEntry.TransformType), InEntry.bAffectChildren, false);
			}
			break;
		}
		case ERigTransformStackEntryType::ControlOffset:
		{
			SetControlOffsetTransform(Cast<FRigControlElement>(Element), Transform, InEntry.TransformType, InEntry.bAffectChildren, false); 
			break;
		}
		case ERigTransformStackEntryType::ControlShape:
		{
			SetControlShapeTransform(Cast<FRigControlElement>(Element), Transform, InEntry.TransformType, false); 
			break;
		}
		case ERigTransformStackEntryType::CurveValue:
		{
			const float CurveValue = Transform.GetTranslation().X;
			SetCurveValue(Cast<FRigCurveElement>(Element), CurveValue, false);
			break;
		}
	}

	return true;
#else
	return false;
#endif
}

void URigHierarchy::ComputeAllTransforms()
{
	for(int32 ElementIndex = 0; ElementIndex < Elements.Num(); ElementIndex++)
	{
		for(int32 TransformTypeIndex = 0; TransformTypeIndex < (int32) ERigTransformType::NumTransformTypes; TransformTypeIndex++)
		{
			const ERigTransformType::Type TransformType = (ERigTransformType::Type)TransformTypeIndex; 
			if(FRigControlElement* ControlElement = Get<FRigControlElement>(ElementIndex))
			{
				GetControlOffsetTransform(ControlElement, TransformType);
			}
			if(FRigTransformElement* TransformElement = Get<FRigTransformElement>(ElementIndex))
			{
				GetTransform(TransformElement, TransformType);
			}
			if(FRigControlElement* ControlElement = Get<FRigControlElement>(ElementIndex))
			{
				GetControlShapeTransform(ControlElement, TransformType);
			}
		}
	}
}

bool URigHierarchy::IsAnimatable(const FRigElementKey& InKey) const
{
	if(const FRigControlElement* ControlElement = Find<FRigControlElement>(InKey))
	{
		return IsAnimatable(ControlElement);
	}
	return false;
}

bool URigHierarchy::IsAnimatable(const FRigControlElement* InControlElement) const
{
	if(InControlElement)
	{
		if(!InControlElement->Settings.IsAnimatable())
		{
			return false;
		}

		// animation channels are dependent on the control they are under.
		if(InControlElement->IsAnimationChannel())
		{
			if(const FRigControlElement* ParentControlElement = Cast<FRigControlElement>(GetFirstParent(InControlElement)))
			{
				return IsAnimatable(ParentControlElement);
			}
		}
		
		return true;
	}
	return false;
}

bool URigHierarchy::ShouldBeGrouped(const FRigElementKey& InKey) const
{
	if(const FRigControlElement* ControlElement = Find<FRigControlElement>(InKey))
	{
		return ShouldBeGrouped(ControlElement);
	}
	return false;
}

bool URigHierarchy::ShouldBeGrouped(const FRigControlElement* InControlElement) const
{
	if(InControlElement)
	{
		if(!InControlElement->Settings.ShouldBeGrouped())
		{
			return false;
		}

		if(!GetChildren(InControlElement).IsEmpty())
		{
			return false;
		}

		if(const FRigControlElement* ParentControlElement = Cast<FRigControlElement>(GetFirstParent(InControlElement)))
		{
			return ParentControlElement->Settings.AnimationType == ERigControlAnimationType::AnimationControl;
		}
	}
	return false;
}

FTransform URigHierarchy::GetWorldTransformForReference(const FRigVMExecuteContext* InContext, const FRigElementKey& InKey, bool bInitial)
{
	if(const USceneComponent* OuterSceneComponent = GetTypedOuter<USceneComponent>())
	{
		return OuterSceneComponent->GetComponentToWorld().Inverse();
	}
	return FTransform::Identity;
}

FTransform URigHierarchy::ComputeLocalControlValue(FRigControlElement* ControlElement,
	const FTransform& InGlobalTransform, ERigTransformType::Type InTransformType) const
{
	check(ERigTransformType::IsGlobal(InTransformType));

	const FTransform OffsetTransform =
		GetControlOffsetTransform(ControlElement, ERigTransformType::MakeLocal(InTransformType));

	FTransform Result = InverseSolveParentConstraints(
		InGlobalTransform,
		ControlElement->ParentConstraints,
		InTransformType,
		OffsetTransform);

	return Result;
}

FTransform URigHierarchy::SolveParentConstraints(
	const FRigElementParentConstraintArray& InConstraints,
	const ERigTransformType::Type InTransformType,
	const FTransform& InLocalOffsetTransform,
	bool bApplyLocalOffsetTransform,
	const FTransform& InLocalPoseTransform,
	bool bApplyLocalPoseTransform) const
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));
	FTransform Result = FTransform::Identity;
	const bool bInitial = IsInitial(InTransformType);

	// collect all of the weights
	FConstraintIndex FirstConstraint;
	FConstraintIndex SecondConstraint;
	FConstraintIndex NumConstraintsAffecting(0);
	FRigElementWeight TotalWeight(0.f);
	ComputeParentConstraintIndices(InConstraints, InTransformType, FirstConstraint, SecondConstraint, NumConstraintsAffecting, TotalWeight);

	// performance improvement for case of a single parent
	if(NumConstraintsAffecting.Location == 1 &&
		NumConstraintsAffecting.Rotation == 1 &&
		NumConstraintsAffecting.Scale == 1 &&
		FirstConstraint.Location == FirstConstraint.Rotation &&
		FirstConstraint.Location == FirstConstraint.Scale)
	{
		return LazilyComputeParentConstraint(InConstraints, FirstConstraint.Location, InTransformType,
			InLocalOffsetTransform, bApplyLocalOffsetTransform, InLocalPoseTransform, bApplyLocalPoseTransform);
	}

	if(NumConstraintsAffecting.Location == 0 ||
		NumConstraintsAffecting.Rotation == 0 ||
		NumConstraintsAffecting.Scale == 0)
	{
		if(bApplyLocalOffsetTransform)
		{
			Result = InLocalOffsetTransform;
		}
		
		if(bApplyLocalPoseTransform)
		{
			Result = InLocalPoseTransform * Result;
		}

		if(NumConstraintsAffecting.Location == 0 &&
			NumConstraintsAffecting.Rotation == 0 &&
			NumConstraintsAffecting.Scale == 0)
		{
			Result.NormalizeRotation();
			return Result;
		}
	}

	if(NumConstraintsAffecting.Location == 1)
	{
		check(FirstConstraint.Location != INDEX_NONE);

		const FRigElementParentConstraint& ParentConstraint = InConstraints[FirstConstraint.Location];
		const FRigElementWeight& Weight = ParentConstraint.GetWeight(bInitial);
		const FTransform Transform = LazilyComputeParentConstraint(InConstraints, FirstConstraint.Location, InTransformType,
			InLocalOffsetTransform, bApplyLocalOffsetTransform, InLocalPoseTransform, bApplyLocalPoseTransform);

		check(Weight.AffectsLocation());
		Result.SetLocation(Transform.GetLocation());
	}
	else if(NumConstraintsAffecting.Location == 2)
	{
		check(FirstConstraint.Location != INDEX_NONE);
		check(SecondConstraint.Location != INDEX_NONE);

		const FRigElementParentConstraint& ParentConstraintA = InConstraints[FirstConstraint.Location];
		const FRigElementParentConstraint& ParentConstraintB = InConstraints[SecondConstraint.Location];

		const FRigElementWeight& WeightA = ParentConstraintA.GetWeight(bInitial); 
		const FRigElementWeight& WeightB = ParentConstraintB.GetWeight(bInitial);
		check(WeightA.AffectsLocation());
		check(WeightB.AffectsLocation());
		const float Weight = GetWeightForLerp(WeightA.Location, WeightB.Location);

		const FTransform TransformA = LazilyComputeParentConstraint(InConstraints, FirstConstraint.Location, InTransformType,
			InLocalOffsetTransform, bApplyLocalOffsetTransform, InLocalPoseTransform, bApplyLocalPoseTransform);
		const FTransform TransformB = LazilyComputeParentConstraint(InConstraints, SecondConstraint.Location, InTransformType,
			InLocalOffsetTransform, bApplyLocalOffsetTransform, InLocalPoseTransform, bApplyLocalPoseTransform);

		const FVector ParentLocationA = TransformA.GetLocation();
		const FVector ParentLocationB = TransformB.GetLocation();
		Result.SetLocation(FMath::Lerp<FVector>(ParentLocationA, ParentLocationB, Weight));
	}
	else if(NumConstraintsAffecting.Location > 2)
	{
		check(TotalWeight.Location > SMALL_NUMBER);
		
		FVector Location = FVector::ZeroVector;
		
		for(int32 ConstraintIndex = 0; ConstraintIndex < InConstraints.Num(); ConstraintIndex++)
		{
			const FRigElementParentConstraint& ParentConstraint = InConstraints[ConstraintIndex];
			const FRigElementWeight& Weight = ParentConstraint.GetWeight(bInitial);
			if(!Weight.AffectsLocation())
			{
				continue;
			}

			const FTransform Transform = LazilyComputeParentConstraint(InConstraints, ConstraintIndex, InTransformType,
				InLocalOffsetTransform, bApplyLocalOffsetTransform, InLocalPoseTransform, bApplyLocalPoseTransform);

			IntegrateParentConstraintVector(Location, Transform, Weight.Location / TotalWeight.Location, true);
		}

		Result.SetLocation(Location);
	}

	if(NumConstraintsAffecting.Rotation == 1)
	{
		check(FirstConstraint.Rotation != INDEX_NONE);

		const FRigElementParentConstraint& ParentConstraint = InConstraints[FirstConstraint.Rotation];
		const FRigElementWeight& Weight = ParentConstraint.GetWeight(bInitial);
		const FTransform Transform = LazilyComputeParentConstraint(InConstraints, FirstConstraint.Rotation, InTransformType,
			InLocalOffsetTransform, bApplyLocalOffsetTransform, InLocalPoseTransform, bApplyLocalPoseTransform);
		check(Weight.AffectsRotation());
		Result.SetRotation(Transform.GetRotation());
	}
	else if(NumConstraintsAffecting.Rotation == 2)
	{
		check(FirstConstraint.Rotation != INDEX_NONE);
		check(SecondConstraint.Rotation != INDEX_NONE);

		const FRigElementParentConstraint& ParentConstraintA = InConstraints[FirstConstraint.Rotation];
		const FRigElementParentConstraint& ParentConstraintB = InConstraints[SecondConstraint.Rotation];

		const FRigElementWeight& WeightA = ParentConstraintA.GetWeight(bInitial); 
		const FRigElementWeight& WeightB = ParentConstraintB.GetWeight(bInitial);
		check(WeightA.AffectsRotation());
		check(WeightB.AffectsRotation());
		const float Weight = GetWeightForLerp(WeightA.Rotation, WeightB.Rotation);

		const FTransform TransformA = LazilyComputeParentConstraint(InConstraints, FirstConstraint.Rotation, InTransformType,
			InLocalOffsetTransform, bApplyLocalOffsetTransform, InLocalPoseTransform, bApplyLocalPoseTransform);
		const FTransform TransformB = LazilyComputeParentConstraint(InConstraints, SecondConstraint.Rotation, InTransformType,
			InLocalOffsetTransform, bApplyLocalOffsetTransform, InLocalPoseTransform, bApplyLocalPoseTransform);

		const FQuat ParentRotationA = TransformA.GetRotation();
		const FQuat ParentRotationB = TransformB.GetRotation();
		Result.SetRotation(FQuat::Slerp(ParentRotationA, ParentRotationB, Weight));
	}
	else if(NumConstraintsAffecting.Rotation > 2)
	{
		check(TotalWeight.Rotation > SMALL_NUMBER);
		
		int NumMixedRotations = 0;
		FQuat FirstRotation = FQuat::Identity;
		FQuat MixedRotation = FQuat(0.f, 0.f, 0.f, 0.f);

		for(int32 ConstraintIndex = 0; ConstraintIndex < InConstraints.Num(); ConstraintIndex++)
		{
			const FRigElementParentConstraint& ParentConstraint = InConstraints[ConstraintIndex];
			const FRigElementWeight& Weight = ParentConstraint.GetWeight(bInitial);
			if(!Weight.AffectsRotation())
			{
				continue;
			}

			const FTransform Transform = LazilyComputeParentConstraint(InConstraints, ConstraintIndex, InTransformType,
				InLocalOffsetTransform, bApplyLocalOffsetTransform, InLocalPoseTransform, bApplyLocalPoseTransform);

			IntegrateParentConstraintQuat(
				NumMixedRotations,
				FirstRotation,
				MixedRotation,
				Transform,
				Weight.Rotation / TotalWeight.Rotation);
		}

		Result.SetRotation(MixedRotation.GetNormalized());
	}

	if(NumConstraintsAffecting.Scale == 1)
	{
		check(FirstConstraint.Scale != INDEX_NONE);

		const FRigElementParentConstraint& ParentConstraint = InConstraints[FirstConstraint.Scale];
		const FRigElementWeight& Weight = ParentConstraint.GetWeight(bInitial);
		
		const FTransform Transform = LazilyComputeParentConstraint(InConstraints, FirstConstraint.Scale, InTransformType,
			InLocalOffsetTransform, bApplyLocalOffsetTransform, InLocalPoseTransform, bApplyLocalPoseTransform);

		check(Weight.AffectsScale());
		Result.SetScale3D(Transform.GetScale3D());
	}
	else if(NumConstraintsAffecting.Scale == 2)
	{
		check(FirstConstraint.Scale != INDEX_NONE);
		check(SecondConstraint.Scale != INDEX_NONE);

		const FRigElementParentConstraint& ParentConstraintA = InConstraints[FirstConstraint.Scale];
		const FRigElementParentConstraint& ParentConstraintB = InConstraints[SecondConstraint.Scale];

		const FRigElementWeight& WeightA = ParentConstraintA.GetWeight(bInitial); 
		const FRigElementWeight& WeightB = ParentConstraintB.GetWeight(bInitial);
		check(WeightA.AffectsScale());
		check(WeightB.AffectsScale());
		const float Weight = GetWeightForLerp(WeightA.Scale, WeightB.Scale);

		const FTransform TransformA = LazilyComputeParentConstraint(InConstraints, FirstConstraint.Scale, InTransformType,
			InLocalOffsetTransform, bApplyLocalOffsetTransform, InLocalPoseTransform, bApplyLocalPoseTransform);
		const FTransform TransformB = LazilyComputeParentConstraint(InConstraints, SecondConstraint.Scale, InTransformType,
			InLocalOffsetTransform, bApplyLocalOffsetTransform, InLocalPoseTransform, bApplyLocalPoseTransform);

		const FVector ParentScaleA = TransformA.GetScale3D();
		const FVector ParentScaleB = TransformB.GetScale3D();
		Result.SetScale3D(FMath::Lerp<FVector>(ParentScaleA, ParentScaleB, Weight));
	}
	else if(NumConstraintsAffecting.Scale > 2)
	{
		check(TotalWeight.Scale > SMALL_NUMBER);
		
		FVector Scale = FVector::ZeroVector;
		
		for(int32 ConstraintIndex = 0; ConstraintIndex < InConstraints.Num(); ConstraintIndex++)
		{
			const FRigElementParentConstraint& ParentConstraint = InConstraints[ConstraintIndex];
			const FRigElementWeight& Weight = ParentConstraint.GetWeight(bInitial);
			if(!Weight.AffectsScale())
			{
				continue;
			}

			const FTransform Transform = LazilyComputeParentConstraint(InConstraints, ConstraintIndex, InTransformType,
				InLocalOffsetTransform, bApplyLocalOffsetTransform, InLocalPoseTransform, bApplyLocalPoseTransform);

			IntegrateParentConstraintVector(Scale, Transform, Weight.Scale / TotalWeight.Scale, false);
		}

		Result.SetScale3D(Scale);
	}

	Result.NormalizeRotation();
	return Result;
}

FTransform URigHierarchy::InverseSolveParentConstraints(
	const FTransform& InGlobalTransform,
	const FRigElementParentConstraintArray& InConstraints,
	const ERigTransformType::Type InTransformType,
	const FTransform& InLocalOffsetTransform) const
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));

	FTransform Result = FTransform::Identity;

	// this function is doing roughly the following 
	// ResultLocalTransform = InGlobalTransform.GetRelative(ParentGlobal)
	// InTransformType is only used to determine Initial vs Current
	const bool bInitial = IsInitial(InTransformType);
	check(ERigTransformType::IsGlobal(InTransformType));

	// collect all of the weights
	FConstraintIndex FirstConstraint;
	FConstraintIndex SecondConstraint;
	FConstraintIndex NumConstraintsAffecting(0);
	FRigElementWeight TotalWeight(0.f);
	ComputeParentConstraintIndices(InConstraints, InTransformType, FirstConstraint, SecondConstraint, NumConstraintsAffecting, TotalWeight);

	// performance improvement for case of a single parent
	if(NumConstraintsAffecting.Location == 1 &&
		NumConstraintsAffecting.Rotation == 1 &&
		NumConstraintsAffecting.Scale == 1 &&
		FirstConstraint.Location == FirstConstraint.Rotation &&
		FirstConstraint.Location == FirstConstraint.Scale)
	{
		const FTransform Transform = LazilyComputeParentConstraint(InConstraints, FirstConstraint.Location, InTransformType,
			InLocalOffsetTransform, true, FTransform::Identity, false);

		return InGlobalTransform.GetRelativeTransform(Transform);
	}

	if(NumConstraintsAffecting.Location == 0 ||
		NumConstraintsAffecting.Rotation == 0 ||
		NumConstraintsAffecting.Scale == 0)
	{
		Result = InGlobalTransform.GetRelativeTransform(InLocalOffsetTransform);
		
		if(NumConstraintsAffecting.Location == 0 &&
			NumConstraintsAffecting.Rotation == 0 &&
			NumConstraintsAffecting.Scale == 0)
		{
			Result.NormalizeRotation();
			return Result;
		}
	}

	if(NumConstraintsAffecting.Location == 1)
	{
		check(FirstConstraint.Location != INDEX_NONE);

		const FRigElementParentConstraint& ParentConstraint = InConstraints[FirstConstraint.Location];
		const FRigElementWeight& Weight = ParentConstraint.GetWeight(bInitial);
		const FTransform Transform = LazilyComputeParentConstraint(InConstraints, FirstConstraint.Location, InTransformType,
			InLocalOffsetTransform, true, FTransform::Identity, false);

		check(Weight.AffectsLocation());
		Result.SetLocation(InGlobalTransform.GetRelativeTransform(Transform).GetLocation());
	}
	else if(NumConstraintsAffecting.Location == 2)
	{
		check(FirstConstraint.Location != INDEX_NONE);
		check(SecondConstraint.Location != INDEX_NONE);

		const FRigElementParentConstraint& ParentConstraintA = InConstraints[FirstConstraint.Location];
		const FRigElementParentConstraint& ParentConstraintB = InConstraints[SecondConstraint.Location];

		const FRigElementWeight& WeightA = ParentConstraintA.GetWeight(bInitial); 
		const FRigElementWeight& WeightB = ParentConstraintB.GetWeight(bInitial);
		check(WeightA.AffectsLocation());
		check(WeightB.AffectsLocation());
		const float Weight = GetWeightForLerp(WeightA.Location, WeightB.Location);

		const FTransform TransformA = LazilyComputeParentConstraint(InConstraints, FirstConstraint.Location, InTransformType,
			InLocalOffsetTransform, true, FTransform::Identity, false);
		const FTransform TransformB = LazilyComputeParentConstraint(InConstraints, SecondConstraint.Location, InTransformType,
			InLocalOffsetTransform, true, FTransform::Identity, false);

		const FTransform MixedTransform = FControlRigMathLibrary::LerpTransform(TransformA, TransformB, Weight);
		Result.SetLocation(InGlobalTransform.GetRelativeTransform(MixedTransform).GetLocation());
	}
	else if(NumConstraintsAffecting.Location > 2)
	{
		check(TotalWeight.Location > SMALL_NUMBER);
		
		FVector Location = FVector::ZeroVector;
		int NumMixedRotations = 0;
		FQuat FirstRotation = FQuat::Identity;
		FQuat MixedRotation = FQuat(0.f, 0.f, 0.f, 0.f);
		FVector Scale = FVector::ZeroVector;

		for(int32 ConstraintIndex = 0; ConstraintIndex < InConstraints.Num(); ConstraintIndex++)
		{
			const FRigElementParentConstraint& ParentConstraint = InConstraints[ConstraintIndex];
			const FRigElementWeight& Weight = ParentConstraint.GetWeight(bInitial);
			if(!Weight.AffectsLocation())
			{
				continue;
			}

			const FTransform Transform = LazilyComputeParentConstraint(InConstraints, ConstraintIndex, InTransformType,
				InLocalOffsetTransform, true, FTransform::Identity, false);

			const float NormalizedWeight = Weight.Location / TotalWeight.Location;
			IntegrateParentConstraintVector(Location, Transform, NormalizedWeight, true);
			IntegrateParentConstraintQuat(NumMixedRotations, FirstRotation, MixedRotation, Transform, NormalizedWeight);
			IntegrateParentConstraintVector(Scale, Transform, NormalizedWeight, false);
		}

		FTransform ParentTransform(MixedRotation.GetNormalized(), Location, Scale);
		Result.SetLocation(InGlobalTransform.GetRelativeTransform(ParentTransform).GetLocation());
	}

	if(NumConstraintsAffecting.Rotation == 1)
	{
		check(FirstConstraint.Rotation != INDEX_NONE);

		const FRigElementParentConstraint& ParentConstraint = InConstraints[FirstConstraint.Rotation];
		const FRigElementWeight& Weight = ParentConstraint.GetWeight(bInitial);
		const FTransform Transform = LazilyComputeParentConstraint(InConstraints, FirstConstraint.Rotation, InTransformType,
			InLocalOffsetTransform, true, FTransform::Identity, false);
		check(Weight.AffectsRotation());
		Result.SetRotation(InGlobalTransform.GetRelativeTransform(Transform).GetRotation());
	}
	else if(NumConstraintsAffecting.Rotation == 2)
	{
		check(FirstConstraint.Rotation != INDEX_NONE);
		check(SecondConstraint.Rotation != INDEX_NONE);

		const FRigElementParentConstraint& ParentConstraintA = InConstraints[FirstConstraint.Rotation];
		const FRigElementParentConstraint& ParentConstraintB = InConstraints[SecondConstraint.Rotation];

		const FRigElementWeight& WeightA = ParentConstraintA.GetWeight(bInitial); 
		const FRigElementWeight& WeightB = ParentConstraintB.GetWeight(bInitial);
		check(WeightA.AffectsRotation());
		check(WeightB.AffectsRotation());
		const float Weight = GetWeightForLerp(WeightA.Rotation, WeightB.Rotation);

		const FTransform TransformA = LazilyComputeParentConstraint(InConstraints, FirstConstraint.Rotation, InTransformType,
			InLocalOffsetTransform, true, FTransform::Identity, false);
		const FTransform TransformB = LazilyComputeParentConstraint(InConstraints, SecondConstraint.Rotation, InTransformType,
			InLocalOffsetTransform, true, FTransform::Identity, false);

		const FTransform MixedTransform = FControlRigMathLibrary::LerpTransform(TransformA, TransformB, Weight);
		Result.SetRotation(InGlobalTransform.GetRelativeTransform(MixedTransform).GetRotation());
	}
	else if(NumConstraintsAffecting.Rotation > 2)
	{
		check(TotalWeight.Rotation > SMALL_NUMBER);
		
		FVector Location = FVector::ZeroVector;
		int NumMixedRotations = 0;
		FQuat FirstRotation = FQuat::Identity;
		FQuat MixedRotation = FQuat(0.f, 0.f, 0.f, 0.f);
		FVector Scale = FVector::ZeroVector;

		for(int32 ConstraintIndex = 0; ConstraintIndex < InConstraints.Num(); ConstraintIndex++)
		{
			const FRigElementParentConstraint& ParentConstraint = InConstraints[ConstraintIndex];
			const FRigElementWeight& Weight = ParentConstraint.GetWeight(bInitial);
			if(!Weight.AffectsRotation())
			{
				continue;
			}

			const FTransform Transform = LazilyComputeParentConstraint(InConstraints, ConstraintIndex, InTransformType,
				InLocalOffsetTransform, true, FTransform::Identity, false);

			const float NormalizedWeight = Weight.Rotation / TotalWeight.Rotation;
			IntegrateParentConstraintVector(Location, Transform, NormalizedWeight, true);
			IntegrateParentConstraintQuat(NumMixedRotations, FirstRotation, MixedRotation, Transform, NormalizedWeight);
			IntegrateParentConstraintVector(Scale, Transform, NormalizedWeight, false);
		}

		FTransform ParentTransform(MixedRotation.GetNormalized(), Location, Scale);
		Result.SetRotation(InGlobalTransform.GetRelativeTransform(ParentTransform).GetRotation());
	}

	if(NumConstraintsAffecting.Scale == 1)
	{
		check(FirstConstraint.Scale != INDEX_NONE);

		const FRigElementParentConstraint& ParentConstraint = InConstraints[FirstConstraint.Scale];
		const FRigElementWeight& Weight = ParentConstraint.GetWeight(bInitial);
		
		const FTransform Transform = LazilyComputeParentConstraint(InConstraints, FirstConstraint.Scale, InTransformType,
			InLocalOffsetTransform, true, FTransform::Identity, false);

		check(Weight.AffectsScale());
		Result.SetScale3D(InGlobalTransform.GetRelativeTransform(Transform).GetScale3D());
	}
	else if(NumConstraintsAffecting.Scale == 2)
	{
		check(FirstConstraint.Scale != INDEX_NONE);
		check(SecondConstraint.Scale != INDEX_NONE);

		const FRigElementParentConstraint& ParentConstraintA = InConstraints[FirstConstraint.Scale];
		const FRigElementParentConstraint& ParentConstraintB = InConstraints[SecondConstraint.Scale];

		const FRigElementWeight& WeightA = ParentConstraintA.GetWeight(bInitial); 
		const FRigElementWeight& WeightB = ParentConstraintB.GetWeight(bInitial);
		check(WeightA.AffectsScale());
		check(WeightB.AffectsScale());
		const float Weight = GetWeightForLerp(WeightA.Scale, WeightB.Scale);

		const FTransform TransformA = LazilyComputeParentConstraint(InConstraints, FirstConstraint.Scale, InTransformType,
			InLocalOffsetTransform, true, FTransform::Identity, false);
		const FTransform TransformB = LazilyComputeParentConstraint(InConstraints, SecondConstraint.Scale, InTransformType,
			InLocalOffsetTransform, true, FTransform::Identity, false);

		const FTransform MixedTransform = FControlRigMathLibrary::LerpTransform(TransformA, TransformB, Weight);
		Result.SetScale3D(InGlobalTransform.GetRelativeTransform(MixedTransform).GetScale3D());
	}
	else if(NumConstraintsAffecting.Scale > 2)
	{
		check(TotalWeight.Scale > SMALL_NUMBER);
		
		FVector Location = FVector::ZeroVector;
		int NumMixedRotations = 0;
		FQuat FirstRotation = FQuat::Identity;
		FQuat MixedRotation = FQuat(0.f, 0.f, 0.f, 0.f);
		FVector Scale = FVector::ZeroVector;
		
		for(int32 ConstraintIndex = 0; ConstraintIndex < InConstraints.Num(); ConstraintIndex++)
		{
			const FRigElementParentConstraint& ParentConstraint = InConstraints[ConstraintIndex];
			const FRigElementWeight& Weight = ParentConstraint.GetWeight(bInitial);
			if(!Weight.AffectsScale())
			{
				continue;
			}

			const FTransform Transform = LazilyComputeParentConstraint(InConstraints, ConstraintIndex, InTransformType,
				InLocalOffsetTransform, true, FTransform::Identity, false);

			const float NormalizedWeight = Weight.Scale / TotalWeight.Scale;
			IntegrateParentConstraintVector(Location, Transform, NormalizedWeight, true);
			IntegrateParentConstraintQuat(NumMixedRotations, FirstRotation, MixedRotation, Transform, NormalizedWeight);
			IntegrateParentConstraintVector(Scale, Transform, NormalizedWeight, false);
		}

		FTransform ParentTransform(MixedRotation.GetNormalized(), Location, Scale);
		Result.SetScale3D(InGlobalTransform.GetRelativeTransform(ParentTransform).GetScale3D());
	}

	Result.NormalizeRotation();
	return Result;
}

FTransform URigHierarchy::LazilyComputeParentConstraint(
	const FRigElementParentConstraintArray& InConstraints,
	int32 InIndex,
	const ERigTransformType::Type InTransformType,
	const FTransform& InLocalOffsetTransform,
	bool bApplyLocalOffsetTransform,
	const FTransform& InLocalPoseTransform,
	bool bApplyLocalPoseTransform) const
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));
	const FRigElementParentConstraint& Constraint = InConstraints[InIndex];
	if(Constraint.bCacheIsDirty)
	{
		FTransform Transform = GetTransform(Constraint.ParentElement, InTransformType);
		if(bApplyLocalOffsetTransform)
		{
			Transform = InLocalOffsetTransform * Transform;
		}
		if(bApplyLocalPoseTransform)
		{
			Transform = InLocalPoseTransform * Transform;
		}

		Transform.NormalizeRotation();
		Constraint.Cache.Transform = Transform;
		Constraint.bCacheIsDirty = false;
	}
	return Constraint.Cache.Transform;
}

void URigHierarchy::ComputeParentConstraintIndices(
	const FRigElementParentConstraintArray& InConstraints,
	ERigTransformType::Type InTransformType,
	FConstraintIndex& OutFirstConstraint,
	FConstraintIndex& OutSecondConstraint,
	FConstraintIndex& OutNumConstraintsAffecting,
	FRigElementWeight& OutTotalWeight)
{
	const bool bInitial = IsInitial(InTransformType);
	
	// find all of the weights affecting this output
	for(int32 ConstraintIndex = 0; ConstraintIndex < InConstraints.Num(); ConstraintIndex++)
	{
		// this is not relying on the cache whatsoever. we might as well remove it.
		InConstraints[ConstraintIndex].bCacheIsDirty = true;
		
		const FRigElementWeight& Weight = InConstraints[ConstraintIndex].GetWeight(bInitial);
		if(Weight.AffectsLocation())
		{
			OutNumConstraintsAffecting.Location++;
			OutTotalWeight.Location += Weight.Location;

			if(OutFirstConstraint.Location == INDEX_NONE)
			{
				OutFirstConstraint.Location = ConstraintIndex;
			}
			else if(OutSecondConstraint.Location == INDEX_NONE)
			{
				OutSecondConstraint.Location = ConstraintIndex;
			}
		}
		if(Weight.AffectsRotation())
		{
			OutNumConstraintsAffecting.Rotation++;
			OutTotalWeight.Rotation += Weight.Rotation;

			if(OutFirstConstraint.Rotation == INDEX_NONE)
			{
				OutFirstConstraint.Rotation = ConstraintIndex;
			}
			else if(OutSecondConstraint.Rotation == INDEX_NONE)
			{
				OutSecondConstraint.Rotation = ConstraintIndex;
			}
		}
		if(Weight.AffectsScale())
		{
			OutNumConstraintsAffecting.Scale++;
			OutTotalWeight.Scale += Weight.Scale;

			if(OutFirstConstraint.Scale == INDEX_NONE)
			{
				OutFirstConstraint.Scale = ConstraintIndex;
			}
			else if(OutSecondConstraint.Scale == INDEX_NONE)
			{
				OutSecondConstraint.Scale = ConstraintIndex;
			}
		}
	}
}
void URigHierarchy::IntegrateParentConstraintVector(
	FVector& OutVector,
	const FTransform& InTransform,
	float InWeight,
	bool bIsLocation)
{
	if(bIsLocation)
	{
		OutVector += InTransform.GetLocation() * InWeight;
	}
	else
	{
		OutVector += InTransform.GetScale3D() * InWeight;
	}
}

void URigHierarchy::IntegrateParentConstraintQuat(
	int32& OutNumMixedRotations,
	FQuat& OutFirstRotation,
	FQuat& OutMixedRotation,
	const FTransform& InTransform,
	float InWeight)
{
	LLM_SCOPE_BYNAME(TEXT("Animation/ControlRig"));
	FQuat ParentRotation = InTransform.GetRotation().GetNormalized();

	if(OutNumMixedRotations == 0)
	{
		OutFirstRotation = ParentRotation; 
	}
	else if ((ParentRotation | OutFirstRotation) <= 0.f)
	{
		InWeight = -InWeight;
	}

	OutMixedRotation.X += InWeight * ParentRotation.X;
	OutMixedRotation.Y += InWeight * ParentRotation.Y;
	OutMixedRotation.Z += InWeight * ParentRotation.Z;
	OutMixedRotation.W += InWeight * ParentRotation.W;
	OutNumMixedRotations++;
}

#if WITH_EDITOR
TArray<FString> URigHierarchy::ControlSettingsToPythonCommands(const FRigControlSettings& Settings, const FString& NameSettings)
{
	TArray<FString> Commands;
	Commands.Add(FString::Printf(TEXT("%s = unreal.RigControlSettings()"),
			*NameSettings));

	ERigControlType ControlType = Settings.ControlType;
	switch(ControlType)
	{
		case ERigControlType::Transform:
		case ERigControlType::TransformNoScale:
		{
			ControlType = ERigControlType::EulerTransform;
			break;
		}
		default:
		{
			break;
		}
	}

	const FString AnimationTypeStr = RigVMPythonUtils::EnumValueToPythonString<ERigControlAnimationType>((int64)Settings.AnimationType);
	const FString ControlTypeStr = RigVMPythonUtils::EnumValueToPythonString<ERigControlType>((int64)ControlType);

	static const TCHAR* TrueText = TEXT("True");
	static const TCHAR* FalseText = TEXT("False");

	TArray<FString> LimitEnabledParts;
	for(const FRigControlLimitEnabled& LimitEnabled : Settings.LimitEnabled)
	{
		LimitEnabledParts.Add(FString::Printf(TEXT("unreal.RigControlLimitEnabled(%s, %s)"),
						   LimitEnabled.bMinimum ? TrueText : FalseText,
						   LimitEnabled.bMaximum ? TrueText : FalseText));
	}
	
	const FString LimitEnabledStr = FString::Join(LimitEnabledParts, TEXT(", "));
	
	Commands.Add(FString::Printf(TEXT("%s.animation_type = %s"),
									*NameSettings,
									*AnimationTypeStr));
	Commands.Add(FString::Printf(TEXT("%s.control_type = %s"),
									*NameSettings,
									*ControlTypeStr));
	Commands.Add(FString::Printf(TEXT("%s.display_name = '%s'"),
		*NameSettings,
		*Settings.DisplayName.ToString()));
	Commands.Add(FString::Printf(TEXT("%s.draw_limits = %s"),
		*NameSettings,
		Settings.bDrawLimits ? TrueText : FalseText));
	Commands.Add(FString::Printf(TEXT("%s.shape_color = %s"),
		*NameSettings,
		*RigVMPythonUtils::LinearColorToPythonString(Settings.ShapeColor)));
	Commands.Add(FString::Printf(TEXT("%s.shape_name = '%s'"),
		*NameSettings,
		*Settings.ShapeName.ToString()));
	Commands.Add(FString::Printf(TEXT("%s.shape_visible = %s"),
		*NameSettings,
		Settings.bShapeVisible ? TrueText : FalseText));
	Commands.Add(FString::Printf(TEXT("%s.is_transient_control = %s"),
		*NameSettings,
		Settings.bIsTransientControl ? TrueText : FalseText));
	Commands.Add(FString::Printf(TEXT("%s.limit_enabled = [%s]"),
		*NameSettings,
		*LimitEnabledStr));
	Commands.Add(FString::Printf(TEXT("%s.minimum_value = %s"),
		*NameSettings,
		*Settings.MinimumValue.ToPythonString(Settings.ControlType)));
	Commands.Add(FString::Printf(TEXT("%s.maximum_value = %s"),
		*NameSettings,
		*Settings.MaximumValue.ToPythonString(Settings.ControlType)));
	Commands.Add(FString::Printf(TEXT("%s.primary_axis = %s"),
		*NameSettings,
		*RigVMPythonUtils::EnumValueToPythonString<ERigControlAxis>((int64)Settings.PrimaryAxis)));

	return Commands;
}

TArray<FString> URigHierarchy::ConnectorSettingsToPythonCommands(const FRigConnectorSettings& Settings, const FString& NameSettings)
{
	TArray<FString> Commands;
	Commands.Add(FString::Printf(TEXT("%s = unreal.RigConnectorSettings()"),
			*NameSettings));

	// no content values just yet - we are skipping the ResolvedItem here since
	// we don't assume it is going to be resolved initially.

	return Commands;
}

#endif

FRigHierarchyRedirectorGuard::FRigHierarchyRedirectorGuard(UControlRig* InControlRig)
: Guard(InControlRig->GetHierarchy()->ElementKeyRedirector, &InControlRig->GetElementKeyRedirector())
{
}
;