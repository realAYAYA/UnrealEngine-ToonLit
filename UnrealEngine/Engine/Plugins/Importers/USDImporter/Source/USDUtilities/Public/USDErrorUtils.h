// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Logging/TokenizedMessage.h"

namespace UsdUtils
{
    /**
     * Pushes an USD error monitoring object into the stack and catches any emitted errors
     */
	USDUTILITIES_API void StartMonitoringErrors();

    /**
     * Returns all errors that were captured since StartMonitoringErrors(), clears and pops an
     * error monitoring object from the stack
     */
	USDUTILITIES_API TArray<FString> GetErrorsAndStopMonitoring();

    /**
     * Displays the error messages for each captured error since StartMonitoringErrors(),
	 * clears and pops an error monitoring object from the stack.
	 * If ToastMessage is empty, a default message will be displayed.
     * Returns true if there were any errors.
     */
	USDUTILITIES_API bool ShowErrorsAndStopMonitoring(const FText& ToastMessage = FText());
}

class FUsdLogManager;

namespace UE
{
	namespace Internal
	{
		class FUsdMessageLog
		{
			friend class ::FUsdLogManager;

		public:
			FUsdMessageLog() = default;

			FUsdMessageLog( const FUsdMessageLog& ) = default;
			FUsdMessageLog& operator=( const FUsdMessageLog& ) = default;
			FUsdMessageLog( FUsdMessageLog&& ) = default;
			FUsdMessageLog& operator=( FUsdMessageLog&& ) = default;

			~FUsdMessageLog();

		private:
			/** Display pending messages, show the log window */
			void Dump();

		
			/** Append a message */
			void Push( const TSharedRef< FTokenizedMessage >& Message );

		private:
			TArray< TSharedRef< FTokenizedMessage > > TokenizedMessages;
		};
	}
}

class USDUTILITIES_API FUsdLogManager
{
public:
	/** Sends the message to Message Log if it exists, if not, sends it to the Output Log */
	static void LogMessage( EMessageSeverity::Type Severity, const FText& Message );
	static void LogMessage( const TSharedRef< FTokenizedMessage >& Message );

	static void EnableMessageLog();
	static void DisableMessageLog();

private:
	static TOptional< UE::Internal::FUsdMessageLog > MessageLog;
	static int32 MessageLogRefCount;

	static FCriticalSection MessageLogLock;
};

/**
 * Starts sending USD messages to the message log and will display the message log on destruction if any messages were logged.
 */
class USDUTILITIES_API FScopedUsdMessageLog
{
public:
	FScopedUsdMessageLog();
	~FScopedUsdMessageLog();

	FScopedUsdMessageLog( const FScopedUsdMessageLog& ) = delete;
	FScopedUsdMessageLog& operator=( const FScopedUsdMessageLog& ) = delete;
	FScopedUsdMessageLog( FScopedUsdMessageLog&& ) = delete;
	FScopedUsdMessageLog& operator=( FScopedUsdMessageLog&& ) = delete;
};
