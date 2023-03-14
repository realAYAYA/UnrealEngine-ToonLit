// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/DataStream/DataStream.h"

UDataStream::~UDataStream()
{
}

UDataStream::EWriteResult UDataStream::BeginWrite()
{
	return EWriteResult::HasMoreData;
}

void UDataStream::EndWrite()
{
}

