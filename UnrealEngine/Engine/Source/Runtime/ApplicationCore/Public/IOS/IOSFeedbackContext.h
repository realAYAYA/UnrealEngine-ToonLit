// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Misc/App.h"
#include "Misc/FeedbackContext.h"
#include "Misc/OutputDeviceRedirector.h"

/**
 * Feedback context implementation for IOS.
 */
class FIOSFeedbackContext : public FFeedbackContext
{
public:

	/** Default constructor. */
	FIOSFeedbackContext();

	virtual void Serialize( const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category ) override;
	virtual bool YesNof(const FText& Question) override;
};
