// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Utils/AddonTools.h"

BEGIN_NAMESPACE_UE_AC

class FExport
{
  public:
	static GSErrCode Register();

	static GSErrCode Initialize();

	static GSErrCode SaveDatasmithFile(const API_IOParams& IOParams, const Modeler::Sight& InSight);
};

END_NAMESPACE_UE_AC
