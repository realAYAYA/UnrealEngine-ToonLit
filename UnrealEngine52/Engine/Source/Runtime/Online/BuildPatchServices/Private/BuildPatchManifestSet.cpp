// Copyright Epic Games, Inc. All Rights Reserved.

#include "IBuildManifestSet.h"
#include "Algo/Accumulate.h"

#include "BuildPatchSettings.h"

#include "Data/ManifestData.h"
#include "Installer/OptimisedDelta.h"
#include "BuildPatchUtil.h"
#include "BuildPatchFeatureLevel.h"

namespace BuildPatchServices
{
	struct FManifestIterator
	{
	public:
		FManifestIterator() : InstallerAction(nullptr), Current(nullptr) { }
		FManifestIterator(const FBuildPatchInstallerAction& InInstallerAction) : InstallerAction(&InInstallerAction) { Current = InstallerAction->TryGetCurrentManifest() ? InstallerAction->TryGetCurrentManifest() : InstallerAction->TryGetInstallManifest(); }
		FManifestIterator operator++() { Current = (InstallerAction->TryGetInstallManifest() != Current) ? InstallerAction->TryGetInstallManifest() : nullptr; return *this; }
		bool operator!=(const FManifestIterator & Other) const { return Current != Other.Current; }
		const FBuildPatchAppManifest& operator*() const { return *Current; }
		FBuildPatchInstallerAction const * const InstallerAction;
		FBuildPatchAppManifest const * Current;
	};
	FManifestIterator begin(const FBuildPatchInstallerAction& InstallerAction) { return FManifestIterator(InstallerAction); }
	FManifestIterator end(const FBuildPatchInstallerAction& InstallerAction) { return FManifestIterator(); }

	class FBuildPatchManifestSet : public IBuildManifestSet
	{
		// Lookup for data references.
		typedef TTuple<FChunkInfo const* const, FBuildPatchAppManifest const * const> FDataReference;
		TMap<FGuid, FDataReference> DataLookup;
		// Lookup for file references.
		typedef TTuple<FFileManifest const* const, FBuildPatchAppManifest const * const> FFileReference;
		TMap<FString, FFileReference> CurrentFileLookup;
		TMap<FString, FFileReference> NewFileLookup;
	public:
		FBuildPatchManifestSet(TArray<FBuildPatchInstallerAction>&& InInstallerActions)
			: InstallerActions(MoveTemp(InInstallerActions))
		{
			// We need to perform our own lookups to avoid looping in some scenarios.
			for (const FBuildPatchInstallerAction& InstallerAction : InstallerActions)
			{
				for (const FBuildPatchAppManifest& Manifest : InstallerAction)
				{
					for (const TPair<FGuid, const BuildPatchServices::FChunkInfo*>& ChunkInfoPair : Manifest.ChunkInfoLookup)
					{
						DataLookup.Add(ChunkInfoPair.Key, FDataReference{ ChunkInfoPair.Value, &Manifest });
					}
				}
				if (!InstallerAction.IsInstall())
				{
					for (const TPair<FString, const BuildPatchServices::FFileManifest*>& FileManifestPair : InstallerAction.GetCurrentManifest().FileManifestLookup)
					{
						CurrentFileLookup.Add(FileManifestPair.Key, FFileReference{ FileManifestPair.Value, InstallerAction.TryGetCurrentManifest() });
					}
				}
			}
			// Reverse iterate provided actions, making sure there is only one unique reference to each file.
			// Though an undesirable request, the expected behaviour for multiple actions producing the same file would be clobbering by later actions.
			TSet<FString> VisitedFiles;
			for (int32 Idx = InstallerActions.Num() - 1; Idx >= 0; --Idx)
			{
				FBuildPatchInstallerAction& InstallerAction = InstallerActions[Idx];
				if (!InstallerAction.IsUninstall())
				{
					InstallerAction.SetTaggedFiles(InstallerAction.GetTaggedFiles().Difference(VisitedFiles));
					VisitedFiles.Append(InstallerAction.GetTaggedFiles());
				}
				// Ensure that all tagged files, have their new file manifest in the lookup, and current equiv in the lookup in case of multiple file references.
				for (const FString& TaggedFile : InstallerAction.GetTaggedFiles())
				{
					NewFileLookup.Add(TaggedFile, FFileReference{ InstallerAction.GetInstallManifest().FileManifestLookup[TaggedFile], InstallerAction.TryGetInstallManifest() });
					if (!InstallerAction.IsInstall() && InstallerAction.GetCurrentManifest().FileManifestLookup.Contains(TaggedFile))
					{
						CurrentFileLookup.Add(TaggedFile, FFileReference{ InstallerAction.GetCurrentManifest().FileManifestLookup[TaggedFile], InstallerAction.TryGetCurrentManifest() });
					}
				}
			}
		}

		virtual void GetInstallResumeIds(TSet<FString>& ResumeIds, bool bIncludeLegacy) const
		{
			for (const FBuildPatchInstallerAction& InstallerAction : InstallerActions)
			{
				if (!InstallerAction.IsUninstall())
				{
					if (bIncludeLegacy)
					{
						ResumeIds.Add(InstallerAction.GetInstallManifest().GetAppName() + InstallerAction.GetInstallManifest().GetVersionString());
					}
					ResumeIds.Add(InstallerAction.GetInstallManifest().GetBuildId());
				}
			}
		}

		virtual void GetInstallResumeIdsForFile(const FString& BuildFile, TSet<FString>& ResumeIds, bool bIncludeLegacy) const
		{
			for (const FBuildPatchInstallerAction& InstallerAction : InstallerActions)
			{
				if (InstallerAction.GetTaggedFiles().Contains(BuildFile))
				{
					if (bIncludeLegacy)
					{
						ResumeIds.Add(InstallerAction.GetInstallManifest().GetAppName() + InstallerAction.GetInstallManifest().GetVersionString());
					}
					ResumeIds.Add(InstallerAction.GetInstallManifest().GetBuildId());
					return;
				}
			}
		}

		virtual void GetReferencedChunks(TSet<FGuid>& DataGuids) const override
		{
			for (const FBuildPatchInstallerAction& InstallerAction : InstallerActions)
			{
				if (!InstallerAction.IsUninstall())
				{
					InstallerAction.GetInstallManifest().GetChunksRequiredForFiles(InstallerAction.GetTaggedFiles(), DataGuids);
				}
			}
		}

		virtual uint64 GetDownloadSize(const FGuid& DataGuid) const override
		{
			FDataReference const * const LookupResult = DataLookup.Find(DataGuid);
			if (LookupResult)
			{
				return LookupResult->Get<0>()->FileSize;
			}
			return 0;
		}

		virtual uint64 GetDownloadSize(const TSet<FGuid>& DataGuids) const override
		{
			return Algo::Accumulate<uint64>(DataGuids, 0, [this](uint64 Size, const FGuid& DataGuid) { return Size + GetDownloadSize(DataGuid); });
		}

		virtual bool GetChunkShaHash(const FGuid& DataGuid, FSHAHash& OutHash) const override
		{
			static const uint8 Zero[FSHA1::DigestSize] = { 0 };
			FDataReference const * const LookupResult = DataLookup.Find(DataGuid);
			if (LookupResult)
			{
				OutHash = LookupResult->Get<0>()->ShaHash;
				return FMemory::Memcmp(OutHash.Hash, Zero, FSHA1::DigestSize) != 0;
			}
			return false;
		}

		virtual FString GetDataFilename(const FString& RootDirectory, const FGuid& DataGuid) const override
		{
			FDataReference const * const LookupResult = DataLookup.Find(DataGuid);
			if (LookupResult)
			{
				return FBuildPatchUtils::GetDataFilename(*LookupResult->Get<1>(), RootDirectory, DataGuid);
			}
			return FString();
		}

		virtual int32 GetNumExpectedFiles() const
		{
			return NewFileLookup.Num();
		}

		virtual void GetExpectedFiles(TSet<FString>& Filenames) const
		{
			Filenames.Reserve(Filenames.Num() + NewFileLookup.Num());
			for (const TPair<FString, FFileReference>& NewFilePair : NewFileLookup)
			{
				Filenames.Add(NewFilePair.Key);
			}
		}

		virtual void GetOutdatedFiles(const FString& InstallDirectory, TSet<FString>& OutdatedFiles) const override
		{
			for (const FBuildPatchInstallerAction& InstallerAction : InstallerActions)
			{
				if (!InstallerAction.IsUninstall())
				{
					InstallerAction.GetInstallManifest().GetOutdatedFiles(InstallerAction.TryGetCurrentManifest(), InstallDirectory, InstallerAction.GetTaggedFiles(), OutdatedFiles);
				}
			}
		}

		virtual void GetRemovableFiles(TSet<FString>& FilesToRemove) const override
		{
			// We enumerate all trackable files, adding anything not in the NewFileLookup map.
			// This way we gracefully handle tagged/untagged files, and also uninstalling actions.
			for (const FBuildPatchInstallerAction& InstallerAction : InstallerActions)
			{
				for (const FBuildPatchAppManifest& Manifest : InstallerAction)
				{
					for (const FString& TrackedFile : Manifest.GetBuildFileList())
					{
						if (!NewFileLookup.Contains(TrackedFile))
						{
							FilesToRemove.Add(TrackedFile);
						}
					}
				}
			}
		}

		virtual const BuildPatchServices::FFileManifest* GetCurrentFileManifest(const FString& BuildFile) const override
		{
			FFileReference const * const LookupResult = CurrentFileLookup.Find(BuildFile);
			if (LookupResult)
			{
				return LookupResult->Get<0>();
			}
			return nullptr;
		}

		virtual const BuildPatchServices::FFileManifest* GetNewFileManifest(const FString& BuildFile) const
		{
			FFileReference const * const LookupResult = NewFileLookup.Find(BuildFile);
			if (LookupResult)
			{
				return LookupResult->Get<0>();
			}
			return nullptr;
		}

		template<typename ContainerType>
		uint64 GetTotalNewFileSizeHelper(const ContainerType& Filenames) const
		{
			uint64 TotalFileSize = 0;
			for (const FString& Filename : Filenames)
			{
				const BuildPatchServices::FFileManifest* FileManifest = GetNewFileManifest(Filename);
				if (nullptr != FileManifest)
				{
					TotalFileSize += FileManifest->FileSize;
				}
			}
			return TotalFileSize;
		}
		virtual uint64 GetTotalNewFileSize(const TArray<FString>& Filenames) const { return GetTotalNewFileSizeHelper(Filenames); }
		virtual uint64 GetTotalNewFileSize(const TSet<FString>& Filenames) const { return GetTotalNewFileSizeHelper(Filenames); }

		virtual void GetPreReqInfo(TArray<FPreReqInfo>& PreReqInfos) const override
		{
			for (const FBuildPatchInstallerAction& InstallerAction : InstallerActions)
			{
				if (!InstallerAction.IsUninstall())
				{
					const FBuildPatchAppManifest& Manifest = InstallerAction.GetInstallManifest();
					if (!Manifest.GetPrereqPath().IsEmpty())
					{
						FPreReqInfo& PreInfo = PreReqInfos.AddDefaulted_GetRef();
						PreInfo.IdSet = Manifest.GetPrereqIds();
						PreInfo.AppName = Manifest.GetAppName();
						PreInfo.Args = Manifest.GetPrereqArgs();
						PreInfo.Name = Manifest.GetPrereqName();
						PreInfo.Path = Manifest.GetPrereqPath();
						PreInfo.VersionString = Manifest.GetVersionString();
						PreInfo.bIsRepair = InstallerAction.IsRepair();
					}
				}
			}
		}

		virtual void GetFilesTaggedForRepair(TSet<FString>& Filenames) const override
		{
			for (const FBuildPatchInstallerAction& InstallerAction : InstallerActions)
			{
				if (InstallerAction.IsRepair())
				{
					Filenames.Append(InstallerAction.GetTaggedFiles());
				}
			}
		}

		virtual bool IsFileRepairAction(const FString& Filename) const override
		{
			for (const FBuildPatchInstallerAction& InstallerAction : InstallerActions)
			{
				if (InstallerAction.IsRepair() && InstallerAction.GetTaggedFiles().Contains(Filename))
				{
					return true;
				}
			}
			return false;
		}

		virtual bool ContainsUpdate() const override
		{
			for (const FBuildPatchInstallerAction& InstallerAction : InstallerActions)
			{
				if (InstallerAction.IsUpdate() && InstallerAction.GetCurrentManifest().GetBuildId() != InstallerAction.GetInstallManifest().GetBuildId())
				{
					return true;
				}
			}
			return false;
		}

		virtual bool IsRepairOnly() const override
		{
			bool bHasRepair = false;
			for (const FBuildPatchInstallerAction& InstallerAction : InstallerActions)
			{
				if (InstallerAction.IsRepair())
				{
					bHasRepair = true;
				}
				else
				{
					return false;
				}
			}
			return bHasRepair;
		}

		virtual bool HasFileAttributes() const override
		{
			for (const FBuildPatchInstallerAction& InstallerAction : InstallerActions)
			{
				if (!InstallerAction.IsUninstall())
				{
					if (InstallerAction.GetInstallManifest().HasFileAttributes())
					{
						return true;
					}
				}
			}
			return false;
		}

	private:
		TArray<FBuildPatchInstallerAction> InstallerActions;
	};

	IBuildManifestSet* FBuildManifestSetFactory::Create(TArray<FBuildPatchInstallerAction> InstallerActions)
	{
		return new FBuildPatchManifestSet(MoveTemp(InstallerActions));
	}
}