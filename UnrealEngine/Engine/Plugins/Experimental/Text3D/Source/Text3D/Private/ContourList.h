// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "Text3DPrivate.h"
#include "Contour.h"
#include "Containers/List.h"


class FContourList final : public TDoubleLinkedList<FContour>
{
public:
	FContourList();
	FContourList(const FContourList& Other);

	/**
	 * Initialize Countours.
	 * @param Data - Needed to add vertices for split corners.
	 */
	void Initialize(const TSharedRef<class FData>& Data);

	/**
	 * Create contour.
	 * @return Reference to created contour.
	 */
	FContour& Add();
	/**
	 * Remove contour.
	 * @param Contour - Const reference to contour that should be removed.
	 */
	void Remove(const FContour& Contour);

	void Reset();
};
