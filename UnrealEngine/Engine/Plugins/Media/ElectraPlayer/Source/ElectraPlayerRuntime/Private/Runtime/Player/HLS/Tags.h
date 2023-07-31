// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace Electra
{
    namespace HLSPlaylistParser
    {
        // Media Segment related tags.
        const FString ExtXByteRange = TEXT("EXT-X-BYTERANGE");
        const FString ExtXVersion = TEXT("EXT-X-VERSION");
        const FString ExtINF = TEXT("EXTINF");
        const FString ExtXDiscontinuity = TEXT("EXT-X-DISCONTINUITY");
        const FString ExtXKey = TEXT("EXT-X-KEY");
        const FString ExtXMap = TEXT("EXT-X-MAP");
        const FString ExtXProgramDateTime = TEXT("EXT-X-PROGRAM-DATE-TIME");
        const FString ExtXDateRange = TEXT("EXT-X-DATERANGE");
        // Playlist related tags for playlists of Media Segments.
        const FString ExtXTargetDuration = TEXT("EXT-X-TARGETDURATION");
        const FString ExtXMediaSequence = TEXT("EXT-X-MEDIA-SEQUENCE");
        const FString ExtXDiscontinuitySequence = TEXT("EXT-X-DISCONTINUITY-SEQUENCE");
        const FString ExtXEndlist = TEXT("EXT-X-ENDLIST");
        const FString ExtXPlaylistType = TEXT("EXT-X-PLAYLIST-TYPE");
        const FString ExtXIFramesOnly = TEXT("EXT-X-I-FRAMES-ONLY");
        // Master Playlist related tags.
        const FString ExtXMedia = TEXT("EXT-X-MEDIA");
        const FString ExtXStreamInf = TEXT("EXT-X-STREAM-INF");
        const FString ExtXIFrameStreamInf = TEXT("EXT-X-I-FRAME-STREAM-INF");
        const FString ExtXSessionData = TEXT("EXT-X-SESSION-DATA");
        const FString ExtXSessionKey = TEXT("EXT-X-SESSION-KEY");
        // Master or Media Playlist related tags which are applied to the playlist.
        const FString ExtXIndependentSegments = TEXT("EXT-X-INDEPENDENT-SEGMENTS");
        const FString ExtXStart = TEXT("EXT-X-START");
        const FString ExtXAllowCache = TEXT("EXT-X-ALLOW-CACHE");
    }
} // namespace Electra



