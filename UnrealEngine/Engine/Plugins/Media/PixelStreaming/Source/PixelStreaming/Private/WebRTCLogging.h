// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// Start WebRTC Includes
#include "PreWebRTCApi.h"
#include "rtc_base/logging.h"
#include "PostWebRTCApi.h"
// End WebRTC Includes

void RedirectWebRtcLogsToUnreal(rtc::LoggingSeverity Verbosity);
