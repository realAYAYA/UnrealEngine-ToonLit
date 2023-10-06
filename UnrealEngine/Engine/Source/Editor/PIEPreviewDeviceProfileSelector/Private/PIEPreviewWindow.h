// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "Widgets/SWindow.h"
#include "PIEPreviewDevice.h"

class SPIEPreviewWindowTitleBar;

class PIEPREVIEWDEVICEPROFILESELECTOR_API SPIEPreviewWindow
	: public SWindow
{
public:
	/**
	* Default constructor. Use SNew(SPIEPreviewWindow) instead.
	*/
	SPIEPreviewWindow();

	~SPIEPreviewWindow();

	// call this function set the 'preview device' and set initial values for the position and resolution scaling factor
	void PrepareWindow(FVector2D WindowPosition, const float InitialScaleFactor, TSharedPtr<FPIEPreviewDevice> PreviewDevice);

	// call this function to perform cleanup
	void PrepareShutdown();

	// retrieves the display device
	TSharedPtr<class FPIEPreviewDevice> GetDevice();

	// call to retrieve the window current resolution scale factor
	float GetWindowScaleFactor() const;

	// call to set the window resolution scale factor
	void SetWindowScaleFactor(const float ScaleFactor, const bool bStore = true);

	// call to rotate the window emulating a rotation of the physical device
	void RotateWindow();

	// wrapper function that will query the same functionality on the device
	bool IsRotationAllowed() const;

	// call to enable/disable bezel visibility
	void FlipBezelVisibility();

	// call to determine whether or not the phone bezel is displayed 
	bool GetBezelVisibility() const;

	static int32 GetDefaultTitleBarSize();

	// call with a true argument to restrict the window size to the desktop size
	void SetClampWindowSize(const bool bClamp);
	bool IsClampingWindowSize() const;

	/** creates and returns the settings menu */
	TSharedRef<SWidget> BuildSettingsMenu();

	/** we need the game layer manager to control the DPI scaling behavior and this function can be called should be called when the manager is available */
	void SetGameLayerManagerWidget(TSharedPtr<class SGameLayerManager> GameLayerManager);

private:
	virtual EHorizontalAlignment GetTitleAlignment() override;
	virtual TSharedRef<SWidget> MakeWindowTitleBar(const TSharedRef<SWindow>& Window, const TSharedPtr<SWidget>& CenterContent, EHorizontalAlignment CenterContentAlignment) override;

	// sets the device displayed by this window
	void SetDevice(TSharedPtr<class FPIEPreviewDevice> InDevice);

	// returns whether or not the provided ScaleFactor is a 'scale to physical device size' factor
	bool IsScalingToDeviceSizeFactor(const float ScaleFactor) const;

	// returns the scaling value reserved for 'scale to physical device size' factor
	float GetScaleToDeviceSizeFactor() const;

	// utility function used to correct a given window position if it's out of the display area
	void ValidatePosition(FVector2D& WindowPos);

	// call to perform resolution scaling on the device
	void ScaleWindow(float ScaleFactor);

	// this will properly scale and rotate the bezel to match the orientation of the device
	void ComputeBezelOrientation();

	// function that adds a widget to display the device's bezel
	void CreatePIEPreviewBezelOverlay(UTexture2D* pBezelImage);

	/** helper function used to create a menu item formed from a description text and a checkbox */
	/** @ IsCheckedFunction - computes the state of the checkbox */
	/** @ ExecuteActionFunction - function to be called when this menu item is clicked */
	void CreateMenuEntry(class FMenuBuilder& MenuBuilder, FText&& TextEntry, TFunction<ECheckBoxState()>&& IsCheckedFunction, TFunction<void()>&& ExecuteActionFunction);

	/** helper function used to create a simple text widget to the provided menu */
	/** @ CreateTextFunction - function should return the menu text that we want to display */
	void CreateTextMenuEntry(class FMenuBuilder& MenuBuilder, TFunction<FText()>&& CreateTextFunction);

	// call to compute window size, position on screen and bezel orientation
	void UpdateWindow();

	// utility function that will compute a screen and DPI scale factor needed to scale the display window to the physical device size
	float ComputeScaleToDeviceSizeFactor() const;

	// utility function used to retrieve the DPI scaling factor based on window's position
	float ComputeDPIScaleFactor();

	// callback used when DPI value varies, useful when we need to constrain the window to the physical device size
	void OnWindowMoved(const TSharedRef<SWindow>& Window);

	// callback used when DPI value varies, useful when we need to constrain the window to the physical device size
	void OnDisplayDPIChanged(TSharedRef<SWindow> Window);

	// utility function used to control game layer manager's DPI scaling behavior
	// this should be called every time the window is rotated
	void UpdateGameLayerManagerDefaultViewport();

protected:
	// brush created to display the bezel
	FSlateBrush BezelBrush;

	// pointer to the actual bezel image
	TSharedPtr<SImage> BezelImage;

	// pointer to the device that this window will display
	TSharedPtr<class FPIEPreviewDevice> Device;

	float CachedScaleToDeviceFactor = 0.0f;
	float CachedDPIScaleFactor = 0.0f;

	/** when true the window size will be restricted to the desktop size  */
	bool bClampWindowSizeState = true;

	float WindowScalingFactor = 0.0f;

	TSharedPtr<SPIEPreviewWindowTitleBar> WindowTitleBar;

	FDelegateHandle HandleDPIChange;

	/** Pointer to the game layer manager widget. This is needed because we want to control the DPI scaling behavior */
	TSharedPtr<SGameLayerManager> GameLayerManagerWidget;
};

FORCEINLINE bool SPIEPreviewWindow::IsRotationAllowed() const
{
	if (!Device.IsValid())
	{
		return false;
	}

	return Device->IsRotationAllowed();
}

FORCEINLINE TSharedPtr<class FPIEPreviewDevice> SPIEPreviewWindow::GetDevice()
{
	return Device;
}

FORCEINLINE void SPIEPreviewWindow::SetClampWindowSize(const bool bClamp)
{
	bClampWindowSizeState = bClamp;
}

FORCEINLINE bool SPIEPreviewWindow::IsClampingWindowSize() const
{
	return bClampWindowSizeState;
}

FORCEINLINE bool SPIEPreviewWindow::IsScalingToDeviceSizeFactor(const float ScaleFactor) const
{
	float DeviceSizeFactor = GetScaleToDeviceSizeFactor();
	return FMath::IsNearlyEqual(ScaleFactor, DeviceSizeFactor);
}

FORCEINLINE float SPIEPreviewWindow::GetWindowScaleFactor() const
{
	return WindowScalingFactor;
}

FORCEINLINE float SPIEPreviewWindow::GetScaleToDeviceSizeFactor() const
{
	return 0.0f;
}

#endif