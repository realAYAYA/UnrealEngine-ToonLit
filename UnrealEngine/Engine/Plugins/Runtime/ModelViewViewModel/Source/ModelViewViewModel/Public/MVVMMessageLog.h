// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Logging/MessageLog.h"

#include "UObject/WeakObjectPtr.h"

class UUserWidget;

MODELVIEWVIEWMODEL_API DECLARE_LOG_CATEGORY_EXTERN(LogMVVM, Log, All);

namespace UE::MVVM
{
	class MODELVIEWVIEWMODEL_API FMessageLog : private ::FMessageLog
	{
	public:
		static const FName LogName;

		FMessageLog(const UUserWidget* InUserWidget)
			: ::FMessageLog(LogName)
		{}

		using ::FMessageLog::AddMessage;
		using ::FMessageLog::AddMessages;
		using ::FMessageLog::Message;
		using ::FMessageLog::CriticalError;
		using ::FMessageLog::Error;
		using ::FMessageLog::PerformanceWarning;
		using ::FMessageLog::Warning;
		using ::FMessageLog::Info;

	private:
		TWeakObjectPtr<UUserWidget> UserWidget;
	};
}
