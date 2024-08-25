// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "Math/IntPoint.h"

class UMaterial;
class UMaterialExpression;

/**
 * Algorithm for arranging material nodes.
 */
class FDMMaterialNodeArranger
{
public:
	FDMMaterialNodeArranger(UMaterial* InDynamicMaterial);

	void ArrangeNodes();

protected:
	UMaterial* DynamicMaterial;
	TMap<UMaterialExpression*, FIntPoint> NodePositions;
	FIntPoint OffsetStart = {0, 0};

	void ArrangeMaterialInputNodes(UMaterialExpression* MaterialInputExpression);

	void ArrangeNode(TMap<UMaterialExpression*, FIntPoint>& InOutNodePositions, const FIntPoint& InOffsetStart,
		UMaterialExpression* InNode, FIntPoint& InOutNodeSize);
};
