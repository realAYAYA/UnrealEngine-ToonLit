// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Render/Viewport/Containers/DisplayClusterViewport_OverscanSettings.h"

/**
* Runtime overscan settings
*/
struct FDisplayClusterViewport_OverscanRuntimeSettings
{
	/** Update overscan settings
	* 
	* @param InViewport            - owner viewport
	* @param InOutRuntimeSettings  - the overscan runtime settings.
	* @param InOutRenderTargetRect - Viewport rect, changeable during overscanning
	*/
	static void UpdateOverscanSettings(const FString& InViewportId, const FDisplayClusterViewport_OverscanSettings& InOverscanSettings, FDisplayClusterViewport_OverscanRuntimeSettings& InOutOverscanRuntimeSettings, FIntRect& InOutRenderTargetRect);

	/** Update projection angles by overscan
	* 
	* @param InOverscanRuntimeSettings - the overscan runtime settings.
	* @param InRenderTargetSize         - RenderTarget texture size
	* @param InOutLeft                 - the value of the left projection plane that you want to change
	* @param InOutRight                - the value of the right projection plane that you want to change
	* @param InOutTop                  - the value of the top projection plane that you want to change
	* @parma InOutBottom               - the value of the bottom projection plane that you want to change
	*/
	static bool UpdateProjectionAngles(const FDisplayClusterViewport_OverscanRuntimeSettings& InOverscanRuntimeSettings, const FIntPoint& InRenderTargetSize, double& InOutLeft, double& InOutRight, double& InOutTop, double& InOutBottom);

	/**
	* Overscan values in percent
	*/
	struct FOverscanPercent
	{
		double Left = 0;
		double Right = 0;
		double Top = 0;
		double Bottom = 0;
	};

	/**
	* Overscan values in pixels
	*/
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

	// Enable overscan
	bool bIsEnabled = false;

	// Overscan sides in percent
	FOverscanPercent OverscanPercent;

	// Overscan sides in pixels
	FOverscanPixels  OverscanPixels;
};
