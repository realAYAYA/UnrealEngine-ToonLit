// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlateScreenReaderBuilder.h"
#include "SlateScreenReader.h"

TSharedRef<FScreenReaderBase> FSlateScreenReaderBuilder::Create(const IScreenReaderBuilder::FArgs& InArgs)
{
	TSharedRef<FScreenReaderBase> ScreenReader = MakeShared<FSlateScreenReader>(InArgs.PlatformApplication);
	// @TODOAccessibility: Set custom screen reader application message handlers for different input handling 
	return ScreenReader;
}
