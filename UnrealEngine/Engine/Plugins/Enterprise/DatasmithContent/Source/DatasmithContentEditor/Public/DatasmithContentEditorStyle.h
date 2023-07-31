// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

/** Contains a collection of named properties (StyleSet) that guide the appearance of Datasmith related UI. */
class DATASMITHCONTENTEDITOR_API FDatasmithContentEditorStyle
{
public:
	static void Initialize();
	static void Shutdown();
	static TSharedPtr<ISlateStyle> Get() { return StyleSet; }

	static FName GetStyleSetName();

private:
	static FString InContent(const FString& RelativePath, const TCHAR* Extension);

private:
	static TSharedPtr<FSlateStyleSet> StyleSet;
};
