// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AddonTools.h"

BEGIN_NAMESPACE_UE_AC

class FViewEvent
{
  public:
	static GSErrCode Initialize();
	static GSErrCode Event(const API_NotifyViewEventType& ViewEvent);
};

END_NAMESPACE_UE_AC
