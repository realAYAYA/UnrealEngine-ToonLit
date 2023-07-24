// Copyright Epic Games, Inc. All Rights Reserved.

#include "TestMessage.h"
#include "Misc/StringBuilder.h"
#include "Net/Core/NetHandle/NetHandle.h"

namespace UE::Net
{

FTestMessage& operator<<(FTestMessage& Message, const FNetHandle& NetHandle)
{
	TStringBuilder<64> StringBuilder;
	StringBuilder << NetHandle;
	return Message << StringBuilder;
}

}
