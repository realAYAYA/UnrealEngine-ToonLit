// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/SavePackage/SavePackageUtilities.h"

#include "Algo/Sort.h"
#include "Algo/Unique.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/CookTagList.h"
#include "Blueprint/BlueprintSupport.h"
#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "Interfaces/ITargetPlatform.h"
#include "Misc/AssetRegistryInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/PathViews.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Misc/ScopedSlowTask.h"
#include "SaveContext.h"
#include "Serialization/BulkData.h"
#include "Serialization/EditorBulkData.h"
#include "Serialization/CompactBinarySerialization.h"
#include "Serialization/CompactBinaryWriter.h"
#include "Serialization/FileRegionArchive.h"
#include "Serialization/LargeMemoryWriter.h"
#include "Serialization/PackageWriter.h"
#include "Tasks/Task.h"
#include "UObject/ArchiveCookContext.h"
#include "UObject/AssetRegistryTagsContext.h"
#include "UObject/Class.h"
#include "UObject/GCScopeLock.h"
#include "UObject/Linker.h"
#include "UObject/LinkerLoad.h"
#include "UObject/LinkerSave.h"
#include "UObject/Object.h"
#include "UObject/ObjectRedirector.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectThreadContext.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY(LogSavePackage);
UE_TRACE_CHANNEL_DEFINE(SaveTimeChannel);

#if ENABLE_COOK_STATS
#include "ProfilingDebugging/ScopedTimers.h"

int32 FSavePackageStats::NumPackagesSaved = 0;
double FSavePackageStats::SavePackageTimeSec = 0.0;
double FSavePackageStats::TagPackageExportsPresaveTimeSec = 0.0;
double FSavePackageStats::TagPackageExportsTimeSec = 0.0;
double FSavePackageStats::FullyLoadLoadersTimeSec = 0.0;
double FSavePackageStats::ResetLoadersTimeSec = 0.0;
double FSavePackageStats::TagPackageExportsGetObjectsWithOuter = 0.0;
double FSavePackageStats::TagPackageExportsGetObjectsWithMarks = 0.0;
double FSavePackageStats::SerializeImportsTimeSec = 0.0;
double FSavePackageStats::SortExportsSeekfreeInnerTimeSec = 0.0;
double FSavePackageStats::SerializeExportsTimeSec = 0.0;
double FSavePackageStats::SerializeBulkDataTimeSec = 0.0;
double FSavePackageStats::AsyncWriteTimeSec = 0.0;
double FSavePackageStats::MBWritten = 0.0;
TMap<FName, FArchiveDiffStats> FSavePackageStats::PackageDiffStats;
int32 FSavePackageStats::NumberOfDifferentPackages= 0;

FCookStatsManager::FAutoRegisterCallback FSavePackageStats::RegisterCookStats(FSavePackageStats::AddSavePackageStats);

void FSavePackageStats::AddSavePackageStats(FCookStatsManager::AddStatFuncRef AddStat)
{
	// Don't use FCookStatsManager::CreateKeyValueArray because there's just too many arguments. Don't need to overburden the compiler here.
	TArray<FCookStatsManager::StringKeyValue> StatsList;
	StatsList.Empty(15);
#define ADD_COOK_STAT(Name) StatsList.Emplace(TEXT(#Name), LexToString(Name))
	ADD_COOK_STAT(NumPackagesSaved);
	ADD_COOK_STAT(SavePackageTimeSec);
	ADD_COOK_STAT(TagPackageExportsPresaveTimeSec);
	ADD_COOK_STAT(TagPackageExportsTimeSec);
	ADD_COOK_STAT(FullyLoadLoadersTimeSec);
	ADD_COOK_STAT(ResetLoadersTimeSec);
	ADD_COOK_STAT(TagPackageExportsGetObjectsWithOuter);
	ADD_COOK_STAT(TagPackageExportsGetObjectsWithMarks);
	ADD_COOK_STAT(SerializeImportsTimeSec);
	ADD_COOK_STAT(SortExportsSeekfreeInnerTimeSec);
	ADD_COOK_STAT(SerializeExportsTimeSec);
	ADD_COOK_STAT(SerializeBulkDataTimeSec);
	ADD_COOK_STAT(AsyncWriteTimeSec);
	ADD_COOK_STAT(MBWritten);

	AddStat(TEXT("Package.Save"), StatsList);

	{
		PackageDiffStats.ValueSort([](const FArchiveDiffStats& Lhs, const FArchiveDiffStats& Rhs) { return Lhs.NewFileTotalSize > Rhs.NewFileTotalSize; });

		StatsList.Empty(15);
		for (const TPair<FName, FArchiveDiffStats>& Stat : PackageDiffStats)
		{
			StatsList.Emplace(Stat.Key.ToString(), LexToString((double)Stat.Value.NewFileTotalSize / 1024.0 / 1024.0));
		}

		AddStat(TEXT("Package.DifferentPackagesSizeMBPerAsset"), StatsList);
	}

	{
		PackageDiffStats.ValueSort([](const FArchiveDiffStats& Lhs, const FArchiveDiffStats& Rhs) { return Lhs.NumDiffs > Rhs.NumDiffs; });

		StatsList.Empty(15);
		for (const TPair<FName, FArchiveDiffStats>& Stat : PackageDiffStats)
		{
			StatsList.Emplace(Stat.Key.ToString(), LexToString(Stat.Value.NumDiffs));
		}

		AddStat(TEXT("Package.NumberOfDifferencesInPackagesPerAsset"), StatsList);
	}

	{
		PackageDiffStats.ValueSort([](const FArchiveDiffStats& Lhs, const FArchiveDiffStats& Rhs) { return Lhs.DiffSize > Rhs.DiffSize; });

		StatsList.Empty(15);
		for (const TPair<FName, FArchiveDiffStats>& Stat : PackageDiffStats)
		{
			StatsList.Emplace(Stat.Key.ToString(), LexToString((double)Stat.Value.DiffSize / 1024.0 / 1024.0));
		}

		AddStat(TEXT("Package.PackageDifferencesSizeMBPerAsset"), StatsList);
	}

	int64 NewFileTotalSize = 0;
	int64 NumDiffs = 0;
	int64 DiffSize = 0;
	for (const TPair<FName, FArchiveDiffStats>& PackageStat : PackageDiffStats)
	{
		NewFileTotalSize += PackageStat.Value.NewFileTotalSize;
		NumDiffs += PackageStat.Value.NumDiffs;
		DiffSize += PackageStat.Value.DiffSize;
	}

	const double DifferentPackagesSizeMB = (double)NewFileTotalSize / 1024.0 / 1024.0;
	const int64  NumberOfDifferencesInPackages = NumDiffs;
	const double PackageDifferencesSizeMB = (double)DiffSize / 1024.0 / 1024.0;

	StatsList.Empty(15);
	ADD_COOK_STAT(NumberOfDifferentPackages);
	ADD_COOK_STAT(DifferentPackagesSizeMB);
	ADD_COOK_STAT(NumberOfDifferencesInPackages);
	ADD_COOK_STAT(PackageDifferencesSizeMB);

	AddStat(TEXT("Package.DiffTotal"), StatsList);

#undef ADD_COOK_STAT		
	const FString TotalString = TEXT("Total");
}

void FSavePackageStats::MergeStats(const TMap<FName, FArchiveDiffStats>& ToMerge)
{
	for (const TPair<FName, FArchiveDiffStats>& Stat : ToMerge)
	{
		PackageDiffStats.FindOrAdd(Stat.Key).DiffSize += Stat.Value.DiffSize;
		PackageDiffStats.FindOrAdd(Stat.Key).NewFileTotalSize += Stat.Value.NewFileTotalSize;
		PackageDiffStats.FindOrAdd(Stat.Key).NumDiffs += Stat.Value.NumDiffs;
	}
};

#endif

static FThreadSafeCounter OutstandingAsyncWrites;


// Initialization for GIsSavingPackage
bool GIsSavingPackage = false;

namespace UE
{
	bool IsSavingPackage(UObject* InOuter)
	{
		if (InOuter == nullptr)
		{
			// That global is only meant to be set and read from the game-thread...
			// otherwise it could interfere with normal operations coming from
			// other threads like async loading, etc...
			return GIsSavingPackage && IsInGameThread();
		}
		return InOuter->GetPackage()->HasAnyPackageFlags(PKG_IsSaving);
	}
}

FSavePackageSettings& FSavePackageSettings::GetDefaultSettings()
{
	static FSavePackageSettings Default;
	return Default;
}

namespace UE::SavePackageUtilities
{

const FName NAME_World("World");
const FName NAME_Level("Level");
const FName NAME_PrestreamPackage("PrestreamPackage");

/**
* A utility that records the state of a package's files before we start moving and overwriting them. 
* This provides an easy way for us to restore the original state of the package incase of failures 
* while saving.
*/
class FPackageBackupUtility
{
public:
	FPackageBackupUtility(const FPackagePath& InPackagePath)
		: PackagePath(InPackagePath)
	{

	}

	/** 
	* Record a file that has been moved. These will need to be moved back to
	* restore the package.
	*/
	void RecordMovedFile( const FString& OriginalPath, const FString& NewLocation)
	{
		MovedOriginalFiles.Emplace(OriginalPath, NewLocation);
	}

	/** 
	* Record a newly created file that did not exist before. These will need
	* deleting to restore the package.
	*/
	void RecordNewFile(const FString& NewLocation)
	{
		NewFiles.Add(NewLocation);
	}

	/** Restores the package to it's original state */
	void RestorePackage()
	{
		IFileManager& FileSystem = IFileManager::Get();

		UE_LOG(LogSavePackage, Verbose, TEXT("Restoring package '%s'"), *PackagePath.GetDebugName());

		// First we should delete any new file that has been saved for the package
		for (const FString& Entry : NewFiles)
		{
			if (!FileSystem.Delete(*Entry))
			{
				UE_LOG(LogSavePackage, Error, TEXT("Failed to delete newly added file '%s' when trying to restore the package state and the package could be unstable, please revert in revision control!"), *Entry);
			}
		}

		// Now we can move back the original files
		for (const TPair<FString, FString>& Entry : MovedOriginalFiles)
		{
			if (!FileSystem.Move(*Entry.Key, *Entry.Value))
			{
				UE_LOG(LogSavePackage, Error, TEXT("Failed to restore package '%s', the file '%s' is in an incorrect state and the package could be unstable, please revert in revision control!"), *PackagePath.GetDebugName(), *Entry.Key);
			}
		}
	}

	/** Deletes the backed up files once they are no longer required. */
	void DiscardBackupFiles()
	{
		IFileManager& FileSystem = IFileManager::Get();

		// Note that we do not warn if we fail to delete a backup file as that is probably
		// the least of the users problems at the moment.
		for (const TPair<FString, FString>& Entry : MovedOriginalFiles)
		{
			FileSystem.Delete(*Entry.Value, /*RequireExists*/false, /*EvenReadOnly*/true);
		}
	}

private:
	const FPackagePath& PackagePath;

	TArray<FString> NewFiles;
	TArray<TPair<FString, FString>> MovedOriginalFiles;	
};

/**
 * Determines the set of object marks that should be excluded for the target platform
 *
 * @param TargetPlatform	The platform being saved for, or null for saving platform-agnostic version
 *
 * @return Excluded object marks specific for the particular target platform, objects with any of these marks will be rejected from the cook
 */
EObjectMark GetExcludedObjectMarksForTargetPlatform(const class ITargetPlatform* TargetPlatform)
{
	// we always want to exclude NotForTargetPlatform (in other words, later on, the target platform
	// can mark an object as NotForTargetPlatform, and then this will exlude that object and anything
	// inside it, from being saved out)
	EObjectMark ObjectMarks = OBJECTMARK_NotForTargetPlatform;

	if (TargetPlatform)
	{
		if (!TargetPlatform->AllowsEditorObjects())
		{
			ObjectMarks = (EObjectMark)(ObjectMarks | OBJECTMARK_EditorOnly);
		}

		const bool bIsServerOnly = TargetPlatform->IsServerOnly();
		const bool bIsClientOnly = TargetPlatform->IsClientOnly();

		if (bIsServerOnly)
		{
			ObjectMarks = (EObjectMark)(ObjectMarks | OBJECTMARK_NotForServer);
		}
		else if (bIsClientOnly)
		{
			ObjectMarks = (EObjectMark)(ObjectMarks | OBJECTMARK_NotForClient);
		}
	}

	return ObjectMarks;
}

/**
 * Find most likely culprit that caused the objects in the passed in array to be considered for saving.
 *
 * @param	BadObjects	array of objects that are considered "bad" (e.g. non- RF_Public, in different map package, ...)
 * @return	UObject that is considered the most likely culprit causing them to be referenced or NULL
 */
void FindMostLikelyCulprit(const TArray<UObject*>& BadObjects, UObject*& MostLikelyCulprit, FString& OutReferencer, FSaveContext* InOptionalSaveContext)
{
	UObject* ArchetypeCulprit = nullptr;
	UObject* ReferencedCulprit = nullptr;
	const FProperty* ReferencedCulpritReferencer = nullptr;

	auto IsObjectIncluded = [InOptionalSaveContext](UObject* InObject)
	{
		// if we passed in a SaveContext use that to validate if the object is an import or an export instead of marks
		if (InOptionalSaveContext)
		{
			return InOptionalSaveContext->IsIncluded(InObject);
		}
		return InObject->HasAnyMarks(EObjectMark(OBJECTMARK_TagExp | OBJECTMARK_TagImp));
	};

	// Iterate over all objects that are marked as unserializable/ bad and print out their referencers.
	for (int32 BadObjIndex = 0; BadObjIndex < BadObjects.Num(); BadObjIndex++)
	{
		UObject* Obj = BadObjects[BadObjIndex];

		// SavePackage adds references to the class archetype manually; if this type is a class archetype and it is private, mark it as an error
		// for that reason rather than checking references. Class archetypes must be public since instances of their class in other packages can refer to them
		if (Obj->HasAnyFlags(RF_ArchetypeObject | RF_DefaultSubObject | RF_ClassDefaultObject))
		{
			UE_LOG(LogSavePackage, Warning, TEXT("%s is a private Archetype object"), *Obj->GetFullName());
			TArray<const TCHAR*> Flags;
			auto AddFlagIfPresent = [&Flags, Obj](EObjectFlags InFlag, const TCHAR* Descriptor)
			{
				if (Obj->HasAnyFlags(InFlag))
				{
					Flags.Add(Descriptor);
				}
			};
			AddFlagIfPresent(RF_ArchetypeObject, TEXT("RF_ArchetypeObject"));
			AddFlagIfPresent(RF_ClassDefaultObject, TEXT("RF_ClassDefaultObject"));
			AddFlagIfPresent(RF_DefaultSubObject, TEXT("RF_DefaultSubObject"));
			UE_LOG(LogSavePackage, Warning, TEXT("\tThis object is an archetype (flags include %s) but is private. This is a code error from the generator of the object. All archetype objects must be public."),
				*FString::Join(Flags, TEXT("|")));

			if (ArchetypeCulprit == nullptr)
			{
				ArchetypeCulprit = Obj;
			}
			continue;
		}

		// Do not run more than a certain number of ref gather if there's a lot of illegal references
		const int32 MaxNumberOfRefGather = 5;
		if (BadObjIndex < MaxNumberOfRefGather)
		{
			UE_LOG(LogSavePackage, Warning, TEXT("\r\nReferencers of %s:"), *Obj->GetFullName());
			FReferencerInformationList Refs;
			if (IsReferenced(Obj, RF_Public, EInternalObjectFlags::Native, true, &Refs))
			{
				for (int32 i = 0; i < Refs.ExternalReferences.Num(); i++)
				{
					UObject* RefObj = Refs.ExternalReferences[i].Referencer;
					if (IsObjectIncluded(RefObj))
					{
						if (RefObj->GetFName() == NAME_PersistentLevel || RefObj->GetClass()->GetFName() == NAME_World)
						{
							// these types of references should be ignored
							continue;
						}

						UE_LOG(LogSavePackage, Warning, TEXT("\t%s (%i refs)"), *RefObj->GetFullName(), Refs.ExternalReferences[i].TotalReferences);
						for (int32 j = 0; j < Refs.ExternalReferences[i].ReferencingProperties.Num(); j++)
						{
							const FProperty* Prop = Refs.ExternalReferences[i].ReferencingProperties[j];
							UE_LOG(LogSavePackage, Warning, TEXT("\t\t%i) %s"), j, *Prop->GetFullName());
							ReferencedCulpritReferencer = Prop;
						}

						// Later ReferencedCulprits are higher priority than earlier culprits. TODO: Not sure if this is an intentional behavior or if they choice was arbitrary.
						ReferencedCulprit = Obj;
					}
				}
			}
		}
	}

	if (ArchetypeCulprit)
	{
		// ArchetypeCulprits are the most likely to be the problem; they are definitely a problem
		MostLikelyCulprit = ArchetypeCulprit;
		OutReferencer = TEXT("Referenced because it is an archetype object");
	}
	else
	{
		MostLikelyCulprit = ReferencedCulprit; // Might be null, in which case we didn't find one
		if (ReferencedCulpritReferencer)
		{
			OutReferencer = *ReferencedCulpritReferencer->GetName();
		}
		else
		{
			OutReferencer = TEXT("Unknown property");
		}
	}

	if (MostLikelyCulprit == nullptr)
	{
		// Make sure we report something
		for (UObject* BadObject : BadObjects)
		{
			if (BadObject)
			{
				MostLikelyCulprit = BadObject;
				break;
			}
		}
	}
}

ESavePackageResult FinalizeTempOutputFiles(const FPackagePath& PackagePath, const FSavePackageOutputFileArray& OutputFiles, const FDateTime& FinalTimeStamp)
{
	UE_LOG(LogSavePackage, Log,  TEXT("Moving output files for package: %s"), *PackagePath.GetDebugName());

	IFileManager& FileSystem = IFileManager::Get();
	FPackageBackupUtility OriginalPackageState(PackagePath);

	UE_LOG(LogSavePackage, Verbose, TEXT("Moving existing files to the temp directory"));

	TArray<bool, TInlineAllocator<4>> CanFileBeMoved;
	CanFileBeMoved.SetNum(OutputFiles.Num());

	// First check if any of the target files that already exist are read only still, if so we can fail the 
	// whole thing before we try to move any files
	for (int32 Index = 0; Index < OutputFiles.Num(); ++Index)
	{	
		const FSavePackageOutputFile& File = OutputFiles[Index];

		if (File.FileMemoryBuffer.IsValid())
		{
			ensureMsgf(false, TEXT("FinalizeTempOutputFiles does not handle async saving files! (%s)"), *PackagePath.GetDebugName());
			return ESavePackageResult::Error;
		}

		if (!File.TempFilePath.IsEmpty())
		{
			FFileStatData FileStats = FileSystem.GetStatData(*File.TargetPath);
			if (FileStats.bIsValid && FileStats.bIsReadOnly)
			{
				UE_LOG(LogSavePackage, Error, TEXT("Cannot remove '%s' as it is read only!"), *File.TargetPath);
				return ESavePackageResult::Error;
			}
			CanFileBeMoved[Index] = FileStats.bIsValid;
		}
		else
		{
			CanFileBeMoved[Index] = false;
		}
	}

	// Now we need to move all of the files that we are going to overwrite (if any) so that we 
	// can restore them if anything goes wrong.
	for (int32 Index = 0; Index < OutputFiles.Num(); ++Index)
	{
		if (CanFileBeMoved[Index]) 
		{
			const FSavePackageOutputFile& File = OutputFiles[Index];

			const FString BaseFilename = FPaths::GetBaseFilename(File.TargetPath);
			const FString TempFilePath = FPaths::CreateTempFilename(*FPaths::ProjectSavedDir(), *BaseFilename.Left(32));

			if (FileSystem.Move(*TempFilePath, *File.TargetPath))
			{
				OriginalPackageState.RecordMovedFile(File.TargetPath, TempFilePath);
			}
			else
			{
				UE_LOG(LogSavePackage, Warning, TEXT("Failed to move '%s' to temp directory"), *File.TargetPath);
				OriginalPackageState.RestorePackage();

				return ESavePackageResult::Error;
			}
		}
	}

	// Now attempt to move the new files from the temp location to the final location
	for (const FSavePackageOutputFile& File : OutputFiles)
	{
		if (!File.TempFilePath.IsEmpty()) // Only try to move output files that were saved to temp files
		{
			UE_LOG(LogSavePackage, Log, TEXT("Moving '%s' to '%s'"), *File.TempFilePath, *File.TargetPath);

			if (FileSystem.Move(*File.TargetPath, *File.TempFilePath))
			{
				OriginalPackageState.RecordNewFile(File.TargetPath);
			}
			else
			{
				UE_LOG(LogSavePackage, Warning, TEXT("Failed to move '%s' from temp directory"), *File.TargetPath);
				OriginalPackageState.RestorePackage();

				return ESavePackageResult::Error;
			}

			if (FinalTimeStamp != FDateTime::MinValue())
			{
				FileSystem.SetTimeStamp(*File.TargetPath, FinalTimeStamp);
			}
		}
	}

	// Finally we can clean up the temp files as we do not need to restore them (failure to delete them will not be considered an error)
	OriginalPackageState.DiscardBackupFiles();

	return ESavePackageResult::Success;
}

void WriteToFile(const FString& Filename, const uint8* InDataPtr, int64 InDataSize)
{
	IFileManager& FileManager = IFileManager::Get();

	for (int tries = 0; tries < 3; ++tries)
	{
		if (FArchive* Ar = FileManager.CreateFileWriter(*Filename))
		{
			Ar->Serialize(/* grrr */ const_cast<uint8*>(InDataPtr), InDataSize);
			bool bArchiveError = Ar->IsError();
			delete Ar;

			int64 ActualSize = FileManager.FileSize(*Filename);
			if (ActualSize != InDataSize)
			{
				FileManager.Delete(*Filename);

				UE_LOG(LogSavePackage, Fatal, TEXT("Could not save to %s! Tried to write %" INT64_FMT " bytes but resultant size was %" INT64_FMT ".%s"),
					*Filename, InDataSize, ActualSize, bArchiveError ? TEXT(" Ar->Serialize failed.") : TEXT(""));
			}
			return;
		}
	}

	UE_LOG(LogSavePackage, Fatal, TEXT("Could not write to %s!"), *Filename);
}

void AsyncWriteFile(FLargeMemoryPtr Data, const int64 DataSize, const TCHAR* Filename, EAsyncWriteOptions Options, TArrayView<const FFileRegion> InFileRegions)
{
	OutstandingAsyncWrites.Increment();
	FString OutputFilename(Filename);

	UE::Tasks::Launch(TEXT("PackageAsyncFileWrite"), [Data = MoveTemp(Data), DataSize, OutputFilename = MoveTemp(OutputFilename), Options, FileRegions = TArray<FFileRegion>(InFileRegions)]() mutable
	{
		WriteToFile(OutputFilename, Data.Get(), DataSize);

		if (FileRegions.Num() > 0)
		{
			TArray<uint8> Memory;
			FMemoryWriter Ar(Memory);
			FFileRegion::SerializeFileRegions(Ar, FileRegions);

			WriteToFile(OutputFilename + FFileRegion::RegionsFileExtension, Memory.GetData(), Memory.Num());
		}

		OutstandingAsyncWrites.Decrement();
	});
}

void AsyncWriteFile(EAsyncWriteOptions Options, FSavePackageOutputFile& File)
{
	checkf(File.TempFilePath.IsEmpty(), TEXT("AsyncWriteFile does not handle temp files!"));
	AsyncWriteFile(FLargeMemoryPtr(File.FileMemoryBuffer.Release()), File.DataSize, *File.TargetPath, Options, File.FileRegions);
}

/** For a CDO get all of the subobjects templates nested inside it or it's class */
void GetCDOSubobjects(UObject* CDO, TArray<UObject*>& Subobjects)
{
	TArray<UObject*> CurrentSubobjects;
	TArray<UObject*> NextSubobjects;

	// Recursively search for subobjects. Only care about ones that have a full subobject chain as some nested objects are set wrong
	GetObjectsWithOuter(CDO->GetClass(), NextSubobjects, false);
	GetObjectsWithOuter(CDO, NextSubobjects, false);

	while (NextSubobjects.Num() > 0)
	{
		CurrentSubobjects = NextSubobjects;
		NextSubobjects.Empty();
		for (UObject* SubObj : CurrentSubobjects)
		{
			if (SubObj->HasAnyFlags(RF_DefaultSubObject | RF_ArchetypeObject))
			{
				Subobjects.Add(SubObj);
				GetObjectsWithOuter(SubObj, NextSubobjects, false);
			}
		}
	}
}
	
bool IsStrippedEditorOnlyObject(const UObject* InObject, EEditorOnlyObjectFlags Flags)
{
#if WITH_EDITOR
	// Configurable via ini setting
	static struct FCanStripEditorOnlyExportsAndImports
	{
		bool bCanStripEditorOnlyObjects;
		FCanStripEditorOnlyExportsAndImports()
			: bCanStripEditorOnlyObjects(true)
		{
			GConfig->GetBool(TEXT("Core.System"), TEXT("CanStripEditorOnlyExportsAndImports"), bCanStripEditorOnlyObjects, GEngineIni);
		}
		FORCEINLINE operator bool() const { return bCanStripEditorOnlyObjects; }
	} CanStripEditorOnlyExportsAndImports;
	if (!CanStripEditorOnlyExportsAndImports)
	{
		return false;
	}
	
	return IsEditorOnlyObjectInternal(InObject, Flags);
#else
	return true;
#endif
}

bool IsUpdatingLoadedPath(bool bIsCooking, const FPackagePath& TargetPackagePath, uint32 SaveFlags)
{
#if WITH_EDITOR
	return !bIsCooking &&							// Do not update the loadedpath if we're cooking
		TargetPackagePath.IsMountedPath() &&		// Do not update the loadedpath if the new path is not a viable mounted path
		!(SaveFlags & SAVE_BulkDataByReference) &&	// Do not update the loadedpath if it's an EditorDomainSave. TODO: Change the name of this flag.
		!(SaveFlags & SAVE_FromAutosave);			// Do not update the loadedpath if it's an autosave.
#else
	return false; // Saving when not in editor never updates the LoadedPath
#endif
}

bool IsProceduralSave(bool bIsCooking, const FPackagePath& TargetPackagePath, uint32 SaveFlags)
{
#if WITH_EDITOR
	return bIsCooking ||							// Cooking is a procedural save
		(SaveFlags & SAVE_BulkDataByReference);		// EditorDomainSave is a procedural save. TODO: Change the name of this flag.
#else
	return false; // Saving when not in editor never has user changes
#endif
}

void CallPreSave(UObject* Object, FObjectSaveContextData& ObjectSaveContext)
{
	SCOPED_SAVETIMER_TEXT(*WriteToString<256>(GetClassTraceScope(Object), TEXTVIEW("_PreSave")));
	PRAGMA_DISABLE_DEPRECATION_WARNINGS;
	Object->PreSave(ObjectSaveContext.TargetPlatform);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS;

	FObjectPreSaveContext ObjectPreSaveContext(ObjectSaveContext);
	ObjectSaveContext.bBaseClassCalled = false;
	ObjectSaveContext.NumRefPasses = 0;
	Object->PreSave(ObjectPreSaveContext);
	if (!ObjectSaveContext.bBaseClassCalled)
	{
		UE_LOG(LogSavePackage, Warning, TEXT("Class %s did not call Super::PreSave"), *Object->GetClass()->GetName());
	}
	// When we deprecate PreSave, and need to take different actions based on the PreSave, remove this bAllowPreSave variable
	constexpr bool bAllowPreSave = true;
	if (!bAllowPreSave && ObjectSaveContext.NumRefPasses > 1)
	{
		UE_LOG(LogSavePackage, Warning, TEXT("Class %s overrides the deprecated PreSave function"), *Object->GetClass()->GetName());
	}
}

void CallPreSaveRoot(UObject* Object, FObjectSaveContextData& ObjectSaveContext)
{
	SCOPED_SAVETIMER_TEXT(*WriteToString<256>(GetClassTraceScope(Object), TEXTVIEW("_PreSave")));
	PRAGMA_DISABLE_DEPRECATION_WARNINGS;
	bool bLegacyNeedsCleanup = Object->PreSaveRoot(*ObjectSaveContext.TargetFilename);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS;

	ObjectSaveContext.bCleanupRequired = false;
	Object->PreSaveRoot(FObjectPreSaveRootContext(ObjectSaveContext));
	ObjectSaveContext.bCleanupRequired |= bLegacyNeedsCleanup;
}

void CallPostSaveRoot(UObject* Object, FObjectSaveContextData& ObjectSaveContext, bool bNeedsCleanup)
{
	SCOPED_SAVETIMER_TEXT(*WriteToString<256>(GetClassTraceScope(Object), TEXTVIEW("_PreSave")));
	PRAGMA_DISABLE_DEPRECATION_WARNINGS;
	Object->PostSaveRoot(bNeedsCleanup);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS;

	ObjectSaveContext.bCleanupRequired = bNeedsCleanup;
	Object->PostSaveRoot(FObjectPostSaveRootContext(ObjectSaveContext));
}

EObjectFlags NormalizeTopLevelFlags(EObjectFlags TopLevelFlags, bool bIsCooking)
{
	// if we aren't cooking and top level flags aren't empty, add RF_HasExternalPackage to them to catch external packages data
	if (TopLevelFlags != RF_NoFlags && !bIsCooking)
	{
		TopLevelFlags |= RF_HasExternalPackage;
	}
	return TopLevelFlags;
}

void IncrementOutstandingAsyncWrites()
{
	OutstandingAsyncWrites.Increment();
}

void DecrementOutstandingAsyncWrites()
{
	OutstandingAsyncWrites.Decrement();
}

void ResetCookStats()
{
#if ENABLE_COOK_STATS
	FSavePackageStats::NumPackagesSaved = 0;
#endif
}

int32 GetNumPackagesSaved()
{
#if ENABLE_COOK_STATS
	return FSavePackageStats::NumPackagesSaved;
#else
	return 0;
#endif
}

#if WITH_EDITOR
FAddResaveOnDemandPackage OnAddResaveOnDemandPackage;
#endif

} // end namespace UE::SavePackageUtilities

FObjectSaveContextData::FObjectSaveContextData(UPackage* Package, const ITargetPlatform* InTargetPlatform, const TCHAR* InTargetFilename, uint32 InSaveFlags)
{
	Set(Package, InTargetPlatform, InTargetFilename, InSaveFlags);
}

FObjectSaveContextData::FObjectSaveContextData(UPackage* Package, const ITargetPlatform* InTargetPlatform, const FPackagePath& TargetPath, uint32 InSaveFlags)
{
	Set(Package, InTargetPlatform, TargetPath, InSaveFlags);
}

void FObjectSaveContextData::Set(UPackage* Package, const ITargetPlatform* InTargetPlatform, const TCHAR* InTargetFilename, uint32 InSaveFlags)
{
	FPackagePath PackagePath(FPackagePath::FromLocalPath(InTargetFilename));
	if (PackagePath.GetHeaderExtension() == EPackageExtension::Unspecified)
	{
		PackagePath.SetHeaderExtension(EPackageExtension::EmptyString);
	}
	Set(Package, InTargetPlatform, PackagePath, InSaveFlags);
}

void FObjectSaveContextData::Set(UPackage* Package, const ITargetPlatform* InTargetPlatform, const FPackagePath& TargetPath, uint32 InSaveFlags)
{
	TargetFilename = TargetPath.GetLocalFullPath();
	TargetPlatform = InTargetPlatform;
	SaveFlags = InSaveFlags;
	OriginalPackageFlags = Package ? Package->GetPackageFlags() : 0;
	bProceduralSave = UE::SavePackageUtilities::IsProceduralSave(InTargetPlatform != nullptr, TargetPath, InSaveFlags);
	bUpdatingLoadedPath = UE::SavePackageUtilities::IsUpdatingLoadedPath(InTargetPlatform != nullptr, TargetPath, InSaveFlags);
}

bool IsEditorOnlyObject(const UObject* InObject, bool bCheckRecursive)
{
	using namespace UE::SavePackageUtilities;
	EEditorOnlyObjectFlags Flags = EEditorOnlyObjectFlags::None;
	Flags |= bCheckRecursive ? EEditorOnlyObjectFlags::CheckRecursive : EEditorOnlyObjectFlags::None;
	return IsEditorOnlyObjectInternal(InObject, Flags);
}

bool IsEditorOnlyObject(const UObject* InObject, bool bCheckRecursive, bool bCheckMarks)
{
	using namespace UE::SavePackageUtilities;
	EEditorOnlyObjectFlags Flags = EEditorOnlyObjectFlags::None;
	Flags |= bCheckRecursive ? EEditorOnlyObjectFlags::CheckRecursive : EEditorOnlyObjectFlags::None;
	PRAGMA_DISABLE_DEPRECATION_WARNINGS;
	Flags |= bCheckMarks ? EEditorOnlyObjectFlags::CheckMarks : EEditorOnlyObjectFlags::None;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS;
	return IsEditorOnlyObjectInternal(InObject, Flags);
}

namespace UE::SavePackageUtilities
{

bool IsEditorOnlyObjectInternal(const UObject* InObject, EEditorOnlyObjectFlags Flags)
{
	bool bCheckRecursive = EnumHasAnyFlags(Flags, EEditorOnlyObjectFlags::CheckRecursive);
	bool bIgnoreEditorOnlyClass = EnumHasAnyFlags(Flags, EEditorOnlyObjectFlags::ApplyHasNonEditorOnlyReferences) &&
		InObject->HasNonEditorOnlyReferences();
	PRAGMA_DISABLE_DEPRECATION_WARNINGS;
	bool bCheckMarks = EnumHasAnyFlags(Flags, EEditorOnlyObjectFlags::CheckMarks);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS;

	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("IsEditorOnlyObject"), STAT_IsEditorOnlyObject, STATGROUP_LoadTime);
	check(InObject);

	// CDOs must be included if their class and archetype and outer are included.
	// Ignore their value of IsEditorOnly
	if (!InObject->HasAnyFlags(RF_ClassDefaultObject))
	{
		if (!bIgnoreEditorOnlyClass &&
			((bCheckMarks && InObject->HasAnyMarks(OBJECTMARK_EditorOnly)) || InObject->IsEditorOnly()))
		{
			return true;
		}
	}

	// If this is a package that is editor only or the object is in editor-only package,
	// the object is editor-only too.
	const bool bIsAPackage = InObject->IsA<UPackage>();
	const UPackage* Package;
	if (bIsAPackage)
	{
		if (InObject->HasAnyFlags(RF_ClassDefaultObject))
		{
			// The default package is not editor-only, and it is part of a cycle that would cause infinite recursion: DefaultPackage -> GetOuter() -> Package:/Script/CoreUObject -> GetArchetype() -> DefaultPackage
			return false;
		}
		Package = static_cast<const UPackage*>(InObject);
	}
	else
	{
		// In the case that the object is an external object, we want to use its host package, rather than its
		// external package when testing editor-only. All external packages are editor-only, but the objects in
		// the external package logically belong to the host package, and are editoronly if and only if the host is.
		// So use GetOutermostObject()->GetPackage(). This will be the same as GetPackage for non-external objects.
		UObject* HostObject = InObject->GetOutermostObject();
		Package = HostObject->GetPackage();
	}
	
	if (Package && Package->HasAnyPackageFlags(PKG_EditorOnly))
	{
		return true;
	}

	if (bCheckRecursive && !InObject->IsNative())
	{
		UObject* Outer = InObject->GetOuter();
		if (Outer && Outer != Package)
		{
			if (IsEditorOnlyObjectInternal(Outer, Flags))
			{
				return true;
			}
		}
		if (!bIgnoreEditorOnlyClass)
		{
			const UStruct* InStruct = Cast<UStruct>(InObject);
			if (InStruct)
			{
				const UStruct* SuperStruct = InStruct->GetSuperStruct();
				if (SuperStruct && IsEditorOnlyObjectInternal(SuperStruct, Flags))
				{
					return true;
				}
			}
			else
			{
				if (IsEditorOnlyObjectInternal(InObject->GetClass(), Flags))
				{
					return true;
				}

				UObject* Archetype = InObject->GetArchetype();
				if (Archetype && IsEditorOnlyObjectInternal(Archetype, Flags))
				{
					return true;
				}
			}
		}
	}
	return false;
}

} // namespace UE::SavePackageUtilities

void FObjectImportSortHelper::SortImports(FLinkerSave* Linker)
{
	TArray<FObjectImport>& Imports = Linker->ImportMap;
	if (Imports.IsEmpty())
	{
		return;
	}

	// Map of UObject => full name; optimization for sorting.
	TMap<TObjectPtr<UObject>, FString> ObjectToFullNameMap;
	ObjectToFullNameMap.Reserve(Imports.Num());

	for (const FObjectImport& Import : Imports)
	{
		if (Import.XObject)
		{
			ObjectToFullNameMap.Add(Import.XObject, Import.XObject->GetFullName());
		}
	}

	auto CompareObjectImports = [&ObjectToFullNameMap](const FObjectImport& A, const FObjectImport& B)
	{
		int32 Result = 0;
		if (A.XObject == nullptr)
		{
			Result = 1;
		}
		else if (B.XObject == nullptr)
		{
			Result = -1;
		}
		else
		{
			const FString* FullNameA = ObjectToFullNameMap.Find(A.XObject);
			const FString* FullNameB = ObjectToFullNameMap.Find(B.XObject);
			checkSlow(FullNameA);
			checkSlow(FullNameB);

			Result = FCString::Stricmp(**FullNameA, **FullNameB);
		}

		return Result < 0;
	};
	
	Algo::Sort(Linker->ImportMap, CompareObjectImports);
}

void FObjectExportSortHelper::SortExports(FLinkerSave* Linker)
{
	TArray<FObjectExport>& Exports = Linker->ExportMap;
	if (Exports.IsEmpty())
	{
		return;
	}
	
	// Map of UObject => full name; optimization for sorting.
	TMap<UObject*, FString> ObjectToFullNameMap;
	ObjectToFullNameMap.Reserve(Exports.Num());

	for (const FObjectExport& Export : Exports)
	{
		if (Export.Object)
		{
			ObjectToFullNameMap.Add(Export.Object, Export.Object->GetFullName());
		}
	}

	auto CompareObjectExports = [&ObjectToFullNameMap](const FObjectExport& A, const FObjectExport& B)
	{
		int32 Result = 0;
		if (A.Object == nullptr)
		{
			Result = 1;
		}
		else if (B.Object == nullptr)
		{
			Result = -1;
		}
		else
		{
			const FString* FullNameA = ObjectToFullNameMap.Find(A.Object);
			const FString* FullNameB = ObjectToFullNameMap.Find(B.Object);
			checkSlow(FullNameA);
			checkSlow(FullNameB);

			Result = FCString::Stricmp(**FullNameA, **FullNameB);
		}

		return Result < 0;
	};
	
	Algo::Sort(Linker->ExportMap, CompareObjectExports);
}

FEDLCookChecker::FEDLNodeHash::FEDLNodeHash()
{
}

FEDLCookChecker::FEDLNodeHash::FEDLNodeHash(const FEDLNodeHash& Other)
{
	bIsNode = Other.bIsNode;
	if (bIsNode)
	{
		Nodes = Other.Nodes;
	}
	else
	{
		Object = Other.Object;
	}
	NodeID = Other.NodeID;
	ObjectEvent = Other.ObjectEvent;
}

FEDLCookChecker::FEDLNodeHash::FEDLNodeHash(const TArray<FEDLNodeData>* InNodes, FEDLNodeID InNodeID, EObjectEvent InObjectEvent)
	: Nodes(InNodes)
	, NodeID(InNodeID)
	, bIsNode(true)
	, ObjectEvent(InObjectEvent)
{
}

FEDLCookChecker::FEDLNodeHash::FEDLNodeHash(TObjectPtr<UObject> InObject, EObjectEvent InObjectEvent)
	: Object(InObject)
	, bIsNode(false)
	, ObjectEvent(InObjectEvent)
{
}

bool FEDLCookChecker::FEDLNodeHash::operator==(const FEDLNodeHash& Other) const
{
	if (ObjectEvent != Other.ObjectEvent)
	{
		return false;
	}

	uint32 LocalNodeID;
	uint32 OtherNodeID;
	TObjectPtr<const UObject> LocalObject;
	TObjectPtr<const UObject> OtherObject;
	FName LocalName = ObjectNameFirst(*this, LocalNodeID, LocalObject);
	FName OtherName = ObjectNameFirst(Other, OtherNodeID, OtherObject);

	do
	{
		if (LocalName != OtherName)
		{
			return false;
		}
		LocalName = ObjectNameNext(*this, LocalNodeID, LocalObject);
		OtherName = ObjectNameNext(Other, OtherNodeID, OtherObject);
	} while (!LocalName.IsNone() && !OtherName.IsNone());
	return LocalName.IsNone() == OtherName.IsNone();
}

FEDLCookChecker::FEDLNodeHash& FEDLCookChecker::FEDLNodeHash::operator=(const FEDLNodeHash& Other)
{
	bIsNode = Other.bIsNode;
	if (bIsNode)
	{
		Nodes = Other.Nodes;
	}
	else
	{
		Object = Other.Object;
	}
	NodeID = Other.NodeID;
	ObjectEvent = Other.ObjectEvent;

	return *this;
}

uint32 GetTypeHash(const FEDLCookChecker::FEDLNodeHash& A)
{
	uint32 Hash = 0;

	uint32 LocalNodeID;
	TObjectPtr<const UObject> LocalObject;
	FName LocalName = FEDLCookChecker::FEDLNodeHash::ObjectNameFirst(A, LocalNodeID, LocalObject);
	do
	{
		Hash = HashCombine(Hash, GetTypeHash(LocalName));
		LocalName = FEDLCookChecker::FEDLNodeHash::ObjectNameNext(A, LocalNodeID, LocalObject);
	} while (!LocalName.IsNone());

	return (Hash << 1) | (uint32)A.ObjectEvent;
}

FName FEDLCookChecker::FEDLNodeHash::GetName() const
{
	if (bIsNode)
	{
		return (*Nodes)[NodeID].Name;
	}
	else
	{
		return Object.GetFName();
	}
}

bool FEDLCookChecker::FEDLNodeHash::TryGetParent(FEDLCookChecker::FEDLNodeHash& Parent) const
{
	EObjectEvent ParentObjectEvent = EObjectEvent::Create; // For purposes of parents, which is used only to get the ObjectPath, we always use the Create version of the node as the parent
	if (bIsNode)
	{
		FEDLNodeID ParentID = (*Nodes)[NodeID].ParentID;
		if (ParentID != NodeIDInvalid)
		{
			Parent = FEDLNodeHash(Nodes, ParentID, ParentObjectEvent);
			return true;
		}
	}
	else
	{
		TObjectPtr<UObject> ParentObject = Object.GetOuter();
		if (ParentObject)
		{
			Parent = FEDLNodeHash(ParentObject, ParentObjectEvent);
			return true;
		}
	}
	return false;
}

FEDLCookChecker::EObjectEvent FEDLCookChecker::FEDLNodeHash::GetObjectEvent() const
{
	return ObjectEvent;
}

void FEDLCookChecker::FEDLNodeHash::SetNodes(const TArray<FEDLNodeData>* InNodes)
{
	if (bIsNode)
	{
		Nodes = InNodes;
	}
}

FName FEDLCookChecker::FEDLNodeHash::ObjectNameFirst(const FEDLNodeHash& InNode, uint32& OutNodeID,  TObjectPtr<const UObject>& OutObject)
{
	if (InNode.bIsNode)
	{
		OutNodeID = InNode.NodeID;
		return (*InNode.Nodes)[OutNodeID].Name;
	}
	else
	{
		OutObject = InNode.Object;
		return OutObject.GetFName();
	}
}

FName FEDLCookChecker::FEDLNodeHash::ObjectNameNext(const FEDLNodeHash& InNode, uint32& OutNodeID, TObjectPtr<const UObject>& OutObject)
{
	if (InNode.bIsNode)
	{
		OutNodeID = (*InNode.Nodes)[OutNodeID].ParentID;
		return OutNodeID != NodeIDInvalid ? (*InNode.Nodes)[OutNodeID].Name : NAME_None;
	}
	else
	{
		OutObject = OutObject.GetOuter();
		return OutObject ? OutObject.GetFName() : NAME_None;
	}
}

FEDLCookChecker::FEDLNodeData::FEDLNodeData(FEDLNodeID InID, FEDLNodeID InParentID, FName InName, EObjectEvent InObjectEvent)
	: Name(InName)
	, ID(InID)
	, ParentID(InParentID)
	, ObjectEvent(InObjectEvent)
	, bIsExport(false)
{
}

FEDLCookChecker::FEDLNodeData::FEDLNodeData(FEDLNodeID InID, FEDLNodeID InParentID, FName InName, FEDLNodeData&& Other)
	: Name(InName)
	, ID(InID)
	, ImportingPackagesSorted(MoveTemp(Other.ImportingPackagesSorted))
	, ParentID(InParentID)
	, ObjectEvent(Other.ObjectEvent)
	, bIsExport(Other.bIsExport)
{
	// Note that Other Name and ParentID must be unmodified, since they might still be needed for GetHashCode calls from children
	Other.ImportingPackagesSorted.Empty();
}

FEDLCookChecker::FEDLNodeHash FEDLCookChecker::FEDLNodeData::GetNodeHash(const FEDLCookChecker& Owner) const
{
	return FEDLNodeHash(&Owner.Nodes, ID, ObjectEvent);
}

FString FEDLCookChecker::FEDLNodeData::ToString(const FEDLCookChecker& Owner) const
{
	TStringBuilder<NAME_SIZE> Result;
	switch (ObjectEvent)
	{
	case EObjectEvent::Create:
		Result << TEXT("Create:");
		break;
	case EObjectEvent::Serialize:
		Result << TEXT("Serialize:");
		break;
	default:
		check(false);
		break;
	}
	AppendPathName(Owner, Result);
	return FString(Result);
}

void FEDLCookChecker::FEDLNodeData::AppendPathName(const FEDLCookChecker& Owner, FStringBuilderBase& Result) const
{
	if (ParentID != NodeIDInvalid)
	{
		const FEDLNodeData& ParentNode = Owner.Nodes[ParentID];
		ParentNode.AppendPathName(Owner, Result);
		bool bParentIsOutermost = ParentNode.ParentID == NodeIDInvalid;
		Result << (bParentIsOutermost ? TEXT(".") : SUBOBJECT_DELIMITER);
	}
	Name.AppendString(Result);
}

FName FEDLCookChecker::FEDLNodeData::GetPackageName(const FEDLCookChecker& Owner) const
{
	if (ParentID != NodeIDInvalid)
	{
		// @todo ExternalPackages: We need to store ExternalPackage pointers on the Node and return that
		return Owner.Nodes[ParentID].GetPackageName(Owner);
	}
	return Name;
}

void FEDLCookChecker::FEDLNodeData::Merge(FEDLCookChecker::FEDLNodeData&& Other)
{
	check(ObjectEvent == Other.ObjectEvent);
	bIsExport = bIsExport | Other.bIsExport;

	ImportingPackagesSorted.Append(Other.ImportingPackagesSorted);
	Algo::Sort(ImportingPackagesSorted, FNameFastLess());
	ImportingPackagesSorted.SetNum(Algo::Unique(ImportingPackagesSorted), EAllowShrinking::Yes);
}

FEDLCookCheckerThreadState::FEDLCookCheckerThreadState()
{
	Checker.SetActiveIfNeeded();

	FScopeLock CookCheckerInstanceLock(&FEDLCookChecker::CookCheckerInstanceCritical);
	FEDLCookChecker::CookCheckerInstances.Add(&Checker);
}

void FEDLCookChecker::SetActiveIfNeeded()
{
	bIsActive = !FParse::Param(FCommandLine::Get(), TEXT("DisableEDLCookChecker"));
}

void FEDLCookChecker::Reset()
{
	check(!GIsSavingPackage);

	Nodes.Reset();
	NodeHashToNodeID.Reset();
	NodePrereqs.Reset();
	bIsActive = false;
}

LLM_DEFINE_TAG(EDLCookChecker);

void FEDLCookChecker::AddImport(TObjectPtr<UObject> Import, UPackage* ImportingPackage)
{
	if (bIsActive)
	{
		if (!Import->GetOutermost()->HasAnyPackageFlags(PKG_CompiledIn))
		{
			LLM_SCOPE_BYTAG(EDLCookChecker);
			FEDLNodeID NodeId = FindOrAddNode(FEDLNodeHash(Import, EObjectEvent::Serialize));
			FEDLNodeData& NodeData = Nodes[NodeId];
			FName ImportingPackageName = ImportingPackage->GetFName();
			TArray<FName>& Sorted = NodeData.ImportingPackagesSorted;
			int32 InsertionIndex = Algo::LowerBound(Sorted, ImportingPackageName, FNameFastLess());
			if (InsertionIndex == Sorted.Num() || Sorted[InsertionIndex] != ImportingPackageName)
			{
				Sorted.Insert(ImportingPackageName, InsertionIndex);
			}
		}
	}
}
void FEDLCookChecker::AddExport(UObject* Export)
{
	if (bIsActive)
	{
		LLM_SCOPE_BYTAG(EDLCookChecker);
		FEDLNodeID SerializeID = FindOrAddNode(FEDLNodeHash(Export, EObjectEvent::Serialize));
		Nodes[SerializeID].bIsExport = true;
		FEDLNodeID CreateID = FindOrAddNode(FEDLNodeHash(Export, EObjectEvent::Create));
		Nodes[CreateID].bIsExport = true;
		AddDependency(SerializeID, CreateID); // every export must be created before it can be serialized...these arcs are implicit and not listed in any table.
	}
}

void FEDLCookChecker::AddArc(UObject* DepObject, bool bDepIsSerialize, UObject* Export, bool bExportIsSerialize)
{
	if (bIsActive)
	{
		LLM_SCOPE_BYTAG(EDLCookChecker);
		FEDLNodeID ExportID = FindOrAddNode(FEDLNodeHash(Export, bExportIsSerialize ? EObjectEvent::Serialize : EObjectEvent::Create));
		FEDLNodeID DepID = FindOrAddNode(FEDLNodeHash(DepObject, bDepIsSerialize ? EObjectEvent::Serialize : EObjectEvent::Create));
		AddDependency(ExportID, DepID);
	}
}

void FEDLCookChecker::AddPackageWithUnknownExports(FName LongPackageName)
{
	if (bIsActive)
	{
		LLM_SCOPE_BYTAG(EDLCookChecker);
		PackagesWithUnknownExports.Add(LongPackageName);
	}
}


void FEDLCookChecker::AddDependency(FEDLNodeID SourceID, FEDLNodeID TargetID)
{
	NodePrereqs.Add(SourceID, TargetID);
}

void FEDLCookChecker::StartSavingEDLCookInfoForVerification()
{
	LLM_SCOPE_BYTAG(EDLCookChecker);
	FScopeLock CookCheckerInstanceLock(&CookCheckerInstanceCritical);
	for (FEDLCookChecker* Checker : CookCheckerInstances)
	{
		Checker->Reset();
		Checker->SetActiveIfNeeded();
	}
}

bool FEDLCookChecker::CheckForCyclesInner(TSet<FEDLNodeID>& Visited, TSet<FEDLNodeID>& Stack, const FEDLNodeID& Visit, FEDLNodeID& FailNode)
{
	bool bResult = false;
	if (Stack.Contains(Visit))
	{
		FailNode = Visit;
		bResult = true;
	}
	else
	{
		bool bWasAlreadyTested = false;
		Visited.Add(Visit, &bWasAlreadyTested);
		if (!bWasAlreadyTested)
		{
			Stack.Add(Visit);
			for (auto It = NodePrereqs.CreateConstKeyIterator(Visit); !bResult && It; ++It)
			{
				bResult = CheckForCyclesInner(Visited, Stack, It.Value(), FailNode);
			}
			Stack.Remove(Visit);
		}
	}
	UE_CLOG(bResult && Stack.Contains(FailNode), LogSavePackage, Error, TEXT("Cycle Node %s"), *Nodes[Visit].ToString(*this));
	return bResult;
}

FEDLCookChecker::FEDLNodeID FEDLCookChecker::FindOrAddNode(const FEDLNodeHash& NodeHash)
{
	uint32 TypeHash = GetTypeHash(NodeHash);
	FEDLNodeID* NodeIDPtr = NodeHashToNodeID.FindByHash(TypeHash, NodeHash);
	if (NodeIDPtr)
	{
		return *NodeIDPtr;
	}

	FName Name = NodeHash.GetName();
	FEDLNodeHash ParentHash;
	FEDLNodeID ParentID = NodeHash.TryGetParent(ParentHash) ? FindOrAddNode(ParentHash) : NodeIDInvalid;
	FEDLNodeID NodeID = Nodes.Num();
	FEDLNodeData& NewNodeData = Nodes.Emplace_GetRef(NodeID, ParentID, Name, NodeHash.GetObjectEvent());
	NodeHashToNodeID.AddByHash(TypeHash, NewNodeData.GetNodeHash(*this), NodeID);
	return NodeID;
}

FEDLCookChecker::FEDLNodeID FEDLCookChecker::FindOrAddNode(FEDLNodeData&& NodeData, const FEDLCookChecker& OldOwnerOfNode, FEDLNodeID ParentIDInThis, bool& bNew)
{
	// Note that NodeData's Name and ParentID must be unmodified, since they might still be needed for GetHashCode calls from children

	FEDLNodeHash NodeHash = NodeData.GetNodeHash(OldOwnerOfNode);
	uint32 TypeHash = GetTypeHash(NodeHash);
	FEDLNodeID* NodeIDPtr = NodeHashToNodeID.FindByHash(TypeHash, NodeHash);
	if (NodeIDPtr)
	{
		bNew = false;
		return *NodeIDPtr;
	}

	FEDLNodeID NodeID = Nodes.Num();
	FEDLNodeData& NewNodeData = Nodes.Emplace_GetRef(NodeID, ParentIDInThis, NodeData.Name, MoveTemp(NodeData));
	NodeHashToNodeID.AddByHash(TypeHash, NewNodeData.GetNodeHash(*this), NodeID);
	bNew = true;
	return NodeID;
}

FEDLCookChecker::FEDLNodeID FEDLCookChecker::FindNode(const FEDLNodeHash& NodeHash)
{
	const FEDLNodeID* NodeIDPtr = NodeHashToNodeID.Find(NodeHash);
	return NodeIDPtr ? *NodeIDPtr : NodeIDInvalid;
}

void FEDLCookChecker::Merge(FEDLCookChecker&& Other)
{
	if (Nodes.Num() == 0)
	{
		Swap(Nodes, Other.Nodes);
		Swap(NodeHashToNodeID, Other.NodeHashToNodeID);
		Swap(NodePrereqs, Other.NodePrereqs);

		// Switch the pointers in all of the swapped data to point at this instead of Other
		for (TPair<FEDLNodeHash, FEDLNodeID>& KVPair : NodeHashToNodeID)
		{
			FEDLNodeHash& NodeHash = KVPair.Key;
			NodeHash.SetNodes(&Nodes);
		}
	}
	else
	{
		Other.NodeHashToNodeID.Empty(); // We will be invalidating the data these NodeHashes point to in the Other.Nodes loop, so empty the array now to avoid using it by accident

		TArray<FEDLNodeID> RemapIDs;
		RemapIDs.Reserve(Other.Nodes.Num());
		for (FEDLNodeData& NodeData : Other.Nodes)
		{
			FEDLNodeID ParentID;
			if (NodeData.ParentID == NodeIDInvalid)
			{
				ParentID = NodeIDInvalid;
			}
			else
			{
				// Parents should be earlier in the nodes list than children, since we always FindOrAdd the parent (and hence add it to the nodelist) when creating the child.
				// Since the parent is earlier in the nodes list, we have already transferred it, and its ID in this->Nodes is therefore RemapIDs[Other.ParentID]
				check(NodeData.ParentID < NodeData.ID);
				ParentID = RemapIDs[NodeData.ParentID];
			}

			bool bNew;
			FEDLNodeID NodeID = FindOrAddNode(MoveTemp(NodeData), Other, ParentID, bNew);
			if (!bNew)
			{
				Nodes[NodeID].Merge(MoveTemp(NodeData));
			}
			RemapIDs.Add(NodeID);
		}

		for (const TPair<FEDLNodeID, FEDLNodeID>& Prereq : Other.NodePrereqs)
		{
			FEDLNodeID SourceID = RemapIDs[Prereq.Key];
			FEDLNodeID TargetID = RemapIDs[Prereq.Value];
			AddDependency(SourceID, TargetID);
		}

		Other.NodePrereqs.Empty();
		Other.Nodes.Empty();
	}

	if (PackagesWithUnknownExports.Num() == 0)
	{
		Swap(PackagesWithUnknownExports, Other.PackagesWithUnknownExports);
	}
	else
	{
		PackagesWithUnknownExports.Reserve(Other.PackagesWithUnknownExports.Num());
		for (FName PackageName : Other.PackagesWithUnknownExports)
		{
			PackagesWithUnknownExports.Add(PackageName);
		}
		Other.PackagesWithUnknownExports.Empty();
	}
}

FEDLCookChecker FEDLCookChecker::AccumulateAndClear()
{
	FEDLCookChecker Accumulator;

	FScopeLock CookCheckerInstanceLock(&CookCheckerInstanceCritical);
	for (FEDLCookChecker* Checker : CookCheckerInstances)
	{
		if (Checker->bIsActive)
		{
			Accumulator.bIsActive = true;
			Accumulator.Merge(MoveTemp(*Checker));
			Checker->Reset();
			Checker->bIsActive = true;
		}
	}
	return Accumulator;
}

void FEDLCookChecker::Verify(const UE::SavePackageUtilities::FEDLMessageCallback& MessageCallback,
	bool bFullReferencesExpected)
{
	check(!GIsSavingPackage);
	FEDLCookChecker Accumulator = AccumulateAndClear();

	FString SeverityStr;
	GConfig->GetString(TEXT("CookSettings"), TEXT("CookContentMissingSeverity"), SeverityStr, GEditorIni);
	ELogVerbosity::Type MissingContentSeverity = ParseLogVerbosityFromString(SeverityStr);

	if (Accumulator.bIsActive)
	{
		double StartTime = FPlatformTime::Seconds();
			
		if (bFullReferencesExpected)
		{
			// imports to things that are not exports...
			for (const FEDLNodeData& NodeData : Accumulator.Nodes)
			{
				if (NodeData.bIsExport)
				{
					// The node is an export; imports of it are valid
					continue;
				}

				if (Accumulator.PackagesWithUnknownExports.Contains(NodeData.GetPackageName(Accumulator)))
				{
					// The node is an object in a package that exists, but for which we do not know the exports
					// because e.g. it was iteratively skipped in the current cook. Suppress warnings about it
					continue;
				}

				// Any imports of this non-exported node are an error; log them all if they exist
				if (NodeData.ImportingPackagesSorted.IsEmpty())
				{
					continue;
				}

				const FEDLNodeData* NodeDataOfExportPackage = &NodeData;
				while (NodeDataOfExportPackage->ParentID != NodeIDInvalid)
				{
					int32 ParentNodeIndex = static_cast<int32>(NodeDataOfExportPackage->ParentID);
					check(Accumulator.Nodes.IsValidIndex(ParentNodeIndex));
					NodeDataOfExportPackage = &Accumulator.Nodes[ParentNodeIndex];
				}

				const TCHAR* ReasonExportIsMissing = TEXT("");
				if (NodeDataOfExportPackage->bIsExport)
				{
					ReasonExportIsMissing = TEXT("the object was stripped out of the target package when saved");
				}
				else
				{
					ReasonExportIsMissing = TEXT("the target package was marked NeverCook or is not cookable for the target platform");
				}

				for (FName PackageName : NodeData.ImportingPackagesSorted)
				{
					TStringBuilder<512> Message;
					Message << TEXTVIEW("Content is missing from cook. Source package referenced an object in target package but ");
					Message << ReasonExportIsMissing << TEXT(".\n");
					Message << TEXT("\tSource package: ") << PackageName << TEXT("\n");
					Message << TEXT("\tTarget package: ") << NodeDataOfExportPackage->Name << TEXT("\n");
					Message << TEXT("\tReferenced object: ");
					NodeData.AppendPathName(Accumulator, Message);
					MessageCallback(MissingContentSeverity, Message);
				}
			}
		}

		// cycles in the dep graph
		TSet<FEDLNodeID> Visited;
		TSet<FEDLNodeID> Stack;
		bool bHadCycle = false;
		for (const FEDLNodeData& NodeData : Accumulator.Nodes)
		{
			if (!NodeData.bIsExport)
			{
				continue;
			}
			FEDLNodeID FailNode;
			if (Accumulator.CheckForCyclesInner(Visited, Stack, NodeData.ID, FailNode))
			{
				UE_LOG(LogSavePackage, Error, TEXT("----- %s contained a cycle (listed above)."), *Accumulator.Nodes[FailNode].ToString(Accumulator));
				bHadCycle = true;
			}
		}
		if (bHadCycle)
		{
			UE_LOG(LogSavePackage, Fatal, TEXT("EDL dep graph contained a cycle (see errors, above). This is fatal at runtime so it is fatal at cook time."));
		}
		UE_LOG(LogSavePackage, Display, TEXT("Took %fs to verify the EDL loading graph."), float(FPlatformTime::Seconds() - StartTime));
	}
}

void FEDLCookChecker::MoveToCompactBinaryAndClear(FCbWriter& Writer, bool& bOutHasData)
{
	bOutHasData = false;
	FEDLCookChecker Accumulator = AccumulateAndClear();
	if (!Accumulator.bIsActive)
	{
		return;
	}
	if (Accumulator.Nodes.IsEmpty() && Accumulator.NodePrereqs.IsEmpty() && Accumulator.PackagesWithUnknownExports.IsEmpty())
	{
		return;
	}
	bOutHasData = true;

	Accumulator.WriteToCompactBinary(Writer);
}

bool FEDLCookChecker::AppendFromCompactBinary(FCbFieldView Field)
{
	FEDLCookChecker Instance;
	if (!Instance.ReadFromCompactBinary(Field))
	{
		return false;
	}
	FEDLCookChecker& CurrentChecker = FEDLCookCheckerThreadState::Get().Checker;
	CurrentChecker.Merge(MoveTemp(Instance));
	return true;
}

void FEDLCookChecker::WriteToCompactBinary(FCbWriter& Writer)
{
	Writer.BeginObject();
	{
		Writer.BeginArray("Nodes");
		for (const FEDLNodeData& Node : Nodes)
		{
			Writer << Node.Name;
			Writer << Node.ImportingPackagesSorted;
			Writer << Node.ParentID;
			Writer << static_cast<uint8>(Node.ObjectEvent);
			Writer << Node.bIsExport;
		}
		Writer.EndArray();
		Writer.BeginArray("NodePrereqs");
		for (const TPair<FEDLNodeID, FEDLNodeID>& Pair : NodePrereqs)
		{
			Writer << static_cast<uint32>(Pair.Key);
			Writer << static_cast<uint32>(Pair.Value);
		}
		Writer.EndArray();
		Writer.BeginArray("PackagesWithUnknownExports");
		for (FName PackageName : PackagesWithUnknownExports)
		{
			Writer << PackageName;
		}
		Writer.EndArray();
	}
	Writer.EndObject();
}


bool FEDLCookChecker::ReadFromCompactBinary(FCbFieldView Field)
{
	Reset();

	bool bSuccess = false;
	ON_SCOPE_EXIT
	{
		if (!bSuccess)
		{
			Reset();
		}
	};

	FCbFieldView NodesField = Field["Nodes"];
	const uint64 NumNodes = NodesField.AsArrayView().Num() / 5;
	if (NumNodes > MAX_int32)
	{
		return false;
	}
	Nodes.Reserve(static_cast<int32>(NumNodes));
	if (NodesField.HasError())
	{
		return false;
	}
	FCbFieldViewIterator NodeIter = NodesField.CreateViewIterator();
	while (NodeIter)
	{
		FEDLNodeID NodeID = Nodes.Num();
		FEDLNodeData& Node = Nodes.Emplace_GetRef();
		Node.ID = NodeID;
		if (!LoadFromCompactBinary(NodeIter, Node.Name)) { return false; }
		++NodeIter;
		if (!LoadFromCompactBinary(NodeIter, Node.ImportingPackagesSorted)) { return false; }
		++NodeIter;
		if (!LoadFromCompactBinary(NodeIter, Node.ParentID)) { return false; }
		++NodeIter;
		uint8 LocalObjectEvent;
		if (!LoadFromCompactBinary(NodeIter, LocalObjectEvent)) { return false; }
		if (LocalObjectEvent > static_cast<uint8>(EObjectEvent::Max)) { return false; }
		Node.ObjectEvent = static_cast<EObjectEvent>(LocalObjectEvent);
		++NodeIter;
		if (!LoadFromCompactBinary(NodeIter, Node.bIsExport)) { return false; }
		++NodeIter;
	}

	FCbFieldView PrereqsField = Field["NodePrereqs"];
	const int64 NumNodePrereqs = PrereqsField.AsArrayView().Num() / 2;
	if (NumNodePrereqs > MAX_int32)
	{
		return false;
	}
	NodePrereqs.Reserve(static_cast<int32>(NumNodePrereqs));
	if (PrereqsField.HasError())
	{
		return false;
	}
	FCbFieldViewIterator PrereqsIter = PrereqsField.CreateViewIterator();
	while (PrereqsIter)
	{
		uint32 Key;
		uint32 Value;
		if (!LoadFromCompactBinary(PrereqsIter, Key)) { return false; }
		++PrereqsIter;
		if (!LoadFromCompactBinary(PrereqsIter, Value)) { return false; }
		++PrereqsIter;
		NodePrereqs.Add(static_cast<FEDLNodeID>(Key), static_cast<FEDLNodeID>(Value));
	}

	FCbFieldView PackagesWithUnknownExportsField = Field["PackagesWithUnknownExports"];
	const int64 NumPackagesWithUnknownExports = PackagesWithUnknownExportsField.AsArrayView().Num();
	if (NumPackagesWithUnknownExports > MAX_int32)
	{
		return false;
	}
	PackagesWithUnknownExports.Reserve(static_cast<int32>(NumPackagesWithUnknownExports));
	if (PackagesWithUnknownExportsField.HasError())
	{
		return false;
	}
	for (FCbFieldView PackageNameField : PackagesWithUnknownExportsField)
	{
		FName PackageName;
		if (!LoadFromCompactBinary(PackageNameField, PackageName))
		{
			return false;
		}
		PackagesWithUnknownExports.Add(PackageName);
	}

	for (const FEDLNodeData& Node : Nodes)
	{
		NodeHashToNodeID.Add(Node.GetNodeHash(*this), Node.ID);
	}
	bIsActive = !Nodes.IsEmpty() || !NodePrereqs.IsEmpty() || !PackagesWithUnknownExports.IsEmpty();

	bSuccess = true;
	return true;
}

FCriticalSection FEDLCookChecker::CookCheckerInstanceCritical;
TArray<FEDLCookChecker*> FEDLCookChecker::CookCheckerInstances;

namespace UE::SavePackageUtilities
{

void StartSavingEDLCookInfoForVerification()
{
	LLM_SCOPE_BYTAG(EDLCookChecker);
	FEDLCookChecker::StartSavingEDLCookInfoForVerification();
}

void VerifyEDLCookInfo(bool bFullReferencesExpected)
{
	VerifyEDLCookInfo([](ELogVerbosity::Type Verbosity, FStringView Message)
		{
#if !NO_LOGGING
			FMsg::Logf(__FILE__, __LINE__, LogSavePackage.GetCategoryName(), Verbosity, TEXT("%.*s"),
				Message.Len(), Message.GetData());
#endif
		}, bFullReferencesExpected);
}

void VerifyEDLCookInfo(const UE::SavePackageUtilities::FEDLMessageCallback& MessageCallback,
	bool bFullReferencesExpected)
{
	LLM_SCOPE_BYTAG(EDLCookChecker);
	FEDLCookChecker::Verify(MessageCallback, bFullReferencesExpected);
}

void EDLCookInfoAddIterativelySkippedPackage(FName LongPackageName)
{
	LLM_SCOPE_BYTAG(EDLCookChecker);
	FEDLCookCheckerThreadState::Get().AddPackageWithUnknownExports(LongPackageName);
}

void EDLCookInfoMoveToCompactBinaryAndClear(FCbWriter& Writer, bool& bOutHasData)
{
	LLM_SCOPE_BYTAG(EDLCookChecker);
	FEDLCookChecker::MoveToCompactBinaryAndClear(Writer, bOutHasData);
}

void EDLCookInfoMoveToCompactBinaryAndClear(FCbWriter& Writer, bool& bOutHasData, FName PackageName)
{
	// For simplicity, instead of sending only information related to the given Package, we send all data.
	LLM_SCOPE_BYTAG(EDLCookChecker);
	FEDLCookChecker::MoveToCompactBinaryAndClear(Writer, bOutHasData);
}

bool EDLCookInfoAppendFromCompactBinary(FCbFieldView Field)
{
	LLM_SCOPE_BYTAG(EDLCookChecker);
	return FEDLCookChecker::AppendFromCompactBinary(Field);
}

}

FScopedSavingFlag::FScopedSavingFlag(bool InSavingConcurrent, UPackage* InSavedPackage)
	: bSavingConcurrent(InSavingConcurrent)
	, SavedPackage(InSavedPackage)
{
	check(!IsGarbageCollecting());

	// We need the same lock as GC so that no StaticFindObject can happen in parallel to saving a package
	if (IsInGameThread())
	{
		FGCCSyncObject::Get().GCLock();
	}
	else
	{
		FGCCSyncObject::Get().LockAsync();
	}

	// Do not change GIsSavingPackage while saving concurrently. It should have been set before and after all packages are saved
	if (!bSavingConcurrent)
	{
		GIsSavingPackage = true;
	}

	// Mark the package as being saved 
	if (SavedPackage)
	{
		SavedPackage->SetPackageFlags(PKG_IsSaving);
	}
}

FScopedSavingFlag::~FScopedSavingFlag()
{
	if (!bSavingConcurrent)
	{
		GIsSavingPackage = false;
	}
	if (IsInGameThread())
	{
		FGCCSyncObject::Get().GCUnlock();
	}
	else
	{
		FGCCSyncObject::Get().UnlockAsync();
	}

	if (SavedPackage)
	{
		SavedPackage->ClearPackageFlags(PKG_IsSaving);
	}

}

FCanSkipEditorReferencedPackagesWhenCooking::FCanSkipEditorReferencedPackagesWhenCooking()
	: bCanSkipEditorReferencedPackagesWhenCooking(UE::SavePackageUtilities::CanSkipEditorReferencedPackagesWhenCooking())
{
}

namespace UE::SavePackageUtilities
{

bool CanSkipEditorReferencedPackagesWhenCooking()
{
	bool bResult = true;
	GConfig->GetBool(TEXT("Core.System"), TEXT("CanSkipEditorReferencedPackagesWhenCooking"), bResult, GEngineIni);
	return bResult;
}

/**
 * Static: Saves thumbnail data for the specified package outer and linker
 *
 * @param	InOuter							the outer to use for the new package
 * @param	Linker							linker we're currently saving with
 * @param	Slot							structed archive slot we are saving too (temporary)
 */
void SaveThumbnails(UPackage* InOuter, FLinkerSave* Linker, FStructuredArchive::FSlot Slot)
{
	FStructuredArchive::FRecord Record = Slot.EnterRecord();

	Linker->Summary.ThumbnailTableOffset = 0;

#if WITH_EDITORONLY_DATA
	// Do we have any thumbnails to save?
	if( !(Linker->Summary.GetPackageFlags() & PKG_FilterEditorOnly) && InOuter->HasThumbnailMap() )
	{
		const FThumbnailMap& PackageThumbnailMap = InOuter->GetThumbnailMap();


		// Figure out which objects have thumbnails.  Note that we only want to save thumbnails
		// for objects that are actually in the export map.  This is so that we avoid saving out
		// thumbnails that were cached for deleted objects and such.
		TArray< FObjectFullNameAndThumbnail > ObjectsWithThumbnails;
		for( int32 i=0; i<Linker->ExportMap.Num(); i++ )
		{
			FObjectExport& Export = Linker->ExportMap[i];
			if( Export.Object )
			{
				const FName ObjectFullName( *Export.Object->GetFullName(), FNAME_Find );
				const FObjectThumbnail* ObjectThumbnail = nullptr;
				// If the FName does not exist, then we know it is not in the map and do not need to search
				if (!ObjectFullName.IsNone())
				{
					ObjectThumbnail = PackageThumbnailMap.Find(ObjectFullName);
				}
		
				// if we didn't find the object via full name, try again with ??? as the class name, to support having
				// loaded old packages without going through the editor (ie cooking old packages)
				if (ObjectThumbnail == nullptr)
				{
					// can't overwrite ObjectFullName, so that we add it properly to the map
					FName OldPackageStyleObjectFullName = FName(*FString::Printf(TEXT("??? %s"), *Export.Object->GetPathName()), FNAME_Find);
					if (!OldPackageStyleObjectFullName.IsNone())
					{
						ObjectThumbnail = PackageThumbnailMap.Find(OldPackageStyleObjectFullName);
					}
				}
				if( ObjectThumbnail != nullptr )
				{
					// IMPORTANT: We save all thumbnails here, even if they are a shared (empty) thumbnail!
					// Empty thumbnails let us know that an asset is in a package without having to
					// make a linker for it.
					ObjectsWithThumbnails.Add( FObjectFullNameAndThumbnail( ObjectFullName, ObjectThumbnail ) );
				}
			}
		}

		// preserve thumbnail rendered for the level
		const FObjectThumbnail* ObjectThumbnail = PackageThumbnailMap.Find(FName(*InOuter->GetFullName()));
		if (ObjectThumbnail != nullptr)
		{
			ObjectsWithThumbnails.Add( FObjectFullNameAndThumbnail(FName(*InOuter->GetFullName()), ObjectThumbnail ) );
		}
		
		// Do we have any thumbnails?  If so, we'll save them out along with a table of contents
		if( ObjectsWithThumbnails.Num() > 0 )
		{
			// Save out the image data for the thumbnails
			FStructuredArchive::FStream ThumbnailStream = Record.EnterStream(TEXT("Thumbnails"));

			for( int32 CurObjectIndex = 0; CurObjectIndex < ObjectsWithThumbnails.Num(); ++CurObjectIndex )
			{
				FObjectFullNameAndThumbnail& CurObjectThumb = ObjectsWithThumbnails[ CurObjectIndex ];

				// Store the file offset to this thumbnail
				CurObjectThumb.FileOffset = (int32)Linker->Tell();

				// Serialize the thumbnail!
				FObjectThumbnail* SerializableThumbnail = const_cast< FObjectThumbnail* >( CurObjectThumb.ObjectThumbnail );
				SerializableThumbnail->Serialize(ThumbnailStream.EnterElement());
			}


			// Store the thumbnail table of contents
			{
				Linker->Summary.ThumbnailTableOffset = (int32)Linker->Tell();

				// Save number of thumbnails
				int32 ThumbnailCount = ObjectsWithThumbnails.Num();
				FStructuredArchive::FArray IndexArray = Record.EnterField(TEXT("Index")).EnterArray(ThumbnailCount);

				// Store a list of object names along with the offset in the file where the thumbnail is stored
				for( int32 CurObjectIndex = 0; CurObjectIndex < ObjectsWithThumbnails.Num(); ++CurObjectIndex )
				{
					const FObjectFullNameAndThumbnail& CurObjectThumb = ObjectsWithThumbnails[ CurObjectIndex ];

					// Object name
					const FString ObjectFullName = CurObjectThumb.ObjectFullName.ToString();

					// Break the full name into it's class and path name parts
					const int32 FirstSpaceIndex = ObjectFullName.Find( TEXT( " " ) );
					check( FirstSpaceIndex != INDEX_NONE && FirstSpaceIndex > 0 );
					FString ObjectClassName = ObjectFullName.Left( FirstSpaceIndex );
					const FString ObjectPath = ObjectFullName.Mid( FirstSpaceIndex + 1 );

					// Remove the package name from the object path since that will be implicit based
					// on the package file name
					FString ObjectPathWithoutPackageName = ObjectPath.Mid( ObjectPath.Find( TEXT( "." ) ) + 1 );

					// File offset for the thumbnail (already saved out.)
					int32 FileOffset = CurObjectThumb.FileOffset;

					IndexArray.EnterElement().EnterRecord()
						<< SA_VALUE(TEXT("ObjectClassName"), ObjectClassName)
						<< SA_VALUE(TEXT("ObjectPathWithoutPackageName"), ObjectPathWithoutPackageName)
						<< SA_VALUE(TEXT("FileOffset"), FileOffset);
				}
			}
		}
	}

	// if content browser isn't enabled, clear the thumbnail map so we're not using additional memory for nothing
	if ( !GIsEditor || IsRunningCommandlet() )
	{
		InOuter->SetThumbnailMap(nullptr);
	}
#endif
}

ESavePackageResult AppendAdditionalData(FLinkerSave& Linker, int64& InOutDataStartOffset, FSavePackageContext* SavePackageContext)
{
	if (Linker.AdditionalDataToAppend.Num() == 0)
	{
		return ESavePackageResult::Success;
	}

	IPackageWriter* PackageWriter = SavePackageContext ? SavePackageContext->PackageWriter : nullptr;
	if (PackageWriter)
	{
		bool bDeclareRegionForEachAdditionalFile = SavePackageContext->PackageWriterCapabilities.bDeclareRegionForEachAdditionalFile;
		FFileRegionMemoryWriter DataArchive;
		for (FLinkerSave::AdditionalDataCallback& Callback : Linker.AdditionalDataToAppend)
		{
			if (bDeclareRegionForEachAdditionalFile)
			{
				DataArchive.PushFileRegionType(EFileRegionType::None);
			}
			Callback(Linker, DataArchive, InOutDataStartOffset + DataArchive.Tell());
			if (bDeclareRegionForEachAdditionalFile)
			{
				DataArchive.PopFileRegionType();
			}
		}
		IPackageWriter::FLinkerAdditionalDataInfo DataInfo{ Linker.LinkerRoot->GetFName() };
		int64 DataSize = DataArchive.TotalSize();
		FIoBuffer DataBuffer = FIoBuffer(FIoBuffer::AssumeOwnership, DataArchive.ReleaseOwnership(), DataSize);
		PackageWriter->WriteLinkerAdditionalData(DataInfo, DataBuffer, DataArchive.GetFileRegions());
		InOutDataStartOffset += DataSize;
	}
	else
	{
		int64 LinkerStart = Linker.Tell();
		FArchive& Ar = Linker;
		for (FLinkerSave::AdditionalDataCallback& Callback : Linker.AdditionalDataToAppend)
		{
			Callback(Linker, Linker, Linker.Tell());
		}
		InOutDataStartOffset += Linker.Tell() - LinkerStart;
	}

	Linker.AdditionalDataToAppend.Empty();

	// Note that we currently have no failure condition here, but we return a ESavePackageResult
	// in case one needs to be added in future code.
	return ESavePackageResult::Success;
}

ESavePackageResult CreatePayloadSidecarFile(FLinkerSave& Linker, const FPackagePath& PackagePath, const bool bSaveToMemory, FSavePackageOutputFileArray& AdditionalPackageFiles, FSavePackageContext* SavePackageContext)
{
	if (Linker.SidecarDataToAppend.IsEmpty())
	{
		return ESavePackageResult::Success;
	}

	// Note since we only allow sidecar file generation when saving a package and not when cooking 
	// we know that we don't need to generate the hash or check if we should write the file, since those 
	// operations are cooking only. However we still accept the parameters and check against them for 
	// safety in case someone tries to add support in the future.
	// We could add support but it is difficult to test and would be better left for a proper clean up pass
	// once we enable SavePackage2 only. 
	checkf(!Linker.IsCooking(), TEXT("Cannot write a sidecar file during cooking! (%s)"), *PackagePath.GetDebugName());
	IPackageWriter* PackageWriter = SavePackageContext ? SavePackageContext->PackageWriter : nullptr;

	UE::FPackageTrailerBuilder Builder;

	for (FLinkerSave::FSidecarStorageInfo& Info : Linker.SidecarDataToAppend)
	{
		Builder.AddPayload(Info.Identifier, Info.Payload, UE::Virtualization::EPayloadFilterReason::None);
	}

	Linker.SidecarDataToAppend.Empty();

	FLargeMemoryWriter Ar(0, true /* bIsPersistent */);
	if (!Builder.BuildAndAppendTrailer(nullptr, Ar))
	{
		UE_LOG(LogSavePackage, Error, TEXT("Failed to build sidecar package trailer for '%s'"), *PackagePath.GetDebugName());
		return ESavePackageResult::Error;
	}

	const int64 DataSize = Ar.TotalSize();
	checkf(DataSize > 0, TEXT("Sidecar archive should not be empty! (%s)"), *PackagePath.GetDebugName());

	FString TargetFilePath = PackagePath.GetLocalFullPath(EPackageSegment::PayloadSidecar);

	if (PackageWriter)
	{
		IPackageWriter::FAdditionalFileInfo SidecarSegmentInfo;
		SidecarSegmentInfo.PackageName = PackagePath.GetPackageFName();
		SidecarSegmentInfo.Filename = MoveTemp(TargetFilePath);
		FIoBuffer FileData(FIoBuffer::AssumeOwnership, Ar.ReleaseOwnership(), DataSize);
		PackageWriter->WriteAdditionalFile(SidecarSegmentInfo, FileData);
	}
	else if (bSaveToMemory)
	{
		AdditionalPackageFiles.Emplace(MoveTemp(TargetFilePath), FLargeMemoryPtr(Ar.ReleaseOwnership()), TArray<FFileRegion>(), DataSize);
	}
	else
	{
		const FString BaseFilename = FPaths::GetBaseFilename(TargetFilePath);
		FString TempFilePath = FPaths::CreateTempFilename(*FPaths::ProjectSavedDir(), *BaseFilename.Left(32));
			
		SavePackageUtilities::WriteToFile(TempFilePath, Ar.GetData(), DataSize); // TODO: Check the error handling here!
		UE_LOG(LogSavePackage, Verbose, TEXT("Saved '%s' as temp file '%s'"), *TargetFilePath, *TempFilePath);

		AdditionalPackageFiles.Emplace(MoveTemp(TargetFilePath), MoveTemp(TempFilePath), DataSize);		
	}

	return ESavePackageResult::Success;
}

void SaveWorldLevelInfo(UPackage* InOuter, FLinkerSave* Linker, FStructuredArchive::FRecord Record)
{
	Linker->Summary.WorldTileInfoDataOffset = 0;
	
	if(FWorldTileInfo* WorldTileInfo = InOuter->GetWorldTileInfo())
	{
		Linker->Summary.WorldTileInfoDataOffset = (int32)Linker->Tell();
		Record << SA_VALUE(TEXT("WorldLevelInfo"), *WorldTileInfo);
	}
}

} // end namespace UE::SavePackageUtilities

void UPackage::WaitForAsyncFileWrites()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPackage::WaitForAsyncFileWrites);

	while (OutstandingAsyncWrites.GetValue())
	{
		FPlatformProcess::Sleep(0.0f);
	}
}

bool UPackage::HasAsyncFileWrites()
{
	return OutstandingAsyncWrites.GetValue() > 0;
}

bool UPackage::IsEmptyPackage(UPackage* Package, const UObject* LastReferencer)
{
	// Don't count null or volatile packages as empty, just let them be NULL or get GCed
	if ( Package != nullptr )
	{
		// Make sure the package is fully loaded before determining if it is empty
		if( !Package->IsFullyLoaded() )
		{
			Package->FullyLoad();
		}

		bool bIsEmpty = true;
		ForEachObjectWithPackage(Package, [LastReferencer, &bIsEmpty](UObject* InObject)
		{
			// if the package contains at least one object that has asset registry data and isn't the `LastReferencer` consider it not empty
			if (InObject->IsAsset() && InObject != LastReferencer)
			{
				bIsEmpty = false;
				// we can break out of the iteration as soon as we find one valid object
				return false;
			}
			return true;
		// Don't consider transient, class default or garbage objects
		}, false, RF_Transient | RF_ClassDefaultObject, EInternalObjectFlags::Garbage);
		return bIsEmpty;
	}

	// Invalid package
	return false;
}


namespace UE::AssetRegistry
{

void WritePackageData(FStructuredArchiveRecord& ParentRecord, bool bIsCooking, const UPackage* Package,
	FLinkerSave* Linker, const TSet<TObjectPtr<UObject>>& ImportsUsedInGame, const TSet<FName>& SoftPackagesUsedInGame,
	const ITargetPlatform* TargetPlatform, TArray<FAssetData>* OutAssetDatas)
{
	if (TargetPlatform)
	{
		FArchiveCookContext CookContext(const_cast<UPackage*>(Package), UE::Cook::ECookType::Unknown,
			UE::Cook::ECookingDLC::Unknown, TargetPlatform);
		WritePackageData(ParentRecord, &CookContext, Package, Linker, ImportsUsedInGame, SoftPackagesUsedInGame,
			OutAssetDatas, true /* bProceduralSave */);
	}
	else
	{
		WritePackageData(ParentRecord, nullptr, Package, Linker, ImportsUsedInGame, SoftPackagesUsedInGame,
			OutAssetDatas, false/* bProceduralSave */);
	}
}

// See the corresponding ReadPackageDataMain and ReadPackageDataDependencies defined in PackageReader.cpp in AssetRegistry module
void WritePackageData(FStructuredArchiveRecord& ParentRecord, FArchiveCookContext* CookContext, const UPackage* Package,
	FLinkerSave* Linker, const TSet<TObjectPtr<UObject>>& ImportsUsedInGame, const TSet<FName>& SoftPackagesUsedInGame,
	TArray<FAssetData>* OutAssetDatas, bool bProceduralSave)
{
	bProceduralSave = bProceduralSave | (CookContext != nullptr);
	IAssetRegistryInterface* AssetRegistry = IAssetRegistryInterface::GetPtr();

	// To avoid large patch sizes, we have frozen cooked package format at the format before VER_UE4_ASSETREGISTRY_DEPENDENCYFLAGS
	// Non-cooked saves do a full update. Orthogonally, they also store the assets in the package in addition to the output variable.
	// Cooked saves do an additive update and do not store the assets in the package.
	bool bPreDependencyFormat = false;
	bool bWriteAssetsToPackage = true;
	// Editor saves do a full update, but procedural saves (including cook) do not
	bool bFullUpdate = !bProceduralSave;
	FCookTagList* CookTagList = nullptr;
	if (CookContext)
	{
		bPreDependencyFormat = true;
		bWriteAssetsToPackage = false;
		CookTagList = CookContext->GetCookTagList();
	}	

	// WritePackageData is currently only called if not bTextFormat; we rely on that to save offsets
	FArchive& BinaryArchive = ParentRecord.GetUnderlyingArchive();
	check(!BinaryArchive.IsTextFormat());

	// Store the asset registry offset in the file and enter a record for the asset registry data
	Linker->Summary.AssetRegistryDataOffset = (int32)BinaryArchive.Tell();
	FStructuredArchiveRecord AssetRegistryRecord = ParentRecord.EnterField(TEXT("AssetRegistry")).EnterRecord();

	// Offset to Dependencies
	int64 OffsetToAssetRegistryDependencyDataOffset = INDEX_NONE;
	if (!bPreDependencyFormat)
	{
		// Write placeholder data for the offset to the separately-serialized AssetRegistryDependencyData
		OffsetToAssetRegistryDependencyDataOffset = BinaryArchive.Tell();
		int64 AssetRegistryDependencyDataOffset = 0;
		AssetRegistryRecord << SA_VALUE(TEXT("AssetRegistryDependencyDataOffset"), AssetRegistryDependencyDataOffset);
		check(BinaryArchive.Tell() == OffsetToAssetRegistryDependencyDataOffset + sizeof(AssetRegistryDependencyDataOffset));
	}

	TArray<UObject*> AssetObjects;
	for (int32 i = 0; i < Linker->ExportMap.Num(); i++)
	{
		FObjectExport& Export = Linker->ExportMap[i];
		if (Export.Object && Export.Object->IsAsset())
		{
#if WITH_EDITOR
			if (CookContext)
			{
				TArray<UObject*> AdditionalObjects;
				Export.Object->GetAdditionalAssetDataObjectsForCook(*CookContext, AdditionalObjects);
				for (UObject* Object : AdditionalObjects)
				{
					if (Object->IsAsset())
					{
						AssetObjects.Add(Object);
					}
				}
			}
#endif
			AssetObjects.Add(Export.Object);
		}
	}

	int32 ObjectCountInPackage = bWriteAssetsToPackage ? AssetObjects.Num() : 0;
	FStructuredArchive::FArray AssetArray = AssetRegistryRecord.EnterArray(TEXT("TagMap"), ObjectCountInPackage);
	FString PackageName = Package->GetName();

	for (int32 ObjectIdx = 0; ObjectIdx < AssetObjects.Num(); ++ObjectIdx)
	{
		const UObject* Object = AssetObjects[ObjectIdx];

		// Exclude the package name in the object path, we just need to know the path relative to the package we are saving
		FString ObjectPath = Object->GetPathName(Package);
		FString ObjectClassName = Object->GetClass()->GetPathName();

		FAssetRegistryTagsContextData TagsContextData(Object, EAssetRegistryTagsCaller::SavePackage);
		TagsContextData.bProceduralSave = bProceduralSave;
		TagsContextData.TargetPlatform = nullptr;
		if (CookContext)
		{
			TagsContextData.TargetPlatform = CookContext->GetTargetPlatform();
			TagsContextData.CookType = CookContext->GetCookType();
			TagsContextData.CookingDLC = CookContext->GetCookingDLC();
			TagsContextData.bWantsCookTags = TagsContextData.CookType == UE::Cook::ECookType::ByTheBook;
		}
		TagsContextData.bFullUpdateRequested = bFullUpdate;
		FAssetRegistryTagsContext TagsContext(TagsContextData);

		if (AssetRegistry && !TagsContextData.bFullUpdateRequested)
		{
			FAssetData ExistingAssetData;
			if (AssetRegistry->TryGetAssetByObjectPath(FSoftObjectPath(Object), ExistingAssetData)
				== UE::AssetRegistry::EExists::Exists)
			{
				TagsContextData.Tags.Reserve(ExistingAssetData.TagsAndValues.Num());
				ExistingAssetData.TagsAndValues.ForEach(
					[&TagsContextData](const TPair<FName, FAssetTagValueRef>& Pair)
					{
						TagsContextData.Tags.Add(Pair.Key, UObject::FAssetRegistryTag(Pair.Key, Pair.Value.GetStorageString(),
							UObject::FAssetRegistryTag::TT_Alphabetical));
					});
			}
		}
		if (CookTagList)
		{
			TArray<FCookTagList::FTagNameValuePair>* CookTags = CookTagList->ObjectToTags.Find(Object);
			if (CookTags)
			{
				for (FCookTagList::FTagNameValuePair& Pair : *CookTags)
				{
					TagsContext.AddCookTag(UObject::FAssetRegistryTag(Pair.Key, Pair.Value,
						UObject::FAssetRegistryTag::TT_Alphabetical));
				}
			}
		}
		
		Object->GetAssetRegistryTags(TagsContext);

		int32 TagCount = TagsContextData.Tags.Num();
		TagsContextData.Tags.KeySort(FNameLexicalLess());

		if (bWriteAssetsToPackage)
		{
			FStructuredArchive::FRecord AssetRecord = AssetArray.EnterElement().EnterRecord();
			AssetRecord << SA_VALUE(TEXT("Path"), ObjectPath) << SA_VALUE(TEXT("Class"), ObjectClassName);

			FStructuredArchive::FMap TagMap = AssetRecord.EnterField(TEXT("Tags")).EnterMap(TagCount);

			for (const TPair<FName, UObject::FAssetRegistryTag>& TagPair : TagsContextData.Tags)
			{
				const UObject::FAssetRegistryTag& Tag = TagPair.Value;
				FString Key = Tag.Name.ToString();
				FString Value = Tag.Value;

				TagMap.EnterElement(Key) << Value;
			}
		}

		if (OutAssetDatas)
		{
			FAssetDataTagMap TagsAndValues;
			for (TPair<FName, UObject::FAssetRegistryTag>& TagPair : TagsContextData.Tags)
			{
				UObject::FAssetRegistryTag& Tag = TagPair.Value;
				if (!Tag.Name.IsNone() && !Tag.Value.IsEmpty())
				{
					TagsAndValues.Add(Tag.Name, MoveTemp(Tag.Value));
				}
			}
			// if we do not have a full object path already, build it
			const bool bFullObjectPath = ObjectPath.StartsWith(TEXT("/"), ESearchCase::CaseSensitive);
			if (!bFullObjectPath)
			{
				// if we do not have a full object path, ensure that we have a top level object for the package and not a sub object
				if (!ensureMsgf(!ObjectPath.Contains(TEXT("."), ESearchCase::CaseSensitive),
					TEXT("Cannot make FAssetData for sub object %s in package %s!"), *ObjectPath, *PackageName))
				{
					continue;
				}
				ObjectPath = PackageName + TEXT(".") + ObjectPath;
			}

			OutAssetDatas->Emplace(PackageName, ObjectPath, FTopLevelAssetPath(ObjectClassName),
				MoveTemp(TagsAndValues), Package->GetChunkIDs(), Package->GetPackageFlags());
		}
	}
	if (bPreDependencyFormat)
	{
		// The legacy format did not write the other sections, or the offsets to those other sections
		return;
	}

	// Overwrite the placeholder offset for the AssetRegistryDependencyData and enter a record for the asset registry dependency data
	{
		int64 AssetRegistryDependencyDataOffset = Linker->Tell();
		BinaryArchive.Seek(OffsetToAssetRegistryDependencyDataOffset);
		BinaryArchive << AssetRegistryDependencyDataOffset;
		BinaryArchive.Seek(AssetRegistryDependencyDataOffset);
	}
	FStructuredArchiveRecord DependencyDataRecord = ParentRecord.EnterField(TEXT("AssetRegistryDependencyData")).EnterRecord();

	// Convert the IsUsedInGame sets into a bitarray with a value per import/softpackagereference
	TBitArray<> ImportUsedInGameBits;
	TBitArray<> SoftPackageUsedInGameBits;
	ImportUsedInGameBits.Reserve(Linker->ImportMap.Num());
	for (int32 ImportIndex = 0; ImportIndex < Linker->ImportMap.Num(); ++ImportIndex)
	{
		ImportUsedInGameBits.Add(ImportsUsedInGame.Contains(Linker->ImportMap[ImportIndex].XObject));
	}
	SoftPackageUsedInGameBits.Reserve(Linker->SoftPackageReferenceList.Num());
	for (int32 SoftPackageIndex = 0; SoftPackageIndex < Linker->SoftPackageReferenceList.Num(); ++SoftPackageIndex)
	{
		SoftPackageUsedInGameBits.Add(SoftPackagesUsedInGame.Contains(Linker->SoftPackageReferenceList[SoftPackageIndex]));
	}

	// Serialize the Dependency section
	DependencyDataRecord << SA_VALUE(TEXT("ImportUsedInGame"), ImportUsedInGameBits);
	DependencyDataRecord << SA_VALUE(TEXT("SoftPackageUsedInGame"), SoftPackageUsedInGameBits);
}

}
