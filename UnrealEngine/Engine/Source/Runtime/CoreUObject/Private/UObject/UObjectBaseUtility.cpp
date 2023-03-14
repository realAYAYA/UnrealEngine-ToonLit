// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UObjectBaseUtility.cpp: Unreal UObject functions that only depend on UObjectBase
=============================================================================*/

#include "UObject/UObjectBaseUtility.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"
#include "Templates/Casts.h"
#include "UObject/Interface.h"
#include "Misc/StringBuilder.h"
#include "Modules/ModuleManager.h"
#include "ProfilingDebugging/MallocProfiler.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Containers/VersePath.h"

#if UE_USE_VERSE_PATHS
#include "Misc/PathViews.h"
#include "Interfaces/IPluginManager.h"
#endif

/***********************/
/******** Names ********/
/***********************/

/**
 * Returns the fully qualified pathname for this object, in the format:
 * 'Outermost.[Outer:]Name'
 *
 * @param	StopOuter	if specified, indicates that the output string should be relative to this object.  if StopOuter
 *						does not exist in this object's Outer chain, the result would be the same as passing NULL.
 *
 * @note	safe to call on NULL object pointers!
 */
FString UObjectBaseUtility::GetPathName( const UObject* StopOuter/*=NULL*/ ) const
{
	FString Result;
	GetPathName(StopOuter, Result);
	return Result;
}

/**
 * Version of GetPathName() that eliminates unnecessary copies and appends an existing string.
 */
void UObjectBaseUtility::GetPathName(const UObject* StopOuter, FString& ResultString) const
{
	TStringBuilder<256> ResultBuilder;
	GetPathName(StopOuter, ResultBuilder);
	ResultString += FStringView(ResultBuilder);
}

void UObjectBaseUtility::GetPathName(const UObject* StopOuter, FStringBuilderBase& ResultString) const
{
	if(this != StopOuter && this != NULL)
	{
		UObject* ObjOuter = GetOuter();
		if (ObjOuter && ObjOuter != StopOuter)
		{
			ObjOuter->GetPathName(StopOuter, ResultString);

			// SUBOBJECT_DELIMITER_CHAR is used to indicate that this object's outer is not a UPackage
			if (ObjOuter->GetClass() != UPackage::StaticClass()
			&& ObjOuter->GetOuter()->GetClass() == UPackage::StaticClass())
			{
				ResultString << SUBOBJECT_DELIMITER_CHAR;
			}
			else
			{
				ResultString << TEXT('.');
			}
		}
		GetFName().AppendString(ResultString);
	}
	else
	{
		ResultString << TEXT("None");
	}
}

/**
 * Returns the fully qualified pathname for this object as well as the name of the class, in the format:
 * 'ClassName Outermost.[Outer:]Name'.
 *
 * @param	StopOuter	if specified, indicates that the output string should be relative to this object.  if StopOuter
 *						does not exist in this object's Outer chain, the result would be the same as passing NULL.
 *
 * @note	safe to call on NULL object pointers!
 */
FString UObjectBaseUtility::GetFullName(const UObject* StopOuter, EObjectFullNameFlags Flags) const
{
	FString Result;
	Result.Empty(128);
	GetFullName(StopOuter, Result, Flags);
	return Result;
}

 /**
  * Version of GetFullName() that eliminates unnecessary copies and appends an existing string.
  */
void UObjectBaseUtility::GetFullName(const UObject* StopOuter, FString& ResultString, EObjectFullNameFlags Flags) const
{
	TStringBuilder<256> StringBuilder;
	GetFullName(StringBuilder, StopOuter, Flags);
	ResultString.Append(StringBuilder);
}

void UObjectBaseUtility::GetFullName(FStringBuilderBase& ResultString, const UObject* StopOuter, EObjectFullNameFlags Flags) const
{
	if (this != nullptr)
	{
		if (EnumHasAllFlags(Flags, EObjectFullNameFlags::IncludeClassPackage))
		{
			GetClass()->GetPathName(nullptr, ResultString);
		}
		else
		{
			GetClass()->GetFName().AppendString(ResultString);
		}
		ResultString += TEXT(' ');
		GetPathName(StopOuter, ResultString);
	}
	else
	{
		ResultString += TEXT("None");
	}
}


/**
 * Walks up the chain of packages until it reaches the top level, which it ignores.
 *
 * @param	bStartWithOuter		whether to include this object's name in the returned string
 * @return	string containing the path name for this object, minus the outermost-package's name
 */
FString UObjectBaseUtility::GetFullGroupName( bool bStartWithOuter ) const
{
	const UObjectBaseUtility* Obj = bStartWithOuter ? GetOuter() : this;
	return Obj ? Obj->GetPathName(GetOutermost()) : TEXT("");
}


/***********************/
/*** Outer & Package ***/
/***********************/

bool UObjectBaseUtility::IsPackageExternal() const
{
	return HasAnyFlags(RF_HasExternalPackage);
}

void UObjectBaseUtility::DetachExternalPackage()
{
	ClearFlags(RF_HasExternalPackage);
}

void UObjectBaseUtility::ReattachExternalPackage()
{
	// GetObjectExternalPackageThreadSafe doesn't check for the RF_HasExternalPackage before looking up the external package
	if (!HasAnyFlags(RF_HasExternalPackage) && GetObjectExternalPackageThreadSafe(this))
	{
		SetFlags(RF_HasExternalPackage);
	}
}

/**
 * Walks up the list of outers until it finds the top-level one that isn't a package.
 * Will return null if called on a package
 * @return outermost non-null, non-package Outer.
 */
UObject* UObjectBaseUtility::GetOutermostObject() const
{
	UObject* Top = (UObject*)(this);
	if (Top->IsA<UPackage>())
	{
		return nullptr;
	}
	for (;;)
	{
		UObject* CurrentOuter = Top->GetOuter();
		if (CurrentOuter->IsA<UPackage>())
		{
			return Top;
		}
		Top = CurrentOuter;
	}
}

/**
 * Walks up the list of outers until it finds a package directly associated with the object.
 *
 * @return the package the object is in.
 */
UPackage* UObjectBaseUtility::GetPackage() const
{
	const UObject* Top = static_cast<const UObject*>(this);
	for (;;)
	{
		// GetExternalPackage will return itself if called on a UPackage
		if (UPackage* Package = Top->GetExternalPackage())
		{
			return Package;
		}
		Top = Top->GetOuter();
	}
}

#if UE_USE_VERSE_PATHS

UE::Core::FVersePath UObjectBaseUtility::GetVersePath() const
{
	UObject* ThisOuter = GetOuter();
	const UObjectBaseUtility* Outermost = nullptr;
	const UObjectBaseUtility* Src       = nullptr;
	if (!ThisOuter)
	{
		Outermost = this;
		Src       = this;
	}
	else
	{
		UObject* OuterOuter = ThisOuter->GetOuter();
		if (!OuterOuter)
		{
			Outermost = ThisOuter;
			Src       = this;
		}
	}

	// We only handle vpaths at the level of the package and top level objects right now
	if (!Outermost)
	{
		return {};
	}

	FString PackageName = Outermost->GetPathName();
	const FStringView MountPointName = FPathViews::GetMountPointNameFromPath(PackageName);
	check(PackageName.StartsWith(TEXT("/") + FString(MountPointName)));

	IPluginManager& PluginManager = IPluginManager::Get();

	// If the object isn't mounted under a plugin, it doesn't have a vpath
	TSharedPtr<IPlugin> Plugin = PluginManager.FindPlugin(MountPointName);
	if (!Plugin)
	{
		return {};
	}

	// If the plugin doesn't have a root vpath, it's not a UEFN plugin
	FString PluginVersePath = Plugin->GetVersePath();
	if (PluginVersePath.IsEmpty())
	{
		return {};
	}

	FString VerseModule = FPaths::Combine(PluginVersePath, PackageName.RightChop(MountPointName.Len() + 1));

	// If this is not the package, append the name of the object
	if (this != Outermost)
	{
		VerseModule = FPaths::Combine(MoveTemp(VerseModule), Src->GetName());
	}

	// Hack to reject names containing "$" - currently used for non-user facing vobject names in Verse, e.g. $SolarisSignatureFunctionOuter
	if (VerseModule.Contains("$"))
	{
		return {};
	}

	UE::Core::FVersePath Result;
	if (!UE::Core::FVersePath::TryMake(Result, VerseModule))
	{
#if !NO_LOGGING
		static thread_local TSet<FString> AlreadyLogged;

		bool bAlreadyInSet = false;
		AlreadyLogged.Add(VerseModule, &bAlreadyInSet);
		if (!bAlreadyInSet)
		{
			UE_LOG(LogCore, Warning, TEXT("Unable to make a VersePath from %s"), *GetPathName());
		}
#endif
	}

	return Result;
}

#endif

/**
 * Legacy function, has the same behavior as GetPackage
 * use GetPackage instead.
 * @return the package the object is in.
 * @see GetPackage
 */
UPackage* UObjectBaseUtility::GetOutermost() const
{
	return GetPackage();
}

/** 
 * Finds the outermost package and marks it dirty
 */
bool UObjectBaseUtility::MarkPackageDirty() const
{
	// since transient objects will never be saved into a package, there is no need to mark a package dirty
	// if we're transient
	if ( !HasAnyFlags(RF_Transient) )
	{
		UPackage* Package = GetOutermost();

		if( Package != NULL	)
		{
			// It is against policy to dirty a map or package during load in the Editor, to enforce this policy
			// we explicitly disable the ability to dirty a package or map during load.  Commandlets can still
			// set the dirty state on load.
			if( IsRunningCommandlet() || 
				(GIsEditor && !GIsEditorLoadingPackage && !GIsCookerLoadingPackage && !GIsPlayInEditorWorld && !IsInAsyncLoadingThread() && !IsReloadActive()
#if WITH_EDITORONLY_DATA
				&& !Package->bIsCookedForEditor // Cooked packages can't be modified nor marked as dirty
#endif
				))
			{
				const bool bIsDirty = Package->IsDirty();

				// We prevent needless re-dirtying as this can be an expensive operation.
				if( !bIsDirty )
				{
					Package->SetDirtyFlag(true);
				}

				// Always call PackageMarkedDirtyEvent, even when the package is already dirty
				Package->PackageMarkedDirtyEvent.Broadcast(Package, bIsDirty);

				return true;
			}
			else
			{
				// notify the caller that the request to mark the package as dirty was suppressed
				return false;
			}
		}
	}
	return true;
}

/**
* Determines whether this object is a template object
*
* @return	true if this object is a template object (owned by a UClass)
*/
bool UObjectBaseUtility::IsTemplate( EObjectFlags TemplateTypes ) const
{
	for (const UObjectBaseUtility* TestOuter = this; TestOuter; TestOuter = TestOuter->GetOuter() )
	{
		if ( TestOuter->HasAnyFlags(TemplateTypes) )
			return true;
	}

	return false;
}


/**
 * Traverses the outer chain searching for the next object of a certain type.  (T must be derived from UObject)
 *
 * @param	Target class to search for
 * @return	a pointer to the first object in this object's Outer chain which is of the correct type.
 */
UObject* UObjectBaseUtility::GetTypedOuter(UClass* Target) const
{
	ensureMsgf(Target != UPackage::StaticClass(), TEXT("Calling GetTypedOuter to retrieve a package is now invalid, you should use GetPackage() instead."));

	UObject* Result = NULL;
	for ( UObject* NextOuter = GetOuter(); Result == NULL && NextOuter != NULL; NextOuter = NextOuter->GetOuter() )
	{
		if ( NextOuter->IsA(Target) )
		{
			Result = NextOuter;
		}
	}
	return Result;
}

UObjectBaseUtility* UObjectBaseUtility::GetImplementingOuterObject(const UClass* InInterfaceClass) const
{
	for (UObject* NextOuter = GetOuter(); NextOuter != nullptr; NextOuter = NextOuter->GetOuter())
	{
		UClass* OuterClass = NextOuter->GetClass();
		if (OuterClass && OuterClass->ImplementsInterface(InInterfaceClass))
		{
			return NextOuter;
		}
	}

	return nullptr;
}

/*-----------------------------------------------------------------------------
	UObject accessors that depend on UClass.
-----------------------------------------------------------------------------*/

/**
 * @return	true if the specified object appears somewhere in this object's outer chain.
 */
bool UObjectBaseUtility::IsIn( const UObject* SomeOuter ) const
{
	if (SomeOuter->IsA<UPackage>())
	{
		return IsInPackage(static_cast<const UPackage*>(SomeOuter));
	}
	return IsInOuter(SomeOuter);
}

/** Overload to determine if an object is in the specified package which can now be different than its outer chain. */
bool UObjectBaseUtility::IsIn(const UPackage* SomePackage) const
{
	// uncomment the ensure to more easily find where IsIn should be changed to IsInPackage
	//ensure(0);
	return IsInPackage(SomePackage);
}

bool UObjectBaseUtility::IsInOuter(const UObject* SomeOuter) const
{
	for (UObject* It = GetOuter(); It; It = It->GetOuter())
	{
		if (It == SomeOuter)
		{
			return true;
		}
	}
	return SomeOuter == nullptr;
}

/**
 * @return	true if the object is contained in the specified package.
 */
bool UObjectBaseUtility::IsInPackage(const UPackage* SomePackage) const
{
	return SomePackage != this && GetPackage() == SomePackage;
}

/**
 * Find out if this object is inside (has an outer) that is of the specified class
 * @param SomeBaseClass	The base class to compare against
 * @return True if this object is in an object of the given type.
 */
bool UObjectBaseUtility::IsInA( const UClass* SomeBaseClass ) const
{
	for (const UObjectBaseUtility* It=this; It; It = It->GetOuter())
	{
		if (It->IsA(SomeBaseClass))
		{
			return true;
		}
	}
	return SomeBaseClass == NULL;
}

/**
* Checks whether this object's top-most package has any of the specified flags
*
* @param	CheckFlagMask	a bitmask of EPackageFlags values to check for
*
* @return	true if the PackageFlags member of this object's top-package has any bits from the mask set.
*/
bool UObjectBaseUtility::RootPackageHasAnyFlags( uint32 CheckFlagMask ) const
{
	return GetOutermost()->HasAnyPackageFlags(CheckFlagMask);
}

/***********************/
/******** Class ********/
/***********************/

/**
 * Finds the most-derived class which is a parent of both TestClass and this object's class.
 *
 * @param	TestClass	the class to find the common base for
 */
const UClass* UObjectBaseUtility::FindNearestCommonBaseClass( const UClass* TestClass ) const
{
	const UClass* Result = NULL;

	if ( TestClass != NULL )
	{
		const UClass* CurrentClass = GetClass();

		// early out if it's the same class or one is the parent of the other
		// (the check for TestClass->IsChildOf(CurrentClass) returns true if TestClass == CurrentClass
		if ( TestClass->IsChildOf(CurrentClass) )
		{
			Result = CurrentClass;
		}
		else if ( CurrentClass->IsChildOf(TestClass) )
		{
			Result = TestClass;
		}
		else
		{
			// find the nearest parent of TestClass which is also a parent of CurrentClass
			for ( UClass* Cls = TestClass->GetSuperClass(); Cls; Cls = Cls->GetSuperClass() )
			{
				if ( CurrentClass->IsChildOf(Cls) )
				{
					Result = Cls;
					break;
				}
			}
		}
	}

	// at this point, Result should only be NULL if TestClass is NULL
	checkfSlow(Result != NULL || TestClass == NULL, TEXT("No common base class found for object '%s' with TestClass '%s'"), *GetFullName(), *TestClass->GetFullName());
	return Result;
}


/**
 * Returns a pointer to this object safely converted to a pointer to the specified interface class.
 *
 * @param	InterfaceClass	the interface class to use for the returned type
 *
 * @return	a pointer that can be assigned to a variable of the interface type specified, or NULL if this object's
 *			class doesn't implement the interface indicated.  Will be the same value as 'this' if the interface class
 *			isn't native.
 */
void* UObjectBaseUtility::GetInterfaceAddress( UClass* InterfaceClass )
{
	void* Result = NULL;

	if ( InterfaceClass != NULL && InterfaceClass->HasAnyClassFlags(CLASS_Interface) && InterfaceClass != UInterface::StaticClass() )
	{
		// Script interface
		if ( !InterfaceClass->HasAnyClassFlags(CLASS_Native) )
		{
			if ( GetClass()->ImplementsInterface(InterfaceClass) )
			{
				// if it isn't a native interface, the address won't be different
				Result = this;
			}
		}
		// Native interface
		else
		{
			for( UClass* CurrentClass=GetClass(); Result == NULL && CurrentClass != NULL; CurrentClass = CurrentClass->GetSuperClass() )
			{
				for (TArray<FImplementedInterface>::TIterator It(CurrentClass->Interfaces); It; ++It)
				{
					// See if this is the implementation we are looking for, and it was done natively, not in K2
					FImplementedInterface& ImplInterface = *It;
					if ( !ImplInterface.bImplementedByK2 && ImplInterface.Class->IsChildOf(InterfaceClass) )
					{
						Result = (uint8*)this + It->PointerOffset;
						break;
					}
				}
			}
		}
	}

	return Result;
}

void* UObjectBaseUtility::GetNativeInterfaceAddress(UClass* InterfaceClass)
{
	check(InterfaceClass != NULL);
	check(InterfaceClass->HasAllClassFlags(CLASS_Interface | CLASS_Native));
	check(InterfaceClass != UInterface::StaticClass());

	for( UClass* CurrentClass=GetClass(); CurrentClass; CurrentClass = CurrentClass->GetSuperClass() )
	{
		for (auto It = CurrentClass->Interfaces.CreateConstIterator(); It; ++It)
		{
			// See if this is the implementation we are looking for, and it was done natively, not in K2
			auto& ImplInterface = *It;
			if ( !ImplInterface.bImplementedByK2 && ImplInterface.Class->IsChildOf(InterfaceClass) )
			{
				if ( It->PointerOffset )
				{
					return (uint8*)this + It->PointerOffset;
				}
			}
		}
	}

	return NULL;
}

bool UObjectBaseUtility::IsDefaultSubobject() const
{
	const bool bIsInstanced = !HasAnyFlags(RF_ClassDefaultObject) && GetOuter() && (GetOuter()->HasAnyFlags(RF_ClassDefaultObject) || ((UObject*)this)->GetArchetype() != GetClass()->GetDefaultObject(false));
	return bIsInstanced;
}


UClass* GetParentNativeClass(UClass* Class)
{
	while (Class && !Class->IsNative())
	{
		Class = Class->GetSuperClass();
	}

	return Class;
}

#if STATS && USE_MALLOC_PROFILER
void FScopeCycleCounterUObject::TrackObjectForMallocProfiling(const UObjectBaseUtility *InObject)
{
	// Get the package name from the outermost item (if available - can't use GetOutermost here)
	FName PackageName;
	if (InObject->GetOuter())
	{
		UObjectBaseUtility* Top = InObject->GetOuter();
		for (;;)
		{
			UObjectBaseUtility* CurrentOuter = Top->GetOuter();
			if (!CurrentOuter)
			{
				PackageName = Top->GetFName();
				break;
			}
			Top = CurrentOuter;
		}
	}

	// Get the class name (if available)
	FName ClassName;
	if (InObject->GetClass())
	{
		ClassName = InObject->GetClass()->GetFName();
	}

	TrackObjectForMallocProfiling(PackageName, ClassName, InObject->GetFName());
}
void FScopeCycleCounterUObject::TrackObjectForMallocProfiling(const FName InPackageName, const FName InClassName, const FName InObjectName)
{
	static const TCHAR PackageTagCategory[] = TEXT("Package:");
	static const TCHAR ObjectTagCategory[] = TEXT("Object:");
	static const TCHAR ClassTagCategory[] = TEXT("Class:");

	// We use an array rather than an FString to try and minimize heap allocations
	TArray<TCHAR, TInlineAllocator<256>> ScratchSpaceBuffer;

	auto AppendNameToBuffer = [&](const FName InName)
	{
		const FNameEntry* NameEntry = InName.GetDisplayNameEntry();
		if (NameEntry->IsWide())
		{
			WIDECHAR WideName[NAME_SIZE];
			NameEntry->GetWideName(WideName);
			const WIDECHAR* NameCharPtr = WideName;
			while (*NameCharPtr != 0)
			{
				ScratchSpaceBuffer.Add((TCHAR)*NameCharPtr++);
			}
		}
		else
		{
			ANSICHAR AnsiName[NAME_SIZE];
			NameEntry->GetAnsiName(AnsiName);
			const ANSICHAR* NameCharPtr = AnsiName;
			while (*NameCharPtr != 0)
			{
				ScratchSpaceBuffer.Add((TCHAR)*NameCharPtr++);
			}
		}
	};

	if (!InPackageName.IsNone())
	{
		// "Package:/Path/To/Package"
		ScratchSpaceBuffer.Reset();
		ScratchSpaceBuffer.Append(PackageTagCategory, UE_ARRAY_COUNT(PackageTagCategory) - 1);
		AppendNameToBuffer(InPackageName);
		ScratchSpaceBuffer.Add(0);
		PackageTag = FName(ScratchSpaceBuffer.GetData());
		GMallocProfiler->AddTag(PackageTag);

		// "Object:/Path/To/Package/ObjectName"
		ScratchSpaceBuffer.Reset();
		ScratchSpaceBuffer.Append(ObjectTagCategory, UE_ARRAY_COUNT(ObjectTagCategory) - 1);
		AppendNameToBuffer(InPackageName);
		ScratchSpaceBuffer.Add(TEXT('/'));
		AppendNameToBuffer(InObjectName);
		ScratchSpaceBuffer.Add(0);
		ObjectTag = FName(ScratchSpaceBuffer.GetData());
		GMallocProfiler->AddTag(ObjectTag);
	}

	if (!InClassName.IsNone())
	{
		// "Class:ClassName"
		ScratchSpaceBuffer.Reset();
		ScratchSpaceBuffer.Append(ClassTagCategory, UE_ARRAY_COUNT(ClassTagCategory) - 1);
		AppendNameToBuffer(InClassName);
		ScratchSpaceBuffer.Add(0);
		ClassTag = FName(ScratchSpaceBuffer.GetData());
		GMallocProfiler->AddTag(ClassTag);
	}
}
void FScopeCycleCounterUObject::UntrackObjectForMallocProfiling()
{
	if (!PackageTag.IsNone())
	{
		GMallocProfiler->RemoveTag(PackageTag);
	}

	if (!ClassTag.IsNone())
	{
		GMallocProfiler->RemoveTag(ClassTag);
	}

	if (!ObjectTag.IsNone())
	{
		GMallocProfiler->RemoveTag(ObjectTag);
	}
}
#endif

#if !STATS && !ENABLE_STATNAMEDEVENTS && USE_LIGHTWEIGHT_STATS_FOR_HITCH_DETECTION && USE_HITCH_DETECTION && USE_LIGHTWEIGHT_UOBJECT_STATS_FOR_HITCH_DETECTION
#include "HAL/ThreadHeartBeat.h"
#include "HAL/ThreadManager.h"

void FScopeCycleCounterUObject::ReportHitch()
{
	float Delta = float(FGameThreadHitchHeartBeat::Get().GetCurrentTime() - FGameThreadHitchHeartBeat::Get().GetFrameStartTime()) * 1000.0f;
	const uint32 CurrentThreadId = FPlatformTLS::GetCurrentThreadId();
	const FString& ThreadString = FThreadManager::GetThreadName(CurrentThreadId);
	FString StackString;
	if (CurrentThreadId == GGameThreadId)
	{
		if (StatObject->IsValidLowLevel() && StatObject->IsValidLowLevelFast())
		{
			StackString = GetFullNameSafe(StatObject);
		}
		else
		{
			StackString = FString(TEXT("[UObject was invalid]"));
		}
	}
	else
	{
		StackString = FString(TEXT("[Not grabbing UObject name from other threads]"));
	}
	UE_LOG(LogCore, Error, TEXT("Leaving UObject scope on hitch (+%8.2fms) [%s] %s"), Delta, *ThreadString, *StackString);
}
#endif


