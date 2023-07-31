// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PlayerCore.h"
#include "StreamTypes.h"


namespace Electra
{

	class IPlayerStreamFilter
	{
	public:
		virtual ~IPlayerStreamFilter() = default;

		virtual bool CanDecodeStream(const FStreamCodecInformation& InStreamCodecInfo) const = 0;

		virtual bool CanDecodeSubtitle(const FString& MimeType, const FString& Codec) const = 0;
	};



} // namespace Electra


