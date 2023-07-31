// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConcertTransportLogSettings.h"

UConcertTransportLogSettings* UConcertTransportLogSettings::GetSettings()
{
	return GetMutableDefault<UConcertTransportLogSettings>();
}
