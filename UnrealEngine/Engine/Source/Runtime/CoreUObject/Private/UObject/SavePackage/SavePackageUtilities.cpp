// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/SavePackage/SavePackageUtilities.h"

#include "Algo/Sort.h"
#include "Algo/Unique.h"
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
#include "Serialization/LargeMemoryWriter.h"
#include "Serialization/PackageWriter.h"
#include "Tasks/Task.h"
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

#if WITH_EDITORONLY_DATA

void FArchiveObjectCrc32NonEditorProperties::Serialize(void* Data, int64 Length)
{
	int32 NewEditorOnlyProp = EditorOnlyProp + this->IsEditorOnlyPropertyOnTheStack();
	TGuardValue<int32> Guard(EditorOnlyProp, NewEditorOnlyProp);
	if (NewEditorOnlyProp == 0)
	{
		Super::Serialize(Data, Length);
	}
}

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

namespace SavePackageUtilities
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
				UE_LOG(LogSavePackage, Error, TEXT("Failed to delete newly added file '%s' when trying to restore the package state and the package could be unstable, please revert in source control!"), *Entry);
			}
		}

		// Now we can move back the original files
		for (const TPair<FString, FString>& Entry : MovedOriginalFiles)
		{
			if (!FileSystem.Move(*Entry.Key, *Entry.Value))
			{
				UE_LOG(LogSavePackage, Error, TEXT("Failed to restore package '%s', the file '%s' is in an incorrect state and the package could be unstable, please revert in source control!"), *PackagePath.GetDebugName(), *Entry.Key);
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
	
bool IsStrippedEditorOnlyObject(const UObject* InObject, bool bCheckRecursive, bool bCheckMarks)
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
	
	return IsEditorOnlyObject(InObject, bCheckRecursive, bCheckMarks);
#else
	return true;
#endif
}
	
} // end namespace SavePackageUtilities

namespace UE::SavePackageUtilities
{

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

bool IsEditorOnlyObject(const UObject* InObject, bool bCheckRecursive, bool bCheckMarks)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("IsEditorOnlyObject"), STAT_IsEditorOnlyObject, STATGROUP_LoadTime);
	check(InObject);

	if ((bCheckMarks && InObject->HasAnyMarks(OBJECTMARK_EditorOnly)) || InObject->IsEditorOnly())
	{
		return true;
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
			if (IsEditorOnlyObject(Outer, true, bCheckMarks))
			{
				return true;
			}
		}
		const UStruct* InStruct = Cast<UStruct>(InObject);
		if (InStruct)
		{
			const UStruct* SuperStruct = InStruct->GetSuperStruct();
			if (SuperStruct && IsEditorOnlyObject(SuperStruct, true, bCheckMarks))
			{
				return true;
			}
		}
		else
		{
			if (IsEditorOnlyObject(InObject->GetClass(), true, bCheckMarks))
			{
				return true;
			}

			UObject* Archetype = InObject->GetArchetype();
			if (Archetype && IsEditorOnlyObject(Archetype, true, bCheckMarks))
			{
				return true;
			}
		}
	}
	return false;
}

bool FObjectImportSortHelper::operator()(const FObjectImport& A, const FObjectImport& B) const
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
}

void FObjectImportSortHelper::SortImports(FLinkerSave* Linker)
{
	TArray<FObjectImport>& Imports = Linker->ImportMap;
	ObjectToFullNameMap.Reserve(Linker->ImportMap.Num());
	for (int32 ImportIndex = 0; ImportIndex < Linker->ImportMap.Num(); ImportIndex++)
	{
		const FObjectImport& Import = Linker->ImportMap[ImportIndex];
		if (Import.XObject)
		{
			ObjectToFullNameMap.Add(Import.XObject, Import.XObject->GetFullName());
		}
	}

	if (Linker->ImportMap.Num())
	{
		Sort(&Linker->ImportMap[0], Linker->ImportMap.Num(), *this);
	}
}

bool FObjectExportSortHelper::operator()(const FObjectExport& A, const FObjectExport& B) const
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
}

void FObjectExportSortHelper::SortExports(FLinkerSave* Linker)
{
	ObjectToFullNameMap.Reserve(Linker->ExportMap.Num());

	for ( int32 ExportIndex = 0; ExportIndex < Linker->ExportMap.Num(); ExportIndex++ )
	{
		const FObjectExport& Export = Linker->ExportMap[ExportIndex];
		if (Export.Object)
		{
			ObjectToFullNameMap.Add(Export.Object, Export.Object->GetFullName());
		}
	}

	if (Linker->ExportMap.Num())
	{
		Sort(&Linker->ExportMap[0], Linker->ExportMap.Num(), *this);
	}
}

FEDLCookChecker::FEDLNodeHash::FEDLNodeHash()
{
}

FEDLCookChecker::FEDLNodeHash::FEDLNodeHash(const TArray<FEDLNodeData>* InNodes, FEDLNodeID InNodeID, EObjectEvent InObjectEvent)
	: Nodes(InNodes)
	, NodeID(InNodeID)
	, bIsNode(true)
	, ObjectEvent(InObjectEvent)
{
}

FEDLCookChecker::FEDLNodeHash::FEDLNodeHash(const UObject* InObject, EObjectEvent InObjectEvent)
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
	const UObject* LocalObject;
	const UObject* OtherObject;
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

uint32 GetTypeHash(const FEDLCookChecker::FEDLNodeHash& A)
{
	uint32 Hash = 0;

	uint32 LocalNodeID;
	const UObject* LocalObject;
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
		return Object->GetFName();
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
		UObject* ParentObject = Object->GetOuter();
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

FName FEDLCookChecker::FEDLNodeHash::ObjectNameFirst(const FEDLNodeHash& InNode, uint32& OutNodeID, const UObject*& OutObject)
{
	if (InNode.bIsNode)
	{
		OutNodeID = InNode.NodeID;
		return (*InNode.Nodes)[OutNodeID].Name;
	}
	else
	{
		OutObject = InNode.Object;
		return OutObject->GetFName();
	}
}

FName FEDLCookChecker::FEDLNodeHash::ObjectNameNext(const FEDLNodeHash& InNode, uint32& OutNodeID, const UObject*& OutObject)
{
	if (InNode.bIsNode)
	{
		OutNodeID = (*InNode.Nodes)[OutNodeID].ParentID;
		return OutNodeID != NodeIDInvalid ? (*InNode.Nodes)[OutNodeID].Name : NAME_None;
	}
	else
	{
		OutObject = OutObject->GetOuter();
		return OutObject ? OutObject->GetFName() : NAME_None;
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
	ImportingPackagesSorted.SetNum(Algo::Unique(ImportingPackagesSorted), true /* bAllowShrinking */);
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

	Nodes.Empty();
	NodeHashToNodeID.Empty();
	NodePrereqs.Empty();
	bIsActive = false;
}

LLM_DEFINE_TAG(EDLCookChecker);

void FEDLCookChecker::AddImport(UObject* Import, UPackage* ImportingPackage)
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

void FEDLCookChecker::Verify(bool bFullReferencesExpected)
{
	check(!GIsSavingPackage);
	FEDLCookChecker Accumulator = AccumulateAndClear();

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
				for (FName PackageName : NodeData.ImportingPackagesSorted)
				{
					UE_LOG(LogSavePackage, Warning, TEXT("%s imported %s, but it was never saved as an export."), *PackageName.ToString(), *NodeData.ToString(Accumulator));
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
	Nodes.Reserve(NodesField.AsArrayView().Num() / 5);
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
	NodePrereqs.Reserve(PrereqsField.AsArrayView().Num() / 2);
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
	PackagesWithUnknownExports.Reserve(PackagesWithUnknownExportsField.AsArrayView().Num());
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
	FEDLCookChecker::StartSavingEDLCookInfoForVerification();
}

void VerifyEDLCookInfo(bool bFullReferencesExpected)
{
	FEDLCookChecker::Verify(bFullReferencesExpected);
}

void EDLCookInfoAddIterativelySkippedPackage(FName LongPackageName)
{
	FEDLCookCheckerThreadState::Get().AddPackageWithUnknownExports(LongPackageName);
}

void EDLCookInfoMoveToCompactBinaryAndClear(FCbWriter& Writer, bool& bOutHasData)
{
	FEDLCookChecker::MoveToCompactBinaryAndClear(Writer, bOutHasData);
}

bool EDLCookInfoAppendFromCompactBinary(FCbFieldView Field)
{
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
	: bCanSkipEditorReferencedPackagesWhenCooking(true)
{
	GConfig->GetBool(TEXT("Core.System"), TEXT("CanSkipEditorReferencedPackagesWhenCooking"), bCanSkipEditorReferencedPackagesWhenCooking, GEngineIni);
}


namespace SavePackageUtilities
{
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

class FLargeMemoryWriterWithRegions : public FLargeMemoryWriter
{
public:
	FLargeMemoryWriterWithRegions()
		: FLargeMemoryWriter(0, /* IsPersistent */ true)
	{}

	TArray<FFileRegion> FileRegions;
};

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
		FLargeMemoryWriterWithRegions DataArchive;
		for (FLinkerSave::AdditionalDataCallback& Callback : Linker.AdditionalDataToAppend)
		{
			int64 RegionStart = DataArchive.Tell();
			Callback(Linker, DataArchive, InOutDataStartOffset + RegionStart);
			int64 RegionEnd = DataArchive.Tell();
			if (RegionEnd != RegionStart && bDeclareRegionForEachAdditionalFile)
			{
				DataArchive.FileRegions.Add(FFileRegion(RegionStart, RegionEnd - RegionStart, EFileRegionType::None));
			}
		}
		IPackageWriter::FLinkerAdditionalDataInfo DataInfo{ Linker.LinkerRoot->GetFName() };
		int64 DataSize = DataArchive.TotalSize();
		FIoBuffer DataBuffer = FIoBuffer(FIoBuffer::AssumeOwnership, DataArchive.ReleaseOwnership(), DataSize);
		PackageWriter->WriteLinkerAdditionalData(DataInfo, DataBuffer, DataArchive.FileRegions);
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

	FLargeMemoryWriter Ar(0, true /* bIsPersistent */);

	uint32 VersionNumber = UE::Serialization::FTocEntry::PayloadSidecarFileVersion;
	Ar << VersionNumber;

	int64 TocPosition = Ar.Tell();

	TArray<UE::Serialization::FTocEntry> TableOfContents;
	TableOfContents.SetNum(Linker.SidecarDataToAppend.Num());

	// First we write an empty table of contents to the file to reserve the space
	Ar << TableOfContents;

	int32 Index = 0;
	for (FLinkerSave::FSidecarStorageInfo& Info : Linker.SidecarDataToAppend)
	{
		// Fill out the entry to the table of contents
		TableOfContents[Index].Identifier = Info.Identifier;
		TableOfContents[Index].OffsetInFile = Ar.Tell();
		TableOfContents[Index].UncompressedSize = Info.Payload.GetRawSize();

		Index++;

		// Now write the payload to the archive
		for (const FSharedBuffer& Buffer : Info.Payload.GetCompressed().GetSegments())
		{
			// Const cast because FArchive requires a non-const pointer!
			Ar.Serialize(const_cast<void*>(Buffer.GetData()), static_cast<int64>(Buffer.GetSize()));
		}

		// Reset each payload reference after it has been written to the archive, this could
		// potentially release memory and keep our high water mark down.
		Info.Payload.Reset();
	}

	// Now write out the table of contents again but with valid data
	int64 EndPos = Ar.Tell();
	Ar.Seek(TocPosition);
	Ar << TableOfContents;
	Ar.Seek(EndPos); 

	const int64 DataSize = Ar.TotalSize();
	checkf(DataSize > 0, TEXT("The archive should not be empty at this point!"));

	FString TargetFilePath = PackagePath.GetLocalFullPath(EPackageSegment::PayloadSidecar);

	if (PackageWriter)
	{
		IPackageWriter::FAdditionalFileInfo SidecarSegmentInfo;
		SidecarSegmentInfo.PackageName = PackagePath.GetPackageFName();
		SidecarSegmentInfo.Filename = TargetFilePath;
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

	Linker.SidecarDataToAppend.Empty();

	return ESavePackageResult::Success;
}

ESavePackageResult SaveBulkData(FLinkerSave* Linker, int64& InOutStartOffset, const UPackage* InOuter, const TCHAR* Filename, const ITargetPlatform* TargetPlatform,
				  FSavePackageContext* SavePackageContext, uint32 SaveFlags, const bool bTextFormat, 
				  int64& TotalPackageSizeUncompressed, bool bIsOptionalRealm)
{
	// Now we write all the bulkdata that is supposed to be at the end of the package
	// and fix up the offset
	IPackageWriter* PackageWriter = SavePackageContext != nullptr ? SavePackageContext->PackageWriter : nullptr;
	Linker->Summary.BulkDataStartOffset = InOutStartOffset;

	if (Linker->BulkDataToAppend.Num() == 0)
	{
		return ESavePackageResult::Success;;
	}
	check(!bTextFormat);

	COOK_STAT(FScopedDurationTimer SaveTimer(FSavePackageStats::SerializeBulkDataTimeSec));

	FScopedSlowTask BulkDataFeedback((float)Linker->BulkDataToAppend.Num());

	TUniquePtr<FLargeMemoryWriterWithRegions> BulkArchive;
	TUniquePtr<FLargeMemoryWriterWithRegions> OptionalBulkArchive;
	TUniquePtr<FLargeMemoryWriterWithRegions> MappedBulkArchive;

	const bool bSeparateSegmentsEnabled = Linker->IsCooking();

	int64 LinkerStart = 0;
	if (PackageWriter || bSeparateSegmentsEnabled)
	{
		BulkArchive.Reset(new FLargeMemoryWriterWithRegions);
		if (bSeparateSegmentsEnabled)
		{
			OptionalBulkArchive.Reset(new FLargeMemoryWriterWithRegions);
			MappedBulkArchive.Reset(new FLargeMemoryWriterWithRegions);
		}
	}
	else
	{
		LinkerStart = Linker->Tell();
	}
	bool bRequestSaveByReference = SaveFlags & SAVE_BulkDataByReference;

	bool bAlignBulkData = false;
	bool bDeclareSpecialRegions = false;
	bool bDeclareRegionForEachAdditionalFile = false;
	int64 BulkDataAlignment = 0;

	if (TargetPlatform)
	{
		bAlignBulkData = TargetPlatform->SupportsFeature(ETargetPlatformFeatures::MemoryMappedFiles);
		bDeclareSpecialRegions = TargetPlatform->SupportsFeature(ETargetPlatformFeatures::CookFileRegionMetadata);
		BulkDataAlignment = TargetPlatform->GetMemoryMappingAlignment();
	}
	if (PackageWriter)
	{
		bDeclareRegionForEachAdditionalFile = SavePackageContext->PackageWriterCapabilities.bDeclareRegionForEachAdditionalFile;
	}

	if (bRequestSaveByReference)
	{
		const TCHAR* FailureReason = nullptr;
		if (Linker->bUpdatingLoadedPath)
		{
			FailureReason = TEXT("SAVE_BulkDataByReference is incompatible with bUpdatingLoadedPath");
		}
		if (FailureReason)
		{
			UE_LOG(LogSavePackage, Error, TEXT("SaveBulkData failed for %s: %s."), Filename, FailureReason);
			return ESavePackageResult::Error;
		}
	}

	for (FLinkerSave::FBulkDataStorageInfo& BulkDataStorageInfo : Linker->BulkDataToAppend)
	{
		BulkDataFeedback.EnterProgressFrame();

		// Set bulk data flags to what they were during initial serialization (they might have changed after that)
		uint32 BulkDataFlags = BulkDataStorageInfo.BulkDataFlags;
		FBulkData* BulkData = BulkDataStorageInfo.BulkData;
		checkf(BulkDataFlags& BULKDATA_PayloadAtEndOfFile, TEXT("Inlined BulkData data should not have been added to BulkDataToAppend"));

		const bool bBulkItemIsOptional = (BulkDataFlags & BULKDATA_OptionalPayload) != 0;
		bool bBulkItemIsMapped = bAlignBulkData && ((BulkDataFlags & BULKDATA_MemoryMappedPayload) != 0);

		if (bBulkItemIsMapped && bBulkItemIsOptional)
		{
			UE_LOG(LogSavePackage, Warning, TEXT("%s has bulk data that is both mapped and optional. This is not currently supported. Will not be mapped."), Filename);
			BulkDataFlags &= ~BULKDATA_MemoryMappedPayload;
			bBulkItemIsMapped = false;
		}
		if (bBulkItemIsMapped && bRequestSaveByReference)
		{
			UE_LOG(LogSavePackage, Warning, TEXT("%s has bulk data that is mapped, but the save method is SAVE_BulkDataByReference."
				"This is not currently supported. Will not be mapped."), Filename);
			BulkDataFlags &= ~BULKDATA_MemoryMappedPayload;
			bBulkItemIsMapped = false;
		}

		enum ESaveLocation
		{
			Invalid,
			EndOfPackage,
			SeparateSegment,
			Reference,
			SeparateArchiveAtEndOfPackage,
		};
		ESaveLocation SaveLocation = ESaveLocation::Invalid;
		auto CanSaveByReference = [](FBulkData* BulkData)
		{
			return BulkData->GetBulkDataOffsetInFile() != INDEX_NONE &&
				// We don't support yet loading from a separate file
				!BulkData->IsInSeparateFile() &&
				// It is possible to have a BulkData marked as optional without putting it into a separate file, and we
				// assume that if BulkData is optional and in a separate file, then it is in the BulkDataOptional
				// segment. Rather than changing that assumption to support optional ExternalResource bulkdata, we
				// instead require that optional inlined/endofpackagedata BulkDatas can not be read from an
				// ExternalResource and must remain inline.
				!BulkData->IsOptional() &&
				// Inline or end-of-package-file data can only be loaded from the workspace domain package file if the
				// archive used by the bulk data was actually from the package file; BULKDATA_LazyLoadable is set by
				// Serialize iff that is the case										
				(BulkData->GetBulkDataFlags() & BULKDATA_LazyLoadable);
		};
		if (bRequestSaveByReference && CanSaveByReference(BulkData))
		{
			SaveLocation = ESaveLocation::Reference;
		}
		else if (bSeparateSegmentsEnabled)
		{
			SaveLocation = ESaveLocation::SeparateSegment;
		}
		else if (PackageWriter)
		{
			SaveLocation = ESaveLocation::SeparateArchiveAtEndOfPackage;
		}
		else
		{
			SaveLocation = ESaveLocation::EndOfPackage;
		}

		int64 BulkDataOffsetInFile;
		int64 BulkDataSizeOnDisk;
		if (SaveLocation == ESaveLocation::Reference)
		{
			BulkDataFlags |= BULKDATA_PayloadInSeperateFile | BULKDATA_WorkspaceDomainPayload;
			BulkDataFlags |= BULKDATA_NoOffsetFixUp; // We don't use legacy offset fixups when referencing
			BulkDataFlags &= ~BULKDATA_MemoryMappedPayload; // We don't support memory mapping from referenced

			BulkDataOffsetInFile = BulkData->GetBulkDataOffsetInFile();
			BulkDataSizeOnDisk = BulkData->GetBulkDataSizeOnDisk();
		}
		else
		{
			TArray<FFileRegion>* TargetRegions = &Linker->FileRegions;
			FArchive* TargetArchive = Linker;
			if (SaveLocation == ESaveLocation::SeparateSegment)
			{
				BulkDataFlags |= BULKDATA_PayloadInSeperateFile;
				// OffsetFixup is not useful when bulkdata is in separate segments.
				// It is also not supported if we are using IoStore, which always uses SeparateSegments, because 
				// the Linker's Summary.BulkDataStartOffset information is not available when loading with AsyncLoader2.
				BulkDataFlags |= BULKDATA_NoOffsetFixUp;

				if (bBulkItemIsOptional)
				{
					TargetArchive = OptionalBulkArchive.Get();
					TargetRegions = &OptionalBulkArchive->FileRegions;
				}
				else if (bBulkItemIsMapped)
				{
					TargetArchive = MappedBulkArchive.Get();
					TargetRegions = &MappedBulkArchive->FileRegions;
				}
				else
				{
					TargetArchive = BulkArchive.Get();
					TargetRegions = &BulkArchive->FileRegions;
				}
			}
			else if (SaveLocation == ESaveLocation::SeparateArchiveAtEndOfPackage)
			{
				TargetArchive = BulkArchive.Get();
				TargetRegions = &BulkArchive->FileRegions;
			}

			check(TargetArchive && TargetRegions);

			// Pad archive for proper alignment for memory mapping
			if (bBulkItemIsMapped && BulkDataAlignment > 0)
			{
				const int64 BulkStartOffset = TargetArchive->Tell();

				if (!IsAligned(BulkStartOffset, BulkDataAlignment))
				{
					const int64 AlignedOffset = Align(BulkStartOffset, BulkDataAlignment);

					int64 Padding = AlignedOffset - BulkStartOffset;
					check(Padding > 0);

					uint64 Zero64 = 0;
					while (Padding >= 8)
					{
						*TargetArchive << Zero64;
						Padding -= 8;
					}

					uint8 Zero8 = 0;
					while (Padding > 0)
					{
						*TargetArchive << Zero8;
						Padding--;
					}

					check(TargetArchive->Tell() == AlignedOffset);
				}
			}

			const int64 BulkStartOffset = TargetArchive->Tell();
			BulkData->SerializeBulkData(*TargetArchive, BulkData->Lock(LOCK_READ_ONLY), static_cast<EBulkDataFlags>(BulkDataFlags));
			BulkData->Unlock();
			const int64 BulkEndOffset = TargetArchive->Tell();

			BulkDataOffsetInFile = BulkStartOffset;
			if (SaveLocation == ESaveLocation::SeparateArchiveAtEndOfPackage)
			{
				// BulkDatas are written in a separate archive that is appended onto the exports archive;
				// the input BulkStartOffset is relative to the beginning of this separate archive.
				// But records of the BulkData's offset needs to instead be relative to the beginning of the exports archive.
				// We have to do this to distinguish them from inline bulkdatas in the exports segment
				BulkDataOffsetInFile += Linker->Summary.BulkDataStartOffset;
			}
			if ((BulkDataFlags & BULKDATA_NoOffsetFixUp) == 0)
			{
				// The runtime will add in the Summary.BulkDataStartOffset, so we subtract it here.
				// This allows the decoupling of the values written into the package from the size of the exports section.
				BulkDataOffsetInFile -= Linker->Summary.BulkDataStartOffset;
			}
			BulkDataSizeOnDisk = BulkEndOffset - BulkStartOffset;

			bool bDeclareBecauseSpecial = bDeclareSpecialRegions && BulkDataStorageInfo.BulkDataFileRegionType != EFileRegionType::None;
			if ((bDeclareRegionForEachAdditionalFile || bDeclareBecauseSpecial) && BulkDataSizeOnDisk > 0)
			{
				TargetRegions->Add(FFileRegion(BulkStartOffset, BulkDataSizeOnDisk, BulkDataStorageInfo.BulkDataFileRegionType));
			}
		}

		const int64 SavedLinkerOffset = Linker->Tell();
		Linker->Seek(BulkDataStorageInfo.BulkDataFlagsPos);
		*Linker << BulkDataFlags;
		Linker->Seek(BulkDataStorageInfo.BulkDataSizeOnDiskPos);
		SerializeBulkDataSizeInt(*Linker, BulkDataSizeOnDisk, static_cast<EBulkDataFlags>(BulkDataFlags));
		Linker->Seek(BulkDataStorageInfo.BulkDataOffsetInFilePos);
		*Linker << BulkDataOffsetInFile;
		Linker->Seek(SavedLinkerOffset);

#if WITH_EDITOR
		// If we are overwriting the LoadedPath for the current package, the bulk data flags and location need to be updated to match
		// the values set in the package on disk.
		if (Linker->bUpdatingLoadedPath)
		{
			BulkData->SetFlagsFromDiskWrittenValues(static_cast<EBulkDataFlags>(BulkDataFlags), BulkDataOffsetInFile,
				BulkDataSizeOnDisk, Linker->Summary.BulkDataStartOffset);
		}
#endif
	}

	if (BulkArchive)
	{
		if (PackageWriter)
		{
			auto AddSizeAndConvertToIoBuffer = [&TotalPackageSizeUncompressed](FLargeMemoryWriter* Writer)
			{
				const int64 TotalSize = Writer->TotalSize();
				TotalPackageSizeUncompressed += TotalSize;
				return FIoBuffer(FIoBuffer::AssumeOwnership, Writer->ReleaseOwnership(), TotalSize);
			};

			IPackageWriter::FBulkDataInfo BulkInfo;
			BulkInfo.PackageName = InOuter->GetFName();
			// Adjust LooseFilePath if needed
			if (bIsOptionalRealm)
			{
				// Optional output have the form PackagePath.o.ext
				BulkInfo.LooseFilePath = FPathViews::ChangeExtension(Filename, TEXT("o.") + FPaths::GetExtension(Filename));
				BulkInfo.MultiOutputIndex = 1;
			}
			else
			{
				BulkInfo.LooseFilePath = Filename;
			}
			FPackageId PackageId = FPackageId::FromName(BulkInfo.PackageName);
				
			if (BulkArchive->TotalSize())
			{
				BulkInfo.ChunkId = CreateIoChunkId(PackageId.Value(), BulkInfo.MultiOutputIndex, EIoChunkType::BulkData);
				BulkInfo.BulkDataType = bSeparateSegmentsEnabled ?
					IPackageWriter::FBulkDataInfo::BulkSegment : IPackageWriter::FBulkDataInfo::AppendToExports;
				BulkInfo.LooseFilePath = FPathViews::ChangeExtension(BulkInfo.LooseFilePath, LexToString(EPackageExtension::BulkDataDefault));
					
				PackageWriter->WriteBulkData(BulkInfo, AddSizeAndConvertToIoBuffer(BulkArchive.Get()), BulkArchive->FileRegions);
			}
			// @note FH: temporarily do not handle optional bulk data into editor optional packages, proper support will be added soon
			if (OptionalBulkArchive && OptionalBulkArchive->TotalSize() && !bIsOptionalRealm)
			{
				BulkInfo.ChunkId = CreateIoChunkId(PackageId.Value(), BulkInfo.MultiOutputIndex, EIoChunkType::OptionalBulkData);
				BulkInfo.BulkDataType = IPackageWriter::FBulkDataInfo::Optional;
				BulkInfo.LooseFilePath = FPathViews::ChangeExtension(BulkInfo.LooseFilePath, LexToString(EPackageExtension::BulkDataOptional));
				PackageWriter->WriteBulkData(BulkInfo, AddSizeAndConvertToIoBuffer(OptionalBulkArchive.Get()), OptionalBulkArchive->FileRegions);
			}
			if (MappedBulkArchive && MappedBulkArchive->TotalSize())
			{
				checkf(!bIsOptionalRealm, TEXT("MemoryMappedBulkData is currently unsupported with optional package multi output for %s"), *InOuter->GetName());
				BulkInfo.ChunkId = CreateIoChunkId(PackageId.Value(), 0, EIoChunkType::MemoryMappedBulkData);
				BulkInfo.BulkDataType = IPackageWriter::FBulkDataInfo::Mmap;
				BulkInfo.LooseFilePath = FPathViews::ChangeExtension(BulkInfo.LooseFilePath, LexToString(EPackageExtension::BulkDataMemoryMapped));
				PackageWriter->WriteBulkData(BulkInfo, AddSizeAndConvertToIoBuffer(MappedBulkArchive.Get()), MappedBulkArchive->FileRegions);
			}
		}
		else
		{
			checkf(!bIsOptionalRealm, TEXT("Package optional package multi output is unsupported without a PackageWriter for %s"), *InOuter->GetName());
			auto WriteBulkData = [&](FLargeMemoryWriterWithRegions* Archive, const TCHAR* BulkFileExtension)
			{
				if (const int64 DataSize = Archive ? Archive->TotalSize() : 0)
				{
					TotalPackageSizeUncompressed += DataSize;

					FLargeMemoryPtr DataPtr(Archive->ReleaseOwnership());

					const FString ArchiveFilename = FPaths::ChangeExtension(Filename, BulkFileExtension);

					EAsyncWriteOptions WriteOptions(EAsyncWriteOptions::None);
					SavePackageUtilities::AsyncWriteFile(MoveTemp(DataPtr), DataSize, *ArchiveFilename, WriteOptions, Archive->FileRegions);
				}
			};

			WriteBulkData(BulkArchive.Get(), LexToString(EPackageExtension::BulkDataDefault));
			WriteBulkData(OptionalBulkArchive.Get(), LexToString(EPackageExtension::BulkDataOptional));
			WriteBulkData(MappedBulkArchive.Get(), LexToString(EPackageExtension::BulkDataMemoryMapped));
		}
	}

	if (!bSeparateSegmentsEnabled)
	{
		if (PackageWriter)
		{
			check(BulkArchive.IsValid() && LinkerStart == 0);
			InOutStartOffset += BulkArchive->TotalSize();
		}
		else
		{
			check(!BulkArchive.IsValid() && LinkerStart > 0);
			InOutStartOffset += Linker->Tell() - LinkerStart;
		}
	}

	Linker->BulkDataToAppend.Empty();
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

} // end namespace SavePackageUtilities

void UPackage::WaitForAsyncFileWrites()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPackage::WaitForAsyncFileWrites);

	while (OutstandingAsyncWrites.GetValue())
	{
		FPlatformProcess::Sleep(0.0f);
	}
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


namespace UE
{
namespace AssetRegistry
{
	// See the corresponding ReadPackageDataMain and ReadPackageDataDependencies defined in PackageReader.cpp in AssetRegistry module
	void WritePackageData(FStructuredArchiveRecord& ParentRecord, bool bIsCooking, const UPackage* Package, FLinkerSave* Linker, const TSet<UObject*>& ImportsUsedInGame, const TSet<FName>& SoftPackagesUsedInGame, const ITargetPlatform* TargetPlatform)
	{
		// To avoid large patch sizes, we have frozen cooked package format at the format before VER_UE4_ASSETREGISTRY_DEPENDENCYFLAGS
		bool bPreDependencyFormat = bIsCooking;

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

		// Collect the tag map
		TArray<UObject*> AssetObjects;
		if (!(Linker->Summary.GetPackageFlags() & PKG_FilterEditorOnly))
		{
			// Find any exports which are not in the tag map
			for (int32 i = 0; i < Linker->ExportMap.Num(); i++)
			{
				FObjectExport& Export = Linker->ExportMap[i];
				if (Export.Object && Export.Object->IsAsset())
				{
					AssetObjects.Add(Export.Object);
				}
			}
		}
		int32 ObjectCount = AssetObjects.Num();
		FStructuredArchive::FArray AssetArray = AssetRegistryRecord.EnterArray(TEXT("TagMap"), ObjectCount);
		for (int32 ObjectIdx = 0; ObjectIdx < AssetObjects.Num(); ++ObjectIdx)
		{
			const UObject* Object = AssetObjects[ObjectIdx];

			// Exclude the package name in the object path, we just need to know the path relative to the package we are saving
			FString ObjectPath = Object->GetPathName(Package);
			FString ObjectClassName = Object->GetClass()->GetPathName();

			TArray<UObject::FAssetRegistryTag> SourceTags;
			Object->GetAssetRegistryTags(SourceTags);
#if WITH_EDITOR
			Object->GetExtendedAssetRegistryTagsForSave(TargetPlatform, SourceTags);
#endif // WITH_EDITOR

			TArray<UObject::FAssetRegistryTag> Tags;
			for (UObject::FAssetRegistryTag& SourceTag : SourceTags)
			{
				UObject::FAssetRegistryTag* Existing = Tags.FindByPredicate([SourceTag](const UObject::FAssetRegistryTag& InTag) { return InTag.Name == SourceTag.Name; });
				if (Existing)
				{
					Existing->Value = SourceTag.Value;
				}
				else
				{
					Tags.Add(SourceTag);
				}
			}

			int32 TagCount = Tags.Num();

			FStructuredArchive::FRecord AssetRecord = AssetArray.EnterElement().EnterRecord();
			AssetRecord << SA_VALUE(TEXT("Path"), ObjectPath) << SA_VALUE(TEXT("Class"), ObjectClassName);

			FStructuredArchive::FMap TagMap = AssetRecord.EnterField(TEXT("Tags")).EnterMap(TagCount);

			for (TArray<UObject::FAssetRegistryTag>::TConstIterator TagIter(Tags); TagIter; ++TagIter)
			{
				FString Key = TagIter->Name.ToString();
				FString Value = TagIter->Value;

				TagMap.EnterElement(Key) << Value;
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
}
