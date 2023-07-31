// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMXEntityDragDropOp.h"


class FDMXEntityFixturePatchDragDropOperation
	: public FDMXEntityDragDropOperation
{
public:
	/** 
	 * Constructs the fixture patch drag drop operation 
	 * 
	 * @param InLibrary					The library that contains the entities. Dragging from different libraries at once is not supported.
	 * @param InFixturePatches			The dragged fixture patches.
	 * @param InChannelOffset			The channel offset from the patch starting channel. Usually ChannelOffset = DraggedChannel - StartingChannel.
 	 */
	FDMXEntityFixturePatchDragDropOperation(UDMXLibrary* InLibrary, const TArray<TWeakObjectPtr<UDMXEntity>>& InFixturePatches, int32 InChannelOffset);

	int32 GetChannelOffset() const { return ChannelOffset; }

	void SetChannelOffset(int32 InChannelOffset) { ChannelOffset = InChannelOffset; }

protected:
	int32 ChannelOffset;
};
