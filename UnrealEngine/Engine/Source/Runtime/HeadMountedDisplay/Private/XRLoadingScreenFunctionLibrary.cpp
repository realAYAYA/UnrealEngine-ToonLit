// Copyright Epic Games, Inc. All Rights Reserved.

#include "XRLoadingScreenFunctionLibrary.h"
#include "EngineGlobals.h"
#include "TextureResource.h"
#include "Engine/Texture.h"
#include "Engine/Engine.h"
#include "IXRTrackingSystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(XRLoadingScreenFunctionLibrary)

static IXRLoadingScreen* GetLoadingScreen()
{
	if (GEngine && GEngine->XRSystem.IsValid())
	{
		return GEngine->XRSystem->GetLoadingScreen();
	}
	
	return nullptr;
}

UXRLoadingScreenFunctionLibrary::UXRLoadingScreenFunctionLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UXRLoadingScreenFunctionLibrary::SetLoadingScreen(class UTexture* Texture, FVector2D Scale, FVector Offset, bool bShowLoadingMovie, bool bShowOnSet)
{
	IXRLoadingScreen* LoadingSrcreen = GetLoadingScreen();
	if (LoadingSrcreen && Texture && Texture->GetResource())
	{
		LoadingSrcreen->ClearSplashes();
		const bool bIsExternal = Texture->GetMaterialType() == MCT_TextureExternal;
		IXRLoadingScreen::FSplashDesc Splash;
		Splash.Transform = FTransform(Offset);
		Splash.QuadSize = Scale;
		Splash.bIsDynamic = bShowLoadingMovie || bIsExternal;
		Splash.bIsExternal = bIsExternal;
		Splash.Texture = Texture->GetResource()->TextureRHI;
		LoadingSrcreen->AddSplash(Splash);

		if (bShowOnSet)
		{
			LoadingSrcreen->ShowLoadingScreen();
		}
	}
}

void UXRLoadingScreenFunctionLibrary::ClearLoadingScreenSplashes()
{
	IXRLoadingScreen* LoadingSrcreen = GetLoadingScreen();
	if (LoadingSrcreen)
	{
		LoadingSrcreen->ClearSplashes();
	}

}

void UXRLoadingScreenFunctionLibrary::AddLoadingScreenSplash(class UTexture* Texture, FVector Translation, FRotator Rotation, FVector2D Size, FRotator DeltaRotation, bool bClearBeforeAdd)
{
	IXRLoadingScreen* LoadingSrcreen = GetLoadingScreen();
	if (LoadingSrcreen && Texture && Texture->GetResource())
	{
		if (bClearBeforeAdd)
		{
			LoadingSrcreen->ClearSplashes();
		}

		IXRLoadingScreen::FSplashDesc Splash;
		Splash.Texture = Texture->GetResource()->TextureRHI;
		Splash.QuadSize = Size;
		Splash.Transform = FTransform(Rotation, Translation);
		Splash.DeltaRotation = FQuat(DeltaRotation);
		LoadingSrcreen->AddSplash(Splash);
	}
}

void UXRLoadingScreenFunctionLibrary::ShowLoadingScreen()
{
	IXRLoadingScreen* LoadingSrcreen = GetLoadingScreen();
	if (LoadingSrcreen)
	{
		LoadingSrcreen->ShowLoadingScreen();
	}
}

void UXRLoadingScreenFunctionLibrary::HideLoadingScreen()
{
	IXRLoadingScreen* LoadingSrcreen = GetLoadingScreen();
	if (LoadingSrcreen)
	{
		LoadingSrcreen->HideLoadingScreen();
	}
}

