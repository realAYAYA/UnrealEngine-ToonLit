// Copyright Epic Games, Inc. All Rights Reserved.

#include "PrimitiveSceneInfoData.h"


/** Next id to be used by a component. */
// 0 is reserved to mean invalid
FThreadSafeCounter FPrimitiveSceneInfoData::NextPrimitiveId;

/** Next registration serial number to be assigned to a component when it is registered. */
// -1 is reserved to mean invalid
FThreadSafeCounter FPrimitiveSceneInfoData::NextRegistrationSerialNumber;