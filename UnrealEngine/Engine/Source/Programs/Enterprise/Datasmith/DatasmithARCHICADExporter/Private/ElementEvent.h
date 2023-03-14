// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Utils/AddonTools.h"

BEGIN_NAMESPACE_UE_AC

class FElementEvent
{
  public:
	static GSErrCode Initialize();

	static GSErrCode Event(const API_NotifyElementType& ElemType);
};

END_NAMESPACE_UE_AC
