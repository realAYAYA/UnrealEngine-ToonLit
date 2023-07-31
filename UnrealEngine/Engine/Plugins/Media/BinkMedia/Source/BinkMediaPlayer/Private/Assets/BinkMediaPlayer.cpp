// Copyright Epic Games Tools LLC
//   Licenced under the Unreal Engine EULA 

#include "BinkMediaPlayer.h"

#include "BinkMediaPlayerPrivate.h"
#include "binkplugin.h"
#include "BinkMediaTexture.h"
#include "BinkFunctionLibrary.h"

#include "Misc/Paths.h"
#include "RenderingThread.h"

#include "Slate/SlateTextures.h"
#include "Slate/SceneViewport.h"

#include "Internationalization/Internationalization.h"
#include "Internationalization/Culture.h"
#include "SubtitleManager.h"

//#include "Media/Public/IMediaEventSink.h" //For EMediaEvent

static void Command_ShowBinks()
{
	for (TObjectIterator<UBinkMediaPlayer> It; It; ++It)
	{
		if (!It->HasAnyFlags(RF_ClassDefaultObject))
		{
			const TCHAR* StateStr = TEXT("Unloaded");
			if (It->IsInitialized())
			{
				if (It->IsPaused())
				{
					StateStr = TEXT("Paused");
				}
				else
				{
					StateStr = TEXT("Playing");
				}
			}

			const TCHAR* DrawStyleStr = TEXT("RenderToTexture");
			if (It->BinkDrawStyle != BMASM_Bink_DS_RenderToTexture)
			{
				DrawStyleStr = TEXT("Overlay");
			}
			
			const double DurationSeconds = It->GetDuration().GetTotalSeconds();
			const double PlaybackCompletion = (DurationSeconds > 0.0) ? It->GetTime().GetTotalSeconds() / DurationSeconds : 0.0;

			UE_LOG(LogBinkMoviePlayer, Display, TEXT("[%s] URL: %s, DrawStyle: %s, State: %s, PlaybackPercentage: %d%%"), *It->GetName(), *It->GetUrl(), DrawStyleStr, StateStr, static_cast<int>(PlaybackCompletion * 100.f));
		}
	}
}

static FAutoConsoleCommand ConsoleCmdShowBinks(
	TEXT("bink.List"),
	TEXT("Display all Bink Media Player objects and their current state"),
	FConsoleCommandDelegate::CreateStatic(&Command_ShowBinks));

UBinkMediaPlayer::UBinkMediaPlayer( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
	, Looping(true)
	, StartImmediately(true)
	, DelayedOpen(true)
	, BinkDestinationUpperLeft(0,0)
	, BinkDestinationLowerRight(1,1)
	, BinkBufferMode(BMASM_Bink_Stream)
	, BinkSoundTrack(BMASM_Bink_Sound_None)
	, BinkSoundTrackStart(0)
	, BinkDrawStyle()
	, BinkLayerDepth()
	, CurrentBinkBufferMode(BMASM_Bink_MAX)
	, CurrentBinkSoundTrack(BMASM_Bink_Sound_MAX)
	, CurrentBinkSoundTrackStart(-1)
	, CurrentUrl()
	, CurrentDrawStyle()
	, CurrentLayerDepth()
    , bnk()
    , paused()
    , reached_end()
{
}

bool UBinkMediaPlayer::CanPause() const { return IsPlaying(); }
bool UBinkMediaPlayer::CanPlay() const { return IsReady(); }
bool UBinkMediaPlayer::IsStopped() const { return !IsReady(); }
const FString& UBinkMediaPlayer::GetUrl() const { return CurrentUrl; }
bool UBinkMediaPlayer::Pause() { return SetRate(0.0f); }
bool UBinkMediaPlayer::Play() { return SetRate(1.0f); }
bool UBinkMediaPlayer::Rewind() { return Seek(FTimespan::Zero()); }
bool UBinkMediaPlayer::SupportsRate( float Rate, bool Unthinned ) const { return Rate == 1; }
bool UBinkMediaPlayer::SupportsScrubbing() const { return true; }
bool UBinkMediaPlayer::SupportsSeeking() const { return true; }
FString UBinkMediaPlayer::GetDesc() { return TEXT("UBinkMediaPlayer"); }

FTimespan UBinkMediaPlayer::GetDuration() const 
{
	double ms = 0;
	if(bnk) 
	{
		BINKPLUGININFO bpinfo = {};
		BinkPluginInfo(bnk, &bpinfo);
		ms = ((double)bpinfo.Frames) * ((double)bpinfo.FrameRateDiv) * 1000.0 / ((double)bpinfo.FrameRate);
	}
	return FTimespan::FromMilliseconds(ms);
}

float UBinkMediaPlayer::GetRate() const 
{
	if(bnk) 
	{
		BINKPLUGININFO bpinfo = {};
		BinkPluginInfo(bnk, &bpinfo);
		return bpinfo.PlaybackState == 0 && !paused ? 1 : 0;
	}
	return 0; 
}

FTimespan UBinkMediaPlayer::GetTime() const 
{
	double ms = 0;
	if(bnk) 
	{
		BINKPLUGININFO bpinfo = {};
		BinkPluginInfo(bnk, &bpinfo);
		ms = ((double)bpinfo.FrameNum) * ((double)bpinfo.FrameRateDiv) *1000.0 / ((double)bpinfo.FrameRate);
	}
	return FTimespan::FromMilliseconds(ms);
}

bool UBinkMediaPlayer::IsLooping() const 
{ 
	if(bnk) 
	{
		BINKPLUGININFO bpinfo = {};
		BinkPluginInfo(bnk, &bpinfo);
		if (bpinfo.PlaybackState < 3 && !bpinfo.LoopsRemaining) 
		{
			return true;
		}
	}
	return false;
}

bool UBinkMediaPlayer::IsPaused() const 
{ 
	if(bnk) 
	{
		BINKPLUGININFO bpinfo = {};
		BinkPluginInfo(bnk, &bpinfo);
		return bpinfo.PlaybackState == 1 || paused; // TODO: When the video is paused, the PlaybackState is 0 (should be 1).
	}
	return false;
}

bool UBinkMediaPlayer::IsPlaying() const 
{
    if(bnk) 
	{
        BINKPLUGININFO bpinfo = {};
        BinkPluginInfo(bnk, &bpinfo);
        return bpinfo.PlaybackState == 0 && !paused;
    }
    return false;
}

bool UBinkMediaPlayer::OpenUrl( const FString& NewUrl ) 
{
	if (NewUrl.IsEmpty()) 
	{
		return false;
	}

	URL = NewUrl;
	InitializePlayer();
	return CurrentUrl == NewUrl;
}

void UBinkMediaPlayer::CloseUrl( ) 
{
	URL = "";
	InitializePlayer();
}

bool UBinkMediaPlayer::SetLooping( bool InLooping ) 
{
	if(bnk) 
	{
		BinkPluginLoop(bnk, InLooping ? 0 : 1);
	}
	return false;
}

bool UBinkMediaPlayer::SetRate( float Rate ) 
{ 
	if(bnk) 
	{
		BINKPLUGININFO bpinfo = {};
		BinkPluginInfo(bnk, &bpinfo);
		if((bpinfo.PlaybackState == 1 || paused) && Rate == 1) // If paused and set the rate to 1.0
		{
			BinkPluginPause(bnk, 0);
			paused = false;
			reached_end = false;
			//MediaEvent.Broadcast(EMediaEvent::PlaybackResumed);
			return true;
		} 
		else if(bpinfo.PlaybackState == 3 && Rate == 1) 
		{
			BinkPluginGoto(bnk, 1, -1);
			BinkPluginPause(bnk, 0);
			paused = false;
			reached_end = false;
			//MediaEvent.Broadcast(EMediaEvent::PlaybackResumed);
			return true;
		} 
		else if (bpinfo.PlaybackState == 0 && Rate == 0) 
		{
			BinkPluginPause(bnk, -1);
			paused = true;
			//MediaEvent.Broadcast(EMediaEvent::PlaybackSuspended);
			OnPlaybackSuspended.Broadcast();
			return true;
		}
	}
	return false;
}

void UBinkMediaPlayer::SetVolume( float Volume ) 
{ 
	if(bnk) 
	{
		// Clamp 0 .. 1
		Volume = Volume < 0 ? 0 : Volume > 1 ? 1 : Volume;
		BinkPluginVolume(bnk, Volume);
	}
}

bool UBinkMediaPlayer::Seek( const FTimespan& InTime ) 
{ 
	if (!bnk) 
	{
		return false;
	}
	BINKPLUGININFO bpinfo = {};
	BinkPluginInfo(bnk, &bpinfo);
	U32 desiredFrame = (U32)(InTime.GetTotalMilliseconds() * ((double)bpinfo.FrameRate) / (1000.0*((double)bpinfo.FrameRateDiv)));
	if (bpinfo.FrameNum != (desiredFrame + 1))
	{
		BinkPluginGoto(bnk, desiredFrame + 1, -1);
		reached_end = false;
	}
	return true;
}

void UBinkMediaPlayer::BeginDestroy() 
{
	Super::BeginDestroy();
	Close();
}

void UBinkMediaPlayer::PostLoad() 
{
	Super::PostLoad();
	if (!HasAnyFlags(RF_ClassDefaultObject) && !GIsBuildMachine && (!DelayedOpen || StartImmediately)) 
	{
		InitializePlayer();
	}
}

#if BINKPLUGIN_UE4_EDITOR

void UBinkMediaPlayer::PostEditChangeProperty( FPropertyChangedEvent& PropertyChangedEvent ) 
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	InitializePlayer();
}

#endif

bool UBinkMediaPlayer::Open(const FString& Url) 
{
	if (Url.IsEmpty() || IsPlaying()) 
	{
		return false;
	}

	if(bnk) 
	{
		ENQUEUE_RENDER_COMMAND(BinkMediaPlayer_Open_CloseBink)([bnk=bnk](FRHICommandListImmediate& RHICmdList) 
		{ 
			BinkPluginClose(bnk);
		});
		bnk = NULL;
	}

	BinkPluginLimitSpeakers(GetNumSpeakers());

	// Use the platform file layer to open the media file. We
	// need to access Bink specific information to allow
	// for playing media that is embedded in the APK, OBBs,
	// and/or PAKs.

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

#if PLATFORM_ANDROID
	// Construct a canonical path for the movie.
	FString MoviePath = Url;
	FPaths::NormalizeFilename(MoviePath);

	// Don't bother trying to play it if we can't find it.
	if (!IAndroidPlatformFile::GetPlatformPhysical().FileExists(*MoviePath)) {
		return false;
	}

	// Get information about the movie.
	int64 FileOffset = IAndroidPlatformFile::GetPlatformPhysical().FileStartOffset(*MoviePath);
	FString FileRootPath = IAndroidPlatformFile::GetPlatformPhysical().FileRootPath(*MoviePath);

	// Play the movie as a file or asset.
	if (IAndroidPlatformFile::GetPlatformPhysical().IsAsset(*MoviePath)) 
	{
		if (JNIEnv* env = FAndroidApplication::GetJavaEnv()) 
		{
			extern struct android_app* GNativeAndroidApp;
			jclass clazz = env->GetObjectClass(GNativeAndroidApp->activity->clazz);
			jmethodID methodID = env->GetMethodID(clazz, "getPackageCodePath", "()Ljava/lang/String;");
			jobject result = env->CallObjectMethod(GNativeAndroidApp->activity->clazz, methodID);
			jboolean isCopy;
			const char *apkPath = env->GetStringUTFChars((jstring)result, &isCopy);
			bnk = BinkPluginOpen(apkPath, BinkSoundTrack, BinkSoundTrackStart, BinkBufferMode, FileOffset);
			env->ReleaseStringUTFChars((jstring)result, apkPath);
		}
	}
	else 
	{
		bnk = BinkPluginOpen(TCHAR_TO_ANSI(*FileRootPath), BinkSoundTrack, BinkSoundTrackStart, BinkBufferMode, FileOffset);
	}
#else
	if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*Url)) 
	{
#if PLATFORM_WINDOWS 
		bnk = BinkPluginOpenUTF16((unsigned short *)*PlatformFile.ConvertToAbsolutePathForExternalAppForRead(*Url), BinkSoundTrack, BinkSoundTrackStart, BinkBufferMode, 0);
#else
		bnk = BinkPluginOpen(TCHAR_TO_ANSI(*PlatformFile.ConvertToAbsolutePathForExternalAppForRead(*Url)), BinkSoundTrack, BinkSoundTrackStart, BinkBufferMode, 0);
#endif
	}
#endif
	if(!bnk) 
	{
		//MediaEvent.Broadcast(EMediaEvent::MediaOpenFailed);
		return false;
	}

	// Try to open subtitles...
	{
		// Determine what out current culture is, and grab the most appropriate set of subtitles for it
		FInternationalization& Internationalization = FInternationalization::Get();

		const TArray<FString> PrioritizedLanguageNames = Internationalization.GetPrioritizedCultureNames(Internationalization.GetCurrentLanguage()->GetName());

		CurrentHasSubtitles = 0;
		for (const FString& LanguageName : PrioritizedLanguageNames)
		{
			int32 Pos = INDEX_NONE;
			if (Url.FindLastChar(TEXT('.'), Pos))
			{
				const int32 PathEndPos = Url.FindLastCharByPredicate([](TCHAR C) { return C == TEXT('/') || C == TEXT('\\'); });
				if (PathEndPos != INDEX_NONE && PathEndPos > Pos)
				{
					// The dot found was part of the path rather than the name
					Pos = INDEX_NONE;
				}
			}
			FString SubtitleUrl = (Pos == INDEX_NONE ? Url : Url.Left(Pos)) + "_" + LanguageName + ".srt";
			if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*SubtitleUrl))
			{
#if PLATFORM_WINDOWS 
				if (BinkPluginLoadSubtitlesUTF16(bnk, (unsigned short*)*PlatformFile.ConvertToAbsolutePathForExternalAppForRead(*SubtitleUrl)))
#else
				if (BinkPluginLoadSubtitles(bnk, TCHAR_TO_ANSI(*PlatformFile.ConvertToAbsolutePathForExternalAppForRead(*SubtitleUrl))))
#endif
				{
					CurrentHasSubtitles = 1;
					break;
				}
			}
		}
	}
    
	paused = false;
	reached_end = false;
	HandleMediaPlayerMediaOpened(Url);
	return true;
}

void UBinkMediaPlayer::Close() 
{
	if(bnk) 
	{
		if (CurrentHasSubtitles) 
		{
			CurrentHasSubtitles = 0;

			// Clear the movie subtitle for this object
			TArray<FString> StopSubtitles = { TEXT("") };
			FSubtitleManager::GetSubtitleManager()->SetMovieSubtitle(this, StopSubtitles);
		}

		ENQUEUE_RENDER_COMMAND(BinkMediaPlayer_Close_CloseBink)([bnk=bnk](FRHICommandListImmediate& RHICmdList) 
		{ 
			BinkPluginClose(bnk);
		});
		bnk = NULL;
	}
	CurrentUrl = FString();
	HandleMediaPlayerMediaClosed();
}

void UBinkMediaPlayer::InitializePlayer() 
{
	if (URL != CurrentUrl 
		|| BinkBufferMode != CurrentBinkBufferMode 
		|| BinkSoundTrack != CurrentBinkSoundTrack 
		|| BinkSoundTrackStart != CurrentBinkSoundTrackStart 
		|| BinkDrawStyle != CurrentDrawStyle
		|| BinkLayerDepth != CurrentLayerDepth ) 
	{
		Close();

		if (URL.IsEmpty()) 
		{
			return;
		}

		// open the new media file
		bool OpenedSuccessfully = false;

		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		FString FullUrl = FPaths::ConvertRelativePathToFull(FPaths::IsRelative(URL) ? BINKCONTENTPATH / URL : URL);
		OpenedSuccessfully = Open(FullUrl);
		if (!OpenedSuccessfully) 
		{
			FString cookpath = BinkUE4CookOnTheFlyPath(FPaths::ConvertRelativePathToFull(BINKCONTENTPATH), *URL);
			OpenedSuccessfully = Open(cookpath);
		}

		// finish initialization
		if (OpenedSuccessfully) 
		{
			CurrentUrl = URL;
			CurrentBinkBufferMode = BinkBufferMode;
			CurrentBinkSoundTrack = BinkSoundTrack;
			CurrentBinkSoundTrackStart = BinkSoundTrackStart;
			CurrentDrawStyle = BinkDrawStyle;
			CurrentLayerDepth = BinkLayerDepth;
		}
	}

	SetLooping(Looping);
	SetRate(StartImmediately ? 1 : 0);
}

void UBinkMediaPlayer::HandleMediaPlayerMediaClosed() 
{
	MediaChangedEvent.Broadcast();
	OnMediaClosed.Broadcast();
	//MediaEvent.Broadcast(EMediaEvent::MediaClosed);
}

void UBinkMediaPlayer::HandleMediaPlayerMediaOpened( FString OpenedUrl ) 
{
	MediaChangedEvent.Broadcast();
	OnMediaOpened.Broadcast(OpenedUrl);
	//MediaEvent.Broadcast(EMediaEvent::MediaOpened);
}

void UBinkMediaPlayer::Tick(float DeltaTime) 
{
	// Check for if we should issue the reached end event
	if (bnk && !reached_end && !IsPlaying() && !IsPaused() && !IsLooping()) 
	{
		OnMediaReachedEnd.Broadcast();
		reached_end = true;
		//MediaEvent.Broadcast(EMediaEvent::PlaybackEndReached);
	}
	if (bnk && GEngine && GEngine->GameViewport) 
	{
		if (!IsPlaying() && !IsPaused()) 
		{
			return;
		}

		FVector2D screenSize;
		GEngine->GameViewport->GetViewportSize(screenSize);
		if (CurrentHasSubtitles == 1)
		{
			TArray<FString> SubtitlesText;
			unsigned i = 0;
			while(const char *sub = BinkPluginCurrentSubtitle(bnk, &i))
			{
				SubtitlesText.Add((UTF8CHAR*)sub);
			}

			FSubtitleManager::GetSubtitleManager()->SetMovieSubtitle(this, SubtitlesText);
		}

		if (BinkDrawStyle != 0)
		{
			BINKPLUGININFO bpinfo = {};
			BinkPluginInfo(bnk, &bpinfo);
			int binkw = bpinfo.Width;
			int binkh = bpinfo.Height;

			float ulx = BinkDestinationUpperLeft.X;
			float uly = BinkDestinationUpperLeft.Y;
			float lrx = BinkDestinationLowerRight.X;
			float lry = BinkDestinationLowerRight.Y;

			// figure out the x,y screencoords for all of the overlay types
			if (BinkDrawStyle == 1/*BNK_DS_OverlayFillScreenWithAspectRatio*/) 
			{
				lrx = binkw / screenSize.X;
				lry = binkh / screenSize.Y;

				if (lrx > lry) 
				{
					lry /= lrx;
					lrx = 1;
				}
				else 
				{
					lrx /= lry;
					lry = 1;
				}
				ulx = (1.0f - lrx) / 2.0f;
				uly = (1.0f - lry) / 2.0f;
				lrx += ulx;
				lry += uly;
			}
			else if (BinkDrawStyle == 2/*BNK_DS_OverlayOriginalMovieSize*/) 
			{
				ulx = (screenSize.X - binkw) / (2.0f * screenSize.X);
				uly = (screenSize.Y - binkh) / (2.0f * screenSize.Y);
				lrx = binkw / screenSize.X + ulx;
				lry = binkh / screenSize.Y + uly;
			}

#if PLATFORM_ANDROID
			uly = 1 - uly;
			lry = 1 - lry;
#endif

			ENQUEUE_RENDER_COMMAND(BinkScheduleOverlay)([bnk=bnk,ulx,uly,lrx,lry](FRHICommandListImmediate& RHICmdList) 
			{ 
				BinkPluginSetDrawFlags(bnk, 0);
				BinkPluginScheduleOverlay(bnk, ulx, uly, lrx, lry, 0);
			});
		}
	}
}

void UBinkMediaPlayer::UpdateTexture(FRHICommandListImmediate &RHICmdList, FTexture2DRHIRef ref, void *nativePtr, int width, int height, bool isEditor, bool tonemap, int output_nits, float alpha, bool srgb_decode, bool is_hdr) 
{
	check(IsInRenderingThread());

	if (bnk && (BinkDrawStyle == 0 || isEditor)) 
	{
		BinkPluginSetHdrSettings(bnk, tonemap, 1.0f, output_nits);
		BinkPluginSetAlphaSettings(bnk, alpha);
		BinkPluginSetDrawFlags(bnk, srgb_decode ? BinkDrawDecodeSRGB : 0);
		BinkPluginSetRenderTargetFormat(bnk, is_hdr ? 1 : 0);
		BinkPluginScheduleToTexture(bnk, BinkDestinationUpperLeft.X, BinkDestinationUpperLeft.Y, BinkDestinationLowerRight.X, BinkDestinationLowerRight.Y, 0, ref.GetReference(), width, height);
		BinkActiveTextureRefs.Push(ref);
	}
}

void UBinkMediaPlayer::Draw(UTexture *texture, bool tonemap, int out_nits, float alpha, bool srgb_decode, bool hdr) 
{
	if (!bnk || !texture->GetResource()) 
	{
		return;
	}
	FTexture2DRHIRef ref = texture->GetResource()->TextureRHI->GetTexture2D();
	if ((!IsPlaying() && !IsPaused()) || !ref) 
	{
		return;
	}
	int width = texture->GetSurfaceWidth();
	int height = texture->GetSurfaceHeight();
	void *native = ref->GetNativeResource();
	if (!native) 
	{
		texture->UpdateResource();
		return;
	}

	struct parms_t 
	{
		UBinkMediaPlayer *player;
		FTexture2DRHIRef ref;
		void *native;
		int width, height;
		bool tonemap;
		bool srgb_decode;
		bool hdr;
		int out_nits;
		float alpha;
	} parms = { this, ref, native, width, height, tonemap, srgb_decode, hdr, out_nits, alpha };
	ENQUEUE_RENDER_COMMAND(BinkMediaPlayer_Draw)([parms](FRHICommandListImmediate& RHICmdList) 
	{ 
		parms.player->UpdateTexture(RHICmdList, parms.ref, parms.native, parms.width, parms.height, false, parms.tonemap, parms.out_nits, parms.alpha, parms.srgb_decode, parms.hdr);
	});
}

FIntPoint UBinkMediaPlayer::GetDimensions() const
{
	if (bnk)
	{
		BINKPLUGININFO bpinfo = {};
		BinkPluginInfo(bnk, &bpinfo);
		return FIntPoint(bpinfo.Width, bpinfo.Height);
	}
	return FIntPoint(0, 0);
}

float UBinkMediaPlayer::GetFrameRate() const
{
	if (bnk)
	{
		BINKPLUGININFO bpinfo = {};
		BinkPluginInfo(bnk, &bpinfo);
		return (float)(((double)bpinfo.FrameRate) / ((double)bpinfo.FrameRateDiv));
	}
	return 0;
}
