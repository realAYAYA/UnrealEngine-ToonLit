// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

class FEditorTutorialStyle :
    public FSlateStyleSet
{
public:
	virtual const FName& GetStyleSetName() const override;

	static const FEditorTutorialStyle& Get();
	static void Shutdown();

	~FEditorTutorialStyle();

private:
	FEditorTutorialStyle();

	static FName StyleName;
	static TUniquePtr<FEditorTutorialStyle> Instance;
};

