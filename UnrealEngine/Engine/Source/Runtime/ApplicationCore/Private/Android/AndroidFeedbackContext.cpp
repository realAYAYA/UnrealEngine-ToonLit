// Copyright Epic Games, Inc. All Rights Reserved.

#include "Android/AndroidFeedbackContext.h"

FAndroidFeedbackContext::FAndroidFeedbackContext()
	: FFeedbackContext()
{ }

void FAndroidFeedbackContext::Serialize( const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category )
{
	if( !GLog->IsRedirectingTo( this ) )
	{
		GLog->Serialize( V, Verbosity, Category );
	}
}

bool FAndroidFeedbackContext::YesNof(const FText& Question)
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
