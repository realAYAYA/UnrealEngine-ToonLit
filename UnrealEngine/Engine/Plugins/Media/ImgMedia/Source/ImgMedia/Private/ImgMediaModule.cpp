// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImgMediaPrivate.h"

#include "IMediaClock.h"
#include "IMediaModule.h"
#include "Misc/QueuedThreadPool.h"

#include "ImgMediaGlobalCache.h"
#include "ImgMediaPlayer.h"
#include "ImgMediaSceneViewExtension.h"
#include "ImgMediaScheduler.h"
#include "ImgMediaSource.h"
#include "IImgMediaModule.h"

CSV_DEFINE_CATEGORY_MODULE(IMGMEDIA_API, ImgMedia, false);
DEFINE_LOG_CATEGORY(LogImgMedia);

FLazyName IImgMediaModule::CustomFormatAttributeName(TEXT("EpicGamesCustomFormat"));
FLazyName IImgMediaModule::CustomFormatTileWidthAttributeName(TEXT("EpicGamesTileWidth"));
FLazyName IImgMediaModule::CustomFormatTileHeightAttributeName(TEXT("EpicGamesTileHeight"));
FLazyName IImgMediaModule::CustomFormatTileBorderAttributeName(TEXT("EpicGamesTileBorder"));

TSharedPtr<FImgMediaGlobalCache, ESPMode::ThreadSafe> IImgMediaModule::GlobalCache;

#if USE_IMGMEDIA_DEALLOC_POOL
struct FImgMediaThreadPool
{
public:

	FImgMediaThreadPool() :
		Pool(nullptr),
		bHasInit(false)
	{
	}

	~FImgMediaThreadPool()
	{
		Reset();
	}

	void Reset()
	{
		FScopeLock Lock(&CriticalSection);
		if (Pool != nullptr)
		{
			Pool->Destroy();
			Pool = nullptr;
		}

		bHasInit = false;
	}

	FQueuedThreadPool* GetThreadPool()
	{
		FScopeLock Lock(&CriticalSection);
		if (bHasInit)
		{
			return Pool;
		}

		// initialize worker thread pools
		if (FPlatformProcess::SupportsMultithreading())
		{
			// initialize dealloc thread pool
			const int32 ThreadPoolSize = 1;
			const uint32 StackSize = 128 * 1024;

			Pool = FQueuedThreadPool::Allocate();
			verify(Pool->Create(ThreadPoolSize, StackSize, TPri_Normal));
		}

		bHasInit = true;

		return Pool;
	}

private:
	FCriticalSection CriticalSection;
	FQueuedThreadPool* Pool;
	bool bHasInit;
};

FImgMediaThreadPool ImgMediaThreadPool;

FQueuedThreadPool* GetImgMediaThreadPoolSlow()
{
	return ImgMediaThreadPool.GetThreadPool();
}
#endif // USE_IMGMEDIA_DEALLOC_POOL


/**
 * Implements the AVFMedia module.
 */
class FImgMediaModule
	: public IImgMediaModule
{
public:

	/** Default constructor. */
	FImgMediaModule() { }

public:

	//~ IImgMediaModule interface

	virtual TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CreatePlayer(IMediaEventSink& EventSink) override
	{
		if (!Scheduler.IsValid())
		{
			InitScheduler();
		}
		if (!GlobalCache.IsValid())
		{
			InitGlobalCache();
		}

		TSharedPtr<FImgMediaPlayer, ESPMode::ThreadSafe> Player = MakeShared<FImgMediaPlayer, ESPMode::ThreadSafe>(EventSink, Scheduler.ToSharedRef(), GlobalCache.ToSharedRef());
		OnImgMediaPlayerCreated.Broadcast(Player);

		return Player;
	}

	const TSharedPtr<FImgMediaSceneViewExtension, ESPMode::ThreadSafe>& GetSceneViewExtension() const override
	{
		return SceneViewExtension;
	}

public:

	//~ IModuleInterface interface

	virtual void StartupModule() override
	{
		// Register media source spawners.
		FMediaSourceSpawnDelegate SpawnDelegate =
			FMediaSourceSpawnDelegate::CreateStatic(&FImgMediaModule::SpawnMediaSourceForString);
		for (const FString& Ext : FileExtensions)
		{
			UMediaSource::RegisterSpawnFromFileExtension(Ext, SpawnDelegate);
		}

		FCoreDelegates::OnPostEngineInit.AddRaw(this, &FImgMediaModule::OnPostEngineInit);
		FCoreDelegates::OnEnginePreExit.AddRaw(this, &FImgMediaModule::OnEnginePreExit);
	}

	virtual void ShutdownModule() override
	{
		// Unregister media source spawners.
		for (const FString& Ext : FileExtensions)
		{
			UMediaSource::UnregisterSpawnFromFileExtension(Ext);
		}

		Scheduler.Reset();
		GlobalCache.Reset();

		FCoreDelegates::OnEnginePreExit.RemoveAll(this);
		FCoreDelegates::OnPostEngineInit.RemoveAll(this);

#if USE_IMGMEDIA_DEALLOC_POOL
		ImgMediaThreadPool.Reset();
#endif
	}

private:

	void OnPostEngineInit()
	{
		SceneViewExtension = FSceneViewExtensions::NewExtension<FImgMediaSceneViewExtension>();
	}

	void OnEnginePreExit()
	{
		SceneViewExtension.Reset();
	}

	void InitScheduler()
	{
		// initialize scheduler
		Scheduler = MakeShared<FImgMediaScheduler, ESPMode::ThreadSafe>();
		Scheduler->Initialize();

		IMediaModule* MediaModule = FModuleManager::LoadModulePtr<IMediaModule>("Media");

		if (MediaModule != nullptr)
		{
			MediaModule->GetClock().AddSink(Scheduler.ToSharedRef());
		}
	}

	void InitGlobalCache()
	{
		// Initialize global cache.
		GlobalCache = MakeShared<FImgMediaGlobalCache, ESPMode::ThreadSafe>();
		GlobalCache->Initialize();
	}

	/**
	 * Creates a media source for MediaPath.
	 *
	 * @param	MediaPath		File path to the media.
	 * @param	Outer			Outer to use for this object.
	 */
	static UMediaSource* SpawnMediaSourceForString(const FString& MediaPath, UObject* Outer)
	{
		TObjectPtr<UImgMediaSource> MediaSource = 
			NewObject<UImgMediaSource>(Outer, NAME_None, RF_Transactional);
		MediaSource->SetSequencePath(MediaPath);

		return MediaSource;
	}

	TSharedPtr<FImgMediaScheduler, ESPMode::ThreadSafe> Scheduler;

	/** Scene view extension used to track view/camera info. */
	TSharedPtr<FImgMediaSceneViewExtension, ESPMode::ThreadSafe> SceneViewExtension;

	/** List of file extensions that we support. */
	const TArray<FString> FileExtensions =
	{
		TEXT("bmp"),
		TEXT("exr"),
		TEXT("jpg"),
		TEXT("jpeg"),
		TEXT("png"),
	};
};


IMPLEMENT_MODULE(FImgMediaModule, ImgMedia);
