// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaDefs.h"
#include "Delegates/Delegate.h"
#include "Widgets/SCompoundWidget.h"

class SUniformGridPanel;

/**
 * Motion Design Anchor Alignment
 * 
 * Widget that holds holds axis alignment widgets for Left/Center/Right, Top/Center/Bottom, and Front/Center/Back.
 * One alignment button set per axis row (Horizontal, Vertical, Depth).
 */
class SAvaAnchorAlignment : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnAnchorChanged, FAvaAnchorAlignment)

	SLATE_BEGIN_ARGS(SAvaAnchorAlignment)
	{}
		SLATE_ATTRIBUTE(FAvaAnchorAlignment, Anchors)
		SLATE_ATTRIBUTE(FMargin, UniformPadding)
		SLATE_EVENT(FOnAnchorChanged, OnAnchorChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	EVisibility GetHorizontalVisibility() const;
	EVisibility GetVerticalVisibility() const;
	EVisibility GetDepthVisibility() const;

	EAvaHorizontalAlignment GetHorizontalAlignment() const;
	EAvaVerticalAlignment GetVerticalAlignment() const;
	EAvaDepthAlignment GetDepthAlignment() const;

	void OnHorizontalAlignmentChanged(const EAvaHorizontalAlignment InAlignment);
	void OnVerticalAlignmentChanged(const EAvaVerticalAlignment InAlignment);
	void OnDepthAlignmentChanged(const EAvaDepthAlignment InAlignment);

protected:
	TAttribute<FAvaAnchorAlignment> Anchors;

	FOnAnchorChanged OnAnchorChanged;
};
