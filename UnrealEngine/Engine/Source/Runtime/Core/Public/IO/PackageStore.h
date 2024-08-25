// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "Delegates/Delegate.h"
#include "HAL/PlatformCrt.h"
#include "Misc/EnumClassFlags.h"
#include "Misc/SecureHash.h"
#include "PackageId.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"

class FArchive;
class FCbObject;
class FCbWriter;
class FStructuredArchiveSlot;

/**
 * Package store entry status.
 */
enum class EPackageStoreEntryStatus
{
	None,
	Missing,
	Pending,
	Ok,
};

/**
 * Package store entry.
 */ 
struct FPackageStoreEntry
{
	TArrayView<const FPackageId> ImportedPackageIds;
	TArrayView<const FSHAHash> ShaderMapHashes;
#if WITH_EDITOR
	TArrayView<const FPackageId> OptionalSegmentImportedPackageIds;
	bool bHasOptionalSegment = false;
#endif
};

/**
 * Package store entry flags.
 */
enum class EPackageStoreEntryFlags : uint32
{
	None		= 0,
	AutoOptional= 0x02,
	OptionalSegment = 0x04,
};
ENUM_CLASS_FLAGS(EPackageStoreEntryFlags);

/**
 * Package store entry resource.
 *
 * This is a non-optimized serializable version
 * of a package store entry. Used when cooking
 * and when running cook-on-the-fly.
 */
struct FPackageStoreEntryResource
{
	/** The package store entry flags. */
	EPackageStoreEntryFlags Flags = EPackageStoreEntryFlags::None;
	/** The package name. */
	FName PackageName;
	FPackageId PackageId;
	/** Imported package IDs. */
	TArray<FPackageId> ImportedPackageIds;
	/** Referenced shader map hashes. */
	TArray<FSHAHash> ShaderMapHashes;
	/** Editor data imported package IDs. */
	TArray<FPackageId> OptionalSegmentImportedPackageIds;

	/** Returns the package ID. */
	FPackageId GetPackageId() const
	{
		return PackageId;
	}

	/** Returns whether this package was saved as auto optional */
	bool IsAutoOptional() const
	{
		return EnumHasAnyFlags(Flags, EPackageStoreEntryFlags::AutoOptional);
	}

	bool HasOptionalSegment() const
	{
		return EnumHasAnyFlags(Flags, EPackageStoreEntryFlags::OptionalSegment);
	}

	CORE_API friend FArchive& operator<<(FArchive& Ar, FPackageStoreEntryResource& PackageStoreEntry);
	
	CORE_API friend FCbWriter& operator<<(FCbWriter& Writer, const FPackageStoreEntryResource& PackageStoreEntry);
	
	CORE_API static FPackageStoreEntryResource FromCbObject(const FCbObject& Obj);
};

/**
 * Stores information about available packages that can be loaded.
 */
class UE_DEPRECATED(5.1, "Use IPackageStoreBackend instead") IPackageStore
{
public:
	/* Destructor. */
	virtual ~IPackageStore() { }

	virtual void Initialize() = 0;

	/** Lock the package store for reading */
	virtual void Lock() = 0;

	/** Unlock the package store */
	virtual void Unlock() = 0;

	/* Returns whether the package exists. */
	virtual bool DoesPackageExist(FPackageId PackageId) = 0;

	/* Returns the package store entry data with export info and imported packages for the specified package ID. */
	virtual EPackageStoreEntryStatus GetPackageStoreEntry(FPackageId PackageId, FName PackageName,
		FPackageStoreEntry& OutPackageStoreEntry) = 0;

	/* Returns the redirected package ID and source package name for the specified package ID if it's being redirected. */
	virtual bool GetPackageRedirectInfo(FPackageId PackageId, FName& OutSourcePackageName, FPackageId& OutRedirectedToPackageId) = 0;

	/* Event broadcasted when pending entries are completed and added to the package store */
	DECLARE_EVENT(IPackageStore, FEntriesAddedEvent);
	virtual FEntriesAddedEvent& OnPendingEntriesAdded() = 0;
};

class UE_DEPRECATED(5.1, "Use IPackageStoreBackend instead") FPackageStoreBase
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	: public IPackageStore
PRAGMA_ENABLE_DEPRECATION_WARNINGS
{
public:
	virtual FEntriesAddedEvent& OnPendingEntriesAdded() override
	{
		return PendingEntriesAdded;
	}

protected:
	FEntriesAddedEvent PendingEntriesAdded;
};

class FPackageStoreBackendContext
{
public:
	/* Event broadcasted when pending entries are completed and added to the package store */
	DECLARE_EVENT(FPackageStoreBackendContext, FPendingEntriesAddedEvent);
	FPendingEntriesAddedEvent PendingEntriesAdded;
};

/**
 * Package store backend interface.
 */
class IPackageStoreBackend
{
public:
	/* Destructor. */
	virtual ~IPackageStoreBackend() { }

	/** Called when the backend is mounted */
	virtual void OnMounted(TSharedRef<const FPackageStoreBackendContext> Context) = 0;

	/** Called when the loader enters a package store read scope. */
	virtual void BeginRead() = 0;

	/** Called when the loader exits a package store read scope. */
	virtual void EndRead() = 0;

	/* Returns the package store entry data with export info and imported packages for the specified package ID. */
	virtual EPackageStoreEntryStatus GetPackageStoreEntry(FPackageId PackageId, FName PackageName,
		FPackageStoreEntry& OutPackageStoreEntry) = 0;

	/* Returns the redirected package ID and source package name for the specified package ID if it's being redirected. */
	virtual bool GetPackageRedirectInfo(FPackageId PackageId, FName& OutSourcePackageName, FPackageId& OutRedirectedToPackageId) = 0;
};

/**
 * Stores information about available packages that can be loaded.
 */
class FPackageStore
{
public:
	CORE_API static FPackageStore& Get();

	/* Mount a package store backend. */
	CORE_API void Mount(TSharedRef<IPackageStoreBackend> Backend, int32 Priority = 0);

	/* Returns the package store entry data with export info and imported packages for the specified package ID. */
	CORE_API EPackageStoreEntryStatus GetPackageStoreEntry(FPackageId PackageId, FName PackageName, 
		FPackageStoreEntry& OutPackageStoreEntry);

	/* Returns the redirected package ID and source package name for the specified package ID if it's being redirected. */
	CORE_API bool GetPackageRedirectInfo(FPackageId PackageId, FName& OutSourcePackageName, FPackageId& OutRedirectedToPackageId);

	CORE_API FPackageStoreBackendContext::FPendingEntriesAddedEvent& OnPendingEntriesAdded();

	CORE_API bool HasAnyBackendsMounted() const;

private:
	FPackageStore();

	friend class FPackageStoreReadScope;

	TSharedRef<FPackageStoreBackendContext> BackendContext;
	
	using FBackendAndPriority = TTuple<int32, TSharedRef<IPackageStoreBackend>>;
	TArray<FBackendAndPriority> Backends;

	static thread_local int32 ThreadReadCount;
};

class FPackageStoreReadScope
{
public:
	CORE_API FPackageStoreReadScope(FPackageStore& InPackageStore);
	CORE_API ~FPackageStoreReadScope();

private:
	FPackageStore& PackageStore;
};
