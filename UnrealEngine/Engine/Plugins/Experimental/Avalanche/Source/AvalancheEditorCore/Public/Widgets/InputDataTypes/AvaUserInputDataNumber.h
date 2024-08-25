// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/InputDataTypes/AvaUserInputDataTypeBase.h"
#include "Misc/Optional.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBox.h"

template<typename InNumberType>
struct FAvaUserInputNumberData : public FAvaUserInputDataTypeBase
{
	using ThisStruct = FAvaUserInputNumberData<InNumberType>;

	FAvaUserInputNumberData(InNumberType InValue, TOptional<InNumberType> InMinValue = TOptional<InNumberType>(),
		TOptional<InNumberType> InMaxValue = TOptional<InNumberType>())
		: Value(InValue)
		, MinValue(InMinValue)
		, MaxValue(InMaxValue)
	{
	}

	InNumberType GetValue() const { return Value; }

	//~ Begin FAvaUserInputDataTypeBase
	virtual TSharedRef<SWidget> CreateInputWidget()
	{
		return SNew(SBox)
			.WidthOverride(200.f)
			.HAlign(HAlign_Fill)
			[
				SNew(SSpinBox<InNumberType>)
				.Value(Value)
				.MinValue(MinValue)
				.MaxSliderValue(MinValue)
				.MaxValue(MaxValue)
				.MaxSliderValue(MaxValue)
				.EnableSlider(true)
				.OnValueChanged(this, &ThisStruct::OnValueChanged)
				.OnValueCommitted(this, &ThisStruct::OnValueCommitted)
			];
	}
	//~ End FAvaUserInputDataTypeBase

protected:
	InNumberType Value;
	TOptional<InNumberType> MinValue;
	TOptional<InNumberType> MaxValue;

	void OnValueChanged(InNumberType InValue)
	{
		Value = InValue;
	}

	void OnValueCommitted(InNumberType InValue, ETextCommit::Type InCommitType)
	{
		Value = InValue;
	}
};