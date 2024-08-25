// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UObjectBase.cpp: Unreal UObject base class
=============================================================================*/

#include "UObject/UObjectBase.h"
#include "Misc/MessageDialog.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/IConsoleManager.h"
#include "HAL/LowLevelMemTracker.h"
#include "Misc/FeedbackContext.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectAllocator.h"
#include "UObject/UObjectHash.h"
#include "UObject/Class.h"
#include "UObject/DeferredRegistry.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UObjectStats.h"
#include "UObject/Package.h"
#include "Templates/Casts.h"
#include "UObject/GCObject.h"
#include "UObject/LinkerLoad.h"
#include "UObject/Reload.h"
#include "Misc/CommandLine.h"
#include "Interfaces/IPluginManager.h"
#include "Serialization/LoadTimeTrace.h"

DEFINE_LOG_CATEGORY_STATIC(LogUObjectBase, Log, All);
DEFINE_STAT(STAT_UObjectsStatGroupTester);

DECLARE_CYCLE_STAT(TEXT("CreateStatID"), STAT_CreateStatID, STATGROUP_StatSystem);

DEFINE_LOG_CATEGORY_STATIC(LogUObjectBootstrap, Display, Display);

#if CSV_PROFILER && CSV_TRACK_UOBJECT_COUNT
namespace UObjectStats
{
	COREUOBJECT_API std::atomic<int32> GUObjectCount;
}
#endif

/** Whether uobject system is initialized.												*/
namespace Internal
{
	static bool& GetUObjectSubsystemInitialised()
	{
		static bool ObjInitialized = false;
		return ObjInitialized;
	}
};
bool UObjectInitialized()
{
	return Internal::GetUObjectSubsystemInitialised();
}

/** Objects to automatically register once the object system is ready.					*/
struct FPendingRegistrantInfo
{
	const TCHAR*	Name;
	const TCHAR*	PackageName;
	FPendingRegistrantInfo(const TCHAR* InName,const TCHAR* InPackageName)
		:	Name(InName)
		,	PackageName(InPackageName)
	{}
	static TMap<UObjectBase*, FPendingRegistrantInfo>& GetMap()
	{
		static TMap<UObjectBase*, FPendingRegistrantInfo> PendingRegistrantInfo;
		return PendingRegistrantInfo;
	}
};


/** Objects to automatically register once the object system is ready.					*/
struct FPendingRegistrant
{
	UObjectBase*	Object;
	FPendingRegistrant*	NextAutoRegister;
	FPendingRegistrant(UObjectBase* InObject)
	:	Object(InObject)
	,	NextAutoRegister(NULL)
	{}
};
static FPendingRegistrant* GFirstPendingRegistrant = NULL;
static FPendingRegistrant* GLastPendingRegistrant = NULL;

#if USE_PER_MODULE_UOBJECT_BOOTSTRAP
static TMap<FName, TArray<FPendingRegistrant*>>& GetPerModuleBootstrapMap()
{
	static TMap<FName, TArray<FPendingRegistrant*>> PendingRegistrantInfo;
	return PendingRegistrantInfo;
}

#endif


/**
 * Constructor used for bootstrapping
 * @param	InClass			possibly NULL, this gives the class of the new object, if known at this time
 * @param	InFlags			RF_Flags to assign
 */
UObjectBase::UObjectBase( EObjectFlags InFlags )
:	ObjectFlags			(InFlags)
,	InternalIndex		(INDEX_NONE)
,	ClassPrivate		(nullptr)
,	OuterPrivate		(nullptr)
{
#if CSV_PROFILER && CSV_TRACK_UOBJECT_COUNT
	UObjectStats::IncrementUObjectCount();
#endif
}

/**
 * Constructor used by StaticAllocateObject
 * @param	InClass				non NULL, this gives the class of the new object, if known at this time
 * @param	InFlags				RF_Flags to assign
 * @param	InOuter				outer for this object
 * @param	InName				name of the new object
 * @param	InObjectArchetype	archetype to assign
 */
UObjectBase::UObjectBase(UClass* InClass,
	EObjectFlags InFlags,
	EInternalObjectFlags InInternalFlags,
	UObject *InOuter,
	FName InName,
	int32 InInternalIndex,
	int32 InSerialNumber)
:	ObjectFlags			(InFlags)
,	InternalIndex		(INDEX_NONE)
,	ClassPrivate		(InClass)
,	OuterPrivate		(InOuter)
{
	check(ClassPrivate);
	// Add to global table.
	AddObject(InName, InInternalFlags, InInternalIndex, InSerialNumber);
	
#if CSV_PROFILER && CSV_TRACK_UOBJECT_COUNT
	UObjectStats::IncrementUObjectCount();
#endif
		}	


/**
 * Final destructor, removes the object from the object array, and indirectly, from any annotations
 **/
UObjectBase::~UObjectBase()
{
	// If not initialized, skip out.
	if( UObjectInitialized() && ClassPrivate && !GIsCriticalError )
	{
		// Validate it.
		check(IsValidLowLevel());
		check(GetFName() == NAME_None);
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
		UE::CoreUObject::Private::FreeObjectHandle(this);
#endif 
		GUObjectArray.FreeUObjectIndex(this);
	}

#if CSV_PROFILER && CSV_TRACK_UOBJECT_COUNT
	UObjectStats::DecrementUObjectCount();
#endif
}




/**
 * Convert a boot-strap registered class into a real one, add to uobject array, etc
 *
 * @param UClassStaticClass Now that it is known, fill in UClass::StaticClass() as the class
 */
void UObjectBase::DeferredRegister(UClass *UClassStaticClass,const TCHAR* PackageName,const TCHAR* InName)
{
	check(UObjectInitialized());
	// Set object properties.
	UPackage* Package = CreatePackage(PackageName);
	check(Package);
	Package->SetPackageFlags(PKG_CompiledIn);
	OuterPrivate = Package;

	check(UClassStaticClass);
	check(!ClassPrivate);
	ClassPrivate = UClassStaticClass;

	// Add to the global object table.
	AddObject(FName(InName), EInternalObjectFlags::None);
	// At this point all compiled-in objects should have already been fully constructed so it's safe to remove the NotFullyConstructed flag
	// which was set in FUObjectArray::AllocateUObjectIndex (called from AddObject)
	GUObjectArray.IndexToObject(InternalIndex)->ClearFlags(EInternalObjectFlags::PendingConstruction);

	// Make sure that objects disregarded for GC are part of root set.
	check(!GUObjectArray.IsDisregardForGC(this) || GUObjectArray.IndexToObject(InternalIndex)->IsRootSet());

	UE_LOG(LogUObjectBootstrap, Verbose, TEXT("UObjectBase::DeferredRegister %s %s"), PackageName, InName);
}

/**
 * Add a newly created object to the name hash tables and the object array
 *
 * @param Name name to assign to this uobject
 */
void UObjectBase::AddObject(FName InName, EInternalObjectFlags InSetInternalFlags, int32 InInternalIndex, int32 InSerialNumber)
{
	NamePrivate = InName;
	EInternalObjectFlags InternalFlagsToSet = InSetInternalFlags;
	if (!IsInGameThread())
	{
		InternalFlagsToSet |= EInternalObjectFlags::Async;
	}
	if (ObjectFlags & RF_MarkAsRootSet)
	{		
		InternalFlagsToSet |= EInternalObjectFlags::RootSet;
		ObjectFlags &= ~RF_MarkAsRootSet;
	}
	if (ObjectFlags & RF_MarkAsNative)
	{
		InternalFlagsToSet |= EInternalObjectFlags::Native;
		ObjectFlags &= ~RF_MarkAsNative;
	}
	GUObjectArray.AllocateUObjectIndex(this, InternalFlagsToSet, InInternalIndex, InSerialNumber);
	check(InName != NAME_None && InternalIndex >= 0);
	HashObject(this);
	check(IsValidLowLevel());
}

/**
 * Just change the FName and Outer and rehash into name hash tables. For use by higher level rename functions.
 *
 * @param NewName	new name for this object
 * @param NewOuter	new outer for this object, if NULL, outer will be unchanged
 */
void UObjectBase::LowLevelRename(FName NewName,UObject *NewOuter)
{
#if STATS || ENABLE_STATNAMEDEVENTS_UOBJECT
	((UObject*)this)->ResetStatID(); // reset the stat id since this thing now has a different name
#endif
	UnhashObject(this);
	check(InternalIndex >= 0);
	NamePrivate = NewName;
	if (NewOuter)
	{
		OuterPrivate = NewOuter;
	}
	HashObject(this);
}

UPackage* UObjectBase::GetExternalPackage() const
{
	// if we have no outer, consider this a package, packages returns themselves as their external package
	if (OuterPrivate == nullptr)
	{
		return CastChecked<UPackage>((UObject*)(this));
	}
	UPackage* ExternalPackage = nullptr;
	if ((GetFlags() & RF_HasExternalPackage) != 0)
	{
		ExternalPackage = GetObjectExternalPackageThreadSafe(this);
	}
	return ExternalPackage;
}

UPackage* UObjectBase::GetExternalPackageInternal() const
{
	// if we have no outer, consider this a package, packages returns themselves as their external package
	if (OuterPrivate == nullptr)
	{
		return CastChecked<UPackage>((UObject*)(this));
	}
	return (GetFlags() & RF_HasExternalPackage) != 0 ? GetObjectExternalPackageInternal(this) : nullptr;
}

void UObjectBase::SetExternalPackage(UPackage* InPackage)
{
	// if we have no outer, consider this a package, packages have themselves as their external package and that shouldn't be added to the object hash
	if (OuterPrivate == nullptr)
	{
		// Just validate that we tried to set ourselves or nothing as our external package which is a no-op. anything else is illegal for package
		check(GetClass()->IsChildOf(UPackage::StaticClass()) && (InPackage == this || InPackage == nullptr));
		return;
	}
	if (InPackage)
	{
		HashObjectExternalPackage(this, InPackage);
	}
	else
	{
		UnhashObjectExternalPackage(this);
	}
}

#if WITH_EDITOR
void UObjectBase::SetClass(UClass* NewClass)
{
#if STATS || ENABLE_STATNAMEDEVENTS_UOBJECT
	((UObject*)this)->ResetStatID(); // reset the stat id since this thing now has a different name
#endif

	UnhashObject(this);

	UClass* OldClass = ClassPrivate;
	ClassPrivate->DestroyPersistentUberGraphFrame((UObject*)this);
	ClassPrivate = NewClass;
	ClassPrivate->CreatePersistentUberGraphFrame((UObject*)this, /*bCreateOnlyIfEmpty =*/false, /*bSkipSuperClass =*/false, OldClass);
	HashObject(this);
}
#endif

/**
 * Checks to see if the object appears to be valid
 * @return true if this appears to be a valid object
 */
bool UObjectBase::IsValidLowLevel() const
{
	if( this == nullptr )
	{
		UE_LOG(LogUObjectBase, Warning, TEXT("NULL object") );
		return false;
	}
	if( !ClassPrivate )
	{
		UE_LOG(LogUObjectBase, Warning, TEXT("Object is not registered") );
		return false;
	}
	return GUObjectArray.IsValid(this);
}

bool UObjectBase::IsValidLowLevelFast(bool bRecursive /*= true*/) const
{
	// As DEFAULT_ALIGNMENT is defined to 0 now, I changed that to the original numerical value here
	const int32 AlignmentCheck = MIN_ALIGNMENT - 1;

	// Check 'this' pointer before trying to access any of the Object's members
	if ((this == nullptr) || (UPTRINT)this < 0x100)
	{
		UE_LOG(LogUObjectBase, Error, TEXT("\'this\' pointer is invalid."));
		return false;
	}
	if ((UPTRINT)this & AlignmentCheck)
	{
		UE_LOG(LogUObjectBase, Error, TEXT("\'this\' pointer is misaligned."));
		return false;
	}
	if (*(void**)this == nullptr)
	{
		UE_LOG(LogUObjectBase, Error, TEXT("Virtual functions table is invalid."));
		return false;
	}

	// These should all be 0.
	const UPTRINT CheckZero = (GetFlagsInternal() & ~RF_AllFlags) | ((UPTRINT)ClassPrivate & AlignmentCheck) | ((UPTRINT)OuterPrivate & AlignmentCheck);
	if (!!CheckZero)
	{
		UE_LOG(LogUObjectBase, Error, TEXT("Object flags are invalid or either Class or Outer is misaligned"));
    return false;
	}
	// These should all be non-NULL (except CDO-alignment check which should be 0)
	if (ClassPrivate == nullptr || ClassPrivate->ClassDefaultObject == nullptr || ((UPTRINT)ClassPrivate->ClassDefaultObject & AlignmentCheck) != 0)
	{
#if WITH_LIVE_CODING
		// When live coding is re-instancing blueprint generated classes, we have to clear out the default object so it can get 
		// GC'ed and deleted prior to live coding completing the patching process (of the destructor specifically)
		if (ClassPrivate == nullptr || !ClassPrivate->HasAnyClassFlags(CLASS_NewerVersionExists))
#endif
		{
			UE_LOG(LogUObjectBase, Error, TEXT("Class pointer is invalid or CDO is invalid."));
			return false;
		}
	}
	// Avoid infinite recursion so call IsValidLowLevelFast on the class object with bRecursive = false.
	if (bRecursive && !ClassPrivate->IsValidLowLevelFast(false))
	{
		UE_LOG(LogUObjectBase, Error, TEXT("Class object failed IsValidLowLevelFast test."));
		return false;
	}
	// Lightweight versions of index checks.
	if (!GUObjectArray.IsValidIndex(this) || !NamePrivate.IsValidIndexFast())
	{
		UE_LOG(LogUObjectBase, Error, TEXT("Object array index or name index is invalid."));
		return false;
	}
	return true;
}

#if USE_PER_MODULE_UOBJECT_BOOTSTRAP
static void UObjectReleaseModuleRegistrants(FName Module)
{
	TMap<FName, TArray<FPendingRegistrant*>>& PerModuleMap = GetPerModuleBootstrapMap();

	FName Package = IPluginManager::Get().PackageNameFromModuleName(Module);

	FName ScriptName = *(FString(TEXT("/Script/")) + Package.ToString());

	TArray<FPendingRegistrant*>* Array = PerModuleMap.Find(ScriptName);
	if (Array)
	{
		SCOPED_BOOT_TIMING("UObjectReleaseModuleRegistrants");
		for (FPendingRegistrant* PendingRegistration : *Array)
		{
			if (GLastPendingRegistrant)
			{
				GLastPendingRegistrant->NextAutoRegister = PendingRegistration;
			}
			else
			{
				check(!GFirstPendingRegistrant);
				GFirstPendingRegistrant = PendingRegistration;
			}
			GLastPendingRegistrant = PendingRegistration;
		}
		UE_LOG(LogUObjectBootstrap, Verbose, TEXT("UObjectReleaseModuleRegistrants %d items in %s"), Array->Num(), *ScriptName.ToString());
		PerModuleMap.Remove(ScriptName);
	}
	else
	{
		UE_LOG(LogUObjectBootstrap, Verbose, TEXT("UObjectReleaseModuleRegistrants no items in %s"), *ScriptName.ToString());
	}
}

void UObjectReleaseAllModuleRegistrants()
{
	SCOPED_BOOT_TIMING("UObjectReleaseAllModuleRegistrants");
	TMap<FName, TArray<FPendingRegistrant*>>& PerModuleMap = GetPerModuleBootstrapMap();
	for (auto& Pair : PerModuleMap)
	{
		for (FPendingRegistrant* PendingRegistration : Pair.Value)
		{
			if (GLastPendingRegistrant)
			{
				GLastPendingRegistrant->NextAutoRegister = PendingRegistration;
			}
			else
			{
				check(!GFirstPendingRegistrant);
				GFirstPendingRegistrant = PendingRegistration;
			}
			GLastPendingRegistrant = PendingRegistration;
		}
		UE_LOG(LogUObjectBootstrap, Verbose, TEXT("UObjectReleaseAllModuleRegistrants %d items in %s"), Pair.Value.Num(), *Pair.Key.ToString());
	}
	PerModuleMap.Empty();
	ProcessNewlyLoadedUObjects();
}

static void DumpPendingUObjectModules(const TArray<FString>& Args)
{
	TMap<FName, TArray<FPendingRegistrant*>>& PerModuleMap = GetPerModuleBootstrapMap();
	for (auto& Pair : PerModuleMap)
	{
		UE_LOG(LogUObjectBootstrap, Display, TEXT("Not yet loaded: %d items in %s"), Pair.Value.Num(), *Pair.Key.ToString());
	}
}

static FAutoConsoleCommand DumpPendingUObjectModulesCmd(
	TEXT("DumpPendingUObjectModules"),
	TEXT("When doing per-module UObject bootstrapping, show the modules that are not yet loaded."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&DumpPendingUObjectModules)
);

#endif

/** Enqueue the registration for this object. */
void UObjectBase::Register(const TCHAR* PackageName,const TCHAR* InName)
{
	LLM_SCOPE(ELLMTag::UObject);
	TMap<UObjectBase*, FPendingRegistrantInfo>& PendingRegistrants = FPendingRegistrantInfo::GetMap();

	FPendingRegistrant* PendingRegistration = new FPendingRegistrant(this);
	PendingRegistrants.Add(this, FPendingRegistrantInfo(InName, PackageName));

#if USE_PER_MODULE_UOBJECT_BOOTSTRAP
	if (FName(PackageName) != FName("/Script/CoreUObject"))
	{
		TMap<FName, TArray<FPendingRegistrant*>>& PerModuleMap = GetPerModuleBootstrapMap();

		PerModuleMap.FindOrAdd(FName(PackageName)).Add(PendingRegistration);
	}
	else
#endif
	{
		if (GLastPendingRegistrant)
		{
			GLastPendingRegistrant->NextAutoRegister = PendingRegistration;
		}
		else
		{
			check(!GFirstPendingRegistrant);
			GFirstPendingRegistrant = PendingRegistration;
		}
		GLastPendingRegistrant = PendingRegistration;
	}
}


/**
 * Dequeues registrants from the list of pending registrations into an array.
 * The contents of the array is preserved, and the new elements are appended.
 */
static void DequeuePendingAutoRegistrants(TArray<FPendingRegistrant>& OutPendingRegistrants)
{
	// We process registrations in the order they were enqueued, since each registrant ensures
	// its dependencies are enqueued before it enqueues itself.
	FPendingRegistrant* NextPendingRegistrant = GFirstPendingRegistrant;
	GFirstPendingRegistrant = NULL;
	GLastPendingRegistrant = NULL;
	while(NextPendingRegistrant)
	{
		FPendingRegistrant* PendingRegistrant = NextPendingRegistrant;
		OutPendingRegistrants.Add(*PendingRegistrant);
		NextPendingRegistrant = PendingRegistrant->NextAutoRegister;
		delete PendingRegistrant;
	};
}

/**
 * Process the auto register objects adding them to the UObject array
 */
static void UObjectProcessRegistrants()
{
	SCOPED_BOOT_TIMING("UObjectProcessRegistrants");

	check(UObjectInitialized());
	// Make list of all objects to be registered.
	TArray<FPendingRegistrant> PendingRegistrants;
	DequeuePendingAutoRegistrants(PendingRegistrants);

	for(int32 RegistrantIndex = 0;RegistrantIndex < PendingRegistrants.Num();++RegistrantIndex)
	{
		const FPendingRegistrant& PendingRegistrant = PendingRegistrants[RegistrantIndex];

		UObjectForceRegistration(PendingRegistrant.Object, false);

		check(PendingRegistrant.Object->GetClass()); // should have been set by DeferredRegister

		// Register may have resulted in new pending registrants being enqueued, so dequeue those.
		DequeuePendingAutoRegistrants(PendingRegistrants);
	}
}

void UObjectForceRegistration(UObjectBase* Object, bool bCheckForModuleRelease)
{
	LLM_SCOPE(ELLMTag::UObject);
	TMap<UObjectBase*, FPendingRegistrantInfo>& PendingRegistrants = FPendingRegistrantInfo::GetMap();

	FPendingRegistrantInfo* Info = PendingRegistrants.Find(Object);
	if (Info)
	{
		const TCHAR* PackageName = Info->PackageName;
#if USE_PER_MODULE_UOBJECT_BOOTSTRAP
		if (bCheckForModuleRelease)
		{
			UObjectReleaseModuleRegistrants(FName(PackageName));
		}
#endif
		const TCHAR* Name = Info->Name;
		PendingRegistrants.Remove(Object);  // delete this first so that it doesn't try to do it twice
		Object->DeferredRegister(UClass::StaticClass(),PackageName,Name);
	}
}

// UScriptStruct deferred registration

void RegisterCompiledInInfo(class UScriptStruct* (*InOuterRegister)(), const TCHAR* InPackageName, const TCHAR* InName, FStructRegistrationInfo& InInfo, const FStructReloadVersionInfo& InVersionInfo)
{
	check(InOuterRegister);
	FStructDeferredRegistry::Get().AddRegistration(InOuterRegister, nullptr, InPackageName, InName, InInfo, InVersionInfo);
	NotifyRegistrationEvent(InPackageName, InName, ENotifyRegistrationType::NRT_Struct, ENotifyRegistrationPhase::NRP_Added, (UObject * (*)())(InOuterRegister), false);
}

class UScriptStruct *GetStaticStruct(class UScriptStruct *(*InRegister)(), UObject* StructOuter, const TCHAR* StructName)
{
	UScriptStruct *Result = (*InRegister)();
	NotifyRegistrationEvent(*StructOuter->GetOutermost()->GetName(), StructName, ENotifyRegistrationType::NRT_Struct, ENotifyRegistrationPhase::NRP_Finished, nullptr, false, Result);
	return Result;
}

// UEnum deferred registration

void RegisterCompiledInInfo(class UEnum* (*InOuterRegister)(), const TCHAR* InPackageName, const TCHAR* InName, FEnumRegistrationInfo& InInfo, const FEnumReloadVersionInfo& InVersionInfo)
{
	check(InOuterRegister);
	FEnumDeferredRegistry::Get().AddRegistration(InOuterRegister, nullptr, InPackageName, InName, InInfo, InVersionInfo);
	NotifyRegistrationEvent(InPackageName, InName, ENotifyRegistrationType::NRT_Enum, ENotifyRegistrationPhase::NRP_Added, (UObject * (*)())(InOuterRegister), false);
}

class UEnum *GetStaticEnum(class UEnum *(*InRegister)(), UObject* EnumOuter, const TCHAR* EnumName)
{
	UEnum *Result = (*InRegister)();
	NotifyRegistrationEvent(*EnumOuter->GetOutermost()->GetName(), EnumName, ENotifyRegistrationType::NRT_Enum, ENotifyRegistrationPhase::NRP_Finished, nullptr, false, Result);
	return Result;
}

FName UObjectBase::GetFNameForStatID() const
{
	return GetFName();
}

/** Removes prefix from the native class name */
FString UObjectBase::RemoveClassPrefix(const TCHAR* ClassName)
{
	static const TCHAR* DeprecatedPrefix = TEXT("DEPRECATED_");
	FString NameWithoutPrefix(ClassName);
	NameWithoutPrefix.MidInline(1, MAX_int32, EAllowShrinking::No);
	if (NameWithoutPrefix.StartsWith(DeprecatedPrefix))
	{
		NameWithoutPrefix.MidInline(FCString::Strlen(DeprecatedPrefix), MAX_int32, EAllowShrinking::No);
	}
	return NameWithoutPrefix;
}

void RegisterCompiledInInfo(class UClass* (*InOuterRegister)(), class UClass* (*InInnerRegister)(), const TCHAR* InPackageName, const TCHAR* InName, FClassRegistrationInfo& InInfo, const FClassReloadVersionInfo& InVersionInfo)
{
	check(InOuterRegister);
	check(InInnerRegister);
	FClassDeferredRegistry::AddResult result = FClassDeferredRegistry::Get().AddRegistration(InOuterRegister, InInnerRegister, InPackageName, InName, InInfo, InVersionInfo);
#if WITH_RELOAD
	if (result == FClassDeferredRegistry::AddResult::ExistingChanged && !IsReloadActive())
	{
		// Class exists, this can only happen during hot-reload or live coding
		UE_LOG(LogUObjectBase, Fatal, TEXT("Trying to recreate changed class '%s' outside of hot reload and live coding!"), InName);
	}
#endif
	FString NoPrefix(UObjectBase::RemoveClassPrefix(InName));
	NotifyRegistrationEvent(InPackageName, *NoPrefix, ENotifyRegistrationType::NRT_Class, ENotifyRegistrationPhase::NRP_Added, (UObject * (*)())(InOuterRegister), false);
	NotifyRegistrationEvent(InPackageName, *(FString(DEFAULT_OBJECT_PREFIX) + NoPrefix), ENotifyRegistrationType::NRT_ClassCDO, ENotifyRegistrationPhase::NRP_Added, (UObject * (*)())(InOuterRegister), false);
}

// UPackage registration

void RegisterCompiledInInfo(UPackage* (*InOuterRegister)(), const TCHAR* InPackageName, FPackageRegistrationInfo& InInfo, const FPackageReloadVersionInfo& InVersionInfo)
{
#if WITH_RELOAD
	check(InOuterRegister);
	FPackageDeferredRegistry::Get().AddRegistration(reinterpret_cast<class UPackage* (*)()>(InOuterRegister), nullptr, TEXT(""), InPackageName, InInfo, InVersionInfo);
#endif
}

// Multiple registrations
void RegisterCompiledInInfo(const TCHAR* PackageName, const FClassRegisterCompiledInInfo* ClassInfo, size_t NumClassInfo, const FStructRegisterCompiledInInfo* StructInfo, size_t NumStructInfo, const FEnumRegisterCompiledInInfo* EnumInfo, size_t NumEnumInfo)
{
	LLM_SCOPE(ELLMTag::UObject);

	for (size_t Index = 0; Index < NumClassInfo; ++Index)
	{
		const FClassRegisterCompiledInInfo& Info = ClassInfo[Index];
		RegisterCompiledInInfo(Info.OuterRegister, Info.InnerRegister, PackageName, Info.Name, *Info.Info, Info.VersionInfo);
	}

	for (size_t Index = 0; Index < NumStructInfo; ++Index)
	{
		const FStructRegisterCompiledInInfo& Info = StructInfo[Index];
		RegisterCompiledInInfo(Info.OuterRegister, PackageName, Info.Name, *Info.Info, Info.VersionInfo);
		if (Info.CreateCppStructOps != nullptr)
		{
			UScriptStruct::DeferCppStructOps(FTopLevelAssetPath(FName(PackageName), FName(Info.Name)), (UScriptStruct::ICppStructOps*)Info.CreateCppStructOps());
		}
	}

	for (size_t Index = 0; Index < NumEnumInfo; ++Index)
	{
		const FEnumRegisterCompiledInInfo& Info = EnumInfo[Index];
		RegisterCompiledInInfo(Info.OuterRegister, PackageName, Info.Name, *Info.Info, Info.VersionInfo);
	}
}

/** Register all loaded classes */
void UClassRegisterAllCompiledInClasses()
{
#if WITH_RELOAD
	TArray<UClass*> AddedClasses;
#endif
	SCOPED_BOOT_TIMING("UClassRegisterAllCompiledInClasses");
	LLM_SCOPE(ELLMTag::UObject);

	FClassDeferredRegistry& Registry = FClassDeferredRegistry::Get();

	Registry.ProcessChangedObjects();

	for (const FClassDeferredRegistry::FRegistrant& Registrant : Registry.GetRegistrations())
	{
		UClass* RegisteredClass = FClassDeferredRegistry::InnerRegister(Registrant);
#if WITH_RELOAD
		if (IsReloadActive() && Registrant.OldSingleton == nullptr)
		{
			AddedClasses.Add(RegisteredClass);
		}
#endif
	}

#if WITH_RELOAD
	if (AddedClasses.Num() > 0)
	{
		FCoreUObjectDelegates::ReloadAddedClassesDelegate.Broadcast(AddedClasses);
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		FCoreUObjectDelegates::RegisterHotReloadAddedClassesDelegate.Broadcast(AddedClasses);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
#endif
}

#if WITH_RELOAD
/** Re-instance all existing classes that have changed during reload */
void UClassReplaceReloadClasses()
{
	for (const FClassDeferredRegistry::FRegistrant& Registrant : FClassDeferredRegistry::Get().GetRegistrations())
	{
		if (Registrant.OldSingleton == nullptr)
		{
			continue;
		}

		UClass* RegisteredClass = nullptr;
		if (Registrant.bHasChanged)
		{
			RegisteredClass = FClassDeferredRegistry::InnerRegister(Registrant);
		}

		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		FCoreUObjectDelegates::RegisterClassForHotReloadReinstancingDelegate.Broadcast(Registrant.OldSingleton, RegisteredClass, Registrant.bHasChanged ? EHotReloadedClassFlags::Changed : EHotReloadedClassFlags::None);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FCoreUObjectDelegates::ReinstanceHotReloadedClassesDelegate.Broadcast();
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}
#endif

/**
 * Load any outstanding compiled in default properties
 */
static void UObjectLoadAllCompiledInDefaultProperties(TArray<UClass*>& OutAllNewClasses)
{
	TRACE_LOADTIME_REQUEST_GROUP_SCOPE(TEXT("UObjectLoadAllCompiledInDefaultProperties"));

	static FName LongEnginePackageName(TEXT("/Script/Engine"));

	FClassDeferredRegistry& ClassRegistry = FClassDeferredRegistry::Get();

	if (ClassRegistry.HasPendingRegistrations())
	{
		SCOPED_BOOT_TIMING("UObjectLoadAllCompiledInDefaultProperties");
		TArray<UClass*> NewClasses;
		TArray<UClass*> NewClassesInCoreUObject;
		TArray<UClass*> NewClassesInEngine;
		ClassRegistry.DoPendingOuterRegistrations(true, [&OutAllNewClasses, &NewClasses, &NewClassesInCoreUObject, &NewClassesInEngine](const TCHAR* PackageName, UClass& Class) -> void
			{
				UE_LOG(LogUObjectBootstrap, Verbose, TEXT("UObjectLoadAllCompiledInDefaultProperties After Registrant %s %s"), PackageName, *Class.GetName());

				if (Class.GetOutermost()->GetFName() == GLongCoreUObjectPackageName)
				{
					NewClassesInCoreUObject.Add(&Class);
				}
				else if (Class.GetOutermost()->GetFName() == LongEnginePackageName)
				{
					NewClassesInEngine.Add(&Class);
				}
				else
				{
					NewClasses.Add(&Class);
				}

				OutAllNewClasses.Add(&Class);
			}); 

		auto NotifyClassFinishedRegistrationEvents = [](TArray<UClass*>& Classes)
		{
			for (UClass* Class : Classes)
			{
				TCHAR PackageName[FName::StringBufferSize];
				TCHAR ClassName[FName::StringBufferSize];
				Class->GetOutermost()->GetFName().ToString(PackageName);
				Class->GetFName().ToString(ClassName);
				NotifyRegistrationEvent(PackageName, ClassName, ENotifyRegistrationType::NRT_Class, ENotifyRegistrationPhase::NRP_Finished, nullptr, false, Class);
			}
		};

		// notify async loader of all new classes before creating the class default objects
		{
			SCOPED_BOOT_TIMING("NotifyClassFinishedRegistrationEvents");
			NotifyClassFinishedRegistrationEvents(NewClassesInCoreUObject);
			NotifyClassFinishedRegistrationEvents(NewClassesInEngine);
			NotifyClassFinishedRegistrationEvents(NewClasses);
		}

		{
			SCOPED_BOOT_TIMING("CoreUObject Classes");
			for (UClass* Class : NewClassesInCoreUObject) // we do these first because we assume these never trigger loads
			{
				UE_LOG(LogUObjectBootstrap, Verbose, TEXT("GetDefaultObject Begin %s %s"), *Class->GetOutermost()->GetName(), *Class->GetName());
				Class->GetDefaultObject();
				UE_LOG(LogUObjectBootstrap, Verbose, TEXT("GetDefaultObject End %s %s"), *Class->GetOutermost()->GetName(), *Class->GetName());
			}
		}
		{
			SCOPED_BOOT_TIMING("Engine Classes");
			for (UClass* Class : NewClassesInEngine) // we do these second because we want to bring the engine up before the game
			{
				UE_LOG(LogUObjectBootstrap, Verbose, TEXT("GetDefaultObject Begin %s %s"), *Class->GetOutermost()->GetName(), *Class->GetName());
				Class->GetDefaultObject();
				UE_LOG(LogUObjectBootstrap, Verbose, TEXT("GetDefaultObject End %s %s"), *Class->GetOutermost()->GetName(), *Class->GetName());
			}
		}
		{
			SCOPED_BOOT_TIMING("Other Classes");
			for (UClass* Class : NewClasses)
			{
				UE_LOG(LogUObjectBootstrap, Verbose, TEXT("GetDefaultObject Begin %s %s"), *Class->GetOutermost()->GetName(), *Class->GetName());
				Class->GetDefaultObject();
				UE_LOG(LogUObjectBootstrap, Verbose, TEXT("GetDefaultObject End %s %s"), *Class->GetOutermost()->GetName(), *Class->GetName());
			}
		}
		FFeedbackContext& ErrorsFC = UClass::GetDefaultPropertiesFeedbackContext();
		if (ErrorsFC.GetNumErrors() || ErrorsFC.GetNumWarnings())
		{
			TArray<FString> AllErrorsAndWarnings;
			ErrorsFC.GetErrorsAndWarningsAndEmpty(AllErrorsAndWarnings);

			FString AllInOne;
			UE_LOG(LogUObjectBase, Warning, TEXT("-------------- Default Property warnings and errors:"));
			for (const FString& ErrorOrWarning : AllErrorsAndWarnings)
			{
				UE_LOG(LogUObjectBase, Warning, TEXT("%s"), *ErrorOrWarning);
				AllInOne += ErrorOrWarning;
				AllInOne += TEXT("\n");
			}
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format( NSLOCTEXT("Core", "DefaultPropertyWarningAndErrors", "Default Property warnings and errors:\n{0}"), FText::FromString( AllInOne ) ) );
		}
	}
}

/**
 * Call StaticStruct for each struct...this sets up the internal singleton, and important works correctly with hot reload
 */
static void UObjectLoadAllCompiledInStructs()
{
	SCOPED_BOOT_TIMING("UObjectLoadAllCompiledInStructs");

	FEnumDeferredRegistry& EnumRegistry = FEnumDeferredRegistry::Get();
	FStructDeferredRegistry& StructRegistry = FStructDeferredRegistry::Get();

	{
		SCOPED_BOOT_TIMING("UObjectLoadAllCompiledInStructs -  CreatePackages (could be optimized!)");
		EnumRegistry.DoPendingPackageRegistrations();
		StructRegistry.DoPendingPackageRegistrations();
	}

	// Load Structs
	EnumRegistry.DoPendingOuterRegistrations(true);
	StructRegistry.DoPendingOuterRegistrations(true);
}

void ProcessNewlyLoadedUObjects(FName Package, bool bCanProcessNewlyLoadedObjects)
{
	SCOPED_BOOT_TIMING("ProcessNewlyLoadedUObjects");
#if USE_PER_MODULE_UOBJECT_BOOTSTRAP
	if (Package != NAME_None)
	{
		UObjectReleaseModuleRegistrants(Package);
	}
#endif
	if (!bCanProcessNewlyLoadedObjects)
	{
		return;
	}
	LLM_SCOPE(ELLMTag::UObject);
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("ProcessNewlyLoadedUObjects"), STAT_ProcessNewlyLoadedUObjects, STATGROUP_ObjectVerbose);

	FPackageDeferredRegistry& PackageRegistry = FPackageDeferredRegistry::Get();
	FClassDeferredRegistry& ClassRegistry = FClassDeferredRegistry::Get();
	FStructDeferredRegistry& StructRegistry = FStructDeferredRegistry::Get();
	FEnumDeferredRegistry& EnumRegistry = FEnumDeferredRegistry::Get();

	PackageRegistry.ProcessChangedObjects(true);
	StructRegistry.ProcessChangedObjects();
	EnumRegistry.ProcessChangedObjects();

	UClassRegisterAllCompiledInClasses();

	bool bNewUObjects = false;
	TArray<UClass*> AllNewClasses;
	while (GFirstPendingRegistrant ||
		ClassRegistry.HasPendingRegistrations() ||
		StructRegistry.HasPendingRegistrations() ||
		EnumRegistry.HasPendingRegistrations())
	{
		bNewUObjects = true;
		UObjectProcessRegistrants();
		UObjectLoadAllCompiledInStructs();

		FCoreUObjectDelegates::CompiledInUObjectsRegisteredDelegate.Broadcast(Package);

		UObjectLoadAllCompiledInDefaultProperties(AllNewClasses);
	}

#if WITH_RELOAD
	IReload* Reload = GetActiveReloadInterface();
	if (Reload != nullptr)
	{
		UClassReplaceReloadClasses(); // Legacy
		PackageRegistry.NotifyReload(*Reload);
		EnumRegistry.NotifyReload(*Reload);
		StructRegistry.NotifyReload(*Reload);
		ClassRegistry.NotifyReload(*Reload);
		Reload->Reinstance();
	}
#endif

	PackageRegistry.EmptyRegistrations();
	EnumRegistry.EmptyRegistrations();
	StructRegistry.EmptyRegistrations();
	ClassRegistry.EmptyRegistrations();

	if (TMap<UObjectBase*, FPendingRegistrantInfo>& PendingRegistrants = FPendingRegistrantInfo::GetMap(); PendingRegistrants.IsEmpty())
	{
		PendingRegistrants.Empty();
	}

	if (bNewUObjects && !GIsInitialLoad)
	{
		for (UClass* Class : AllNewClasses)
		{
			// Assemble reference token stream for garbage collection/ RTGC.
			if (!Class->HasAnyFlags(RF_ClassDefaultObject) && !Class->HasAnyClassFlags(CLASS_TokenStreamAssembled))
			{
				Class->AssembleReferenceTokenStream();
			}
		}
	}
}

static int32 GVarMaxObjectsNotConsideredByGC;
static FAutoConsoleVariableRef CMaxObjectsNotConsideredByGC(
	TEXT("gc.MaxObjectsNotConsideredByGC"),
	GVarMaxObjectsNotConsideredByGC,
	TEXT("Placeholder console variable, currently not used in runtime."),
	ECVF_Default
	);

static int32 GSizeOfPermanentObjectPool;
static FAutoConsoleVariableRef CSizeOfPermanentObjectPool(
	TEXT("gc.SizeOfPermanentObjectPool"),
	GSizeOfPermanentObjectPool,
	TEXT("Placeholder console variable, currently not used in runtime."),
	ECVF_Default
	);

static int32 GMaxObjectsInEditor;
static FAutoConsoleVariableRef CMaxObjectsInEditor(
	TEXT("gc.MaxObjectsInEditor"),
	GMaxObjectsInEditor,
	TEXT("Placeholder console variable, currently not used in runtime."),
	ECVF_Default
	);

static int32 GMaxObjectsInGame;
static FAutoConsoleVariableRef CMaxObjectsInGame(
	TEXT("gc.MaxObjectsInGame"),
	GMaxObjectsInGame,
	TEXT("Placeholder console variable, currently not used in runtime."),
	ECVF_Default
	);


/**
 * Final phase of UObject initialization. all auto register objects are added to the main data structures.
 */
void UObjectBaseInit()
{
	SCOPED_BOOT_TIMING("UObjectBaseInit");

	// Zero initialize and later on get value from .ini so it is overridable per game/ platform...
	int32 MaxObjectsNotConsideredByGC = 0;
	int32 SizeOfPermanentObjectPool = 0;
	int32 MaxUObjects = 2 * 1024 * 1024; // Default to ~2M UObjects
	bool bPreAllocateUObjectArray = false;	

	// To properly set MaxObjectsNotConsideredByGC look for "Log: XXX objects as part of root set at end of initial load."
	// in your log file. This is being logged from LaunchEnglineLoop after objects have been added to the root set. 

	// Disregard for GC relies on seekfree loading for interaction with linkers. We also don't want to use it in the Editor, for which
	// FPlatformProperties::RequiresCookedData() will be false. Please note that GIsEditor and FApp::IsGame() are not valid at this point.
	if (FPlatformProperties::RequiresCookedData())
	{
		if (IsRunningCookOnTheFly())
		{
			GCreateGCClusters = false;
		}
		else
		{
			GConfig->GetInt(TEXT("/Script/Engine.GarbageCollectionSettings"), TEXT("gc.MaxObjectsNotConsideredByGC"), MaxObjectsNotConsideredByGC, GEngineIni);

			// Not used on PC as in-place creation inside bigger pool interacts with the exit purge and deleting UObject directly.
			GConfig->GetInt(TEXT("/Script/Engine.GarbageCollectionSettings"), TEXT("gc.SizeOfPermanentObjectPool"), SizeOfPermanentObjectPool, GEngineIni);
		}

		// Maximum number of UObjects in cooked game
		GConfig->GetInt(TEXT("/Script/Engine.GarbageCollectionSettings"), TEXT("gc.MaxObjectsInGame"), MaxUObjects, GEngineIni);

		// If true, the UObjectArray will pre-allocate all entries for UObject pointers
		GConfig->GetBool(TEXT("/Script/Engine.GarbageCollectionSettings"), TEXT("gc.PreAllocateUObjectArray"), bPreAllocateUObjectArray, GEngineIni);
	}
	else
	{
#if IS_PROGRAM
		// Maximum number of UObjects for programs can be low
		MaxUObjects = 100000; // Default to 100K for programs
		GConfig->GetInt(TEXT("/Script/Engine.GarbageCollectionSettings"), TEXT("gc.MaxObjectsInProgram"), MaxUObjects, GEngineIni);
#else
		// Maximum number of UObjects in the editor
		GConfig->GetInt(TEXT("/Script/Engine.GarbageCollectionSettings"), TEXT("gc.MaxObjectsInEditor"), MaxUObjects, GEngineIni);
#endif
	}

	if (MaxObjectsNotConsideredByGC <= 0 && SizeOfPermanentObjectPool > 0)
	{
		// If permanent object pool is enabled but disregard for GC is disabled, GC will mark permanent object pool objects
		// as unreachable and may destroy them so disable permanent object pool too.
		// An alternative would be to make GC not mark permanent object pool objects as unreachable but then they would have to
		// be considered as root set objects because they could be referencing objects from outside of permanent object pool.
		// This would be inconsistent and confusing and also counter productive (the more root set objects the more expensive MarkAsUnreachable phase is).
		SizeOfPermanentObjectPool = 0;
		UE_LOG(LogInit, Warning, TEXT("Disabling permanent object pool because disregard for GC is disabled (gc.MaxObjectsNotConsideredByGC=%d)."), MaxObjectsNotConsideredByGC);
	}

	// Log what we're doing to track down what really happens as log in LaunchEngineLoop doesn't report those settings in pristine form.
	UE_LOG(LogInit, Log, TEXT("%s for max %d objects, including %i objects not considered by GC, pre-allocating %i bytes for permanent pool."), 
		bPreAllocateUObjectArray ? TEXT("Pre-allocating") : TEXT("Presizing"),
		MaxUObjects, MaxObjectsNotConsideredByGC, SizeOfPermanentObjectPool);

	GUObjectAllocator.AllocatePermanentObjectPool(SizeOfPermanentObjectPool);
	GUObjectArray.AllocateObjectPool(MaxUObjects, MaxObjectsNotConsideredByGC, bPreAllocateUObjectArray);
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
	UE::CoreUObject::Private::InitObjectHandles(GUObjectArray.GetObjectArrayCapacity());
#endif

	void InitGarbageElimination();
	InitGarbageElimination();

	void InitAsyncThread();
	InitAsyncThread();

	// Note initialized.
	Internal::GetUObjectSubsystemInitialised() = true;

	UObjectProcessRegistrants();
}

/**
 * Final phase of UObject shutdown
 */
void UObjectBaseShutdown()
{
	void ShutdownAsyncThread();
	ShutdownAsyncThread();

	GUObjectArray.ShutdownUObjectArray();
	Internal::GetUObjectSubsystemInitialised() = false;
}

/**
 * Helper function that can be used inside the debuggers watch window. E.g. "DebugFName(Class)". 
 *
 * @param	Object	Object to look up the name for 
 * @return			Associated name
 */
const TCHAR* DebugFName(UObject* Object)
{
	if ( Object )
	{
		// Hardcoded static array. This function is only used inside the debugger so it should be fine to return it.
		static TCHAR TempName[256];
		FName Name = Object->GetFName();
		FCString::Strcpy(TempName, *FName::SafeString(Name.GetDisplayIndex(), Name.GetNumber()));
		return TempName;
	}
	else
	{
		return TEXT("NULL");
	}
}

/**
 * Helper function that can be used inside the debuggers watch window. E.g. "DebugFName(Object)". 
 *
 * @param	Object	Object to look up the name for 
 * @return			Fully qualified path name
 */
const TCHAR* DebugPathName(UObject* Object)
{
	if( Object )
	{
		// Hardcoded static array. This function is only used inside the debugger so it should be fine to return it.
		static TCHAR PathName[1024];
		PathName[0] = TCHAR('\0');

		// Keep track of how many outers we have as we need to print them in inverse order.
		UObject*	TempObject = Object;
		int32			OuterCount = 0;
		while( TempObject )
		{
			TempObject = TempObject->GetOuter();
			OuterCount++;
		}

		// Iterate over each outer + self in reverse oder and append name.
		for( int32 OuterIndex=OuterCount-1; OuterIndex>=0; OuterIndex-- )
		{
			// Move to outer name.
			TempObject = Object;
			for( int32 i=0; i<OuterIndex; i++ )
			{
				TempObject = TempObject->GetOuter();
			}

			// Dot separate entries.
			if( OuterIndex != OuterCount -1 )
			{
				FCString::Strcat( PathName, TEXT(".") );
			}
			// And app end the name.
			FCString::Strcat( PathName, DebugFName( TempObject ) );
		}

		return PathName;
	}
	else
	{
		return TEXT("None");
	}
}

/**
 * Helper function that can be used inside the debuggers watch window. E.g. "DebugFName(Object)". 
 *
 * @param	Object	Object to look up the name for 
 * @return			Fully qualified path name prepended by class name
 */
const TCHAR* DebugFullName(UObject* Object)
{
	if( Object )
	{
		// Hardcoded static array. This function is only used inside the debugger so it should be fine to return it.
		static TCHAR FullName[1024];
		FullName[0] = TCHAR('\0');

		// Class Full.Path.Name
		FCString::Strcat( FullName, DebugFName(Object->GetClass()) );
		FCString::Strcat( FullName, TEXT(" "));
		FCString::Strcat( FullName, DebugPathName(Object) );

		return FullName;
	}
	else
	{
		return TEXT("None");
	}
}
