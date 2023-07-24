// Copyright Epic Games, Inc. All Rights Reserved.

#include "Pch.h"
#include "AsioIoable.h"

////////////////////////////////////////////////////////////////////////////////
bool FAsioIoable::SetSink(FAsioIoSink* Ptr, uint32 Id)
{
	if (SinkPtr != nullptr)
	{
		return false;
	}

	SinkPtr = Ptr;
	SinkId = Id;
	return true;
}

////////////////////////////////////////////////////////////////////////////////
void FAsioIoable::OnIoComplete(const asio::error_code& ErrorCode, int32 Size)
{
	if (SinkPtr == nullptr)
	{
		return;
	}

#if TS_USING(TS_BUILD_DEBUG) && 0
	std::string ErrorMessage;
	{
		ErrorMessage = ErrorCode.message();
	}
#endif

	if (ErrorCode)
	{
		Size = 0 - ErrorCode.value();
	}

	FAsioIoSink* Ptr = SinkPtr;
	SinkPtr = nullptr;
	Ptr->OnIoComplete(SinkId, Size);
}

/* vim: set noexpandtab : */
