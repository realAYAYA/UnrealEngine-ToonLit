// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuR/NullExtensionDataStreamer.h"

#include "MuR/ExtensionData.h"
#include "Templates/SharedPointer.h"

namespace mu
{

ExtensionDataPtr NullExtensionDataStreamer::CloneExtensionData(const ExtensionDataPtrConst& Source)
{
	// Note that these appear to be valid and should be good enough for testing, but they won't be
	// usable since we're not copying the actual data pointed to by Source.

	ExtensionDataPtr Result = new ExtensionData();
	Result->Index = NextIndex;
	NextIndex++;
	Result->Origin = ExtensionData::EOrigin::Runtime;

	return Result;
}

TSharedRef<const FExtensionDataLoadHandle> NullExtensionDataStreamer::StartLoad(const ExtensionDataPtrConst& Data, TArray<ExtensionDataPtrConst>& OutUnloadedConstants)
{
	OutUnloadedConstants.Reset();

	TSharedRef<FExtensionDataLoadHandle> Result = MakeShared<FExtensionDataLoadHandle>();
	Result->Data = Data;
	Result->LoadState = FExtensionDataLoadHandle::ELoadState::FailedToLoad;

	return Result;
}

}
