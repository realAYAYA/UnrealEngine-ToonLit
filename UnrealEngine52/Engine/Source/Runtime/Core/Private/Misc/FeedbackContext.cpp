// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/FeedbackContext.h"

#include "CoreTypes.h"
#include "HAL/PlatformTime.h"
#include "Logging/StructuredLog.h"
#include "Misc/App.h"
#include "Misc/AssertionMacros.h"
#include "Misc/CoreMisc.h"
#include "Misc/OutputDeviceConsole.h"
#include "Misc/OutputDeviceHelper.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/SlowTask.h"
#include "Misc/StringBuilder.h"

FFeedbackContext::FFeedbackContext() = default;

FFeedbackContext::~FFeedbackContext()
{
	ensureMsgf(LegacyAPIScopes.Num() == 0, TEXT("EndSlowTask has not been called for %d outstanding tasks"), LegacyAPIScopes.Num());
}

void FFeedbackContext::Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category)
{
	if (IsRunningCommandlet())
	{
		if (Verbosity == ELogVerbosity::Error || Verbosity == ELogVerbosity::Warning)
		{
			AddToHistory(V, Verbosity, Category, -1.0);
		}
		if (GLogConsole && !GLog->IsRedirectingTo(GLogConsole))
		{
			GLogConsole->Serialize(V, Verbosity, Category);
		}
	}
	if (!GLog->IsRedirectingTo(this))
	{
		GLog->Serialize(V, Verbosity, Category);
	}
}

void FFeedbackContext::Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category, double Time)
{
	if (IsRunningCommandlet())
	{
		if (Verbosity == ELogVerbosity::Error || Verbosity == ELogVerbosity::Warning)
		{
			AddToHistory(V, Verbosity, Category, Time);
		}
		if (GLogConsole && !GLog->IsRedirectingTo(GLogConsole))
		{
			GLogConsole->Serialize(V, Verbosity, Category, Time);
		}
	}
	if (!GLog->IsRedirectingTo(this))
	{
		GLog->Serialize(V, Verbosity, Category, Time);
	}
}

void FFeedbackContext::SerializeRecord(const UE::FLogRecord& Record)
{
	if (IsRunningCommandlet())
	{
		const ELogVerbosity::Type Verbosity = Record.GetVerbosity();
		if (Verbosity == ELogVerbosity::Error || Verbosity == ELogVerbosity::Warning)
		{
			AddRecordToHistory(Record);
		}
		if (GLogConsole && !GLog->IsRedirectingTo(GLogConsole))
		{
			GLogConsole->SerializeRecord(Record);
		}
	}
	if (!GLog->IsRedirectingTo(this))
	{
		GLog->SerializeRecord(Record);
	}
}

void FFeedbackContext::FormatLine(FStringBuilderBase& Out, const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category, double Time, ELogVerbosity::Type* OutVerbosity) const
{
	if (TreatWarningsAsErrors && Verbosity == ELogVerbosity::Warning)
	{
		Verbosity = ELogVerbosity::Error;
	}
	if (OutVerbosity)
	{
		*OutVerbosity = Verbosity;
	}
	if (FContextSupplier* Context = GetContext())
	{
		Out.Append(Context->GetContext()).Append(TEXTVIEW(" : "));
	}
	FOutputDeviceHelper::AppendFormatLogLine(Out, Verbosity, Category, V);
}

void FFeedbackContext::FormatRecordLine(FStringBuilderBase& Out, const UE::FLogRecord& Record, ELogVerbosity::Type* OutVerbosity) const
{
	ELogVerbosity::Type Verbosity = Record.GetVerbosity();
	if (TreatWarningsAsErrors && Verbosity == ELogVerbosity::Warning)
	{
		Verbosity = ELogVerbosity::Error;
	}
	if (OutVerbosity)
	{
		*OutVerbosity = Verbosity;
	}
	if (FContextSupplier* Context = GetContext())
	{
		Out.Append(Context->GetContext()).Append(TEXTVIEW(" : "));
	}
	FOutputDeviceHelper::AppendFormatLogLine(Out, Verbosity, Record.GetCategory());
	Record.FormatMessageTo(Out);
}

void FFeedbackContext::AddToHistory(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category, double Time)
{
	TStringBuilder<512> Line;
	FormatLine(Line, V, Verbosity, Category, Time, &Verbosity);
	if (Verbosity == ELogVerbosity::Error)
	{
		AddError(FString(Line));
	}
	else
	{
		AddWarning(FString(Line));
	}
}

void FFeedbackContext::AddRecordToHistory(const UE::FLogRecord& Record)
{
	TStringBuilder<512> Line;
	ELogVerbosity::Type Verbosity;
	FormatRecordLine(Line, Record, &Verbosity);
	if (Verbosity == ELogVerbosity::Warning)
	{
		AddWarning(FString(Line));
	}
	else if (Verbosity == ELogVerbosity::Error)
	{
		AddError(FString(Line));
	}
}

bool FFeedbackContext::YesNof(const FText& Question)
{
	if (!GIsSilent && !FApp::IsUnattended())
	{
		FPlatformMisc::LowLevelOutputDebugString(*Question.ToString());
	}
	return false;
}

void FFeedbackContext::RequestUpdateUI(bool bForceUpdate)
{
	// Only update a maximum of 5 times a second
	static double MinUpdateTimeS = 0.2;

	static double LastUIUpdateTime = FPlatformTime::Seconds();
	const double CurrentTime = FPlatformTime::Seconds();

	if (bForceUpdate || CurrentTime - LastUIUpdateTime > MinUpdateTimeS)
	{
		LastUIUpdateTime = CurrentTime;
		UpdateUI();
	}
}

void FFeedbackContext::UpdateUI()
{
	ensure(IsInGameThread());

	if (ScopeStack.Num() != 0)
	{
		ProgressReported(ScopeStack.GetProgressFraction(0), ScopeStack[0]->GetCurrentMessage());
	}
}

/**** Begin legacy API ****/
void FFeedbackContext::BeginSlowTask( const FText& Task, bool ShowProgressDialog, bool bShowCancelButton )
{
	ensure(IsInGameThread());

	TUniquePtr<FSlowTask> NewScope(new FSlowTask(0, Task, true, *this));
	if (ShowProgressDialog)
	{
		NewScope->MakeDialogDelayed(3.0f, bShowCancelButton);
	}

	NewScope->Initialize();
	LegacyAPIScopes.Add(MoveTemp(NewScope));
}

void FFeedbackContext::UpdateProgress( int32 Numerator, int32 Denominator )
{
	ensure(IsInGameThread());

	if (LegacyAPIScopes.Num() != 0)
	{
		LegacyAPIScopes.Last()->TotalAmountOfWork = (float)Denominator;
		LegacyAPIScopes.Last()->CompletedWork = (float)Numerator;
		LegacyAPIScopes.Last()->CurrentFrameScope = (float)(Denominator - Numerator);
		RequestUpdateUI();
	}
}

void FFeedbackContext::StatusUpdate( int32 Numerator, int32 Denominator, const FText& StatusText )
{
	ensure(IsInGameThread());

	if (LegacyAPIScopes.Num() != 0)
	{
		if (Numerator > 0 && Denominator > 0)
		{
			UpdateProgress(Numerator, Denominator);
		}
		LegacyAPIScopes.Last()->FrameMessage = StatusText;
		RequestUpdateUI();
	}
}

void FFeedbackContext::StatusForceUpdate( int32 Numerator, int32 Denominator, const FText& StatusText )
{
	ensure(IsInGameThread());

	if (LegacyAPIScopes.Num() != 0)
	{
		UpdateProgress(Numerator, Denominator);
		LegacyAPIScopes.Last()->FrameMessage = StatusText;
		UpdateUI();
	}
}

void FFeedbackContext::EndSlowTask()
{
	ensure(IsInGameThread());

	check(LegacyAPIScopes.Num() != 0);
	LegacyAPIScopes.Last()->Destroy();
	LegacyAPIScopes.Pop();
}
/**** End legacy API ****/

bool FFeedbackContext::IsPlayingInEditor() const
{
	return GIsPlayInEditorWorld;
}
