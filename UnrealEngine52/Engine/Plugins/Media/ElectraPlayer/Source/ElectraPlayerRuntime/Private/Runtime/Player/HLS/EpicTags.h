// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MediaTag.h"
#include "CoreMinimal.h"

namespace Electra
{
	namespace HLSPlaylistParser
	{
		/**
		 * Epic namespace provides Tags specific to the epicgames streaming backend.
		 */
		namespace Epic
		{
			const FString ExtXEpicGamesCustom = "EXT-X-EPICGAMES-CUSTOM";

			const TMap<FString, HLSPlaylistParser::FMediaTag> TagMap = {
					{ExtXEpicGamesCustom, HLSPlaylistParser::FMediaTag(ExtXEpicGamesCustom, HLSPlaylistParser::TargetPlaylistURL |
																					   HLSPlaylistParser::HasAttributeList)},
			};
		}
	}
} // namespace Electra