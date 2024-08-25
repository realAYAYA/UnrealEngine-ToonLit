// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaAnchorAlignment.h"
#include "SAvaDepthAlignment.h"
#include "SAvaHorizontalAlignment.h"
#include "SAvaVerticalAlignment.h"
#include "SlateOptMacros.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "SAvaAnchorAlignment"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SAvaAnchorAlignment::Construct(const FArguments& InArgs)
{
	Anchors = InArgs._Anchors;

	OnAnchorChanged = InArgs._OnAnchorChanged;

	ChildSlot
	.HAlign(HAlign_Left)
	.VAlign(VAlign_Center)
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 2.0f, 0.0f, 1.0f)
		[
			SNew(SAvaHorizontalAlignment)
			.UniformPadding(InArgs._UniformPadding)
			.Visibility(this, &SAvaAnchorAlignment::GetHorizontalVisibility)
			.Alignment(this, &SAvaAnchorAlignment::GetHorizontalAlignment)
			.OnAlignmentChanged(this, &SAvaAnchorAlignment::OnHorizontalAlignmentChanged)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 1.0f)
		[
			SNew(SAvaVerticalAlignment)
			.UniformPadding(InArgs._UniformPadding)
			.Visibility(this, &SAvaAnchorAlignment::GetVerticalVisibility)
			.Alignment(this, &SAvaAnchorAlignment::GetVerticalAlignment)
			.OnAlignmentChanged(this, &SAvaAnchorAlignment::OnVerticalAlignmentChanged)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 2.0f)
		[
			SNew(SAvaDepthAlignment)
			.UniformPadding(InArgs._UniformPadding)
			.Visibility(this, &SAvaAnchorAlignment::GetDepthVisibility)
			.Alignment(this, &SAvaAnchorAlignment::GetDepthAlignment)
			.OnAlignmentChanged(this, &SAvaAnchorAlignment::OnDepthAlignmentChanged)
		]
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

EVisibility SAvaAnchorAlignment::GetHorizontalVisibility() const
{
	if (!Anchors.IsSet())
	{
		return EVisibility::SelfHitTestInvisible;
	}
	
	return Anchors.Get().bUseHorizontal ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; 
}

EVisibility SAvaAnchorAlignment::GetVerticalVisibility() const
{
	if (!Anchors.IsSet())
	{
		return EVisibility::SelfHitTestInvisible;
	}
	
	return Anchors.Get().bUseVertical ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; 
}

EVisibility SAvaAnchorAlignment::GetDepthVisibility() const
{
	if (!Anchors.IsSet())
	{
		return EVisibility::SelfHitTestInvisible;
	}
	
	return Anchors.Get().bUseDepth ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; 
}

EAvaHorizontalAlignment SAvaAnchorAlignment::GetHorizontalAlignment() const
{
	return Anchors.IsSet() ? Anchors.Get().Horizontal : EAvaHorizontalAlignment::Center;
}

EAvaVerticalAlignment SAvaAnchorAlignment::GetVerticalAlignment() const
{
	return Anchors.IsSet() ? Anchors.Get().Vertical : EAvaVerticalAlignment::Center;
}

EAvaDepthAlignment SAvaAnchorAlignment::GetDepthAlignment() const
{
	return Anchors.IsSet() ? Anchors.Get().Depth : EAvaDepthAlignment::Center;
}

void SAvaAnchorAlignment::OnHorizontalAlignmentChanged(const EAvaHorizontalAlignment InAlignment)
{
	if (!Anchors.IsSet())
	{
		return;
	}

	FAvaAnchorAlignment AnchorAlignment = Anchors.Get();
	AnchorAlignment.Horizontal = InAlignment;

	if (OnAnchorChanged.IsBound())
	{
		OnAnchorChanged.Execute(AnchorAlignment);
	}
}

void SAvaAnchorAlignment::OnVerticalAlignmentChanged(const EAvaVerticalAlignment InAlignment)
{
	if (!Anchors.IsSet())
	{
		return;
	}

	FAvaAnchorAlignment AnchorAlignment = Anchors.Get();
	AnchorAlignment.Vertical = InAlignment;
	
	if (OnAnchorChanged.IsBound())
	{
		OnAnchorChanged.Execute(AnchorAlignment);
	}
}

void SAvaAnchorAlignment::OnDepthAlignmentChanged(const EAvaDepthAlignment InAlignment)
{
	if (!Anchors.IsSet())
	{
		return;
	}

	FAvaAnchorAlignment AnchorAlignment = Anchors.Get();
	AnchorAlignment.Depth = InAlignment;
	
	if (OnAnchorChanged.IsBound())
	{
		OnAnchorChanged.Execute(AnchorAlignment);
	}
}

#undef LOCTEXT_NAMESPACE
