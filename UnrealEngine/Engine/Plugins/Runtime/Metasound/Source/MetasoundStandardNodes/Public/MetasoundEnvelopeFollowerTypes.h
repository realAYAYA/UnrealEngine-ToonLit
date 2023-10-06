// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MetasoundFacade.h"
#include "MetasoundEnumRegistrationMacro.h"

namespace Metasound
{
	enum class EEnvelopePeakMode
	{
		Peak = 0,
		MeanSquared,
		RootMeanSquared,
	};

	DECLARE_METASOUND_ENUM(EEnvelopePeakMode, EEnvelopePeakMode::Peak, METASOUNDSTANDARDNODES_API,
	FEnumEnvelopePeakMode, FEnumEnvelopePeakModeInfo, FEnvelopePeakModeReadRef, FEnumEnvelopePeakModeWriteRef);
}