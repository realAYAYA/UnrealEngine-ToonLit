// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericWindow.h"
#include "GenericPlatform/GenericApplication.h"
#include "Templates/SharedPointer.h"


/**
* A platform specific implementation of FNativeWindow.
*/
class CORE_API FHoloLensWindow
	: public FGenericWindow, public TSharedFromThis<FHoloLensWindow>
{
public:

	/** Destructor. */
	virtual ~FHoloLensWindow();

	/** Create a new FHoloLensWindow. */
	static TSharedRef<FHoloLensWindow> Make();

	/** Init a FHoloLensWindow. */
	void Initialize(class FHoloLensApplication* const Application, const TSharedRef<FGenericWindowDefinition>& InDefinition);

	static FPlatformRect GetOSWindowBounds();
	
public:
	virtual void ReshapeWindow(int32 X, int32 Y, int32 Width, int32 Height) override;

	virtual void AdjustCachedSize(FVector2D& Size) const override;

	virtual void SetWindowMode(EWindowMode::Type InNewWindowMode) override;

	virtual EWindowMode::Type GetWindowMode() const override;

public:
	static int32 ConvertDipsToPixels(int32 Dips, float Dpi);
	static int32 ConvertPixelsToDips(int32 Pixels, float Dpi);

private:

	/** Protect the constructor; only TSharedRefs of this class can be made. */
	FHoloLensWindow();

	/** The mode that the window is in (windowed, fullscreen, windowedfullscreen ) */
	EWindowMode::Type WindowMode;
};
