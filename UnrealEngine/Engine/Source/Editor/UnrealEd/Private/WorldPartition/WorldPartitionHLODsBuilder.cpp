// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/WorldPartitionHLODsBuilder.h"

#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "Logging/LogMacros.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Algo/ForEach.h"
#include "UObject/SavePackage.h"

#include "ActorFolder.h"
#include "Engine/Engine.h"
#include "Engine/LevelStreamingGCHelper.h"
#include "EngineUtils.h"
#include "SourceControlHelpers.h"
#include "ISourceControlModule.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "DerivedDataCacheInterface.h"

#include "WorldPartition/DataLayer/DataLayerInstance.h"
#include "WorldPartition/ActorDescContainerInstance.h"
#include "WorldPartition/WorldPartitionActorDescInstance.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionHelpers.h"
#include "WorldPartition/HLOD/HLODActor.h"
#include "WorldPartition/HLOD/HLODActorDesc.h"
#include "WorldPartition/HLOD/HLODLayer.h"
#include "WorldPartition/HLOD/HLODProviderInterface.h"
#include "WorldPartition/HLOD/HLODRuntimeSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogWorldPartitionHLODsBuilder, All, All);

class FSourceControlHelper : public ISourceControlHelper
{
public:
	FSourceControlHelper(FPackageSourceControlHelper& InPackageHelper, FHLODModifiedFiles& InModifiedFiles)
		: PackageHelper(InPackageHelper)
		, ModifiedFiles(InModifiedFiles)
	{}

	virtual ~FSourceControlHelper()
	{}

	virtual FString GetFilename(const FString& PackageName) const override
	{
		return SourceControlHelpers::PackageFilename(PackageName);
	}

	virtual FString GetFilename(UPackage* Package) const override
	{
		return SourceControlHelpers::PackageFilename(Package);
	}

	virtual bool Checkout(UPackage* Package) const override
	{
		bool bCheckedOut = PackageHelper.Checkout(Package);
		if (bCheckedOut)
		{
			const FString Filename = GetFilename(Package);
			const bool bAdded = ModifiedFiles.Get(FHLODModifiedFiles::EFileOperation::FileAdded).Contains(Filename);
			if (!bAdded)
			{
				ModifiedFiles.Add(FHLODModifiedFiles::EFileOperation::FileEdited, Filename);
			}
		}
		return bCheckedOut;
	}

	virtual bool Add(UPackage* Package) const override
	{
		bool bAdded = PackageHelper.AddToSourceControl(Package);
		if (bAdded)
		{
			ModifiedFiles.Add(FHLODModifiedFiles::EFileOperation::FileAdded, GetFilename(Package));
		}
		return bAdded;
	}

	virtual bool Delete(const FString& PackageName) const override
	{
		bool bDeleted = PackageHelper.Delete(PackageName);
		if (bDeleted)
		{
			ModifiedFiles.Add(FHLODModifiedFiles::EFileOperation::FileDeleted, PackageName);
		}
		return bDeleted;
	}

	virtual bool Delete(UPackage* Package) const override
	{
		FString PackageName = GetFilename(Package);
		bool bDeleted = PackageHelper.Delete(Package);
		if (bDeleted)
		{
			ModifiedFiles.Add(FHLODModifiedFiles::EFileOperation::FileDeleted, PackageName);
		}
		return bDeleted;
	}

	virtual bool Save(UPackage* Package) const override
	{
		bool bFileExists = IPlatformFile::GetPlatformPhysical().FileExists(*GetFilename(Package));

		// Checkout package
		Package->MarkAsFullyLoaded();

		if (bFileExists)
		{
			if (!Checkout(Package))
			{
				UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Error checking out package %s."), *Package->GetName());
				return false;
			}
		}

		// Save package
		FString PackageFileName = GetFilename(Package);
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Standalone;
		SaveArgs.SaveFlags = PackageHelper.UseSourceControl() ? ESaveFlags::SAVE_None : ESaveFlags::SAVE_Async;
		if (!UPackage::SavePackage(Package, nullptr, *PackageFileName, SaveArgs))
		{
			UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Error saving package %s."), *Package->GetName());
			return false;
		}

		// Add new package to source control
		if (!bFileExists)
		{
			if (!Add(Package))
			{
				UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Error adding package %s to revision control."), *Package->GetName());
				return false;
			}
		}

		return true;
	}

private:
	FPackageSourceControlHelper& PackageHelper;
	FHLODModifiedFiles& ModifiedFiles;
};

static const FString DistributedBuildWorkingDirName = TEXT("HLODTemp");
static const FString DistributedBuildManifestName = TEXT("HLODBuildManifest.ini");
static const FString BuildProductsFileName = TEXT("BuildProducts.txt");

FString GetHLODBuilderFolderName(uint32 BuilderIndex) { return FString::Printf(TEXT("HLODBuilder%d"), BuilderIndex); }
FString GetToSubmitFolderName() { return TEXT("ToSubmit"); }

UWorldPartitionHLODsBuilder::UWorldPartitionHLODsBuilder(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, BuilderIdx(INDEX_NONE)
	, BuilderCount(INDEX_NONE)
{
	BuildOptions  = HasParam("SetupHLODs") ? EHLODBuildStep::HLOD_Setup : EHLODBuildStep::None;
	BuildOptions |= HasParam("BuildHLODs") ? EHLODBuildStep::HLOD_Build : EHLODBuildStep::None;
	BuildOptions |= HasParam("RebuildHLODs") ? EHLODBuildStep::HLOD_Build : EHLODBuildStep::None;
	BuildOptions |= HasParam("DeleteHLODs") ? EHLODBuildStep::HLOD_Delete : EHLODBuildStep::None;
	BuildOptions |= HasParam("FinalizeHLODs") ? EHLODBuildStep::HLOD_Finalize : EHLODBuildStep::None;
	BuildOptions |= HasParam("DumpStats") ? EHLODBuildStep::HLOD_Stats : EHLODBuildStep::None;

	bResumeBuild = GetParamValue("ResumeBuild=", ResumeBuildIndex);

	bDistributedBuild = HasParam("DistributedBuild");
	bForceBuild = HasParam("RebuildHLODs");
	bReportOnly = HasParam("ReportOnly");

	GetParamValue("BuildManifest=", BuildManifest);
	GetParamValue("BuilderIdx=", BuilderIdx);
	GetParamValue("BuilderCount=", BuilderCount);
	GetParamValue("BuildHLODLayer=", HLODLayerToBuild);
	GetParamValue("BuildSingleHLOD=", HLODActorToBuild);

	if (!HLODActorToBuild.IsNone() || !HLODLayerToBuild.IsNone())
	{
		BuildOptions |= EHLODBuildStep::HLOD_Build;
		bForceBuild = bForceBuild || !HLODActorToBuild.IsNone();
	}
	
	// Default behavior without any option is to setup + build
	if (BuildOptions == EHLODBuildStep::None)
	{
		BuildOptions = EHLODBuildStep::HLOD_Setup | EHLODBuildStep::HLOD_Build;
	}
}

bool UWorldPartitionHLODsBuilder::RequiresCommandletRendering() const
{
	// Commandlet requires rendering only for building HLODs
	// Building will occur either if -BuildHLODs is provided or no explicit step arguments are provided
	return EnumHasAnyFlags(BuildOptions, EHLODBuildStep::HLOD_Build);
}

bool UWorldPartitionHLODsBuilder::ShouldRunStep(const EHLODBuildStep BuildStep) const
{
	return (BuildOptions & BuildStep) == BuildStep;
}

bool UWorldPartitionHLODsBuilder::ValidateParams() const
{
	if (ShouldRunStep(EHLODBuildStep::HLOD_Setup) && IsUsingBuildManifest())
	{
		if (BuilderCount <= 0)
		{
			UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Missing parameter -BuilderCount=N (where N > 0), exiting..."));
			return false;
		}
	}

	if (ShouldRunStep(EHLODBuildStep::HLOD_Build) && IsUsingBuildManifest())
	{
		if (BuilderIdx < 0)
		{
			UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Missing parameter -BuilderIdx=i, exiting..."));
			return false;
		}

		if (!FPaths::FileExists(BuildManifest))
		{
			UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Build manifest file \"%s\" not found, exiting..."), *BuildManifest);
			return false;
		}

		FString CurrentEngineVersion = FEngineVersion::Current().ToString();
		FString ManifestEngineVersion = TEXT("unknown");

		FConfigFile ConfigFile;
		ConfigFile.Read(BuildManifest);
		ConfigFile.GetString(TEXT("General"), TEXT("EngineVersion"), ManifestEngineVersion);
		if (ManifestEngineVersion != CurrentEngineVersion)
		{
			UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Build manifest engine version doesn't match current engine version (%s vs %s), exiting..."), *ManifestEngineVersion, *CurrentEngineVersion);
			return false;
		}
	}

	return true;
}

FString GetDistributedBuildWorkingDir(UWorld* InWorld)
{
	uint32 WorldPackageHash = GetTypeHash(InWorld->GetPackage()->GetFullName());
	return FString::Printf(TEXT("%s/%s/%08x"), *FPaths::RootDir(), *DistributedBuildWorkingDirName, WorldPackageHash);
}

bool UWorldPartitionHLODsBuilder::ShouldProcessWorld(UWorld* InWorld) const
{
	bool bShouldProcessWorld = true;

	// When building HLODs in a distributed build, if there is no config section for the given builder index
	// it means that the builder can skip processing this world altogether.
	if (bDistributedBuild && ShouldRunStep(EHLODBuildStep::HLOD_Build))
	{
		const FString BuildManifestDirName = GetDistributedBuildWorkingDir(InWorld);
		const FString BuildManifestFileName = BuildManifestDirName / DistributedBuildManifestName;

		FConfigFile ConfigFile;
		ConfigFile.Read(BuildManifestFileName);

		FString SectionName = GetHLODBuilderFolderName(BuilderIdx);

		const FConfigSection* ConfigSection = ConfigFile.FindSection(SectionName);
		if (!ConfigSection || ConfigSection->IsEmpty())
		{
			bShouldProcessWorld = false;
		}
	}

	return bShouldProcessWorld;
}

bool UWorldPartitionHLODsBuilder::PreWorldInitialization(UWorld* InWorld, FPackageSourceControlHelper& PackageHelper)
{
	ModifiedFiles.Empty();

	if (bDistributedBuild)
	{
		DistributedBuildWorkingDir = GetDistributedBuildWorkingDir(InWorld);
		DistributedBuildManifest = DistributedBuildWorkingDir / DistributedBuildManifestName;

		if (!BuildManifest.IsEmpty())
		{
			UE_LOG(LogWorldPartitionHLODsBuilder, Warning, TEXT("Ignoring parameter -BuildManifest when a distributed build is performed"));
		}

		BuildManifest = DistributedBuildManifest;
	}

	if (!ValidateParams())
	{
		return false;
	}

	bool bRet = true;

	// When running a distributed build, retrieve relevant build products from the previous steps
	if (IsDistributedBuild() && (ShouldRunStep(EHLODBuildStep::HLOD_Build) || ShouldRunStep(EHLODBuildStep::HLOD_Finalize)))
	{
		FString WorkingDirFolder = ShouldRunStep(EHLODBuildStep::HLOD_Build) ? GetHLODBuilderFolderName(BuilderIdx) : GetToSubmitFolderName();
		bRet = CopyFilesFromWorkingDir(WorkingDirFolder);
	}

	return bRet;
}

bool UWorldPartitionHLODsBuilder::RunInternal(UWorld* InWorld, const FCellInfo& InCellInfo, FPackageSourceControlHelper& PackageHelper)
{
	World = InWorld;
	WorldPartition = World->GetWorldPartition();
	
	// Allows HLOD Streaming levels to be GCed properly
	FLevelStreamingGCHelper::EnableForCommandlet();

	SourceControlHelper = new FSourceControlHelper(PackageHelper, ModifiedFiles);

	bool bRet = true;

	if (bRet && ShouldRunStep(EHLODBuildStep::HLOD_Setup))
	{
		bRet = SetupHLODActors();
	}

	if (!bReportOnly)
	{
		if (bRet && ShouldRunStep(EHLODBuildStep::HLOD_Build))
		{
			bRet = BuildHLODActors();
		}

		if (bRet && ShouldRunStep(EHLODBuildStep::HLOD_Delete))
		{
			bRet = DeleteHLODActors();
		}

		if (bRet && ShouldRunStep(EHLODBuildStep::HLOD_Finalize))
		{
			bRet = SubmitHLODActors();
		}

		if (bRet && ShouldRunStep(EHLODBuildStep::HLOD_Stats))
		{
			bRet = DumpStats();
		}
	}

	WorldPartition = nullptr;
	delete SourceControlHelper;

	return bRet;
}

bool UWorldPartitionHLODsBuilder::SetupHLODActors()
{
	// No setup needed for non partitioned worlds
	if (WorldPartition)
	{
		auto ActorFolderAddedDelegateHandle = GEngine->OnActorFolderAdded().AddLambda([this](UActorFolder* InActorFolder)
		{
			UPackage* ActorFolderPackage = InActorFolder->GetPackage();
			const bool bIsTempPackage = FPackageName::IsTempPackage(ActorFolderPackage->GetName());
			if (!bIsTempPackage)
			{
				// We don't want the HLOD folders to be expanded by default
				InActorFolder->SetIsInitiallyExpanded(false);
				SourceControlHelper->Save(InActorFolder->GetPackage());
			}
		});
	
		ON_SCOPE_EXIT
		{
			GEngine->OnActorFolderAdded().Remove(ActorFolderAddedDelegateHandle);
		};

		UWorldPartition::FSetupHLODActorsParams SetupHLODActorsParams = UWorldPartition::FSetupHLODActorsParams()
			.SetSourceControlHelper(SourceControlHelper)
			.SetReportOnly(bReportOnly);

		WorldPartition->SetupHLODActors(SetupHLODActorsParams);

		// When performing a distributed build, ensure our work folder is empty
		if (IsDistributedBuild())
		{
			IFileManager::Get().DeleteDirectory(*DistributedBuildWorkingDir, false, true);
		}

		UE_LOG(LogWorldPartitionHLODsBuilder, Display, TEXT("#### World HLOD actors ####"));

		int32 NumActors = 0;
		for (FActorDescContainerInstanceCollection::TIterator<AWorldPartitionHLOD> HLODIterator(WorldPartition); HLODIterator; ++HLODIterator)
		{
			FWorldPartitionActorDescInstance* HLODActorDescInstance = *HLODIterator;
			FString PackageName = HLODActorDescInstance->GetActorPackage().ToString();

			UE_LOG(LogWorldPartitionHLODsBuilder, Display, TEXT("    [%d] %s"), NumActors, *PackageName);

			NumActors++;
		}

		UE_LOG(LogWorldPartitionHLODsBuilder, Display, TEXT("#### World contains %d HLOD actors ####"), NumActors);
	}

	if (IsUsingBuildManifest())
	{
		TMap<FString, int32> FilesToBuilderMap;
		bool bGenerated = GenerateBuildManifest(FilesToBuilderMap);
		if (!bGenerated)
		{
			return false;
		}

		// When performing a distributed build, move modified files to the temporary working dir, to be submitted later in the last "submit" step
		if (IsDistributedBuild())
		{
			// Ensure we don't hold on to packages of always loaded actors
			// When running distributed builds, we wanna leave the machine clean, so added files are deleted, check'd out files are reverted
			// and deleted files are restored.
			if (WorldPartition)
			{
				WorldPartition->Uninitialize();
				FWorldPartitionHelpers::DoCollectGarbage();
			}

			TArray<FHLODModifiedFiles> BuildersFiles;
			BuildersFiles.SetNum(BuilderCount);

			for (int32 i = 0; i < FHLODModifiedFiles::EFileOperation::NumFileOperations; i++)
			{
				FHLODModifiedFiles::EFileOperation FileOp = (FHLODModifiedFiles::EFileOperation)i;
				for (const FString& ModifiedFile : ModifiedFiles.Get(FileOp))
				{
					int32* Idx = FilesToBuilderMap.Find(ModifiedFile);
					if (Idx)
					{
						BuildersFiles[*Idx].Add(FileOp, ModifiedFile);
					}
					else
					{
						// Add general files to the last builder
						BuildersFiles.Last().Add(FileOp, ModifiedFile);
					}
				}
			}

			// Gather build product to ensure intermediary files are copied between the different HLOD generation steps
			TArray<FString> BuildProducts;

			// Copy files that will be handled by the different builders
			for (int32 Idx = 0; Idx < BuilderCount; Idx++)
			{
				if (!CopyFilesToWorkingDir(GetHLODBuilderFolderName(Idx), BuildersFiles[Idx], BuildProducts))
				{
					return false;
				}
			}

			// The build manifest must also be included as a build product to be available in the next steps
			BuildProducts.Add(BuildManifest);

			// Write build products to a file
			if (!AddBuildProducts(BuildProducts))
			{
				return false;
			}
		}
	}

	return true;
}

bool UWorldPartitionHLODsBuilder::BuildHLODActors()
{
	auto SaveHLODActor = [this](AWorldPartitionHLOD* HLODActor)
	{
		UPackage* ActorPackage = HLODActor->GetPackage();
		if (ActorPackage->IsDirty())
		{
			UE_LOG(LogWorldPartitionHLODsBuilder, Display, TEXT("HLOD actor %s was modified, saving..."), *HLODActor->GetActorLabel());

			bool bSaved = SourceControlHelper->Save(ActorPackage);
			if (!bSaved)
			{
				UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Failed to save %s, exiting..."), *USourceControlHelpers::PackageFilename(ActorPackage));
				return false;
			}
		}

		return true;
	};

	if (WorldPartition)
	{
		TArray<FGuid> HLODActorsToBuild;
		if (!GetHLODActorsToBuild(HLODActorsToBuild))
		{
			return false;
		}

		if (!ValidateWorkload(HLODActorsToBuild))
		{
			return false;
		}

		UE_LOG(LogWorldPartitionHLODsBuilder, Display, TEXT("#### Building %d HLOD actors ####"), HLODActorsToBuild.Num());
		if (bResumeBuild)
		{
			UE_LOG(LogWorldPartitionHLODsBuilder, Display, TEXT("#### Resuming build at %d ####"), ResumeBuildIndex);
		}

		for (int32 CurrentActor = ResumeBuildIndex; CurrentActor < HLODActorsToBuild.Num(); ++CurrentActor)
		{
			TRACE_BOOKMARK(TEXT("BuildHLOD Start - %d"), CurrentActor);

			const FGuid& HLODActorGuid = HLODActorsToBuild[CurrentActor];

			FWorldPartitionReference ActorRef(WorldPartition, HLODActorGuid);

			AWorldPartitionHLOD* HLODActor = CastChecked<AWorldPartitionHLOD>(ActorRef.GetActor());

			UE_LOG(LogWorldPartitionHLODsBuilder, Display, TEXT("[%d / %d] Building HLOD actor %s..."), CurrentActor + 1, HLODActorsToBuild.Num(), *HLODActor->GetActorLabel());

			// Simulate an engine tick to make sure engine & render resources that are queued for deletion are processed.
			FWorldPartitionHelpers::FakeEngineTick(World);

			HLODActor->BuildHLOD(bForceBuild);

			bool bSaved = SaveHLODActor(HLODActor);
			if (!bSaved)
			{
				return false;
			}

			if (FWorldPartitionHelpers::ShouldCollectGarbage())
			{
				FWorldPartitionHelpers::DoCollectGarbage();
			}

			TRACE_BOOKMARK(TEXT("BuildHLOD End - %d"), CurrentActor);
		}

		UE_LOG(LogWorldPartitionHLODsBuilder, Display, TEXT("#### Built %d HLOD actors ####"), HLODActorsToBuild.Num());
	}
	else
	{
		IWorldPartitionHLODProvider::FBuildHLODActorParams BuildHLODActorParams;
		BuildHLODActorParams.bForceRebuild = bForceBuild;
		BuildHLODActorParams.OnPackageModified.BindLambda([this](UPackage* ModifiedPackage)
		{
			bool bSaved = SourceControlHelper->Save(ModifiedPackage);
			if (!bSaved)
			{
				UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Failed to save %s, exiting..."), *USourceControlHelpers::PackageFilename(ModifiedPackage));
			}

			return bSaved;
		});

		uint32 NumHLODActors = 0;
		for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
		{
			if (IWorldPartitionHLODProvider* HLODProvider = Cast<IWorldPartitionHLODProvider>(*ActorIt))
			{
				bool bBuildResult = HLODProvider->BuildHLODActor(BuildHLODActorParams);
				if (!bBuildResult)
				{
					return false;
				}

				NumHLODActors++;
			}
		}
		UE_LOG(LogWorldPartitionHLODsBuilder, Display, TEXT("#### Built %d HLOD actor ####"), NumHLODActors);
	}


	// Move modified files to the temporary working dir, to be submitted later in the final "submit" pass, from a single machine.
	if (IsDistributedBuild())
	{
		// Ensure we don't hold on to packages of always loaded actors
		// When running distributed builds, we wanna leave the machine clean, so added files are deleted, check'd out files are reverted
		// and deleted files are restored.
		if (WorldPartition)
		{
			WorldPartition->Uninitialize();
			FWorldPartitionHelpers::DoCollectGarbage();
		}

		// Wait for pending async file writes before copying to working dir
		UPackage::WaitForAsyncFileWrites();

		TArray<FString> BuildProducts;

		if (!CopyFilesToWorkingDir("ToSubmit", ModifiedFiles, BuildProducts))
		{
			return false;
		}

		// Write build products to a file
		if (!AddBuildProducts(BuildProducts))
		{
			return false;
		}
	}

	return true;
}

bool UWorldPartitionHLODsBuilder::DeleteHLODActors()
{
	UE_LOG(LogWorldPartitionHLODsBuilder, Display, TEXT("#### Deleting HLOD actors ####"));

	TArray<UClass*> HLODActorClasses =
	{
		AWorldPartitionHLOD::StaticClass(),
		FindObject<UClass>(nullptr, TEXT("/Script/Engine.SpatialHashRuntimeGridInfo"))
	};

	TArray<FString> PackagesToDelete;
	for (FActorDescContainerInstanceCollection::TIterator<> Iterator(WorldPartition); Iterator; ++Iterator)
	{
		if (HLODActorClasses.FindByPredicate([ActorClass = Iterator->GetActorNativeClass()](const UClass* HLODClass) { return ActorClass->IsChildOf(HLODClass); }))
		{
			FString PackageName = Iterator->GetActorPackage().ToString();
			PackagesToDelete.Add(PackageName);
		}
	}

	// Ensure we don't hold on to packages of always loaded actors
	// When running distributed builds, we wanna leave the machine clean, so added files are deleted, checked out files are reverted
	// and deleted files are restored.
	WorldPartition->Uninitialize();
	FWorldPartitionHelpers::DoCollectGarbage();

	for (int32 PackageIndex = 0; PackageIndex < PackagesToDelete.Num(); ++PackageIndex)
	{
		const FString& PackageName = PackagesToDelete[PackageIndex];

		bool bDeleted = SourceControlHelper->Delete(PackageName);
		if (bDeleted)
		{
			UE_LOG(LogWorldPartitionHLODsBuilder, Display, TEXT("[%d / %d] Deleting %s..."), PackageIndex + 1, PackagesToDelete.Num(), *PackageName);
		}
		else
		{
			UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Failed to delete %s, exiting..."), *PackageName);
			return false;
		}
	}

	UE_LOG(LogWorldPartitionHLODsBuilder, Display, TEXT("#### Deleted %d HLOD actors ####"), PackagesToDelete.Num());

	return true;
}

bool UWorldPartitionHLODsBuilder::SubmitHLODActors()
{
	// Wait for pending async file writes before submitting
	UPackage::WaitForAsyncFileWrites();

	// Check in all modified files
	const FString ChangeDescription = FString::Printf(TEXT("Rebuilt HLODs for %s"), *World->GetPackage()->GetName());
	return OnFilesModified(ModifiedFiles.GetAllFiles(), ChangeDescription);
}

bool UWorldPartitionHLODsBuilder::DumpStats()
{
	const FString HLODStatsOutputFilename = FPaths::ProjectSavedDir() / TEXT("WorldPartition") / FString::Printf(TEXT("HLODStats-%08x.csv"), FPlatformProcess::GetCurrentProcessId());
	return UWorldPartitionHLODRuntimeSubsystem::WriteHLODStatsCSV(World, HLODStatsOutputFilename);
}

bool UWorldPartitionHLODsBuilder::GetHLODActorsToBuild(TArray<FGuid>& HLODActorsToBuild) const
{
	bool bRet = true;

	if (!BuildManifest.IsEmpty())
	{
		// Get HLOD actors to build from the BuildManifest file
		FConfigFile ConfigFile;
		ConfigFile.Read(BuildManifest);

		FString SectionName = GetHLODBuilderFolderName(BuilderIdx);

		const FConfigSection* ConfigSection = ConfigFile.FindSection(SectionName);
		if (ConfigSection)
		{
			TArray<FString> HLODActorGuidStrings;
			ConfigSection->MultiFind(TEXT("+HLODActorGuid"), HLODActorGuidStrings, /*bMaintainOrder=*/true);

			for (const FString& HLODActorGuidString : HLODActorGuidStrings)
			{
				FGuid HLODActorGuid;
				bRet = FGuid::Parse(HLODActorGuidString, HLODActorGuid);
				if (bRet)
				{
					HLODActorsToBuild.Add(HLODActorGuid);
				}
				else
				{
					UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Error parsing section [%s] in config file \"%s\""), *SectionName, *BuildManifest);
					break;
				}
			}
		}
		else
		{
			UE_LOG(LogWorldPartitionHLODsBuilder, Log, TEXT("No section [%s] found in config file \"%s\", assuming no HLOD needs to be built."), *SectionName, *BuildManifest);
			bRet = false;
		}
	}
	else
	{
		TArray<TArray<FGuid>> HLODWorkloads = GetHLODWorkloads(1);
		HLODActorsToBuild = MoveTemp(HLODWorkloads[0]);
	}

	return bRet;
}

TArray<TArray<FGuid>> UWorldPartitionHLODsBuilder::GetHLODWorkloads(int32 NumWorkloads) const
{
	if (!WorldPartition)
	{
		return { { FGuid() } };
	}

	// Build a mapping of 1 HLOD[Level] -> N HLOD[Level - 1]
	TMap<FGuid, TArray<FGuid>>	HLODParenting;
	for (FActorDescContainerInstanceCollection::TIterator<AWorldPartitionHLOD> HLODIterator(WorldPartition); HLODIterator; ++HLODIterator)
	{
		const FHLODActorDesc& HLODActorDesc = *(FHLODActorDesc*)HLODIterator->GetActorDesc();
		// Filter by HLOD actor
		if (!HLODActorToBuild.IsNone() && HLODActorDesc.GetActorLabel() != HLODActorToBuild)
		{
			continue;
		}

		// Filter by HLOD layer
		if (!HLODLayerToBuild.IsNone() && HLODActorDesc.GetSourceHLODLayer().GetAssetName() != HLODLayerToBuild)
		{
			continue;
		}

		HLODParenting.Add(HLODIterator->GetGuid(), HLODActorDesc.GetChildHLODActors());
	}

	// All child HLODs must be built before their parent HLOD
	// Create groups to ensure those will be processed in the correct order, on the same builder
	TMap<FGuid, TArray<FGuid>> HLODGroups;
	TSet<FGuid>				   TriagedHLODs;

	TFunction<void(TArray<FGuid>&, const FGuid&)> RecursiveAdd = [&TriagedHLODs, &HLODParenting, &HLODGroups, &RecursiveAdd](TArray<FGuid>& HLODGroup, const FGuid& HLODGuid)
	{
		if (!TriagedHLODs.Contains(HLODGuid))
		{
			TriagedHLODs.Add(HLODGuid);
			HLODGroup.Insert(HLODGuid, 0); // Child will come first in the list, as they need to be built first...
			TArray<FGuid>* ChildHLODs = HLODParenting.Find(HLODGuid);
			if (ChildHLODs)
			{
				for (const auto& ChildGuid : *ChildHLODs)
				{
					RecursiveAdd(HLODGroup, ChildGuid);
				}
			}
		}
		else
		{
			HLODGroup.Insert(MoveTemp(HLODGroups.FindChecked(HLODGuid)), 0);
			HLODGroups.Remove(HLODGuid);
		}
	};

	for (const auto& Pair : HLODParenting)
	{
		if (!TriagedHLODs.Contains(Pair.Key))
		{
			TArray<FGuid>& HLODGroup = HLODGroups.Add(Pair.Key);
			RecursiveAdd(HLODGroup, Pair.Key);
		}
	}

	// Sort groups by number of HLOD actors
	HLODGroups.ValueSort([](const TArray<FGuid>& GroupA, const TArray<FGuid>& GroupB) { return GroupA.Num() > GroupB.Num(); });

	// Dispatch them in multiple lists and try to balance the workloads as much as possible
	TArray<TArray<FGuid>> Workloads;
	Workloads.SetNum(NumWorkloads);

	int32 Idx = 0;
	for (const auto& Pair : HLODGroups)
	{
		Workloads[Idx % NumWorkloads].Append(Pair.Value);
		Idx++;
	}

	// Validate workloads to ensure our meshes are built in the correct order
	for (const TArray<FGuid>& Workload : Workloads)
	{
		check(ValidateWorkload(Workload));
	}

	return Workloads;
}

bool UWorldPartitionHLODsBuilder::ValidateWorkload(const TArray<FGuid>& Workload) const
{
	check(WorldPartition);

	TSet<FGuid> ProcessedHLOD;
	ProcessedHLOD.Reserve(Workload.Num());

	// For each HLOD entry in the workload, validate that its children are found before itself
	for (const FGuid& HLODActorGuid : Workload)
	{
		const FWorldPartitionActorDescInstance* ActorDescInstance = WorldPartition->GetActorDescInstance(HLODActorGuid);
		if(!ActorDescInstance)
		{
			UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Unknown actor guid found, your HLOD actors are probably out of date. Run with -SetupHLODs to fix this. Exiting..."));
			return false;
		}

		if (!ActorDescInstance->GetActorNativeClass()->IsChildOf<AWorldPartitionHLOD>())
		{
			UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Unexpected actor guid found in HLOD workload, exiting..."));
			return false;
		}

		const FHLODActorDesc* HLODActorDesc = static_cast<const FHLODActorDesc*>(ActorDescInstance->GetActorDesc());

		for (const FGuid& ChildHLODActorGuid : HLODActorDesc->GetChildHLODActors())
		{
			if (!ProcessedHLOD.Contains(ChildHLODActorGuid))
			{
				UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Child HLOD actor missing or out of order in HLOD workload, exiting..."));
				return false;
			}
		}

		ProcessedHLOD.Add(HLODActorGuid);
	}

	return true;
}

bool UWorldPartitionHLODsBuilder::GenerateBuildManifest(TMap<FString, int32>& FilesToBuilderMap) const
{
	TArray<TArray<FGuid>> BuildersWorkload = GetHLODWorkloads(BuilderCount);

	FConfigFile ConfigFile;
	ConfigFile.SetInt64(TEXT("General"), TEXT("BuilderCount"), BuilderCount);
	ConfigFile.SetString(TEXT("General"), TEXT("EngineVersion"), *FEngineVersion::Current().ToString());

	// When processing multiple maps, ensure that the worldload is distributed evenly between builders.
	// Otherwise, maps with a single HLOD would all end up being processed by the first builder, while the others would have no work.
	static int32 BuilderDispatchOffset = 0;

	for(int32 Idx = 0; Idx < BuilderCount; Idx++)
	{
		const int32 WorkloadIndex = Idx;
		const int32 BuilderIndex = (BuilderDispatchOffset + Idx) % BuilderCount;

		if (!BuildersWorkload.IsValidIndex(WorkloadIndex) || BuildersWorkload[WorkloadIndex].IsEmpty())
		{
			continue;
		}

		FString SectionName = GetHLODBuilderFolderName(BuilderIndex);

		for(const FGuid& ActorGuid : BuildersWorkload[WorkloadIndex])
		{
			ConfigFile.AddToSection(*SectionName, TEXT("+HLODActorGuid"), ActorGuid.ToString(EGuidFormats::Digits));

			if (WorldPartition)
			{
				// Track which builder is responsible to handle each actor
				const FWorldPartitionActorDescInstance* ActorDescInstance = WorldPartition->GetActorDescInstance(ActorGuid);
				if (!ActorDescInstance)
				{
					UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Invalid actor GUID found while generating the HLOD build manifest, exiting..."));
					return false;
				}
				FString ActorPackageFilename = USourceControlHelpers::PackageFilename(ActorDescInstance->GetActorPackage().ToString());
				FilesToBuilderMap.Emplace(ActorPackageFilename, BuilderIndex);
			}
		}
	}

	BuilderDispatchOffset++;

	ConfigFile.Dirty = true;

	bool bRet = ConfigFile.Write(BuildManifest);
	if (!bRet)
	{
		UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Failed to write HLOD build manifest \"%s\""), *BuildManifest);
	}

	return bRet;
}

/*
	Working Dir structure
		/HLODBuilder0
			/Add
				NewFileA
				NewFileB
			/Delete
				DeletedFileA
				DeletedFileB
			/Edit
				EditedFileA
				EditedFileB

		/HLODBuilder1
			...
		/ToSubmit
			...

	Distributed mode
		* Distributed mode is ran into 3 steps
			* Setup (1 job)		
			* Build (N jobs)	
			* Submit (1 job)	
		
		* The Setup step will place files under the "HLODBuilder[0-N]" folder. Those files could be new or modified HLOD actors that will be built in the Build step. The setup step will also place files into the "ToSubmit" folder (deleted HLOD actors for example).
		* Each parallel job in the Build step will retrieve files from the "HLODBuilder[0-N]" folder. They will then proceed to build the HLOD actors as specified in the build manifest file. All built HLOD actor files will then be placed in the /ToSubmit folder.
		* The Submit step will gather all files under /ToSubmit and submit them.
		

		|			Setup			|					Build					  |		   Submit			|
		/Content -----------> /HLODBuilder -----------> /Content -----------> /ToSubmit -----------> /Content
*/

const FName FileAction_Add(TEXT("Add"));
const FName FileAction_Edit(TEXT("Edit"));
const FName FileAction_Delete(TEXT("Delete"));

bool UWorldPartitionHLODsBuilder::CopyFilesToWorkingDir(const FString& TargetDir, const FHLODModifiedFiles& Files, TArray<FString>& BuildProducts)
{
	const FString AbsoluteTargetDir = DistributedBuildWorkingDir / TargetDir / TEXT("");

	bool bSuccess = true;

	auto CopyFileToWorkingDir = [&](const FString& SourceFilename, const FName FileAction)
	{
		FString SourceFilenameRelativeToRoot = SourceFilename;
		FPaths::MakePathRelativeTo(SourceFilenameRelativeToRoot, *FPaths::RootDir());

		FString TargetFilename = AbsoluteTargetDir / FileAction.ToString() / SourceFilenameRelativeToRoot;

		BuildProducts.Add(TargetFilename);

		if (FileAction != FileAction_Delete)
		{
			const bool bReplace = true;
			const bool bEvenIfReadOnly = true;
			bool bRet = IFileManager::Get().Copy(*TargetFilename, *SourceFilename, bReplace, bEvenIfReadOnly) == COPY_OK;
			if (!bRet)
			{
				UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Failed to copy file from \"%s\" to \"%s\""), *SourceFilename, *TargetFilename);
				bSuccess = false;
			}
		}
		else
		{
			bool bRet = FFileHelper::SaveStringToFile(TEXT(""), *TargetFilename);
			if (!bRet)
			{
				UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Failed to create empty file at \"%s\""), *TargetFilename);
				bSuccess = false;
			}
		}
	};

	Algo::ForEach(Files.Get(FHLODModifiedFiles::EFileOperation::FileAdded), [&](const FString& SourceFilename) { CopyFileToWorkingDir(SourceFilename, FileAction_Add); });
	Algo::ForEach(Files.Get(FHLODModifiedFiles::EFileOperation::FileEdited), [&](const FString& SourceFilename) { CopyFileToWorkingDir(SourceFilename, FileAction_Edit); });
	Algo::ForEach(Files.Get(FHLODModifiedFiles::EFileOperation::FileDeleted), [&](const FString& SourceFilename) { CopyFileToWorkingDir(SourceFilename, FileAction_Delete); });
	if (!bSuccess)
	{
		return false;
	}

	// Revert any file changes
	if (ISourceControlModule::Get().IsEnabled())
	{
		bool bRet = USourceControlHelpers::RevertFiles(Files.GetAllFiles());
		if (!bRet)
		{
			UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Failed to revert modified files: %s"), *USourceControlHelpers::LastErrorMsg().ToString());
			return false;
		}
	}

	// Delete files we added
	for (const FString& FileToDelete : Files.Get(FHLODModifiedFiles::EFileOperation::FileAdded))
	{
		if (!IFileManager::Get().Delete(*FileToDelete, false, true))
		{
			UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Error deleting file %s locally"), *FileToDelete);
			return false;
		}
	}

	return true;
}

bool UWorldPartitionHLODsBuilder::CopyFilesFromWorkingDir(const FString& SourceDir)
{
	const FString AbsoluteSourceDir = DistributedBuildWorkingDir / SourceDir / TEXT("");

	auto CopyFromWorkingDir = [](const TMap<FString, FString>& FilesToCopy) -> bool
	{
		for (const auto& Pair : FilesToCopy)
		{
			const bool bReplace = true;
			const bool bEvenIfReadOnly = true;
			bool bRet = IFileManager::Get().Copy(*Pair.Key, *Pair.Value, bReplace, bEvenIfReadOnly) == COPY_OK;
			if (!bRet)
			{
				UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Failed to copy file from \"%s\" to \"%s\""), *Pair.Value, *Pair.Key);
				return false;
			}
		}
		return true;
	};

	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *AbsoluteSourceDir, TEXT("*.*"), true, false);

	TMap<FString, FString>	FilesToAdd;
	TMap<FString, FString>	FilesToEdit;
	TArray<FString>			FilesToDelete;

	bool bRet = true;

	for(const FString& File : Files)
	{
		FString PathRelativeToWorkingDir = File;
		FPaths::MakePathRelativeTo(PathRelativeToWorkingDir, *AbsoluteSourceDir);

		FString FileActionString;
		const int32 SlashIndex = PathRelativeToWorkingDir.Find(TEXT("/"));
		if (SlashIndex != INDEX_NONE)
		{
			FileActionString = PathRelativeToWorkingDir.Mid(0, SlashIndex);
		}

		FPaths::MakePathRelativeTo(PathRelativeToWorkingDir, *(FileActionString / TEXT("")));
		FString FullPathInRootDirectory =  FPaths::RootDir() / PathRelativeToWorkingDir;

		FName FileAction(FileActionString);
		if (FileAction == FileAction_Add)
		{
			FilesToAdd.Add(*FullPathInRootDirectory, File);
		}
		else if (FileAction == FileAction_Edit)
		{
			FilesToEdit.Add(FullPathInRootDirectory, File);
		}
		else if (FileAction == FileAction_Delete)
		{
			FilesToDelete.Add(*FullPathInRootDirectory);
		}
		else
		{
			UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Unsupported file action %s for file %s"), *FileActionString, *FullPathInRootDirectory);
		}
	}

	TArray<FString> ToAdd;
	FilesToAdd.GetKeys(ToAdd);

	TArray<FString> ToEdit;
	FilesToEdit.GetKeys(ToEdit);

	// When resuming a build (after a crash for example) we don't need to perform any file operation as these modification were done in the first run.
	if (!bResumeBuild)
	{
	    // Add
	    if (!FilesToAdd.IsEmpty())
	    {
			bRet = CopyFromWorkingDir(FilesToAdd);
			if (!bRet)
			{
				return false;
			}

			if (ISourceControlModule::Get().IsEnabled())
			{
				bRet = USourceControlHelpers::MarkFilesForAdd(ToAdd);
				if (!bRet)
				{
					UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Adding files to revision control failed: %s"), *USourceControlHelpers::LastErrorMsg().ToString());
					return false;
				}
			}
	    }
    
	    // Delete
	    if (!FilesToDelete.IsEmpty())
	    {
			if (ISourceControlModule::Get().IsEnabled())
			{
				bRet = USourceControlHelpers::MarkFilesForDelete(FilesToDelete);
				if (!bRet)
				{
					UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Deleting files from revision control failed: %s"), *USourceControlHelpers::LastErrorMsg().ToString());
					return false;
				}
			}
			else
			{
				for (const FString& FileToDelete : FilesToDelete)
				{
					const bool bRequireExists = false;
					const bool bEvenIfReadOnly = true;
					bRet = IFileManager::Get().Delete(*FileToDelete, bRequireExists, bEvenIfReadOnly);
					if (!bRet)
					{
						UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Failed to delete file from disk: %s"), *USourceControlHelpers::LastErrorMsg().ToString());
						return false;
					}
				}
			}
	    }
    
	    // Edit
	    if (!FilesToEdit.IsEmpty())
	    {
		    if (ISourceControlModule::Get().IsEnabled())
		    {
				bRet = USourceControlHelpers::CheckOutFiles(ToEdit);
				if (!bRet)
				{
					UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Checking out files from revision control failed: %s"), *USourceControlHelpers::LastErrorMsg().ToString());
					return false;
				}
			}
		
			bRet = CopyFromWorkingDir(FilesToEdit);
			if (!bRet)
			{
				return false;
			}
	    }
	}

	// Keep track of all modified files
	ModifiedFiles.Append(FHLODModifiedFiles::EFileOperation::FileAdded, ToAdd);
	ModifiedFiles.Append(FHLODModifiedFiles::EFileOperation::FileDeleted, FilesToDelete);
	ModifiedFiles.Append(FHLODModifiedFiles::EFileOperation::FileEdited, ToEdit);

	// Force a rescan of the updated files
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	AssetRegistry.ScanModifiedAssetFiles(ModifiedFiles.GetAllFiles());

	return true;
}

bool UWorldPartitionHLODsBuilder::AddBuildProducts(const TArray<FString>& BuildProducts) const
{
	// Write build products to a file
	FString BuildProductsFile = FString::Printf(TEXT("%s/%s/%s"), *FPaths::RootDir(), *DistributedBuildWorkingDirName, *BuildProductsFileName);
	bool bRet = FFileHelper::SaveStringArrayToFile(BuildProducts, *BuildProductsFile, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), EFileWrite::FILEWRITE_Append);
	if (!bRet)
	{
		UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Error writing build product file %s"), *BuildProductsFile);
	}
	return bRet;
}
