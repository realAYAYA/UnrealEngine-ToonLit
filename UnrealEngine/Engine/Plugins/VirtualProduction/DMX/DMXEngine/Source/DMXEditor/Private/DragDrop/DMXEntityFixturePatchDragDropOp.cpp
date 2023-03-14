// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXEntityFixturePatchDragDropOp.h"


FDMXEntityFixturePatchDragDropOperation::FDMXEntityFixturePatchDragDropOperation(UDMXLibrary* InLibrary, const TArray<TWeakObjectPtr<UDMXEntity>>& InFixturePatches, int32 InChannelOffset)
	: FDMXEntityDragDropOperation(InLibrary, InFixturePatches)
	, ChannelOffset(InChannelOffset)
{}
