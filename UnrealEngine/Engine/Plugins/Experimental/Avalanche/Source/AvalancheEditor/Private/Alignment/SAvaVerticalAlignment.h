// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"
#include "Widgets/SCompoundWidget.h"

enum class EAvaVerticalAlignment : uint8;

/**
 * Motion Design Vertical Axis Alignment
 * 
 * Widget that holds three buttons for either Top, Center, and Bottom.
 */
class SAvaVerticalAlignment : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnVerticalAlignmentChanged, EAvaVerticalAlignment)

	SLATE_BEGIN_ARGS(SAvaVerticalAlignment)
	{}
		SLATE_ATTRIBUTE(EAvaVerticalAlignment, Alignment)
		SLATE_ATTRIBUTE(FMargin, UniformPadding)
		SLATE_EVENT(FOnVerticalAlignmentChanged, OnAlignmentChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

protected:
	TAttribute<EAvaVerticalAlignment> Alignment;
	FOnVerticalAlignmentChanged OnAlignmentChanged;

	EAvaVerticalAlignment GetCurrentAlignment() const;

	void OnCurrentAlignmentChanged(const EAvaVerticalAlignment NewAlignment);
};
