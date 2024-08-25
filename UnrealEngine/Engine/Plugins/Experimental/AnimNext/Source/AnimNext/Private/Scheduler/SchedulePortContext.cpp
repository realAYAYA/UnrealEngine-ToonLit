// Copyright Epic Games, Inc. All Rights Reserved.

#include "Scheduler/SchedulePortContext.h"

#include "Param/ParamStackLayer.h"

namespace UE::AnimNext
{

FParamResult FSchedulePortContext::GetParamData(FParamId InId, FParamTypeHandle InTypeHandle, TConstArrayView<uint8>& OutParamData, FParamTypeHandle& OutParamTypeHandle, FParamCompatibility InRequiredCompatibility) const
{
	return ResultLayer->GetParamData(InId, InTypeHandle, OutParamData, OutParamTypeHandle, InRequiredCompatibility);
}

}
