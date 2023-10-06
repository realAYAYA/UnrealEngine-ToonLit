// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "HAL/Platform.h"
#include "Templates/Function.h"

class FFeedbackContext;

namespace UE::Testing
{
	struct FFilterFeedback;

	///@brief Scope that captures warning log messages
	///necessary to filter out warning message from appearing the log file as Horde will flag the warnings
	struct FWarnFilterScope
	{
		/// @param LogHandler function to filter log messages. returning true filters message from log file
		FWarnFilterScope(TFunction<bool(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category)> LogHandler);
		FWarnFilterScope(const FWarnFilterScope&) = delete;
		FWarnFilterScope& operator=(const FWarnFilterScope&) = delete;

		~FWarnFilterScope();
	private:
		FFeedbackContext* OldWarn;
		FFilterFeedback* Feedback;
	};

}