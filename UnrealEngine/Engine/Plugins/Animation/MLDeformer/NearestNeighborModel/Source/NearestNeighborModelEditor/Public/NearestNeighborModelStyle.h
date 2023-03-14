// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

namespace UE::NearestNeighborModel
{
	class NEARESTNEIGHBORMODELEDITOR_API FNearestNeighborModelEditorStyle
		: public FSlateStyleSet
	{
	public:
		FNearestNeighborModelEditorStyle();
		~FNearestNeighborModelEditorStyle();

		static FNearestNeighborModelEditorStyle& Get();
	};

}	// namespace UE::NearestNeighborModel
