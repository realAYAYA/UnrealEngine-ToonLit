// Copyright Epic Games Tools LLC
//   Licenced under the Unreal Engine EULA 

#include "BinkMovieStreamer.h"

#include "BinkMediaPlayerPrivate.h"
#include "Rendering/RenderingCommon.h"
#include "Slate/SlateTextures.h"
#include "MoviePlayer.h"
#include "MoviePlayerSettings.h"
#include "RHIUtilities.h"
#include "BinkMediaPlayer.h"
#include "BinkFunctionLibrary.h"
#include "HDRHelper.h"

#include "OneColorShader.h"

DEFINE_LOG_CATEGORY(LogBinkMoviePlayer);

FBinkMovieStreamer::FBinkMovieStreamer() 
	: bnk()
{
	MovieViewport = MakeShareable(new FMovieViewport());
	PlaybackType = MT_Normal;
}

FBinkMovieStreamer::~FBinkMovieStreamer() 
{
	CloseMovie();
	Cleanup();

	FlushRenderingCommands();
	TextureFreeList.Empty();
}

bool FBinkMovieStreamer::Init(const TArray<FString>& MoviePaths, TEnumAsByte<EMoviePlaybackType> inPlaybackType) 
{
	if (MoviePaths.Num() == 0) 
	{
		return false;
	}
	MovieIndex = -1;
	PlaybackType = inPlaybackType;
	StoredMoviePaths = MoviePaths;
	OpenNextMovie();
	return true;
}

FString FBinkMovieStreamer::GetMovieName() 
{
	return StoredMoviePaths.IsValidIndex(MovieIndex) ? StoredMoviePaths[MovieIndex] : TEXT("");
}

bool FBinkMovieStreamer::IsLastMovieInPlaylist() 
{
	return MovieIndex == StoredMoviePaths.Num() -1;
}

void FBinkMovieStreamer::ForceCompletion() 
{
	CloseMovie();
}

bool FBinkMovieStreamer::Tick(float DeltaTime) 
{
	// Loops through all movies while not valid
	if (!bnk) 
	{
		CloseMovie();
		if (MovieIndex < StoredMoviePaths.Num() - 1) 
		{
			OpenNextMovie();
		} 
		else if (PlaybackType != MT_Normal) 
		{
			MovieIndex = PlaybackType == MT_LoadingLoop ? StoredMoviePaths.Num() - 2 : -1;
			OpenNextMovie();
		} 
		else 
		{
			return true;
		}
	}

	BINKPLUGININFO bpinfo = {};
	BinkPluginInfo(bnk, &bpinfo);
	if (bpinfo.PlaybackState == 3) 
	{
		CloseMovie();
		if (MovieIndex < StoredMoviePaths.Num() - 1) 
		{
			OpenNextMovie();
		} 
		else if (PlaybackType != MT_Normal) 
		{
			MovieIndex = PlaybackType == MT_LoadingLoop ? StoredMoviePaths.Num() - 2 : -1;
			OpenNextMovie();
		} 
		else 
		{
			return true;
		}
	}
	if (!bnk) 
	{
		return false;
	}

	FSlateTexture2DRHIRef* CurrentTexture = Texture.Get();
	if(CurrentTexture) 
	{
		FVector2D destUpperLeft = GetDefault<UBinkMoviePlayerSettings>()->BinkDestinationUpperLeft; 
		FVector2D destLowerRight = GetDefault<UBinkMoviePlayerSettings>()->BinkDestinationLowerRight;

		check( IsInRenderingThread() );
		if( !CurrentTexture->IsInitialized() ) 
		{
			CurrentTexture->InitResource();
		}

		FTexture2DRHIRef tex = CurrentTexture->GetTypedResource();
		uint32 binkw = tex.GetReference()->GetSizeX();
		uint32 binkh = tex.GetReference()->GetSizeY();
		bool is_hdr = tex.GetReference()->GetFormat() != PF_B8G8R8A8;

		float ulx = destUpperLeft.X;
		float uly = destUpperLeft.Y;
		float lrx = destLowerRight.X;
		float lry = destLowerRight.Y;

		static const auto CVarHDROutputEnabled = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.HDR.EnableHDROutput"));
		static const auto CVarDisplayOutputDevice = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.HDR.Display.OutputDevice"));
		if (GRHISupportsHDROutput && CVarHDROutputEnabled->GetValueOnRenderThread() != 0)
		{
			EDisplayOutputFormat outDev = static_cast<EDisplayOutputFormat>(CVarDisplayOutputDevice->GetValueOnRenderThread());
			float DisplayMaxLuminance = HDRGetDisplayMaximumLuminance();
			switch (outDev)
			{
			// LDR
			case EDisplayOutputFormat::SDR_sRGB:
			case EDisplayOutputFormat::SDR_Rec709:
			case EDisplayOutputFormat::SDR_ExplicitGammaMapping:
				BinkPluginSetHdrSettings(bnk, 1, 1.0f, 80);
				break;
			// 1k nits
			case EDisplayOutputFormat::HDR_ACES_1000nit_ST2084:
			case EDisplayOutputFormat::HDR_ACES_1000nit_ScRGB:
				BinkPluginSetHdrSettings(bnk, 1, 1.0f, DisplayMaxLuminance);
				break;
			// 2k nits
			case EDisplayOutputFormat::HDR_ACES_2000nit_ST2084:
			case EDisplayOutputFormat::HDR_ACES_2000nit_ScRGB:
				BinkPluginSetHdrSettings(bnk, 1, 1.0f, DisplayMaxLuminance);
				break;
			// no tonemap
			default:
				BinkPluginSetHdrSettings(bnk, 0, 1.0f, 1000);
				break;
			}
		}
		else
		{
			BinkPluginSetHdrSettings(bnk, 1, 1.0f, 80);
		}
		BinkPluginSetRenderTargetFormat(bnk, is_hdr ? 1 : 0);

		BinkPluginScheduleToTexture(bnk, ulx, uly, lrx, lry, 0, tex.GetReference(), binkw, binkh);

		auto& RHICmdList = GRHICommandList.GetImmediateCommandList();
		
		BINKPLUGINFRAMEINFO FrameInfo = {};
		FrameInfo.cmdBuf = &RHICmdList;
		BinkPluginSetPerFrameInfo(&FrameInfo);
		BinkPluginAllScheduled();
		BinkPluginProcessBinks(0);
		BinkPluginDraw(1, 0);

		MovieViewport->SetTexture(Texture);	
	}

	return false;
}

void FBinkMovieStreamer::Cleanup() 
{
	FlushRenderingCommands();
	for( int32 TextureIndex = 0; TextureIndex < TextureFreeList.Num(); ++TextureIndex ) 
	{
		BeginReleaseResource( TextureFreeList[TextureIndex].Get() );
	}
}

bool FBinkMovieStreamer::OpenNextMovie() 
{
	MovieIndex++;
	check(StoredMoviePaths.Num() > 0 && MovieIndex < StoredMoviePaths.Num());

	if (GEngine) {
		BinkPluginLimitSpeakers(GetNumSpeakers());
	}

	BINKPLUGINBUFFERING bufferMode = (BINKPLUGINBUFFERING)(int)GetDefault<UBinkMoviePlayerSettings>()->BinkBufferMode;
	BINKPLUGINSNDTRACK soundTrack = (BINKPLUGINSNDTRACK)(int)GetDefault<UBinkMoviePlayerSettings>()->BinkSoundTrack;
	int soundTrackStart = GetDefault<UBinkMoviePlayerSettings>()->BinkSoundTrackStart;

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	FString MoviePathTbl[] = 
	{
		StoredMoviePaths[MovieIndex] + TEXT(".bk2")
	};
	for (int i = 0; i < sizeof(MoviePathTbl) / sizeof(MoviePathTbl[0]) && !bnk; ++i) 
	{
		FString FullMoviePath = BINKMOVIEPATH + MoviePathTbl[i];
#if PLATFORM_ANDROID
		// Don't bother trying to play it if we can't find it
		if (!IAndroidPlatformFile::GetPlatformPhysical().FileExists(*FullMoviePath)) {
			continue;
		}

		int64 FileOffset = IAndroidPlatformFile::GetPlatformPhysical().FileStartOffset(*FullMoviePath);
		FString FileRootPath = IAndroidPlatformFile::GetPlatformPhysical().FileRootPath(*FullMoviePath);

		if (IAndroidPlatformFile::GetPlatformPhysical().IsAsset(*FullMoviePath)) 
		{
			if (JNIEnv* env = FAndroidApplication::GetJavaEnv()) 
			{
				extern struct android_app* GNativeAndroidApp;
				jclass clazz = env->GetObjectClass(GNativeAndroidApp->activity->clazz);
				jmethodID methodID = env->GetMethodID(clazz, "getPackageCodePath", "()Ljava/lang/String;");
				jobject result = env->CallObjectMethod(GNativeAndroidApp->activity->clazz, methodID);
				jboolean isCopy;
				const char *apkPath = env->GetStringUTFChars((jstring)result, &isCopy);
				bnk = BinkPluginOpen(apkPath, soundTrack, soundTrackStart, bufferMode, FileOffset);
				env->ReleaseStringUTFChars((jstring)result, apkPath);
			}
		}
		else 
		{
			bnk = BinkPluginOpen(TCHAR_TO_ANSI(*FileRootPath), soundTrack, soundTrackStart, bufferMode, FileOffset);
		}
#else
		FString ExternalPath = PlatformFile.ConvertToAbsolutePathForExternalAppForRead(*FullMoviePath);
		if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*ExternalPath)) 
		{
#if PLATFORM_WINDOWS 
			bnk = BinkPluginOpenUTF16((unsigned short*)*ExternalPath, soundTrack, soundTrackStart, bufferMode, 0);
#else
			bnk = BinkPluginOpen(TCHAR_TO_ANSI(*ExternalPath), soundTrack, soundTrackStart, bufferMode, 0);
#endif
		}
		if (!bnk) 
		{
			FString CookPath = *BinkUE4CookOnTheFlyPath(FPaths::ConvertRelativePathToFull(BINKMOVIEPATH), *MoviePathTbl[i]);
			ExternalPath = PlatformFile.ConvertToAbsolutePathForExternalAppForRead(*FullMoviePath);
			if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*ExternalPath)) 
			{
#if PLATFORM_WINDOWS 
				bnk = BinkPluginOpenUTF16((unsigned short *)*ExternalPath, soundTrack, soundTrackStart, bufferMode, 0);
#else
				bnk = BinkPluginOpen(TCHAR_TO_ANSI(*ExternalPath), soundTrack, soundTrackStart, bufferMode, 0);
#endif
			}
		}
#endif
	}
	if (!bnk) 
	{
		return false;
	}

	FIntPoint VideoDimensions = FIntPoint::ZeroValue;
	BINKPLUGININFO bpinfo = {};
	BinkPluginInfo(bnk, &bpinfo);
	VideoDimensions.X = bpinfo.Width;
	VideoDimensions.Y = bpinfo.Height;

	if( VideoDimensions.X > 0 && VideoDimensions.Y > 0 ) 
	{
		EPixelFormat pixelFmt = bink_force_pixel_format == PF_Unknown ? (EPixelFormat)GetDefault<UBinkMoviePlayerSettings>()->BinkPixelFormat : bink_force_pixel_format;
		if( TextureFreeList.Num() > 0 ) 
		{
			Texture = TextureFreeList.Pop();

			if( Texture->GetWidth() != VideoDimensions.X || Texture->GetHeight() != VideoDimensions.Y ) 
			{
				FSlateTexture2DRHIRef *ref = Texture.Get();
				ENQUEUE_RENDER_COMMAND(UpdateMovieTexture)([ref,VideoDimensions](FRHICommandListImmediate& RHICmdList) 
				{
					ref->Resize( VideoDimensions.X, VideoDimensions.Y );
				});
			}
		} 
		else 
		{
			const bool bCreateEmptyTexture = true;
			Texture = MakeShareable(new FSlateTexture2DRHIRef(VideoDimensions.X, VideoDimensions.Y, pixelFmt, NULL, TexCreate_RenderTargetable, bCreateEmptyTexture));
			FSlateTexture2DRHIRef *ref = Texture.Get();

			ENQUEUE_RENDER_COMMAND(InitMovieTexture)([ref](FRHICommandListImmediate& RHICmdList) 
			{ 
				ref->InitResource();
			});
		}
	}
	return true;
}

void FBinkMovieStreamer::CloseMovie() 
{
	if (GetMoviePlayer() != nullptr) 
	{
		BroadcastCurrentMovieClipFinished(GetMovieName());
	}

	if (Texture.IsValid()) 
	{
		TextureFreeList.Add(Texture);
		MovieViewport->SetTexture(NULL);
		Texture.Reset();
	}
	if(bnk) 
	{
		BinkPluginClose(bnk);
		bnk = NULL;
	}
}

