// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PixelStreamingVideoInputRHI.h"
#include "Widgets/SWindow.h"
#include "RHI.h"
#include "Delegates/IDelegateInstance.h"
#include "Widgets/SWindow.h"
#include "GenericPlatform/GenericWindowDefinition.h"

/*
 * Use this if you want to send the full UE editor as video input.
 */
class PIXELSTREAMINGEDITOR_API FPixelStreamingVideoInputBackBufferComposited : public FPixelStreamingVideoInputRHI
{
public:
	static TSharedPtr<FPixelStreamingVideoInputBackBufferComposited> Create();
	virtual ~FPixelStreamingVideoInputBackBufferComposited();

	virtual FString ToString() override;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnFrameSizeChanged, TWeakPtr<FIntRect>);
	FOnFrameSizeChanged OnFrameSizeChanged;

private:
	FPixelStreamingVideoInputBackBufferComposited();
	void CompositeWindows();

	void OnBackBufferReady(SWindow& SlateWindow, const FTexture2DRHIRef& FrameBuffer);

	FDelegateHandle DelegateHandle;

	TArray<TSharedRef<SWindow>> TopLevelWindows;
	TMap<SWindow*, FTextureRHIRef> TopLevelWindowTextures;
	TMap<FString, FTextureRHIRef> StagingTextures;

	FCriticalSection TopLevelWindowsCriticalSection;
	TSharedPtr<FIntRect> SharedFrameRect;

private:
	// Util functions for 2D vectors
	template <class T>
	T VectorMax(const T A, const T B)
	{
		// Returns the component-wise maximum of two vectors
		return T(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y));
	}

	template <class T>
	T VectorMin(const T A, const T B)
	{
		// Returns the component-wise minimum of two vectors
		return T(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y));
	}
};