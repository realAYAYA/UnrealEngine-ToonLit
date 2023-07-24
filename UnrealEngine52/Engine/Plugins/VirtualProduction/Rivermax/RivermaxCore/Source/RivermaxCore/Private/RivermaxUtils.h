// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "RivermaxTypes.h"

namespace UE::RivermaxCore
{
	struct FRivermaxOutputStreamOptions;
}

namespace UE::RivermaxCore::Private::Utils
{	
	/** Various constants used for stream initialization */

	static constexpr uint32 FullHDHeight = 1080;
	static constexpr uint32 FullHDWidth = 1920;

	/** Maximum payload we can send based on UDP max size and RTP header.  */
	static constexpr uint32 MaxPayloadSize = 1420;
	
	/** Smallest payload size to bound our search for a payload that can be equal across a line */
	static constexpr uint32 MinPayloadSize = 950;

	/** SMTPE 2110-10.The Media Clock and RTP Clock rate for streams compliant to this standard shall be 90 kHz. */
	static constexpr double MediaClockSampleRate = 90000.0;
	
	/** Common sleep time used in places where we are waiting for something to complete */
	static constexpr float SleepTimeSeconds = 50 * 1E-6;

	/** Convert a set of streaming option to its SDP description. Currently only support video type. */
	void StreamOptionsToSDPDescription(const UE::RivermaxCore::FRivermaxOutputStreamOptions& Options, FAnsiStringBuilderBase& OutSDPDescription);
}
