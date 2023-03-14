// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

struct FMediaPlayerQueueDepths
{
	static const int32 MaxAudioSinkDepth = 512;
	static const int32 MaxCaptionSinkDepth = 256;
	static const int32 MaxMetadataSinkDepth = 256;
	static const int32 MaxSubtitleSinkDepth = 256;
	static const int32 MaxVideoSinkDepth = 8;
};
