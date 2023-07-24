// Copyright Epic Games, Inc. All Rights Reserved.

#include "Parser.h"

namespace Electra
{
    namespace HLSPlaylistParser
    {
        void FPlaylist::AddSegment(HLSPlaylistParser::FMediaSegment& Segment)
        {
            Segments.Add(MoveTemp(Segment));
        }

        void FPlaylist::AddPlaylist(FMediaPlaylist& Playlist)
        {
            Playlists.Add(MoveTemp(Playlist));
        }
    }
} // namespace Electra

