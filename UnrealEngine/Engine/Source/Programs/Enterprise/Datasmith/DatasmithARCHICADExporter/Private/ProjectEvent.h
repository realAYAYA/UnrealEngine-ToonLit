// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Utils/AddonTools.h"

BEGIN_NAMESPACE_UE_AC

class FProjectEvent
{
  public:
	static GSErrCode Initialize();
	static GSErrCode Event(API_NotifyEventID NotifID, Int32 Param);
};

END_NAMESPACE_UE_AC
