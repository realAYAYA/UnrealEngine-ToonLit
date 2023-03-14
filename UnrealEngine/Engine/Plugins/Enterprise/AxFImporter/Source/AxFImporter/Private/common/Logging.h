// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Logging/LogMacros.h"
#include "Templates/Tuple.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAxFImporter, Log, All);

namespace AxFImporterLogging
{
	enum class EMessageSeverity
	{
		Warning,
		Error,
	};
	using FLogMessage = TTuple<EMessageSeverity, FString>;
}
