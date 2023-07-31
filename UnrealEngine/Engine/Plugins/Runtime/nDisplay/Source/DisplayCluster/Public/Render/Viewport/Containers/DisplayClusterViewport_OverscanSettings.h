// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FDisplayClusterViewport_OverscanSettings
{
	struct FOverscanPercent
	{
		float Left = 0;
		float Right = 0;
		float Top = 0;
		float Bottom = 0;
	};

	struct FOverscanPixels
	{
		inline FIntRect GetInnerRect(const FIntRect& InRect) const
		{
			const FIntPoint InnerSize = InRect.Size() - Size();
			const FIntPoint InnerPos = FIntPoint(Left, Top);

			return FIntRect(InnerPos, InnerPos + InnerSize);
		}

		inline FIntPoint Size() const
		{
			return FIntPoint(Left + Right, Top + Bottom);
		}

		int32 Left = 0;
		int32 Right = 0;
		int32 Top = 0;
		int32 Bottom = 0;
	};

	bool bIsEnabled = false;

	// Overscan sides in percent
	FOverscanPercent OverscanPercent;

	// Overscan sides in pixels
	FOverscanPixels  OverscanPixels;

	static float ClampPercent(float InValue)
	{
		static const float MaxCustomFrustumValue = 5.f;

		return FMath::Clamp(InValue, -MaxCustomFrustumValue, MaxCustomFrustumValue);
	}
};

