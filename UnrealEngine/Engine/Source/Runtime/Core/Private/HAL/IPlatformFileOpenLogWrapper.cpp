// Copyright Epic Games, Inc. All Rights Reserved.

#include "HAL/IPlatformFileOpenLogWrapper.h"

#if !UE_BUILD_SHIPPING
IAsyncReadRequest* FLoggingAsyncReadFileHandle::ReadRequest(int64 Offset, int64 BytesToRead, EAsyncIOPriorityAndFlags PriorityAndFlags, FAsyncFileCallBack* CompleteCallback, uint8* UserSuppliedMemory)
{
	if ((PriorityAndFlags & AIOP_FLAG_PRECACHE) == 0)
	{
		Owner->AddToOpenLog(*Filename);
	}
	return ActualRequest->ReadRequest(Offset, BytesToRead, PriorityAndFlags, CompleteCallback, UserSuppliedMemory);
}
#endif // !UE_BUILD_SHIPPING
