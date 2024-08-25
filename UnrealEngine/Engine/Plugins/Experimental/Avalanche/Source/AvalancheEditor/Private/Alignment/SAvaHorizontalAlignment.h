// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"
#include "Widgets/SCompoundWidget.h"

enum class EAvaHorizontalAlignment : uint8;

/**
 * Avalanche Horizontal Axis Alignment
 * 
 * Widget that holds three buttons for either Left, Center, and Right.
 */
class SAvaHorizontalAlignment : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnHorizontalAlignmentChanged, EAvaHorizontalAlignment)

	SLATE_BEGIN_ARGS(SAvaHorizontalAlignment)
	{}
		SLATE_ATTRIBUTE(EAvaHorizontalAlignment, Alignment)
		SLATE_ATTRIBUTE(FMargin, UniformPadding)
		SLATE_EVENT(FOnHorizontalAlignmentChanged, OnAlignmentChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

protected:
	TAttribute<EAvaHorizontalAlignment> Alignment;

	FOnHorizontalAlignmentChanged OnAlignmentChanged;

	EAvaHorizontalAlignment GetCurrentAlignment() const;

	void OnCurrentAlignmentChanged(const EAvaHorizontalAlignment NewAlignment);
};
