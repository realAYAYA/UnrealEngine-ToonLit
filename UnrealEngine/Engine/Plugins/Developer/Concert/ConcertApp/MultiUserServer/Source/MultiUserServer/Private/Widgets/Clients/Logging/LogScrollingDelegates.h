// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Clients/Logging/ConcertLogEntry.h"

namespace UE::MultiUserServer
{
	DECLARE_DELEGATE_RetVal_ThreeParams(bool, FCanScrollToLog, const FGuid& /*MessageId*/, FConcertLogEntryFilterFunc /*Filter*/, FText& /*ErrorMessage*/);
	DECLARE_DELEGATE_TwoParams(FScrollToLog, const FGuid& /*MessageId*/, FConcertLogEntryFilterFunc /*Filter*/);
}