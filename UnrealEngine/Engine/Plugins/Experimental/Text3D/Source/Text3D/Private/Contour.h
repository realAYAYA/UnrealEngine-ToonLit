// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Util.h"


class FContour final : public FContourBase
{
public:
	struct FPathEntry
	{
		int32 Prev;
		int32 Next;
	};


	FContour();
	~FContour();

	/**
	 * Set Prev and Next in parts.
	 */
	void SetNeighbours();
	/**
	 * Copy parts from other contour.
	 * @param Other - Controur from which parts should be copied.
	 */
	void CopyFrom(const FContour& Other);
};
