// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PlayerCore.h"
#include "Player/PlaylistReader.h"
#include "Player/PlayerSessionServices.h"


namespace Electra
{

struct FPlaylistLoadRequestHLS;



class IPlaylistReaderHLS : public IPlaylistReader
{
public:
	static TSharedPtrTS<IPlaylistReader> Create(IPlayerSessionServices* PlayerSessionServices);

	virtual ~IPlaylistReaderHLS() = default;

	static const FString OptionKeyLiveSeekableStartOffset;				//!< (FTimeValue) value specifying how many seconds away from the Live media timeline the seekable range should start.
	static const FString OptionKeyLiveSeekableEndOffsetAudioOnly;		//!< (FTimeValue) value specifying how many seconds away from the Live media timeline the seekable range should end for audio-only playlists.
	static const FString OptionKeyLiveSeekableEndOffsetBeConservative;	//!< (bool) true to use a larger Live edge distance, false to go with the smaller absolute difference
	static const FString OptionKeyMasterPlaylistLoadConnectTimeout;		//!< (FTimeValue) value specifying connection timeout fetching the master playlist
	static const FString OptionKeyMasterPlaylistLoadNoDataTimeout;		//!< (FTimeValue) value specifying no-data timeout fetching the master playlist
	static const FString OptionKeyVariantPlaylistLoadConnectTimeout;	//!< (FTimeValue) value specifying connection timeout fetching a variant playlist the first time
	static const FString OptionKeyVariantPlaylistLoadNoDataTimeout;		//!< (FTimeValue) value specifying no-data timeout fetching a variant playlist the first time
	static const FString OptionKeyUpdatePlaylistLoadConnectTimeout;		//!< (FTimeValue) value specifying connection timeout fetching a variant playlist repeatedly
	static const FString OptionKeyUpdatePlaylistLoadNoDataTimeout;		//!< (FTimeValue) value specifying no-data timeout fetching a variant playlist repeatedly

	/**
	 * Loads and parses the playlist.
	 *
	 * @param URL     URL of the playlist to load
	 */
	virtual void LoadAndParse(const FString& URL) = 0;

	/**
	 * Returns the URL from which the playlist was loaded (or supposed to be loaded).
	 *
	 * @return The playlist URL
	 */
	virtual FString GetURL() const = 0;


	/**
	 * Requests loading of a variant/rendition playlist.
	 *
	 * @param LoadRequest
	 */
	virtual void RequestPlaylistLoad(const FPlaylistLoadRequestHLS& LoadRequest) = 0;
};


} // namespace Electra



