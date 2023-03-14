// Copyright Epic Games, Inc. All Rights Reserved.

#include "IXRLoadingScreen.h"
#include "IXRTrackingSystem.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"


void IXRLoadingScreen::ShowLoadingScreen_Compat(bool bShow, FTexture2DRHIRef Texture, const FVector& Offset, const FVector2D& Scale)
{
	// Backwards compatibility with IStereoLayers::ShowSplashScreen
	IXRLoadingScreen* LoadingScreen = GEngine && GEngine->XRSystem.IsValid()? GEngine->XRSystem->GetLoadingScreen() : nullptr;
	if (LoadingScreen)
	{
		if (bShow)
		{
			// Any texture passed in will override all splashes set through the IXRLoadingScreen interface,
			// otherwise this function only calls Show/HideLoadingScreen.
			if (Texture.IsValid())
			{
				IXRLoadingScreen::FSplashDesc Splash;
				const FIntPoint TextureSize = Texture->GetSizeXY();
				const float	InvAspectRatio = (TextureSize.X > 0) ? float(TextureSize.Y) / float(TextureSize.X) : 1.0f;

				Splash.Texture = Texture;
				Splash.bIgnoreAlpha = true;
				Splash.Transform = FTransform(FVector(5.0f, 0.0f, 1.0f) + Offset * 0.01f);
				Splash.QuadSize = FVector2D(8.0f, 8.0f*InvAspectRatio) * Scale;

				LoadingScreen->ClearSplashes();
				LoadingScreen->AddSplash(Splash);
			}
			LoadingScreen->ShowLoadingScreen();
		}
		else
		{
			LoadingScreen->HideLoadingScreen();
		}
	}

}
