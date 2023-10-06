// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ExtensionDataStreamer.h"

namespace mu
{

/** A stub implementation that can be used if there's no valid ExtensionDataStreamer */
class NullExtensionDataStreamer : public ExtensionDataStreamer
{
public:
	virtual ExtensionDataPtr CloneExtensionData(const ExtensionDataPtrConst& Source) override;
	virtual TSharedRef<const FExtensionDataLoadHandle> StartLoad(const ExtensionDataPtrConst& Data, TArray<ExtensionDataPtrConst>& OutUnloadedConstants) override;

private:
	int32 NextIndex = 0;
};

}
