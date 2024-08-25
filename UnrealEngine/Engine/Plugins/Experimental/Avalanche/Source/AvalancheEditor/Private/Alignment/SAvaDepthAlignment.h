// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"
#include "Widgets/SCompoundWidget.h"

enum class EAvaDepthAlignment : uint8;

/**
 * Motion Design Depth Axis Alignment
 * 
 * Widget that holds three buttons for either Front, Center, and Back.
 */
class SAvaDepthAlignment : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnDepthAlignmentChanged, EAvaDepthAlignment)

	SLATE_BEGIN_ARGS(SAvaDepthAlignment)
	{}
		SLATE_ATTRIBUTE(EAvaDepthAlignment, Alignment)
		SLATE_ATTRIBUTE(FMargin, UniformPadding)
		SLATE_EVENT(FOnDepthAlignmentChanged, OnAlignmentChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

protected:
	TAttribute<EAvaDepthAlignment> Alignment;

	FOnDepthAlignmentChanged OnAlignmentChanged;

	EAvaDepthAlignment GetCurrentAlignment() const;

	void OnCurrentAlignmentChanged(const EAvaDepthAlignment NewAlignment);
};
