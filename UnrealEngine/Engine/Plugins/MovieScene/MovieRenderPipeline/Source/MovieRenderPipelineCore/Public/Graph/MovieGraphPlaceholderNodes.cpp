// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieGraphPlaceholderNodes.h"

#include "MovieGraphConfig.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "MovieGraphNode"

static const FText NodeCategory_Rendering = LOCTEXT("NodeCategory_Rendering", "Rendering");
static const FText NodeCategory_OutputType = LOCTEXT("NodeCategory_OutputType", "Output Type");
static const FText NodeCategory_Settings = LOCTEXT("NodeCategory_Settings", "Settings");

#if WITH_EDITOR
FText UMovieGraphPathTracedRendererNode::GetNodeTitle(const bool bGetDescriptive) const
{
	static const FText PathTracedRendererNodeName = LOCTEXT("NodeName_PathTracedRenderer", "Path Traced Renderer");
	return PathTracedRendererNodeName;
}

FText UMovieGraphPathTracedRendererNode::GetMenuCategory() const
{
	return NodeCategory_Rendering;
}

FLinearColor UMovieGraphPathTracedRendererNode::GetNodeTitleColor() const
{
	static const FLinearColor DeferredRendererNodeColor = FLinearColor(0.572f, 0.274f, 1.f);
	return DeferredRendererNodeColor;
}

FSlateIcon UMovieGraphPathTracedRendererNode::GetIconAndTint(FLinearColor& OutColor) const
{
	static const FSlateIcon DeferredRendererIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "SequenceRecorder.TabIcon");

	OutColor = FLinearColor::White;
	return DeferredRendererIcon;
}
#endif // WITH_EDITOR

#if WITH_EDITOR
FText UMovieGraphEXRSequenceNode::GetNodeTitle(const bool bGetDescriptive) const
{
	static const FText EXRSequenceNodeName = LOCTEXT("NodeName_EXRSequence", ".exr Sequence");
	return EXRSequenceNodeName;
}

FText UMovieGraphEXRSequenceNode::GetMenuCategory() const
{
	return NodeCategory_OutputType;
}

FLinearColor UMovieGraphEXRSequenceNode::GetNodeTitleColor() const
{
	static const FLinearColor ImageSequenceNodeColor = FLinearColor(0.047f, 0.654f, 0.537f);
	return ImageSequenceNodeColor;
}

FSlateIcon UMovieGraphEXRSequenceNode::GetIconAndTint(FLinearColor& OutColor) const
{
	static const FSlateIcon ImageSequenceIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.Texture2D");

	OutColor = FLinearColor::White;
	return ImageSequenceIcon;
}
#endif // WITH_EDITOR

#if WITH_EDITOR
FText UMovieGraphAntiAliasingNode::GetNodeTitle(const bool bGetDescriptive) const
{
	static const FText AntiAliasingNodeName = LOCTEXT("NodeName_AntiAliasing", "Anti-Aliasing");
	return AntiAliasingNodeName;
}

FText UMovieGraphAntiAliasingNode::GetMenuCategory() const
{
	return NodeCategory_Settings;
}

FLinearColor UMovieGraphAntiAliasingNode::GetNodeTitleColor() const
{
	static const FLinearColor AntiAliasingColor = FLinearColor(0.043f, 0.219f, 0.356f);
	return AntiAliasingColor;
}

FSlateIcon UMovieGraphAntiAliasingNode::GetIconAndTint(FLinearColor& OutColor) const
{
	static const FSlateIcon SettingsIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Settings");

	OutColor = FLinearColor::White;
	return SettingsIcon;
}
#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE