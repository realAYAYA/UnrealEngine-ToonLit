// Copyright Epic Games, Inc. All Rights Reserved.

#include "Message.h"

#if UE_TRACE_ENABLED

#include "Trace/Trace.h"

namespace UE {
namespace Trace {
namespace Private {

////////////////////////////////////////////////////////////////////////////////
static OnMessageFunc* GMessageFunc = nullptr;
	
////////////////////////////////////////////////////////////////////////////////
void Message_SetCallback(OnMessageFunc Callback)
{
	GMessageFunc = Callback;
}

////////////////////////////////////////////////////////////////////////////////
void Message_Send(EMessageType Type, const char* TypeStr, const char* Description /* = nullptr */)
{
	if (!GMessageFunc)
	{
		return;
	}

	FMessageEvent Message { Type, TypeStr, Description };
	GMessageFunc(Message);
}

} } } // namespace UE::Trace::Private

#endif // UE_TRACE_ENABLED