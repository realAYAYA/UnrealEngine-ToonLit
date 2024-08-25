// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "Misc/Guid.h"

class FExternalContentDragDropOp : public FAssetDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FExternalContentDragDropOp, FAssetDragDropOp)

	FExternalContentDragDropOp() : Guid(FGuid::NewGuid()) {}
	virtual ~FExternalContentDragDropOp() {}

	const FGuid& GetGuid() const { return Guid; }

private:
	FGuid Guid;
};