// Copyright Epic Games, Inc. All Rights Reserved.

#include "PackageVirtualizationProcess.h"

#include "Containers/UnrealString.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformTime.h"
#include "Internationalization/Internationalization.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "PackageUtils.h"
#include "Serialization/EditorBulkData.h"
#include "UObject/Linker.h"
#include "UObject/Package.h"
#include "UObject/PackageResourceManager.h"
#include "UObject/PackageTrailer.h"
#include "UObject/UObjectGlobals.h"
#include "Virtualization/VirtualizationSystem.h"
#include "VirtualizationManager.h"

#define LOCTEXT_NAMESPACE "Virtualization"

namespace UE::Virtualization
{

/** 
 * Implementation of the IPayloadProvider interface so that payloads can be requested on demand
 * when they are being virtualized.
 * 
 * This implementation is not optimized. If a package holds many payloads that are all virtualized
 * we will end up loading the same trailer over and over, as well as opening the same package file
 * for read many times.
 * 
 * So far this has shown to be a rounding error compared to the actual cost of virtualization 
 * and so implementing any level of caching has been left as a future task.
 * 
 * TODO: Implement a MRU cache for payloads to prevent loading the same payload off disk many
 * times for different backends if it will not cause a huge memory spike.
 */
class FWorkspaceDomainPayloadProvider final : public IPayloadProvider
{
public: 
	FWorkspaceDomainPayloadProvider() = default;
	virtual ~FWorkspaceDomainPayloadProvider() = default;

	/** Register the payload with it's trailer and package name so that we can access it later as needed */
	void RegisterPayload(const FIoHash& PayloadId, uint64 SizeOnDisk, const FString& PackageName)
	{
		if (!PayloadId.IsZero())
		{
			PayloadLookupTable.Emplace(PayloadId, FPayloadData(SizeOnDisk, PackageName));
		}
	}

private:
	virtual FCompressedBuffer RequestPayload(const FIoHash& Identifier) override
	{
		if (Identifier.IsZero())
		{
			return FCompressedBuffer();
		}

		const FPayloadData* Data = PayloadLookupTable.Find(Identifier);
		if (Data == nullptr)
		{
			UE_LOG(LogVirtualization, Error, TEXT("FWorkspaceDomainPayloadProvider was unable to find a payload with the identifier '%s'"), 
				*LexToString(Identifier));

			return FCompressedBuffer();
		}
		
		TUniquePtr<FArchive> PackageAr = IPackageResourceManager::Get().OpenReadExternalResource(EPackageExternalResource::WorkspaceDomainFile, *Data->PackageName);

		if (!PackageAr.IsValid())
		{
			UE_LOG(LogVirtualization, Error, TEXT("FWorkspaceDomainPayloadProvider was unable to open the package '%s' for reading"), 
				*Data->PackageName);

			return FCompressedBuffer();
		}
			
		PackageAr->Seek(PackageAr->TotalSize());

		FPackageTrailer Trailer;
		if (!Trailer.TryLoadBackwards(*PackageAr))
		{
			UE_LOG(LogVirtualization, Error, TEXT("FWorkspaceDomainPayloadProvider failed to load the package trailer from the package '%s'"), 
				*Data->PackageName);

			return FCompressedBuffer();
		}

		FCompressedBuffer Payload = Trailer.LoadLocalPayload(Identifier, *PackageAr);
		
		if (!Payload)
		{
			UE_LOG(LogVirtualization, Error, TEXT("FWorkspaceDomainPayloadProvider was uanble to load the payload '%s' from the package '%s'"),
				*LexToString(Identifier),
				*Data->PackageName);

			return FCompressedBuffer();
		}

		if (Identifier != FIoHash(Payload.GetRawHash()))
		{
			UE_LOG(LogVirtualization, Error, TEXT("FWorkspaceDomainPayloadProvider loaded an incorrect payload from the package '%s'. Expected '%s' Loaded  '%s'"), 
				*Data->PackageName,
				*LexToString(Identifier),
				*LexToString(Payload.GetRawHash()));

			return FCompressedBuffer();
		}

		return Payload;
	}

	virtual uint64 GetPayloadSize(const FIoHash& Identifier) override
	{
		if (Identifier.IsZero())
		{
			return 0;
		}

		const FPayloadData* Data = PayloadLookupTable.Find(Identifier);
		if (Data != nullptr)
		{
			return Data->SizeOnDisk;
		}
		else
		{
			UE_LOG(LogVirtualization, Error, TEXT("FWorkspaceDomainPayloadProvider was unable to find a payload with the identifier '%s'"),
				*LexToString(Identifier));

			return 0;
		}
	}

	/* This structure holds additional info about the payload that we might need later */
	struct FPayloadData
	{
		FPayloadData(uint64 InSizeOnDisk, const FString& InPackageName)
			: SizeOnDisk(InSizeOnDisk)
			, PackageName(InPackageName)
		{

		}

		uint64 SizeOnDisk;
		FString PackageName;
	};

	TMap<FIoHash, FPayloadData> PayloadLookupTable;
};

void VirtualizePackages(TConstArrayView<FString> PackagePaths, TArray<FText>& OutErrors)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UE::Virtualization::VirtualizePackages);

	IVirtualizationSystem& System = IVirtualizationSystem::Get();

	const double StartTime = FPlatformTime::Seconds();

	FScopedSlowTask Progress(5.0f, LOCTEXT("Virtualization_Task", "Virtualizing Assets..."));
	Progress.MakeDialog();

	// Other systems may have added errors to this array, we need to check so later we can determine if this function added any additional errors.
	const int32 NumErrors = OutErrors.Num();

	struct FPackageInfo
	{
		FPackagePath Path;
		FPackageTrailer Trailer;

		TArray<FIoHash> LocalPayloads;

		/** Index where the FPushRequest for this package can be found */
		int32 PayloadIndex = INDEX_NONE;

		bool bWasTrailerUpdated = false;
	};

	UE_LOG(LogVirtualization, Display, TEXT("Considering %d file(s) for virtualization"), PackagePaths.Num());

	TArray<FPackageInfo> Packages;
	Packages.Reserve(PackagePaths.Num());

	Progress.EnterProgressFrame(1.0f);

	// From the list of files to submit we need to find all of the valid packages that contain
	// local payloads that need to be virtualized.
	int64 TotalPackagesFound = 0;
	int64 TotalPackageTrailersFound = 0;
	int64 TotalPayloadsToVirtualize = 0;
	for (const FString& AbsoluteFilePath : PackagePaths)
	{
		FPackagePath PackagePath = FPackagePath::FromLocalPath(AbsoluteFilePath);

		// TODO: How to handle text packages?
		if (FPackageName::IsPackageExtension(PackagePath.GetHeaderExtension()) || FPackageName::IsTextPackageExtension(PackagePath.GetHeaderExtension()))
		{
			TotalPackagesFound++;

			FPackageTrailer Trailer;
			if (FPackageTrailer::TryLoadFromPackage(PackagePath, Trailer))
			{
				TotalPackageTrailersFound++;

				// The following is not expected to ever happen, currently we give a user facing error but it generally means that the asset is broken somehow.
				ensureMsgf(Trailer.GetNumPayloads(EPayloadStorageType::Referenced) == 0, TEXT("Trying to virtualize a package that already contains payload references which the workspace file should not ever contain!"));
				if (Trailer.GetNumPayloads(EPayloadStorageType::Referenced) > 0)
				{
					FText Message = FText::Format(LOCTEXT("Virtualization_PkgHasReferences", "Cannot virtualize the package '{1}' as it has referenced payloads in the trailer"),
						FText::FromString(PackagePath.GetDebugName()));
					OutErrors.Add(Message);
					return;
				}

				FPackageInfo PkgInfo;

				PkgInfo.Path = MoveTemp(PackagePath);
				PkgInfo.Trailer = MoveTemp(Trailer);
				PkgInfo.LocalPayloads = PkgInfo.Trailer.GetPayloads(EPayloadFilter::CanVirtualize);

				if (!PkgInfo.LocalPayloads.IsEmpty())
				{
					TotalPayloadsToVirtualize += PkgInfo.LocalPayloads.Num();

					Packages.Emplace(MoveTemp(PkgInfo));
				}
			}
		}
	}

	UE_LOG(LogVirtualization, Display, TEXT("Found %" INT64_FMT " package(s), %" INT64_FMT " of which had payload trailers"), TotalPackagesFound, TotalPackageTrailersFound);

	// TODO: Currently not all of the filtering is done as package save time, so some of the local payloads may not get virtualized.
	// When/if we move all filtering to package save we can change this log message to state that the local payloads *will* be virtualized.
	UE_LOG(LogVirtualization, Display, TEXT("Found %" INT64_FMT " locally stored payload(s) in %d package(s) that maybe need to be virtualized"), TotalPayloadsToVirtualize, Packages.Num());

	Progress.EnterProgressFrame(1.0f);

	// TODO Optimization: We might want to check for duplicate payloads and remove them at this point

	// Build up the info in the payload provider and the final array of payload push requests
	FWorkspaceDomainPayloadProvider PayloadProvider;
	TArray<Virtualization::FPushRequest> PayloadsToSubmit;
	PayloadsToSubmit.Reserve(TotalPayloadsToVirtualize);

	for (FPackageInfo& PackageInfo : Packages)
	{
		check(!PackageInfo.LocalPayloads.IsEmpty());

		PackageInfo.PayloadIndex = PayloadsToSubmit.Num();

		for (const FIoHash& PayloadId : PackageInfo.LocalPayloads)
		{
			const uint64 SizeOnDisk = PackageInfo.Trailer.FindPayloadSizeOnDisk(PayloadId);

			PayloadProvider.RegisterPayload(PayloadId, SizeOnDisk, PackageInfo.Path.GetPackageName());
			PayloadsToSubmit.Emplace(PayloadId, PayloadProvider, PackageInfo.Path.GetPackageName());
		}
	}

	// TODO: We should be able to do both Cache and Persistent pushes in the same call

	// Push payloads to cache storage
	{
		Progress.EnterProgressFrame(1.0f);
		
		if (!System.PushData(PayloadsToSubmit, EStorageType::Cache))
		{
			// Caching is not critical to the process so we only warn if it fails
			UE_LOG(LogVirtualization, Warning, TEXT("Failed to push to EStorageType::Cache storage"));
		}

		int64 TotalPayloadsCached = 0;
		for (Virtualization::FPushRequest& Request : PayloadsToSubmit)
		{
			TotalPayloadsCached += Request.GetResult().WasPushed() ? 1 : 0;

			// TODO: This really shouldn't be required, fix when we allow both pushes to be done in the same call
			// Reset the status for the persistent storage push
			Request.ResetResult();
		}
		UE_LOG(LogVirtualization, Display, TEXT("Pushed %" INT64_FMT " payload(s) to cached storage"), TotalPayloadsCached);
	}
	
	// Push payloads to persistent storage
	{
		Progress.EnterProgressFrame(1.0f);

		if (!System.PushData(PayloadsToSubmit, EStorageType::Persistent))
		{
			FText Message = LOCTEXT("Virtualization_PushFailure", "Failed to push payloads");
			OutErrors.Add(Message);
			return;
		}

		int64 TotalPayloadsVirtualized = 0;
		for (const Virtualization::FPushRequest& Request : PayloadsToSubmit)
		{
			TotalPayloadsVirtualized += Request.GetResult().WasPushed() ? 1 : 0;
		}
		UE_LOG(LogVirtualization, Display, TEXT("Pushed %" INT64_FMT " payload(s) to EStorageType::Persistent storage"), TotalPayloadsVirtualized);
	}

	// Update the package info for the submitted payloads
	for (FPackageInfo& PackageInfo : Packages)
	{
		for (int32 Index = 0; Index < PackageInfo.LocalPayloads.Num(); ++Index)
		{
			const Virtualization::FPushRequest& Request = PayloadsToSubmit[PackageInfo.PayloadIndex + Index];
			check(Request.GetIdentifier() == PackageInfo.LocalPayloads[Index]);

			if (Request.GetResult().IsVirtualized())
			{
				if (PackageInfo.Trailer.UpdatePayloadAsVirtualized(Request.GetIdentifier()))
				{
					PackageInfo.bWasTrailerUpdated = true;
				}
				else
				{
					FText Message = FText::Format(	LOCTEXT("Virtualization_UpdateStatusFailed", "Unable to update the status for the payload '{0}' in the package '{1}'"),
													FText::FromString(LexToString(Request.GetIdentifier())),
													FText::FromString(PackageInfo.Path.GetDebugName()));
					OutErrors.Add(Message);
					return;
				}
			}
		}
	}

	Progress.EnterProgressFrame(1.0f);

	TArray<TPair<FPackagePath, FString>> PackagesToReplace;

	// Any package with an updated trailer needs to be copied and an updated trailer appended
	for (FPackageInfo& PackageInfo : Packages)
	{
		if (!PackageInfo.bWasTrailerUpdated)
		{
			continue;
		}

		const FPackagePath& PackagePath = PackageInfo.Path; // No need to validate path, we checked this earlier

		FString NewPackagePath = DuplicatePackageWithUpdatedTrailer(PackagePath.GetLocalFullPath(), PackageInfo.Trailer, OutErrors);

		if (!NewPackagePath.IsEmpty())
		{
			// Now that we have successfully created a new version of the package with an updated trailer 
			// we need to mark that it should replace the original package.
			PackagesToReplace.Emplace(PackagePath, MoveTemp(NewPackagePath));
		}
		else
		{
			return;
		}

	}

	UE_LOG(LogVirtualization, Display, TEXT("%d package(s) had their trailer container modified and need to be updated"), PackagesToReplace.Num());

	if (NumErrors == OutErrors.Num())
	{
		// TODO: Consider using the SavePackage model (move the original, then replace, so we can restore all of the original packages if needed)
		// having said that, once a package is in PackagesToReplace it should still be safe to submit so maybe we don't need this level of protection?

		// We need to reset the loader of any package that we want to re-save over
		for (int32 Index = 0; Index < PackagesToReplace.Num(); ++Index)
		{
			const TPair<FPackagePath, FString>& Pair = PackagesToReplace[Index];

			UPackage* Package = FindObjectFast<UPackage>(nullptr, Pair.Key.GetPackageFName());
			if (Package != nullptr)
			{
				UE_LOG(LogVirtualization, Verbose, TEXT("Detaching '%s' from disk so that it can be virtualized"), *Pair.Key.GetDebugName());
				ResetLoadersForSave(Package, *Pair.Key.GetLocalFullPath());
			}

			if (!CanWriteToFile(Pair.Key.GetLocalFullPath()))
			{
				// Technically the package could have local payloads that won't be virtualized due to filtering or min payload sizes and so the
				// following warning is misleading. This will be solved if we move that evaluation to the point of saving a package.
				// If not then we probably need to extend QueryPayloadStatuses to test filtering etc as well, then check for potential package
				// modification after that.
				// Long term, the stand alone tool should be able to request the UnrealEditor relinquish the lock on the package file so this becomes 
				// less of a problem.
				FText Message = FText::Format(LOCTEXT("Virtualization_PkgLocked", "The package file '{0}' has local payloads but is locked for modification and cannot be virtualized, this package will be skipped!"),
					FText::FromString(Pair.Key.GetDebugName()));
				UE_LOG(LogVirtualization, Warning, TEXT("%s"), *Message.ToString());
				
				PackagesToReplace.RemoveAt(Index--);
			}
		}

		// Since we had no errors we can now replace all of the packages that were virtualized data with the virtualized replacement file.
		for(const TPair<FPackagePath,FString>&  Iterator : PackagesToReplace)
		{
			const FString OriginalPackagePath = Iterator.Key.GetLocalFullPath();
			const FString& NewPackagePath = Iterator.Value;

			if (!IFileManager::Get().Move(*OriginalPackagePath, *NewPackagePath))
			{
				FText Message = FText::Format(	LOCTEXT("Virtualization_MoveFailed", "Unable to replace the package '{0}' with the virtualized version"),
												FText::FromString(Iterator.Key.GetDebugName()));
				OutErrors.Add(Message);
				continue;
			}
		}
	}

	const double TimeInSeconds = FPlatformTime::Seconds() - StartTime;
	UE_LOG(LogVirtualization, Verbose, TEXT("Virtualization pre submit check took %.3f(s)"), TimeInSeconds);
}

} // namespace UE::Virtualization

#undef LOCTEXT_NAMESPACE

