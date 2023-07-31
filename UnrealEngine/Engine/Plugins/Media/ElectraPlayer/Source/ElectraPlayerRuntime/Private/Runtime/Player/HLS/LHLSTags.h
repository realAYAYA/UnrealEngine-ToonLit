// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MediaTag.h"
#include "CoreMinimal.h"

namespace Electra
{
    namespace HLSPlaylistParser
    {
        /**
         * LHLS namespace provides tags for the low-latency HLS (LHLS) by the open LHLS spec provided by hls.js team.
         * See: https://github.com/video-dev/hlsjs-rfcs/blob/lhls-spec/proposals/0001-lhls.md
         */
        namespace LHLS
        {
			/**
			 * Identifies prefetch segments.
			 *
			 * A prefetchable segment will appear in the segment list of a playlist and can be distinguished as such
			 * by checking for the tag using `Segment.ContainsTag(ExtXPrefetch)`.
			 */
            const FString ExtXPrefetch = "EXT-X-PREFETCH";
            const FString ExtXPrefetchDiscontinuity = "EXT-X-PREFETCH-DISCONTINUITY";

            /**
             * Provides the tags and their proper configuration, can be used by calling:
             *  Parser.Configure(HLSPlaylistParser::LHLS::TagMap);
             */
            const TMap<FString, HLSPlaylistParser::FMediaTag> TagMap = {
                    {ExtXPrefetch,              HLSPlaylistParser::FMediaTag(ExtXPrefetch,
                                                                             HLSPlaylistParser::TargetSegmentURL |
                                                                             HLSPlaylistParser::HasDirectValue | HLSPlaylistParser::URLDefinition)},
                    {ExtXPrefetchDiscontinuity, HLSPlaylistParser::FMediaTag(ExtXPrefetchDiscontinuity,
                                                                             HLSPlaylistParser::TargetSegmentURL)}
            };
        }
    }
} // namespace Electra

