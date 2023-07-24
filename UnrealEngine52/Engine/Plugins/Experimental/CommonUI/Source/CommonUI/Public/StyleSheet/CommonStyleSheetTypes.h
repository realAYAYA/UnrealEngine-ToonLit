// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Fonts/SlateFontInfo.h"
#include "Math/Color.h"

#include "CommonStyleSheetTypes.generated.h"

UCLASS(Abstract, EditInlineNew)
class UCommonStyleSheetTypeBase : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Value", meta = (InlineEditConditionToggle))
	bool bIsEnabled = true;
};

UCLASS(meta = (DisplayName = "Color"))
class UCommonStyleSheetTypeColor : public UCommonStyleSheetTypeBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Value", meta = (HideAlphaChannel, EditCondition = "bIsEnabled"))
	FLinearColor Color = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
};

UCLASS(meta = (DisplayName = "Opacity"))
class UCommonStyleSheetTypeOpacity : public UCommonStyleSheetTypeBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Value", meta = (ClampMin = 0.0f, ClampMax = 1.0f, EditCondition = "bIsEnabled"))
	float Opacity = 1.0f;
};

UCLASS(meta = (DisplayName = "Line Height Percentage"))
class UCommonStyleSheetTypeLineHeightPercentage : public UCommonStyleSheetTypeBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Value", meta = (EditCondition = "bIsEnabled"))
	float LineHeightPercentage = 1.0f;
};

UCLASS(meta = (DisplayName = "Font - Typeface"))
class UCommonStyleSheetTypeFontTypeface : public UCommonStyleSheetTypeBase
{
	GENERATED_BODY()

public:
//	FIXME: Should be used instead of SlateFontInfo but no time to do the customization
// 	UPROPERTY(EditDefaultsOnly, Category = "Value", meta = (AllowedClasses = "/Script/Engine.Font"))
// 	TObjectPtr<const UObject> FontFamily;
// 
// 	UPROPERTY(EditDefaultsOnly, Category = "Value")
// 	FName Typeface;

	UPROPERTY(EditDefaultsOnly, Category = "Value", meta = (EditCondition = "bIsEnabled"))
	FSlateFontInfo Typeface;
};

UCLASS(meta = (DisplayName = "Font - Size"))
class UCommonStyleSheetTypeFontSize : public UCommonStyleSheetTypeBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Value", meta = (ClampMin = 1, ClampMax = 1000, EditCondition = "bIsEnabled"))
	int32 Size = 24;
};

UCLASS(meta = (DisplayName = "Font - Letter Spacing"))
class UCommonStyleSheetTypeFontLetterSpacing : public UCommonStyleSheetTypeBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Value", meta = (ClampMin = -1000, ClampMax = 10000, EditCondition = "bIsEnabled"))
	int32 LetterSpacing;
};

UCLASS(meta = (DisplayName = "Margin - Left"))
class UCommonStyleSheetTypeMarginLeft : public UCommonStyleSheetTypeBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Value", meta = (EditCondition = "bIsEnabled"))
	float Left = 0.0f;
};

UCLASS(meta = (DisplayName = "Margin - Right"))
class UCommonStyleSheetTypeMarginRight : public UCommonStyleSheetTypeBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Value", meta = (EditCondition = "bIsEnabled"))
	float Right = 0.0f;
};

UCLASS(meta = (DisplayName = "Margin - Top"))
class UCommonStyleSheetTypeMarginTop : public UCommonStyleSheetTypeBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Value", meta = (EditCondition = "bIsEnabled"))
	float Top = 0.0f;
};

UCLASS(meta = (DisplayName = "Margin - Bottom"))
class UCommonStyleSheetTypeMarginBottom : public UCommonStyleSheetTypeBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Value", meta = (EditCondition = "bIsEnabled"))
	float Bottom = 0.0f;
};
