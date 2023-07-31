// Copyright Epic Games, Inc. All Rights Reserved.

#include "Framework/MetaData/DriverMetaData.h"
#include "Framework/MetaData/DriverIdMetaData.h"

TSharedRef<ISlateMetaData> FDriverMetaData::Id(FName InTag)
{
	return MakeShareable(new FDriverIdMetaData(MoveTemp(InTag)));
}
