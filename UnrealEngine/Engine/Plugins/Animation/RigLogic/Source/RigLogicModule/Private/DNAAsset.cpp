// Copyright Epic Games, Inc. All Rights Reserved.

#include "DNAAsset.h"
#include "DNAUtils.h"

#include "ArchiveMemoryStream.h"
#include "DNAAssetCustomVersion.h"
#include "DNAReaderAdapter.h"
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

#include "riglogic/RigLogic.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DNAAsset)

DEFINE_LOG_CATEGORY(LogDNAAsset);

static constexpr uint32 AVG_EMPTY_SIZE = 4 * 1024;
static constexpr uint32 AVG_BEHAVIOR_SIZE = 5 * 1024 * 1024;
static constexpr uint32 AVG_GEOMETRY_SIZE = 50 * 1024 * 1024;

static TSharedPtr<IDNAReader> ReadDNAFromStream(rl4::BoundedIOStream* Stream, EDNADataLayer Layer, uint16 MaxLOD)
{
	auto DNAStreamReader = rl4::makeScoped<dna::StreamReader>(Stream, static_cast<dna::DataLayer>(Layer), MaxLOD, FMemoryResource::Instance());
	DNAStreamReader->read();
	if (!rl4::Status::isOk())
	{
		UE_LOG(LogDNAAsset, Error, TEXT("%s"), ANSI_TO_TCHAR(rl4::Status::get().message));
		return nullptr;
	}
	return MakeShared<FDNAReader<dna::StreamReader>>(DNAStreamReader.release());
}

static void WriteDNAToStream(const IDNAReader* Source, EDNADataLayer Layer, rl4::BoundedIOStream* Destination)
{
	auto DNAWriter = rl4::makeScoped<dna::StreamWriter>(Destination, FMemoryResource::Instance());
	if (Source != nullptr)
	{
		DNAWriter->setFrom(Source->Unwrap(), static_cast<dna::DataLayer>(Layer), FMemoryResource::Instance());
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

TSharedPtr<IBehaviorReader> UDNAAsset::GetBehaviorReader()
{
	FScopeLock ScopeLock{&DNAUpdateSection};
	return BehaviorReader;
}

#if WITH_EDITORONLY_DATA
TSharedPtr<IGeometryReader> UDNAAsset::GetGeometryReader()
{
	FScopeLock ScopeLock{&DNAUpdateSection};
	return GeometryReader;
}
#endif

void UDNAAsset::SetBehaviorReader(TSharedPtr<IDNAReader> SourceDNAReader)
{
	FScopeLock ScopeLock{&DNAUpdateSection};
	BehaviorReader = CopyDNALayer(SourceDNAReader.Get(), EDNADataLayer::Behavior, AVG_BEHAVIOR_SIZE);
	InvalidateRigRuntimeContext();
}

void UDNAAsset::SetGeometryReader(TSharedPtr<IDNAReader> SourceDNAReader)
{
#if WITH_EDITORONLY_DATA
	FScopeLock ScopeLock{&DNAUpdateSection};
	GeometryReader = CopyDNALayer(SourceDNAReader.Get(), EDNADataLayer::Geometry, AVG_GEOMETRY_SIZE);
#endif // #if WITH_EDITORONLY_DATA
}

void UDNAAsset::InvalidateRigRuntimeContext()
{
	FWriteScopeLock ScopeLock{RigRuntimeContextUpdateLock};
	RigRuntimeContext = nullptr;
}

TSharedPtr<FSharedRigRuntimeContext> UDNAAsset::GetRigRuntimeContext(EDNARetentionPolicy Policy)
{
	LLM_SCOPE_BYNAME(TEXT("Animation/RigLogic"));

	FRWScopeLock ContextScopeLock(RigRuntimeContextUpdateLock, SLT_ReadOnly);
	if (!RigRuntimeContext.IsValid())
	{
		ContextScopeLock.ReleaseReadOnlyLockAndAcquireWriteLock_USE_WITH_CAUTION();
		if (!RigRuntimeContext.IsValid())
		{
			TSharedPtr<FSharedRigRuntimeContext> NewContext = MakeShared<FSharedRigRuntimeContext>();
			FScopeLock DNAScopeLock{&DNAUpdateSection};
			if (BehaviorReader.IsValid() && (BehaviorReader->GetJointCount() != 0))
			{
				NewContext->BehaviorReader = BehaviorReader;
				NewContext->RigLogic = MakeShared<FRigLogic>(BehaviorReader.Get());
				NewContext->CacheVariableJointIndices();
				RigRuntimeContext = NewContext;
#if !WITH_EDITOR
				if (Policy == EDNARetentionPolicy::Unload)
				{
					BehaviorReader->Unload(EDNADataLayer::Behavior);
					BehaviorReader->Unload(EDNADataLayer::Geometry);
				}
#endif  // !WITH_EDITOR
			}
		}
	}
	return RigRuntimeContext;
}

bool UDNAAsset::Init(const FString& DNAFilename)
{
	LLM_SCOPE_BYNAME(TEXT("Animation/RigLogic"));

	if (!rl4::Status::isOk())
	{
		UE_LOG(LogDNAAsset, Warning, TEXT("%s"), ANSI_TO_TCHAR(rl4::Status::get().message));
	}

	this->DNAFileName = DNAFilename; //memorize for re-import
	
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
	
	FScopeLock ScopeLock{&DNAUpdateSection};

	// Load run-time data (behavior) from whole-DNA buffer into BehaviorReader
	BehaviorReader = ReadDNAFromBuffer(&TempFileBuffer, EDNADataLayer::Behavior, 0u); //0u = MaxLOD
	if (!BehaviorReader.IsValid())
	{
		return false;
	}

	InvalidateRigRuntimeContext();

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

	FScopeLock ScopeLock{&DNAUpdateSection};

	if (Ar.CustomVer(FDNAAssetCustomVersion::GUID) >= FDNAAssetCustomVersion::BeforeCustomVersionWasAdded)
	{
		if (Ar.IsLoading())
		{
			FArchiveMemoryStream BehaviorStream{&Ar};
			BehaviorReader = ReadDNAFromStream(&BehaviorStream, EDNADataLayer::Behavior, 0u); //0u = max LOD
			// Geometry data is always present (even if only as an empty placeholder), just so the uasset
			// format remains consistent between editor and non-editor builds
			FArchiveMemoryStream GeometryStream{&Ar};
			auto Reader = ReadDNAFromStream(&GeometryStream, EDNADataLayer::Geometry, 0u); //0u = max LOD
#if WITH_EDITORONLY_DATA
			// Geometry data is discarded unless in Editor
			GeometryReader = Reader;
#endif // #if WITH_EDITORONLY_DATA

			InvalidateRigRuntimeContext();
		}

		if (Ar.IsSaving())
		{
			TSharedPtr<IDNAReader> EmptyDNA = CreateEmptyDNA(AVG_EMPTY_SIZE);
			IDNAReader* BehaviorReaderPtr = (BehaviorReader.IsValid() ? static_cast<IDNAReader*>(BehaviorReader.Get()) : EmptyDNA.Get());
			FArchiveMemoryStream BehaviorStream{&Ar};
			WriteDNAToStream(BehaviorReaderPtr, EDNADataLayer::Behavior, &BehaviorStream);

			// When cooking (or when there was no Geometry data available), an empty DNA structure is written
			// into the stream, serving as a placeholder just so uasset files can be conveniently loaded
			// regardless if they were cooked or prepared for in-editor work
			IDNAReader* GeometryReaderPtr = (GeometryReader.IsValid() && !Ar.IsCooking() ? static_cast<IDNAReader*>(GeometryReader.Get()) : EmptyDNA.Get());
			FArchiveMemoryStream GeometryStream{&Ar};
			WriteDNAToStream(GeometryReaderPtr, EDNADataLayer::Geometry, &GeometryStream);
		}
	}
}

