// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Math/Color.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

struct FDrawContext;
struct FSlateBrush;

////////////////////////////////////////////////////////////////////////////////////////////////////

class TRACEINSIGHTS_API FTooltipDrawState
{
public:
	static constexpr float BorderX = 6.0f;
	static constexpr float BorderY = 3.0f;
	static constexpr float MinWidth = 128.0f;
	static constexpr float MinHeight = 0.0f;

	static constexpr float DefaultTitleHeight = 14.0f;
	static constexpr float DefaultLineHeight = 14.0f;
	static constexpr float NameValueDX = 2.0f;

	static FLinearColor DefaultTitleColor;
	static FLinearColor DefaultNameColor;
	static FLinearColor DefaultValueColor;

private:
	enum class FDrawTextType
	{
		Misc,
		Title,
		Name,  // X is computed at Draw (right aligned at ValueOffsetX)
		Value, // X is computed at Draw (left aligned at ValueOffsetX + NameValueDX)
	};

	struct FDrawTextInfo
	{
		float X; // relative X position of text inside tooltip
		float Y; // relative Y position of text inside tooltip
		FVector2D TextSize;
		FString Text;
		FLinearColor Color;
		FDrawTextType Type;
	};

public:
	FTooltipDrawState();
	~FTooltipDrawState();

	void Reset();

	void ResetContent();
	void AddTitle(FStringView Title);
	void AddTitle(FStringView Title, const FLinearColor& Color);
	void AddNameValueTextLine(FStringView Name, FStringView Value);
	void AddTextLine(FStringView Text, const FLinearColor& Color);
	void AddTextLine(const float X, const float Y, FStringView Text, const FLinearColor& Color);
	void UpdateLayout(); // updates ValueOffsetX and DesiredSize

	const FLinearColor& GetBackgroundColor() const { return BackgroundColor; }
	void SetBackgroundColor(const FLinearColor& InBackgroundColor) { BackgroundColor = InBackgroundColor; }

	float GetOpacity() const { return Opacity; }
	void SetOpacity(const float InOpacity) { Opacity = InOpacity; }

	float GetDesiredOpacity() const { return DesiredOpacity; }
	void SetDesiredOpacity(const float InDesiredOpacity) { DesiredOpacity = InDesiredOpacity; }

	const FVector2D& GetSize() const { return Size; }
	const FVector2D& GetDesiredSize() const { return DesiredSize; }

	void Update(); // updates current Opacity and Size

	const FVector2D& GetPosition() const { return Position; }
	void SetPosition(const FVector2D& MousePosition, const float MinX, const float MaxX, const float MinY, const float MaxY);
	void SetPosition(const float PosX, const float PosY) { Position.X = PosX; Position.Y = PosY; }

	void Draw(const FDrawContext& DrawContext) const;

	void SetFontScale(float InFontScale) { FontScale = InFontScale; }
	float GetFontScale() const { return FontScale; }

	void SetImage(TSharedPtr<FSlateBrush> InImageBrush) { ImageBrush = InImageBrush; }

private:
	const FSlateBrush* WhiteBrush;
	const FSlateFontInfo Font;

	FLinearColor BackgroundColor;

	FVector2D Size;
	FVector2D DesiredSize;

	FVector2D Position;

	float ValueOffsetX;
	float NewLineY;

	float Opacity;
	float DesiredOpacity;

	float FontScale;

	TArray<FDrawTextInfo> Texts;
	TSharedPtr<FSlateBrush> ImageBrush;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
