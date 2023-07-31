// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PropertyHandle.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/NumericTypeInterface.h"
#include "Math/Interval.h"


/**
 * SNumericEntryBox wrapper that supports custom Get/SetValue and dynamic value/UI range
 */
class MODELINGEDITORUI_API SDynamicNumericEntry : public SCompoundWidget
{
public:

	/**
	 * 
	 */
	struct FDataSource
	{
		TFunction<void(float, EPropertyValueSetFlags::Type)> SetValue;
		TFunction<float()> GetValue;
		TFunction<TInterval<float>()> GetValueRange;
		TFunction<TInterval<float>()> GetUIRange;
	};

public:

	SLATE_BEGIN_ARGS( SDynamicNumericEntry )
		: _MaxNumFloatDigits(2)
	{
	}

	SLATE_ARGUMENT( TSharedPtr<FDataSource>, Source )

	/** Maximum number of digits of precision that will be displayed for float slider */
	SLATE_ARGUMENT( int, MaxNumFloatDigits )

	SLATE_END_ARGS()


	/**
	* Construct this widget
	*
	* @param	InArgs	The declaration data for this widget
	*/
	void Construct( const FArguments& InArgs );

	static TSharedPtr<FDataSource> MakeSimpleDataSource(TSharedPtr<IPropertyHandle> PropertyHandle, TInterval<float> ValueRange, TInterval<float> UIRange);

protected:
	TSharedPtr<FDataSource> Source;
	TSharedPtr<SNumericEntryBox<float>> NumericEntry;
	TSharedPtr<TDefaultNumericTypeInterface<float>> TypeInterface;

	float LastSliderCommittedValue;
	void OnValueChanged(float NewValue);
	void OnValueCommitted(float NewValue, ETextCommit::Type CommitInfo);

	bool bIsUsingSlider = false;
	void OnBeginSliderMovement();
	void OnEndSliderMovement(float NewValue);
};
