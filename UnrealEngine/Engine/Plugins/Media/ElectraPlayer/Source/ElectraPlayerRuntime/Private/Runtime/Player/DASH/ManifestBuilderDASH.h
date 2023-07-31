// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PlayerCore.h"
#include "Player/PlaybackTimeline.h"
#include "Player/PlaylistReader.h"
#include "Player/PlayerSessionServices.h"
#include "PlaylistReaderDASH_Internal.h"
#include "StreamTypes.h"
#include "ErrorDetail.h"


namespace Electra
{

class IManifestBuilderDASH
{
public:
	static IManifestBuilderDASH* Create(IPlayerSessionServices* PlayerSessionServices);

	virtual ~IManifestBuilderDASH() = default;

	/**
	 * Builds a new internal manifest from a DASH MPD
	 *
	 * @param OutMPD
	 * @param InOutMPDXML
	 * @param EffectiveURL
	 * @param ETag
	 *
	 * @return
	 */
	virtual FErrorDetail BuildFromMPD(TSharedPtrTS<FManifestDASHInternal>& OutMPD, TCHAR* InOutMPDXML, const FString& EffectiveURL, const FString& ETag) = 0;
};


} // namespace Electra




