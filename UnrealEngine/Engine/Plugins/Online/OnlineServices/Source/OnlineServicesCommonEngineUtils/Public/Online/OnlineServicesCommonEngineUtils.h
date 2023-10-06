// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

class FName;

namespace UE::Online {

ONLINESERVICESCOMMONENGINEUTILS_API int32 GetPortFromNetDriver(FName InstanceName);

/* UE::Online */ }

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "Online/CoreOnline.h"
#endif
