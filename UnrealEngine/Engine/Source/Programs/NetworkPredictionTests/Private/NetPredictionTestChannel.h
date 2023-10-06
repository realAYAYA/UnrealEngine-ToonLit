// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Net/Core/Misc/ResizableCircularQueue.h"
#include "UObject/CoreNet.h"

namespace UE::Net::Private
{

struct FNetPredictionTestChannel
{
	void Send(const FNetBitWriter& Writer);
	bool HasPendingData();
	FNetBitReader Receive();

private:
	TResizableCircularQueue<FNetBitWriter> DataBuffer;
};

}