// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PlayerCore.h"

namespace Electra
{
	//! (FString) mime type of URL to load
	const TCHAR* const OptionKeyMimeType = TEXT("mime_type");										

	//! (int64) value indicating the bitrate to start playback with (initial start).
	const TCHAR* const OptionKeyInitialBitrate = TEXT("initial_bitrate");

	//! (int64) value indicating the bitrate to start buffering with when seeking (not at playback start)
	const TCHAR* const OptionKeySeekStartBitrate = TEXT("seekstart_bitrate");

	//! (int64) value indicating the bitrate to start rebuffering with
	const TCHAR* const OptionKeyRebufferStartBitrate = TEXT("rebufferstart_bitrate");

	//! (FTimeValue) value specifying how many seconds away from the Live media timeline the seekable range should end.
	const TCHAR* const OptionKeyLiveSeekableEndOffset = TEXT("seekable_range_live_end_offset");

	//! (bool) true to just finish the currently loading segment when rebuffering. false to start over with.
	const TCHAR* const OptionRebufferingContinuesLoading = TEXT("rebuffering_continues_loading");

	//! (bool) true to throw a playback error when rebuffering occurs, false to continue normally.
	const TCHAR* const OptionThrowErrorWhenRebuffering = TEXT("throw_error_when_rebuffering");

	//! (bool) true to perform frame accurate seeks (slow as decoding and discarding data from a preceeding keyframe is required)
	const TCHAR* const OptionKeyFrameAccurateSeek = TEXT("frame_accurate_seeking");

	//! (bool) true to optimize seeking for faster frame scrubbing, false to optimize for playback.
	const TCHAR* const OptionKeyFrameOptimizeSeekForScrubbing = TEXT("optimize_seek_for_scrubbing");
	
	//! (FTimeValue) absolute start time (including) of the range to limit playback to
	const TCHAR* const OptionPlayRangeStart = TEXT("play_range_start");

	//! (FTimeValue) absolute end time (excluding) of the range to limit playback to
	const TCHAR* const OptionPlayRangeEnd = TEXT("play_range_end");

	//! (bool) true to emit the first decoded video frame while prerolling so it can be displayed while scrubbing.
	const TCHAR* const OptionKeyDoNotHoldBackFirstVideoFrame = TEXT("do_not_hold_back_first_frame");
	


	const TCHAR* const OptionKeyCurrentAvgStartingVideoBitrate = TEXT("current:avg_video_bitrate");

} // namespace Electra


