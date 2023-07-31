// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PlayerCore.h"


namespace Electra
{

	namespace Facility
	{

		enum class EFacility
		{
			// NOTE: Never change this order even if adding new elements mean they go at the end!
			Unknown,
			Utility,
			Player,
			ABR,
			HTTPReader,
			HLSPlaylistReader,
			HLSPlaylistBuilder,
			HLSManifest,
			HLSFMP4Reader,
			MP4Parser,
			VideoRenderer,
			AudioRenderer,
			SubtitleRenderer,
			AACDecoder,
			H264Decoder,
			MP4PlaylistReader,
			MP4Playlist,
			MP4StreamReader,
			LicenseKey,
			DASHMPDReader,
			DASHMPDParser,
			DASHMPDBuilder,
			DASHManifest,
			DASHFMP4Reader,
			EntityCache,
			DRM,
			H265Decoder,
			SubtitleDecoder,
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
				case EFacility::AACDecoder:
					return TEXT("AAC decoder");
				case EFacility::H264Decoder:
					return TEXT("H.264 decoder");
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
				case EFacility::DASHFMP4Reader:
					return TEXT("DASH fmp4 reader");
				case EFacility::EntityCache:
					return TEXT("Entity cache");
				case EFacility::DRM:
					return TEXT("DRM");
				case EFacility::H265Decoder:
					return TEXT("H.265 decoder");
				case EFacility::SubtitleDecoder:
					return TEXT("Subtitle decoder");
			}
			return TEXT("???");
		}

	} // namespace Facility

} // namespace Electra

