// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PlayerCore.h"


namespace Electra
{

	namespace Facility
	{

		enum class EFacility
		{
			Unknown,
			Utility,
			Player,
			ABR,
			HTTPReader,
			BufferedDataReader,
			HLSPlaylistReader,
			HLSPlaylistBuilder,
			HLSManifest,
			HLSFMP4Reader,
			MP4Parser,
			VideoRenderer,
			AudioRenderer,
			SubtitleRenderer,
			MP4PlaylistReader,
			MP4Playlist,
			MP4StreamReader,
			MKVPlaylistReader,
			MKVPlaylist,
			MKVStreamReader,
			MKVParser,
			LicenseKey,
			DASHMPDReader,
			DASHMPDParser,
			DASHMPDBuilder,
			DASHManifest,
			DASHStreamReader,
			EntityCache,
			DRM,
			SubtitleDecoder,
			AudioDecoder,
			VideoDecoder,
			LastEntry = 255
		};

		static const TCHAR* GetName(EFacility InFacility)
		{
			switch (InFacility)
			{
				case EFacility::LastEntry:
					return TEXT("???");
				case EFacility::Unknown:
					return TEXT("Unknown");
				case EFacility::Utility:
					return TEXT("Utility");
				case EFacility::Player:
					return TEXT("Player");
				case EFacility::ABR:
					return TEXT("ABR");
				case EFacility::HTTPReader:
					return TEXT("HTTP reader");
				case EFacility::BufferedDataReader:
					return TEXT("Buffered data reader");
				case EFacility::HLSPlaylistReader:
					return TEXT("HLS playlist reader");
				case EFacility::HLSPlaylistBuilder:
					return TEXT("HLS playlist builder");
				case EFacility::HLSManifest:
					return TEXT("HLS manifest");
				case EFacility::HLSFMP4Reader:
					return TEXT("HLS fmp4 reader");
				case EFacility::MP4Parser:
					return TEXT("MP4 parser");
				case EFacility::VideoRenderer:
					return TEXT("Video renderer");
				case EFacility::AudioRenderer:
					return TEXT("Audio renderer");
				case EFacility::SubtitleRenderer:
					return TEXT("Subtitle renderer");
				case EFacility::MP4PlaylistReader:
					return TEXT("MP4 playlist reader");
				case EFacility::MP4Playlist:
					return TEXT("MP4 playlist");
				case EFacility::MP4StreamReader:
					return TEXT("MP4 reader");
				case EFacility::LicenseKey:
					return TEXT("License key");
				case EFacility::DASHMPDReader:
					return TEXT("DASH MPD reader");
				case EFacility::DASHMPDParser:
					return TEXT("DASH MPD parser");
				case EFacility::DASHMPDBuilder:
					return TEXT("DASH MPD builder");
				case EFacility::DASHManifest:
					return TEXT("DASH manifest");
				case EFacility::DASHStreamReader:
					return TEXT("DASH stream reader");
				case EFacility::EntityCache:
					return TEXT("Entity cache");
				case EFacility::DRM:
					return TEXT("DRM");
				case EFacility::SubtitleDecoder:
					return TEXT("Subtitle decoder");
				case EFacility::AudioDecoder:
					return TEXT("Audio decoder");
				case EFacility::VideoDecoder:
					return TEXT("Video decoder");
				case EFacility::MKVPlaylistReader:
					return TEXT("MKV playlist reader");
				case EFacility::MKVPlaylist:
					return TEXT("MKV playlist");
				case EFacility::MKVStreamReader:
					return TEXT("MKV reader");
				case EFacility::MKVParser:
					return TEXT("MKV parser");
			}
			return TEXT("???");
		}

	} // namespace Facility

} // namespace Electra

