// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeInstanceData.h"
#include "StateTreeExecutionTypes.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "VisualLogger/VisualLogger.h"
#include "StateTree.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Serialization/CustomVersion.h"
#include "Serialization/PropertyLocalizationDataGathering.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StateTreeInstanceData)

const FGuid FStateTreeInstanceStorageCustomVersion::GUID(0x60C4F0DE, 0x8B264C34, 0xAA937201, 0x5DFF09CC);
FCustomVersionRegistration GRegisterStateTreeInstanceStorageCustomVersion(FStateTreeInstanceStorageCustomVersion::GUID, FStateTreeInstanceStorageCustomVersion::LatestVersion, TEXT("StateTreeInstanceStorage"));

namespace UE::StateTree
{

	/**
	 * Duplicates object, and tries to covert old BP classes (REINST_*) to their newer version.
	 */
	UObject* DuplicateNodeInstance(const UObject& Instance, UObject& InOwner)
	{
		const UClass* InstanceClass = Instance.GetClass();
		if (InstanceClass->HasAnyClassFlags(CLASS_NewerVersionExists))
		{
			const UClass* AuthoritativeClass = InstanceClass->GetAuthoritativeClass();
			UObject* NewInstance = NewObject<UObject>(&InOwner, AuthoritativeClass);

			// Try to copy the values over using serialization
			// FObjectAndNameAsStringProxyArchive is used to store and restore names and objects as memory writer does not support UObject references at all.
			TArray<uint8> Data;
			FMemoryWriter Writer(Data);
			FObjectAndNameAsStringProxyArchive WriterProxy(Writer, /*bInLoadIfFindFails*/true);
			UObject& NonConstInstance = const_cast<UObject&>(Instance);
			NonConstInstance.Serialize(WriterProxy);

			FMemoryReader Reader(Data);
			FObjectAndNameAsStringProxyArchive ReaderProxy(Reader, /*bInLoadIfFindFails*/true);
			NewInstance->Serialize(ReaderProxy);

			const UStateTree* OuterStateTree = Instance.GetTypedOuter<UStateTree>();

			UE_LOG(LogStateTree, Display, TEXT("FStateTreeInstanceData: Duplicating '%s' with old class '%s' as '%s', potential data loss. Please resave State Tree asset %s."),
				*GetFullNameSafe(&Instance), *GetNameSafe(InstanceClass), *GetNameSafe(AuthoritativeClass), *GetFullNameSafe(OuterStateTree));

			return NewInstance;
		}

		return DuplicateObject(&Instance, &InOwner);
	}

#if WITH_EDITORONLY_DATA
	void GatherForLocalization(const FString& PathToParent, const UScriptStruct* Struct, const void* StructData, const void* DefaultStructData, FPropertyLocalizationDataGatherer& PropertyLocalizationDataGatherer, const EPropertyLocalizationGathererTextFlags GatherTextFlags)
	{
		const FStateTreeInstanceData* ThisInstance = static_cast<const FStateTreeInstanceData*>(StructData);
		const FStateTreeInstanceData* DefaultInstance = static_cast<const FStateTreeInstanceData*>(DefaultStructData);

		PropertyLocalizationDataGatherer.GatherLocalizationDataFromStruct(PathToParent, Struct, StructData, DefaultStructData, GatherTextFlags);

		const uint8* DefaultInstanceMemory = nullptr;
		if (DefaultInstance)
		{
			DefaultInstanceMemory = reinterpret_cast<const uint8*>(&DefaultInstance->GetStorage());
		}
		
		const UScriptStruct* StructTypePtr = FStateTreeInstanceStorage::StaticStruct();
		PropertyLocalizationDataGatherer.GatherLocalizationDataFromStructWithCallbacks(PathToParent + TEXT(".InstanceStorage"), StructTypePtr, &ThisInstance->GetStorage(), DefaultInstanceMemory, GatherTextFlags);
	}

	void RegisterInstanceDataForLocalization()
	{
		{ static const FAutoRegisterLocalizationDataGatheringCallback AutomaticRegistrationOfLocalizationGatherer(TBaseStructure<FStateTreeInstanceData>::Get(), &GatherForLocalization); }
	}
#endif // WITH_EDITORONLY_DATA

} // UE::StateTree


//----------------------------------------------------------------//
// FStateTreeInstanceStorage
//----------------------------------------------------------------//

void FStateTreeInstanceStorage::AddTransitionRequest(const UObject* Owner, const FStateTreeTransitionRequest& Request)
{
	constexpr int32 MaxPendingTransitionRequests = 32;
	
	if (TransitionRequests.Num() >= MaxPendingTransitionRequests)
	{
		UE_VLOG_UELOG(Owner, LogStateTree, Error, TEXT("%s: Too many transition requests sent to '%s' (%d pending). Dropping request."), ANSI_TO_TCHAR(__FUNCTION__), *GetNameSafe(Owner), TransitionRequests.Num());
		return;
	}

	TransitionRequests.Add(Request);
}

void FStateTreeInstanceStorage::ResetTransitionRequests()
{
	TransitionRequests.Reset();
}

bool FStateTreeInstanceStorage::AreAllInstancesValid() const
{
	for (FConstStructView Instance : InstanceStructs)
	{
		if (!Instance.IsValid())
		{
			return false;
		}
		if (const FStateTreeInstanceObjectWrapper* Wrapper = Instance.GetPtr<const FStateTreeInstanceObjectWrapper>())
		{
			if (!Wrapper->InstanceObject)
			{
				return false;
			}
		}
	}
	return true;
}

FStructView FStateTreeInstanceStorage::AddTemporaryInstance(UObject& InOwner, const FStateTreeExecutionFrame& Frame, const FStateTreeIndex16 OwnerNodeIndex, const FStateTreeDataHandle DataHandle, FConstStructView NewInstanceData)
{
	FStateTreeTemporaryInstanceData* TempInstance = TemporaryInstances.FindByPredicate([&Frame, &OwnerNodeIndex, &DataHandle](const FStateTreeTemporaryInstanceData& TempInstance)
	{
		return TempInstance.StateTree == Frame.StateTree
				&& TempInstance.RootState == Frame.RootState
				&& TempInstance.OwnerNodeIndex == OwnerNodeIndex
				&& TempInstance.DataHandle == DataHandle;
	});
	
	if (TempInstance)
	{
		if (TempInstance->Instance.GetScriptStruct() != NewInstanceData.GetScriptStruct())
		{
			TempInstance->Instance = NewInstanceData;
		}
	}
	else
	{
		TempInstance = &TemporaryInstances.AddDefaulted_GetRef();
		check(TempInstance);
		TempInstance->StateTree = Frame.StateTree;
		TempInstance->RootState = Frame.RootState;
		TempInstance->OwnerNodeIndex = OwnerNodeIndex;
		TempInstance->DataHandle = DataHandle;
		TempInstance->Instance = NewInstanceData;
	}

	if (FStateTreeInstanceObjectWrapper* Wrapper = TempInstance->Instance.GetMutablePtr<FStateTreeInstanceObjectWrapper>())
	{
		if (Wrapper->InstanceObject)
		{
			Wrapper->InstanceObject = UE::StateTree::DuplicateNodeInstance(*Wrapper->InstanceObject, InOwner);
		}
	}

	return TempInstance->Instance;
}

FStructView FStateTreeInstanceStorage::GetMutableTemporaryStruct(const FStateTreeExecutionFrame& Frame, const FStateTreeDataHandle DataHandle)
{
	FStateTreeTemporaryInstanceData* ExistingInstance = TemporaryInstances.FindByPredicate([&Frame, &DataHandle](const FStateTreeTemporaryInstanceData& TempInstance)
	{
		return TempInstance.StateTree == Frame.StateTree
				&& TempInstance.RootState == Frame.RootState
				&& TempInstance.DataHandle == DataHandle;
	});
	return ExistingInstance ? FStructView(ExistingInstance->Instance) : FStructView();
}

UObject* FStateTreeInstanceStorage::GetMutableTemporaryObject(const FStateTreeExecutionFrame& Frame, const FStateTreeDataHandle DataHandle)
{
	FStateTreeTemporaryInstanceData* ExistingInstance = TemporaryInstances.FindByPredicate([&Frame, &DataHandle](const FStateTreeTemporaryInstanceData& TempInstance)
	{
		return TempInstance.StateTree == Frame.StateTree
				&& TempInstance.RootState == Frame.RootState
				&& TempInstance.DataHandle == DataHandle;
	});
	if (ExistingInstance)
	{
		const FStateTreeInstanceObjectWrapper& Wrapper = ExistingInstance->Instance.Get<FStateTreeInstanceObjectWrapper>();
		return Wrapper.InstanceObject;
	}
	return nullptr;
}

void FStateTreeInstanceStorage::ResetTemporaryInstances()
{
	TemporaryInstances.Reset();
}


void FStateTreeInstanceStorage::SetGlobalParameters(const FInstancedPropertyBag& Parameters)
{
	GlobalParameters = Parameters;
}

//----------------------------------------------------------------//
// FStateTreeInstanceData
//----------------------------------------------------------------//

FStateTreeInstanceData::FStateTreeInstanceData() = default;

FStateTreeInstanceData::FStateTreeInstanceData(const FStateTreeInstanceData& Other)
{
	InstanceStorage = MakeShared<FStateTreeInstanceStorage>(*Other.InstanceStorage);
}

FStateTreeInstanceData::FStateTreeInstanceData(FStateTreeInstanceData&& Other)
{
	InstanceStorage = Other.InstanceStorage;
	Other.InstanceStorage = MakeShared<FStateTreeInstanceStorage>();
}

FStateTreeInstanceData& FStateTreeInstanceData::operator=(const FStateTreeInstanceData& Other)
{
	InstanceStorage = MakeShared<FStateTreeInstanceStorage>(*Other.InstanceStorage);
	return *this;
}

FStateTreeInstanceData& FStateTreeInstanceData::operator=(FStateTreeInstanceData&& Other)
{
	InstanceStorage = Other.InstanceStorage;
	Other.InstanceStorage = MakeShared<FStateTreeInstanceStorage>();
	return *this;
}

FStateTreeInstanceData::~FStateTreeInstanceData()
{
	Reset();
}

const FStateTreeInstanceStorage& FStateTreeInstanceData::GetStorage() const
{
	return *InstanceStorage;
}

TWeakPtr<FStateTreeInstanceStorage> FStateTreeInstanceData::GetWeakMutableStorage()
{
	return InstanceStorage;
}

TWeakPtr<const FStateTreeInstanceStorage> FStateTreeInstanceData::GetWeakStorage() const
{
	return InstanceStorage;
}

FStateTreeInstanceStorage& FStateTreeInstanceData::GetMutableStorage()
{
	return *InstanceStorage;
}

FStateTreeEventQueue& FStateTreeInstanceData::GetMutableEventQueue()
{
	return GetMutableStorage().EventQueue;	
}

const FStateTreeEventQueue& FStateTreeInstanceData::GetEventQueue() const
{
	return GetStorage().EventQueue;
}

void FStateTreeInstanceData::AddTransitionRequest(const UObject* Owner, const FStateTreeTransitionRequest& Request)
{
	GetMutableStorage().AddTransitionRequest(Owner, Request);
}

TConstArrayView<FStateTreeTransitionRequest> FStateTreeInstanceData::GetTransitionRequests() const
{
	return GetStorage().GetTransitionRequests();
}

void FStateTreeInstanceData::ResetTransitionRequests()
{
	GetMutableStorage().ResetTransitionRequests();
}

bool FStateTreeInstanceData::AreAllInstancesValid() const
{
	return GetStorage().AreAllInstancesValid();
}

int32 FStateTreeInstanceData::GetEstimatedMemoryUsage() const
{
	const FStateTreeInstanceStorage& Storage = GetStorage();
	int32 Size = sizeof(FStateTreeInstanceData);

	Size += Storage.InstanceStructs.GetAllocatedMemory();

	for (FConstStructView Instance : Storage.InstanceStructs)
	{
		if (const FStateTreeInstanceObjectWrapper* Wrapper = Instance.GetPtr<const FStateTreeInstanceObjectWrapper>())
		{
			if (Wrapper->InstanceObject)
			{
				Size += Wrapper->InstanceObject->GetClass()->GetStructureSize();
			}
		}
	}

	return Size;
}

bool FStateTreeInstanceData::Identical(const FStateTreeInstanceData* Other, uint32 PortFlags) const
{
	if (Other == nullptr)
	{
		return false;
	}

	const FStateTreeInstanceStorage& Storage = GetStorage();
	const FStateTreeInstanceStorage& OtherStorage = Other->GetStorage();

	// Not identical if global parameters don't match.
	if (!Storage.GlobalParameters.Identical(&OtherStorage.GlobalParameters, PortFlags))
	{
		return false;
	}

	// Not identical if structs are different.
	if (Storage.InstanceStructs.Identical(&OtherStorage.InstanceStructs, PortFlags) == false)
	{
		return false;
	}
	
	// Check that the instance object contents are identical.
	// Copied from object property.
	auto AreObjectsIdentical = [](UObject* A, UObject* B, uint32 PortFlags) -> bool
	{
		if ((PortFlags & PPF_DuplicateForPIE) != 0)
		{
			return false;
		}

		if (A == B)
		{
			return true;
		}

		// Resolve the object handles and run the deep comparison logic 
		if ((PortFlags & (PPF_DeepCompareInstances | PPF_DeepComparison)) != 0)
		{
			return FObjectPropertyBase::StaticIdentical(A, B, PortFlags);
		}

		return true;
	};

	bool bResult = true;

	for (int32 Index = 0; Index < Storage.InstanceStructs.Num(); Index++)
	{
		const FStateTreeInstanceObjectWrapper* Wrapper = Storage.InstanceStructs[Index].GetPtr<const FStateTreeInstanceObjectWrapper>();
		const FStateTreeInstanceObjectWrapper* OtherWrapper = OtherStorage.InstanceStructs[Index].GetPtr<const FStateTreeInstanceObjectWrapper>();

		if (Wrapper)
		{
			if (!OtherWrapper)
			{
				bResult = false;
				break;
			}
			if (Wrapper->InstanceObject && OtherWrapper->InstanceObject)
			{
				if (!AreObjectsIdentical(Wrapper->InstanceObject, OtherWrapper->InstanceObject, PortFlags))
				{
					bResult = false;
					break;
				}
			}
		}
	}
	
	return bResult;
}

void FStateTreeInstanceData::AddStructReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddPropertyReferencesWithStructARO(FStateTreeInstanceStorage::StaticStruct(), &GetMutableStorage());
}

bool FStateTreeInstanceData::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FStateTreeInstanceStorageCustomVersion::GUID);

	if (Ar.IsLoading())
	{
		if (Ar.CustomVer(FStateTreeInstanceStorageCustomVersion::GUID) < FStateTreeInstanceStorageCustomVersion::AddedCustomSerialization)
		{
#if WITH_EDITORONLY_DATA
PRAGMA_DISABLE_DEPRECATION_WARNINGS
			StaticStruct()->SerializeTaggedProperties(Ar, (uint8*)this, StaticStruct(), nullptr);

			if (InstanceStorage_DEPRECATED.IsValid())
			{
				InstanceStorage = MakeShared<FStateTreeInstanceStorage>(MoveTemp(InstanceStorage_DEPRECATED.GetMutable()));
				InstanceStorage_DEPRECATED.Reset();
				return true;
			}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif // WITH_EDITORONLY_DATA

			InstanceStorage = MakeShared<FStateTreeInstanceStorage>();
			return true;
		}

		InstanceStorage = MakeShared<FStateTreeInstanceStorage>();
	}

	FStateTreeInstanceStorage::StaticStruct()->SerializeItem(Ar, &InstanceStorage.Get(), nullptr);

	return true;
}

void FStateTreeInstanceData::GetPreloadDependencies(TArray<UObject*>& OutDeps)
{
	UScriptStruct* ScriptStruct = FStateTreeInstanceStorage::StaticStruct();
	OutDeps.Add(ScriptStruct);

	if (UScriptStruct::ICppStructOps* CppStructOps = ScriptStruct->GetCppStructOps())
	{
		CppStructOps->GetPreloadDependencies(&GetMutableStorage(), OutDeps);
	}

	for (TPropertyValueIterator<FStructProperty> It(ScriptStruct, &GetMutableStorage()); It; ++It)
	{
		const UScriptStruct* StructType = It.Key()->Struct;
		if (UScriptStruct::ICppStructOps* CppStructOps = StructType->GetCppStructOps())
		{
			void* StructDataPtr = const_cast<void*>(It.Value());
			CppStructOps->GetPreloadDependencies(StructDataPtr, OutDeps);
		}
	}
}

void FStateTreeInstanceData::CopyFrom(UObject& InOwner, const FStateTreeInstanceData& InOther)
{
	if (&InOther == this)
	{
		return;
	}

	FStateTreeInstanceStorage& Storage = GetMutableStorage();
	const FStateTreeInstanceStorage& OtherStorage = InOther.GetStorage();

	// Copy structs
	Storage.InstanceStructs = OtherStorage.InstanceStructs;

	// Copy instance objects.
	for (FStructView Instance : Storage.InstanceStructs)
	{
		if (FStateTreeInstanceObjectWrapper* Wrapper = Instance.GetPtr<FStateTreeInstanceObjectWrapper>())
		{
			if (Wrapper->InstanceObject)
			{
				Wrapper->InstanceObject = UE::StateTree::DuplicateNodeInstance(*Wrapper->InstanceObject, InOwner);
			}
		}
	}
}

void FStateTreeInstanceData::Init(UObject& InOwner, TConstArrayView<FInstancedStruct> InStructs)
{
	Reset();
	Append(InOwner, InStructs);
}

void FStateTreeInstanceData::Init(UObject& InOwner, TConstArrayView<FConstStructView> InStructs)
{
	Reset();
	Append(InOwner, InStructs);
}

void FStateTreeInstanceData::Append(UObject& InOwner, TConstArrayView<FInstancedStruct> InStructs)
{
	FStateTreeInstanceStorage& Storage = GetMutableStorage();

	const int32 StartIndex = Storage.InstanceStructs.Num();
	Storage.InstanceStructs.Append(InStructs);

	for (int32 Index = StartIndex; Index < Storage.InstanceStructs.Num(); Index++)
	{
		if (FStateTreeInstanceObjectWrapper* Wrapper = Storage.InstanceStructs[Index].GetPtr<FStateTreeInstanceObjectWrapper>())
		{
			if (Wrapper->InstanceObject)
			{
				Wrapper->InstanceObject = UE::StateTree::DuplicateNodeInstance(*Wrapper->InstanceObject, InOwner);
			}
		}
	}
}

void FStateTreeInstanceData::Append(UObject& InOwner, TConstArrayView<FConstStructView> InStructs)
{
	FStateTreeInstanceStorage& Storage = GetMutableStorage();

	const int32 StartIndex = Storage.InstanceStructs.Num();
	Storage.InstanceStructs.Append(InStructs);
	
	for (int32 Index = StartIndex; Index < Storage.InstanceStructs.Num(); Index++)
	{
		if (FStateTreeInstanceObjectWrapper* Wrapper = Storage.InstanceStructs[Index].GetPtr<FStateTreeInstanceObjectWrapper>())
		{
			if (Wrapper->InstanceObject)
			{
				Wrapper->InstanceObject = UE::StateTree::DuplicateNodeInstance(*Wrapper->InstanceObject, InOwner);
			}
		}
	}
}

void FStateTreeInstanceData::Append(UObject& InOwner, TConstArrayView<FConstStructView> InStructs, TConstArrayView<FInstancedStruct*> InInstancesToMove)
{
	check(InStructs.Num() == InInstancesToMove.Num());
	
	FStateTreeInstanceStorage& Storage = GetMutableStorage();

	const int32 StartIndex = Storage.InstanceStructs.Num();
	Storage.InstanceStructs.Append(InStructs);

	for (int32 Index = StartIndex; Index < Storage.InstanceStructs.Num(); Index++)
	{
		FStructView Struct = Storage.InstanceStructs[Index];
		FInstancedStruct* Source = InInstancesToMove[Index - StartIndex];

		// The source is used to move temporary instance data into instance data. Not all entries may have it.
		// The instance struct can be empty, in which case the temporary instance is ignored.
		// If the source is specified, move it to the instance data.
		// We assume that if the source is object wrapper, it is already the instance we want.
		if (Struct.IsValid()
			&& (Source && Source->IsValid()))
		{
			check(Struct.GetScriptStruct() == Source->GetScriptStruct());
				
			FMemory::Memswap(Struct.GetMemory(), Source->GetMutableMemory(), Struct.GetScriptStruct()->GetStructureSize());
			Source->Reset();
		}
		else if (FStateTreeInstanceObjectWrapper* Wrapper = Struct.GetPtr<FStateTreeInstanceObjectWrapper>())
		{
			if (Wrapper->InstanceObject)
			{
				Wrapper->InstanceObject = UE::StateTree::DuplicateNodeInstance(*Wrapper->InstanceObject, InOwner);
			}
		}
	}
}

void FStateTreeInstanceData::ShrinkTo(const int32 NumStructs)
{
	FStateTreeInstanceStorage& Storage = GetMutableStorage();
	check(NumStructs <= Storage.InstanceStructs.Num());  
	Storage.InstanceStructs.SetNum(NumStructs);
}

void FStateTreeInstanceData::Reset()
{
	FStateTreeInstanceStorage& Storage = GetMutableStorage();
	Storage.InstanceStructs.Reset();
	Storage.EventQueue.Reset();
	Storage.ExecutionState.Reset();
	Storage.TemporaryInstances.Reset();
	Storage.GlobalParameters.Reset();
}
