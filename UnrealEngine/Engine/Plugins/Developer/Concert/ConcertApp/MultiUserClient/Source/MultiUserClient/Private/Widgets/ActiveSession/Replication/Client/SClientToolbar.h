// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

namespace UE::MultiUserClient
{
	/** Contains a shared actions that can be performed on a client view. */
	class SClientToolbar : public SCompoundWidget
	{
	public:
		
		SLATE_BEGIN_ARGS(SClientToolbar)
		{}
			/** Dedicated space for a widget with which to change the view. */
			SLATE_NAMED_SLOT(FArguments, ViewSelectionArea)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);
	};
}

