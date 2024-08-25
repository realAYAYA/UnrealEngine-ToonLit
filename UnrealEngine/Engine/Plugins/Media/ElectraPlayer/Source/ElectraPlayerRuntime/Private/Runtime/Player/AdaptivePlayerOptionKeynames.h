// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PlayerCore.h"

namespace Electra
{
	//! (FString) Use of worker threads. ("shared", "worker" or "worker_and_events")
	const FName OptionKeyWorkerThreads(TEXT("worker_threads"));

	//! (FString) mime type of URL to load
	const FName OptionKeyMimeType(TEXT("mime_type"));

	//! (int64) value indicating the bitrate to start playback with (initial start).
	const FName OptionKeyInitialBitrate(TEXT("initial_bitrate"));

	//! (int64) value indicating the bitrate to start buffering with when seeking (not at playback start)
	const FName OptionKeySeekStartBitrate(TEXT("seekstart_bitrate"));

	//! (int64) value indicating the bitrate to start rebuffering with
	const FName OptionKeyRebufferStartBitrate(TEXT("rebufferstart_bitrate"));

	//! (FTimeValue) value specifying how many seconds away from the Live media timeline the seekable range should end.
	const FName OptionKeyLiveSeekableEndOffset(TEXT("seekable_range_live_end_offset"));

	//! (bool) true to just finish the currently loading segment when rebuffering. false to start over with.
	const FName OptionRebufferingContinuesLoading(TEXT("rebuffering_continues_loading"));

	//! (bool) true to throw a playback error when rebuffering occurs, false to continue normally.
	const FName OptionThrowErrorWhenRebuffering(TEXT("throw_error_when_rebuffering"));

	//! (bool) true to perform frame accurate seeks (slow as decoding and discarding data from a preceeding keyframe is required)
	const FName OptionKeyFrameAccurateSeek(TEXT("frame_accurate_seeking"));

	//! (bool) true to optimize seeking for faster frame scrubbing, false to optimize for playback.
	const FName OptionKeyFrameOptimizeSeekForScrubbing(TEXT("optimize_seek_for_scrubbing"));

	//! (bool) true to allow a new scrubbing seek to cancel an ongoing scrubbing seek. Non-scrubbing seeks always cancel pending seeks.
	const FName OptionKeyNewScrubbingSeekCancelsCurrent(TEXT("new_scrubbing_seek_cancels_current"));

	//! (bool) true to emit the first decoded video frame while prerolling so it can be displayed while scrubbing.
	const FName OptionKeyDoNotHoldBackFirstVideoFrame(TEXT("do_not_hold_back_first_frame"));

	//! (bool) true to not truncate the media segment access units at the end of the presentation. Must only be used without a set playback range end!
	const FName OptionKeyDoNotTruncateAtPresentationEnd(TEXT("do_not_truncate_at_presentation_end"));

	//! (bool) true to have every request to read data break out to an external data reader.
	const FName OptionKeyUseExternalDataReader(TEXT("use_external_data_reader"));

	const FName OptionKeyHoldCurrentFrame(TEXT("hold_current_frame"));

	const FName OptionKeyCurrentAvgStartingVideoBitrate(TEXT("current:avg_video_bitrate"));

	const FName OptionKeyExcludedCodecsVideo(TEXT("excluded_codecs_video"));
	const FName OptionKeyExcludedCodecsAudio(TEXT("excluded_codecs_audio"));
	const FName OptionKeyExcludedCodecsSubtitles(TEXT("excluded_codecs_subtitles"));

	const FName OptionKeyPreferredCodecsVideo(TEXT("preferred_codecs_video"));
	const FName OptionKeyPreferredCodecsAudio(TEXT("preferred_codecs_audio"));
	const FName OptionKeyPreferredCodecsSubtitles(TEXT("preferred_codecs_subtitles"));

} // namespace Electra


