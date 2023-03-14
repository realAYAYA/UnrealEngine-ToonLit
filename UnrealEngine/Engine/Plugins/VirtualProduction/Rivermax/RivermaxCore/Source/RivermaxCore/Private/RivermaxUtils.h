// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "RivermaxTypes.h"

namespace UE::RivermaxCore
{
	struct FRivermaxStreamOptions;
}

namespace UE::RivermaxCore::Private::Utils
{	
	/** Various constants used for stream initialization */
	static constexpr uint32 RTPHeaderSize = 20;
	static constexpr uint32 FullHDHeight = 1080;
	static constexpr uint32 FullHDWidth = 1920;

	/** Maximum payload we can send based on UDP max size and RTP header.  */
	static constexpr uint32 MaxPayloadSize = 1440;
	
	/** Smallest payload size to bound our search for a payload that can be equal across a line */
	static constexpr uint32 MinPayloadSize = 950;

	/** Convert a set of streaming option to its SDP description. Currently only support video type. */
	void StreamOptionsToSDPDescription(const UE::RivermaxCore::FRivermaxStreamOptions& Options, FAnsiStringBuilderBase& OutSDPDescription);
}
