// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDisplayClusterColorGradingColorWheel.h"

#include "Customizations/MathStructCustomizations.h"
#include "DetailLayoutBuilder.h"
#include "Editor.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Colors/SComplexGradient.h"
#include "Widgets/Layout/SBox.h"

#define LOCTEXT_NAMESPACE "DisplayClusterColorGrading"

void SDisplayClusterColorGradingColorWheel::Construct(const FArguments& InArgs)
{
	Orientation = InArgs._Orientation;
	ColorDisplayMode = InArgs._ColorDisplayMode;

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Fill)
		[
			SAssignNew(HeaderBox, SBox)
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SAssignNew(ColorWheelBox, SBox)
		]
	];

	ColorPickerBox = SNew(SBox);
	ColorSlidersBox = SNew(SBox);

	SetOrientation(Orientation);

	if (InArgs._HeaderContent.IsValid())
	{
		HeaderBox->SetContent(InArgs._HeaderContent.ToSharedRef());
	}
}

void SDisplayClusterColorGradingColorWheel::SetColorPropertyHandle(TSharedPtr<IPropertyHandle> InColorPropertyHandle)
{
	ColorGradingPicker.Reset();
	ColorPropertyMetadata.Reset();
	ComponentSliderDynamicMinValue.Reset();
	ComponentSliderDynamicMaxValue.Reset();

	ColorPropertyHandle = InColorPropertyHandle;

	if (ColorPropertyHandle.IsValid() && ColorPropertyHandle.Pin()->IsValidHandle())
	{
		ColorPropertyMetadata = GetColorPropertyMetadata();
		ComponentSliderDynamicMinValue = ColorPropertyMetadata->MinValue;
		ComponentSliderDynamicMaxValue = ColorPropertyMetadata->MaxValue;
	}

	// Since many of the color picker slate properties are not attributes, we need to recreate the widget every time the property handle changes
	// to ensure the picker is configured correctly for the color property
	if (ColorPickerBox.IsValid())
	{
		ColorPickerBox->SetContent(CreateColorGradingPicker());
	}

	if (ColorSlidersBox.IsValid())
	{
		ColorSlidersBox->SetContent(CreateColorComponentSliders());
	}
}

void SDisplayClusterColorGradingColorWheel::SetHeaderContent(const TSharedRef<SWidget>& HeaderContent)
{
	if (HeaderBox.IsValid())
	{
		HeaderBox->SetContent(HeaderContent);
	}
}

void SDisplayClusterColorGradingColorWheel::SetOrientation(EOrientation NewOrientation)
{
	Orientation = NewOrientation;

	if (ColorWheelBox.IsValid())
	{
		TSharedPtr<SWidget> OrientedBox;

		if (Orientation == EOrientation::Orient_Vertical)
		{
			OrientedBox = SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				ColorPickerBox.ToSharedRef()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				ColorSlidersBox.ToSharedRef()
			];
		}
		else
		{
			OrientedBox = SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				ColorPickerBox.ToSharedRef()
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				ColorSlidersBox.ToSharedRef()
			];
		}

		ColorWheelBox->SetContent(OrientedBox.ToSharedRef());
		ColorWheelBox->SetHAlign(Orientation == EOrientation::Orient_Vertical ? HAlign_Center : HAlign_Fill);
	}
}

TSharedRef<SWidget> SDisplayClusterColorGradingColorWheel::SDisplayClusterColorGradingColorWheel::CreateColorGradingPicker()
{
	if (ColorPropertyHandle.IsValid() && ColorPropertyHandle.Pin()->IsValidHandle())
	{
		return SNew(SColorGradingPicker)
			.ValueMin(GetMetadataMinValue())
			.ValueMax(GetMetadataMaxValue())
			.SliderValueMin(GetMetadataSliderMinValue())
			.SliderValueMax(GetMetadataSliderMaxValue())
			.MainDelta(GetMetadataDelta())
			.SupportDynamicSliderMinValue(GetMetadataSupportDynamicSliderMinValue())
			.SupportDynamicSliderMaxValue(GetMetadataSupportDynamicSliderMaxValue())
			.MainShiftMouseMovePixelPerDelta(GetMetadataShiftMouseMovePixelPerDelta())
			.ColorGradingModes(ColorPropertyMetadata->ColorGradingMode)
			.OnColorCommitted(this, &SDisplayClusterColorGradingColorWheel::CommitColor)
			.OnQueryCurrentColor(this, &SDisplayClusterColorGradingColorWheel::GetColor)
			.AllowSpin(ColorPropertyHandle.Pin()->GetNumOuterObjects() == 1) // TODO: May want to find a way to support multiple objects
			.OnBeginSliderMovement(this, &SDisplayClusterColorGradingColorWheel::BeginUsingColorPickerSlider)
			.OnEndSliderMovement(this, &SDisplayClusterColorGradingColorWheel::EndUsingColorPickerSlider)
			.OnBeginMouseCapture(this, &SDisplayClusterColorGradingColorWheel::BeginUsingColorPickerSlider)
			.OnEndMouseCapture(this, &SDisplayClusterColorGradingColorWheel::EndUsingColorPickerSlider)
			.IsEnabled(this, &SDisplayClusterColorGradingColorWheel::IsPropertyEnabled);
	}

	return SNullWidget::NullWidget;
}

TSharedRef<SWidget> SDisplayClusterColorGradingColorWheel::CreateColorComponentSliders()
{
	if (ColorPropertyHandle.IsValid() && ColorPropertyHandle.Pin()->IsValidHandle())
	{
		TSharedRef<SVerticalBox> ColorSlidersVerticalBox = SNew(SVerticalBox);

		uint32 NumComponents = 4;
		for (uint32 ComponentIndex = 0; ComponentIndex < NumComponents; ++ComponentIndex)
		{
			TAttribute<FText> TextGetter = TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SDisplayClusterColorGradingColorWheel::GetComponentLabelText, ComponentIndex));
			TSharedRef<SWidget> LabelWidget = SNumericEntryBox<float>::BuildLabel(TextGetter, FLinearColor::White, FLinearColor(0.2f, 0.2f, 0.2f));

			TSharedRef<SNumericEntryBox<float>> NumericEntryBox = SNew(SNumericEntryBox<float>)
				.SpinBoxStyle(&FCoreStyle::Get().GetWidgetStyle<FSpinBoxStyle>("NumericEntrySpinBox_Dark"))
				.EditableTextBoxStyle(&FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("DarkEditableTextBox"))
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.UndeterminedString(NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values"))
				.Value(this, &SDisplayClusterColorGradingColorWheel::GetComponentValue, ComponentIndex)
				.OnValueChanged(this, &SDisplayClusterColorGradingColorWheel::SetComponentValue, ComponentIndex)
				.OnBeginSliderMovement(this, &SDisplayClusterColorGradingColorWheel::BeginUsingComponentSlider, ComponentIndex)
				.OnEndSliderMovement(this, &SDisplayClusterColorGradingColorWheel::EndUsingComponentSlider, ComponentIndex)
				.AllowSpin(ColorPropertyHandle.Pin()->GetNumOuterObjects() == 1)
				.ShiftMouseMovePixelPerDelta(ColorPropertyMetadata->ShiftMouseMovePixelPerDelta)
				.SupportDynamicSliderMinValue(this, &SDisplayClusterColorGradingColorWheel::ComponentSupportsDynamicSliderValue, ColorPropertyMetadata->bSupportDynamicSliderMinValue, ComponentIndex)
				.SupportDynamicSliderMaxValue(this, &SDisplayClusterColorGradingColorWheel::ComponentSupportsDynamicSliderValue, ColorPropertyMetadata->bSupportDynamicSliderMaxValue, ComponentIndex)
				.OnDynamicSliderMinValueChanged(this, &SDisplayClusterColorGradingColorWheel::UpdateComponentDynamicSliderMinValue)
				.OnDynamicSliderMaxValueChanged(this, &SDisplayClusterColorGradingColorWheel::UpdateComponentDynamicSliderMaxValue)
				.MinValue(ColorPropertyMetadata->MinValue)
				.MaxValue(this, &SDisplayClusterColorGradingColorWheel::GetComponentMaxValue, ColorPropertyMetadata->MaxValue, ComponentIndex)
				.MinSliderValue(this, &SDisplayClusterColorGradingColorWheel::GetComponentMinSliderValue, ColorPropertyMetadata->SliderMinValue, ComponentIndex)
				.MaxSliderValue(this, &SDisplayClusterColorGradingColorWheel::GetComponentMaxSliderValue, ColorPropertyMetadata->SliderMaxValue, ComponentIndex)
				.SliderExponent(ColorPropertyMetadata->SliderExponent)
				.SliderExponentNeutralValue(ColorPropertyMetadata->SliderMinValue.GetValue() + (ColorPropertyMetadata->SliderMaxValue.GetValue() - ColorPropertyMetadata->SliderMinValue.GetValue()) / 2.0f)
				.Delta(this, &SDisplayClusterColorGradingColorWheel::GetComponentSliderDeltaValue, ColorPropertyMetadata->Delta, ComponentIndex)
				.ToolTipText(this, &SDisplayClusterColorGradingColorWheel::GetComponentToolTipText, ComponentIndex)
				.LabelPadding(FMargin(0))
				.IsEnabled(this, &SDisplayClusterColorGradingColorWheel::IsPropertyEnabled)
				.Label()
				[
					LabelWidget
				];

			ColorSlidersVerticalBox->AddSlot()
				.Padding(FMargin(0.0f, 2.0f, 3.0f, 0.0f))
				.AutoHeight()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				[
					NumericEntryBox
				];

			ColorSlidersVerticalBox->AddSlot()
				.Padding(FMargin(15.0f, 0.0f, 3.0f, 2.0f))
				.AutoHeight()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Bottom)
				[
					SNew(SBox)
					.HeightOverride(6.0f)
					[
						SNew(SComplexGradient)
						.GradientColors(TAttribute<TArray<FLinearColor>>::Create(TAttribute<TArray<FLinearColor>>::FGetter::CreateSP(this, &SDisplayClusterColorGradingColorWheel::GetGradientColor, ComponentIndex)))
						.Visibility(this, &SDisplayClusterColorGradingColorWheel::GetGradientVisibility)
					]
				];
		}

		return ColorSlidersVerticalBox;
	}

	return SNullWidget::NullWidget;
}

SDisplayClusterColorGradingColorWheel::FColorPropertyMetadata SDisplayClusterColorGradingColorWheel::GetColorPropertyMetadata() const
{
	FColorPropertyMetadata PropertyMetadata;

	if (ColorPropertyHandle.IsValid() && ColorPropertyHandle.Pin()->IsValidHandle())
	{
		FProperty* Property = ColorPropertyHandle.Pin()->GetProperty();
		const FString ColorGradingModeString = Property->GetMetaData(TEXT("ColorGradingMode")).ToLower();

		if (!ColorGradingModeString.IsEmpty())
		{
			if (ColorGradingModeString.Compare(TEXT("saturation")) == 0)
			{
				PropertyMetadata.ColorGradingMode = EColorGradingModes::Saturation;
			}
			else if (ColorGradingModeString.Compare(TEXT("contrast")) == 0)
			{
				PropertyMetadata.ColorGradingMode = EColorGradingModes::Contrast;
			}
			else if (ColorGradingModeString.Compare(TEXT("gamma")) == 0)
			{
				PropertyMetadata.ColorGradingMode = EColorGradingModes::Gamma;
			}
			else if (ColorGradingModeString.Compare(TEXT("gain")) == 0)
			{
				PropertyMetadata.ColorGradingMode = EColorGradingModes::Gain;
			}
			else if (ColorGradingModeString.Compare(TEXT("offset")) == 0)
			{
				PropertyMetadata.ColorGradingMode = EColorGradingModes::Offset;
			}
		}

		const FString& MetaUIMinString = Property->GetMetaData(TEXT("UIMin"));
		const FString& MetaUIMaxString = Property->GetMetaData(TEXT("UIMax"));
		const FString& SliderExponentString = Property->GetMetaData(TEXT("SliderExponent"));
		const FString& DeltaString = Property->GetMetaData(TEXT("Delta"));
		const FString& LinearDeltaSensitivityString = Property->GetMetaData(TEXT("LinearDeltaSensitivity"));
		const FString& ShiftMouseMovePixelPerDeltaString = Property->GetMetaData(TEXT("ShiftMouseMovePixelPerDelta"));
		const FString& SupportDynamicSliderMaxValueString = Property->GetMetaData(TEXT("SupportDynamicSliderMaxValue"));
		const FString& SupportDynamicSliderMinValueString = Property->GetMetaData(TEXT("SupportDynamicSliderMinValue"));
		const FString& ClampMinString = Property->GetMetaData(TEXT("ClampMin"));
		const FString& ClampMaxString = Property->GetMetaData(TEXT("ClampMax"));

		const FString& UIMinString = MetaUIMinString.Len() ? MetaUIMinString : ClampMinString;
		const FString& UIMaxString = MetaUIMaxString.Len() ? MetaUIMaxString : ClampMaxString;

		float ClampMin = TNumericLimits<float>::Lowest();
		float ClampMax = TNumericLimits<float>::Max();

		if (!ClampMinString.IsEmpty())
		{
			TTypeFromString<float>::FromString(ClampMin, *ClampMinString);
		}

		if (!ClampMaxString.IsEmpty())
		{
			TTypeFromString<float>::FromString(ClampMax, *ClampMaxString);
		}

		float UIMin = TNumericLimits<float>::Lowest();
		float UIMax = TNumericLimits<float>::Max();
		TTypeFromString<float>::FromString(UIMin, *UIMinString);
		TTypeFromString<float>::FromString(UIMax, *UIMaxString);

		const float ActualUIMin = FMath::Max(UIMin, ClampMin);
		const float ActualUIMax = FMath::Min(UIMax, ClampMax);

		PropertyMetadata.MinValue = ClampMinString.Len() ? ClampMin : TOptional<float>();
		PropertyMetadata.MaxValue = ClampMaxString.Len() ? ClampMax : TOptional<float>();
		PropertyMetadata.SliderMinValue = (UIMinString.Len()) ? ActualUIMin : TOptional<float>();
		PropertyMetadata.SliderMaxValue = (UIMaxString.Len()) ? ActualUIMax : TOptional<float>();

		if (SliderExponentString.Len())
		{
			TTypeFromString<float>::FromString(PropertyMetadata.SliderExponent, *SliderExponentString);
		}

		if (DeltaString.Len())
		{
			TTypeFromString<float>::FromString(PropertyMetadata.Delta, *DeltaString);
		}

		if (LinearDeltaSensitivityString.Len())
		{
			TTypeFromString<int32>::FromString(PropertyMetadata.LinearDeltaSensitivity, *LinearDeltaSensitivityString);
			PropertyMetadata.Delta = (PropertyMetadata.Delta == 0.0f) ? 1.0f : PropertyMetadata.Delta;
		}

		if (ShiftMouseMovePixelPerDeltaString.Len())
		{
			TTypeFromString<int32>::FromString(PropertyMetadata.ShiftMouseMovePixelPerDelta, *ShiftMouseMovePixelPerDeltaString);
			PropertyMetadata.ShiftMouseMovePixelPerDelta = FMath::Max(PropertyMetadata.ShiftMouseMovePixelPerDelta, 1);
		}

		PropertyMetadata.bSupportDynamicSliderMaxValue = SupportDynamicSliderMaxValueString.Len() > 0 && SupportDynamicSliderMaxValueString.ToBool();
		PropertyMetadata.bSupportDynamicSliderMinValue = SupportDynamicSliderMinValueString.Len() > 0 && SupportDynamicSliderMinValueString.ToBool();
	}

	return PropertyMetadata;
}

bool SDisplayClusterColorGradingColorWheel::IsPropertyEnabled() const
{
	if (ColorPropertyHandle.IsValid() && ColorPropertyHandle.Pin()->IsValidHandle())
	{
		return ColorPropertyHandle.Pin()->IsEditable();
	}

	return false;
}

bool SDisplayClusterColorGradingColorWheel::GetColor(FVector4& OutCurrentColor)
{
	if (ColorPropertyHandle.IsValid())
	{
		return ColorPropertyHandle.Pin()->GetValue(OutCurrentColor) == FPropertyAccess::Success;
	}

	return false;
}

void SDisplayClusterColorGradingColorWheel::CommitColor(FVector4& NewValue, bool bShouldCommitValueChanges)
{
	FScopedTransaction Transaction(LOCTEXT("ColorWheel_TransactionName", "Color Grading Main Value"), bShouldCommitValueChanges);

	if (ColorPropertyHandle.IsValid())
	{
		// Always perform a purely interactive change. We do this because it won't invoke reconstruction, which may cause that only the first 
		// element gets updated due to its change causing a component reconstruction and the remaining vector element property handles updating 
		// the trashed component.
		ColorPropertyHandle.Pin()->SetValue(NewValue, EPropertyValueSetFlags::InteractiveChange | EPropertyValueSetFlags::NotTransactable);

		// If not purely interactive, set the value with default flags.
		if (bShouldCommitValueChanges || !bIsUsingColorPickerSlider)
		{
			ColorPropertyHandle.Pin()->SetValue(NewValue, EPropertyValueSetFlags::DefaultFlags);
		}

		TransactColorValue();
	}
}

void SDisplayClusterColorGradingColorWheel::TransactColorValue()
{
	if (ColorPropertyHandle.IsValid())
	{
		TArray<UObject*> OuterObjects;
		ColorPropertyHandle.Pin()->GetOuterObjects(OuterObjects);
		for (UObject* Object : OuterObjects)
		{
			if (!Object->HasAnyFlags(RF_Transactional))
			{
				Object->SetFlags(RF_Transactional);
			}

			SaveToTransactionBuffer(Object, false);
			SnapshotTransactionBuffer(Object);
		}
	}
}

void SDisplayClusterColorGradingColorWheel::BeginUsingColorPickerSlider()
{
	bIsUsingColorPickerSlider = true;
	GEditor->BeginTransaction(LOCTEXT("ColorWheel_TransactionName", "Color Grading Main Value"));
}

void SDisplayClusterColorGradingColorWheel::EndUsingColorPickerSlider()
{
	bIsUsingColorPickerSlider = false;
	GEditor->EndTransaction();
}

void SDisplayClusterColorGradingColorWheel::BeginUsingComponentSlider(uint32 ComponentIndex)
{
	bIsUsingComponentSlider = true;
	GEditor->BeginTransaction(LOCTEXT("ColorWheel_TransactionName", "Color Grading Main Value"));
}

void SDisplayClusterColorGradingColorWheel::EndUsingComponentSlider(float NewValue, uint32 ComponentIndex)
{
	bIsUsingComponentSlider = false;
	SetComponentValue(NewValue, ComponentIndex);
	GEditor->EndTransaction();
}

FText SDisplayClusterColorGradingColorWheel::GetComponentLabelText(uint32 ComponentIndex) const
{
	static const FText RGBLabelText[] = {
		LOCTEXT("ColorWheel_RedComponentLabel", "R"),
		LOCTEXT("ColorWheel_GreenComponentLabel", "G"),
		LOCTEXT("ColorWheel_BlueComponentLabel", "B")
	};

	static const FText HSVLabelText[] = {
		LOCTEXT("ColorWheel_HueComponentLabel", "H"),
		LOCTEXT("ColorWheel_SatComponentLabel", "S"),
		LOCTEXT("ColorWheel_ValComponentLabel", "V")
	};

	if (ComponentIndex >= 0 && ComponentIndex < 3)
	{ 
		if (ColorDisplayMode.Get(EColorDisplayMode::RGB) == EColorDisplayMode::RGB)
		{
			return RGBLabelText[ComponentIndex];
		}
		else
		{
			return HSVLabelText[ComponentIndex];
		}
	}
	else if (ComponentIndex == 3)
	{
		return LOCTEXT("ColorWheel_LuminanceComponentLabel", "Y");
	}

	return FText::GetEmpty();
}

TOptional<float> SDisplayClusterColorGradingColorWheel::GetComponentValue(uint32 ComponentIndex) const
{
	if (ColorPropertyHandle.IsValid())
	{
		FVector4 ColorValue;
		if (ColorPropertyHandle.Pin()->GetValue(ColorValue) == FPropertyAccess::Success)
		{
			float Value = 0.0f;

			if (ColorDisplayMode.Get(EColorDisplayMode::RGB) == EColorDisplayMode::RGB)
			{
				Value = ColorValue[ComponentIndex];
			}
			else
			{
				FLinearColor HSVColor = FLinearColor(ColorValue.X, ColorValue.Y, ColorValue.Z, ColorValue.W).LinearRGBToHSV();
				Value = HSVColor.Component(ComponentIndex);
			}

			return Value;
		}
	}

	return TOptional<float>();
}

void SDisplayClusterColorGradingColorWheel::SetComponentValue(float NewValue, uint32 ComponentIndex)
{
	if (ColorPropertyHandle.IsValid())
	{
		FVector4 CurrentColorValue;
		if (ColorPropertyHandle.Pin()->GetValue(CurrentColorValue) == FPropertyAccess::Success)
		{
			FVector4 NewColorValue = CurrentColorValue;

			if (ColorDisplayMode.Get(EColorDisplayMode::RGB) == EColorDisplayMode::RGB)
			{
				NewColorValue[ComponentIndex] = NewValue;
			}
			else
			{
				FLinearColor HSVColor = FLinearColor(CurrentColorValue.X, CurrentColorValue.Y, CurrentColorValue.Z, CurrentColorValue.W).LinearRGBToHSV();
				HSVColor.Component(ComponentIndex) = NewValue;
				NewColorValue = (FVector4)HSVColor.HSVToLinearRGB();
			}

			ColorPropertyHandle.Pin()->SetValue(NewColorValue, bIsUsingComponentSlider ? EPropertyValueSetFlags::InteractiveChange : EPropertyValueSetFlags::DefaultFlags);
			TransactColorValue();
		}
	}
}

bool SDisplayClusterColorGradingColorWheel::ComponentSupportsDynamicSliderValue(bool bDefaultValue, uint32 ComponentIndex) const
{
	if (bDefaultValue)
	{
		if (ColorDisplayMode.Get(EColorDisplayMode::RGB) != EColorDisplayMode::RGB)
		{
			return ComponentIndex >= 2;
		}
	}

	return bDefaultValue;
}

void SDisplayClusterColorGradingColorWheel::UpdateComponentDynamicSliderMinValue(float NewValue, TWeakPtr<SWidget> SourceWidget, bool bIsOriginator, bool bUpdateOnlyIfLower)
{
	if (!ComponentSliderDynamicMinValue.IsSet() || (NewValue < ComponentSliderDynamicMinValue.GetValue() && bUpdateOnlyIfLower) || !bUpdateOnlyIfLower)
	{
		ComponentSliderDynamicMinValue = NewValue;
	}
}

void SDisplayClusterColorGradingColorWheel::UpdateComponentDynamicSliderMaxValue(float NewValue, TWeakPtr<SWidget> SourceWidget, bool bIsOriginator, bool bUpdateOnlyIfHigher)
{
	if (!ComponentSliderDynamicMaxValue.IsSet() || (NewValue > ComponentSliderDynamicMaxValue.GetValue() && bUpdateOnlyIfHigher) || !bUpdateOnlyIfHigher)
	{
		ComponentSliderDynamicMaxValue = NewValue;
	}
}

TOptional<float> SDisplayClusterColorGradingColorWheel::GetComponentMaxValue(TOptional<float> DefaultValue, uint32 ComponentIndex) const
{
	if (ColorDisplayMode.Get(EColorDisplayMode::RGB) != EColorDisplayMode::RGB)
	{
		if (ComponentIndex == 0)
		{
			return 359.0f;
		}
		else if (ComponentIndex == 1)
		{
			return 1.0f;
		}
	}

	return DefaultValue;
}

TOptional<float> SDisplayClusterColorGradingColorWheel::GetComponentMinSliderValue(TOptional<float> DefaultValue, uint32 ComponentIndex) const
{
	if (ColorDisplayMode.Get(EColorDisplayMode::RGB) != EColorDisplayMode::RGB)
	{
		return 0.0f;
	}

	return ComponentSliderDynamicMinValue.IsSet() ? ComponentSliderDynamicMinValue.GetValue() : DefaultValue;
}

TOptional<float> SDisplayClusterColorGradingColorWheel::GetComponentMaxSliderValue(TOptional<float> DefaultValue, uint32 ComponentIndex) const
{
	if (ColorDisplayMode.Get(EColorDisplayMode::RGB) != EColorDisplayMode::RGB)
	{
		if (ComponentIndex == 0)
		{
			return 359.0f;
		}
		else if (ComponentIndex == 1)
		{
			return 1.0f;
		}
	}

	return ComponentSliderDynamicMaxValue.IsSet() ? ComponentSliderDynamicMaxValue : DefaultValue;
}

float SDisplayClusterColorGradingColorWheel::GetComponentSliderDeltaValue(float DefaultValue, uint32 ComponentIndex) const
{
	if (ComponentIndex == 0 && ColorDisplayMode.Get(EColorDisplayMode::RGB) != EColorDisplayMode::RGB)
	{
		return 1.0f;
	}

	return DefaultValue;
}

FText SDisplayClusterColorGradingColorWheel::GetComponentToolTipText(uint32 ComponentIndex) const
{
	static const FText RGBToolTipText[] =
	{
		LOCTEXT("ColorWheel_RedComponentToolTip", "Red"),
		LOCTEXT("ColorWheel_GreenComponentToolTip", "Green"),
		LOCTEXT("ColorWheel_BlueComponentToolTip", "Blue")
	};

	static const FText HSVToolTipText[] =
	{
		LOCTEXT("ColorWheel_HueComponentToolTip", "Hue"),
		LOCTEXT("ColorWheel_SaturationComponentToolTip", "Saturation"),
		LOCTEXT("ColorWheel_ValueComponentToolTip", "Value")
	};

	if (ComponentIndex >= 0 && ComponentIndex < 3)
	{
		if (ColorDisplayMode.Get(EColorDisplayMode::RGB) != EColorDisplayMode::RGB)
		{
			return HSVToolTipText[ComponentIndex];
		}
		else
		{
			return RGBToolTipText[ComponentIndex];
		}
	}
	else if (ComponentIndex == 3)
	{
		return LOCTEXT("ColorWheel_LuminanceComponentToolTip", "Luminance");
	}

	return FText::GetEmpty();
}

TArray<FLinearColor> SDisplayClusterColorGradingColorWheel::GetGradientColor(uint32 ComponentIndex)
{
	TArray<FLinearColor> GradientColors;

	if (ColorPropertyHandle.IsValid())
	{
		const bool bIsRGB = ColorDisplayMode.Get(EColorDisplayMode::RGB) == EColorDisplayMode::RGB;
		FVector4 ColorValue;
			
		if (ColorPropertyHandle.Pin()->GetValue(ColorValue) != FPropertyAccess::Success)
		{
			ColorValue = FVector4(0.0, 0.0, 0.0, 0.0);
		}

		if (bIsRGB || ComponentIndex > 0)
		{
			GradientColors.Add(GetGradientStartColor(ColorValue, bIsRGB, ComponentIndex));
			GradientColors.Add(GetGradientEndColor(ColorValue, bIsRGB, ComponentIndex));
			GradientColors.Add(GetGradientFillerColor(ColorValue, bIsRGB, ComponentIndex));
		}
		else // HSV Hue handling
		{
			for (int32 i = 0; i < 7; ++i)
			{
				GradientColors.Add(FLinearColor((i % 6) * 60.f, 1.f, 1.f).HSVToLinearRGB());
			}
		}
	}

	return GradientColors;
}

FLinearColor SDisplayClusterColorGradingColorWheel::GetGradientStartColor(const FVector4& ColorValue, bool bIsRGB, uint32 ComponentIndex) const
{
	if (bIsRGB)
	{
		switch (ComponentIndex)
		{
		case 0:		return FLinearColor(0.0f, ColorValue.Y, ColorValue.Z, 1.0f);
		case 1:		return FLinearColor(ColorValue.X, 0.0f, ColorValue.Z, 1.0f);
		case 2:		return FLinearColor(ColorValue.X, ColorValue.Y, 0.0f, 1.0f);
		case 3:		return FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);
		default:	return FLinearColor(ForceInit);
		}
	}
	else
	{
		FLinearColor HSVColor = FLinearColor(ColorValue.X, ColorValue.Y, ColorValue.Z, ColorValue.W).LinearRGBToHSV();
		switch (ComponentIndex)
		{
		case 0:		return FLinearColor(HSVColor.R, HSVColor.G, HSVColor.B, 1.0f);
		case 1:		return FLinearColor(HSVColor.R, 0.0f, HSVColor.B, 1.0f).HSVToLinearRGB();
		case 2:		return FLinearColor(HSVColor.R, HSVColor.G, 0.0f, 1.0f).HSVToLinearRGB();
		case 3:		return FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);
		default:	return FLinearColor(ForceInit);
		}
	}
}

FLinearColor SDisplayClusterColorGradingColorWheel::GetGradientEndColor(const FVector4& ColorValue, bool bIsRGB, uint32 ComponentIndex) const
{
	if (bIsRGB)
	{
		switch (ComponentIndex)
		{
		case 0:		return FLinearColor(1.0f, ColorValue.Y, ColorValue.Z, 1.0f);
		case 1:		return FLinearColor(ColorValue.X, 1.0f, ColorValue.Z, 1.0f);
		case 2:		return FLinearColor(ColorValue.X, ColorValue.Y, 1.0f, 1.0f);
		case 3:		return FLinearColor(ColorValue.X, ColorValue.Y, ColorValue.Z, 1.0f);
		default:	return FLinearColor(ForceInit);
		}
	}
	else
	{
		FLinearColor HSVColor = FLinearColor(ColorValue.X, ColorValue.Y, ColorValue.Z, ColorValue.W).LinearRGBToHSV();
		switch (ComponentIndex)
		{
		case 0:		return FLinearColor(HSVColor.R, HSVColor.G, HSVColor.B, 1.0f);
		case 1:		return FLinearColor(HSVColor.R, 1.0f, HSVColor.B, 1.0f).HSVToLinearRGB();
		case 2:		return FLinearColor(HSVColor.R, HSVColor.G, 1.0f, 1.0f).HSVToLinearRGB();
		case 3:		return FLinearColor(HSVColor.R, HSVColor.G, HSVColor.B, 1.0f).HSVToLinearRGB();
		default:	return FLinearColor(ForceInit);
		}
	}
}

FLinearColor SDisplayClusterColorGradingColorWheel::GetGradientFillerColor(const FVector4& ColorValue, bool bIsRGB, uint32 ComponentIndex) const
{
	const float MaxValue = 1.0f/*SpinBoxMinMaxSliderValues.CurrentMaxSliderValue.GetValue()*/;
	if (bIsRGB)
	{
		switch (ComponentIndex)
		{
		case 0:		return FLinearColor(MaxValue, ColorValue.Y, ColorValue.Z, 1.0f);
		case 1:		return FLinearColor(ColorValue.X, MaxValue, ColorValue.Z, 1.0f);
		case 2:		return FLinearColor(ColorValue.X, ColorValue.Y, MaxValue, 1.0f);
		case 3:		return FLinearColor(ColorValue.X, ColorValue.Y, ColorValue.Z, 1.0f);
		default:	return FLinearColor(ForceInit);
		}
	}
	else
	{
		FLinearColor HSVColor = FLinearColor(ColorValue.X, ColorValue.Y, ColorValue.Z, ColorValue.W).LinearRGBToHSV();
		switch (ComponentIndex)
		{
		case 0:		return FLinearColor(HSVColor.R, HSVColor.G, HSVColor.B, 1.0f);
		case 1:		return FLinearColor(HSVColor.R, 1.0f, HSVColor.B, 1.0f).HSVToLinearRGB();
		case 2:		return FLinearColor(HSVColor.R, HSVColor.G, MaxValue, 1.0f).HSVToLinearRGB();
		case 3:		return FLinearColor(HSVColor.R, HSVColor.G, HSVColor.B, 1.0f).HSVToLinearRGB();
		default:	return FLinearColor(ForceInit);
		}
	}
}

EVisibility SDisplayClusterColorGradingColorWheel::GetGradientVisibility() const
{
	bool bShowGradient =
		ColorPropertyMetadata->ColorGradingMode != EColorGradingModes::Offset &&
		ColorPropertyMetadata->ColorGradingMode != EColorGradingModes::Invalid;

	return bShowGradient ? EVisibility::Visible : EVisibility::Hidden;
}

TOptional<float> SDisplayClusterColorGradingColorWheel::GetMetadataMinValue() const
{
	if (ColorPropertyMetadata.IsSet())
	{
		return ColorPropertyMetadata->MinValue;
	}

	return TOptional<float>();
}

TOptional<float> SDisplayClusterColorGradingColorWheel::GetMetadataMaxValue() const
{
	if (ColorPropertyMetadata.IsSet())
	{
		return ColorPropertyMetadata->MaxValue;
	}

	return TOptional<float>();
}

TOptional<float> SDisplayClusterColorGradingColorWheel::GetMetadataSliderMinValue() const
{
	if (ColorPropertyMetadata.IsSet())
	{
		return ColorPropertyMetadata->SliderMinValue;
	}

	return TOptional<float>();
}

TOptional<float> SDisplayClusterColorGradingColorWheel::GetMetadataSliderMaxValue() const
{
	if (ColorPropertyMetadata.IsSet())
	{
		return ColorPropertyMetadata->SliderMaxValue;
	}

	return TOptional<float>();
}

float SDisplayClusterColorGradingColorWheel::GetMetadataDelta() const
{
	if (ColorPropertyMetadata.IsSet())
	{
		return ColorPropertyMetadata->Delta;
	}

	return 0.0f;
}

int32 SDisplayClusterColorGradingColorWheel::GetMetadataShiftMouseMovePixelPerDelta() const
{
	if (ColorPropertyMetadata.IsSet())
	{
		return ColorPropertyMetadata->ShiftMouseMovePixelPerDelta;
	}

	return 0;
}

bool SDisplayClusterColorGradingColorWheel::GetMetadataSupportDynamicSliderMinValue() const
{
	if (ColorPropertyMetadata.IsSet())
	{
		return ColorPropertyMetadata->bSupportDynamicSliderMinValue;
	}

	return false;
}

bool SDisplayClusterColorGradingColorWheel::GetMetadataSupportDynamicSliderMaxValue() const
{
	if (ColorPropertyMetadata.IsSet())
	{
		return ColorPropertyMetadata->bSupportDynamicSliderMaxValue;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE