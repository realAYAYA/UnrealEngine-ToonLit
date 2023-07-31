// Copyright Epic Games, Inc. All Rights Reserved.


#include "ChaosVisualDebuggerSlateStyle.h"
#include "Styling/SlateStyleRegistry.h"

TSharedPtr< ISlateStyle > FVisualDebuggerStyle::Instance = nullptr;


void FVisualDebuggerStyle::ResetToDefault()
{
	SetStyle(FVisualDebuggerStyle::Create());
}

void FVisualDebuggerStyle::SetStyle(const TSharedRef< ISlateStyle >& NewStyle)
{
	if (Instance.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*Instance.Get());
	}

	Instance = NewStyle;

	if (Instance.IsValid())
	{
		FSlateStyleRegistry::RegisterSlateStyle(*Instance.Get());
	}
	else
	{
		ResetToDefault();
	}
}

TSharedRef< ISlateStyle > FVisualDebuggerStyle::Create()
{
	TSharedRef< FSlateStyleSet > Style = MakeShareable(new FSlateStyleSet("TestStyle"));
	Style->SetContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));
	Style->Set("UE4Icon", new FSlateImageBrush(FPaths::EngineContentDir() / TEXT("Slate/Testing/UE4Icon.png"), FVector2D(50, 50)));

	return Style;
}
