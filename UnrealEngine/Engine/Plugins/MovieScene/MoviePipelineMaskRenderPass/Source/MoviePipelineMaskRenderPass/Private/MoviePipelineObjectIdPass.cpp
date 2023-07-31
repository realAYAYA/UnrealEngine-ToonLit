// Copyright Epic Games, Inc. All Rights Reserved.

#include "MoviePipelineObjectIdPass.h"
#include "MoviePipeline.h"
#include "MovieRenderPipelineCoreModule.h"
#include "MovieRenderOverlappedImage.h"
#include "MoviePipelineOutputBuilder.h"
#include "Engine/TextureRenderTarget.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HitProxies.h"
#include "EngineUtils.h"
#include "Containers/HashTable.h"
#include "Misc/CString.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "MoviePipelineHashUtils.h"
#include "EngineModule.h"
#include "Async/ParallelFor.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "UObject/UObjectAnnotation.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MoviePipelineObjectIdPass)

DECLARE_CYCLE_STAT(TEXT("STAT_MoviePipeline_AccumulateMaskSample_TT"), STAT_AccumulateMaskSample_TaskThread, STATGROUP_MoviePipeline);
namespace UE
{
namespace MoviePipeline
{
	struct FMoviePipelineHitProxyCacheKey
	{
		const AActor* Actor;
		const UPrimitiveComponent* PrimComponent;

		FMoviePipelineHitProxyCacheKey(const AActor* InActor, const UPrimitiveComponent* InComponent)
			: Actor(InActor)
			, PrimComponent(InComponent)
		{}

		friend inline uint32 GetTypeHash(const FMoviePipelineHitProxyCacheKey& Key)
		{
			return HashCombine(PointerHash(Key.Actor), PointerHash(Key.PrimComponent));
		}

		bool operator==(const FMoviePipelineHitProxyCacheKey& Other) const
		{
			return (Actor == Other.Actor) && (PrimComponent == Other.PrimComponent);
		}
	};

	struct FMoviePipelineHitProxyCacheValue
	{
		const AActor* Actor;
		const UPrimitiveComponent* PrimComponent;
		int32 SectionIndex;
		int32 MaterialIndex;
		float Hash;
		FString HashAsString;
		FString ProxyName;
	};

	struct FObjectIdAccelerationData
	{
		FObjectIdAccelerationData()
		{}

		FORCEINLINE bool IsDefault()
		{
			return !Cache.IsValid()
				&& !JsonManifest.IsValid()
				&& JsonManifestCachedOutput.Len() == 0
				&& PassIdentifierHashAsShortString.Len() == 0;
		}

		TSharedPtr<TMap<int32, FMoviePipelineHitProxyCacheValue>> Cache;

		// Json Manifest Object
		TSharedPtr<FJsonObject> JsonManifest;

		// Cached version of the serialized Json Manifest
		FString JsonManifestCachedOutput;

		FString PassIdentifierHashAsShortString;
	};

	struct FObjectIdMaskSampleAccumulationArgs
	{
	public:
		TSharedPtr<FMaskOverlappedAccumulator, ESPMode::ThreadSafe> Accumulator;
		TSharedPtr<FMoviePipelineOutputMerger, ESPMode::ThreadSafe> OutputMerger;
		int32 NumOutputLayers;
		TSharedPtr<TMap<int32, FMoviePipelineHitProxyCacheValue>> CacheData;
	};
}
}


static FUObjectAnnotationSparse<UE::MoviePipeline::FObjectIdAccelerationData, true> ManifestAnnotation;

// Forward Declare
namespace MoviePipeline
{
	static void AccumulateSampleObjectId_TaskThread(TUniquePtr<FImagePixelData>&& InPixelData, const UE::MoviePipeline::FObjectIdMaskSampleAccumulationArgs& InParams);
}
extern const TSparseArray<HHitProxy*>& GetAllHitProxies();


UMoviePipelineObjectIdRenderPass::UMoviePipelineObjectIdRenderPass()
	: UMoviePipelineImagePassBase()
	, IdType(EMoviePipelineObjectIdPassIdType::Full)
{
	PassIdentifier = FMoviePipelinePassIdentifier("ActorHitProxyMask");

	// We output three layers which is 6 total influences per pixel.
	for (int32 Index = 0; Index < 3; Index++)
	{
		ExpectedPassIdentifiers.Add(FMoviePipelinePassIdentifier(PassIdentifier.Name + FString::Printf(TEXT("%02d"), Index)));
	}
}

TWeakObjectPtr<UTextureRenderTarget2D> UMoviePipelineObjectIdRenderPass::CreateViewRenderTargetImpl(const FIntPoint& InSize, IViewCalcPayload* OptPayload) const
{
	// Crete the render target with the correct bit depth.
	TWeakObjectPtr<UTextureRenderTarget2D> TileRenderTarget = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
	TileRenderTarget->ClearColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);

	// Ensure there's no gamma in the RT otherwise the HitProxy color ids don't round trip properly.
	TileRenderTarget->TargetGamma = 0.f;
	TileRenderTarget->InitCustomFormat(InSize.X, InSize.Y, EPixelFormat::PF_B8G8R8A8, true);
	TileRenderTarget->AddToRoot();

	return TileRenderTarget;
}

TSharedPtr<FMoviePipelineSurfaceQueue, ESPMode::ThreadSafe> UMoviePipelineObjectIdRenderPass::CreateSurfaceQueueImpl(const FIntPoint& InSize, IViewCalcPayload* OptPayload) const
{
	TSharedPtr<FMoviePipelineSurfaceQueue, ESPMode::ThreadSafe> SurfaceQueue = MakeShared<FMoviePipelineSurfaceQueue, ESPMode::ThreadSafe>(InSize, EPixelFormat::PF_B8G8R8A8, 3, false);

	return SurfaceQueue;
}

void UMoviePipelineObjectIdRenderPass::GatherOutputPassesImpl(TArray<FMoviePipelinePassIdentifier>& ExpectedRenderPasses)
{
	// Don't call the super which adds the generic PassIdentifier, which in this case is numberless and incorrect for the final output spec.
	// Super::GatherOutputPassesImpl(ExpectedRenderPasses);
	ExpectedRenderPasses.Append(ExpectedPassIdentifiers);
}

void UMoviePipelineObjectIdRenderPass::SetupImpl(const MoviePipeline::FMoviePipelineRenderPassInitSettings& InPassInitSettings)
{
	Super::SetupImpl(InPassInitSettings);

	// Re-initialize the render target and surface queue
	GetOrCreateViewRenderTarget(InPassInitSettings.BackbufferResolution);
	GetOrCreateSurfaceQueue(InPassInitSettings.BackbufferResolution);

	AccumulatorPool = MakeShared<TAccumulatorPool<FMaskOverlappedAccumulator>, ESPMode::ThreadSafe>(6);

	UE::MoviePipeline::FObjectIdAccelerationData AccelData = UE::MoviePipeline::FObjectIdAccelerationData();

	// Static metadata needed for Cryptomatte
	uint32 NameHash = MoviePipeline::HashNameToId(TCHAR_TO_UTF8(*PassIdentifier.Name));
	FString PassIdentifierHashAsShortString = FString::Printf(TEXT("%08x"), NameHash);
	PassIdentifierHashAsShortString.LeftInline(7);

	AccelData.PassIdentifierHashAsShortString = PassIdentifierHashAsShortString;

	
	AccelData.JsonManifest = MakeShared<FJsonObject>();

	AccelData.Cache = MakeShared<TMap<int32, UE::MoviePipeline::FMoviePipelineHitProxyCacheValue>>();
	AccelData.Cache->Reserve(1000);

	{
		// Add our default to the manifest.
		static const uint32 DefaultHash = MoviePipeline::HashNameToId(TCHAR_TO_UTF8(TEXT("default")));
		AccelData.JsonManifest->SetStringField(TEXT("default"), FString::Printf(TEXT("%08x"), DefaultHash));
	}
	ManifestAnnotation.AddAnnotation(this, AccelData);
}

void UMoviePipelineObjectIdRenderPass::TeardownImpl()
{
	ManifestAnnotation.RemoveAnnotation(this);

	// Preserve our view state until the rendering thread has been flushed.
	Super::TeardownImpl();
}


void UMoviePipelineObjectIdRenderPass::GetViewShowFlags(FEngineShowFlags& OutShowFlag, EViewModeIndex& OutViewModeIndex) const 
{
	OutShowFlag = FEngineShowFlags(EShowFlagInitMode::ESFIM_Game);
	OutShowFlag.DisableAdvancedFeatures();
	OutShowFlag.SetPostProcessing(false);
	OutShowFlag.SetPostProcessMaterial(false);

	// Screen-percentage scaling mixes IDs when doing downsampling, so it is disabled.
	OutShowFlag.SetScreenPercentage(false);
	OutShowFlag.SetHitProxies(true);
	OutViewModeIndex = EViewModeIndex::VMI_Unlit;
}

FString UMoviePipelineObjectIdRenderPass::ResolveProxyIdGroup(const AActor* InActor, const UPrimitiveComponent* InPrimComponent, const int32 InMaterialIndex, const int32 InSectionIndex) const
{
	// If it doesn't exist in the cache already, then we will do the somewhat expensive of building the string and hashing it.
	TStringBuilder<128> StringBuilder;

	FName FolderPath = InActor->GetFolderPath();

	// If they don't want the hierarchy, we'll just set this to empty string.
	if (IdType == EMoviePipelineObjectIdPassIdType::Actor)
	{
		FolderPath = NAME_None;
	}

	switch (IdType)
	{
	case EMoviePipelineObjectIdPassIdType::Layer:
	{
		if (InActor->Layers.Num() > 0)
		{
			StringBuilder.Append(*InActor->Layers[0].ToString());
		}
		break;
	}
	case EMoviePipelineObjectIdPassIdType::Folder:
	{
		if (!FolderPath.IsNone())
		{
			StringBuilder.Append(*FolderPath.ToString());
		}
		break;
	}

	case EMoviePipelineObjectIdPassIdType::Material:
	{
		if (InPrimComponent->GetNumMaterials() > 0)
		{
			UMaterialInterface* MaterialInterface = InPrimComponent->GetMaterial(FMath::Clamp(InMaterialIndex, 0, InPrimComponent->GetNumMaterials() - 1));

			// This collapses dynamic material instances back into their parent asset so we don't end up with 'MaterialInstanceDynamic_1' instead of MI_Foo
			if (UMaterialInstanceDynamic* AsDynamicMaterialInstance = Cast<UMaterialInstanceDynamic>(MaterialInterface))
			{
				if (AsDynamicMaterialInstance->Parent)
				{
					StringBuilder.Append(*AsDynamicMaterialInstance->Parent->GetName());
				}
				else
				{
					StringBuilder.Append(*AsDynamicMaterialInstance->GetName());
				}
			}
			else if (UMaterialInstance* AsMaterialInstance = Cast<UMaterialInstance>(MaterialInterface))
			{
				StringBuilder.Append(*MaterialInterface->GetName());
			}
			else if (MaterialInterface && MaterialInterface->GetMaterial())
			{
				StringBuilder.Append(*MaterialInterface->GetMaterial()->GetName());
			}
		}
		break;
	}
	case EMoviePipelineObjectIdPassIdType::Actor:
	case EMoviePipelineObjectIdPassIdType::ActorWithHierarchy:
	{
		// Folder Path will be NAME_None for root objects and for the "Actor" group type.
		if (!FolderPath.IsNone())
		{
			StringBuilder.Append(*FolderPath.ToString());
			StringBuilder.Append(TEXT("/"));
		}
		StringBuilder.Append(*InActor->GetActorLabel());
		break;
	}
	case EMoviePipelineObjectIdPassIdType::Full:
	{
		// Full gives as much detail as we can - per folder, per actor, per component, per material
		if (!FolderPath.IsNone())
		{
			StringBuilder.Append(*FolderPath.ToString());
			StringBuilder.Append(TEXT("/"));
		}
		StringBuilder.Appendf(TEXT("%s.%s[%d.%d]"), *InActor->GetActorLabel(), *GetNameSafe(InPrimComponent), InMaterialIndex, InSectionIndex);
		break;
	}
	}

	if (StringBuilder.Len() == 0)
	{
		StringBuilder.Append(TEXT("default"));
	}

	return StringBuilder.ToString();
}

void UMoviePipelineObjectIdRenderPass::RenderSample_GameThreadImpl(const FMoviePipelineRenderPassMetrics& InSampleState)
{
	// Object Ids have no history buffer so no need to render when we're going to discard.
	if (InSampleState.bDiscardResult)
	{
		return;
	}

	// Wait for a surface to be available to write to. This will stall the game thread while the RHI/Render Thread catch up.
	Super::RenderSample_GameThreadImpl(InSampleState);

	FMoviePipelineRenderPassMetrics InOutSampleState = InSampleState;

	TSharedPtr<FSceneViewFamilyContext> ViewFamily = CalculateViewFamily(InOutSampleState);
	
	// Submit to be rendered. Main render pass always uses target 0. We do this before making the Hitproxy cache because
	// BeginRenderingViewFamily ensures render state for things are created.
	TWeakObjectPtr<UTextureRenderTarget2D> ViewRenderTarget = GetOrCreateViewRenderTarget(InSampleState.BackbufferSize);
	check(ViewRenderTarget.IsValid());

	FRenderTarget* RenderTarget = ViewRenderTarget->GameThread_GetRenderTargetResource();
	check(RenderTarget);

	FCanvas Canvas = FCanvas(RenderTarget, nullptr, GetPipeline()->GetWorld(), ViewFamily->GetFeatureLevel(), FCanvas::CDM_DeferDrawing, 1.0f);
	GetRendererModule().BeginRenderingViewFamily(&Canvas, ViewFamily.Get());

	// The Hitproxy array gets invalidated quite often, so the results are no longer valid in the accumulation thread.
	// To solve this, we will cache the required info on the game thread and pass the required info along with the render so that
	// it stays in sync with what was actually rendered. Additionally, we cache the hashes between frames as they will be largely 
	// the same between each frame.
	const TSparseArray<HHitProxy*>& AllHitProxies = GetAllHitProxies();

	std::atomic<int32> NumCacheHits(0);
	std::atomic<int32> NumCacheMisses(0);
	std::atomic<int32> NumCacheUpdates(0);

	// Update the data in place, no need to copy back to the annotation.
	UE::MoviePipeline::FObjectIdAccelerationData AccelData = ManifestAnnotation.GetAnnotation(this);
	const double CacheStartTime = FPlatformTime::Seconds();

	for (typename TSparseArray<HHitProxy*>::TConstIterator It(AllHitProxies); It; ++It)
	{
		HActor* ActorHitProxy = HitProxyCast<HActor>(*It);
		HInstancedStaticMeshInstance* FoliageHitProxy = HitProxyCast<HInstancedStaticMeshInstance>(*It);

		const AActor* ProxyActor = nullptr;
		const UPrimitiveComponent* ProxyComponent = nullptr;
		int32 ProxySectionIndex = -1;
		int32 ProxyMaterialIndex = -1;

		if (ActorHitProxy && IsValid(ActorHitProxy->Actor) && IsValid(ActorHitProxy->PrimComponent))
		{
			ProxyActor = ActorHitProxy->Actor;
			ProxyComponent = ActorHitProxy->PrimComponent;
			ProxySectionIndex = ActorHitProxy->SectionIndex;
			ProxyMaterialIndex = ActorHitProxy->MaterialIndex;
		}
		else if (FoliageHitProxy && IsValid(FoliageHitProxy->Component))
		{
			ProxyActor = FoliageHitProxy->Component->GetOwner();
			ProxyComponent = FoliageHitProxy->Component;
			ProxySectionIndex = FoliageHitProxy->InstanceIndex;
		}

		if(ProxyActor && ProxyComponent)
		{
			// We assume names to be stable within a shot. This is technically incorrect if you were to 
			// rename an actor mid-frame but using this assumption allows us to skip calculating the string
			// name every frame.
			UE::MoviePipeline::FMoviePipelineHitProxyCacheValue* CacheEntry = nullptr;
			FColor Color = (*It)->Id.GetColor();
			int32 IdToInt = ((int32)Color.R << 16) | ((int32)Color.G << 8) | ((int32)Color.B << 0);
			{
				CacheEntry = AccelData.Cache->Find(IdToInt);
			}


			if (CacheEntry)
			{
				// The cache could be out of date since it's only an index. We'll double check that the actor and component
				// are the same and assume if they are, the cache is still valid.
				const bool bSameActor = CacheEntry->Actor == ProxyActor;
				const bool bSameComp = CacheEntry->PrimComponent == ProxyComponent;
				const bool bSameSection = CacheEntry->SectionIndex == ProxySectionIndex;
				const bool bSameMaterial = CacheEntry->MaterialIndex == ProxyMaterialIndex;

				if (bSameActor && bSameComp && bSameSection && bSameMaterial)
				{
					NumCacheHits++;
					continue;
				}
				NumCacheUpdates++;
			}
			NumCacheMisses++;

			FString ProxyIdName = ResolveProxyIdGroup(ProxyActor, ProxyComponent, ProxyMaterialIndex, ProxySectionIndex);
			
			// We hash the string and printf it here to reduce allocations later, even though it makes this loop ~% more expensive.
			uint32 Hash = MoviePipeline::HashNameToId(TCHAR_TO_UTF8(*ProxyIdName));
			FString HashAsString = FString::Printf(TEXT("%08x"), Hash);

			{
				UE::MoviePipeline::FMoviePipelineHitProxyCacheValue& NewCacheEntry = AccelData.Cache->Add(IdToInt);
				NewCacheEntry.ProxyName = ProxyIdName;
				NewCacheEntry.Hash = *(float*)(&Hash);
				NewCacheEntry.Actor = ProxyActor;
				NewCacheEntry.PrimComponent = ProxyComponent;
				NewCacheEntry.SectionIndex = ProxySectionIndex;
				NewCacheEntry.MaterialIndex = ProxyMaterialIndex;

				// Add the object to the manifest. Done here because this takes ~170ms a frame for 700 objects.
				// May as well only take that hit once per shot. This will add or update an existing field.
				AccelData.JsonManifest->SetStringField(ProxyIdName, HashAsString);

				// Only move it after we've used it to update the Json Manifest.
				NewCacheEntry.HashAsString = MoveTemp(HashAsString);
			}
		}
	}

	const double CacheEndTime = FPlatformTime::Seconds();
	const float ElapsedMs = float((CacheEndTime - CacheStartTime) * 1000.0f);

	//{
		const double JsonBeginTime = FPlatformTime::Seconds();

		// We only update the serialized manifest file if something has changed since it's slow.
		if (NumCacheMisses.load() > 0)
		{
			AccelData.JsonManifestCachedOutput.Empty();
			TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&AccelData.JsonManifestCachedOutput);
			FJsonSerializer::Serialize(AccelData.JsonManifest.ToSharedRef(), Writer);
		}

		FString PassIdentifierHashAsShortString = AccelData.PassIdentifierHashAsShortString;
		InOutSampleState.OutputState.FileMetadata.Add(FString::Printf(TEXT("cryptomatte/%s/manifest"), *PassIdentifierHashAsShortString), AccelData.JsonManifestCachedOutput);
		InOutSampleState.OutputState.FileMetadata.Add(FString::Printf(TEXT("cryptomatte/%s/name"), *PassIdentifierHashAsShortString), PassIdentifier.Name);
		InOutSampleState.OutputState.FileMetadata.Add(FString::Printf(TEXT("cryptomatte/%s/hash"), *PassIdentifierHashAsShortString), TEXT("MurmurHash3_32"));
		InOutSampleState.OutputState.FileMetadata.Add(FString::Printf(TEXT("cryptomatte/%s/conversion"), *PassIdentifierHashAsShortString), TEXT("uint32_to_float32"));
		const double JsonEndTime = FPlatformTime::Seconds();
		const float ElapsedJsonMs = float((JsonEndTime - JsonBeginTime) * 1000.0f);
	//}

	UE_LOG(LogMovieRenderPipeline, VeryVerbose, TEXT("Cache Size: %d NumCacheHits: %d NumCacheMisses: %d NumCacheUpdates: %d CacheDuration: %8.2fms JsonDuration: %8.2fms"), AccelData.Cache->Num(), NumCacheHits.load(), NumCacheMisses.load(), NumCacheUpdates.load(), ElapsedMs, ElapsedJsonMs);
	
	// Update the annotation with new cached data
	ManifestAnnotation.AddAnnotation(this, AccelData);

	// Main Render Pass
	{

		// Readback + Accumulate.
		TSharedRef<FImagePixelDataPayload, ESPMode::ThreadSafe> FramePayload = MakeShared<FImagePixelDataPayload, ESPMode::ThreadSafe>();
		FramePayload->PassIdentifier = PassIdentifier;
		FramePayload->SampleState = InOutSampleState;
		FramePayload->SortingOrder = GetOutputFileSortingOrder();

		TSharedPtr<FAccumulatorPool::FAccumulatorInstance, ESPMode::ThreadSafe> SampleAccumulator = nullptr;
		{
			SCOPE_CYCLE_COUNTER(STAT_MoviePipeline_WaitForAvailableAccumulator);
			SampleAccumulator = AccumulatorPool->BlockAndGetAccumulator_GameThread(InOutSampleState.OutputState.OutputFrameNumber, FramePayload->PassIdentifier);
		}

		UE::MoviePipeline::FObjectIdMaskSampleAccumulationArgs AccumulationArgs;
		{
			AccumulationArgs.OutputMerger = GetPipeline()->OutputBuilder;
			AccumulationArgs.Accumulator = StaticCastSharedPtr<FMaskOverlappedAccumulator>(SampleAccumulator->Accumulator);
			AccumulationArgs.NumOutputLayers = ExpectedPassIdentifiers.Num();
			// Create a copy of our hash map and shuffle it along with the readback data so they stay in sync.
			AccumulationArgs.CacheData = MakeShared<TMap<int32, UE::MoviePipeline::FMoviePipelineHitProxyCacheValue>>(*AccelData.Cache);
		}

		auto Callback = [this, FramePayload, AccumAgs = MoveTemp(AccumulationArgs), SampleAccumulator](TUniquePtr<FImagePixelData>&& InPixelData) mutable
		{
			bool bFinalSample = FramePayload->IsLastTile() && FramePayload->IsLastTemporalSample();
			bool bFirstSample = FramePayload->IsFirstTile() && FramePayload->IsFirstTemporalSample();

			FMoviePipelineBackgroundAccumulateTask Task;
			Task.LastCompletionEvent = SampleAccumulator->TaskPrereq;

			FGraphEventRef Event = Task.Execute([PixelData = MoveTemp(InPixelData), AccumArgsInner = MoveTemp(AccumAgs), bFinalSample, SampleAccumulator]() mutable
			{
				// Enqueue a encode for this frame onto our worker thread.
				MoviePipeline::AccumulateSampleObjectId_TaskThread(MoveTemp(PixelData), MoveTemp(AccumArgsInner));
				if (bFinalSample)
				{
					SampleAccumulator->bIsActive = false;
					SampleAccumulator->TaskPrereq = nullptr;
				}
			});

			SampleAccumulator->TaskPrereq = Event;
			this->OutstandingTasks.Add(Event);
		};

		TSharedPtr<FMoviePipelineSurfaceQueue, ESPMode::ThreadSafe> LocalSurfaceQueue = GetOrCreateSurfaceQueue(InSampleState.BackbufferSize, (IViewCalcPayload*)(&FramePayload.Get()));

		ENQUEUE_RENDER_COMMAND(CanvasRenderTargetResolveCommand)(
			[LocalSurfaceQueue, FramePayload, Callback, RenderTarget](FRHICommandListImmediate& RHICmdList) mutable
			{
				// Enqueue a encode for this frame onto our worker thread.
				LocalSurfaceQueue->OnRenderTargetReady_RenderThread(RenderTarget->GetRenderTargetTexture(), FramePayload, MoveTemp(Callback));
			});
	}
}

void UMoviePipelineObjectIdRenderPass::PostRendererSubmission(const FMoviePipelineRenderPassMetrics& InSampleState)
{
}

namespace MoviePipeline
{
	static void AccumulateSampleObjectId_TaskThread(TUniquePtr<FImagePixelData>&& InPixelData, const UE::MoviePipeline::FObjectIdMaskSampleAccumulationArgs& InParams)
	{
		SCOPE_CYCLE_COUNTER(STAT_AccumulateMaskSample_TaskThread);
		const double TotalSampleBeginTime = FPlatformTime::Seconds();

		bool bIsWellFormed = InPixelData->IsDataWellFormed();

		if (!bIsWellFormed)
		{
			// figure out why it is not well formed, and print a warning.
			int64 RawSize = InPixelData->GetRawDataSizeInBytes();

			int64 SizeX = InPixelData->GetSize().X;
			int64 SizeY = InPixelData->GetSize().Y;
			int64 ByteDepth = int64(InPixelData->GetBitDepth() / 8);
			int64 NumChannels = int64(InPixelData->GetNumChannels());
			int64 ExpectedTotalSize = SizeX * SizeY * ByteDepth * NumChannels;
			int64 ActualTotalSize = InPixelData->GetRawDataSizeInBytes();

			UE_LOG(LogMovieRenderPipeline, Log, TEXT("MaskPassAccumulateSample_TaskThread: Data is not well formed."));
			UE_LOG(LogMovieRenderPipeline, Log, TEXT("Image dimension: %lldx%lld, %lld, %lld"), SizeX, SizeY, ByteDepth, NumChannels);
			UE_LOG(LogMovieRenderPipeline, Log, TEXT("Expected size: %lld"), ExpectedTotalSize);
			UE_LOG(LogMovieRenderPipeline, Log, TEXT("Actual size:   %lld"), ActualTotalSize);
		}

		check(bIsWellFormed);

		FImagePixelDataPayload* FramePayload = InPixelData->GetPayload<FImagePixelDataPayload>();
		check(FramePayload);

		static const uint32 DefaultHash = HashNameToId(TCHAR_TO_UTF8(TEXT("default")));
		static const float DefaultHashAsFloat = *(float*)(&DefaultHash);

		// Writing tiles can be useful for debug reasons. These get passed onto the output every frame.
		if (FramePayload->SampleState.bWriteSampleToDisk)
		{
			// Send the data to the Output Builder. This has to be a copy of the pixel data from the GPU, since
			// it enqueues it onto the game thread and won't be read/sent to write to disk for another frame. 
			// The extra copy is unfortunate, but is only the size of a single sample (ie: 1920x1080 -> 17mb)
			TUniquePtr<FImagePixelData> SampleData = InPixelData->CopyImageData();
			InParams.OutputMerger->OnSingleSampleDataAvailable_AnyThread(MoveTemp(SampleData));
		}


		// For the first sample in a new output, we allocate memory
		if (FramePayload->IsFirstTile() && FramePayload->IsFirstTemporalSample())
		{
			InParams.Accumulator->InitMemory(FIntPoint(FramePayload->SampleState.TileSize.X * FramePayload->SampleState.TileCounts.X, FramePayload->SampleState.TileSize.Y * FramePayload->SampleState.TileCounts.Y));
			InParams.Accumulator->ZeroPlanes();
		}

		// Accumulate the new sample to our target
		{
			const double RemapBeginTime = FPlatformTime::Seconds();

			FIntPoint RawSize = InPixelData->GetSize();

			check(FramePayload->SampleState.TileSize.X + 2 * FramePayload->SampleState.OverlappedPad.X == RawSize.X);
			check(FramePayload->SampleState.TileSize.Y + 2 * FramePayload->SampleState.OverlappedPad.Y == RawSize.Y);

			const void* RawData;
			int64 TotalSize;
			InPixelData->GetRawData(RawData, TotalSize);

			const FColor* RawDataPtr = static_cast<const FColor*>(RawData);
			TArray64<float> IdData;
			IdData.SetNumUninitialized(RawSize.X * RawSize.Y);

			// Remap hitproxy id into precalculated Cryptomatte hash
			ParallelFor(RawSize.Y,
				[&](int32 ScanlineIndex = 0)
				{
					for (int64 Index = 0; Index < RawSize.X; Index++)
					{
						int64 DstIndex = int64(ScanlineIndex) * int64(RawSize.X) + int64(Index);
						// Turn the FColor into an integer index
						const FColor* Color = &RawDataPtr[DstIndex];

						int32 HitProxyIndex = ((int32)Color->R << 16) | ((int32)Color->G << 8) | ((int32)Color->B << 0);

						float Hash = DefaultHashAsFloat;
						const UE::MoviePipeline::FMoviePipelineHitProxyCacheValue* CachedValue = InParams.CacheData->Find(HitProxyIndex);
						if (CachedValue)
						{
							Hash = CachedValue->Hash;
						}
						else
						{
							UE_LOG(LogMovieRenderPipeline, VeryVerbose, TEXT("Failed to find cache data for Hitproxy! Id: %d"), HitProxyIndex);
						}

						IdData[DstIndex] = Hash;
					}
				});
			const double RemapEndTime = FPlatformTime::Seconds();
			const float ElapsedRemapMs = float((RemapEndTime - RemapBeginTime) * 1000.0f);

			const double AccumulateBeginTime = FPlatformTime::Seconds();
			MoviePipeline::FTileWeight1D WeightFunctionX;
			MoviePipeline::FTileWeight1D WeightFunctionY;
			FramePayload->GetWeightFunctionParams(/*Out*/ WeightFunctionX, /*Out*/ WeightFunctionY);
			{
				InParams.Accumulator->AccumulatePixelData((float*)(IdData.GetData()), RawSize, FramePayload->SampleState.OverlappedOffset, FramePayload->SampleState.OverlappedSubpixelShift,
					WeightFunctionX, WeightFunctionY);
			}

			const double AccumulateEndTime = FPlatformTime::Seconds();
			const float ElapsedAccumulateMs = float((AccumulateEndTime - AccumulateBeginTime) * 1000.0f);

			UE_LOG(LogMovieRenderPipeline, VeryVerbose, TEXT("Remap Time: %8.2fms Accumulation time: %8.2fms"), ElapsedRemapMs, ElapsedAccumulateMs);
		}

		if (FramePayload->IsLastTile() && FramePayload->IsLastTemporalSample())
		{
			const double FetchBeginTime = FPlatformTime::Seconds();

			int32 FullSizeX = InParams.Accumulator->PlaneSize.X;
			int32 FullSizeY = InParams.Accumulator->PlaneSize.Y;

			// Now that a tile is fully built and accumulated we can notify the output builder that the
			// data is ready so it can pass that onto the output containers (if needed).
			// 32 bit FLinearColor
			TArray<TArray64<FLinearColor>> OutputLayers;
			for (int32 Index = 0; Index < InParams.NumOutputLayers; Index++)
			{
				OutputLayers.Add(TArray64<FLinearColor>());
			}

			InParams.Accumulator->FetchFinalPixelDataLinearColor(OutputLayers);

			for (int32 Index = 0; Index < InParams.NumOutputLayers; Index++)
			{			
				// We unfortunately can't share ownership of the payload from the last sample due to the changed pass identifiers.
				TSharedRef<FImagePixelDataPayload, ESPMode::ThreadSafe> NewPayload = FramePayload->Copy();
				NewPayload->PassIdentifier = FMoviePipelinePassIdentifier(FramePayload->PassIdentifier.Name + FString::Printf(TEXT("%02d"), Index));

				TUniquePtr<TImagePixelData<FLinearColor>> FinalPixelData = MakeUnique<TImagePixelData<FLinearColor>>(FIntPoint(FullSizeX, FullSizeY), MoveTemp(OutputLayers[Index]), NewPayload);

				// Send each layer to the Output Builder
				InParams.OutputMerger->OnCompleteRenderPassDataAvailable_AnyThread(MoveTemp(FinalPixelData));
			}
			
			// Free the memory in the accumulator now that we've extracted all
			InParams.Accumulator->Reset();

			const double FetchEndTime = FPlatformTime::Seconds();
			const float ElapsedFetchMs = float((FetchEndTime - FetchBeginTime) * 1000.0f);

			UE_LOG(LogMovieRenderPipeline, VeryVerbose, TEXT("Final Frame Fetch Time: %8.2fms"), ElapsedFetchMs);
		}
		const double TotalSampleEndTime = FPlatformTime::Seconds();
		const float ElapsedTotalSampleMs = float((TotalSampleEndTime - TotalSampleBeginTime) * 1000.0f);
		UE_LOG(LogMovieRenderPipeline, VeryVerbose, TEXT("Total Sample Time: %8.2fms"), ElapsedTotalSampleMs);

	}
}

