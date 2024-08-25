// Copyright Epic Games, Inc. All Rights Reserved.

#include "DynamicMaterialEditorStyle.h"
#include "Brushes/SlateImageBrush.h"
#include "DetailLayoutBuilder.h"
#include "DynamicMaterialEditorModule.h"
#include "Engine/Texture2D.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/StyleColors.h"

TSharedPtr<FSlateStyleSet> FDynamicMaterialEditorStyle::StyleInstance = nullptr;

void FDynamicMaterialEditorStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FDynamicMaterialEditorStyle::Shutdown()
{
	if (StyleInstance.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
		ensure(StyleInstance.IsUnique());
		StyleInstance.Reset();
	}
}

FName FDynamicMaterialEditorStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("DynamicMaterialEditorStyle"));
	return StyleSetName;
}

const ISlateStyle& FDynamicMaterialEditorStyle::Get()
{
	if (!StyleInstance.IsValid())
	{
		Initialize();
	}
	return *StyleInstance;
}

FLinearColor FDynamicMaterialEditorStyle::GetColor(const FName& InName)
{
	return FDynamicMaterialEditorStyle::Get().GetColor(InName);
}

const FSlateBrush* FDynamicMaterialEditorStyle::GetBrush(const FName& InName)
{
	return FDynamicMaterialEditorStyle::Get().GetBrush(InName);
}

namespace UE::DynamicMaterialEditor::Private
{
	const TMap<EDMValueType, FName> TypeIcons = {
		{EDMValueType::VT_None,        TEXT("Icons.Type.None")},
		{EDMValueType::VT_Bool,        TEXT("Icons.Type.Bool")},
		{EDMValueType::VT_Float1,      TEXT("Icons.Type.Float1")},
		{EDMValueType::VT_Float2,      TEXT("Icons.Type.Float2")},
		{EDMValueType::VT_Float3_RPY,  TEXT("Icons.Type.Float3_RPY")},
		{EDMValueType::VT_Float3_RGB,  TEXT("Icons.Type.Float3_RGB")},
		{EDMValueType::VT_Float3_XYZ,  TEXT("Icons.Type.Float3_XYZ")},
		{EDMValueType::VT_Float4_RGBA, TEXT("Icons.Type.Float4_RGBA")},
		{EDMValueType::VT_Float_Any,   TEXT("Icons.Type.Float_Any")},
		{EDMValueType::VT_Texture,     TEXT("Icons.Type.Texture")},
	};
}

FName FDynamicMaterialEditorStyle::GetBrushNameForType(EDMValueType InType)
{
	if (UE::DynamicMaterialEditor::Private::TypeIcons.Contains(InType))
	{
		return UE::DynamicMaterialEditor::Private::TypeIcons[InType];
	}

	checkNoEntry();

	static FName NoType = TEXT("Icons.Type.None");
	return NoType;
}


#define IMAGE_BRUSH(RelativePath, ... ) FSlateImageBrush(Style->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define BOX_BRUSH(RelativePath, ... ) FSlateBoxBrush(Style->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define BORDER_BRUSH(RelativePath, ... ) FSlateBorderBrush(Style->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)

#define IMAGE_BRUSH_SVG(RelativePath, ...) FSlateVectorImageBrush(Style->RootToContentDir(RelativePath, TEXT(".svg")), __VA_ARGS__)
#define BOX_BRUSH_SVG(RelativePath, ...) FSlateVectorBoxBrush(Style->RootToContentDir(RelativePath, TEXT(".svg")), __VA_ARGS__)
#define BORDER_BRUSH_SVG(RelativePath, ...) FSlateVectorBorderBrush(Style->RootToContentDir(RelativePath, TEXT(".svg")), __VA_ARGS__)

#define DEFAULT_FONT(...) FCoreStyle::GetDefaultFontStyle(__VA_ARGS__)

#define IMAGE_PLUGIN_BRUSH(RelativePath, ...) FSlateImageBrush(FDynamicMaterialEditorStyle::InContent(RelativePath, ".png"), __VA_ARGS__)
#define IMAGE_PLUGIN_BRUSH_SVG(RelativePath, ...) FSlateVectorImageBrush(FDynamicMaterialEditorStyle::InContent(RelativePath, ".svg"), __VA_ARGS__)

const FVector2D Icon8x8(8.f, 8.f);
const FVector2D Icon12x12(12.f, 12.f);
const FVector2D Icon16x16(16.f, 16.f);
const FVector2D Icon20x20(20.f, 20.f);
const FVector2D Icon24x24(24.f, 24.f);
const FVector2D Icon32x32(32.f, 32.f);
const FVector2D Icon40x40(40.f, 40.f);

FLinearColor ReplaceColorAlpha(const FLinearColor& InColor, const float InNewAlpha)
{
	FLinearColor OutColor(InColor);
	OutColor.A = InNewAlpha;
	return OutColor;
}
TSharedRef<FSlateStyleSet> FDynamicMaterialEditorStyle::Create()
{
	TSharedRef<FSlateStyleSet> Style = MakeShared<FSlateStyleSet>(TEXT("DynamicMaterial"));

	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("DynamicMaterial"));
	check(Plugin.IsValid());

	Style->SetContentRoot(FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources")));

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// Color Styles
	const FLinearColor EngineSelectColor = FStyleColors::Primary.GetSpecifiedColor();
	const FLinearColor EngineSelectHoverColor = FStyleColors::PrimaryHover.GetSpecifiedColor();
	const FLinearColor EngineSelectPressColor = FStyleColors::PrimaryPress.GetSpecifiedColor();

	const FLinearColor SelectColor = ReplaceColorAlpha(FStyleColors::Select.GetSpecifiedColor(), 0.9f);
	const FLinearColor SelectHoverColor = FStyleColors::Select.GetSpecifiedColor();
	const FLinearColor SelectPressColor = EngineSelectPressColor;

	Style->Set("Color.Select", SelectColor);
	Style->Set("Color.Select.Hover", SelectHoverColor);
	Style->Set("Color.Select.Press", SelectPressColor);

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// Brush Styles
	Style->Set("Icons.Menu.Dropdown", new IMAGE_BRUSH_SVG("Icons/MenuDropdown", Icon16x16));

	Style->Set("Icons.Type.None", new IMAGE_BRUSH("Icons/ValueTypes/None", Icon12x12));
	Style->Set("Icons.Type.Bool", new IMAGE_BRUSH("Icons/ValueTypes/Bool", Icon12x12));
	Style->Set("Icons.Type.Float1", new IMAGE_BRUSH("Icons/ValueTypes/Float1", Icon12x12));
	Style->Set("Icons.Type.Float2", new IMAGE_BRUSH("Icons/ValueTypes/Float2", Icon12x12));
	Style->Set("Icons.Type.Float3_RPY", new IMAGE_BRUSH("Icons/ValueTypes/Float3_RPY", Icon12x12));
	Style->Set("Icons.Type.Float3_RGB", new IMAGE_BRUSH("Icons/ValueTypes/Float3_RGB", Icon12x12));
	Style->Set("Icons.Type.Float3_XYZ", new IMAGE_BRUSH("Icons/ValueTypes/Float3_XYZ", Icon12x12));
	Style->Set("Icons.Type.Float4_RGBA", new IMAGE_BRUSH("Icons/ValueTypes/Float4_RGBA", Icon12x12));
	Style->Set("Icons.Type.Float_Any", new IMAGE_BRUSH("Icons/ValueTypes/Float_Any", Icon12x12));
	Style->Set("Icons.Type.Texture", new IMAGE_BRUSH("Icons/ValueTypes/Texture", Icon12x12));

	Style->Set("Icons.Material.DefaultLit", new IMAGE_BRUSH("Icons/EditorIcons/MaterialTypeDefaultLit", Icon32x32));
	Style->Set("Icons.Material.Unlit", new IMAGE_BRUSH("Icons/EditorIcons/MaterialTypeUnlit", Icon32x32));

	Style->Set("Icons.Lock", new IMAGE_BRUSH_SVG("Icons/EditorIcons/Lock", Icon16x16));
	Style->Set("Icons.Unlock", new IMAGE_BRUSH_SVG("Icons/EditorIcons/Unlock", Icon16x16));

	Style->Set("Icons.Remove", new IMAGE_BRUSH("Icons/EditorIcons/Remove_16px", Icon16x16));

	Style->Set("Icons.Normalize", new IMAGE_BRUSH("Icons/EditorIcons/Normalize", Icon16x16));

	Style->Set("Icons.Stage.EnabledButton", new IMAGE_BRUSH("Icons/EditorIcons/WhiteBall", Icon8x8));
	Style->Set("Icons.Stage.BaseToggle", new IMAGE_BRUSH("Icons/EditorIcons/BaseToggle_16x", Icon16x16));
	Style->Set("Icons.Stage.MaskToggle", new IMAGE_BRUSH("Icons/EditorIcons/MaskToggle_16x", Icon16x16));
	Style->Set("Icons.Stage.Enabled", new IMAGE_BRUSH_SVG("Icons/EditorIcons/Enable", Icon24x24));
	Style->Set("Icons.Stage.Disabled", new IMAGE_BRUSH_SVG("Icons/EditorIcons/Disable", Icon24x24));

	Style->Set("Icons.Stage.ChainLinked", new IMAGE_BRUSH_SVG("Icons/EditorIcons/ChainLinked", Icon16x16));
	Style->Set("Icons.Stage.ChainUnlinked", new IMAGE_BRUSH_SVG("Icons/EditorIcons/ChainUnlinked", Icon16x16));
	Style->Set("Icons.Stage.ChainLinked.Horizontal", new IMAGE_BRUSH_SVG("Icons/EditorIcons/ChainLinked_Horizontal", Icon24x24));
	Style->Set("Icons.Stage.ChainUnlinked.Horizontal", new IMAGE_BRUSH_SVG("Icons/EditorIcons/ChainUnlinked_Horizontal", Icon24x24));
	Style->Set("Icons.Stage.ChainLinked.Vertical", new IMAGE_BRUSH_SVG("Icons/EditorIcons/ChainLinked_Vertical", Icon24x24));
	Style->Set("Icons.Stage.ChainUnlinked.Vertical", new IMAGE_BRUSH_SVG("Icons/EditorIcons/ChainUnlinked_Vertical", Icon24x24));

	Style->Set("ImageBorder", new FSlateRoundedBoxBrush(
		FLinearColor::Transparent, 10.0f, 
		FStyleColors::Panel.GetSpecifiedColor(), 2.0f));

	Style->Set("Border.SinglePixel", new BORDER_BRUSH(TEXT("Images/Borders/Border_SinglePixel"), FMargin(1.0f / 4.0f)));
	Style->Set("Border.LeftTopRight", new BORDER_BRUSH(TEXT("Images/Borders/Border_LeftTopRight"), FMargin(1.0f / 4.0f, 1.0f / 2.0f)));
	Style->Set("Border.LeftBottomRight", new BORDER_BRUSH(TEXT("Images/Borders/Border_LeftBottomRight"), FMargin(1.0f / 4.0f, 1.0f / 2.0f)));
	Style->Set("Border.TopLeftBottom", new BORDER_BRUSH(TEXT("Images/Borders/Border_TopLeftBottom"), FMargin(1.0f / 2.0f, 1.0f / 4.0f)));
	Style->Set("Border.TopRightBottom", new BORDER_BRUSH(TEXT("Images/Borders/Border_TopRightBottom"), FMargin(1.0f / 2.0f, 1.0f / 4.0f)));
	Style->Set("Border.Top", new BORDER_BRUSH(TEXT("Images/Borders/Border_Top"), FMargin(0.0f, 1.0f / 2.0f)));
	Style->Set("Border.Bottom", new BORDER_BRUSH(TEXT("Images/Borders/Border_Bottom"), FMargin(0.0f, 1.0f / 2.0f)));
	Style->Set("Border.Left", new BORDER_BRUSH(TEXT("Images/Borders/Border_Left"), FMargin(1.0f / 2.0f, 0.0f)));
	Style->Set("Border.Right", new BORDER_BRUSH(TEXT("Images/Borders/Border_Right"), FMargin(1.0f / 2.0f, 0.0f)));

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// Button Styles
	Style->Set("HoverHintOnly", FButtonStyle()
		.SetNormal(FSlateNoResource())
		.SetHovered(FSlateRoundedBoxBrush(FLinearColor(1, 1, 1, 0.15f), 4.0f))
		.SetPressed(FSlateRoundedBoxBrush(FLinearColor(1, 1, 1, 0.25f), 4.0f))
		.SetNormalPadding(FMargin(0, 0, 0, 1))
		.SetPressedPadding(FMargin(0, 1, 0, 0)));

	Style->Set("HoverHintOnly.Bordered", FButtonStyle()
		.SetNormal(FSlateRoundedBoxBrush(FLinearColor::Transparent, 4.0f, FLinearColor(1, 1, 1, 0.25f), 1.0f))
		.SetHovered(FSlateRoundedBoxBrush(FLinearColor(1, 1, 1, 0.15f), 4.0f, FLinearColor(1, 1, 1, 0.4f), 1.0f))
		.SetPressed(FSlateRoundedBoxBrush(FLinearColor(1, 1, 1, 0.25f), 4.0f, FLinearColor(1, 1, 1, 0.5f), 1.0f))
		.SetNormalPadding(FMargin(0, 0, 0, 1))
		.SetPressedPadding(FMargin(0, 1, 0, 0)));

	Style->Set("HoverHintOnly.Bordered.Dark", FButtonStyle()
		.SetNormal(FSlateRoundedBoxBrush(FLinearColor::Transparent, 4.0f, FStyleColors::InputOutline.GetSpecifiedColor(), 1.0f))
		.SetHovered(FSlateRoundedBoxBrush(FLinearColor(1, 1, 1, 0.15f), 4.0f, FLinearColor(1, 1, 1, 0.4f), 1.0f))
		.SetPressed(FSlateRoundedBoxBrush(FLinearColor(1, 1, 1, 0.25f), 4.0f, FLinearColor(1, 1, 1, 0.5f), 1.0f))
		.SetNormalPadding(FMargin(0, 0, 0, 1))
		.SetPressedPadding(FMargin(0, 1, 0, 0)));

	SetupStageStyles(Style);
	SetupLayerViewStyles(Style);
	SetupLayerViewItemHandleStyles(Style);
	SetupEffectsViewStyles(Style);
	SetupTextStyles(Style);

	//////////////////////////////////////////////////////
	/// Editable TextBox Style
	FEditableTextBoxStyle InlineEditableTextBoxStyle;
	InlineEditableTextBoxStyle.SetPadding(FMargin(0));
	InlineEditableTextBoxStyle.SetBackgroundColor(FSlateColor(FLinearColor::Transparent));

	Style->Set("InlineEditableTextBoxStyle", InlineEditableTextBoxStyle);

	return Style;
}

void FDynamicMaterialEditorStyle::SetupStageStyles(const TSharedRef<FSlateStyleSet>& Style)
{
	constexpr FLinearColor StageEnabledColor = FLinearColor(0.0f, 1.0f, 0.0f, 0.7f);
	constexpr FLinearColor StageDisabledColor = FLinearColor(1.0f, 0.0f, 0.0f, 0.7f);

	Style->Set("Color.Stage.Enabled", StageEnabledColor);
	Style->Set("Color.Stage.Disabled", StageDisabledColor);

	const float StageCornerRadius = 6.0f;
	const float StageBorderWidth = 2.0f;

	Style->Set("Stage.Inactive", new FSlateRoundedBoxBrush(
		FLinearColor::Transparent, StageCornerRadius,
		FStyleColors::Panel.GetSpecifiedColor(), 1.0f));
	Style->Set("Stage.Inactive.Hover", new FSlateRoundedBoxBrush(
		FLinearColor::Transparent, StageCornerRadius,
		ReplaceColorAlpha(FStyleColors::Foreground.GetSpecifiedColor(), 0.2f), 1.0f));
	Style->Set("Stage.Inactive.Select", new FSlateRoundedBoxBrush(
		FLinearColor::Transparent, StageCornerRadius,
		Style->GetColor("Color.Select"), 2.0f));
	Style->Set("Stage.Inactive.Select.Hover", new FSlateRoundedBoxBrush(
		FLinearColor::Transparent, StageCornerRadius,
		Style->GetColor("Color.Select.Hover"), 2.0f));

	Style->Set("Stage.Disabled", new FSlateRoundedBoxBrush(
		FLinearColor::Transparent, StageCornerRadius,
		ReplaceColorAlpha(StageDisabledColor, 0.8f), 1.0f));
	Style->Set("Stage.Disabled.Hover", new FSlateRoundedBoxBrush(
		FLinearColor::Transparent, StageCornerRadius,
		StageDisabledColor, 1.0f));
	Style->Set("Stage.Disabled.Select", new FSlateRoundedBoxBrush(
		FLinearColor::Transparent, StageCornerRadius,
		ReplaceColorAlpha(FStyleColors::Select.GetSpecifiedColor(), 0.9f), 2.0f));
	Style->Set("Stage.Disabled.Select.Hover", new FSlateRoundedBoxBrush(
		FLinearColor::Transparent, StageCornerRadius,
		FStyleColors::Select.GetSpecifiedColor(), 2.0f));

	Style->Set("Stage.Outline", new FSlateRoundedBoxBrush(
		FStyleColors::InputOutline.GetSpecifiedColor(), 1.0f));
	Style->Set("Stage.Drag", new FSlateRoundedBoxBrush(
		FLinearColor::Transparent, StageCornerRadius,
		FStyleColors::AccentYellow.GetSpecifiedColor(), 2.0f));

	Style->Set("Stage.TextBlock", new FSlateRoundedBoxBrush(
		ReplaceColorAlpha(FStyleColors::Panel.GetSpecifiedColor(), 0.7f), 0.0f,
		ReplaceColorAlpha(FStyleColors::InputOutline.GetSpecifiedColor(), 0.4f), 1.0f));
}

void FDynamicMaterialEditorStyle::SetupLayerViewStyles(const TSharedRef<FSlateStyleSet>& Style)
{
	Style->Set("LayerView.Background", new FSlateRoundedBoxBrush(
		FStyleColors::Panel.GetSpecifiedColor()/*FLinearColor(0, 0, 0, 0.25f)*/, 6.0f,
		FStyleColors::Header.GetSpecifiedColor()/*FLinearColor(1, 1, 1, 0.2f)*/, 1.0f));

	Style->Set("LayerView.Details.Background", new FSlateRoundedBoxBrush(
		FStyleColors::Recessed.GetSpecifiedColor()/*FLinearColor(0, 0, 0, 0.25f)*/, 6.0f,
		FStyleColors::Header.GetSpecifiedColor()/*FLinearColor(1, 1, 1, 0.2f)*/, 1.0f));

	/**
	 * SListView and and FTableViewStyle have no support for adding padding between the background brush
	 * and the SListView widget, so we are not using this style for the SDMSlotLayerView. Instead, we add a
	 * SBorder around the SDMBLayerView and style that.
	 */
	Style->Set("LayerView", FTableViewStyle()
		.SetBackgroundBrush(*Style->GetBrush("LayerView.Background"))
	);

	const float LayerViewItemCornerRadius = 5.0f;
	const float LayerViewItemBorderWidth = 1.0f;

	const FLinearColor LayerViewItemFillColor = FLinearColor::Transparent;
	constexpr FLinearColor LayerViewItemBorderColor = FLinearColor(1, 1, 1, 0.15f);

	const FLinearColor LayerItemHoverFillColor = FStyleColors::Recessed.GetSpecifiedColor();
	constexpr FLinearColor LayerItemHoverBorderColor = FLinearColor(1, 1, 1, 0.2f);

	const FLinearColor LayerItemSelectFillColor = FStyleColors::Header.GetSpecifiedColor();
	const FLinearColor LayerItemSelectBorderColor = ReplaceColorAlpha(FStyleColors::Select.GetSpecifiedColor(), 0.9f);

	//constexpr FLinearColor LayerItemSelectHoverFillColor = FLinearColor(1, 1, 1, 0.25f);
	//const FLinearColor LayerItemSelectHoverBorderColor = FStyleColors::Select.GetSpecifiedColor();

	Style->Set("LayerView.Row.Item", new FSlateRoundedBoxBrush(
		LayerViewItemFillColor, LayerViewItemCornerRadius,
		LayerViewItemBorderColor, LayerViewItemBorderWidth));

	Style->Set("LayerView.Row.Hovered", new FSlateRoundedBoxBrush(
		LayerItemHoverFillColor, LayerViewItemCornerRadius,
		LayerItemHoverBorderColor, LayerViewItemBorderWidth));

	Style->Set("LayerView.Row.Selected", new FSlateRoundedBoxBrush(
		LayerItemSelectFillColor, LayerViewItemCornerRadius,
		LayerItemSelectBorderColor, LayerViewItemBorderWidth));

	Style->Set("LayerView.Row.ActiveBrush", new FSlateRoundedBoxBrush(
		LayerItemSelectFillColor, LayerViewItemCornerRadius,
		LayerItemSelectBorderColor, LayerViewItemBorderWidth));
	Style->Set("LayerView.Row.ActiveHoveredBrush", new FSlateRoundedBoxBrush(
		LayerItemSelectFillColor, LayerViewItemCornerRadius,
		LayerItemSelectBorderColor, LayerViewItemBorderWidth));
	Style->Set("LayerView.Row.InactiveBrush", new FSlateRoundedBoxBrush(
		LayerItemSelectFillColor, LayerViewItemCornerRadius,
		LayerItemSelectBorderColor, LayerViewItemBorderWidth));
	Style->Set("LayerView.Row.InactiveHoveredBrush", new FSlateRoundedBoxBrush(
		LayerItemSelectFillColor, LayerViewItemCornerRadius,
		LayerItemSelectBorderColor, LayerViewItemBorderWidth));

	Style->Set("LayerView.Row", FTableRowStyle()
		.SetTextColor(FStyleColors::Foreground)
		.SetSelectedTextColor(FStyleColors::ForegroundHover)
		.SetEvenRowBackgroundBrush(*Style->GetBrush(TEXT("LayerView.Row.Item")))
		.SetEvenRowBackgroundHoveredBrush(*Style->GetBrush(TEXT("LayerView.Row.Hovered")))
		.SetOddRowBackgroundBrush(*Style->GetBrush(TEXT("LayerView.Row.Item")))
		.SetOddRowBackgroundHoveredBrush(*Style->GetBrush(TEXT("LayerView.Row.Hovered")))
		.SetSelectorFocusedBrush(*Style->GetBrush(TEXT("LayerView.Row.Selected")))
		.SetActiveBrush(*Style->GetBrush(TEXT("LayerView.Row.ActiveBrush")))
		.SetActiveHoveredBrush(*Style->GetBrush(TEXT("LayerView.Row.ActiveHoveredBrush")))
		.SetInactiveBrush(*Style->GetBrush(TEXT("LayerView.Row.InactiveBrush")))
		.SetInactiveHoveredBrush(*Style->GetBrush(TEXT("LayerView.Row.InactiveHoveredBrush")))
		.SetSelectorFocusedBrush(BORDER_BRUSH("Images/DropIndicators/DropZoneIndicator_Onto", FMargin(4.f / 16.f), Style->GetColor(TEXT("Color.Select.Hover"))))
		.SetDropIndicator_Onto(BOX_BRUSH("Images/DropIndicators/LayerView_DropIndicator_Onto", FMargin(4.0f / 16.0f), Style->GetColor(TEXT("Color.Select.Hover"))))
		.SetDropIndicator_Above(BOX_BRUSH("Images/DropIndicators/LayerView_DropIndicator_Above", FMargin(4.0f / 16.0f, 4.0f / 16.0f, 0.f, 0.f), Style->GetColor(TEXT("Color.Select.Hover"))))
		.SetDropIndicator_Below(BOX_BRUSH("Images/DropIndicators/LayerView_DropIndicator_Below", FMargin(4.0f / 16.0f, 0.f, 0.f, 4.0f / 16.0f), Style->GetColor(TEXT("Color.Select.Hover"))))
	);

	Style->Set("LayerView.AddIcon", new IMAGE_BRUSH("Icons/EditorIcons/LayerAdd", Icon16x16));
	Style->Set("LayerView.DuplicateIcon", new IMAGE_BRUSH("Icons/EditorIcons/Duplicate_40x", Icon40x40));
	Style->Set("LayerView.RemoveIcon", new IMAGE_BRUSH("Icons/EditorIcons/LayerRemove", Icon16x16));

	Style->Set("LayerView.Row.Handle", new IMAGE_BRUSH_SVG("Icons/DragHandle", Icon16x16));
}

void FDynamicMaterialEditorStyle::SetupLayerViewItemHandleStyles(const TSharedRef<FSlateStyleSet>& Style)
{
	constexpr FLinearColor RowHandleFillColor = FLinearColor(1, 1, 1, 0.3f);
	const FLinearColor RowHandleHoverFillColor = FLinearColor(1, 1, 1, 0.4f);
	const FLinearColor RowHandleBorderColor = FLinearColor::Transparent;
	constexpr float RowHandleCornerRadius = 6.0f;
	constexpr float RowHandleBorderWidth = 1.0f;

	Style->Set("LayerView.Row.Handle.Left", new FSlateRoundedBoxBrush(
		RowHandleFillColor, FVector4(RowHandleCornerRadius, 0.0f, 0.0f, RowHandleCornerRadius),
		RowHandleBorderColor, RowHandleBorderWidth));
	Style->Set("LayerView.Row.Handle.Top", new FSlateRoundedBoxBrush(
		RowHandleFillColor, FVector4(RowHandleCornerRadius, RowHandleCornerRadius, 0.0f, 0.0f),
		RowHandleBorderColor, RowHandleBorderWidth));
	Style->Set("LayerView.Row.Handle.Right", new FSlateRoundedBoxBrush(
		RowHandleFillColor, FVector4(0.0f, RowHandleCornerRadius, RowHandleCornerRadius, 0.0f),
		RowHandleBorderColor, RowHandleBorderWidth));
	Style->Set("LayerView.Row.Handle.Bottom", new FSlateRoundedBoxBrush(
		RowHandleFillColor, FVector4(0.0f, 0.0f, RowHandleCornerRadius, RowHandleCornerRadius),
		RowHandleBorderColor, RowHandleBorderWidth));

	Style->Set("LayerView.Row.Handle.Left.Hover", new FSlateRoundedBoxBrush(
		RowHandleHoverFillColor, FVector4(RowHandleCornerRadius, 0.0f, 0.0f, RowHandleCornerRadius),
		RowHandleBorderColor, RowHandleBorderWidth));
	Style->Set("LayerView.Row.Handle.Top.Hover", new FSlateRoundedBoxBrush(
		RowHandleHoverFillColor, FVector4(RowHandleCornerRadius, RowHandleCornerRadius, 0.0f, 0.0f),
		RowHandleBorderColor, RowHandleBorderWidth));
	Style->Set("LayerView.Row.Handle.Right.Hover", new FSlateRoundedBoxBrush(
		RowHandleHoverFillColor, FVector4(0.0f, RowHandleCornerRadius, RowHandleCornerRadius, 0.0f),
		RowHandleBorderColor, RowHandleBorderWidth));
	Style->Set("LayerView.Row.Handle.Bottom.Hover", new FSlateRoundedBoxBrush(
		RowHandleHoverFillColor, FVector4(0.0f, 0.0f, RowHandleCornerRadius, RowHandleCornerRadius),
		RowHandleBorderColor, RowHandleBorderWidth));

	Style->Set("LayerView.Row.Handle.Left.Select", new FSlateRoundedBoxBrush(
		Style->GetColor(TEXT("Color.Select")), 0.0f,
		RowHandleBorderColor, 0.0f));
	Style->Set("LayerView.Row.Handle.Top.Select", new FSlateRoundedBoxBrush(
		Style->GetColor(TEXT("Color.Select")), 0.0f,
		RowHandleBorderColor, 0.0f));
	Style->Set("LayerView.Row.Handle.Right.Select", new FSlateRoundedBoxBrush(
		Style->GetColor(TEXT("Color.Select")), 0.0f,
		RowHandleBorderColor, 0.0f));
	Style->Set("LayerView.Row.Handle.Bottom.Select", new FSlateRoundedBoxBrush(
		Style->GetColor(TEXT("Color.Select")), 0.0f,
		RowHandleBorderColor, 0.0f));

	Style->Set("LayerView.Row.Handle.Left.Select.Hover", new FSlateRoundedBoxBrush(
		Style->GetColor(TEXT("Color.Select.Hover")), 0.0f,
		RowHandleBorderColor, 0.0f));
	Style->Set("LayerView.Row.Handle.Top.Select.Hover", new FSlateRoundedBoxBrush(
		Style->GetColor(TEXT("Color.Select.Hover")), 0.0f,
		RowHandleBorderColor, 0.0f));
	Style->Set("LayerView.Row.Handle.Right.Select.Hover", new FSlateRoundedBoxBrush(
		Style->GetColor(TEXT("Color.Select.Hover")), 0.0f,
		RowHandleBorderColor, 0.0f));
	Style->Set("LayerView.Row.Handle.Bottom.Select.Hover", new FSlateRoundedBoxBrush(
		Style->GetColor(TEXT("Color.Select.Hover")), 0.0f,
		RowHandleBorderColor, 0.0f));
}

void FDynamicMaterialEditorStyle::SetupEffectsViewStyles(const TSharedRef<FSlateStyleSet>& Style)
{
	Style->Set("EffectsView.Background", new FSlateRoundedBoxBrush(
		FStyleColors::Panel.GetSpecifiedColor(), 0.0f,
		FStyleColors::Header.GetSpecifiedColor(), 1.0f));

	Style->Set("EffectsView.Details.Background", new FSlateRoundedBoxBrush(
		FStyleColors::Recessed.GetSpecifiedColor(), 0.0f,
		FStyleColors::Header.GetSpecifiedColor(), 1.0f));

	Style->Set("EffectsView", FTableViewStyle()
		.SetBackgroundBrush(*Style->GetBrush(TEXT("EffectsView.Background")))
	);

	const float LayerViewItemCornerRadius = 0.0f;
	const float LayerViewItemBorderWidth = 1.0f;

	const FLinearColor LayerViewItemFillColor = FLinearColor::Transparent;
	constexpr FLinearColor LayerViewItemBorderColor = FLinearColor(1, 1, 1, 0.15f);

	const FLinearColor LayerItemHoverFillColor = FStyleColors::Recessed.GetSpecifiedColor();
	constexpr FLinearColor LayerItemHoverBorderColor = FLinearColor(1, 1, 1, 0.2f);

	const FLinearColor LayerItemSelectFillColor = FStyleColors::Header.GetSpecifiedColor();
	const FLinearColor LayerItemSelectBorderColor = ReplaceColorAlpha(FStyleColors::Select.GetSpecifiedColor(), 0.9f);

	Style->Set("EffectsView.Row.Item", new FSlateRoundedBoxBrush(
		LayerViewItemFillColor, LayerViewItemCornerRadius,
		LayerViewItemBorderColor, LayerViewItemBorderWidth));

	Style->Set("EffectsView.Row.Hovered", new FSlateRoundedBoxBrush(
		LayerItemHoverFillColor, LayerViewItemCornerRadius,
		LayerItemHoverBorderColor, LayerViewItemBorderWidth));

	Style->Set("EffectsView.Row.Selected", new FSlateRoundedBoxBrush(
		LayerItemSelectFillColor, LayerViewItemCornerRadius,
		LayerItemSelectBorderColor, LayerViewItemBorderWidth));

	Style->Set("EffectsView.Row.ActiveBrush", new FSlateRoundedBoxBrush(
		LayerItemSelectFillColor, LayerViewItemCornerRadius,
		LayerItemSelectBorderColor, LayerViewItemBorderWidth));
	Style->Set("EffectsView.Row.ActiveHoveredBrush", new FSlateRoundedBoxBrush(
		LayerItemSelectFillColor, LayerViewItemCornerRadius,
		LayerItemSelectBorderColor, LayerViewItemBorderWidth));
	Style->Set("EffectsView.Row.InactiveBrush", new FSlateRoundedBoxBrush(
		LayerItemSelectFillColor, LayerViewItemCornerRadius,
		LayerItemSelectBorderColor, LayerViewItemBorderWidth));
	Style->Set("EffectsView.Row.InactiveHoveredBrush", new FSlateRoundedBoxBrush(
		LayerItemSelectFillColor, LayerViewItemCornerRadius,
		LayerItemSelectBorderColor, LayerViewItemBorderWidth));

	Style->Set("EffectsView.Row", FTableRowStyle()
		.SetTextColor(FStyleColors::Foreground)
		.SetSelectedTextColor(FStyleColors::ForegroundHover)
		.SetEvenRowBackgroundBrush(*Style->GetBrush(TEXT("EffectsView.Row.Item")))
		.SetEvenRowBackgroundHoveredBrush(*Style->GetBrush(TEXT("EffectsView.Row.Hovered")))
		.SetOddRowBackgroundBrush(*Style->GetBrush(TEXT("EffectsView.Row.Item")))
		.SetOddRowBackgroundHoveredBrush(*Style->GetBrush(TEXT("EffectsView.Row.Hovered")))
		.SetSelectorFocusedBrush(*Style->GetBrush(TEXT("EffectsView.Row.Selected")))
		.SetActiveBrush(*Style->GetBrush(TEXT("EffectsView.Row.ActiveBrush")))
		.SetActiveHoveredBrush(*Style->GetBrush(TEXT("EffectsView.Row.ActiveHoveredBrush")))
		.SetInactiveBrush(*Style->GetBrush(TEXT("EffectsView.Row.InactiveBrush")))
		.SetInactiveHoveredBrush(*Style->GetBrush(TEXT("EffectsView.Row.InactiveHoveredBrush")))
		.SetSelectorFocusedBrush(BORDER_BRUSH("Images/DropIndicators/DropZoneIndicator_Onto", FMargin(4.f / 16.f), Style->GetColor(TEXT("Color.Select.Hover"))))
		.SetDropIndicator_Onto(BOX_BRUSH("Images/DropIndicators/LayerView_DropIndicator_Onto", FMargin(4.0f / 16.0f), Style->GetColor(TEXT("Color.Select.Hover"))))
		.SetDropIndicator_Above(BOX_BRUSH("Images/DropIndicators/LayerView_DropIndicator_Above", FMargin(4.0f / 16.0f, 4.0f / 16.0f, 0.f, 0.f), Style->GetColor(TEXT("Color.Select.Hover"))))
		.SetDropIndicator_Below(BOX_BRUSH("Images/DropIndicators/LayerView_DropIndicator_Below", FMargin(4.0f / 16.0f, 0.f, 0.f, 4.0f / 16.0f), Style->GetColor(TEXT("Color.Select.Hover"))))
	);

	Style->Set("EffectsView.Row.Fx.Closed", new IMAGE_BRUSH_SVG("Icons/Fx_Closed", Icon24x24));
	Style->Set("EffectsView.Row.Fx.Opened", new IMAGE_BRUSH_SVG("Icons/Fx_Opened", Icon24x24));
	Style->Set("EffectsView.Row.Fx", new IMAGE_BRUSH_SVG("Icons/Fx", Icon24x24));
}

void FDynamicMaterialEditorStyle::SetupTextStyles(const TSharedRef<FSlateStyleSet>& Style)
{
	const FTextBlockStyle NormalTextStyle(FAppStyle::GetWidgetStyle<FTextBlockStyle>(TEXT("NormalText")));

	const FLinearColor LayerViewItemTextShadowColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.5f);

	FFontOutlineSettings HandleFontOutline;
	HandleFontOutline.OutlineColor = LayerViewItemTextShadowColor;
	HandleFontOutline.OutlineSize = 1;

	FTextBlockStyle ActorNameTextStyle(NormalTextStyle);
	ActorNameTextStyle.SetFont(DEFAULT_FONT("Regular", 10));
	Style->Set("ActorName", ActorNameTextStyle);

	FTextBlockStyle ActorNameBigTextStyle(NormalTextStyle);
	ActorNameBigTextStyle.SetFont(DEFAULT_FONT("Regular", 14));
	Style->Set("ActorNameBig", ActorNameBigTextStyle);

	FTextBlockStyle ComponentNameBigTextStyle(NormalTextStyle);
	ComponentNameBigTextStyle.SetFont(DEFAULT_FONT("Regular", 12));
	Style->Set("ComponentNameBig", ComponentNameBigTextStyle);

	FTextBlockStyle SlotLayerInfoTextStyle(NormalTextStyle);
	SlotLayerInfoTextStyle.SetFont(DEFAULT_FONT("Italic", 8));
	Style->Set("SlotLayerInfo", SlotLayerInfoTextStyle);

	FSlateFontInfo LayerViewItemFont = DEFAULT_FONT("Bold", 12);
	LayerViewItemFont.OutlineSettings = HandleFontOutline;
	Style->Set("LayerView.Row.Font", LayerViewItemFont);

	FSlateFontInfo LayerViewItemHandleSmallFont = DEFAULT_FONT("Regular", 10);
	LayerViewItemHandleSmallFont.OutlineSettings = HandleFontOutline;
	Style->Set("LayerView.Row.HandleFont", LayerViewItemHandleSmallFont);

	FTextBlockStyle LayerViewItemTextStyle(NormalTextStyle);
	LayerViewItemTextStyle.SetShadowOffset(FVector2D(1.0f, 1.0f));
	LayerViewItemTextStyle.SetColorAndOpacity(LayerViewItemTextShadowColor);

	Style->Set("LayerView.Row.HeaderText",
		FTextBlockStyle(LayerViewItemTextStyle)
		.SetColorAndOpacity(FStyleColors::Foreground)
		.SetFont(LayerViewItemFont));

	Style->Set("LayerView.Row.HeaderText.Small",
		FTextBlockStyle(LayerViewItemTextStyle)
		.SetColorAndOpacity(FStyleColors::Foreground)
		.SetFont(LayerViewItemHandleSmallFont));

	FTextBlockStyle StagePropertyDetailsTextStyle(NormalTextStyle);
	StagePropertyDetailsTextStyle.SetFont(DEFAULT_FONT("Regular", 12));
	Style->Set("Font.Stage.Details", StagePropertyDetailsTextStyle);

	FTextBlockStyle StagePropertyDetailsBoldTextStyle(NormalTextStyle);
	StagePropertyDetailsBoldTextStyle.SetFont(DEFAULT_FONT("Bold", 12));
	Style->Set("Font.Stage.Details.Bold", StagePropertyDetailsBoldTextStyle);

	FTextBlockStyle StagePropertyDetailsSmallTextStyle(NormalTextStyle);
	StagePropertyDetailsSmallTextStyle.SetFont(IDetailLayoutBuilder::GetDetailFont());
	Style->Set("Font.Stage.Details.Small", StagePropertyDetailsSmallTextStyle);

	FTextBlockStyle StagePropertyDetailsSmallBoldTextStyle(NormalTextStyle);
	StagePropertyDetailsSmallBoldTextStyle.SetFont(IDetailLayoutBuilder::GetDetailFontBold());
	Style->Set("Font.Stage.Details.Small.Bold", StagePropertyDetailsSmallBoldTextStyle);

	Style->Set("Font.Stage.Details.Small.Bold", FTextBlockStyle(NormalTextStyle)
		.SetFont(IDetailLayoutBuilder::GetDetailFontBold()));
}

#undef IMAGE_BRUSH
#undef BOX_BRUSH
#undef BORDER_BRUSH

#undef IMAGE_BRUSH_SVG
#undef BOX_BRUSH_SVG
#undef BORDER_BRUSH_SVG

#undef DEFAULT_FONT

#undef IMAGE_PLUGIN_BRUSH
#undef IMAGE_PLUGIN_BRUSH_SVG
