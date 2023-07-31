// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetPlacementEdModeStyle.h"

#include "Styling/CoreStyle.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/StarshipCoreStyle.h"
#include "Styling/SlateStyleRegistry.h"

FName FAssetPlacementEdModeStyle::StyleName("AssetPlacementEdModeStyle");
TUniquePtr<FAssetPlacementEdModeStyle> FAssetPlacementEdModeStyle::Inst(nullptr);

const FName& FAssetPlacementEdModeStyle::GetStyleSetName() const
{
	return StyleName;
}

const FAssetPlacementEdModeStyle& FAssetPlacementEdModeStyle::Get()
{
	if (!Inst.IsValid())
	{
		Inst = TUniquePtr<FAssetPlacementEdModeStyle>(new FAssetPlacementEdModeStyle);
	}
	return *(Inst.Get());
}

void FAssetPlacementEdModeStyle::Shutdown()
{
	Inst.Reset();
}

FAssetPlacementEdModeStyle::FAssetPlacementEdModeStyle()
	: FSlateStyleSet(StyleName)
{

	const FVector2D Icon20x20(20.0f, 20.0f);

	SetParentStyleName("EditorStyle");
	SetContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate"));
	SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

	Set("LevelEditor.AssetPlacementEdMode", new IMAGE_BRUSH_SVG("Starship/MainToolbar/AssetPlacementMode", Icon20x20));
	Set("ClassIcon.PlacementPaletteAsset", new IMAGE_BRUSH_SVG("Starship/AssetIcons/AssetPalette", FVector2D(16.0f, 16.0f)));
	Set("ClassThumbnail.PlacementPaletteAsset", new IMAGE_BRUSH_SVG("Starship/AssetIcons/AssetPalette", FVector2D(64.0f, 64.0f)));

	Set("AssetPlacementEdMode.Select",                      new IMAGE_BRUSH("Icons/GeneralTools/Select_40x", Icon20x20));
	Set("AssetPlacementEdMode.Select.Small",                new IMAGE_BRUSH("Icons/GeneralTools/Select_40x", Icon20x20));
	Set("AssetPlacementEdMode.SelectInvalid",               new IMAGE_BRUSH("Icons/GeneralTools/SelectInvalid_40x", Icon20x20));
	Set("AssetPlacementEdMode.SelectInvalid.Small",         new IMAGE_BRUSH("Icons/GeneralTools/SelectInvalid_40x", Icon20x20));
	Set("AssetPlacementEdMode.LassoSelect",                 new IMAGE_BRUSH("Icons/GeneralTools/Lasso_40x", Icon20x20));
	Set("AssetPlacementEdMode.LassoSelect.Small",           new IMAGE_BRUSH("Icons/GeneralTools/Lasso_40x", Icon20x20));
	Set("AssetPlacementEdMode.PlaceSingle",                 new IMAGE_BRUSH("Icons/GeneralTools/Foliage_40x", Icon20x20));
	Set("AssetPlacementEdMode.PlaceSingle.Small",           new IMAGE_BRUSH("Icons/GeneralTools/Foliage_40x", Icon20x20));
	Set("AssetPlacementEdMode.Place",                       new IMAGE_BRUSH("Icons/GeneralTools/Paint_40x", Icon20x20));
	Set("AssetPlacementEdMode.Place.Small",                 new IMAGE_BRUSH("Icons/GeneralTools/Paint_40x", Icon20x20));
	Set("AssetPlacementEdMode.ReapplySettings",             new IMAGE_BRUSH("Icons/GeneralTools/Repaint_40x", Icon20x20));
	Set("AssetPlacementEdMode.ReapplySettings.Small",       new IMAGE_BRUSH("Icons/GeneralTools/Repaint_40x", Icon20x20));
	Set("AssetPlacementEdMode.Fill",                        new IMAGE_BRUSH("Icons/GeneralTools/PaintBucket_40x", Icon20x20));
	Set("AssetPlacementEdMode.Fill.Small",                  new IMAGE_BRUSH("Icons/GeneralTools/PaintBucket_40x", Icon20x20));
	Set("AssetPlacementEdMode.Erase",                       new IMAGE_BRUSH("Icons/GeneralTools/Erase_40x", Icon20x20));
	Set("AssetPlacementEdMode.Erase.Small",                 new IMAGE_BRUSH("Icons/GeneralTools/Erase_40x", Icon20x20));
	Set("AssetPlacementEdMode.Filter",                      new IMAGE_BRUSH("Icons/GeneralTools/Filter_40x", Icon20x20));
	Set("AssetPlacementEdMode.Filter.Small",                new IMAGE_BRUSH("Icons/GeneralTools/Filter_40x", Icon20x20));
	Set("AssetPlacementEdMode.Settings",                    new IMAGE_BRUSH("Icons/GeneralTools/Settings_40x", Icon20x20));
	Set("AssetPlacementEdMode.Settings.Small",              new IMAGE_BRUSH("Icons/GeneralTools/Settings_40x", Icon20x20));
	Set("AssetPlacementEdMode.MoveToActivePartition",       new IMAGE_BRUSH("Icons/GeneralTools/MoveToLevel_40x", Icon20x20));
	Set("AssetPlacementEdMode.MoveToActivePartition.Small", new IMAGE_BRUSH("Icons/GeneralTools/MoveToLevel_40x", Icon20x20));

	Set("AssetPlacementEdMode.AddAssetType.Text", FTextBlockStyle(FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText"))
		.SetFont(DEFAULT_FONT("Bold", 10))
		.SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f))
		.SetHighlightColor(FLinearColor(1.0f, 1.0f, 1.0f))
		.SetShadowOffset(FVector2D(1, 1))
		.SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.9f)));

	FSlateStyleRegistry::RegisterSlateStyle(*this);
}

FAssetPlacementEdModeStyle::~FAssetPlacementEdModeStyle()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*this);
}
