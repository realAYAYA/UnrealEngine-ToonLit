// Copyright Epic Games, Inc. All Rights Reserved.

#include "Exceptions.h"

#include "ClassMaps.h"
#include "BaseParser.h"
#include "UnrealHeaderTool.h"
#include "UnrealHeaderToolGlobals.h"
#include "UnrealSourceFile.h"
#include "UnrealTypeDefinitionInfo.h"

#include "CoreGlobals.h"
#include "Async/TaskGraphInterfaces.h"
#include "HAL/FileManager.h"
#include "Misc/FeedbackContext.h"
#include "Misc/ScopeLock.h"

#include <atomic>

FUHTException::FUHTException(ECompilationResult::Type InResult, const FUHTMessageProvider& InContext, FString&& InMessage)
	: Result(InResult)
	, Message(MoveTemp(InMessage))
	, Filename(InContext.GetFilename())
	, Line(InContext.GetLineNumber())
{
}

namespace UE::UnrealHeaderTool::Exceptions::Private
{
	std::atomic<ECompilationResult::Type> OverallResults = ECompilationResult::Succeeded;
	std::atomic<int32> NumFailures = 0;
	std::atomic<bool> OverallWarnings = false;
	std::atomic<int32> NumWarnings = 0;
	FGraphEventArray ErrorTasks;
	FCriticalSection ErrorTasksCS;

	void LogErrorInternal(ECompilationResult::Type InResult, const FString& Filename, int32 Line, const FString& Message)
	{
		TGuardValue<ELogTimes::Type> DisableLogTimes(GPrintLogTimes, ELogTimes::None);

		FString FormattedErrorMessage;
		if (Filename.IsEmpty())
		{
			FormattedErrorMessage = FString::Printf(TEXT("Error: %s\r\n"), *Message);
		}
		else
		{
			FormattedErrorMessage = FString::Printf(TEXT("%s(%d): Error: %s\r\n"), *Filename, Line, *Message);
		}

		UE_LOG(LogCompile, Log, TEXT("%s"), *FormattedErrorMessage);
		GWarn->Log(ELogVerbosity::Error, FormattedErrorMessage);

		FResults::SetResult(InResult);
	}

	void LogErrorOuter(ECompilationResult::Type InResult, const FString& InFilename, int32 Line, FString&& Message)
	{
		using namespace UE::UnrealHeaderTool::Exceptions::Private;

		FString Filename = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*InFilename);

		if (IsInGameThread())
		{
			LogErrorInternal(InResult, Filename, Line, Message);
		}
		else
		{
			auto LogWarningTask = [Filename = MoveTemp(Filename), Line, Message = MoveTemp(Message), InResult]()
			{
				LogErrorInternal(InResult, Filename, Line, Message);
			};

			FGraphEventRef EventRef = FFunctionGraphTask::CreateAndDispatchWhenReady(MoveTemp(LogWarningTask), TStatId(), nullptr, ENamedThreads::GameThread);

			FScopeLock Lock(&ErrorTasksCS);
			ErrorTasks.Add(EventRef);
		}
	}

	void LogWarningInternal(const FString& Filename, int32 Line, const FString& Message)
	{
		TGuardValue<ELogTimes::Type> DisableLogTimes(GPrintLogTimes, ELogTimes::None);

		FString FormattedErrorMessage;
		if (Filename.IsEmpty())
		{
			FormattedErrorMessage = FString::Printf(TEXT("Warning: %s\r\n"), *Message);
		}
		else
		{
			FormattedErrorMessage = FString::Printf(TEXT("%s(%d): Warning: %s\r\n"), *Filename, Line, *Message);
		}

		UE_LOG(LogCompile, Log, TEXT("%s"), *FormattedErrorMessage);
		GWarn->Log(ELogVerbosity::Warning, FormattedErrorMessage);

		FResults::MarkWarning();
	}

	void LogInfoInternal(const FString& Filename, int32 Line, const FString& Message)
	{
		TGuardValue<ELogTimes::Type> DisableLogTimes(GPrintLogTimes, ELogTimes::None);

		FString FormattedErrorMessage;
		if (Filename.IsEmpty())
		{
			FormattedErrorMessage = FString::Printf(TEXT("Info: %s\r\n"), *Message);
		}
		else
		{
			FormattedErrorMessage = FString::Printf(TEXT("%s(%d): Info: %s\r\n"), *Filename, Line, *Message);
		}

		UE_LOG(LogCompile, Log, TEXT("%s"), *FormattedErrorMessage);
		GWarn->Log(ELogVerbosity::Display, FormattedErrorMessage);
	}

}

void FResults::LogError(const FUHTMessageProvider& Context, const TCHAR* ErrorMsg, ECompilationResult::Type InResult)
{
	using namespace UE::UnrealHeaderTool::Exceptions::Private;
	LogErrorOuter(InResult, Context.GetFilename(), Context.GetLineNumber(), FString(ErrorMsg));
}

void FResults::LogError(const FUHTException& Ex)
{
	using namespace UE::UnrealHeaderTool::Exceptions::Private;
	LogErrorOuter(Ex.GetResult(), Ex.GetFilename(), Ex.GetLine(), FString(Ex.GetMessage()));
}

void FResults::LogError(const TCHAR* Message)
{
	using namespace UE::UnrealHeaderTool::Exceptions::Private;
	LogErrorOuter(ECompilationResult::Type::OtherCompilationError, TEXT("UnknownFile"), 1, FString(Message));
}

void FResults::LogWarning(const FUHTMessageProvider& Context, const TCHAR* ErrorMsg)
{
	using namespace UE::UnrealHeaderTool::Exceptions::Private;

	FString Filename = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*Context.GetFilename());
	int32 Line = Context.GetLineNumber();

	if (IsInGameThread())
	{
		LogWarningInternal(Filename, Line, ErrorMsg);
	}
	else
	{
		auto LogWarningTask = [Filename = MoveTemp(Filename), Line, Message = FString(ErrorMsg)]()
		{
			LogWarningInternal(Filename, Line, Message);
		};

		FGraphEventRef EventRef = FFunctionGraphTask::CreateAndDispatchWhenReady(MoveTemp(LogWarningTask), TStatId(), nullptr, ENamedThreads::GameThread);

		FScopeLock Lock(&ErrorTasksCS);
		ErrorTasks.Add(EventRef);
	}
}

void FResults::LogInfo(const FUHTMessageProvider& Context, const TCHAR* InfoMsg)
{
	using namespace UE::UnrealHeaderTool::Exceptions::Private;

	FString Filename = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*Context.GetFilename());
	int32 Line = Context.GetLineNumber();

	if (IsInGameThread())
	{
		LogInfoInternal(Filename, Line, InfoMsg);
	}
	else
	{
		auto LogInfoTask = [Filename = MoveTemp(Filename), Line, Message = FString(InfoMsg)]()
		{
			LogInfoInternal(Filename, Line, Message);
		};

		FGraphEventRef EventRef = FFunctionGraphTask::CreateAndDispatchWhenReady(MoveTemp(LogInfoTask), TStatId(), nullptr, ENamedThreads::GameThread);

		FScopeLock Lock(&ErrorTasksCS);
		ErrorTasks.Add(EventRef);
	}
}

void FResults::WaitForErrorTasks()
{
	using namespace UE::UnrealHeaderTool::Exceptions::Private;

	FGraphEventArray LocalExceptionTasks;
	{
		FScopeLock Lock(&ErrorTasksCS);
		LocalExceptionTasks = MoveTemp(ErrorTasks);
	}
	FTaskGraphInterface::Get().WaitUntilTasksComplete(LocalExceptionTasks);
}

bool FResults::IsSucceeding()
{
	using namespace UE::UnrealHeaderTool::Exceptions::Private;

	return OverallResults == ECompilationResult::Succeeded;
}

void FResults::SetResult(ECompilationResult::Type InResult)
{
	using namespace UE::UnrealHeaderTool::Exceptions::Private;

	checkf(InResult != ECompilationResult::Succeeded, TEXT("The results can't be set to succeeded."));
	OverallResults = InResult;
	++NumFailures;
}

ECompilationResult::Type FResults::GetResults()
{
	using namespace UE::UnrealHeaderTool::Exceptions::Private;

	return OverallResults;
}

void FResults::MarkWarning()
{
	using namespace UE::UnrealHeaderTool::Exceptions::Private;
	OverallWarnings = true;
	++NumWarnings;
}

ECompilationResult::Type FResults::GetOverallResults()
{
	using namespace UE::UnrealHeaderTool::Exceptions::Private;

	// For some legacy reason, we don't actually return the result
	if (OverallResults != ECompilationResult::Succeeded || NumFailures > 0 || (NumWarnings > 0 && GWarn->TreatWarningsAsErrors))
	{
		return ECompilationResult::OtherCompilationError;
	}
	return OverallResults;
}
