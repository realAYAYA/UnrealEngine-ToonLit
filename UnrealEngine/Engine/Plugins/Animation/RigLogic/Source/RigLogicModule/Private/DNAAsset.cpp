// Copyright Epic Games, Inc. All Rights Reserved.

#include "DNAAsset.h"
#include "DNAUtils.h"

#include "ArchiveMemoryStream.h"
#include "DNAAssetCustomVersion.h"
#include "DNAReaderAdapter.h"
#include "DNAIndexMapping.h"
#include "FMemoryResource.h"
#include "RigLogicMemoryStream.h"
#include "SharedRigRuntimeContext.h"

#if WITH_EDITORONLY_DATA
    #include "EditorFramework/AssetImportData.h"
#endif
#include "Engine/AssetUserData.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "UObject/UObjectGlobals.h"
#include "HAL/LowLevelMemTracker.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"

#include "riglogic/RigLogic.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DNAAsset)

DEFINE_LOG_CATEGORY(LogDNAAsset);

static constexpr uint32 AVG_EMPTY_SIZE = 4 * 1024;
static constexpr uint32 AVG_BEHAVIOR_SIZE = 5 * 1024 * 1024;
static constexpr uint32 AVG_MACHINE_LEARNED_BEHAVIOR_SIZE = 5 * 1024 * 1024;
static constexpr uint32 AVG_GEOMETRY_SIZE = 50 * 1024 * 1024;

static TSharedPtr<IDNAReader> ReadDNAFromStream(rl4::BoundedIOStream* Stream, EDNADataLayer Layer, uint16 MaxLOD)
{
	auto DNAStreamReader = rl4::makeScoped<dna::BinaryStreamReader>(Stream, static_cast<dna::DataLayer>(Layer), dna::UnknownLayerPolicy::Preserve, MaxLOD, FMemoryResource::Instance());
	DNAStreamReader->read();
	if (!rl4::Status::isOk())
	{
		UE_LOG(LogDNAAsset, Error, TEXT("%s"), ANSI_TO_TCHAR(rl4::Status::get().message));
		return nullptr;
	}
	return MakeShared<FDNAReader<dna::BinaryStreamReader>>(DNAStreamReader.release());
}

static void WriteDNAToStream(const IDNAReader* Source, EDNADataLayer Layer, rl4::BoundedIOStream* Destination)
{
	auto DNAWriter = rl4::makeScoped<dna::BinaryStreamWriter>(Destination, FMemoryResource::Instance());
	if (Source != nullptr)
	{
		DNAWriter->setFrom(Source->Unwrap(), static_cast<dna::DataLayer>(Layer), dna::UnknownLayerPolicy::Preserve, FMemoryResource::Instance());
	}
	DNAWriter->write();
}

static TSharedPtr<IDNAReader> CopyDNALayer(const IDNAReader* Source, EDNADataLayer DNADataLayer, uint32 PredictedSize)
{
	// To avoid lots of reallocations in `FRigLogicMemoryStream`, reserve an approximate size
	// that we know would cause at most one reallocation in the worst case (but none for the average DNA)
	TArray<uint8> MemoryBuffer;
	MemoryBuffer.Reserve(PredictedSize);

	FRigLogicMemoryStream MemoryStream(&MemoryBuffer);
	WriteDNAToStream(Source, DNADataLayer, &MemoryStream);

	MemoryStream.seek(0ul);

	return ReadDNAFromBuffer(&MemoryBuffer, DNADataLayer);
}

static TSharedPtr<IDNAReader> CreateEmptyDNA(uint32 PredictedSize)
{
	// To avoid lots of reallocations in `FRigLogicMemoryStream`, reserve an approximate size
	// that we know would cause at most one reallocation in the worst case (but none for the average DNA)
	TArray<uint8> MemoryBuffer;
	MemoryBuffer.Reserve(PredictedSize);

	FRigLogicMemoryStream MemoryStream(&MemoryBuffer);
	WriteDNAToStream(nullptr, EDNADataLayer::All, &MemoryStream);

	MemoryStream.seek(0ul);

	return ReadDNAFromBuffer(&MemoryBuffer, EDNADataLayer::All);
}

UDNAAsset::UDNAAsset() : RigRuntimeContext{nullptr}
{
}

UDNAAsset::~UDNAAsset() = default;

TSharedPtr<IDNAReader> UDNAAsset::GetBehaviorReader()
{
	FReadScopeLock DNAScopeLock{DNAUpdateLock};
	return BehaviorReader;
}

#if WITH_EDITORONLY_DATA
TSharedPtr<IDNAReader> UDNAAsset::GetGeometryReader()
{
	FReadScopeLock DNAScopeLock{DNAUpdateLock};
	return GeometryReader;
}
#endif

void UDNAAsset::SetBehaviorReader(TSharedPtr<IDNAReader> SourceDNAReader)
{
	FWriteScopeLock DNAScopeLock{DNAUpdateLock};
	const size_t PredictedSize = (SourceDNAReader->GetNeuralNetworkCount() != 0) ? AVG_BEHAVIOR_SIZE + AVG_MACHINE_LEARNED_BEHAVIOR_SIZE : AVG_BEHAVIOR_SIZE;
	BehaviorReader = CopyDNALayer(SourceDNAReader.Get(), EDNADataLayer::Behavior | EDNADataLayer::MachineLearnedBehavior, PredictedSize);
	InvalidateRigRuntimeContext();
	InitializeRigRuntimeContext();
}

void UDNAAsset::SetGeometryReader(TSharedPtr<IDNAReader> SourceDNAReader)
{
#if WITH_EDITORONLY_DATA
	FWriteScopeLock DNAScopeLock{DNAUpdateLock};
	GeometryReader = CopyDNALayer(SourceDNAReader.Get(), EDNADataLayer::Geometry, AVG_GEOMETRY_SIZE);
#endif // #if WITH_EDITORONLY_DATA
}

void UDNAAsset::InitializeForRuntimeFrom(UDNAAsset* Other)
{
	FWriteScopeLock DNAScopeLock{DNAUpdateLock};
	FWriteScopeLock ContextScopeLock{RigRuntimeContextUpdateLock};
	FWriteScopeLock MappingScopeLock{DNAIndexMappingUpdateLock};

	// Store a reference to the other asset's runtime context, using the accessor function to 
	// ensure that the necessary lock is taken.
	//
	// The runtime context is immutable, i.e. not modified after initialization, so it's safe to
	// share across UDNAAssets.
	RigRuntimeContext = Other->GetRigRuntimeContext();

	// BehaviorReader is used at runtime by GetDNAIndexMapping, so it needs to be populated here.
	//
	// The reference is taken from RigRuntimeContext to ensure it's consistent with the runtime
	// context, as the UDNAAsset's BehaviorReader can be changed to point to a new one before the
	// runtime context is updated.
	//
	// As with the runtime context itself, the BehaviorReader is immutable and safe to share.
	BehaviorReader = RigRuntimeContext->BehaviorReader;

	// Ensure bKeepDNAAfterInitialization reflects the current state of the BehaviorReader, i.e.
	// if bKeepDNAAfterInitialization is true, BehaviorReader will contain DNA and vice versa.
	bKeepDNAAfterInitialization = Other->bKeepDNAAfterInitialization;

	// This map is populated on demand, so doesn't need to be copied.
	DNAIndexMappingContainer.Empty(1);

	// Clear any fields not needed at runtime to avoid any old data causing confusion
#if WITH_EDITORONLY_DATA
	AssetImportData = nullptr;
#endif

	DnaFileName.Empty();
	GeometryReader = nullptr;
}

void UDNAAsset::InvalidateRigRuntimeContext()
{
	FWriteScopeLock ContextScopeLock{RigRuntimeContextUpdateLock};
	FWriteScopeLock MappingScopeLock(DNAIndexMappingUpdateLock);
	RigRuntimeContext = nullptr;
	DNAIndexMappingContainer.Empty(1);
}

void UDNAAsset::InitializeRigRuntimeContext()
{
	LLM_SCOPE_BYNAME(TEXT("Animation/RigLogic"));

	// Assumes DNAUpdateLock is locked by caller

	TSharedPtr<FSharedRigRuntimeContext> NewContext = MakeShared<FSharedRigRuntimeContext>();
	if (BehaviorReader.IsValid() && (BehaviorReader->GetJointCount() != 0))
	{
		NewContext->BehaviorReader = BehaviorReader;
		NewContext->RigLogic = MakeShared<FRigLogic>(BehaviorReader.Get());
		NewContext->CacheVariableJointIndices();
		{
			FWriteScopeLock ContextScopeLock{RigRuntimeContextUpdateLock};
			RigRuntimeContext = NewContext;
		}
#if !WITH_EDITOR
		if (!bKeepDNAAfterInitialization)
		{
			BehaviorReader->Unload(EDNADataLayer::Behavior);
			BehaviorReader->Unload(EDNADataLayer::Geometry);
			BehaviorReader->Unload(EDNADataLayer::MachineLearnedBehavior);
		}
#endif  // !WITH_EDITOR
	}
}

TSharedPtr<FSharedRigRuntimeContext> UDNAAsset::GetRigRuntimeContext()
{
	LLM_SCOPE_BYNAME(TEXT("Animation/RigLogic"));

	FReadScopeLock ContextScopeLock{RigRuntimeContextUpdateLock};
	return RigRuntimeContext;
}

TSharedPtr<FDNAIndexMapping> UDNAAsset::GetDNAIndexMapping(const USkeleton* Skeleton,
														   const USkeletalMesh* SkeletalMesh,
														   const USkeletalMeshComponent* SkeletalMeshComponent)
{
	LLM_SCOPE_BYNAME(TEXT("Animation/RigLogic"));

	FReadScopeLock DNAScopeLock{DNAUpdateLock};
	FWriteScopeLock MappingScopeLock(DNAIndexMappingUpdateLock);

	// Find currently needed mapping and also clean stale objects along the way (requires only one iteration over the map)
	TSharedPtr<FDNAIndexMapping> DNAIndexMapping;
	for (auto Iterator = DNAIndexMappingContainer.CreateIterator(); Iterator; ++Iterator)
	{
		if (Iterator->Key.IsValid())
		{
			if (Iterator->Key == SkeletalMesh)
			{
				DNAIndexMapping = Iterator->Value;
			}
		}
		else
		{
			Iterator.RemoveCurrent();
		}
	}

	// Check if currently needed mapping exists, and if not, create it now
	const FGuid SkeletonGuid = Skeleton->GetGuid();
	if (!DNAIndexMapping.IsValid() || (SkeletonGuid != DNAIndexMapping->SkeletonGuid))
	{
		DNAIndexMapping = MakeShared<FDNAIndexMapping>();
		DNAIndexMapping->SkeletonGuid = SkeletonGuid;
		DNAIndexMapping->MapControlCurves(BehaviorReader.Get(), Skeleton);
		DNAIndexMapping->MapNeuralNetworkMaskCurves(BehaviorReader.Get(), Skeleton);
		DNAIndexMapping->MapJoints(BehaviorReader.Get(), SkeletalMeshComponent);
		DNAIndexMapping->MapMorphTargets(BehaviorReader.Get(), Skeleton, SkeletalMesh);
		DNAIndexMapping->MapMaskMultipliers(BehaviorReader.Get(), Skeleton);
		DNAIndexMappingContainer.Add(SkeletalMesh, DNAIndexMapping);
	}

	return DNAIndexMapping;
}

bool UDNAAsset::Init(const FString& DNAFilename)
{
	LLM_SCOPE_BYNAME(TEXT("Animation/RigLogic"));

	if (!rl4::Status::isOk())
	{
		UE_LOG(LogDNAAsset, Warning, TEXT("%s"), ANSI_TO_TCHAR(rl4::Status::get().message));
	}

	DnaFileName = DNAFilename; //memorize for re-import
	
	if (!FPaths::FileExists(DNAFilename))
	{
		UE_LOG(LogDNAAsset, Error, TEXT("DNA file %s doesn't exist!"), *DNAFilename);
		return false;
	}
	
	// Temporary buffer for the DNA file
	TArray<uint8> TempFileBuffer;
	
	if (!FFileHelper::LoadFileToArray(TempFileBuffer, *DNAFilename)) //load entire DNA file into the array
	{
		UE_LOG(LogDNAAsset, Error, TEXT("Couldn't read DNA file %s!"), *DNAFilename);
		return false;
	}
	
	FWriteScopeLock DNAScopeLock{DNAUpdateLock};

	// Load run-time data (behavior) from whole-DNA buffer into BehaviorReader
	BehaviorReader = ReadDNAFromBuffer(&TempFileBuffer, EDNADataLayer::Behavior | EDNADataLayer::MachineLearnedBehavior, 0u); //0u = MaxLOD
	if (!BehaviorReader.IsValid())
	{
		return false;
	}

	InvalidateRigRuntimeContext();
	InitializeRigRuntimeContext();

#if WITH_EDITORONLY_DATA
	//We use geometry part of the data in MHC only (for updating the SkeletalMesh with
	//result of GeneSplicer), so we can drop geometry part when cooking for runtime
	GeometryReader = ReadDNAFromBuffer(&TempFileBuffer, EDNADataLayer::Geometry, 0u); //0u = MaxLOD
	if (!GeometryReader.IsValid())
	{
		return false;
	}
	//Note: in future, we will want to load geometry data in-game too 
	//to enable GeneSplicer to read geometry directly from SkeletalMeshes, as
	//a way to save memory, as on consoles the "database" will be exactly the set of characters
	//used in the game
#endif // #if WITH_EDITORONLY_DATA

	return true;
}

void UDNAAsset::Serialize(FArchive& Ar)
{
	LLM_SCOPE_BYNAME(TEXT("Animation/RigLogic"));

	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FDNAAssetCustomVersion::GUID);

	FWriteScopeLock DNAScopeLock{DNAUpdateLock};

	if (Ar.CustomVer(FDNAAssetCustomVersion::GUID) >= FDNAAssetCustomVersion::BeforeCustomVersionWasAdded)
	{
		if (Ar.IsLoading())
		{
			FArchiveMemoryStream BehaviorStream{&Ar};
			BehaviorReader = ReadDNAFromStream(&BehaviorStream, EDNADataLayer::Behavior | EDNADataLayer::MachineLearnedBehavior, 0u); //0u = max LOD
			// Geometry data is always present (even if only as an empty placeholder), just so the uasset
			// format remains consistent between editor and non-editor builds
			FArchiveMemoryStream GeometryStream{&Ar};
			auto Reader = ReadDNAFromStream(&GeometryStream, EDNADataLayer::Geometry, 0u); //0u = max LOD
#if WITH_EDITORONLY_DATA
			// Geometry data is discarded unless in Editor
			GeometryReader = Reader;
#endif // #if WITH_EDITORONLY_DATA

			InvalidateRigRuntimeContext();
			InitializeRigRuntimeContext();
		}

		if (Ar.IsSaving())
		{
			TSharedPtr<IDNAReader> EmptyDNA = CreateEmptyDNA(AVG_EMPTY_SIZE);
			IDNAReader* BehaviorReaderPtr = (BehaviorReader.IsValid() ? static_cast<IDNAReader*>(BehaviorReader.Get()) : EmptyDNA.Get());
			FArchiveMemoryStream BehaviorStream{&Ar};
			WriteDNAToStream(BehaviorReaderPtr, EDNADataLayer::Behavior | EDNADataLayer::MachineLearnedBehavior, &BehaviorStream);

			// When cooking (or when there was no Geometry data available), an empty DNA structure is written
			// into the stream, serving as a placeholder just so uasset files can be conveniently loaded
			// regardless if they were cooked or prepared for in-editor work
			IDNAReader* GeometryReaderPtr = (GeometryReader.IsValid() && !Ar.IsCooking() ? static_cast<IDNAReader*>(GeometryReader.Get()) : EmptyDNA.Get());
			FArchiveMemoryStream GeometryStream{&Ar};
			WriteDNAToStream(GeometryReaderPtr, EDNADataLayer::Geometry, &GeometryStream);
		}
	}
}

