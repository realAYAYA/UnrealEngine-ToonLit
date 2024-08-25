// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

namespace UE::MultiUserClient
{
	/** Displays a warning triangle */
	class SWarningIcon : public SCompoundWidget
	{
	public:

		SLATE_BEGIN_ARGS(SWarningIcon)
		{}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);
	};
}


