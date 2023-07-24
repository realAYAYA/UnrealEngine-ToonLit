// Copyright Epic Games, Inc. All Rights Reserved.

#include "Windows/WindowsFeedbackContext.h"

#include "HAL/ThreadHeartBeat.h"
#include "Internationalization/Internationalization.h"
#include "Misc/App.h"
#include "Windows/WindowsHWrapper.h"

bool FWindowsFeedbackContext::YesNof(const FText& Question)
{
	if ((GIsClient || GIsEditor) && !GIsSilent && !FApp::IsUnattended())
	{
		FSlowHeartBeatScope SuspendHeartBeat;
		return ::MessageBox(nullptr, Question.ToString().GetCharArray().GetData(), *NSLOCTEXT("Core", "Question", "Question").ToString(), MB_YESNO | MB_TASKMODAL) == IDYES;
	}
	return false;
}
