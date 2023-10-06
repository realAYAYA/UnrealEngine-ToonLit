// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectEditorStyle.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateStyleMacros.h"

TUniquePtr<FSmartObjectEditorStyle> FSmartObjectEditorStyle::Instance(nullptr);
FColor FSmartObjectEditorStyle::TypeColor(104,49,178);

FSmartObjectEditorStyle::FSmartObjectEditorStyle() : FSlateStyleSet("SmartObjectEditorStyle")
{
	const FString PluginContentDir = FPaths::EnginePluginsDir() / TEXT("Runtime/SmartObjects/Resources");
	SetContentRoot(PluginContentDir);
	
	Set("ClassIcon.BlackboardKeyType_SOClaimHandle", new FSlateRoundedBoxBrush(FLinearColor(TypeColor), 2.5f, FVector2D(16.f, 5.f)));

	Set("ClassIcon.SmartObjectDefinition", new IMAGE_BRUSH_SVG("Icons/SmartObject_16", CoreStyleConstants::Icon16x16));
	Set("ClassThumbnail.SmartObjectDefinition", new IMAGE_BRUSH_SVG("Icons/SmartObject_64", CoreStyleConstants::Icon64x64));
	Set("ClassIcon.SmartObjectComponent", new IMAGE_BRUSH_SVG("Icons/SmartObject_16", CoreStyleConstants::Icon16x16));
	Set("ClassThumbnail.SmartObjectComponent", new IMAGE_BRUSH_SVG("Icons/SmartObject_64", CoreStyleConstants::Icon64x64));
	
	FSlateStyleRegistry::RegisterSlateStyle(*this);
}

FSmartObjectEditorStyle::~FSmartObjectEditorStyle()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*this);
}

FSmartObjectEditorStyle& FSmartObjectEditorStyle::Get()
{
	if (!Instance.IsValid())
	{
		Instance = TUniquePtr<FSmartObjectEditorStyle>(new FSmartObjectEditorStyle);
	}
	return *(Instance.Get());
}

void FSmartObjectEditorStyle::Shutdown()
{
	Instance.Reset();
}
