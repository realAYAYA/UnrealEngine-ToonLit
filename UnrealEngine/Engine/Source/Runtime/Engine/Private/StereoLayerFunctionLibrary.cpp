// Copyright Epic Games, Inc. All Rights Reserved.
//
#include "Kismet/StereoLayerFunctionLibrary.h"
#include "TextureResource.h"
#include "Engine/Texture.h"
#include "Engine/Engine.h"
#include "StereoRendering.h"
#include "IXRTrackingSystem.h"
#include "IXRLoadingScreen.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StereoLayerFunctionLibrary)

static IXRLoadingScreen* GetLoadingScreen()
{
	if (GEngine && GEngine->XRSystem.IsValid())
	{
		return GEngine->XRSystem->GetLoadingScreen();
	}

	return nullptr;
}

static IStereoLayers* GetStereoLayers()
{
	if (GEngine && GEngine->StereoRenderingDevice.IsValid())
	{
		return GEngine->StereoRenderingDevice->GetStereoLayers();
	}

	return nullptr;
}


class FAutoShow: public TSharedFromThis<FAutoShow>
{
public:
	void OnPreLoadMap(const FString&);
	void OnPostLoadMap(UWorld* LoadedWorld);

	void Register();
	void Unregister();
};

void FAutoShow::OnPreLoadMap(const FString&)
{
	IXRLoadingScreen* LoadingScreen = GetLoadingScreen();
	if (LoadingScreen)
	{
		LoadingScreen->ShowLoadingScreen();
	}
}

void FAutoShow::OnPostLoadMap(UWorld* LoadedWorld)
{
	IXRLoadingScreen* LoadingScreen = GetLoadingScreen();
	if (LoadingScreen)
	{
		LoadingScreen->HideLoadingScreen();
	}
}

void FAutoShow::Register()
{
	FCoreUObjectDelegates::PreLoadMap.AddSP(this, &FAutoShow::OnPreLoadMap);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddSP(this, &FAutoShow::OnPostLoadMap);
}

void FAutoShow::Unregister()
{
	FCoreUObjectDelegates::PreLoadMap.RemoveAll(this);
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
}

static TSharedPtr<FAutoShow> AutoShow;

UStereoLayerFunctionLibrary::UStereoLayerFunctionLibrary(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

void UStereoLayerFunctionLibrary::SetSplashScreen(class UTexture* Texture, FVector2D Scale, FVector Offset, bool bShowLoadingMovie, bool bShowOnSet)
{
	IXRLoadingScreen* LoadingScreen = GetLoadingScreen();
	if (LoadingScreen && Texture && Texture->GetResource())
	{
		LoadingScreen->ClearSplashes();
		IXRLoadingScreen::FSplashDesc Splash;
		Splash.Texture = Texture->GetResource()->TextureRHI;
		Splash.QuadSize = Scale;
		Splash.Transform = FTransform(Offset);
		LoadingScreen->AddSplash(Splash);

		if (bShowOnSet)
		{
			LoadingScreen->ShowLoadingScreen();
		}
	}
}

void UStereoLayerFunctionLibrary::ShowSplashScreen()
{
	IXRLoadingScreen* LoadingScreen = GetLoadingScreen();
	if (LoadingScreen)
	{
		LoadingScreen->ShowLoadingScreen();
	}
}

void UStereoLayerFunctionLibrary::HideSplashScreen()
{
	IXRLoadingScreen* LoadingScreen = GetLoadingScreen();
	if (LoadingScreen)
	{
		LoadingScreen->HideLoadingScreen();
	}
}

void UStereoLayerFunctionLibrary::EnableAutoLoadingSplashScreen(bool InAutoShowEnabled)
{
	if (InAutoShowEnabled)
	{
		if (!AutoShow.IsValid())
		{
			AutoShow = MakeShareable(new FAutoShow);
			AutoShow->Register();
		}
	}
	else 
	{
		if (AutoShow.IsValid())
		{
			AutoShow->Unregister();
			AutoShow = nullptr;
		}
	}
}

