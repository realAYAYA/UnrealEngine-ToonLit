// Copyright Epic Games, Inc. All Rights Reserved.

#include "IOS/IOSFeedbackContext.h"

FIOSFeedbackContext::FIOSFeedbackContext()
	: FFeedbackContext()
{ }

void FIOSFeedbackContext::Serialize( const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category )
{
	if( !GLog->IsRedirectingTo( this ) )
	{
		GLog->Serialize( V, Verbosity, Category );
	}
}

bool FIOSFeedbackContext::YesNof(const FText& Question)
{
	if( ( GIsSilent != true ) && ( FApp::IsUnattended() != true ) )
	{
		FPlatformMisc::LowLevelOutputDebugStringf( *(Question.ToString()) );
		return false;
	}
	else
	{
		return false;
	}
}
