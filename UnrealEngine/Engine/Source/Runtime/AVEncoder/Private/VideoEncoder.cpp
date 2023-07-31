// Copyright Epic Games, Inc. All Rights Reserved.

#include "VideoEncoder.h"
#include "VideoEncoderCommon.h"

DEFINE_LOG_CATEGORY(LogVideoEncoder);

namespace AVEncoder
{

FVideoEncoder::~FVideoEncoder()
{
	// calling virtual methods from the destructor is bad so 'DestroyLayer' was removed from here
	// its now up to the derived classes to clean up
	for (auto&& layer : Layers)
	{
		if (layer)
		{
			UE_LOG(LogVideoEncoder, Warning, TEXT("Encoder layer may not have been cleaned up!"));
			break;
		}
	}
}

bool FVideoEncoder::AddLayer(FLayerConfig const& config)
{
    if (GetNumLayers() >= GetMaxLayers())
    {
        UE_LOG(LogVideoEncoder, Error, TEXT("Encoder does not support more than %d layers."), GetMaxLayers());
        return false;
    }

    auto const newLayer = CreateLayer(Layers.Num(), config);
    if (nullptr != newLayer)
    {
        Layers.Push(newLayer);
        return true;
    }
    return false;
}

FVideoEncoder::FLayerConfig FVideoEncoder::GetLayerConfig(uint32 layerIdx) const
{
	if (layerIdx >= GetNumLayers())
	{
		UE_LOG(LogVideoEncoder, Error, TEXT("FVideoEncoder::GetLayerConfig: Layer index %d out of range."), layerIdx);
		return {};
	}

	return Layers[layerIdx]->GetConfig();
}

void FVideoEncoder::UpdateLayerConfig(uint32 layerIdx, FLayerConfig const& config)
{
    if (layerIdx >= GetNumLayers())
    {
        UE_LOG(LogVideoEncoder, Error, TEXT("FVideoEncoder::UpdateLayerConfig: Layer index %d out of range."), layerIdx);
        return;
    }

    Layers[layerIdx]->UpdateConfig(config);
}

} /* namespace AVEncoder */
