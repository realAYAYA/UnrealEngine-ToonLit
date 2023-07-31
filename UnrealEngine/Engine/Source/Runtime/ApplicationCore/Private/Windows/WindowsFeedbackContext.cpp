// Copyright Epic Games, Inc. All Rights Reserved.

#include "Windows/WindowsFeedbackContext.h"
#include "Windows/WindowsHWrapper.h"
#include "HAL/ThreadHeartBeat.h"
#include "Internationalization/Internationalization.h"
#include "Misc/App.h"
#include "Windows/WindowsHWrapper.h"

bool FWindowsFeedbackContext::YesNof(const FText& Question)
{
	if ((GIsClient || GIsEditor) && ((GIsSilent != true) && (FApp::IsUnattended() != true)))
	{
		FSlowHeartBeatScope SuspendHeartBeat;
		return(::MessageBox(NULL, Question.ToString().GetCharArray().GetData(), *NSLOCTEXT("Core", "Question", "Question").ToString(), MB_YESNO | MB_TASKMODAL) == IDYES);
	}
	else
	{
		return false;
	}
}
