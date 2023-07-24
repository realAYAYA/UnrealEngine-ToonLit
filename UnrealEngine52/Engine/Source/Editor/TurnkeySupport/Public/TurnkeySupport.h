// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once



/* Public Dependencies
 *****************************************************************************/

#include "Modules/ModuleInterface.h"
#include "SlateFwd.h"
#include "Widgets/SWidget.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxExtender.h"


/* Public Includes
 *****************************************************************************/

#include "Interfaces/ITurnkeySupportModule.h"



DECLARE_LOG_CATEGORY_EXTERN(LogTurnkeySupport, Log, All);

FString ConvertToDDPIPlatform(const FString& Platform);
FName ConvertToDDPIPlatform(const FName& Platform);
FString ConvertToUATPlatform(const FString& Platform);
FString ConvertToUATDeviceId(const FString& DeviceId);
FString ConvertToDDPIDeviceId(const FString& DeviceId);
