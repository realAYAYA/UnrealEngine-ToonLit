// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/SMutableParametersWidget.h"

#include "Containers/Array.h"
#include "Containers/StringConv.h"
#include "Containers/UnrealString.h"
#include "Fonts/SlateFontInfo.h"
#include "Framework/SlateDelegates.h"
#include "Framework/Text/TextLayout.h"
#include "HAL/PlatformCrt.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Internationalization/Text.h"
#include "Layout/Children.h"
#include "Math/Vector2D.h"
#include "Misc/Attribute.h"
#include "MuCOE/SMutableTextSearchBox.h"
#include "MuR/Ptr.h"
#include "SlotBase.h"
#include "Styling/AppStyle.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SVectorInputBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

struct FGeometry;


#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


void SMutableParametersWidget::Construct(const FArguments& InArgs)
{
	OnParametersValueChanged = InArgs._OnParametersValueChanged;

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 4.0f)
		.HAlign(HAlign_Fill)
		[
			SAssignNew(ParamBox, SVerticalBox)
		]
	];
}


void SMutableParametersWidget::SetParameters(const mu::ParametersPtr& InParameters)
{
	MutableParameters = InParameters;
	bIsPendingUpdate = true;
}


void SMutableParametersWidget::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (!bIsPendingUpdate)
	{
		return;
	}

	bIsPendingUpdate = false;

	ParamBox->ClearChildren();

	if (!MutableParameters)
	{
		return;
	}

	for ( int32 ParamIndex=0; ParamIndex< MutableParameters->GetCount(); ++ParamIndex )
	{
		FString ParamName = ANSI_TO_TCHAR(MutableParameters->GetName(ParamIndex));
		TSharedPtr<SHorizontalBox> ParameterBox;

		ParamBox->AddSlot()
			.Padding(1.0f, 6.0f)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.AutoHeight()
			[
				SAssignNew(ParameterBox, SHorizontalBox)

				+ SHorizontalBox::Slot()
				.FillWidth(0.66f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(ParamName))
					.Justification(ETextJustify::Left)
					.AutoWrapText(true)
				]
			];

		switch (MutableParameters->GetType(ParamIndex) )
		{
		case mu::PARAMETER_TYPE::T_BOOL:
			ParameterBox->AddSlot()
				.Padding(4.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SCheckBox)
					.Visibility(this, &SMutableParametersWidget::GetParameterVisibility, ParamIndex)
					.IsChecked(this, &SMutableParametersWidget::GetBoolParameterValue, ParamIndex)
					.OnCheckStateChanged(this, &SMutableParametersWidget::OnBoolParameterChanged, ParamIndex)
				];
			break;

		case mu::PARAMETER_TYPE::T_FLOAT:
			ParameterBox->AddSlot()
				.Padding(4.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SNumericEntryBox<float>)
					.Visibility(this, &SMutableParametersWidget::GetParameterVisibility, ParamIndex)
					.AllowSpin(true)
					.MinSliderValue(0.0f)
					.MaxSliderValue(1.0f)
					.Value(this, &SMutableParametersWidget::GetFloatParameterValue, ParamIndex)
					.OnValueChanged(this, &SMutableParametersWidget::OnFloatParameterChanged, ParamIndex)
				];
			break;

		case mu::PARAMETER_TYPE::T_COLOUR:
			ParameterBox->AddSlot()
				.Padding(4.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SColorBlock)
					.Color(this, &SMutableParametersWidget::GetColorParameterValue, ParamIndex)
					.ShowBackgroundForAlpha(false)
					.AlphaDisplayMode(EColorBlockAlphaDisplayMode::Ignore)
					.UseSRGB(false)
					.OnMouseButtonDown(this, &SMutableParametersWidget::OnColorBlockMouseButtonDown, ParamIndex)
					.Size(FVector2D(10.0f, 10.0f))
				];
			break;

		case mu::PARAMETER_TYPE::T_INT:
		{
			// If we have a list of options, add a combo box too
			TSharedPtr<SMutableTextSearchBox> ParamComboBox;
			int32 ValueCount = MutableParameters->GetIntPossibleValueCount(ParamIndex);
			if (ValueCount > 0)
			{
				FString ToolTipText = FString("None");
				TArray<FString> OptionNamesAttribute;
				int Value = MutableParameters->GetIntValue( ParamIndex );
				int ValueIndex = MutableParameters->GetIntValueIndex( ParamIndex, Value );
				for (int i = 0; i < ValueCount; ++i)
				{
					const char* ValueText = MutableParameters->GetIntPossibleValueName( ParamIndex, i );
					OptionNamesAttribute.Add( FString(ANSI_TO_TCHAR(ValueText)) );				
				}

				ParameterBox->AddSlot()
					.Padding(4.0f)
					.VAlign(VAlign_Center)
					[
						SAssignNew(ParamComboBox, SMutableTextSearchBox)
						.Visibility(this, &SMutableParametersWidget::GetParameterVisibility, ParamIndex)
						.ToolTipText(FText::FromString(ToolTipText))
						.PossibleSuggestions(OptionNamesAttribute)
						.InitialText(FText::FromString(OptionNamesAttribute[ValueIndex]))
						.MustMatchPossibleSuggestions(TAttribute<bool>(true))
						.SuggestionListPlacement(EMenuPlacement::MenuPlacement_ComboBox)
						.OnTextCommitted(this, &SMutableParametersWidget::OnIntParameterTextChanged, ParamIndex)
					];
			}

			ParameterBox->AddSlot()
				.Padding(4.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SNumericEntryBox<int32>)
					.Visibility(this, &SMutableParametersWidget::GetParameterVisibility, ParamIndex)
					.AllowSpin(true)
					.MinSliderValue(0)
					.MaxSliderValue(this, &SMutableParametersWidget::GetIntParameterValueMax, ParamIndex)
					.Value(this, &SMutableParametersWidget::GetIntParameterValue, ParamIndex)
					.OnValueChanged(this, &SMutableParametersWidget::OnIntParameterChanged, ParamIndex, ParamComboBox)
				];
			break;
		}

		case mu::PARAMETER_TYPE::T_PROJECTOR:
			ParameterBox->AddSlot()
				.Padding(4.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SGridPanel)
					.FillColumn(0, 1.0f)

					+SGridPanel::Slot(0,0)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("Location")))
						.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					]

					+SGridPanel::Slot(1, 0)
					[
						SNew(SNumericVectorInputBox<FVector::FReal>)
						.X(this, &SMutableParametersWidget::GetProjectorLocation, EAxis::X, ParamIndex)
						.Y(this, &SMutableParametersWidget::GetProjectorLocation, EAxis::Y, ParamIndex)
						.Z(this, &SMutableParametersWidget::GetProjectorLocation, EAxis::Z, ParamIndex)
						.bColorAxisLabels(true)
						.OnXChanged(this, &SMutableParametersWidget::SetProjectorLocation, ETextCommit::Default, EAxis::X, false, ParamIndex)
						.OnYChanged(this, &SMutableParametersWidget::SetProjectorLocation, ETextCommit::Default, EAxis::Y, false, ParamIndex)
						.OnZChanged(this, &SMutableParametersWidget::SetProjectorLocation, ETextCommit::Default, EAxis::Z, false, ParamIndex)
						.OnXCommitted(this, &SMutableParametersWidget::SetProjectorLocation, EAxis::X, true, ParamIndex)
						.OnYCommitted(this, &SMutableParametersWidget::SetProjectorLocation, EAxis::Y, true, ParamIndex)
						.OnZCommitted(this, &SMutableParametersWidget::SetProjectorLocation, EAxis::Z, true, ParamIndex)
						.AllowSpin(true)
						.SpinDelta(1)
					]

					+ SGridPanel::Slot(0, 1)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("Scale")))
						.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
					]

					+ SGridPanel::Slot(1, 1)
					[
						SNew(SNumericVectorInputBox<FVector::FReal>)
						.X(this, &SMutableParametersWidget::GetProjectorScale, EAxis::X, ParamIndex)
						.Y(this, &SMutableParametersWidget::GetProjectorScale, EAxis::Y, ParamIndex)
						.Z(this, &SMutableParametersWidget::GetProjectorScale, EAxis::Z, ParamIndex)
						.bColorAxisLabels(true)
						.OnXChanged(this, &SMutableParametersWidget::SetProjectorScale, ETextCommit::Default, EAxis::X, false, ParamIndex)
						.OnYChanged(this, &SMutableParametersWidget::SetProjectorScale, ETextCommit::Default, EAxis::Y, false, ParamIndex)
						.OnZChanged(this, &SMutableParametersWidget::SetProjectorScale, ETextCommit::Default, EAxis::Z, false, ParamIndex)
						.OnXCommitted(this, &SMutableParametersWidget::SetProjectorScale, EAxis::X, true, ParamIndex)
						.OnYCommitted(this, &SMutableParametersWidget::SetProjectorScale, EAxis::Y, true, ParamIndex)
						.OnZCommitted(this, &SMutableParametersWidget::SetProjectorScale, EAxis::Z, true, ParamIndex)
						.AllowSpin(true)
						.SpinDelta(1)
					]

				];
			break;

		default:
			// Unsupported parameter type
			ParameterBox->AddSlot()
				.Padding(4.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("Unsupported parameter type.")))
					.Justification(ETextJustify::Left)
					.AutoWrapText(true)
				];
		}

	}
}


EVisibility SMutableParametersWidget::GetParameterVisibility(int32 ParamIndex) const
{
	// \TODO: Parameter relevancy?
	if (!MutableParameters
		|| ParamIndex >= MutableParameters->GetCount() )
	{
		return EVisibility::HitTestInvisible;
	}

	return EVisibility::Visible;
}


ECheckBoxState SMutableParametersWidget::GetBoolParameterValue(int32 ParamIndex) const
{
	if ( !MutableParameters 
		|| ParamIndex>=MutableParameters->GetCount()
		|| MutableParameters->GetType(ParamIndex)!=mu::PARAMETER_TYPE::T_BOOL )
	{
		return ECheckBoxState::Undetermined;
	}	

	return MutableParameters->GetBoolValue(ParamIndex) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}


void SMutableParametersWidget::OnBoolParameterChanged(ECheckBoxState InCheckboxState, int32 ParamIndex)
{
	if (!MutableParameters
		|| ParamIndex >= MutableParameters->GetCount()
		|| MutableParameters->GetType(ParamIndex) != mu::PARAMETER_TYPE::T_BOOL)
	{
		return;
	}

	MutableParameters->SetBoolValue( ParamIndex, InCheckboxState==ECheckBoxState::Checked );

	OnParametersValueChanged.ExecuteIfBound(ParamIndex);
}


TOptional<float> SMutableParametersWidget::GetFloatParameterValue(int32 ParamIndex) const
{
	if (!MutableParameters
		|| ParamIndex >= MutableParameters->GetCount()
		|| MutableParameters->GetType(ParamIndex) != mu::PARAMETER_TYPE::T_FLOAT)
	{
		return TOptional<float>();
	}

	return MutableParameters->GetFloatValue(ParamIndex);
}


void SMutableParametersWidget::OnFloatParameterChanged(float InValue, int32 ParamIndex)
{
	if (!MutableParameters
		|| ParamIndex >= MutableParameters->GetCount()
		|| MutableParameters->GetType(ParamIndex) != mu::PARAMETER_TYPE::T_FLOAT)
	{
		return;
	}

	MutableParameters->SetFloatValue(ParamIndex, InValue);

	OnParametersValueChanged.ExecuteIfBound(ParamIndex);
}


TOptional<int32> SMutableParametersWidget::GetIntParameterValue(int32 ParamIndex) const
{
	if (!MutableParameters
		|| ParamIndex >= MutableParameters->GetCount()
		|| MutableParameters->GetType(ParamIndex) != mu::PARAMETER_TYPE::T_INT)
	{
		return TOptional<int32>();
	}

	int32 Value = MutableParameters->GetIntValue(ParamIndex);
	int32 ValueIndex = MutableParameters->GetIntValueIndex(ParamIndex, Value);

	return ValueIndex;
}


TOptional<int32> SMutableParametersWidget::GetIntParameterValueMax(int32 ParamIndex) const
{
	if (!MutableParameters
		|| ParamIndex >= MutableParameters->GetCount()
		|| MutableParameters->GetType(ParamIndex) != mu::PARAMETER_TYPE::T_INT)
	{
		return TOptional<int32>();
	}

	int32 ValueCount = MutableParameters->GetIntPossibleValueCount(ParamIndex);

	if (!ValueCount)
	{
		ValueCount = 16;
	}

	return ValueCount -1;
}


void SMutableParametersWidget::OnIntParameterChanged(int32 InValue, int32 ParamIndex, TSharedPtr<SMutableTextSearchBox> Combo )
{
	if (!MutableParameters
		|| ParamIndex >= MutableParameters->GetCount()
		|| MutableParameters->GetType(ParamIndex) != mu::PARAMETER_TYPE::T_INT)
	{
		return;
	}

	// The slider value is actually the value index for the parameter
	if (InValue >= 0 && InValue < MutableParameters->GetIntPossibleValueCount(ParamIndex))
	{
		int32 RealValue = MutableParameters->GetIntPossibleValue(ParamIndex, InValue);
		MutableParameters->SetIntValue(ParamIndex, RealValue);

		// Update the text combo if any
		if (Combo)
		{
			FString Text = TEXT("Invalid");
			Text = ANSI_TO_TCHAR(MutableParameters->GetIntPossibleValueName(ParamIndex, InValue));
			Combo->SetText(FText::FromString(Text));
		}

		OnParametersValueChanged.ExecuteIfBound(ParamIndex);
	}
}


void SMutableParametersWidget::OnIntParameterTextChanged(TSharedPtr<FString> Selection, ESelectInfo::Type, int32 ParamIndex)
{
	if (!Selection
		|| !MutableParameters
		|| ParamIndex >= MutableParameters->GetCount()
		|| MutableParameters->GetType(ParamIndex) != mu::PARAMETER_TYPE::T_INT)
	{
		return;
	}

	int32 ValueIndex = MutableParameters->GetIntValueIndex(ParamIndex, TCHAR_TO_ANSI(**Selection));
	if (ValueIndex >= 0 && ValueIndex < MutableParameters->GetIntPossibleValueCount(ParamIndex))
	{
		int32 RealValue = MutableParameters->GetIntPossibleValue(ParamIndex, ValueIndex);
		MutableParameters->SetIntValue(ParamIndex, RealValue);

		OnParametersValueChanged.ExecuteIfBound(ParamIndex);
	}
}


FLinearColor SMutableParametersWidget::GetColorParameterValue(int32 ParamIndex) const
{
	if (!MutableParameters
		|| ParamIndex >= MutableParameters->GetCount()
		|| MutableParameters->GetType(ParamIndex) != mu::PARAMETER_TYPE::T_COLOUR)
	{
		return FLinearColor();
	}

	FLinearColor Result;
	MutableParameters->GetColourValue(ParamIndex, &Result.R, &Result.G, &Result.B);
	Result.A = 1.0f;

	return Result;
}


FReply SMutableParametersWidget::OnColorBlockMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, int32 ParamIndex)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}

	FLinearColor col = GetColorParameterValue(ParamIndex);

	TArray<FLinearColor*> LinearColorArray;
	LinearColorArray.Add(&col);

	FColorPickerArgs args;
	args.bIsModal = true;
	args.bUseAlpha = false;
	args.bOnlyRefreshOnMouseUp = false;
	args.InitialColorOverride = col;
	args.OnColorCommitted = FOnLinearColorValueChanged::CreateSP(this, &SMutableParametersWidget::OnSetColorFromColorPicker, ParamIndex);
	OpenColorPicker(args);

	return FReply::Handled();
}


void SMutableParametersWidget::OnSetColorFromColorPicker(FLinearColor NewColor, int32 ParamIndex)
{
	if (!MutableParameters
		|| ParamIndex >= MutableParameters->GetCount()
		|| MutableParameters->GetType(ParamIndex) != mu::PARAMETER_TYPE::T_COLOUR)
	{
		return;
	}

	MutableParameters->SetColourValue(ParamIndex, NewColor.R, NewColor.G, NewColor.B);

	OnParametersValueChanged.ExecuteIfBound(ParamIndex);
}


TOptional<FVector::FReal> SMutableParametersWidget::GetProjectorLocation(EAxis::Type Axis, int32 ParamIndex) const
{ 
	if (!MutableParameters
		|| ParamIndex >= MutableParameters->GetCount()
		|| MutableParameters->GetType(ParamIndex) != mu::PARAMETER_TYPE::T_PROJECTOR)
	{
		return 0.0f;
	}

	mu::PROJECTOR_TYPE Type;
	float Pos[3];
	float Dir[3];
	float Up[3];
	float Scale[3];
	float Angle;
	MutableParameters->GetProjectorValue(ParamIndex, &Type, 
		&Pos[0], &Pos[1], &Pos[2],
		&Dir[0], &Dir[1], &Dir[2],
		&Up[0], &Up[1], &Up[2],
		&Scale[0], &Scale[1], &Scale[2],
		&Angle );

	return Pos[ Axis-EAxis::X ];
}


void SMutableParametersWidget::SetProjectorLocation(FVector::FReal NewValue, ETextCommit::Type, EAxis::Type Axis, bool bCommitted, int32 ParamIndex)
{
	if (!MutableParameters
		|| ParamIndex >= MutableParameters->GetCount()
		|| MutableParameters->GetType(ParamIndex) != mu::PARAMETER_TYPE::T_PROJECTOR)
	{
		return;
	}

	mu::PROJECTOR_TYPE Type;
	float Pos[3];
	float Dir[3];
	float Up[3];
	float Scale[3];
	float Angle;
	MutableParameters->GetProjectorValue(ParamIndex, &Type,
		&Pos[0], &Pos[1], &Pos[2],
		&Dir[0], &Dir[1], &Dir[2],
		&Up[0], &Up[1], &Up[2],
		&Scale[0], &Scale[1], &Scale[2],
		&Angle);

	Pos[Axis - EAxis::X] = NewValue;

	MutableParameters->SetProjectorValue(ParamIndex,
		Pos[0], Pos[1], Pos[2],
		Dir[0], Dir[1], Dir[2],
		Up[0], Up[1], Up[2],
		Scale[0], Scale[1], Scale[2],
		Angle);

	OnParametersValueChanged.ExecuteIfBound(ParamIndex);
}


TOptional<FVector::FReal> SMutableParametersWidget::GetProjectorScale(EAxis::Type Axis, int32 ParamIndex) const
{
	if (!MutableParameters
		|| ParamIndex >= MutableParameters->GetCount()
		|| MutableParameters->GetType(ParamIndex) != mu::PARAMETER_TYPE::T_PROJECTOR)
	{
		return 0.0f;
	}

	mu::PROJECTOR_TYPE Type;
	float Pos[3];
	float Dir[3];
	float Up[3];
	float Scale[3];
	float Angle;
	MutableParameters->GetProjectorValue(ParamIndex, &Type,
		&Pos[0], &Pos[1], &Pos[2],
		&Dir[0], &Dir[1], &Dir[2],
		&Up[0], &Up[1], &Up[2],
		&Scale[0], &Scale[1], &Scale[2],
		&Angle);

	return Scale[Axis - EAxis::X];
}


void SMutableParametersWidget::SetProjectorScale(FVector::FReal NewValue, ETextCommit::Type, EAxis::Type Axis, bool bCommitted, int32 ParamIndex)
{
	if (!MutableParameters
		|| ParamIndex >= MutableParameters->GetCount()
		|| MutableParameters->GetType(ParamIndex) != mu::PARAMETER_TYPE::T_PROJECTOR)
	{
		return;
	}

	mu::PROJECTOR_TYPE Type;
	float Pos[3];
	float Dir[3];
	float Up[3];
	float Scale[3];
	float Angle;
	MutableParameters->GetProjectorValue(ParamIndex, &Type,
		&Pos[0], &Pos[1], &Pos[2],
		&Dir[0], &Dir[1], &Dir[2],
		&Up[0], &Up[1], &Up[2],
		&Scale[0], &Scale[1], &Scale[2],
		&Angle);

	Scale[Axis - EAxis::X] = NewValue;

	MutableParameters->SetProjectorValue(ParamIndex,
		Pos[0], Pos[1], Pos[2],
		Dir[0], Dir[1], Dir[2],
		Up[0], Up[1], Up[2],
		Scale[0], Scale[1], Scale[2],
		Angle);

	OnParametersValueChanged.ExecuteIfBound(ParamIndex);
}


#undef LOCTEXT_NAMESPACE

