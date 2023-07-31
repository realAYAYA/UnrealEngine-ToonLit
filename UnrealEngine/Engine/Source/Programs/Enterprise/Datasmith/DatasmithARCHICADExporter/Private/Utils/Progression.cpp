// Copyright Epic Games, Inc. All Rights Reserved.

#include "Progression.h"

#include "Error.h"

#include "CurrentOS.h"

#include "Guard.hpp"

#if PLATFORM_WINDOWS
#else
	#include <sys/time.h>
#endif

BEGIN_NAMESPACE_UE_AC

inline void* ACAPI_Interface_Str(const GS::UniString& InStr)
{
	return (void*)&InStr;
}

// Constructor (SetUp progression window)
FProgression::FProgression(int InResID, EPhaseStrId InTitle, short InNbPhases, EMode InCancelMode,
						   volatile bool* OutUserCancelled)
	: ResID(InResID)
	, CurrentPhase(kCommonPhaseInvalid)
	, CurrentValue(0)
	, bProgressionShown(false)
	, ErrorCode(NoError)
	, ErrorMgs(NULL)
	, CancelMode(InCancelMode)
	, bUserCancelled(OutUserCancelled)
	, NextPhase(kCommonPhaseInvalid)
	, NextPhaseMaxValue(0)
	, NextCurrentValue(0)
	, mCV(AccessControl)
{
#if PLATFORM_WINDOWS
	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);
	InvFreq = 1.0 / freq.QuadPart;
#endif
	LastUpdate = GetHiResTime();

	// Intialize the progression window and specify the number of phases
	API_ProcessControlTypeID DisableMenu = API_MenuCommandDisabled;
	ErrorCode =
		ACAPI_Interface(APIIo_InitProcessWindowID, ACAPI_Interface_Str(GetText(InTitle)), &InNbPhases, &DisableMenu);
	if (ErrorCode == NoError)
	{
		bProgressionShown = true;
	}
	else
	{
		ErrorMgs = "initialising progression window";
	}
}

// Destructor (Automatically close the progression window)
FProgression::~FProgression()
{
	// If we succeed to initialize progression window
	if (bProgressionShown)
	{
		// Close the progression window
		GSErrCode ErrCode = ACAPI_Interface(APIIo_CloseProcessWindowID, nullptr, nullptr);
		if (ErrorCode == NoError)
		{
			ErrorCode = ErrCode;
			ErrorMgs = "closing progression window";
		}
	}

	// If we got an progression windows error, we report to stderr
	if (ErrorCode != NoError)
	{
		fprintf(stderr, "Got error (%d) with progress window when %s", (int)ErrorCode, ErrorMgs);
	}
}

// Set new cancel mode
FProgression::EMode FProgression::SetCancelMode(EMode InNewCancelMode)
{
	EMode PreviousMode = CancelMode;
	CancelMode = InNewCancelMode;
	return PreviousMode;
}

// Start the next phase
void FProgression::NewPhase(EPhaseStrId InPhaseId, int InMaxValue)
{
	// If we got an error from AC before, we do nothing after
	if (ErrorCode != NoError)
	{
		return;
	}

	NextPhase = kCommonPhaseInvalid;
	NextCurrentValue = 0;

	// Setup the next phase
	CurrentValue = 0;
	CurrentPhase = InPhaseId;
	Ratio100 = 100.0;
	bool showPercent = false;
	if (InMaxValue > 0)
	{
		Ratio100 = 100.0 / InMaxValue;
		showPercent = true;
		InMaxValue = 100;
	}
	ErrorCode = ACAPI_Interface(APIIo_SetNextProcessPhaseID, ACAPI_Interface_Str(GetText(CurrentPhase)), &InMaxValue,
								&showPercent);
	ErrorMgs = "setting next phase";

	LastUpdate = GetHiResTime();
}

// Start the next phase
void FProgression::NewPhaseX(EPhaseStrId InPhaseId, int InMaxValue)
{
	GS::Guard< GS::Lock > lck(AccessControl);
	NextPhase = InPhaseId;
	NextPhaseMaxValue = InMaxValue;
}

// Advance progression bar for the current value
void FProgression::NewCurrentValue(int InCurrentValue)
{
	// If we got an error from AC before, we do nothing after
	if (ErrorCode != NoError)
	{
		return;
	}

	// Special case of progression: When we are waiting and not advancing
	if (InCurrentValue != -1)
	{
		InCurrentValue = int(Ratio100 * InCurrentValue + 0.5);
	}
	else
	{
		InCurrentValue = 0;
	}

	// Send the value to ArchiCAD
	if (CurrentValue != InCurrentValue)
	{
		CurrentValue = InCurrentValue;
		ErrorCode = ACAPI_Interface(APIIo_SetProcessValueID, &CurrentValue, nullptr);
		ErrorMgs = "setting current value";
	}

	// Check that at least 1/30 of seconds elapsed since last update
	double CurrentTime = GetHiResTime();
	if ((CurrentTime - LastUpdate) < (1.0 / 30))
	{
		return;
	}
	// Take note of the last update time
	LastUpdate = CurrentTime;

	// Check if user want to cancel
	CheckForCancel();
}

// Advance progression bar for the current value
void FProgression::NewCurrentValueX(int InCurrentValue)
{
	GS::Guard< GS::Lock > lck(AccessControl);
	NextCurrentValue = InCurrentValue;
}

// Call from the main thread (Usualy when Join wait)
void FProgression::Update()
{
	GS::Guard< GS::Lock > lck(AccessControl);
	if (NextPhase)
	{
		int NewValue = NextCurrentValue;
		NewPhase(NextPhase, NextPhaseMaxValue);
		if (NewValue)
		{
			NewCurrentValue(NewValue);
		}
	}
	else if (NextCurrentValue)
	{
		NewCurrentValue(NextCurrentValue);
	}
	else
	{
		NewCurrentValue();
	}
}

// Throw a kUserCancelled error if user push the cancel button
void FProgression::CheckForCancel()
{
	if (CancelMode == kNoCancel) // Dont show cancel interface
		return;
	if (CancelMode == kSetFlags && bUserCancelled == nullptr) // Specify to set flag, but not flag to set ?
		return;
	if (CancelMode == kThrowOnCancel && bUserCancelled != nullptr && *bUserCancelled)
	{
		// Previous mode was kSetFlags and user has cancelled
		*bUserCancelled = false;
		throw UE_AC_Error("User cancelled operation", UE_AC_Error::kUserCancelled);
	}

	// Show cancel interface (when mouse is over progression window)
	if (ACAPI_Interface(APIIo_IsProcessCanceledID, nullptr, nullptr))
	{
		// True if user cancelled
		if (CancelMode == kSetFlags)
		{
			*bUserCancelled = true;
		}
		else
		{
			throw UE_AC_Error("User cancelled operation", UE_AC_Error::kUserCancelled);
		}
	}
}

// Throw an exception if user cancelled current operation
void FProgression::ThrowIfUserCancelled() const
{
	if (bUserCancelled != nullptr && *bUserCancelled)
	{
		throw UE_AC_Error("User cancelled operation", UE_AC_Error::kUserCancelled);
	}
}

// Return the localized string specified by the index
GS::UniString FProgression::GetText(int InIndex)
{
	GS::UniString s;
	try
	{
		s = GetUniString(ResID, InIndex);
	}
	catch (...)
	{
		s = "Invalide text resources";
	}

	return s;
}

// Return time value in sec with hi resolution
double FProgression::GetHiResTime()
{
#if PLATFORM_WINDOWS
	LARGE_INTEGER tick;
	QueryPerformanceCounter(&tick);
	return tick.QuadPart * InvFreq;
#else
	struct timeval CurrentTime;
	gettimeofday(&CurrentTime, nullptr);
	return CurrentTime.tv_sec + (CurrentTime.tv_usec * (1.0 / 1000000));
#endif
}

END_NAMESPACE_UE_AC
