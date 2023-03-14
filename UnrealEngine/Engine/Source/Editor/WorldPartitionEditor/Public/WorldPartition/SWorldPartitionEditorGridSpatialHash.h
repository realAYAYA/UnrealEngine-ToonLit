// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "SWorldPartitionEditorGrid2D.h"

class SWorldPartitionEditorGridSpatialHash : public SWorldPartitionEditorGrid2D
{
public:
	WORLD_PARTITION_EDITOR_IMPL(SWorldPartitionEditorGridSpatialHash);

	void Construct(const FArguments& InArgs);

	virtual int32 GetSelectionSnap() const override;
	virtual int32 PaintGrid(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId) const override;
};
