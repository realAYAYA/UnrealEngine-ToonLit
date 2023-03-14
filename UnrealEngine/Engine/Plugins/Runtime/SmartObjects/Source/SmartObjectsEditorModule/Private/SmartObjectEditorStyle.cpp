// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectEditorStyle.h"
#include "Styling/SlateStyleRegistry.h"

TUniquePtr<FSmartObjectEditorStyle> FSmartObjectEditorStyle::Instance(nullptr);
FColor FSmartObjectEditorStyle::TypeColor(104,49,178);

FSmartObjectEditorStyle::FSmartObjectEditorStyle() : FSlateStyleSet("SmartObjectEditorStyle")
{
	Set("ClassIcon.BlackboardKeyType_SOClaimHandle", new FSlateRoundedBoxBrush(FLinearColor(TypeColor), 2.5f, FVector2D(16.f, 5.f)));
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
	Instance.Release();
}
