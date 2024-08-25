// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "2D/TextureType.h"

class TEXTUREGRAPHENGINE_API ColorUtil
{
public:
	//Converting from Array to Functions to avoid static fiasco problem with CPP
	static const FLinearColor			DefaultColor(TextureType Type);
	static const FLinearColor			DefaultDiffuse();
	static const FLinearColor			DefaultSpecular();
	static const FLinearColor			DefaultAlbedo();
	static const FLinearColor			DefaultMetalness();
	static const FLinearColor			DefaultNormal();
	static const FLinearColor			DefaultDisplacement();
	static const FLinearColor			DefaultOpacity();
	static const FLinearColor			DefaultRoughness();
	static const FLinearColor			DefaultAO();
	static const FLinearColor			DefaultCurvature();
	static const FLinearColor			DefaultPreview();

	static float						GetHue(const FColor& Color);
	static float						GetSquaredDistance(FLinearColor Current, FLinearColor Match);
	static FString						GetColorName(FLinearColor Color);

	static bool							IsColorBlack(const FLinearColor& Color, bool IgnoreAlpha = true);
	static bool							IsColorWhite(const FLinearColor& Color, bool IgnoreAlpha = true);
	static bool							IsColorGray(const FLinearColor& Color, bool IgnoreAlpha = true);
	static bool							IsColorRed(const FLinearColor& Color, bool IgnoreAlpha = true);
	static bool							IsColorGreen(const FLinearColor& Color, bool IgnoreAlpha = true);
	static bool							IsColorBlue(const FLinearColor& Color, bool IgnoreAlpha = true);
	static bool							IsColorYellow(const FLinearColor& Color, bool IgnoreAlpha = true);
	static bool							IsColorMagenta(const FLinearColor& Color, bool IgnoreAlpha = true);
	static bool							IsColorNear(const FLinearColor& Color, const FLinearColor& Ref, bool IgnoreAlpha = true);
};
