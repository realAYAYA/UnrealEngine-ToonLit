// Copyright Epic Games, Inc. All Rights Reserved.

#include "USDErrorUtils.h"

#include "Math/NumericLimits.h"
#include "Misc/ScopeLock.h"
#include "Modules/ModuleManager.h"

#include "USDLog.h"

#if WITH_EDITOR
#include "IMessageLogListing.h"
#include "MessageLogModule.h"
#endif // WITH_EDITOR

#if USE_USD_SDK

#include "USDMemory.h"
#include "USDTypesConversion.h"

#include "Framework/Notifications/NotificationManager.h"
#include "Templates/SharedPointer.h"
#include "Widgets/Notifications/SNotificationList.h"

#include "USDIncludesStart.h"
#include "pxr/pxr.h"
#include "pxr/base/tf/errorMark.h"
#include "USDIncludesEnd.h"

#endif // #if USE_USD_SDK

#define LOCTEXT_NAMESPACE "USDErrorUtils"

#if USE_USD_SDK

namespace UsdUtils
{
	// We need an extra level of indirection because TfErrorMark is noncopyable
	using MarkRef = TUsdStore<TSharedRef<pxr::TfErrorMark>>;

	static TArray<MarkRef> ErrorMarkStack;

	void StartMonitoringErrors()
	{
		FScopedUsdAllocs Allocs;

		TSharedRef<pxr::TfErrorMark> Mark = MakeShared<pxr::TfErrorMark>();
		Mark->SetMark();

		ErrorMarkStack.Emplace(Mark);
	}

	TArray<FString> GetErrorsAndStopMonitoring()
	{
		if (ErrorMarkStack.Num() == 0)
		{
			return {};
		}

		MarkRef Store = ErrorMarkStack.Pop();
		pxr::TfErrorMark& Mark = Store.Get().Get();

		if (Mark.IsClean())
		{
			return {};
		}

		TArray<FString> Errors;

		for (pxr::TfErrorMark::Iterator ErrorIter = Mark.GetBegin();
			 ErrorIter != Mark.GetEnd();
			 ++ErrorIter)
		{
			std::string ErrorStr = ErrorIter->GetErrorCodeAsString();
			ErrorStr += ": ";
			ErrorStr += ErrorIter->GetCommentary();

			// Add unique here as for some errors (e.g. parsing errors) USD can emit the exact same
			// error message 5+ times in a row
			Errors.AddUnique(UsdToUnreal::ConvertString(ErrorStr));
		}

		Mark.Clear();

		return Errors;
	}

	bool ShowErrorsAndStopMonitoring(const FText& ToastMessage)
	{
		TArray<FString> Errors = GetErrorsAndStopMonitoring();
		bool bHadErrors = Errors.Num() > 0;

		if (bHadErrors)
		{
			FNotificationInfo ErrorToast(!ToastMessage.IsEmpty() ? ToastMessage : LOCTEXT("USDErrorsToast", "Encountered USD errors!\nCheck the Output Log for details."));

			ErrorToast.ExpireDuration = 5.0f;
			ErrorToast.bFireAndForget = true;
			ErrorToast.Image = FCoreStyle::Get().GetBrush(TEXT("MessageLog.Error"));
			FSlateNotificationManager::Get().AddNotification(ErrorToast);
		}

		for (const FString& Error : Errors)
		{
			FUsdLogManager::LogMessage( FTokenizedMessage::Create( EMessageSeverity::Error, FText::FromString( Error ) ) );
		}

		return Errors.Num() > 0;
	}
}; // namespace UsdUtils

#else // #if USE_USD_SDK

namespace UsdUtils
{
	void StartMonitoringErrors()
	{
	}
	TArray<FString> GetErrorsAndStopMonitoring()
	{
		return {TEXT("USD SDK is not available!")};
	}
	bool ShowErrorsAndStopMonitoring(const FText& ToastMessage)
	{
		return false;
	}
}

#endif // #if USE_USD_SDK

namespace UE
{
	namespace Internal
	{
		FUsdMessageLog::~FUsdMessageLog()
		{
			Dump();
		}

#if WITH_EDITOR
		void FUsdMessageLog::Push( const TSharedRef< FTokenizedMessage >& Message )
		{
			TokenizedMessages.Add( Message );
		}
#endif // WITH_EDITOR

		void FUsdMessageLog::Dump()
		{
#if WITH_EDITOR
			FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked< FMessageLogModule >( "MessageLog" );
			TSharedRef< IMessageLogListing > LogListing = MessageLogModule.GetLogListing( TEXT("USD") );

			if ( TokenizedMessages.Num() > 0 )
			{
				// This crashes internally at runtime due to Slate not finding the correct brush...
				// I think MessageLog is not supposed to be used at runtime, even though it technically can.
				// At runtime we shouldn't ever push, but just in case
				LogListing->AddMessages( TokenizedMessages );
				LogListing->NotifyIfAnyMessages( LOCTEXT("Log", "There were some issues loading the USD Stage."), EMessageSeverity::Info );
				TokenizedMessages.Empty();
			}
#endif // WITH_EDITOR
		}
	}
}

TOptional< UE::Internal::FUsdMessageLog > FUsdLogManager::MessageLog;
int32 FUsdLogManager::MessageLogRefCount;
FCriticalSection FUsdLogManager::MessageLogLock;

TSet<FString> LoggedMessages;

void FUsdLogManager::LogMessage( EMessageSeverity::Type Severity, const FText& Message )
{
	LogMessage( FTokenizedMessage::Create( Severity, Message ) );
}

void FUsdLogManager::LogMessage( const TSharedRef< FTokenizedMessage >& Message )
{
	bool bMessageProcessed = false;

	{
		FScopeLock Lock(&MessageLogLock);
		const FString& Str = Message->ToText().ToString();
		if (LoggedMessages.Contains(Str))
		{
			return;
		}

		LoggedMessages.Add(Str);

#if WITH_EDITOR
		if (MessageLog)
		{
			MessageLog->Push(Message);
			bMessageProcessed = true;
		}
#endif
	}

	if ( !bMessageProcessed )
	{
		if ( Message->GetSeverity() == EMessageSeverity::Error )
		{
			UE_LOG( LogUsd, Error, TEXT("%s"), *(Message->ToText().ToString()) );
		}
		else if ( Message->GetSeverity() == EMessageSeverity::Warning || Message->GetSeverity() == EMessageSeverity::PerformanceWarning )
		{
			UE_LOG( LogUsd, Warning, TEXT("%s"), *(Message->ToText().ToString()) );
		}
		else
		{
			UE_LOG( LogUsd, Log, TEXT("%s"), *(Message->ToText().ToString()) );
		}
	}
}

void FUsdLogManager::EnableMessageLog()
{
	FScopeLock Lock( &MessageLogLock );

	LoggedMessages.Reset();

	if ( ++MessageLogRefCount == 1 )
	{
		MessageLog.Emplace();
		UsdUtils::StartMonitoringErrors();
	}

	check( MessageLogRefCount < MAX_int32 );
}

void FUsdLogManager::DisableMessageLog()
{
	FScopeLock Lock( &MessageLogLock );

	if ( --MessageLogRefCount == 0 )
	{
		UsdUtils::ShowErrorsAndStopMonitoring();
		MessageLog.Reset();
	}
}

FScopedUsdMessageLog::FScopedUsdMessageLog()
{
	FUsdLogManager::EnableMessageLog();
}

FScopedUsdMessageLog::~FScopedUsdMessageLog()
{
	FUsdLogManager::DisableMessageLog();
}

#undef LOCTEXT_NAMESPACE
