// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "ProfilingDebugging/LoadTimeTracker.h"
#include "Serialization/CustomVersion.h"
#include "UObject/UObjectBaseUtility.h"
#include "UObject/Package.h"
#include "UObject/LinkerLoad.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"

/**
 * Returns the UE version of the linker for this object.
 *
 * @return	the UEversion of the engine's package file when this object
 *			was last saved, or GPackageFileUEVersion (current version) if
 *			this object does not have a linker, which indicates that
 *			a) this object is a native only class, or
 *			b) this object's linker has been detached, in which case it is already fully loaded
 */
FPackageFileVersion UObjectBaseUtility::GetLinkerUEVersion() const
{
	FLinkerLoad* Loader = GetLinker();

	// No linker.
	if (Loader == nullptr)
	{
		// the _Linker reference is never set for the top-most UPackage of a package (the linker root), so if this object
		// is the linker root, find our loader in the global list.
		if (GetOutermost() == this)
		{
			Loader = FLinkerLoad::FindExistingLinkerForPackage(const_cast<UPackage*>(CastChecked<UPackage>((const UObject*)this)));
		}
	}

	if (Loader != nullptr)
	{
		// We have a linker so we can return its version.
		return Loader->UEVer();

	}
	else if (GetOutermost())
	{
		// Get the linker version associated with the package this object lives in
		return GetOutermost()->GetLinkerPackageVersion();
	}
	else
	{
		// We don't have a linker associated as we e.g. might have been saved or had loaders reset, ...
		return GPackageFileUEVersion;
	}
}

int32 UObjectBaseUtility::GetLinkerCustomVersion(FGuid CustomVersionKey) const
{
	FLinkerLoad* Loader = GetLinker();

	// No linker.
	if( Loader == nullptr )
	{
		// the _Linker reference is never set for the top-most UPackage of a package (the linker root), so if this object
		// is the linker root, find our loader in the global list.
		if( GetOutermost() == this )
		{
			Loader = FLinkerLoad::FindExistingLinkerForPackage(const_cast<UPackage*>(CastChecked<UPackage>((const UObject*)this)));
		}
	}

	if ( Loader != nullptr)
	{
		// We have a linker so we can return its version.
		const FCustomVersion* CustomVersion = Loader->Summary.GetCustomVersionContainer().GetVersion(CustomVersionKey);
		return CustomVersion ? CustomVersion->Version : -1;
	}
	else if (GetOutermost() && GetOutermost()->GetLinkerCustomVersions().GetAllVersions().Num())
	{
		// Get the linker version associated with the package this object lives in
		const FCustomVersion* CustomVersion = GetOutermost()->GetLinkerCustomVersions().GetVersion(CustomVersionKey);
		return CustomVersion ? CustomVersion->Version : -1;
	}
	// We don't have a linker associated as we e.g. might have been saved or had loaders reset, ...
	// We must have a current version for this tag.
	return FCurrentCustomVersions::Get(CustomVersionKey).GetValue().Version;
}

/**
 * Returns the licensee version of the linker for this object.
 *
 * @return	the licensee version of the engine's package file when this object
 *			was last saved, or GPackageFileLicenseeVersion (current version) if
 *			this object does not have a linker, which indicates that
 *			a) this object is a native only class, or
 *			b) this object's linker has been detached, in which case it is already fully loaded
 */
int32 UObjectBaseUtility::GetLinkerLicenseeUEVersion() const
{
	FLinkerLoad* Loader = GetLinker();

	// No linker.
	if( Loader == nullptr)
	{
		// the _Linker reference is never set for the top-most UPackage of a package (the linker root), so if this object
		// is the linker root, find our loader in the global list.
		if( GetOutermost() == this )
		{
			Loader = FLinkerLoad::FindExistingLinkerForPackage(const_cast<UPackage*>(CastChecked<UPackage>((const UObject*)this)));
		}
	}

	if ( Loader != nullptr)
	{
		// We have a linker so we can return its version.
		return Loader->LicenseeUEVer();
	}
	else if (GetOutermost())
	{
		// Get the linker version associated with the package this object lives in
		return GetOutermost()->GetLinkerLicenseeVersion();
	}
	else
	{
		// We don't have a linker associated as we e.g. might have been saved or had loaders reset, ...
		return GPackageFileLicenseeUEVersion;
	}
}

#if CPUPROFILERTRACE_ENABLED
COREUOBJECT_API FName GetClassTraceScope(const UObjectBaseUtility* Object)
{
	UClass* Class = Object->GetClass();
	if (Class->IsNative())
	{
		return Class->GetFName();
	}
	else
	{
		static const FName NAME_Blueprint(TEXT("Blueprint"));
		return NAME_Blueprint;
	}
}
#endif

#if STATS && CPUPROFILERTRACE_ENABLED
void FScopeCycleCounterUObject::StartObjectTrace(const UObjectBaseUtility* Object)
{
#if LOADTIMEPROFILERTRACE_ENABLED
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(AssetLoadTimeChannel))
	{
		StartTrace(Object->GetFName());
	}
	else
#endif
	{
		StartTrace(GetClassTraceScope(Object));
	}
}
#endif


// Console variable so that GarbageCollectorSettings work in the editor but we don't want to use it in runtime as we can't support changing its value from console
int32 GPendingKillEnabled = 1;
static FAutoConsoleVariableRef CVarPendingKillEnabled(
	TEXT("gc.PendingKillEnabled"),
	GPendingKillEnabled,
	TEXT("If true, objects marked as PendingKill will be automatically nulled and destroyed by Garbage Collector."),
	ECVF_Default
);
bool UObjectBaseUtility::bPendingKillDisabled = !GPendingKillEnabled;

void InitNoPendingKill()
{
#if !UE_BUILD_SHIPPING
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	{
		UObjectBaseUtility::bPendingKillDisabled = false;
		check(UObjectBaseUtility::FixGarbageOrPendingKillInternalObjectFlags(EInternalObjectFlags::None) == EInternalObjectFlags::None);
		check(UObjectBaseUtility::FixGarbageOrPendingKillInternalObjectFlags(EInternalObjectFlags::PendingKill) == EInternalObjectFlags::PendingKill);
		check(UObjectBaseUtility::FixGarbageOrPendingKillInternalObjectFlags(EInternalObjectFlags::Garbage) == EInternalObjectFlags::PendingKill);
		check(UObjectBaseUtility::FixGarbageOrPendingKillInternalObjectFlags(EInternalObjectFlags::PendingKill | EInternalObjectFlags::Garbage) == EInternalObjectFlags::PendingKill);
		check(UObjectBaseUtility::FixGarbageOrPendingKillInternalObjectFlags(EInternalObjectFlags::RootSet | EInternalObjectFlags::PendingKill) == (EInternalObjectFlags::RootSet | EInternalObjectFlags::PendingKill));
		check(UObjectBaseUtility::FixGarbageOrPendingKillInternalObjectFlags(EInternalObjectFlags::RootSet | EInternalObjectFlags::Garbage) == (EInternalObjectFlags::RootSet | EInternalObjectFlags::PendingKill));
		UObjectBaseUtility::bPendingKillDisabled = true;
		check(UObjectBaseUtility::FixGarbageOrPendingKillInternalObjectFlags(EInternalObjectFlags::None) == EInternalObjectFlags::None);
		check(UObjectBaseUtility::FixGarbageOrPendingKillInternalObjectFlags(EInternalObjectFlags::PendingKill) == EInternalObjectFlags::Garbage);
		check(UObjectBaseUtility::FixGarbageOrPendingKillInternalObjectFlags(EInternalObjectFlags::Garbage) == EInternalObjectFlags::Garbage);
		check(UObjectBaseUtility::FixGarbageOrPendingKillInternalObjectFlags(EInternalObjectFlags::PendingKill | EInternalObjectFlags::Garbage) == EInternalObjectFlags::Garbage);
		check(UObjectBaseUtility::FixGarbageOrPendingKillInternalObjectFlags(EInternalObjectFlags::RootSet | EInternalObjectFlags::PendingKill) == (EInternalObjectFlags::RootSet | EInternalObjectFlags::Garbage));
		check(UObjectBaseUtility::FixGarbageOrPendingKillInternalObjectFlags(EInternalObjectFlags::RootSet | EInternalObjectFlags::Garbage) == (EInternalObjectFlags::RootSet | EInternalObjectFlags::Garbage));
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif // !UE_BUILD_SHIPPING

	check(GConfig);
	bool bPendingKillEnabled = false;
	GConfig->GetBool(TEXT("/Script/Engine.GarbageCollectionSettings"), TEXT("gc.PendingKillEnabled"), bPendingKillEnabled, GEngineIni);
	// Allow command-line overrides for easy testing without unique builds.
	if (FParse::Param(FCommandLine::Get(), TEXT("DisablePendingKill")))
	{
		bPendingKillEnabled = false;
	}
	else if (FParse::Param(FCommandLine::Get(), TEXT("EnablePendingKill")))
	{
		bPendingKillEnabled = true;
	}
	// Try to sync even though we're not gonna use the console var
	UObjectBaseUtility::bPendingKillDisabled = !bPendingKillEnabled;
	GPendingKillEnabled = bPendingKillEnabled;
}
