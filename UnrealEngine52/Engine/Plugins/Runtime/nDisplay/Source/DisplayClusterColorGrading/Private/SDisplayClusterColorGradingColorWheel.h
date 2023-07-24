// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Customizations/MathStructCustomizations.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Colors/SColorGradingPicker.h"

class IPropertyHandle;
class SBox;

/** A widget which encapsulates a color picker and numeric sliders for each color component, hooked up to a color property handle */
class SDisplayClusterColorGradingColorWheel : public SCompoundWidget
{
public:
	enum class EColorDisplayMode
	{
		RGB,
		HSV
	};

	struct FColorPropertyMetadata
	{
		EColorGradingModes ColorGradingMode = EColorGradingModes::Invalid;
		TOptional<float> MinValue = TOptional<float>();
		TOptional<float> MaxValue = TOptional<float>();
		TOptional<float> SliderMinValue = TOptional<float>();
		TOptional<float> SliderMaxValue = TOptional<float>();
		float SliderExponent = 1.0f;
		float Delta = 0.0f;
		int32 LinearDeltaSensitivity = 0;
		int32 ShiftMouseMovePixelPerDelta = 1;
		bool bSupportDynamicSliderMaxValue = false;
		bool bSupportDynamicSliderMinValue = false;
	};

public:
	SLATE_BEGIN_ARGS(SDisplayClusterColorGradingColorWheel)
		: _Orientation(Orient_Vertical)
	{}
		SLATE_ARGUMENT(EOrientation, Orientation)
		SLATE_ATTRIBUTE(EColorDisplayMode, ColorDisplayMode)
		SLATE_ARGUMENT(TSharedPtr<SWidget>, HeaderContent)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Sets the property handle for the color property to edit with this color wheel */
	void SetColorPropertyHandle(TSharedPtr<IPropertyHandle> InColorPropertyHandle);

	/** Sets the widget to display as the header of the color wheel */
	void SetHeaderContent(const TSharedRef<SWidget>& HeaderContent);

	/** Sets the orientation of the color wheel, which determines if the numeric sliders are below or to the right of the color picker wheel */
	void SetOrientation(EOrientation NewOrientation);

private:
	TSharedRef<SWidget> CreateColorGradingPicker();
	TSharedRef<SWidget> CreateColorComponentSliders();

	FColorPropertyMetadata GetColorPropertyMetadata() const;

	bool IsPropertyEnabled() const;

	bool GetColor(FVector4& OutCurrentColor);
	void CommitColor(FVector4& NewValue, bool bShouldCommitValueChanges);
	void TransactColorValue();

	void BeginUsingColorPickerSlider();
	void EndUsingColorPickerSlider();
	void BeginUsingComponentSlider(uint32 ComponentIndex);
	void EndUsingComponentSlider(float NewValue, uint32 ComponentIndex);

	FText GetComponentLabelText(uint32 ComponentIndex) const;
	TOptional<float> GetComponentValue(uint32 ComponentIndex) const;
	void SetComponentValue(float NewValue, uint32 ComponentIndex);

	bool ComponentSupportsDynamicSliderValue(bool bDefaultValue, uint32 ComponentIndex) const;
	void UpdateComponentDynamicSliderMinValue(float NewValue, TWeakPtr<SWidget> SourceWidget, bool bIsOriginator, bool bUpdateOnlyIfLower);
	void UpdateComponentDynamicSliderMaxValue(float NewValue, TWeakPtr<SWidget> SourceWidget, bool bIsOriginator, bool bUpdateOnlyIfHigher);

	TOptional<float> GetComponentMaxValue(TOptional<float> DefaultValue, uint32 ComponentIndex) const;
	TOptional<float> GetComponentMinSliderValue(TOptional<float> DefaultValue, uint32 ComponentIndex) const;
	TOptional<float> GetComponentMaxSliderValue(TOptional<float> DefaultValue, uint32 ComponentIndex) const;
	float GetComponentSliderDeltaValue(float DefaultValue, uint32 ComponentIndex) const;
	FText GetComponentToolTipText(uint32 ComponentIndex) const;

	TArray<FLinearColor> GetGradientColor(uint32 ComponentIndex);

	FLinearColor GetGradientStartColor(const FVector4& ColorValue, bool bIsRGB, uint32 ComponentIndex) const;
	FLinearColor GetGradientEndColor(const FVector4& ColorValue, bool bIsRGB, uint32 ComponentIndex) const;
	FLinearColor GetGradientFillerColor(const FVector4& ColorValue, bool bIsRGB, uint32 ComponentIndex) const;
	EVisibility GetGradientVisibility() const;

	TOptional<float> GetMetadataMinValue() const;
	TOptional<float> GetMetadataMaxValue() const;
	TOptional<float> GetMetadataSliderMinValue() const;
	TOptional<float> GetMetadataSliderMaxValue() const;
	float GetMetadataDelta() const;
	int32 GetMetadataShiftMouseMovePixelPerDelta() const;
	bool GetMetadataSupportDynamicSliderMinValue() const;
	bool GetMetadataSupportDynamicSliderMaxValue() const;

private:
	TSharedPtr<SColorGradingPicker> ColorGradingPicker;
	TSharedPtr<SBox> HeaderBox;
	TSharedPtr<SBox> ColorWheelBox;
	TSharedPtr<SBox> ColorPickerBox;
	TSharedPtr<SBox> ColorSlidersBox;

	/** The property handle of the linear color property being edited */
	TWeakPtr<IPropertyHandle> ColorPropertyHandle;

	/** The metadata of the color property */
	TOptional<FColorPropertyMetadata> ColorPropertyMetadata;

	/** Attribute for the color mode type the color wheel is presenting the color components in */
	TAttribute<EColorDisplayMode> ColorDisplayMode;

	/** The orientation of the color wheel, which determines whether the color component sliders are next to or above the color picker */
	EOrientation Orientation = Orient_Vertical;

	/** Stored current min value of the color component numeric sliders */
	TOptional<float> ComponentSliderDynamicMinValue;

	/** Stored current max value of the color component numeric sliders */
	TOptional<float> ComponentSliderDynamicMaxValue;

	/** Indicates that the color picker slider is currently being used to change the color on the color picker */
	bool bIsUsingColorPickerSlider = false;

	/** Indicates that a component's numeric slider is currently being used to change the color */
	bool bIsUsingComponentSlider = false;
};