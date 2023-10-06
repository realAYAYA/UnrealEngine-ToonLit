// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Logging/LogMacros.h"

namespace UE::Learning
{
	/**
	* Log Settings
	*/
	enum class ELogSetting : uint8
	{
		// Logs basic information
		Normal,

		// No logging
		Silent,
	};
}

LEARNING_API DECLARE_LOG_CATEGORY_EXTERN(LogLearning, Log, All);
