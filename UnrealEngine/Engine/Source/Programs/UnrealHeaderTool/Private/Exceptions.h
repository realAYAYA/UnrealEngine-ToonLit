// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/UnrealString.h"
#include "Misc/CompilationResult.h"
#include "ProfilingDebugging/ScopedTimers.h"

class FBaseParser;
class FUnrealSourceFile;
class FUnrealTypeDefinitionInfo;

class FUHTMessageProvider;

class FUHTException
{
public:

	/**
	 * Generate an exception
	 * @param InContext The context of the exception
	 * @param InFmt The format string
	 * @param InArgs InMessage The text of the message
	 */
	FUHTException(ECompilationResult::Type InResult, const FUHTMessageProvider& InContext, FString&& InMessage);

	/**
	 * Return the result code of the exception
	 */
	ECompilationResult::Type GetResult() const 
	{ 
		return Result; 
	}

	/**
	 * Return the filename of the exception.
	 */
	const TCHAR* GetFilename() const 
	{
		return *Filename; 
	}

	/**
	 * Return the line number in the file of the exception
	 */
	int32 GetLine() const
	{
		return Line;
	}

	/**
	 * Return the message of the exception
	 */
	const FString& GetMessage() const 
	{ 
		return Message; 
	}

private:
	ECompilationResult::Type Result = ECompilationResult::OtherCompilationError;
	FString Message;
	FString Filename;
	int32 Line;
};

/** Helper methods for working with exceptions and compilation results */
struct FResults
{
	friend class FUHTMessageProvider;

	/**
	 * Wait for any pending error tasks to complete.
	 * 
	 * When job threads are used to log errors or throw exceptions, those errors and exceptions are collected by the main game thread.
	 * After waiting for all the pending jobs to complete, invoke this method to ensure that all pending errors and exceptions have
	 * been collected.
	 */
	static void WaitForErrorTasks();

	/**
	 * Test to see if no errors or exceptions have been posted.
	 */
	static bool IsSucceeding();

	/**
	 * Set the overall results.
	 */
	static void SetResult(ECompilationResult::Type InResult);

	/**
	 * Get the current results without processing for overall result
	 */
	static ECompilationResult::Type GetResults();

	/**
	 * Mark that a warning has happened
	 */
	static void MarkWarning();

	/**
	 * Get the overall results to be returned from compilation.
	 */
	static ECompilationResult::Type GetOverallResults();

	/**
	 * Log an error
	 * @param Ex The exception generating the error
	 */
	static void LogError(const FUHTException& Ex);

	/**
	 * Log an error
	 * @param Text
	 */
	static void LogError(const TCHAR* Text);

	/**
	 * Log an error for the given source file where the type is defined
	 * @param Context The context of the exception
	 * @param ErrorMsg The text of the error
	 * @param Result Compilation result of the error
	 */
	static void LogError(const FUHTMessageProvider& Context, const TCHAR* ErrorMsg, ECompilationResult::Type Result);

	/**
	 * Log a warning for the given source file where the type is defined
	 * @param Context The context of the warning
	 * @param ErrorMsg The text of the warning
	 * @param Result Compilation result of the warning
	 */
	static void LogWarning(const FUHTMessageProvider& Context, const TCHAR* ErrorMsg);

	/**
	 * Log an info for the given source file where the type is defined
	 * @param Context The context of the warning
	 * @param InfoMsg The text of the information
	 */
	static void LogInfo(const FUHTMessageProvider& Context, const TCHAR* InfoMsg);

	/**
	 * Invoke the given lambda in a try block catching all supported exception types.
	 * @param InLambda The code to be executed in the try block
	 */
	template<typename Lambda>
	static void Try(Lambda&& InLambda)
	{
		if (IsSucceeding())
		{
#if !PLATFORM_EXCEPTIONS_DISABLED
			try
#endif
			{
				InLambda();
			}
#if !PLATFORM_EXCEPTIONS_DISABLED
			catch (const FUHTException& Ex)
			{
				LogError(Ex);
			}
			catch (const TCHAR* ErrorMsg)
			{
				LogError(ErrorMsg);
			}
#endif
		}
	}

	/**
	 * Invoke the given lambda in a try block catching all supported exception types.
	 * @param InLambda The code to be executed in the try block
	 */
	template<typename Lambda>
	static void TryAlways(Lambda&& InLambda)
	{
#if !PLATFORM_EXCEPTIONS_DISABLED
		try
#endif
		{
			InLambda();
		}
#if !PLATFORM_EXCEPTIONS_DISABLED
		catch (const FUHTException& Ex)
		{
			LogError(Ex);
		}
		catch (const TCHAR* ErrorMsg)
		{
			LogError(ErrorMsg);
		}
#endif
	}

	/**
	 * Invoke the given lambda in a try block catching all supported exception types.
	 * @param InLambda The code to be executed in the try block
	 * @return The time in seconds it took to execute the lambda
	 */
	template<typename Lambda>
	static double TimedTry(Lambda&& InLambda)
	{
		double DeltaTime = 0.0;
		{
			FScopedDurationTimer Timer(DeltaTime);
			Try(InLambda);
		}
		return DeltaTime;
	}
};

class FUHTMessageProvider
{
public:
	virtual ~FUHTMessageProvider() = default;
	virtual FString GetFilename() const = 0;
	virtual int32 GetLineNumber() const = 0;

	/**
	 * Generate an exception
	 * @param InText The text of the error
	 */
	UE_NORETURN void Throwf(FString&& InText) const
	{
		throw FUHTException(ECompilationResult::OtherCompilationError, *this, MoveTemp(InText));
	}

	/**
	 * Generate an exception
	 * @param InResult Result code to be returned as the overall result of the compilation process
	 * @param InText The text of the error
	 */
	UE_NORETURN void Throwf(ECompilationResult::Type InResult, FString&& InText) const
	{
		throw FUHTException(InResult, *this, MoveTemp(InText));
	}

	/**
	 * Generate an exception
	 * @param InFmt The format string
	 * @param InArgs Arguments supplied to the format string
	 */
	template <typename FmtType, typename... Types>
	UE_NORETURN void VARARGS Throwf(const FmtType& InFmt, Types... InArgs) const
	{
		FString ResultString = FString::Printf(InFmt, InArgs ...);
		throw FUHTException(ECompilationResult::OtherCompilationError, *this, MoveTemp(ResultString));
	}

	/**
	 * Generate an exception
	 * @param InResult Result code to be returned as the overall result of the compilation process
	 * @param InFmt The format string
	 * @param InArgs Arguments supplied to the format string
	 */
	template <typename FmtType, typename... Types>
	UE_NORETURN void VARARGS Throwf(ECompilationResult::Type InResult, const FmtType& InFmt, Types... InArgs) const
	{
		FString ResultString = FString::Printf(InFmt, InArgs ...);
		throw FUHTException(InResult, *this, MoveTemp(ResultString));
	}

	/**
	 * Log an error
	 * @param InText The text of the error
	 */
	void LogError(const FString& InText) const
	{
		FResults::LogError(*this, *InText, ECompilationResult::OtherCompilationError);
	}

	/**
	 * Log an error
	 * @param InResult Result code to be returned as the overall result of the compilation process
	 * @param InText The text of the error
	 */
	void LogError(ECompilationResult::Type InResult, const FString& InText) const
	{
		FResults::LogError(*this, *InText, InResult);
	}

	/**
	 * Log an error
	 * @param InFmt The format string
	 * @param InArgs Arguments supplied to the format string
	 */
	template <typename FmtType, typename... Types>
	void VARARGS LogError(const FmtType& InFmt, Types... InArgs) const
	{
		FString ResultString = FString::Printf(InFmt, InArgs ...);
		FResults::LogError(*this, *ResultString, ECompilationResult::OtherCompilationError);
	}

	/**
	 * Log an error
	 * @param InResult Result code to be returned as the overall result of the compilation process
	 * @param InFmt The format string
	 * @param InArgs Arguments supplied to the format string
	 */
	template <typename FmtType, typename... Types>
	void VARARGS LogError(ECompilationResult::Type InResult, const FmtType& InFmt, Types... InArgs) const
	{
		FString ResultString = FString::Printf(InFmt, InArgs ...);
		FResults::LogError(*this, *ResultString, InResult);
	}

	/**
	 * Log an warning
	 * @param InText The text of the warning
	 */
	void LogWarning(const FString& InText) const
	{
		FResults::LogWarning(*this, *InText);
	}

	/**
	 * Log an warning
	 * @param InFmt The format string
	 * @param InArgs Arguments supplied to the format string
	 */
	template <typename FmtType, typename... Types>
	void VARARGS LogWarning(const FmtType& InFmt, Types... InArgs) const
	{
		FString ResultString = FString::Printf(InFmt, InArgs ...);
		FResults::LogWarning(*this, *ResultString);
	}

	/**
	 * Log an info
	 * @param InText The text of the info
	 */
	void LogInfo(const FString& InText) const
	{
		FResults::LogInfo(*this, *InText);
	}

};

template <typename T>
FString GetMessageFilename(const T& Source);

template <>
inline FString GetMessageFilename<FString>(const FString& Source)
{
	return Source;
}

class FUHTMessage
	: public FUHTMessageProvider
{
public:

	template <typename T>
	explicit FUHTMessage(const T& Source, int32 InLineNumber = 1)
		: Filename(GetMessageFilename(Source))
		, LineNumber(InLineNumber)
	{
	}

	virtual FString GetFilename() const override
	{
		return Filename;
	}

	virtual int32 GetLineNumber() const override
	{
		return LineNumber;
	}

private:
	FString Filename;
	int32 LineNumber;
};
