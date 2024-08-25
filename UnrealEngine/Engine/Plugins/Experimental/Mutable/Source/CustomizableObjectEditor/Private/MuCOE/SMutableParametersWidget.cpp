// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/SMutableParametersWidget.h"

#include "SSearchableComboBox.h"
#include "MuCOE/UnrealEditorPortabilityHelpers.h"
#include "MuR/ModelPrivate.h"
#include "MuR/ParametersPrivate.h"
#include "Templates/SharedPointer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Widgets/Input/SVectorInputBox.h"
#include "Widgets/Layout/SGridPanel.h"

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
		FString ParamName = MutableParameters->GetName(ParamIndex);

		TSharedPtr<SHorizontalBox> ParameterBox;

		ParamBox->AddSlot()
			.Padding(1.0f, 6.0f)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.AutoHeight()
			[
				SAssignNew(ParameterBox, SHorizontalBox)

#if UE_BUILD_DEBUG
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4,0)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.ColorAndOpacity(FSlateColor(FLinearColor(0.5f,0.5f,0.5f,1.0f)))
					.Text(FText::FromString(FString::FromInt(ParamIndex)))
					.Justification(ETextJustify::Left)
					.AutoWrapText(true)
				]
#endif

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
		
		
		// Use the parameter descriptor to know what parameter value determines the amount of values this one has
		mu::FParameterDesc& ParameterDescription = MutableParameters->GetPrivate()->m_pModel->GetPrivate()->m_program.
															m_parameters[ParamIndex];
		TArray<uint32> Ranges = ParameterDescription.m_ranges;
		if (Ranges.Num() >= 1)
		{
			if (Ranges.Num() > 1)
			{
				// todo: Only supports first dimension
				UE_LOG(LogTemp,Warning,TEXT("Currently the debugger only suports showing the first dimension of each range."))
			}

			// This parameter does have more than one value (multivalue)
			
			// A range index can have more than one range
			constexpr int32 Dimension = 0;		// Only operate over the first dimension
			// Default amount of values to display
			int32 PositionCount = 1;			
			
			// grab only first value (first dimension)
			const uint32 RangeIdx = Ranges[Dimension];
			// Get the parameter that controls the amount of values of current parameter
			const int32 ParameterControllingValueCount = MutableParameters->GetPrivate()->m_pModel->GetPrivate()->m_program.m_ranges[RangeIdx].m_dimensionParameter;

			// If a parameter index has been reported then we know that another parameter's value determines the amount of values we have
			if (ParameterControllingValueCount >= 0)
			{
				// Get the numerical value of that parameters
				mu::PARAMETER_TYPE ParameterType = MutableParameters->GetType(ParameterControllingValueCount);
				if (ParameterType == mu::PARAMETER_TYPE::T_INT)
				{
					PositionCount = MutableParameters->GetIntValue(ParameterControllingValueCount);
				}
				else if (ParameterType == mu::PARAMETER_TYPE::T_FLOAT)
				{
					PositionCount = MutableParameters->GetFloatValue(ParameterControllingValueCount);
				}
				else
				{
					checkNoEntry();
					// Unable to get the amount of options of a non numerical value
				}

				// Show one value at least.
				if (PositionCount < 1)
				{
					PositionCount = 1;
				}
			}

			// If no parameter is controlling the amount of values of this one or if the amount of values is 0 the
			// amount of values that will therefore be used will be 1.

			// Slate where to place all the values for each of the positions of the current parameter at the target dimension
			TSharedPtr<SVerticalBox> ParameterValuesCollection = SNew(SVerticalBox);
			
			// Iterate over all the values for the parameter and show them. Updating their value is managed by the UI
			for (int32 Position = 0; Position < PositionCount; Position++)
			{
				mu::RangeIndexPtr RangeIndex = MutableParameters->NewRangeIndex(ParamIndex);
				RangeIndex->SetPosition(Dimension,Position);
			
				// Show position data if more than one is set to avoid UI clutter
				if (PositionCount > 1)
				{
					// Add identifying data on top
					ParameterValuesCollection->AddSlot()
					.AutoHeight()
					.HAlign(EHorizontalAlignment::HAlign_Right)
					.Padding(4,4,4,2)
					[
						SNew(STextBlock)
						.Text(FText::FromString(FString::Printf(TEXT("Value : %i"), Position )))
						.Justification(ETextJustify::Right)
						.AutoWrapText(true)
					];
				}
			
				// Generate a new temporary slate container to be later added to the vertical container for all dimensions and positions
				TSharedPtr<SHorizontalBox> ParameterProxyContainer = SNew(SHorizontalBox);

				// Process the parameter 
				GenerateAndAttachParameterSlate(ParamIndex,ParameterProxyContainer,RangeIndex);

				// Add the horizontal slot we have used to place the proxy now onto our vertical box (so it is placed inside a vertical slot)
				ParameterValuesCollection->AddSlot()
				.AutoHeight()
				[
					ParameterProxyContainer.ToSharedRef()
				];
			}
		
			// Add the collection of parameter value objects to the ParameterBox
			ParameterBox->AddSlot()
			.FillWidth(1)
			.Padding(4)
			.HAlign(HAlign_Fill)
			[
				ParameterValuesCollection.ToSharedRef()
			];

		}
		else
		{
			// The parameter only can have one value (no multivalue) so draw it normally
			GenerateAndAttachParameterSlate(ParamIndex,ParameterBox,nullptr);
		}
		
	}
}


void SMutableParametersWidget::ScheduleUpdateIfRequired(const int32& InParameterIndex)
{
	// Update the ui to display the new amount of values we want to show for the multidimensional values if this parameter's value
	// controls the amount of values another parameter has.
	const bool bIsRangeSize = MutableParameters->GetPrivate()->m_pModel->GetPrivate()->m_program.m_ranges.ContainsByPredicate([InParameterIndex](const mu::FRangeDesc& r)
		{ return r.m_dimensionParameter == InParameterIndex; });

	// Schedule update since we know the value does change the amount of values shown by another parameter (multivalue parameter)
	if (bIsRangeSize && !bIsPendingUpdate)
	{
		bIsPendingUpdate = true;
	}
}


void SMutableParametersWidget::GenerateAndAttachParameterSlate(const int32 ParamIndex, TSharedPtr<SHorizontalBox> ParameterHorizontalBox,  mu::RangeIndexPtrConst RangeIndex)
{
	switch (MutableParameters->GetType(ParamIndex) )
	{
	case mu::PARAMETER_TYPE::T_BOOL:
		ParameterHorizontalBox->AddSlot()
			.Padding(4.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SCheckBox)
				.Visibility(this, &SMutableParametersWidget::GetParameterVisibility, ParamIndex)
				.IsChecked(this, &SMutableParametersWidget::GetBoolParameterValue, ParamIndex, RangeIndex)
				.OnCheckStateChanged(this, &SMutableParametersWidget::OnBoolParameterChanged, ParamIndex, RangeIndex)
			];
		break;

	case mu::PARAMETER_TYPE::T_FLOAT:
		ParameterHorizontalBox->AddSlot()
			.Padding(4.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SNumericEntryBox<float>)
				.Visibility(this, &SMutableParametersWidget::GetParameterVisibility, ParamIndex)
				.AllowSpin(true)
				.MinSliderValue(0.0f)
				.MaxSliderValue(1.0f)			// TODO: Use actual max possible value instead of hard-coding it
				.Value(this, &SMutableParametersWidget::GetFloatParameterValue, ParamIndex, RangeIndex)
				.OnValueChanged(this, &SMutableParametersWidget::OnFloatParameterChanged, ParamIndex, RangeIndex)
				.OnValueCommitted(this,&SMutableParametersWidget::OnFloatParameterCommitted,ParamIndex)
			];
		break;

	case mu::PARAMETER_TYPE::T_COLOUR:
		ParameterHorizontalBox->AddSlot()
			.Padding(4.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SColorBlock)
				.Color(this, &SMutableParametersWidget::GetColorParameterValue, ParamIndex, RangeIndex)
				.ShowBackgroundForAlpha(false)
				.AlphaDisplayMode(EColorBlockAlphaDisplayMode::Ignore)
				.UseSRGB(false)
				.OnMouseButtonDown(this, &SMutableParametersWidget::OnColorBlockMouseButtonDown, ParamIndex, RangeIndex)
				.Size(FVector2D(10.0f, 10.0f))
			];
		break;

	case mu::PARAMETER_TYPE::T_INT:
	{
		// If we have a list of options, add a combo box too
		TSharedPtr<SSearchableComboBox> ParamComboBox;
		const int32 ValueCount = MutableParameters->GetIntPossibleValueCount(ParamIndex);
		if (ValueCount > 0)
		{
			const FString ToolTipText = FString("None");

			TSharedPtr<TArray<TSharedPtr<FString>>>* FoundOptions = IntParameterOptions.Find(ParamIndex);
			TArray<TSharedPtr<FString>>& OptionNamesAttribute = FoundOptions && FoundOptions->IsValid() ?
																*FoundOptions->Get() :
																*(IntParameterOptions.Add(ParamIndex, MakeShared<TArray<TSharedPtr<FString>>>()).Get());

			OptionNamesAttribute.Empty();

			const int32 Value = MutableParameters->GetIntValue( ParamIndex , RangeIndex);
			const int32 ValueIndex = MutableParameters->GetIntValueIndex( ParamIndex, Value );
			for (int32 i = 0; i < ValueCount; ++i)
			{
				const FString& ValueText = MutableParameters->GetIntPossibleValueName( ParamIndex, i );
				OptionNamesAttribute.Add(MakeShared<FString>(ValueText));				
			}

			ParameterHorizontalBox->AddSlot()
				.Padding(4.0f)
				.VAlign(VAlign_Center)
				[
					SAssignNew(ParamComboBox, SSearchableComboBox)
					.SearchVisibility(this, &SMutableParametersWidget::GetParameterVisibility, ParamIndex)
					.ToolTipText(FText::FromString(ToolTipText))
					.OptionsSource(&OptionNamesAttribute)
					.InitiallySelectedItem(OptionNamesAttribute[ValueIndex])
					.Method(EPopupMethod::UseCurrentWindow)
					.OnSelectionChanged(this, &SMutableParametersWidget::OnIntParameterTextChanged, ParamIndex, RangeIndex)
					.OnGenerateWidget(this, &SMutableParametersWidget::OnGenerateWidgetIntParameter)
					.Content()
					[
						SNew(STextBlock)
						.Text_Lambda([this, ParamIndex, RangeIndex, OptionNamesAttribute]() -> FText
						{
							const int32 Value = MutableParameters->GetIntValue(ParamIndex, RangeIndex);
							const int32 ValueIndex = MutableParameters->GetIntValueIndex(ParamIndex, Value);

							return FText::FromString(*OptionNamesAttribute[ValueIndex]);
						})
					]
				];
		}

		ParameterHorizontalBox->AddSlot()
			.Padding(4.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SNumericEntryBox<int32>)
				.Visibility(this, &SMutableParametersWidget::GetParameterVisibility, ParamIndex)
				.AllowSpin(true)
				.MinSliderValue(0)
				.MaxSliderValue(this, &SMutableParametersWidget::GetIntParameterValueMax, ParamIndex)
				.Value(this, &SMutableParametersWidget::GetIntParameterValue, ParamIndex, RangeIndex)
				.OnValueChanged(this, &SMutableParametersWidget::OnIntParameterChanged, ParamIndex, ParamComboBox, RangeIndex)
			];
		break;
	}

	case mu::PARAMETER_TYPE::T_PROJECTOR:
		ParameterHorizontalBox->AddSlot()
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
					.Font(UE_MUTABLE_GET_FONTSTYLE(TEXT("PropertyWindow.NormalFont")))
				]

				+SGridPanel::Slot(1, 0)
				[
					SNew(SNumericVectorInputBox<FVector::FReal>)
					.X(this, &SMutableParametersWidget::GetProjectorLocation, EAxis::X, ParamIndex, RangeIndex)
					.Y(this, &SMutableParametersWidget::GetProjectorLocation, EAxis::Y, ParamIndex, RangeIndex)
					.Z(this, &SMutableParametersWidget::GetProjectorLocation, EAxis::Z, ParamIndex, RangeIndex)
					.bColorAxisLabels(true)
					.OnXChanged(this, &SMutableParametersWidget::SetProjectorLocation, ETextCommit::Default, EAxis::X, false, ParamIndex, RangeIndex)
					.OnYChanged(this, &SMutableParametersWidget::SetProjectorLocation, ETextCommit::Default, EAxis::Y, false, ParamIndex, RangeIndex)
					.OnZChanged(this, &SMutableParametersWidget::SetProjectorLocation, ETextCommit::Default, EAxis::Z, false, ParamIndex, RangeIndex)
					.OnXCommitted(this, &SMutableParametersWidget::SetProjectorLocation, EAxis::X, true, ParamIndex, RangeIndex)
					.OnYCommitted(this, &SMutableParametersWidget::SetProjectorLocation, EAxis::Y, true, ParamIndex, RangeIndex)
					.OnZCommitted(this, &SMutableParametersWidget::SetProjectorLocation, EAxis::Z, true, ParamIndex, RangeIndex)
					.AllowSpin(true)
					.SpinDelta(1)
				]

				+ SGridPanel::Slot(0, 1)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("Scale")))
					.Font(UE_MUTABLE_GET_FONTSTYLE("PropertyWindow.NormalFont"))
				]

				+ SGridPanel::Slot(1, 1)
				[
					SNew(SNumericVectorInputBox<FVector::FReal>)
					.X(this, &SMutableParametersWidget::GetProjectorScale, EAxis::X, ParamIndex, RangeIndex)
					.Y(this, &SMutableParametersWidget::GetProjectorScale, EAxis::Y, ParamIndex, RangeIndex)
					.Z(this, &SMutableParametersWidget::GetProjectorScale, EAxis::Z, ParamIndex, RangeIndex)
					.bColorAxisLabels(true)
					.OnXChanged(this, &SMutableParametersWidget::SetProjectorScale, ETextCommit::Default, EAxis::X, false, ParamIndex, RangeIndex)
					.OnYChanged(this, &SMutableParametersWidget::SetProjectorScale, ETextCommit::Default, EAxis::Y, false, ParamIndex, RangeIndex)
					.OnZChanged(this, &SMutableParametersWidget::SetProjectorScale, ETextCommit::Default, EAxis::Z, false, ParamIndex, RangeIndex)
					.OnXCommitted(this, &SMutableParametersWidget::SetProjectorScale, EAxis::X, true, ParamIndex, RangeIndex)
					.OnYCommitted(this, &SMutableParametersWidget::SetProjectorScale, EAxis::Y, true, ParamIndex, RangeIndex)
					.OnZCommitted(this, &SMutableParametersWidget::SetProjectorScale, EAxis::Z, true, ParamIndex, RangeIndex)
					.AllowSpin(true)
					.SpinDelta(1)
				]

			];
		break;

	default:
		// Unsupported parameter type
		ParameterHorizontalBox->AddSlot()
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


ECheckBoxState SMutableParametersWidget::GetBoolParameterValue(int32 ParamIndex,  mu::RangeIndexPtrConst RangeIndex) const
{
	if ( !MutableParameters 
		|| ParamIndex>=MutableParameters->GetCount()
		|| MutableParameters->GetType(ParamIndex)!=mu::PARAMETER_TYPE::T_BOOL )
	{
		return ECheckBoxState::Undetermined;
	}	
	
	return MutableParameters->GetBoolValue(ParamIndex,RangeIndex) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}


void SMutableParametersWidget::OnBoolParameterChanged(ECheckBoxState InCheckboxState, int32 ParamIndex,  mu::RangeIndexPtrConst RangeIndex)
{
	if (!MutableParameters
		|| ParamIndex >= MutableParameters->GetCount()
		|| MutableParameters->GetType(ParamIndex) != mu::PARAMETER_TYPE::T_BOOL)
	{
		return;
	}

	MutableParameters->SetBoolValue( ParamIndex, InCheckboxState==ECheckBoxState::Checked, RangeIndex );

	OnParametersValueChanged.ExecuteIfBound(ParamIndex);
}


TOptional<float> SMutableParametersWidget::GetFloatParameterValue(int32 ParamIndex, mu::RangeIndexPtrConst RangeIndex) const
{
	if (!MutableParameters
		|| ParamIndex >= MutableParameters->GetCount()
		|| MutableParameters->GetType(ParamIndex) != mu::PARAMETER_TYPE::T_FLOAT)
	{
		return TOptional<float>();
	}

	return MutableParameters->GetFloatValue(ParamIndex,RangeIndex);
}


void SMutableParametersWidget::OnFloatParameterChanged(float InValue, int32 ParamIndex, mu::RangeIndexPtrConst RangeIndex)
{
	if (!MutableParameters
		|| ParamIndex >= MutableParameters->GetCount()
		|| MutableParameters->GetType(ParamIndex) != mu::PARAMETER_TYPE::T_FLOAT)
	{
		return;
	}

	MutableParameters->SetFloatValue(ParamIndex, InValue, RangeIndex);
	
	OnParametersValueChanged.ExecuteIfBound(ParamIndex);
}

void SMutableParametersWidget::OnFloatParameterCommitted(float Value, ETextCommit::Type CommitType, int32 ParamIndex)
{
	// Think on the possibility of asking for a reconstruction if the new value is a valid integer
	if (Value >= 1 && Value == (int)Value)
	{
		ScheduleUpdateIfRequired(ParamIndex);
	}
}


TOptional<int32> SMutableParametersWidget::GetIntParameterValue (int32 ParamIndex, mu::RangeIndexPtrConst RangeIndex) const
{
	if (!MutableParameters
		|| ParamIndex >= MutableParameters->GetCount()
		|| MutableParameters->GetType(ParamIndex) != mu::PARAMETER_TYPE::T_INT)
	{
		return TOptional<int32>();
	}

	int32 Value = MutableParameters->GetIntValue(ParamIndex, RangeIndex);
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


void SMutableParametersWidget::OnIntParameterChanged(int32 InValue, int32 ParamIndex, TSharedPtr<SSearchableComboBox> Combo, mu::RangeIndexPtrConst RangeIndex )
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
		MutableParameters->SetIntValue(ParamIndex, RealValue,RangeIndex);
		
		// Update the text combo if any
		if (Combo)
		{
			const FString Text = MutableParameters->GetIntPossibleValueName(ParamIndex, InValue);
			Combo->RefreshOptions();
			Combo->SetSelectedItem(MakeShared<FString>(Text), ESelectInfo::Direct);
		}
		
		OnParametersValueChanged.ExecuteIfBound(ParamIndex);
		
		ScheduleUpdateIfRequired(ParamIndex);
	}
}


void SMutableParametersWidget::OnIntParameterTextChanged(TSharedPtr<FString> Selection, ESelectInfo::Type, int32 ParamIndex, mu::RangeIndexPtrConst RangeIndex)
{
	if (!Selection
		|| !MutableParameters
		|| ParamIndex >= MutableParameters->GetCount()
		|| MutableParameters->GetType(ParamIndex) != mu::PARAMETER_TYPE::T_INT)
	{
		return;
	}

	int32 ValueIndex = MutableParameters->GetIntValueIndex(ParamIndex, StringCast<ANSICHAR>(**Selection).Get());
	if (ValueIndex >= 0 && ValueIndex < MutableParameters->GetIntPossibleValueCount(ParamIndex))
	{
		int32 RealValue = MutableParameters->GetIntPossibleValue(ParamIndex, ValueIndex);
		MutableParameters->SetIntValue(ParamIndex, RealValue,RangeIndex);
		
		OnParametersValueChanged.ExecuteIfBound(ParamIndex);
		
		ScheduleUpdateIfRequired(ParamIndex);
	}
}


TSharedRef<SWidget> SMutableParametersWidget::OnGenerateWidgetIntParameter(TSharedPtr<FString> InItem) const
{
	return SNew(STextBlock).Text(FText::FromString(*InItem.Get()));
}


FLinearColor SMutableParametersWidget::GetColorParameterValue(int32 ParamIndex, mu::RangeIndexPtrConst RangeIndex) const
{
	if (!MutableParameters
		|| ParamIndex >= MutableParameters->GetCount()
		|| MutableParameters->GetType(ParamIndex) != mu::PARAMETER_TYPE::T_COLOUR)
	{
		return FLinearColor();
	}

	FLinearColor Result;
	MutableParameters->GetColourValue(ParamIndex, &Result.R, &Result.G, &Result.B, &Result.A, RangeIndex);

	return Result;
}


FReply SMutableParametersWidget::OnColorBlockMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, int32 ParamIndex, mu::RangeIndexPtrConst RangeIndex)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}

	FLinearColor Col = GetColorParameterValue(ParamIndex,RangeIndex);

	FColorPickerArgs args;
	args.bIsModal = true;
	args.bUseAlpha = true;
	args.bOnlyRefreshOnMouseUp = false;
	args.InitialColor = Col;
	args.OnColorCommitted = FOnLinearColorValueChanged::CreateSP(this, &SMutableParametersWidget::OnSetColorFromColorPicker, ParamIndex, RangeIndex);
	OpenColorPicker(args);

	return FReply::Handled();
}


void SMutableParametersWidget::OnSetColorFromColorPicker(FLinearColor NewColor, int32 ParamIndex,  mu::RangeIndexPtrConst RangeIndex)
{
	if (!MutableParameters
		|| ParamIndex >= MutableParameters->GetCount()
		|| MutableParameters->GetType(ParamIndex) != mu::PARAMETER_TYPE::T_COLOUR)
	{
		return;
	}
	
	MutableParameters->SetColourValue(ParamIndex, NewColor.R, NewColor.G, NewColor.B, NewColor.A, RangeIndex);

	OnParametersValueChanged.ExecuteIfBound(ParamIndex);
}


TOptional<FVector::FReal> SMutableParametersWidget::GetProjectorLocation(EAxis::Type Axis, int32 ParamIndex, mu::RangeIndexPtrConst RangeIndex) const
{ 
	if (!MutableParameters
		|| ParamIndex >= MutableParameters->GetCount()
		|| MutableParameters->GetType(ParamIndex) != mu::PARAMETER_TYPE::T_PROJECTOR)
	{
		return 0.0f;
	}
	
	mu::PROJECTOR_TYPE Type;
	FVector3f Pos;
	FVector3f Dir;
	FVector3f Up;
	FVector3f Scale;
	float Angle;
	MutableParameters->GetProjectorValue(ParamIndex, &Type, &Pos, &Dir, &Up, &Scale, &Angle , RangeIndex);

	return Pos[ Axis-EAxis::X ];
}


void SMutableParametersWidget::SetProjectorLocation(FVector::FReal NewValue, ETextCommit::Type, EAxis::Type Axis, bool bCommitted, int32 ParamIndex, mu::RangeIndexPtrConst RangeIndex)
{
	if (!MutableParameters
		|| ParamIndex >= MutableParameters->GetCount()
		|| MutableParameters->GetType(ParamIndex) != mu::PARAMETER_TYPE::T_PROJECTOR)
	{
		return;
	}
	
	//mu::RangeIndexPtr UpdatedRange = RangeIndexPayload.GetRangeIndex();
	
	mu::PROJECTOR_TYPE Type;
	FVector3f Pos;
	FVector3f Dir;
	FVector3f Up;
	FVector3f Scale;
	float Angle;
	MutableParameters->GetProjectorValue(ParamIndex, &Type, &Pos, &Dir, &Up, &Scale, &Angle, RangeIndex);

	Pos[Axis - EAxis::X] = NewValue;

	MutableParameters->SetProjectorValue(ParamIndex, Pos, Dir, Up, Scale, Angle, RangeIndex);

	OnParametersValueChanged.ExecuteIfBound(ParamIndex);
}


TOptional<FVector::FReal> SMutableParametersWidget::GetProjectorScale(EAxis::Type Axis, int32 ParamIndex, mu::RangeIndexPtrConst RangeIndex) const
{
	if (!MutableParameters
		|| ParamIndex >= MutableParameters->GetCount()
		|| MutableParameters->GetType(ParamIndex) != mu::PARAMETER_TYPE::T_PROJECTOR)
	{
		return 0.0f;
	}

	mu::PROJECTOR_TYPE Type;
	FVector3f Pos;
	FVector3f Dir;
	FVector3f Up;
	FVector3f Scale;
	float Angle;
	MutableParameters->GetProjectorValue(ParamIndex, &Type, &Pos, &Dir, &Up, &Scale, &Angle, RangeIndex);

	return Scale[Axis - EAxis::X];
}


void SMutableParametersWidget::SetProjectorScale(FVector::FReal NewValue, ETextCommit::Type, EAxis::Type Axis, bool bCommitted, int32 ParamIndex,  mu::RangeIndexPtrConst RangeIndex)
{
	if (!MutableParameters
		|| ParamIndex >= MutableParameters->GetCount()
		|| MutableParameters->GetType(ParamIndex) != mu::PARAMETER_TYPE::T_PROJECTOR)
	{
		return;
	}

	
	mu::PROJECTOR_TYPE Type;
	FVector3f Pos;
	FVector3f Dir;
	FVector3f Up;
	FVector3f Scale;
	float Angle;
	MutableParameters->GetProjectorValue(ParamIndex, &Type, &Pos, &Dir, &Up, &Scale, &Angle, RangeIndex);

	Scale[Axis - EAxis::X] = NewValue;

	MutableParameters->SetProjectorValue(ParamIndex, Pos, Dir, Up, Scale, Angle, RangeIndex);

	OnParametersValueChanged.ExecuteIfBound(ParamIndex);
}


#undef LOCTEXT_NAMESPACE

