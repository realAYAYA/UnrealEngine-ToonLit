// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/ObjectHandle.h"
#include "Async/TaskGraphInterfaces.h"
#include "HAL/CriticalSection.h"
#include "HAL/PlatformAtomics.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeRWLock.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "UObject/Class.h"
#include "UObject/Linker.h"
#include "UObject/LinkerLoad.h"
#include "UObject/ObjectRedirector.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"

DEFINE_LOG_CATEGORY(LogObjectHandle);

FName FObjectRef::GetFName() const
{
	if (PackageName == NAME_None)
	{
		return NAME_None;
	}

	FObjectPathId::ResolvedNameContainerType ResolvedNames;
	ObjectPath.Resolve(ResolvedNames);

	if (ResolvedNames.Num() > 0)
	{
		const FName& Name = ResolvedNames[ResolvedNames.Num() - 1];
		return Name;
	}

	return NAME_None;
}

void FObjectRef::AppendClassPathName(FStringBuilderBase& OutClassPathNameBuilder, EObjectFullNameFlags Flags) const
{
	if (EnumHasAllFlags(Flags, EObjectFullNameFlags::IncludeClassPackage))
	{
		ClassPackageName.AppendString(OutClassPathNameBuilder);
		OutClassPathNameBuilder << TEXT(".");
	}

	ClassName.AppendString(OutClassPathNameBuilder);
}

void FObjectRef::AppendPathName(FStringBuilderBase& OutPathNameBuilder) const
{
	if (PackageName == NAME_None)
	{
		OutPathNameBuilder.Append(TEXT("None"));
		return;
	}

	PackageName.ToString(OutPathNameBuilder);

	FObjectPathId::ResolvedNameContainerType ResolvedNames;
	ObjectPath.Resolve(ResolvedNames);

	for (int Index = 0; Index < ResolvedNames.Num(); ++Index)
	{
		if (Index == 1)
		{
			OutPathNameBuilder << SUBOBJECT_DELIMITER_CHAR;
		}
		else
		{
			OutPathNameBuilder << TEXT(".");
		}

		const FName& Name = ResolvedNames[Index];
		Name.AppendString(OutPathNameBuilder);
	}
}

//===========================================================================

bool operator==(const FObjectHandleDataClassDescriptor& Lhs, const FObjectHandleDataClassDescriptor& Rhs)
{
	return (Lhs.PackageName == Rhs.PackageName) && (Lhs.ClassName == Rhs.ClassName);
}

namespace ObjectHandle_Private
{
	class FPackageId
	{
		static constexpr uint32 InvalidId = ~uint32(0u);
		uint32 Id = InvalidId;

		inline explicit FPackageId(int32 InId) : Id(InId) {}

	public:
		FPackageId() = default;

		inline static FPackageId FromIndex(uint32 Index)
		{
			return FPackageId(Index);
		}

		inline bool IsValid() const
		{
			return Id != InvalidId;
		}

		inline uint32 ToIndex() const
		{
			check(Id != InvalidId);
			return Id;
		}

		inline bool operator==(FPackageId Other) const
		{
			return Id == Other.Id;
		}
	};
	enum class EObjectId: uint32
	{
		Invalid = 0,
	};

	union FObjectId
	{
	private:
		uint32 RawData;
	public:
		struct
		{
			uint32 DataClassDescriptorId : 8;
			uint32 ObjectPathId : 24;
		} Components;

		FObjectId() = default;
		FObjectId(EObjectId Id) : RawData(static_cast<uint32>(Id)) {}

		bool operator==(EObjectId Id) { return RawData == static_cast<uint32>(Id); }
		bool operator!=(EObjectId Id) { return RawData != static_cast<uint32>(Id); }
	};

	static_assert(sizeof(FObjectId) == sizeof(uint32), "FObjectId type must always compile to something equivalent to a uint32 size.");

	struct FObjectHandlePackageData
	{
		FMinimalName PackageName;
		TArray<FObjectPathId> ObjectPaths;
		TArray<FObjectHandleDataClassDescriptor> DataClassDescriptors;
		TMap<FObjectPathId, FObjectId> PathToObjectId;
		FRWLock Lock;
	};

	static_assert(STRUCT_OFFSET(FObjectHandlePackageData, PackageName) == STRUCT_OFFSET(FObjectHandlePackageDebugData, PackageName), "FObjectHandlePackageData and FObjectHandlePackageDebugData must match in position of PackageNameField.");
	static_assert(STRUCT_OFFSET(FObjectHandlePackageData, ObjectPaths) == STRUCT_OFFSET(FObjectHandlePackageDebugData, ObjectPaths), "FObjectHandlePackageData and FObjectHandlePackageDebugData must match in position of ObjectPaths.");
	static_assert(STRUCT_OFFSET(FObjectHandlePackageData, DataClassDescriptors) == STRUCT_OFFSET(FObjectHandlePackageDebugData, DataClassDescriptors), "FObjectHandlePackageData and FObjectHandlePackageDebugData must match in position of DataClassDescriptors.");
	static_assert(sizeof(FObjectHandlePackageData) == sizeof(FObjectHandlePackageDebugData), "FObjectHandlePackageData and FObjectHandlePackageDebugData must match in size.");

	struct FObjectHandleIndex
	{
		FRWLock Lock; // @TODO: OBJPTR: Want to change this to a striped lock per object bucket to allow more concurrency when adding and looking up objects in a package
		TMap<FMinimalName, FPackageId> NameToPackageId;
		TArray<FObjectHandlePackageData> PackageData;
	} GObjectHandleIndex;

#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE

	static FAutoConsoleCommand CmdPrintUnresolvedObjects(
		TEXT("LazyLoad.PrintUnresolvedObjects"),
		TEXT("Prints a list of all unresolved objects from the object handle index."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
			{
				FRWScopeLock GlobalLockScope(GObjectHandleIndex.Lock, SLT_ReadOnly);
				const int32 TotalObjectHandlePackages = GObjectHandleIndex.PackageData.Num();
				TArray<FName> LoadedPackages;
				TArray<FName> UnloadedPackages;
				for (int32 PackageIndex = 0; PackageIndex < TotalObjectHandlePackages; PackageIndex++)
				{
					FName PackageName;
					{
						FObjectHandlePackageData& PackageData = GObjectHandleIndex.PackageData[PackageIndex];
						FRWScopeLock LocalLockScope(PackageData.Lock, SLT_ReadOnly);
						PackageName = MinimalNameToName(PackageData.PackageName);
					}

					UPackage* Package = FindObjectFast<UPackage>(nullptr, PackageName);
					if (Package)
					{
						LoadedPackages.Add(PackageName);
					}
					else
					{
						UnloadedPackages.Add(PackageName);
					}
				}

				UnloadedPackages.Sort(FNameLexicalLess());
				for (FName PackageName : UnloadedPackages)
				{
					UE_LOG(LogUObjectGlobals, Log, TEXT("Unloaded %s"), *PackageName.ToString());
				}

				LoadedPackages.Sort(FNameLexicalLess());
				for (FName PackageName : LoadedPackages)
				{
					UE_LOG(LogUObjectGlobals, Log, TEXT("Loaded %s"), *PackageName.ToString());
				}

				UE_LOG(LogUObjectGlobals, Log, TEXT("Unloaded Packages (%d out of %d)"), UnloadedPackages.Num(), LoadedPackages.Num() + UnloadedPackages.Num());
			})
	);

#endif

	static inline FPackedObjectRef Pack(FPackageId PackageId, FObjectId ObjectId)
	{
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE || UE_WITH_OBJECT_HANDLE_TRACKING
		checkf(PackageId.ToIndex() <= 0x7FFFFFFF, TEXT("Package count exceeded the space permitted within packed object references.  This implies over 2 billion packages are in use."));
		return {static_cast<UPTRINT>(PackageId.ToIndex()) << PackageIdShift |
				static_cast<UPTRINT>(ObjectId.Components.DataClassDescriptorId) << DataClassDescriptorIdShift |
				static_cast<UPTRINT>(ObjectId.Components.ObjectPathId) << ObjectPathIdShift |
				1};
#else
		unimplemented();
		return {0};
#endif
	}

	static inline void Unpack(FPackedObjectRef PackedObjectRef, FPackageId& OutPackageId, FObjectId& OutObjectId)
	{
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE || UE_WITH_OBJECT_HANDLE_TRACKING
		checkf((PackedObjectRef.EncodedRef & 1) == 1, TEXT("Packed object reference is malformed."));
		OutObjectId.Components.ObjectPathId = (static_cast<uint32>((PackedObjectRef.EncodedRef >> ObjectPathIdShift) & ObjectPathIdMask));
		OutObjectId.Components.DataClassDescriptorId = (static_cast<uint32>((PackedObjectRef.EncodedRef >> DataClassDescriptorIdShift) & DataClassDescriptorIdMask));
		OutPackageId = FPackageId::FromIndex(static_cast<uint32>((PackedObjectRef.EncodedRef >> PackageIdShift) & PackageIdMask));
#else
		unimplemented();
		OutPackageId = FPackageId();
		OutObjectId = FObjectId(EObjectId::Invalid);
#endif
	}

	static void MakeReferenceIds(FName PackageName, FName ClassPackageName, FName ClassName, FObjectPathId ObjectPath, FPackageId& OutPackageId, FObjectId& OutObjectId)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ObjectHandle_Private::MakeReferenceIds);
		FMinimalName MinimalName = NameToMinimalName(PackageName);

		//Biases for read-only locking by default at the expense of having to do a second map search if a write is necessary
		//Doing one search with either a read or locked write seems impossible with the TMap interface at the moment.
		FRWScopeLock GlobalLockScope(GObjectHandleIndex.Lock, SLT_ReadOnly);
		FPackageId* FoundPackageId = GObjectHandleIndex.NameToPackageId.Find(MinimalName);
		FObjectHandlePackageData* PackageData = nullptr;
		if (!FoundPackageId)
		{
			GlobalLockScope.ReleaseReadOnlyLockAndAcquireWriteLock_USE_WITH_CAUTION();
			//Has to be FindOrAdd, as the NameToPackageId may have changed between relinquishing the read lock
			//and acquiring the write lock.
			FPackageId NextId = FPackageId::FromIndex(GObjectHandleIndex.PackageData.Num());
			FoundPackageId = &GObjectHandleIndex.NameToPackageId.FindOrAdd(MinimalName, NextId);
			if (*FoundPackageId == NextId)
			{
				PackageData = &GObjectHandleIndex.PackageData.AddDefaulted_GetRef();
				PackageData->PackageName = NameToMinimalName(PackageName);
				GCoreObjectHandlePackageDebug = reinterpret_cast<FObjectHandlePackageDebugData*>(GObjectHandleIndex.PackageData.GetData());
			}
			else
			{
				PackageData = &GObjectHandleIndex.PackageData[FoundPackageId->ToIndex()];
			}
			//Can't reasonably switch back to a read lock here because the downgrade would have a window where 
			//the global map could be modified and invalidate the pointer we're holding to an element in it.
		}
		else
		{
			PackageData = &GObjectHandleIndex.PackageData[FoundPackageId->ToIndex()];
		}

		OutPackageId = *FoundPackageId;

		FRWScopeLock LocalLockScope(PackageData->Lock, SLT_ReadOnly);
		FObjectId* FoundId = PackageData->PathToObjectId.Find(ObjectPath);

		if (FoundId)
		{
			check(*FoundId != EObjectId::Invalid);
			OutObjectId = *FoundId;
			return;
		}

		LocalLockScope.ReleaseReadOnlyLockAndAcquireWriteLock_USE_WITH_CAUTION();
		//The PathToObjectId could have been modified when the read lock was released and the write
		//lock was acquired, so we must check and see if the ObjectPath was added in that window.
		FoundId = &PackageData->PathToObjectId.FindOrAdd(ObjectPath, EObjectId::Invalid);
		if (*FoundId != EObjectId::Invalid)
		{
			OutObjectId = *FoundId;
			return;
		}
		uint32 PathIndex = PackageData->ObjectPaths.Emplace(ObjectPath);
		checkf(((PathIndex + 1) & ~ObjectHandle_Private::ObjectPathIdMask) == 0, TEXT("Path id overflowed space in ObjectHandle"));
		FoundId->Components.ObjectPathId = PathIndex + 1;

		if (!ClassName.IsNone() && !ClassPackageName.IsNone())
		{
			// @TODO: OBJPTR: This could be inefficient if there are a high number of references to blueprint data instances
			//		or references to unique blueprints in a single package.  Evaluate whether that's likely to be
			//		the case in practice.
			FObjectHandleDataClassDescriptor DataClassDesc{NameToMinimalName(ClassPackageName), NameToMinimalName(ClassName)};
			uint32 DataClassDescriptorIndex = PackageData->DataClassDescriptors.AddUnique(DataClassDesc);
			checkf(((DataClassDescriptorIndex + 1) & ~ObjectHandle_Private::DataClassDescriptorIdMask) == 0, TEXT("Data class descriptor id overflowed space in ObjectHandle"));
			FoundId->Components.DataClassDescriptorId = DataClassDescriptorIndex + 1;
		}
		OutObjectId = *FoundId;
	}

	static inline FPackedObjectRef MakePackedObjectRef(FName PackageName, FName ClassPackageName, FName ClassName, FObjectPathId ObjectPath)
	{
		FPackageId PackageId;
		FObjectId ObjectId;
		MakeReferenceIds(PackageName, ClassPackageName, ClassName, ObjectPath, PackageId, ObjectId);
		return Pack(PackageId, ObjectId);
	}

	static void GetObjectDataFromId(FPackageId PackageId, FObjectId ObjectId, FMinimalName& OutPackageName, FObjectPathId& OutPathId, FMinimalName& OutClassPackageName, FMinimalName& OutClassName)
	{
		if ((ObjectId == EObjectId::Invalid) || !PackageId.IsValid())
		{
			return;
		}

		FRWScopeLock GlobalLockScope(GObjectHandleIndex.Lock, SLT_ReadOnly);
		const uint32 PackageIndex = PackageId.ToIndex();
		if (PackageIndex >= static_cast<uint32>(GObjectHandleIndex.PackageData.Num()))
		{
			//checkf(false, TEXT("FObjectHandle: PackageIndex invalid.  This ObjectHandle is from malformed data."));
			return;
		}
		FObjectHandlePackageData& FoundPackageData = GObjectHandleIndex.PackageData[PackageIndex];

		FRWScopeLock LocalLockScope(FoundPackageData.Lock, SLT_ReadOnly);
		if ((ObjectId.Components.ObjectPathId >= static_cast<uint32>(FoundPackageData.ObjectPaths.Num() + 1)) ||
			(ObjectId.Components.DataClassDescriptorId >= static_cast<uint32>(FoundPackageData.DataClassDescriptors.Num() + 1)))
		{
			//checkf(false, TEXT("FObjectHandle: ObjectId.Components.ObjectPathId or ObjectId.Components.DataClassDescriptorId invalid.  This ObjectHandle is from malformed data."));
			return;
		}

		OutPackageName = FoundPackageData.PackageName;
		OutPathId = FoundPackageData.ObjectPaths[ObjectId.Components.ObjectPathId - 1];

		if (ObjectId.Components.DataClassDescriptorId > 0)
		{
			FObjectHandleDataClassDescriptor& Desc = FoundPackageData.DataClassDescriptors[ObjectId.Components.DataClassDescriptorId - 1];
			OutClassPackageName = Desc.PackageName;
			OutClassName = Desc.ClassName;
		}
		else
		{
			//@TODO OBJPTR: Is this an error case we should check on?
			//checkf(false, TEXT("FObjectHandle: ObjectId.Components.ObjectPathId or ObjectId.Components.DataClassDescriptorId invalid.  This ObjectHandle is from malformed data."));
			return;
		}
	}

	static inline FObjectRef MakeObjectRef(FPackedObjectRef PackedObjectRef)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ObjectHandle_Private::MakeObjectRef);
		FObjectId ObjectId;
		FPackageId PackageId;
		Unpack(PackedObjectRef, PackageId, ObjectId);

		// Default reference must be invalid if GetObjectDataFromId doesn't populate the reference fields.
		FMinimalName PackageName;
		FObjectPathId PathId(FObjectPathId::Invalid);
		FMinimalName ClassPackageName;
		FMinimalName ClassName;
		GetObjectDataFromId(PackageId, ObjectId, PackageName, PathId, ClassPackageName, ClassName);
		return FObjectRef{MinimalNameToName(PackageName), MinimalNameToName(ClassPackageName), MinimalNameToName(ClassName), PathId};
	}
};

static inline FName GetNameOrNone(UObject* Object)
{
	return Object ? Object->GetFName() : NAME_None;
}

FObjectRef MakeObjectRef(const UObject* Object)
{
	if (!Object)
	{
		return {NAME_None, NAME_None, NAME_None, FObjectPathId()};
	}

#if WITH_EDITORONLY_DATA	
	UObject* ClassGeneratedBy = Object->GetClass()->ClassGeneratedBy;
	UPackage* ClassGeneratedByPackage = ClassGeneratedBy ? ClassGeneratedBy->GetOutermost() : nullptr;
	return FObjectRef {GetNameOrNone(Object->GetOutermost()), GetNameOrNone(ClassGeneratedByPackage), GetNameOrNone(ClassGeneratedBy), FObjectPathId(Object)};
#else
	return FObjectRef{ GetNameOrNone(Object->GetOutermost()), NAME_None, NAME_None, FObjectPathId(Object) };
#endif
}

FObjectRef MakeObjectRef(FPackedObjectRef PackedObjectRef)
{
	if (IsPackedObjectRefNull(PackedObjectRef))
	{
		return {NAME_None, NAME_None, NAME_None, FObjectPathId()};
	}

	return ObjectHandle_Private::MakeObjectRef(PackedObjectRef);
}

FPackedObjectRef MakePackedObjectRef(const UObject* Object)
{
	if (!Object)
	{
		return {0};
	}

	FName PackageName = GetNameOrNone(Object->GetOutermost());
#if WITH_EDITORONLY_DATA
	UObject* ClassGeneratedBy = Object->GetClass()->ClassGeneratedBy;
	UPackage* ClassGeneratedByPackage = ClassGeneratedBy ? ClassGeneratedBy->GetOutermost() : nullptr;
	return ObjectHandle_Private::MakePackedObjectRef(PackageName, GetNameOrNone(ClassGeneratedByPackage), GetNameOrNone(ClassGeneratedBy), FObjectPathId(Object));
#else
	return ObjectHandle_Private::MakePackedObjectRef(PackageName, NAME_None, NAME_None, FObjectPathId(Object));
#endif
}

FPackedObjectRef MakePackedObjectRef(const FObjectRef& ObjectRef)
{
	if (IsObjectRefNull(ObjectRef))
	{
		return {0};
	}

	return ObjectHandle_Private::MakePackedObjectRef(ObjectRef.PackageName, ObjectRef.ClassPackageName, ObjectRef.ClassName, ObjectRef.ObjectPath);
}

static inline UPackage* FindOrLoadPackage(FName PackageName, int32 LoadFlags, OUT bool& bWasPackageLoaded)
{
	bWasPackageLoaded = false;

	// @TODO: OBJPTR: Want to replicate the functional path of an import here.  See things like FindImportFast in BlueprintSupport.cpp
	// 		 for additional behavior that we're not handling here yet.
	FName* ScriptPackageName = FPackageName::FindScriptPackageName(PackageName);
	UPackage* TargetPackage = (UPackage*)StaticFindObjectFastInternal(UPackage::StaticClass(), nullptr, PackageName);
	if (UObjectRedirector* Redirector = dynamic_cast<UObjectRedirector*>(TargetPackage))
	{
		TargetPackage = (UPackage*)Redirector->DestinationObject;
	}
	if (!ScriptPackageName && !TargetPackage)
	{
		// @TODO: OBJPTR: When using the "external package" feature, we will have objects that have a differing package path vs "outer hierarchy" path
		//				  The package path should be used when loading.  The "outer hierarchy" path may need to be used when finding existing objects in memory.
		//				  This will need further evaluation and testing before lazy load can be enabled.
		// @TODO: OBJPTR: Instancing context may be important to consider when loading the package.
		if (FLinkerLoad::IsKnownMissingPackage(PackageName))
		{
			return nullptr;
		}
		LoadFlags |= LOAD_NoWarn | LOAD_NoVerify; //This does nothing? | LOAD_DisableDependencyPreloading;
		TargetPackage = LoadPackage(nullptr, *PackageName.ToString(), LoadFlags);
		bWasPackageLoaded = true;
	}
	return TargetPackage;
}

UClass* ResolveObjectRefClass(const FObjectRef& ObjectRef, uint32 LoadFlags /*= LOAD_None*/)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ResolveObjectRef);
	UClass* ClassObject = nullptr;
	UPackage* ClassPackage = nullptr;
	if (!ObjectRef.ClassPackageName.IsNone())
	{
		bool bWasPackageLoaded = false;
		ClassPackage = FindOrLoadPackage(ObjectRef.ClassPackageName, LoadFlags, bWasPackageLoaded);

		if (!ObjectRef.ClassName.IsNone())
		{
			ClassObject = (UClass*)StaticFindObjectFastInternal(UClass::StaticClass(), ClassPackage, ObjectRef.ClassName);
			if (UObjectRedirector* Redirector = dynamic_cast<UObjectRedirector*>(ClassObject))
			{
				ClassObject = (UClass*)Redirector->DestinationObject;
				if (ClassObject != nullptr)
				{
					ClassPackage = ClassObject->GetPackage();
				}
			}

			if (ClassObject)
			{
				if (ClassObject->HasAnyFlags(RF_NeedLoad) && ClassPackage->GetLinker())
				{
					ClassPackage->GetLinker()->Preload(ClassObject);
				}
				ClassObject->GetDefaultObject(); // build the CDO if it isn't already built
			}
		}
	}

	ObjectHandle_Private::OnClassReferenceResolved(ObjectRef, ClassPackage, ClassObject);
	return ClassObject;
}

class FFullyLoadPackageOnHandleResolveTask
{
public:
	FFullyLoadPackageOnHandleResolveTask(UPackage* InPackage): Package(InPackage)
	{
	}

	FORCEINLINE TStatId GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(FFullyLoadPackageOnHandleResolveTask, STATGROUP_TaskGraphTasks); }
	ENamedThreads::Type GetDesiredThread() { return ENamedThreads::Type::GameThread; }
	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::FireAndForget; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		Package->GetLinker()->LoadAllObjects(true);
	}

private:
	UPackage* Package = nullptr;
};

UObject* ResolveObjectRef(const FObjectRef& ObjectRef, uint32 LoadFlags /*= LOAD_None*/)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ResolveObjectRef);

	if (IsObjectRefNull(ObjectRef) || !ObjectRef.ObjectPath.IsValid())
	{
		ObjectHandle_Private::OnReferenceResolved(ObjectRef, nullptr, nullptr);
		return nullptr;
	}

	ResolveObjectRefClass(ObjectRef, LoadFlags);

	bool bWasObjectOrPackageLoaded = false;

	UPackage* TargetPackage = FindOrLoadPackage(ObjectRef.PackageName, LoadFlags, bWasObjectOrPackageLoaded);

	if (!TargetPackage)
	{
		ObjectHandle_Private::OnReferenceResolved(ObjectRef, nullptr, nullptr);
		return nullptr;
	}

	FObjectPathId::ResolvedNameContainerType ResolvedNames;
	ObjectRef.ObjectPath.Resolve(ResolvedNames);

	UObject* CurrentObject = TargetPackage;
	for (int32 ObjectPathIndex = 0; ObjectPathIndex < ResolvedNames.Num(); ++ObjectPathIndex)
	{
		UObject* PreviousOuter = CurrentObject;
		CurrentObject = StaticFindObjectFastInternal(nullptr, CurrentObject, ResolvedNames[ObjectPathIndex]);
		if (UObjectRedirector* Redirector = dynamic_cast<UObjectRedirector*>(CurrentObject))
		{
			CurrentObject = Redirector->DestinationObject;
			if (CurrentObject != nullptr)
			{
				TargetPackage = CurrentObject->GetPackage();
			}
		}

		if (!CurrentObject && !TargetPackage->IsFullyLoaded() && TargetPackage->GetLinker() && TargetPackage->GetLinker()->IsLoading())
		{
			if (IsInAsyncLoadingThread() || IsInGameThread())
			{
				TargetPackage->GetLinker()->LoadAllObjects(true);
				bWasObjectOrPackageLoaded = true;
			}
			else
			{
				// Shunt the load request to happen on the game thread and block on its completion.  This is a deadlock risk!  The game thread may be blocked waiting on this thread.
				UE_LOG(LogObjectHandle, Warning, TEXT("Resolve of object in package '%s' from a non-game thread was shunted to the game thread."), *ObjectRef.PackageName.ToString());
				TGraphTask<FFullyLoadPackageOnHandleResolveTask>::CreateTask().ConstructAndDispatchWhenReady(TargetPackage)->Wait();
				bWasObjectOrPackageLoaded = true;
			}

			CurrentObject = StaticFindObjectFastInternal(nullptr, PreviousOuter, ResolvedNames[ObjectPathIndex]);
			if (UObjectRedirector* Redirector = dynamic_cast<UObjectRedirector*>(CurrentObject))
			{
				CurrentObject = Redirector->DestinationObject;
				if (CurrentObject != nullptr)
				{
					TargetPackage = CurrentObject->GetPackage();
				}
			}
		}

		if (!CurrentObject)
		{
			ObjectHandle_Private::OnReferenceResolved(ObjectRef, TargetPackage, nullptr);
			return nullptr;
		}
	}

	if (CurrentObject->HasAnyFlags(RF_NeedLoad) && TargetPackage->GetLinker())
	{
		if (IsInAsyncLoadingThread() || IsInGameThread())
		{
			TargetPackage->GetLinker()->LoadAllObjects(true);
			bWasObjectOrPackageLoaded = true;
		}
		else
		{
			// Shunt the load request to happen on the game thread and block on its completion.  This is a deadlock risk!  The game thread may be blocked waiting on this thread.
			UE_LOG(LogObjectHandle, Warning, TEXT("Resolve of object in package '%s' from a non-game thread was shunted to the game thread."), *ObjectRef.PackageName.ToString());
			TGraphTask<FFullyLoadPackageOnHandleResolveTask>::CreateTask().ConstructAndDispatchWhenReady(TargetPackage)->Wait();
			bWasObjectOrPackageLoaded = true;
		}
	}
	ObjectHandle_Private::OnReferenceResolved(ObjectRef, TargetPackage, CurrentObject);

	if (bWasObjectOrPackageLoaded)
	{
		ObjectHandle_Private::OnReferenceLoaded(ObjectRef, TargetPackage, CurrentObject);
	}

	return CurrentObject;
}

UClass* ResolvePackedObjectRefClass(FPackedObjectRef PackedObjectRef, uint32 LoadFlags /*= LOAD_None*/)
{
	return ResolveObjectRefClass(MakeObjectRef(PackedObjectRef), LoadFlags);
}

UObject* ResolvePackedObjectRef(FPackedObjectRef PackedObjectRef, uint32 LoadFlags /*= LOAD_None*/)
{
	return ResolveObjectRef(MakeObjectRef(PackedObjectRef), LoadFlags);
}

#if UE_WITH_OBJECT_HANDLE_TRACKING

COREUOBJECT_API std::atomic<int32> ObjectHandle_Private::ObjectHandleEventIndex;
COREUOBJECT_API ObjectHandle_Private::FObjectHandleEvents ObjectHandle_Private::ObjectHandleEvents[2];

ObjectHandle_Private::FObjectHandleEvents& BeginWritingEvents()
{
	// We only allow adding/removing the callbacks from a single thread.
	check(IsInGameThread());

	// Get what the last and new index will be.
	const int32 LastIndex = ObjectHandle_Private::ObjectHandleEventIndex;
	const int32 NewIndex = LastIndex == 0 ? 1 : 0;

	// Make sure we're not still using the events, this should definitely not be true, because
	// we spin lock at the end of any of these functions to make sure all using has stopped.
	check(!ObjectHandle_Private::ObjectHandleEvents[NewIndex].IsUsing());

	// We need to take whatever delegates are setup on the previous index and copy them over to new set.
	ObjectHandle_Private::ObjectHandleEvents[NewIndex].ObjectHandleReadEvent = ObjectHandle_Private::ObjectHandleEvents[LastIndex].ObjectHandleReadEvent;
	ObjectHandle_Private::ObjectHandleEvents[NewIndex].ClassReferenceResolvedEvent = ObjectHandle_Private::ObjectHandleEvents[LastIndex].ClassReferenceResolvedEvent;
	ObjectHandle_Private::ObjectHandleEvents[NewIndex].ObjectHandleReferenceResolvedEvent = ObjectHandle_Private::ObjectHandleEvents[LastIndex].ObjectHandleReferenceResolvedEvent;
	ObjectHandle_Private::ObjectHandleEvents[NewIndex].ObjectHandleReferenceLoadedEvent = ObjectHandle_Private::ObjectHandleEvents[LastIndex].ObjectHandleReferenceLoadedEvent;

	// Now toggle which buffer we're using.
	ObjectHandle_Private::ObjectHandleEventIndex = NewIndex;

	// Ok, now we need to spin lock until nobody is using the LastIndex, this will prevent us from
	// calling this function right after this one and assuming the previous set isn't being used,
	// similarly while we were doing this, the BeginReadingEvents() function spun lock after marked
	// that they were using the event set, so now that we've set the new event index, if we spin lock
	// here, that should ensure that any existing readers finish up what they were doing.
	while (ObjectHandle_Private::ObjectHandleEvents[LastIndex].IsUsing())
	{
		// Spin!
	}

	return ObjectHandle_Private::ObjectHandleEvents[NewIndex];
}

FDelegateHandle AddObjectHandleReadCallback(FObjectHandleReadDelegate Callback)
{
	ObjectHandle_Private::FObjectHandleEvents& Events = BeginWritingEvents();
	return Events.ObjectHandleReadEvent.Add(Callback);
}

void RemoveObjectHandleReadCallback(FDelegateHandle DelegateHandle)
{
	ObjectHandle_Private::FObjectHandleEvents& Events = BeginWritingEvents();
	Events.ObjectHandleReadEvent.Remove(DelegateHandle);
}

FDelegateHandle AddObjectHandleClassResolvedCallback(FObjectHandleClassResolvedDelegate Callback)
{
	ObjectHandle_Private::FObjectHandleEvents& Events = BeginWritingEvents();
	return Events.ClassReferenceResolvedEvent.Add(Callback);
}

void RemoveObjectHandleClassResolvedCallback(FDelegateHandle DelegateHandle)
{
	ObjectHandle_Private::FObjectHandleEvents& Events = BeginWritingEvents();
	Events.ClassReferenceResolvedEvent.Remove(DelegateHandle);
}

FDelegateHandle AddObjectHandleReferenceResolvedCallback(FObjectHandleReferenceResolvedDelegate Callback)
{
	ObjectHandle_Private::FObjectHandleEvents& Events = BeginWritingEvents();
	return Events.ObjectHandleReferenceResolvedEvent.Add(Callback);
}

void RemoveObjectHandleReferenceResolvedCallback(FDelegateHandle DelegateHandle)
{
	ObjectHandle_Private::FObjectHandleEvents& Events = BeginWritingEvents();
	Events.ObjectHandleReferenceResolvedEvent.Remove(DelegateHandle);
}

FDelegateHandle AddObjectHandleReferenceLoadedCallback(FObjectHandleReferenceLoadedDelegate Callback)
{
	ObjectHandle_Private::FObjectHandleEvents& Events = BeginWritingEvents();
	return Events.ObjectHandleReferenceLoadedEvent.Add(Callback);
}

void RemoveObjectHandleReferenceLoadedCallback(FDelegateHandle DelegateHandle)
{
	ObjectHandle_Private::FObjectHandleEvents& Events = BeginWritingEvents();
	Events.ObjectHandleReferenceLoadedEvent.Remove(DelegateHandle);
}

#endif // UE_WITH_OBJECT_HANDLE_TRACKING
