// Copyright Epic Games, Inc. All Rights Reserved.

#include "DefaultXRLoadingScreen.h"
#include "IXRTrackingSystem.h"

using FLayerDesc = IStereoLayers::FLayerDesc;
using FSplashDesc = IXRLoadingScreen::FSplashDesc;


FDefaultXRLoadingScreen::FDefaultXRLoadingScreen(class IXRTrackingSystem* InTrackingSystem)
	: TXRLoadingScreenBase(InTrackingSystem)
{
	check(TrackingSystem->GetStereoRenderingDevice().IsValid());
}

IStereoLayers* FDefaultXRLoadingScreen::GetStereoLayers() const
{
	IStereoLayers* StereoLayers = nullptr;
	auto StereoRendering = TrackingSystem->GetStereoRenderingDevice();
	if (StereoRendering.IsValid())
	{
		StereoLayers = StereoRendering->GetStereoLayers();
		if (!StereoLayers || !StereoLayers->SupportsLayerState())
		{
			StereoLayers = nullptr;
			UE_LOG(LogHMD, Warning, TEXT("FDefaultXRLoadingScreen requires a working IStereoLayers implementation supporting IStereoLayers::Push/PopLayerState."));
		}
	}
	return StereoLayers;
}

void FDefaultXRLoadingScreen::ShowLoadingScreen()
{
	auto StereoLayers = GetStereoLayers();
	if (!StereoLayers)
	{
		return;
	}

	if (!bShowing)
	{
		StereoLayers->PushLayerState();
		StereoLayers->HideBackgroundLayer();
	}
	TXRLoadingScreenBase::ShowLoadingScreen();
}

void FDefaultXRLoadingScreen::HideLoadingScreen()
{
	if (!bShowing)
	{
		return;
	}
	TXRLoadingScreenBase::HideLoadingScreen();

	auto StereoLayers = GetStereoLayers();
	if (StereoLayers) // StereoLayers may be null if we are shutting down
	{
		StereoLayers->PopLayerState();
	}
}

void FDefaultXRLoadingScreen::DoHideSplash(FSplashData& Splash)
{
	// Pop layer state will destroy all created layers, so we clear the layer id stored locally
	Splash.LayerId = FLayerDesc::INVALID_LAYER_ID;
}

void FDefaultXRLoadingScreen::DoShowSplash(FSplashData& Splash)
{
	FLayerDesc LayerDesc;
	bool bUpdate = (Splash.LayerId != FLayerDesc::INVALID_LAYER_ID);
	auto StereoLayers = GetStereoLayers();
	float WorldScale = TrackingSystem->GetWorldToMetersScale();

	if (bUpdate)
	{
		StereoLayers->GetLayerDesc(Splash.LayerId, LayerDesc);
	}

	LayerDesc.Transform = Splash.Desc.Transform;
	LayerDesc.Transform.ScaleTranslation(WorldScale);
	LayerDesc.Transform *= FTransform(HMDOrientation);
	LayerDesc.QuadSize = Splash.Desc.QuadSize * WorldScale;
	LayerDesc.UVRect = Splash.Desc.UVRect;
	// Sort layers by distance from the viewer
	LayerDesc.Priority = INT32_MAX - static_cast<int32>(Splash.Desc.Transform.GetTranslation().X * 1000.f);
	LayerDesc.PositionType = IStereoLayers::TrackerLocked;
	
	if (Splash.Desc.Texture->GetTextureCube() != nullptr)
	{
		if (!LayerDesc.HasShape<FCubemapLayer>())
		{
			LayerDesc.SetShape<FCubemapLayer>();
		}
	}
	else
	{
		if (!LayerDesc.HasShape<FQuadLayer>())
		{
			LayerDesc.SetShape<FQuadLayer>();
		}
	}

	LayerDesc.Texture = Splash.Desc.Texture;
	LayerDesc.LeftTexture = Splash.Desc.LeftTexture;
	LayerDesc.Flags = IStereoLayers::LAYER_FLAG_QUAD_PRESERVE_TEX_RATIO
		| (Splash.Desc.bIgnoreAlpha ? IStereoLayers::LAYER_FLAG_TEX_NO_ALPHA_CHANNEL  : 0)
		| (Splash.Desc.bIsDynamic   ? IStereoLayers::LAYER_FLAG_TEX_CONTINUOUS_UPDATE : 0)
		| (Splash.Desc.bIsExternal  ? IStereoLayers::LAYER_FLAG_TEX_EXTERNAL		  : 0);

	if (bUpdate)
	{
		StereoLayers->SetLayerDesc(Splash.LayerId, LayerDesc);
	}
	else
	{
		Splash.LayerId = StereoLayers->CreateLayer(LayerDesc);
	}
}

void FDefaultXRLoadingScreen::DoDeleteSplash(FSplashData& Splash)
{
	auto StereoLayers = GetStereoLayers();
	check(StereoLayers); // bShowing will never get set unless stereo layers are available
	if (Splash.LayerId != FLayerDesc::INVALID_LAYER_ID)
	{
		StereoLayers->DestroyLayer(Splash.LayerId);
		Splash.LayerId = FLayerDesc::INVALID_LAYER_ID;
	}
}

void FDefaultXRLoadingScreen::ApplyDeltaRotation(const FSplashData& Splash)
{
	if (!bShowing || Splash.LayerId == FLayerDesc::INVALID_LAYER_ID || Splash.Desc.DeltaRotation == FQuat::Identity)
	{
		return;
	}
	auto StereoLayers = GetStereoLayers();

	FLayerDesc LayerDesc;
	StereoLayers->GetLayerDesc(Splash.LayerId, LayerDesc);
	LayerDesc.Transform.SetRotation(LayerDesc.Transform.GetRotation() * Splash.Desc.DeltaRotation);
	LayerDesc.Transform.NormalizeRotation();
	StereoLayers->SetLayerDesc(Splash.LayerId, LayerDesc);
}

FSplashData::FSplashData(const FSplashDesc& InDesc) 
	: Desc(InDesc)
	, LayerId(FLayerDesc::INVALID_LAYER_ID)
{

}
