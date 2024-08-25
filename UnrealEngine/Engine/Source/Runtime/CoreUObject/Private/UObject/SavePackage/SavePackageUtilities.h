// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/StringFwd.h"
#include "HAL/ThreadSingleton.h"
#include "ProfilingDebugging/CookStats.h"
#include "Serialization/ArchiveObjectCrc32.h"
#include "Serialization/ArchiveStackTrace.h"
#include "Serialization/FileRegions.h"
#include "UObject/NameTypes.h"
#include "UObject/Package.h"
#include "UObject/UObjectMarks.h"
#include "UObject/ObjectPtr.h"
#include "UObject/SavePackage.h"

// This file contains private utilities shared by UPackage::Save and UPackage::Save2 

class FCbFieldView;
class FCbWriter;
class FPackagePath;
class FSaveContext;
class FSavePackageContext;
class IPackageWriter;

enum class ESavePackageResult;

// Save Time trace
#if UE_TRACE_ENABLED && !UE_BUILD_SHIPPING
UE_TRACE_CHANNEL_EXTERN(SaveTimeChannel)
#define SCOPED_SAVETIMER(TimerName) TRACE_CPUPROFILER_EVENT_SCOPE_ON_CHANNEL(TimerName, SaveTimeChannel)
#define SCOPED_SAVETIMER_TEXT(TimerName) TRACE_CPUPROFILER_EVENT_SCOPE_TEXT_ON_CHANNEL(TimerName, SaveTimeChannel)
#else
#define SCOPED_SAVETIMER(TimerName)
#define SCOPED_SAVETIMER_TEXT(TimerName)
#endif

struct FLargeMemoryDelete
{
	void operator()(uint8* Ptr) const
	{
		if (Ptr)
		{
			FMemory::Free(Ptr);
		}
	}
};
typedef TUniquePtr<uint8, FLargeMemoryDelete> FLargeMemoryPtr;

enum class EAsyncWriteOptions
{
	None = 0
};
ENUM_CLASS_FLAGS(EAsyncWriteOptions)

struct FScopedSavingFlag
{
	FScopedSavingFlag(bool InSavingConcurrent, UPackage* InSavedPackage);
	~FScopedSavingFlag();

	bool bSavingConcurrent;

private:
	// The package being saved
	UPackage* SavedPackage = nullptr;
};

struct FCanSkipEditorReferencedPackagesWhenCooking
{
	bool bCanSkipEditorReferencedPackagesWhenCooking;
	FCanSkipEditorReferencedPackagesWhenCooking();
	FORCEINLINE operator bool() const { return bCanSkipEditorReferencedPackagesWhenCooking; }
};


/** Represents an output file from the package when saving */
struct FSavePackageOutputFile
{
	/** Constructor used for async saving */
	FSavePackageOutputFile(const FString& InTargetPath, FLargeMemoryPtr&& MemoryBuffer, const TArray<FFileRegion>& InFileRegions, int64 InDataSize)
		: TargetPath(InTargetPath)
		, FileMemoryBuffer(MoveTemp(MemoryBuffer))
		, FileRegions(InFileRegions)
		, DataSize(InDataSize)
	{

	}

	/** Constructor used for saving first to a temp file which can be later moved to the target directory */
	FSavePackageOutputFile(const FString& InTargetPath, const FString& InTempFilePath, int64 InDataSize)
		: TargetPath(InTargetPath)
		, TempFilePath(InTempFilePath)
		, DataSize(InDataSize)
	{

	}

	/** The final target location of the file once all saving operations are completed */
	FString TargetPath;

	/** The temp location (if any) that the file is stored at, pending a move to the TargetPath */
	FString TempFilePath;

	/** The entire file stored as a memory buffer for the async saving path */
	FLargeMemoryPtr FileMemoryBuffer;
	/** An array of file regions in FileMemoryBuffer generated during cooking */
	TArray<FFileRegion> FileRegions;

	/** The size of the file in bytes */
	int64 DataSize;
};

// Currently we only expect to store up to 2 files in this, so set the inline capacity to double of this
using FSavePackageOutputFileArray = TArray<FSavePackageOutputFile, TInlineAllocator<4>>;

 /**
  * Helper structure to encapsulate sorting a linker's import table alphabetically
  * @note Save2 should not have to use this sorting long term
  */
struct FObjectImportSortHelper
{
	/**
	 * Sorts imports according to the order in which they occur in the list of imports.
	 *
	 * @param	Linker				linker containing the imports that need to be sorted
	 */
	static void SortImports(FLinkerSave* Linker);
};

/**
 * Helper structure to encapsulate sorting a linker's export table alphabetically
 * @note Save2 should not have to use this sorting long term
 */
struct FObjectExportSortHelper
{
	/**
	 * Sorts exports alphabetically.
	 *
	 * @param	Linker				linker containing the exports that need to be sorted
	 */
	static void SortExports(FLinkerSave* Linker);
};

struct FEDLCookCheckerThreadState;

/**
 * Helper struct used during cooking to validate EDL dependencies
 */
struct FEDLCookChecker
{
	void SetActiveIfNeeded();

	void Reset();

	void AddImport(TObjectPtr<UObject> Import, UPackage* ImportingPackage);
	void AddExport(UObject* Export);
	void AddArc(UObject* DepObject, bool bDepIsSerialize, UObject* Export, bool bExportIsSerialize);
	void AddPackageWithUnknownExports(FName LongPackageName);

	static void StartSavingEDLCookInfoForVerification();
	static void Verify(const UE::SavePackageUtilities::FEDLMessageCallback& MessageCallback,
		bool bFullReferencesExpected);
	static void MoveToCompactBinaryAndClear(FCbWriter& Writer, bool& bOutHasData);
	static bool AppendFromCompactBinary(FCbFieldView Field);

private:
	static FEDLCookChecker AccumulateAndClear();
	void WriteToCompactBinary(FCbWriter& Writer);
	bool ReadFromCompactBinary(FCbFieldView Field);

	typedef uint32 FEDLNodeID;
	static const FEDLNodeID NodeIDInvalid = static_cast<FEDLNodeID>(-1);

	struct FEDLNodeData;
public: // FEDLNodeHash is public only so that GetTypeHash can be defined
	enum class EObjectEvent : uint8
	{
		Create,
		Serialize,
		Max = Serialize,
	};

	/**
	 * Wrapper aroundan FEDLNodeData (or around a UObject when searching for an FEDLNodeData corresponding to the UObject)
	 * that provides the hash-by-objectpath to lookup the FEDLNodeData for an objectpath.
	 */
	struct FEDLNodeHash
	{
		FEDLNodeHash(); // creates an uninitialized node; only use this to provide as an out parameter
		FEDLNodeHash(const TArray<FEDLNodeData>* InNodes, FEDLNodeID InNodeID, EObjectEvent InObjectEvent);
		FEDLNodeHash(TObjectPtr<UObject> InObject, EObjectEvent InObjectEvent);
		FEDLNodeHash(const FEDLNodeHash& Other);
		bool operator==(const FEDLNodeHash& Other) const;
		FEDLNodeHash& operator=(const FEDLNodeHash& Other);
		friend uint32 GetTypeHash(const FEDLNodeHash& A);

		FName GetName() const;
		bool TryGetParent(FEDLNodeHash& Parent) const;
		EObjectEvent GetObjectEvent() const;
		void SetNodes(const TArray<FEDLNodeData>* InNodes);

	private:
		static FName ObjectNameFirst(const FEDLNodeHash& InNode, uint32& OutNodeID, TObjectPtr<const UObject>& OutObject);
		static FName ObjectNameNext(const FEDLNodeHash& InNode, uint32& OutNodeID, TObjectPtr<const UObject>& OutObject);
			
		union
		{
			/**
			 * The array of nodes from the FEDLCookChecker; this is how we lookup the node for the FEDLNodeData.
			 * Because the FEDLNodeData are elements in an array which can resize and therefore reallocate the nodes, we cannot store the pointer to the node.
			 * Only used if bIsNode is true.
			 */
			const TArray<FEDLNodeData>* Nodes;
			/** Pointer to the Object we are looking up, if this hash was created during lookup-by-objectpath for an object */
			TObjectPtr<const UObject> Object;
		};
		/** The identifier for the FEDLNodeData this hash is wrapping. Only used if bIsNode is true. */
		FEDLNodeID NodeID;
		/** True if this hash is wrapping an FEDLNodeData, false if it is wrapping a UObject. */
		bool bIsNode;
		EObjectEvent ObjectEvent;
	};

private:

	/**
	 * Node representing either the Create event or Serialize event of a UObject in the graph of runtime dependencies between UObjects.
	 */
	struct FEDLNodeData
	{
		// Note that order of the variables is important to reduce alignment waste in the size of FEDLNodeData.
		/** Name of the UObject represented by this node; full objectpath name is obtainable by combining parent data with the name. */
		FName Name;
		/** Index of this node in the FEDLCookChecker's Nodes array. This index is used to provide a small-memory-usage identifier for the node. */
		FEDLNodeID ID;
		/**
		 * Tracks references to this node's UObjects from other packages (which is the reverse of the references from each node that we track in NodePrereqs.)
		 * We only need this information from each package, so we track by package name instead of node id.
		 */
		TArray<FName> ImportingPackagesSorted;
		/**
		 * ID of the node representing the UObject parent of this node's UObject. NodeIDInvalid if the UObject has no parent.
		 * The ParentID always refers to the node for the Create event of the parent UObject.
		 */
		uint32 ParentID;
		/** True if this node represents the Serialize event on the UObject, false if it represents the Create event. */
		EObjectEvent ObjectEvent;
		/** True if the UObject represented by this node has been exported by a SavePackage call; used to verify that the imports requested by packages are present somewhere in the cook. */
		bool bIsExport;

		FEDLNodeData() { /* Fields are uninitialized */ }
		FEDLNodeData(FEDLNodeID InID, FEDLNodeID InParentID, FName InName, EObjectEvent InObjectEvent);
		FEDLNodeData(FEDLNodeID InID, FEDLNodeID InParentID, FName InName, FEDLNodeData&& Other);
		FEDLNodeHash GetNodeHash(const FEDLCookChecker& Owner) const;

		FString ToString(const FEDLCookChecker& Owner) const;
		void AppendPathName(const FEDLCookChecker& Owner, FStringBuilderBase& Result) const;
		FName GetPackageName(const FEDLCookChecker& Owner) const;
		void Merge(FEDLNodeData&& Other);
	};

	FEDLNodeID FindOrAddNode(const FEDLNodeHash& NodeLookup);
	FEDLNodeID FindOrAddNode(FEDLNodeData&& NodeData, const FEDLCookChecker& OldOwnerOfNode, FEDLNodeID ParentIDInThis, bool& bNew);
	FEDLNodeID FindNode(const FEDLNodeHash& NodeHash);
	void Merge(FEDLCookChecker&& Other);
	bool CheckForCyclesInner(TSet<FEDLNodeID>& Visited, TSet<FEDLNodeID>& Stack, const FEDLNodeID& Visit, FEDLNodeID& FailNode);
	void AddDependency(FEDLNodeID SourceID, FEDLNodeID TargetID);

	/**
	 * All the FEDLNodeDatas that have been created for this checker. These are allocated as elements of an array rather than pointers to reduce cputime and
	 * memory due to many small allocations, and to provide index-based identifiers. Nodes are not deleted until the checker is reset.
	 */
	TArray<FEDLNodeData> Nodes;
	/** A map to lookup the node for a UObject or for the corresponding node in another thread's FEDLCookChecker. */
	TMap<FEDLNodeHash, FEDLNodeID> NodeHashToNodeID;
	/** The graph of dependencies between nodes. */
	TMultiMap<FEDLNodeID, FEDLNodeID> NodePrereqs;
	/**
	 * Packages that were cooked iteratively and therefore have an unknown set of exports.
	 * We suppress warnings for exports missing from these packages.
	 */
	TSet<FName> PackagesWithUnknownExports;
	/** True if the EDLCookChecker should be active; it is turned off if the runtime will not be using EDL. */
	bool bIsActive = false;

	/** When cooking with concurrent saving, each thread has its own FEDLCookChecker, and these are merged after the cook is complete. */
	static FCriticalSection CookCheckerInstanceCritical;
	static TArray<FEDLCookChecker*> CookCheckerInstances;

	friend FEDLCookCheckerThreadState;
};

/** Per-thread accessor for writing EDL dependencies to global FEDLCookChecker storage. */
struct FEDLCookCheckerThreadState : public TThreadSingleton<FEDLCookCheckerThreadState>
{
	FEDLCookCheckerThreadState();

	void AddImport(TObjectPtr<UObject> Import, UPackage* ImportingPackage)
	{
		Checker.AddImport(Import, ImportingPackage);
	}
	void AddExport(UObject* Export)
	{
		Checker.AddExport(Export);
	}
	void AddArc(UObject* DepObject, bool bDepIsSerialize, UObject* Export, bool bExportIsSerialize)
	{
		Checker.AddArc(DepObject, bDepIsSerialize, Export, bExportIsSerialize);
	}
	void AddPackageWithUnknownExports(FName LongPackageName)
	{
		Checker.AddPackageWithUnknownExports(LongPackageName);
	}

private:
	FEDLCookChecker Checker;
	friend TThreadSingleton<FEDLCookCheckerThreadState>;
	friend FEDLCookChecker;
};

// Utility functions used by both UPackage::Save and/or UPackage::Save2
namespace UE::SavePackageUtilities
{

extern const FName NAME_World;
extern const FName NAME_Level;
extern const FName NAME_PrestreamPackage;

void SaveThumbnails(UPackage* InOuter, FLinkerSave* Linker, FStructuredArchive::FSlot Slot);

/**
	* Used to append additional data to the end of the package file by invoking callbacks stored in the linker.
	* They may be saved to the end of the file, or to a separate archive passed into the PackageWriter.
	 
	* @param Linker The linker containing the exports. Provides the list of AdditionalData, and the data may write to it as their target archive.
	* @param InOutStartOffset In value is the offset in the Linker's archive where the datas will be put. If SavePackageContext settings direct
	*        the datas to write in a separate archive that will be combined after the linker, the value is the offset after the Linker archive's 
	*        totalsize and after any previous post-Linker archive data such as BulkDatas.
	*        Output value is incremented by the number of bytes written the Linker or the separate archive at the end of the linker.
	* @param SavePackageContext If non-null and configured to require it, data is passed to this PackageWriter on this context rather than appended to the Linker archive.
	*/
ESavePackageResult AppendAdditionalData(FLinkerSave& Linker, int64& InOutDataStartOffset, FSavePackageContext* SavePackageContext);
	
/** Used to create the sidecar file (.upayload) from payloads that have been added to the linker */
ESavePackageResult CreatePayloadSidecarFile(FLinkerSave& Linker, const FPackagePath& PackagePath, const bool bSaveToMemory,
	FSavePackageOutputFileArray& AdditionalPackageFiles, FSavePackageContext* SavePackageContext);
	
void SaveWorldLevelInfo(UPackage* InOuter, FLinkerSave* Linker, FStructuredArchive::FRecord Record);
EObjectMark GetExcludedObjectMarksForTargetPlatform(const class ITargetPlatform* TargetPlatform);
void FindMostLikelyCulprit(const TArray<UObject*>& BadObjects, UObject*& MostLikelyCulprit, FString& OutReferencer, FSaveContext* InOptionalSaveContext = nullptr);
	
/** 
	* Search 'OutputFiles' for output files that were saved to the temp directory and move those files
	* to their final location. Output files that were not saved to the temp directory will be ignored.
	* 
	* If errors are encountered then the original state of the package will be restored and should continue to work.
	*/
ESavePackageResult FinalizeTempOutputFiles(const FPackagePath& PackagePath, const FSavePackageOutputFileArray& OutputFiles, const FDateTime& FinalTimeStamp);

void WriteToFile(const FString& Filename, const uint8* InDataPtr, int64 InDataSize);
void AsyncWriteFile(FLargeMemoryPtr Data, const int64 DataSize, const TCHAR* Filename, EAsyncWriteOptions Options, TArrayView<const FFileRegion> InFileRegions);
void AsyncWriteFile(EAsyncWriteOptions Options, FSavePackageOutputFile& File);

void GetCDOSubobjects(UObject* CDO, TArray<UObject*>& Subobjects);

enum class EEditorOnlyObjectFlags
{
	None = 0,
	CheckRecursive = 1 << 1,
	ApplyHasNonEditorOnlyReferences = 1 << 2,
	CheckMarks UE_DEPRECATED(5.3, "CheckMarks is no longer supported") = 1 << 3,

};
ENUM_CLASS_FLAGS(EEditorOnlyObjectFlags);

/** Returns result of IsEditorOnlyObjectInternal if Engine:[Core.System]:CanStripEditorOnlyExportsAndImports (ini) is set to true */
bool IsStrippedEditorOnlyObject(const UObject* InObject, EEditorOnlyObjectFlags Flags);

bool IsEditorOnlyObjectInternal(const UObject* InObject, EEditorOnlyObjectFlags Flags);

}

#if ENABLE_COOK_STATS
struct FSavePackageStats
{
	static int32 NumPackagesSaved;
	static double SavePackageTimeSec;
	static double TagPackageExportsPresaveTimeSec;
	static double TagPackageExportsTimeSec;
	static double FullyLoadLoadersTimeSec;
	static double ResetLoadersTimeSec;
	static double TagPackageExportsGetObjectsWithOuter;
	static double TagPackageExportsGetObjectsWithMarks;
	static double SerializeImportsTimeSec;
	static double SortExportsSeekfreeInnerTimeSec;
	static double SerializeExportsTimeSec;
	static double SerializeBulkDataTimeSec;
	static double AsyncWriteTimeSec;
	static double MBWritten;
	static TMap<FName, FArchiveDiffStats> PackageDiffStats;
	static int32 NumberOfDifferentPackages;
	static FCookStatsManager::FAutoRegisterCallback RegisterCookStats;
	static void AddSavePackageStats(FCookStatsManager::AddStatFuncRef AddStat);
	static void MergeStats(const TMap<FName, FArchiveDiffStats>& ToMerge);
};
#endif
