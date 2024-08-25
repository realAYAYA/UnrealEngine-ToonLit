// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaMediaSettings.h"

UAvaMediaSettings::UAvaMediaSettings()
{
	CategoryName = TEXT("Motion Design");
	SectionName  = TEXT("Playback & Broadcast");
}

UAvaMediaSettings* UAvaMediaSettings::GetSingletonInstance()
{
	UAvaMediaSettings* DefaultSettings = GetMutableDefault<UAvaMediaSettings>();
	static bool bInitialized = false;
	if (!bInitialized)
	{
		bInitialized = true;
		DefaultSettings->SetFlags(RF_Transactional);
	}
	return DefaultSettings;
}

ELogVerbosity::Type UAvaMediaSettings::ToLogVerbosity(EAvaMediaLogVerbosity InAvaMediaLogVerbosity)
{
	switch (InAvaMediaLogVerbosity)
	{
	case EAvaMediaLogVerbosity::NoLogging:
		return ELogVerbosity::NoLogging;
	case EAvaMediaLogVerbosity::Fatal:
		return ELogVerbosity::Fatal;
	case EAvaMediaLogVerbosity::Error:
		return ELogVerbosity::Error;
	case EAvaMediaLogVerbosity::Warning:
		return ELogVerbosity::Warning;
	case EAvaMediaLogVerbosity::Display:
		return ELogVerbosity::Display;
	case EAvaMediaLogVerbosity::Log:
		return ELogVerbosity::Log;
	case EAvaMediaLogVerbosity::Verbose:
		return ELogVerbosity::Verbose;
	case EAvaMediaLogVerbosity::VeryVerbose:
		return ELogVerbosity::VeryVerbose;
	default:
		return ELogVerbosity::NoLogging;
	}
}



