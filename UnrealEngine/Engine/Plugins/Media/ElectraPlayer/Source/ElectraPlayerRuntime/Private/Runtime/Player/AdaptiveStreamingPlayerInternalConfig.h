// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Decoder/VideoDecoderH264.h"
#include "Decoder/VideoDecoderH265.h"
#include "Decoder/AudioDecoderAAC.h"
#include "StreamAccessUnitBuffer.h"


namespace Electra
{
	namespace AdaptiveStreamingPlayerConfig
	{
		struct FConfiguration
		{
			FConfiguration()
			{
				// TODO: set certain configuration items for this type of player that make sense (like image size auto config)

				WorkerThread.Priority = TPri_Normal;
				WorkerThread.StackSize = 32768;
				WorkerThread.CoreAffinity = -1;

				// Set default values to maximum permitted values.
				StreamBufferConfigVideo.MaxDataSize = 16 << 20;
				StreamBufferConfigVideo.MaxDuration.SetFromSeconds(20.0);

				StreamBufferConfigAudio.MaxDataSize = 2 << 20;
				StreamBufferConfigAudio.MaxDuration.SetFromSeconds(20.0);

				// Subtitle streams tend to be sparse and each AU could potentially have
				// a huge duration, so we need to ignore durations altogether.
				StreamBufferConfigText.MaxDataSize = 8 << 20;
				StreamBufferConfigText.MaxDuration.SetToPositiveInfinity();

				bHoldLastFrameDuringSeek = true;

				InitialBufferMinTimeAvailBeforePlayback = 5.0;
				SeekBufferMinTimeAvailBeforePlayback = 5.0;
				RebufferMinTimeAvailBeforePlayback = 5.0;
			}

			struct FVideoDecoderLimit
			{
				struct FTierProfileLevel
				{
					int32 Tier = 0;
					int32 Profile = 0;
					int32 Level = 0;
				};
				FStreamCodecInformation::FResolution	MaxResolution;
				FTierProfileLevel						MaxTierProfileLevel;
			};


			FMediaRunnable::Param								WorkerThread;

			FAccessUnitBuffer::FConfiguration					StreamBufferConfigVideo;
			FAccessUnitBuffer::FConfiguration					StreamBufferConfigAudio;
			FAccessUnitBuffer::FConfiguration					StreamBufferConfigText;

			IVideoDecoderH264::FInstanceConfiguration			DecoderCfg264;
			IAudioDecoderAAC::FInstanceConfiguration			DecoderCfgAAC;

			FVideoDecoderLimit									H264LimitUpto30fps;
			FVideoDecoderLimit									H264LimitAbove30fps;

			bool												bHoldLastFrameDuringSeek;

			double												InitialBufferMinTimeAvailBeforePlayback;
			double												SeekBufferMinTimeAvailBeforePlayback;
			double												RebufferMinTimeAvailBeforePlayback;
		};

	} // namespace AdaptiveStreamingPlayerConfig

} // namespace Electra


