// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

class FLiveLinkHub;

/**
 * Utility base class for all content that is displayed in an entire major tab.
 */
class SLiveLinkHubTabViewBase : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SLiveLinkHubTabViewBase) {}
		/** The tab's content */
		SLATE_NAMED_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	/**
	 * @param InArgs
	 */
	void Construct(const FArguments& InArgs);
};
