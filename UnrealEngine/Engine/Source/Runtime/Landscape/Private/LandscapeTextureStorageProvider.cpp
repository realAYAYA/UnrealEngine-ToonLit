// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
LandscapeTextureStorageProvider.cpp: Alternative Texture Storage for Landscape Textures
=============================================================================*/

#include "LandscapeTextureStorageProvider.h"
#include "LandscapeDataAccess.h"
#include "RHIGlobals.h"
#include "ContentStreaming.h"
#include "LandscapePrivate.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LandscapeTextureStorageProvider)

FLandscapeTextureStorageMipProvider::FLandscapeTextureStorageMipProvider(ULandscapeTextureStorageProviderFactory* InFactory)
	: FTextureMipDataProvider(InFactory->Texture, ETickState::Init, ETickThread::Async)
{
	this->Factory = InFactory;

	if (InFactory->Texture)
	{
		TextureName = InFactory->Texture->GetFName();
	}
}

FLandscapeTextureStorageMipProvider::~FLandscapeTextureStorageMipProvider()
{
}

ULandscapeTextureStorageProviderFactory::ULandscapeTextureStorageProviderFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void FLandscapeTexture2DMipMap::Serialize(FArchive& Ar, UObject* Owner, uint32 SaveOverrideFlags)
{
	Ar << SizeX;
	Ar << SizeY;
	Ar << bCompressed;
	BulkData.SerializeWithFlags(Ar, Owner, SaveOverrideFlags);
}

template<typename T, typename F>
static bool SerializeArray(FArchive& Ar, TArray<T>& Array, F&& SerializeElementFn)
{
	int32 Num = Array.Num();
	Ar << Num;
	if (Ar.IsLoading())
	{
		if (Num < 0)
		{
			return false;
		}
		else
		{
			Array.SetNum(Num);
			for (int32 Index = 0; Index < Num; ++Index)
			{
				SerializeElementFn(Ar, Index, Array[Index]);
			}
		}
	}
	else
	{
		for (int32 Index = 0; Index < Num; ++Index)
		{
			SerializeElementFn(Ar, Index, Array[Index]);
		}
	}
	return true;
}

void ULandscapeTextureStorageProviderFactory::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	//  mip 0                                                      mip N
	// 	high rez <---------------------------------------------> low rez
	// 	[ Optional Mips ][            Non Optional Mips                ]
	// 	                 [ Streaming Mips ][ Non Streaming Inline Mips ]

	int32 OptionalMips = Mips.Num() - NumNonOptionalMips;
	check(OptionalMips >= 0);

	int32 FirstInlineMip = Mips.Num() - NumNonStreamingMips;
	check(FirstInlineMip >= 0);

	Ar << NumNonOptionalMips;
	Ar << NumNonStreamingMips;
	Ar << LandscapeGridScale;

	SerializeArray(Ar, Mips,
		[this, OptionalMips, FirstInlineMip](FArchive& Ar, int32 Index, FLandscapeTexture2DMipMap& Mip)
		{
			// select bulk data flags for optional/streaming/inline mips
			uint32 BulkDataFlags;
			if (Index < OptionalMips)
			{
				// optional mip
				BulkDataFlags = BULKDATA_Force_NOT_InlinePayload | BULKDATA_OptionalPayload;
			}
			else if (Index < FirstInlineMip)
			{
				// streaming mip
				bool bDuplicateNonOptionalMips = false; // TODO [chris.tchou] : if we add support for optional mips, we might need to calculate this.
				BulkDataFlags = BULKDATA_Force_NOT_InlinePayload | (bDuplicateNonOptionalMips ? BULKDATA_DuplicateNonOptionalPayload : 0);
			}
			else
			{
				// non streaming inline mip (can be single use as we only need to upload to GPU once, are never streamed out)
				BulkDataFlags = BULKDATA_ForceInlinePayload | BULKDATA_SingleUse;
			}
			Mip.Serialize(Ar, this, BulkDataFlags);
		});

	Ar << Texture;
}

FStreamableRenderResourceState ULandscapeTextureStorageProviderFactory::GetResourcePostInitState(const UTexture* Owner, bool bAllowStreaming)
{
	// We are using the non-offline mode to upload these textures currently, so we don't need to consider mip tails.
	// (RHI will handle it during upload, just less optimal than having them pre-packed)
	// If we ever want to optimize the GPU upload by using the offline mode, we can add the logic here to take mip tails into account.
	const int32 PlatformNumMipsInTail = 1;

	const int32 TotalMips = Mips.Num();
	const int32 ExpectedAssetLODBias = FMath::Clamp<int32>(Owner->GetCachedLODBias() - Owner->NumCinematicMipLevels, 0, TotalMips - 1);
	const int32 MaxRuntimeMipCount = FMath::Min<int32>(GMaxTextureMipCount, FStreamableRenderResourceState::MAX_LOD_COUNT);

	const int32 NumMips = FMath::Min<int32>(TotalMips - ExpectedAssetLODBias, MaxRuntimeMipCount);

	bool bTextureIsStreamable = true;		// landscape texture storage is always streamable (we should not use it for platforms that are not)

	// clamp non-optional and non-streaming mips to reflect potentially reduced mip count because of bias
	const int32 BiasedNumNonOptionalMips = FMath::Min<int32>(NumMips, NumNonOptionalMips);
	const int32 NumOfNonStreamingMips = FMath::Min<int32>(NumMips, NumNonStreamingMips);

	// Optional mips must be streaming mips :
	check(BiasedNumNonOptionalMips >= NumOfNonStreamingMips);

	if (NumOfNonStreamingMips == NumMips)
	{
		bTextureIsStreamable = false;
	}

	const int32 AssetMipIdxForResourceFirstMip = FMath::Max<int32>(0, TotalMips - NumMips);

	const bool bMakeStreamable = bAllowStreaming;
	int32 NumRequestedMips = 0;
	if (!bTextureIsStreamable)
	{
		// in Editor , NumOfNonStreamingMips may not be all mips
		// but once we cook it will be
		// so check this early to make behavior consistent
		NumRequestedMips = NumMips;
	}
	else if (bMakeStreamable && IStreamingManager::Get().IsRenderAssetStreamingEnabled(EStreamableRenderAssetType::Texture))
	{
		NumRequestedMips = NumOfNonStreamingMips;
	}
	else
	{
		// we are not streaming (bMakeStreamable is false)
		// but this may select a mip below the top mip
		// (due to cinematic lod bias)
		// but only if the texture itself is streamable

		// Adjust CachedLODBias so that it takes into account FStreamableRenderResourceState::AssetLODBias.
		const int32 ResourceLODBias = FMath::Max<int32>(0, Owner->GetCachedLODBias() - AssetMipIdxForResourceFirstMip);

		// Ensure NumMipsInTail is within valid range to safeguard on the above expressions. 
		const int32 NumMipsInTail = FMath::Clamp<int32>(PlatformNumMipsInTail, 1, NumMips);

		// Bias is not allowed to shrink the mip count below NumMipsInTail.
		NumRequestedMips = FMath::Max<int32>(NumMips - ResourceLODBias, NumMipsInTail);

		// If trying to load optional mips, check if the first resource mip is available.
		if (NumRequestedMips > BiasedNumNonOptionalMips && !DoesMipDataExist(AssetMipIdxForResourceFirstMip))
		{
			NumRequestedMips = BiasedNumNonOptionalMips;
		}

		// Ensure we don't request a top mip in the NonStreamingMips
		NumRequestedMips = FMath::Max(NumRequestedMips, NumOfNonStreamingMips);
	}

	const int32 MinRequestMipCount = 0;
	if (NumRequestedMips < MinRequestMipCount && MinRequestMipCount < NumMips)
	{
		NumRequestedMips = MinRequestMipCount;
	}

	FStreamableRenderResourceState PostInitState;
	PostInitState.bSupportsStreaming = bMakeStreamable;
	PostInitState.NumNonStreamingLODs = IntCastChecked<uint8>(NumOfNonStreamingMips);
	PostInitState.NumNonOptionalLODs = IntCastChecked<uint8>(BiasedNumNonOptionalMips);
	PostInitState.MaxNumLODs = IntCastChecked<uint8>(NumMips);
	PostInitState.AssetLODBias = IntCastChecked<uint8>(AssetMipIdxForResourceFirstMip);
	PostInitState.NumResidentLODs = IntCastChecked<uint8>(NumRequestedMips);
	PostInitState.NumRequestedLODs = IntCastChecked<uint8>(NumRequestedMips);

	return PostInitState;
}

bool ULandscapeTextureStorageProviderFactory::GetInitialMipData(int32 FirstMipToLoad, TArrayView<void*> OutMipData, TArrayView<int64> OutMipSize, FStringView DebugContext)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ULandscapeTextureStorageProviderFactory::GetInitialMipData);
	check(FirstMipToLoad >= 0);
	int32 NumberOfMipsToLoad = OutMipData.Num();
	check(NumberOfMipsToLoad > 0);
	check(OutMipData.GetData());

	const int32 LoadableMips = Mips.Num();
	check(NumberOfMipsToLoad == LoadableMips - FirstMipToLoad);

	const int32 MipLoadEnd = FirstMipToLoad + NumberOfMipsToLoad;
	check(MipLoadEnd <= LoadableMips);

	check(OutMipSize.Num() == NumberOfMipsToLoad || OutMipSize.Num() == 0);

	int32 NumMipsCached = 0;

	// Handle the case where we inlined more mips than we intend to upload immediately, by discarding the unneeded mips
	for (int32 MipIndex = 0; MipIndex < FirstMipToLoad && MipIndex < LoadableMips; ++MipIndex)
	{
		FLandscapeTexture2DMipMap& Mip = Mips[MipIndex];
		if (Mip.BulkData.IsBulkDataLoaded())
		{
			// we know inline mips are set up with the discard after first use flag, so simply locking then unlocking will cause them to be deleted
			Mip.BulkData.Lock(LOCK_READ_ONLY);
			Mip.BulkData.Unlock();
		}
	}

	// Get data for the remaining mips from bulk data.
	for (int32 MipIndex = FirstMipToLoad; MipIndex < MipLoadEnd; ++MipIndex)
	{
		FLandscapeTexture2DMipMap& Mip = Mips[MipIndex];
		const int64 DestBytes = Mip.SizeX * Mip.SizeY * 4;
		const int64 BulkDataSize = Mip.BulkData.GetBulkDataSize();
		if (BulkDataSize > 0)
		{
			void* SourceData = nullptr;
			bool bDiscardInternalCopy = true;
			Mip.BulkData.GetCopy(&SourceData, bDiscardInternalCopy);
			check(SourceData);
			uint8* DestData = static_cast<uint8*>(FMemory::Malloc(DestBytes));
			DecompressMip((uint8*) SourceData, BulkDataSize, DestData, DestBytes, MipIndex);
			OutMipData[MipIndex - FirstMipToLoad] = DestData;
			FMemory::Free(SourceData);

			if (OutMipSize.Num() > 0)
			{
				OutMipSize[MipIndex - FirstMipToLoad] = DestBytes;
			}
			NumMipsCached++;
		}
	}

	if (NumMipsCached != (LoadableMips - FirstMipToLoad))
	{
		UE_LOG(LogLandscape, Warning, TEXT("ULandscapeTextureStorageProviderFactory::TryLoadMips failed for %.*s, NumMipsCached: %d, LoadableMips: %d, FirstMipToLoad: %d"),
			DebugContext.Len(), DebugContext.GetData(),
			NumMipsCached,
			LoadableMips,
			FirstMipToLoad);

		// Unable to cache all mips. Release memory for those that were cached.
		for (int32 MipIndex = FirstMipToLoad; MipIndex < LoadableMips; ++MipIndex)
		{
			FLandscapeTexture2DMipMap& Mip = Mips[MipIndex];
			UE_LOG(LogLandscape, Verbose, TEXT("  Mip %d, BulkDataSize: %" INT64_FMT),
				MipIndex,
				Mip.BulkData.GetBulkDataSize());

			if (OutMipData[MipIndex - FirstMipToLoad])
			{
				FMemory::Free(OutMipData[MipIndex - FirstMipToLoad]);
				OutMipData[MipIndex - FirstMipToLoad] = nullptr;
			}
			if (OutMipSize.Num() > 0)
			{
				OutMipSize[MipIndex - FirstMipToLoad] = 0;
			}
		}
		return false;
	}
	return true;
}


#if WITH_EDITORONLY_DATA
ULandscapeTextureStorageProviderFactory* ULandscapeTextureStorageProviderFactory::ApplyTo(UTexture2D* TargetTexture, const FVector& InLandsapeGridScale)
{
	check(TargetTexture);
	check(TargetTexture->Source.IsValid())

	check(TargetTexture->Source.GetFormat() == TSF_BGRA8);
	EPixelFormat Format = PF_B8G8R8A8;

	int32 Width = TargetTexture->Source.GetSizeX();
	int32 Height = TargetTexture->Source.GetSizeY();
	int32 MipCount = TargetTexture->Source.GetNumMips();

	uint32 SrcBpp = GPixelFormats[Format].BlockBytes;
	uint32 SrcPitch = Width * SrcBpp;

	// try to get an existing factory
	ULandscapeTextureStorageProviderFactory* Factory= TargetTexture->GetAssetUserData<ULandscapeTextureStorageProviderFactory>();
	if (Factory == nullptr)
	{
		// create a new one
		Factory = NewObject<ULandscapeTextureStorageProviderFactory>(TargetTexture);
		Factory->Texture = TargetTexture;
		TargetTexture->AddAssetUserData(Factory);
	}

	Factory->LandscapeGridScale = InLandsapeGridScale;

	// calculate number of non-streaming mips
	// TODO [chris.tchou] : we could make this calculation platform specific, like Texture2D does.
	// We would have to calculate it during serialization, when we know the target platform.
	{
		int32 NumNonStreamingMips = 1;

		// TODO [chris.tchou] : we could ensure Mip Tails are not streamed, as it's more overhead to upload.
		// we would have to query TextureCompressorModule for platform specific info.
		// Ignoring the mip tail should still work, just less optimal as it does more work at runtime to blit into the mip tail.
		int32 NumMipsInTail = 0;

		NumNonStreamingMips = FMath::Max(NumNonStreamingMips, NumMipsInTail);
		NumNonStreamingMips = FMath::Max(NumNonStreamingMips, UTexture2D::GetStaticMinTextureResidentMipCount());
		NumNonStreamingMips = FMath::Min(NumNonStreamingMips, MipCount);
		Factory->NumNonStreamingMips = NumNonStreamingMips;
	}

	// calculate number of non-optional mips
	{
		// for now, landscape texture storage does not have any optional mips
		Factory->NumNonOptionalMips = MipCount;
	}

	Factory->Mips.Empty();
	int32 MipWidth = Width;
	int32 MipHeight = Height;
	for (int32 MipIndex = 0; MipIndex < MipCount; MipIndex++)
	{
		FLandscapeTexture2DMipMap* Mip = new(Factory->Mips) FLandscapeTexture2DMipMap();
		Mip->SizeX = MipWidth;
		Mip->SizeY = MipHeight;
		
		TArray64<uint8> MipData;
		TargetTexture->Source.GetMipData(MipData, MipIndex);

		// store small mips uncompressed
		constexpr int32 UncompressedMipSizeThreshold = 8;
		if ((MipWidth <= UncompressedMipSizeThreshold) || (MipHeight <= UncompressedMipSizeThreshold))
		{
			Mip->bCompressed = false;
			CopyMipToBulkData(MipIndex, MipWidth, MipHeight, MipData.GetData(), MipData.Num(), Mip->BulkData);
		}
		else
		{
			Mip->bCompressed = true;
			CompressMipToBulkData(MipIndex, MipWidth, MipHeight, MipData.GetData(), MipData.Num(), Mip->BulkData);
		}

		MipWidth >>= 1;
		MipHeight >>= 1;
	}

	return Factory;
}
#endif // WITH_EDITORONLY_DATA


// Helper to configure the AsyncFileCallBack.
void FLandscapeTextureStorageMipProvider::CreateAsyncFileCallback(const FTextureUpdateSyncOptions& SyncOptions)
{
	FThreadSafeCounter* Counter = SyncOptions.Counter;
	FTextureUpdateSyncOptions::FCallback RescheduleCallback = SyncOptions.RescheduleCallback;
	check(Counter && RescheduleCallback);

	AsyncFileCallBack = [this, Counter, RescheduleCallback](bool bWasCancelled, IBulkDataIORequest* Req)
	{
		// At this point task synchronization would hold the number of pending requests.
		Counter->Decrement();

		if (bWasCancelled)
		{
			bIORequestCancelled = true;
		}

		if (Counter->GetValue() == 0)
		{
			RescheduleCallback();
		}
	};
}

void FLandscapeTextureStorageMipProvider::ClearIORequests()
{
	for (FIORequest& IORequest : IORequests)
	{
		// If requests are not yet completed, cancel and wait.
		if (IORequest.BulkDataIORequest && !IORequest.BulkDataIORequest->PollCompletion())
		{
			IORequest.BulkDataIORequest->Cancel();
			IORequest.BulkDataIORequest->WaitCompletion();
		}
	}
	IORequests.Empty();
}

void FLandscapeTextureStorageMipProvider::Init(const FTextureUpdateContext& Context, const FTextureUpdateSyncOptions& SyncOptions)
{
	IORequests.AddDefaulted(CurrentFirstLODIdx);

	// If this resource has optional LODs and we are streaming one of them.
	if (ResourceState.NumNonOptionalLODs < ResourceState.MaxNumLODs && PendingFirstLODIdx < ResourceState.LODCountToFirstLODIdx(ResourceState.NumNonOptionalLODs))
	{
		// Generate the FilenameHash of each optional LOD before the first one requested, so that we can handle properly PAK unmount events.
		// Note that streamer only stores the hash for the first optional mip.
		for (int32 MipIdx = 0; MipIdx < PendingFirstLODIdx; ++MipIdx)
		{
			const FLandscapeTexture2DMipMap* SourceMip = Factory->GetMip(MipIdx);
			// const FTexture2DMipMap& OwnerMip = *Context.MipsView[MipIdx];
			IORequests[MipIdx].FilenameHash = SourceMip->BulkData.GetIoFilenameHash();
		}
	}

	// Otherwise validate each streamed in mip.
	for (int32 MipIdx = PendingFirstLODIdx; MipIdx < CurrentFirstLODIdx; ++MipIdx)
	{
		const FLandscapeTexture2DMipMap* SourceMip = Factory->GetMip(MipIdx);
		if (SourceMip->BulkData.IsStoredCompressedOnDisk())
		{
			// Compression at the package level is no longer supported
			continue;
		}
		else if (SourceMip->BulkData.GetBulkDataSize() <= 0)
		{
			// Invalid bulk data size.
			continue;
		}
		else
		{
			IORequests[MipIdx].FilenameHash = SourceMip->BulkData.GetIoFilenameHash();
		}
	}

	AdvanceTo(ETickState::GetMips, ETickThread::Async);
}

int32 FLandscapeTextureStorageMipProvider::GetMips(const FTextureUpdateContext& Context, int32 StartingMipIndex, const FTextureMipInfoArray& MipInfos, const FTextureUpdateSyncOptions& SyncOptions)\
{
	CreateAsyncFileCallback(SyncOptions);	// this just creates it... callback has to be passed to the IO request completion to actually get called...
	check(SyncOptions.Counter != nullptr);

	FirstRequestedMipIndex = StartingMipIndex;
	while (StartingMipIndex < CurrentFirstLODIdx && MipInfos.IsValidIndex(StartingMipIndex))
	{
		const FTextureMipInfo& DestMip = MipInfos[StartingMipIndex];
		const FLandscapeTexture2DMipMap* SourceMip = Factory->GetMip(StartingMipIndex);
		if (SourceMip == nullptr || !DestMip.DestData)
		{
			break;
		}

		// Check the validity of the filename.
		if (IORequests[StartingMipIndex].FilenameHash == INVALID_IO_FILENAME_HASH)
		{
			break;
		}

		// If Data size is specified, check compatibility for safety		// TODO: size doesn't have to match when compressed...
		if (DestMip.DataSize && (uint64)SourceMip->BulkData.GetBulkDataSize() > DestMip.DataSize)
		{
			break;
		}

		// Increment the sync counter.  This causes the system to not advance to the next tick, until RescheduleCallback() is called (by AsyncFileCallBack when counter reaches zero)
		// If a request completes immediately, then it will call the RescheduleCallback,
		// but that won't do anything because the tick would not try to acquire the lock since it is already locked.
		SyncOptions.Counter->Increment();

		int64 StreamDataSize = SourceMip->BulkData.GetBulkDataSize();
		uint8* StreamData = static_cast<uint8*>(FMemory::Malloc(StreamDataSize));
		IORequests[StartingMipIndex].BulkDataIORequest.Reset(
			SourceMip->BulkData.CreateStreamingRequest(
				0,
				StreamDataSize,
				(EAsyncIOPriorityAndFlags) FMath::Clamp<int32>(AIOP_Low + (bPrioritizedIORequest ? 1 : 0), AIOP_Low, AIOP_High) | AIOP_FLAG_DONTCACHE,
				&AsyncFileCallBack,
				StreamData)
		);

		// remember the dest mip data buffer (we can't fill it out now, must wait until streaming is complete)
		IORequests[StartingMipIndex].DestMipData = static_cast<uint8*>(DestMip.DestData);

		StartingMipIndex++;
	}

	AdvanceTo(ETickState::PollMips, ETickThread::Async);
	return StartingMipIndex;	// return the mips we handled (if this is not CurrentFirstLODIdx, it will fall back to other providers)
}

bool FLandscapeTextureStorageMipProvider::PollMips(const FTextureUpdateSyncOptions& SyncOptions)
{
	// poll mips will run once all io requests are complete (or cancelled)

	// Notify that some files have possibly been unmounted / missing.
	if (bIORequestCancelled && !bIORequestAborted)
	{
		IRenderAssetStreamingManager& StreamingManager = IStreamingManager::Get().GetRenderAssetStreamingManager();
		for (FIORequest& IORequest : IORequests)
		{
			StreamingManager.MarkMountedStateDirty(IORequest.FilenameHash);
		}
		UE_LOG(LogLandscape, Warning, TEXT("[%s] FLandscapeTextureStorageMipProvider Texture stream in request failed due to IO error (Mip %d-%d)."), *TextureName.ToString(), ResourceState.AssetLODBias + PendingFirstLODIdx, ResourceState.AssetLODBias + CurrentFirstLODIdx - 1);
	}

	if (!bIORequestCancelled && !bIORequestAborted)
	{
		// decompress the mips (note that this is using the dest mip data pointer we memorized during GetMips)
		for (int MipIndex = FirstRequestedMipIndex; MipIndex < CurrentFirstLODIdx; MipIndex++)
		{
			const FLandscapeTexture2DMipMap* SourceMip = Factory->GetMip(MipIndex);
			uint8* SourceData = IORequests[MipIndex].BulkDataIORequest->GetReadResults();
			int64 DestDataBytes = SourceMip->SizeX * SourceMip->SizeY * 4;
			uint8* DestData = IORequests[MipIndex].DestMipData;
			Factory->DecompressMip(SourceData, SourceMip->BulkData.GetBulkDataSize(), DestData, DestDataBytes, MipIndex);
		}
	}

	ClearIORequests();

	AdvanceTo(ETickState::Done, ETickThread::None);

	return !bIORequestCancelled;	// return true if successful and it can upload the DestMip data to the GPU
}

void FLandscapeTextureStorageMipProvider::AbortPollMips()
{
	// ... cancel all streaming ops in progress ...
	for (FIORequest& IORequest : IORequests)
	{
		if (IORequest.BulkDataIORequest)
		{
			// Calling cancel() here will trigger the AsyncFileCallBack and precipitate the execution of Cancel().
			IORequest.BulkDataIORequest->Cancel();
			bIORequestAborted = true;
		}
	}
}

void FLandscapeTextureStorageMipProvider::CleanUp(const FTextureUpdateSyncOptions& SyncOptions)
{
	AdvanceTo(ETickState::Done, ETickThread::None);
}

void FLandscapeTextureStorageMipProvider::Cancel(const FTextureUpdateSyncOptions& SyncOptions)
{
	ClearIORequests();
}

FTextureMipDataProvider::ETickThread FLandscapeTextureStorageMipProvider::GetCancelThread() const
{
	return IORequests.Num() ? FTextureMipDataProvider::ETickThread::Async : FTextureMipDataProvider::ETickThread::None;
}

void ULandscapeTextureStorageProviderFactory::CopyMipToBulkData(int32 MipIndex, int32 MipSizeX, int32 MipSizeY, uint8* SourceData, int32 SourceDataBytes, FByteBulkData& DestBulkData)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ULandscapeTextureStorageProviderFactory::CopyMipToBulkData);
	DestBulkData.Lock(LOCK_READ_WRITE);

	int32 TotalPixels = MipSizeX * MipSizeY;
	check(SourceDataBytes == TotalPixels * 4);

	int32 DestBytes = SourceDataBytes;
	uint8* DestData = DestBulkData.Realloc(DestBytes);

	memcpy(DestData, SourceData, DestBytes);

	DestBulkData.Unlock();
}

void ULandscapeTextureStorageProviderFactory::CompressMipToBulkData(int32 MipIndex, int32 MipSizeX, int32 MipSizeY, uint8* SourceData, int32 SourceDataBytes, FByteBulkData& DestBulkData)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ULandscapeTextureStorageProviderFactory::CompressMipToBulkData);

	DestBulkData.Lock(LOCK_READ_WRITE);

	int32 TotalPixels = MipSizeX * MipSizeY;
	check(SourceDataBytes == TotalPixels * 4);
	check(TotalPixels >= 16);	// shouldn't be used on very small mips

	// DestData consists of a 16 bit height per pixel, then an 8:8 normal per edge pixel
	int32 DestBytes = (TotalPixels + (MipSizeX + MipSizeY) * 2 - 4) * 2;
	uint8* DestData = DestBulkData.Realloc(DestBytes);

	// delta encode the heights -- this (usually) greatly reduces the variance in the data, which makes it compress much better on disk when package compression is applied.
	uint16 LastHeight = 32768;
	int32 DestOffset = 0;
	for (int32 SourceOffset = 0; SourceOffset < SourceDataBytes; SourceOffset += 4)
	{
		// texture data is stored as BGRA, or [normal x, height low bits, height high bits, normal y]
		uint16 Height = SourceData[SourceOffset + 2] * 256 + SourceData[SourceOffset + 1];
		uint16 DeltaHeight = Height - LastHeight;
		LastHeight = Height;

		// store delta height
		DestData[DestOffset + 0] = DeltaHeight >> 8;
		DestData[DestOffset + 1] = DeltaHeight & 0xff;
		DestOffset += 2;
	}

	int32 DeltaCount = DestOffset;

	// capture normals along the edge (delta encoded clockwise starting from top left)
	uint8 LastNormalX = 128;
	uint8 LastNormalY = 128;

	auto EncodeNormal = [&LastNormalX, &LastNormalY, SourceData, DestData, MipSizeX, &DestOffset](int32 X, int32 Y)
	{
		int32 SourceOffset = (Y * MipSizeX + X) * 4;
		uint8 NormalX = SourceData[SourceOffset + 0];
		uint8 NormalY = SourceData[SourceOffset + 3];
		DestData[DestOffset + 0] = NormalX - LastNormalX;
		DestData[DestOffset + 1] = NormalY - LastNormalY;
		LastNormalX = NormalX;
		LastNormalY = NormalY;
		DestOffset += 2;
	};

	for (int32 X = 0; X < MipSizeX; X++)				// [0 ... MipSizeX-1], 0
	{
		EncodeNormal(X, 0);
	}

	for (int32 Y = 1; Y < MipSizeY; Y++)				// MipSizeX-1, [1 ... MipSizeY-1]
	{
		EncodeNormal(MipSizeX - 1, Y);
	}

	for (int32 X = MipSizeX-2; X >= 0; X--)				// [MipSizeX-2 ... 0], MipSizeY-1
	{
		EncodeNormal(X, MipSizeY - 1);
	}

	for (int32 Y = MipSizeY-2; Y >= 1; Y--)				// 0, [MipSizeY-2 ... 1]
	{
		EncodeNormal(0, Y);
	}

	check(DestOffset == DestBytes);

	DestBulkData.Unlock();
}

// Compute the normal of the triangle formed by the 3 points (in winding order).
static FVector ComputeTriangleNormal(const FVector& InPoint0, const FVector& InPoint1, const FVector& InPoint2)
{
	FVector Normal = (InPoint0 - InPoint1).Cross(InPoint1 - InPoint2);
	Normal.Normalize();
	return Normal;
}

static void SampleWorldPositionAtOffset(FVector& OutPoint, const uint8* MipData, int32 X, int32 Y, int32 MipSizeX, const FVector& InLandscapeGridScale)
{
	int32 OffsetBytes = (Y * MipSizeX + X) * 4;
	uint16 HeightData = MipData[OffsetBytes + 2] * 256 + MipData[OffsetBytes + 1];

	// NOTE: since we are using deltas between points to calculate the normal, we don't care about constant offsets in the position, only relative scales
	OutPoint.Set(
		X * InLandscapeGridScale.X,
		Y * InLandscapeGridScale.Y,
		LandscapeDataAccess::GetLocalHeight(HeightData) * InLandscapeGridScale.Z);
}

void ULandscapeTextureStorageProviderFactory::DecompressMip(uint8* SourceData, int64 SourceDataBytes, uint8* DestData, int64 DestDataBytes, int32 MipIndex)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ULandscapeTextureStorageProviderFactory::DecompressMip);

	check(SourceData && DestData);

	FLandscapeTexture2DMipMap& Mip = Mips[MipIndex];

	if (!Mip.bCompressed)
	{
		// mip is uncompressed, just copy it
		check(SourceDataBytes == DestDataBytes);
		memcpy(DestData, SourceData, DestDataBytes);
		return;
	}

	check(Mip.bCompressed);

	int32 TotalPixels = Mip.SizeX * Mip.SizeY;
	check(SourceDataBytes == (TotalPixels + (Mip.SizeX + Mip.SizeY) * 2 - 4) * 2);
	check(DestDataBytes == TotalPixels * 4);

	// Undo Delta Encode of Heights
	uint16 LastHeight = 32768;
	for (int32 PixelIndex = 0; PixelIndex < TotalPixels; PixelIndex++)
	{
		int32 SourceOffset = PixelIndex * 2;
		uint16 DeltaHeight = SourceData[SourceOffset + 0] * 256 + SourceData[SourceOffset + 1];

		// undo delta
		LastHeight += DeltaHeight;

		// texture data is stored as BGRA, or [normal x, height low bits, height high bits, normal y]
		int32 DestOffset = PixelIndex * 4;
		DestData[DestOffset + 0] = 128;
		DestData[DestOffset + 1] = LastHeight & 0xff;
		DestData[DestOffset + 2] = LastHeight >> 8;
		DestData[DestOffset + 3] = 128;
	}

	// recompute normals in the interior
	{
		// we skip computing the edges, as they will be overwritten later (and this way we don't have to handle samples that go off the edge)
		for (int32 Y = 1; Y < Mip.SizeY - 1; Y++)
		{
			for (int32 X = 1; X < Mip.SizeX - 1; X++)
			{
				// based on shader code in LandscapeLayersPS.usf
				FVector TL, TT, CC, LL, RR, BR, BB;

				SampleWorldPositionAtOffset(TL, DestData, X - 1, Y - 1, Mip.SizeX, LandscapeGridScale);
				SampleWorldPositionAtOffset(TT, DestData, X + 0, Y - 1, Mip.SizeX, LandscapeGridScale);
				SampleWorldPositionAtOffset(CC, DestData, X + 0, Y + 0, Mip.SizeX, LandscapeGridScale);
				SampleWorldPositionAtOffset(LL, DestData, X - 1, Y + 0, Mip.SizeX, LandscapeGridScale);
				SampleWorldPositionAtOffset(RR, DestData, X + 1, Y + 0, Mip.SizeX, LandscapeGridScale);
				SampleWorldPositionAtOffset(BR, DestData, X + 1, Y + 1, Mip.SizeX, LandscapeGridScale);
				SampleWorldPositionAtOffset(BB, DestData, X + 0, Y + 1, Mip.SizeX, LandscapeGridScale);

				FVector N0 = ComputeTriangleNormal(CC, LL, TL);
				FVector N1 = ComputeTriangleNormal(TL, TT, CC);
				FVector N2 = ComputeTriangleNormal(LL, CC, BB);
				FVector N3 = ComputeTriangleNormal(RR, CC, TT);
				FVector N4 = ComputeTriangleNormal(BR, BB, CC);
				FVector N5 = ComputeTriangleNormal(CC, RR, BR);

				FVector FinalNormal = (N0 + N1 + N2 + N3 + N4 + N5);
				FinalNormal.Normalize();

				// rescale normal.xy to [0,255] range, and write out as bytes
				int32 OffsetBytes = (Y * Mip.SizeX + X) * 4;
				DestData[OffsetBytes + 0] = static_cast<uint8>(FMath::Clamp(((FinalNormal.X + 1.0) * 0.5) * 255.0, 0.0, 255.0));
				DestData[OffsetBytes + 3] = static_cast<uint8>(FMath::Clamp(((FinalNormal.Y + 1.0) * 0.5) * 255.0, 0.0, 255.0));
			}
		}
	}

	// write out normals along the edge (delta encoded clockwise starting from top left)
	int32 SourceOffset = TotalPixels * 2;
	uint8 LastNormalX = 128;
	uint8 LastNormalY = 128;

	auto DecodeNormal = [&LastNormalX, &LastNormalY, SourceData, DestData, MipSizeX = Mip.SizeX, &SourceOffset](int32 X, int32 Y)
	{
		int32 DestOffset = (Y * MipSizeX + X) * 4;
		LastNormalX += SourceData[SourceOffset + 0];
		LastNormalY += SourceData[SourceOffset + 1];
		DestData[DestOffset + 0] = LastNormalX;
		DestData[DestOffset + 3] = LastNormalY;
		SourceOffset += 2;
	};

	for (int32 X = 0; X < Mip.SizeX; X++)				// [0 ... MipSizeX-1], 0
	{
		DecodeNormal(X, 0);
	}

	for (int32 Y = 1; Y < Mip.SizeY; Y++)				// MipSizeX-1, [1 ... MipSizeY-1]
	{
		DecodeNormal(Mip.SizeX - 1, Y);
	}

	for (int32 X = Mip.SizeX - 2; X >= 0; X--)				// [MipSizeX-2 ... 0], MipSizeY-1
	{
		DecodeNormal(X, Mip.SizeY - 1);
	}

	for (int32 Y = Mip.SizeY - 2; Y >= 1; Y--)				// 0, [MipSizeY-2 ... 1]
	{
		DecodeNormal(0, Y);
	}

	check(SourceOffset == SourceDataBytes);
}
