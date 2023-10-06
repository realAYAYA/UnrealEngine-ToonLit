// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/WeightedValueCustomization.h"
#include "ChaosClothAsset/WeightedValue.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Editor.h"

#define LOCTEXT_NAMESPACE "ChaosClothAssetWeightedValueCustomization"

namespace UE::Chaos::ClothAsset
{
	namespace Private
	{
		static const FString OverridePrefix = TEXT("_Override");

		static bool IsOverrideProperty(const TSharedPtr<IPropertyHandle>& Property)
		{
			const FStringView PropertyPath = Property ? Property->GetPropertyPath() : FStringView();
			return PropertyPath.EndsWith(OverridePrefix, ESearchCase::CaseSensitive);
		}

		static bool IsOverridePropertyOf(const TSharedPtr<IPropertyHandle>& OverrideProperty, const TSharedPtr<IPropertyHandle>& Property)
		{
			const FStringView OverridePropertyPath = OverrideProperty ? OverrideProperty->GetPropertyPath() : FStringView();
			const FStringView PropertyPath = Property ? Property->GetPropertyPath() : FStringView();
			return OverridePropertyPath == FString(PropertyPath) + OverridePrefix;
		}
	}

	TSharedRef<IPropertyTypeCustomization> FWeightedValueCustomization::MakeInstance()
	{
		return MakeShareable(new FWeightedValueCustomization);
	}

	FWeightedValueCustomization::FWeightedValueCustomization() = default;
	
	FWeightedValueCustomization::~FWeightedValueCustomization() = default;

	void FWeightedValueCustomization::MakeHeaderRow(TSharedRef<class IPropertyHandle>& StructPropertyHandle, FDetailWidgetRow& Row)
	{
		TWeakPtr<IPropertyHandle> StructWeakHandlePtr = StructPropertyHandle;

		TSharedPtr<SHorizontalBox> HorizontalBox;

		Row.NameContent()
			[
				StructPropertyHandle->CreatePropertyNameWidget()
			]
		.ValueContent()
			// Make enough space for each child handle
			.MinDesiredWidth(125.f * SortedChildHandles.Num())
			.MaxDesiredWidth(125.f * SortedChildHandles.Num())
			[
				SAssignNew(HorizontalBox, SHorizontalBox)
				.IsEnabled(this, &FMathStructCustomization::IsValueEnabled, StructWeakHandlePtr)
			];

		for (int32 ChildIndex = 0; ChildIndex < SortedChildHandles.Num(); ++ChildIndex)
		{
			TSharedRef<IPropertyHandle> ChildHandle = SortedChildHandles[ChildIndex];

			if (Private::IsOverrideProperty(ChildHandle))
			{
				continue;  // Skip overrides
			}

			// Propagate metadata to child properties so that it's reflected in the nested, individual spin boxes
			ChildHandle->SetInstanceMetaData(TEXT("UIMin"), StructPropertyHandle->GetMetaData(TEXT("UIMin")));
			ChildHandle->SetInstanceMetaData(TEXT("UIMax"), StructPropertyHandle->GetMetaData(TEXT("UIMax")));
			ChildHandle->SetInstanceMetaData(TEXT("SliderExponent"), StructPropertyHandle->GetMetaData(TEXT("SliderExponent")));
			ChildHandle->SetInstanceMetaData(TEXT("Delta"), StructPropertyHandle->GetMetaData(TEXT("Delta")));
			ChildHandle->SetInstanceMetaData(TEXT("LinearDeltaSensitivity"), StructPropertyHandle->GetMetaData(TEXT("LinearDeltaSensitivity")));
			ChildHandle->SetInstanceMetaData(TEXT("ShiftMouseMovePixelPerDelta"), StructPropertyHandle->GetMetaData(TEXT("ShiftMouseMovePixelPerDelta")));
			ChildHandle->SetInstanceMetaData(TEXT("SupportDynamicSliderMaxValue"), StructPropertyHandle->GetMetaData(TEXT("SupportDynamicSliderMaxValue")));
			ChildHandle->SetInstanceMetaData(TEXT("SupportDynamicSliderMinValue"), StructPropertyHandle->GetMetaData(TEXT("SupportDynamicSliderMinValue")));
			ChildHandle->SetInstanceMetaData(TEXT("ClampMin"), StructPropertyHandle->GetMetaData(TEXT("ClampMin")));
			ChildHandle->SetInstanceMetaData(TEXT("ClampMax"), StructPropertyHandle->GetMetaData(TEXT("ClampMax")));

			const bool bLastChild = SortedChildHandles.Num() - 1 == ChildIndex;

			TSharedRef<SWidget> ChildWidget = MakeChildWidget(StructPropertyHandle, ChildHandle);
			
			if (ChildHandle->GetPropertyClass() == FBoolProperty::StaticClass())
			{
				HorizontalBox->AddSlot()
					.Padding(FMargin(0.f, 2.f, bLastChild ? 0.f : 3.f, 2.f))
					.AutoWidth()  // keep the check box slots small
					[
						ChildWidget
					];
			}
			else
			{
				if (ChildHandle->GetPropertyClass() == FFloatProperty::StaticClass())
				{
					NumericEntryBoxWidgetList.Add(ChildWidget);
				}

				HorizontalBox->AddSlot()
					.Padding(FMargin(0.f, 2.f, bLastChild ? 0.f : 3.f, 2.f))
					[
						ChildWidget
					];
			}
		}
	}

	TSharedRef<SWidget> FWeightedValueCustomization::MakeChildWidget(
		TSharedRef<IPropertyHandle>& StructurePropertyHandle,
		TSharedRef<IPropertyHandle>& PropertyHandle)
	{
		const FFieldClass* PropertyClass = PropertyHandle->GetPropertyClass();

		if (PropertyClass == FFloatProperty::StaticClass())
		{
			return MakeFloatWidget(StructurePropertyHandle, PropertyHandle);
		}
		if (PropertyClass == FBoolProperty::StaticClass())
		{
			TWeakPtr<IPropertyHandle> WeakHandlePtr = PropertyHandle;
			return
				SNew(SCheckBox)
				.ToolTipText(
					LOCTEXT(
						"IsAnimatable",
						"Whether the property can ever be updated/animated in real time.\n"
						"This could make the simulation takes more CPU time, even more so if the weight maps needs updating."))
				.Type(ESlateCheckBoxType::CheckBox)
				.IsChecked_Lambda([WeakHandlePtr]()->ECheckBoxState
					{
						bool bValue = false;
						WeakHandlePtr.Pin()->GetValue(bValue);
						return bValue ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					})
				.OnCheckStateChanged_Lambda([WeakHandlePtr](ECheckBoxState CheckBoxState)
					{
						WeakHandlePtr.Pin()->SetValue(CheckBoxState == ECheckBoxState::Checked, EPropertyValueSetFlags::DefaultFlags);
					});
		}
		if (PropertyClass == FStrProperty::StaticClass())
		{
			// Manage override property values (properties ending with _Override)
			TWeakPtr<IPropertyHandle> OverrideHandleWeakPtr;

			for (int32 ChildIndex = 0; ChildIndex < SortedChildHandles.Num(); ++ChildIndex)
			{
				const bool bLastChild = SortedChildHandles.Num() - 1 == ChildIndex;
				const TSharedRef<IPropertyHandle>& ChildHandle = SortedChildHandles[ChildIndex];

				if (Private::IsOverridePropertyOf(ChildHandle, PropertyHandle))
				{
					OverrideHandleWeakPtr = ChildHandle;
					break;
				}
			}

			TWeakPtr<IPropertyHandle> HandleWeakPtr = PropertyHandle;
			return
				SNew(SEditableTextBox)
				.ToolTipText(LOCTEXT("WeightMap", "The name of the weight map for this property."))
				.Text_Lambda([HandleWeakPtr, OverrideHandleWeakPtr]() -> FText
					{
						FString Text;
						if (const TSharedPtr<IPropertyHandle> OverrideHandlePtr = OverrideHandleWeakPtr.Pin())
						{
							OverrideHandlePtr->GetValue(Text);
						}
						if (Text == UE::Chaos::ClothAsset::FWeightMapTools::NotOverridden)
						{
							Text.Empty();  // GetValue seems to concatenate the text if the string isn't emptied first
							if (const TSharedPtr<IPropertyHandle> HandlePtr = HandleWeakPtr.Pin())
							{
								HandlePtr->GetValue(Text);
							}
						}
						return FText::FromString(Text);

					})
				.OnTextCommitted_Lambda([HandleWeakPtr](const FText& Text, ETextCommit::Type)
					{
						if (const TSharedPtr<IPropertyHandle> HandlePtr = HandleWeakPtr.Pin())
						{
							HandlePtr->SetValue(Text.ToString(), EPropertyValueSetFlags::DefaultFlags);
						}
					})
				.IsEnabled_Lambda([OverrideHandleWeakPtr]() -> bool
					{
						FString Text;
						if (const TSharedPtr<IPropertyHandle> OverrideHandlePtr = OverrideHandleWeakPtr.Pin())
						{
							OverrideHandlePtr->GetValue(Text);
						}
						return Text == UE::Chaos::ClothAsset::FWeightMapTools::NotOverridden;
					})
				.Font(IPropertyTypeCustomizationUtils::GetRegularFont());
		}

		checkf(false, TEXT("Unsupported property class for the Weighted Values customization."));
		return SNullWidget::NullWidget;
	}

	TSharedRef<SWidget> FWeightedValueCustomization::MakeFloatWidget(
		TSharedRef<IPropertyHandle>& StructurePropertyHandle,
		TSharedRef<IPropertyHandle>& PropertyHandle)
	{
		TOptional<float> MinValue, MaxValue, SliderMinValue, SliderMaxValue;
		float SliderExponent, Delta;
		int32 ShiftMouseMovePixelPerDelta = 1;
		bool SupportDynamicSliderMaxValue = false;
		bool SupportDynamicSliderMinValue = false;

		ExtractFloatMetadata(StructurePropertyHandle, MinValue, MaxValue, SliderMinValue, SliderMaxValue, SliderExponent, Delta, ShiftMouseMovePixelPerDelta, SupportDynamicSliderMaxValue, SupportDynamicSliderMinValue);

		TWeakPtr<IPropertyHandle> WeakHandlePtr = PropertyHandle;

		return SNew(SNumericEntryBox<float>)
			.IsEnabled(this, &FMathStructCustomization::IsValueEnabled, WeakHandlePtr)
			.EditableTextBoxStyle(&FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox"))
			.Value_Lambda([WeakHandlePtr]()
				{
					float Value = 0.;
					return (WeakHandlePtr.Pin()->GetValue(Value) == FPropertyAccess::Success) ?
						TOptional<float>(Value) :
						TOptional<float>();  // Value couldn't be accessed, return an unset value
				})
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.UndeterminedString(NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values"))
			.OnValueCommitted_Lambda([WeakHandlePtr](float Value, ETextCommit::Type)
				{
					WeakHandlePtr.Pin()->SetValue(Value, EPropertyValueSetFlags::DefaultFlags);
				})
			.OnValueChanged_Lambda([this, WeakHandlePtr](float Value)
				{
					if (bIsUsingSlider)
					{
						WeakHandlePtr.Pin()->SetValue(Value, EPropertyValueSetFlags::InteractiveChange);
					}
				})
			.OnBeginSliderMovement_Lambda([this]()
				{
					bIsUsingSlider = true;
					GEditor->BeginTransaction(LOCTEXT("SetVectorProperty", "Set Vector Property"));
				})
			.OnEndSliderMovement_Lambda([this](float Value)
				{
					bIsUsingSlider = false;
					GEditor->EndTransaction();
				})
			.LabelVAlign(VAlign_Center)
			// Only allow spin on handles with one object.  Otherwise it is not clear what value to spin
			.AllowSpin(PropertyHandle->GetNumOuterObjects() < 2)
			.ShiftMouseMovePixelPerDelta(ShiftMouseMovePixelPerDelta)
			.SupportDynamicSliderMaxValue(SupportDynamicSliderMaxValue)
			.SupportDynamicSliderMinValue(SupportDynamicSliderMinValue)
			.OnDynamicSliderMaxValueChanged(this, &FWeightedValueCustomization::OnDynamicSliderMaxValueChanged)
			.OnDynamicSliderMinValueChanged(this, &FWeightedValueCustomization::OnDynamicSliderMinValueChanged)
			.MinValue(MinValue)
			.MaxValue(MaxValue)
			.MinSliderValue(SliderMinValue)
			.MaxSliderValue(SliderMaxValue)
			.SliderExponent(SliderExponent)
			.Delta(Delta)
			.Label()
			[
				SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(FText::FromString(PropertyHandle->GetMetaData(TEXT("ChaosClothAssetShortName"))))  // Case specific metadata, uses ChaosCloth prefix to avoid conflict with future engine extensions
			];
	}

	// The following code is just a plain copy of FMathStructCustomization which
	// would need changes to be able to serve as a base class for this customization.
	void FWeightedValueCustomization::ExtractFloatMetadata(TSharedRef<IPropertyHandle>& PropertyHandle, TOptional<float>& MinValue, TOptional<float>& MaxValue, TOptional<float>& SliderMinValue, TOptional<float>& SliderMaxValue, float& SliderExponent, float& Delta, int32& ShiftMouseMovePixelPerDelta, bool& SupportDynamicSliderMaxValue, bool& SupportDynamicSliderMinValue)
	{
		FProperty* Property = PropertyHandle->GetProperty();

		const FString& MetaUIMinString = Property->GetMetaData(TEXT("UIMin"));
		const FString& MetaUIMaxString = Property->GetMetaData(TEXT("UIMax"));
		const FString& SliderExponentString = Property->GetMetaData(TEXT("SliderExponent"));
		const FString& DeltaString = Property->GetMetaData(TEXT("Delta"));
		const FString& ShiftMouseMovePixelPerDeltaString = Property->GetMetaData(TEXT("ShiftMouseMovePixelPerDelta"));
		const FString& SupportDynamicSliderMaxValueString = Property->GetMetaData(TEXT("SupportDynamicSliderMaxValue"));
		const FString& SupportDynamicSliderMinValueString = Property->GetMetaData(TEXT("SupportDynamicSliderMinValue"));
		const FString& ClampMinString = Property->GetMetaData(TEXT("ClampMin"));
		const FString& ClampMaxString = Property->GetMetaData(TEXT("ClampMax"));

		// If no UIMin/Max was specified then use the clamp string
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

		SliderExponent = float(1);

		if (SliderExponentString.Len())
		{
			TTypeFromString<float>::FromString(SliderExponent, *SliderExponentString);
		}

		Delta = float(0);

		if (DeltaString.Len())
		{
			TTypeFromString<float>::FromString(Delta, *DeltaString);
		}

		ShiftMouseMovePixelPerDelta = 1;
		if (ShiftMouseMovePixelPerDeltaString.Len())
		{
			TTypeFromString<int32>::FromString(ShiftMouseMovePixelPerDelta, *ShiftMouseMovePixelPerDeltaString);
			//The value should be greater or equal to 1
			// 1 is neutral since it is a multiplier of the mouse drag pixel
			if (ShiftMouseMovePixelPerDelta < 1)
			{
				ShiftMouseMovePixelPerDelta = 1;
			}
		}

		const float ActualUIMin = FMath::Max(UIMin, ClampMin);
		const float ActualUIMax = FMath::Min(UIMax, ClampMax);

		MinValue = ClampMinString.Len() ? ClampMin : TOptional<float>();
		MaxValue = ClampMaxString.Len() ? ClampMax : TOptional<float>();
		SliderMinValue = (UIMinString.Len()) ? ActualUIMin : TOptional<float>();
		SliderMaxValue = (UIMaxString.Len()) ? ActualUIMax : TOptional<float>();

		SupportDynamicSliderMaxValue = SupportDynamicSliderMaxValueString.Len() > 0 && SupportDynamicSliderMaxValueString.ToBool();
		SupportDynamicSliderMinValue = SupportDynamicSliderMinValueString.Len() > 0 && SupportDynamicSliderMinValueString.ToBool();
	}

	void FWeightedValueCustomization::OnDynamicSliderMaxValueChanged(float NewMaxSliderValue, TWeakPtr<SWidget> InValueChangedSourceWidget, bool IsOriginator, bool UpdateOnlyIfHigher)
	{
		for (TWeakPtr<SWidget>& Widget : NumericEntryBoxWidgetList)
		{
			TSharedPtr<SNumericEntryBox<float>> NumericBox = StaticCastSharedPtr<SNumericEntryBox<float>>(Widget.Pin());

			if (NumericBox.IsValid())
			{
				TSharedPtr<SSpinBox<float>> SpinBox = StaticCastSharedPtr<SSpinBox<float>>(NumericBox->GetSpinBox());

				if (SpinBox.IsValid())
				{
					if (SpinBox != InValueChangedSourceWidget)
					{
						if ((NewMaxSliderValue > SpinBox->GetMaxSliderValue() && UpdateOnlyIfHigher) || !UpdateOnlyIfHigher)
						{
							// Make sure the max slider value is not a getter otherwise we will break the link!
							verifySlow(!SpinBox->IsMaxSliderValueBound());
							SpinBox->SetMaxSliderValue(NewMaxSliderValue);
						}
					}
				}
			}
		}

		if (IsOriginator)
		{
			OnNumericEntryBoxDynamicSliderMaxValueChanged.Broadcast((float)NewMaxSliderValue, InValueChangedSourceWidget, false, UpdateOnlyIfHigher);
		}
	}

	void FWeightedValueCustomization::OnDynamicSliderMinValueChanged(float NewMinSliderValue, TWeakPtr<SWidget> InValueChangedSourceWidget, bool IsOriginator, bool UpdateOnlyIfLower)
	{
		for (TWeakPtr<SWidget>& Widget : NumericEntryBoxWidgetList)
		{
			TSharedPtr<SNumericEntryBox<float>> NumericBox = StaticCastSharedPtr<SNumericEntryBox<float>>(Widget.Pin());

			if (NumericBox.IsValid())
			{
				TSharedPtr<SSpinBox<float>> SpinBox = StaticCastSharedPtr<SSpinBox<float>>(NumericBox->GetSpinBox());

				if (SpinBox.IsValid())
				{
					if (SpinBox != InValueChangedSourceWidget)
					{
						if ((NewMinSliderValue < SpinBox->GetMinSliderValue() && UpdateOnlyIfLower) || !UpdateOnlyIfLower)
						{
							// Make sure the min slider value is not a getter otherwise we will break the link!
							verifySlow(!SpinBox->IsMinSliderValueBound());
							SpinBox->SetMinSliderValue(NewMinSliderValue);
						}
					}
				}
			}
		}

		if (IsOriginator)
		{
			OnNumericEntryBoxDynamicSliderMinValueChanged.Broadcast((float)NewMinSliderValue, InValueChangedSourceWidget, false, UpdateOnlyIfLower);
		}
	}
}  // End namespace UE::Chaos::ClothAsset

#undef LOCTEXT_NAMESPACE
