// Copyright Epic Games, Inc. All Rights Reserved.

#include "TestMessage.h"
#include "Iris/ReplicationSystem/NetRefHandle.h"
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

FTestMessage& operator<<(FTestMessage& Message, const FNetRefHandle& NetRefHandle)
{
	TStringBuilder<64> StringBuilder;
	StringBuilder << NetRefHandle;
	return Message << StringBuilder;
}

}
