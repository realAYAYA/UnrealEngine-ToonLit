// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PlayerCore.h"
#include "MediaSubtitleDecoderOutput.h"

namespace Electra
{

class IAdaptiveStreamingPlayerSubtitleReceiver
{
public:
	virtual ~IAdaptiveStreamingPlayerSubtitleReceiver() = default;

	virtual void OnMediaPlayerSubtitleReceived(ISubtitleDecoderOutputPtr Subtitle) = 0;
	virtual void OnMediaPlayerFlushSubtitles() = 0;
};



} // namespace Electra

