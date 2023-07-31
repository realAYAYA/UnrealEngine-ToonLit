// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"
#include "Input/Reply.h"
#include "Layout/Visibility.h"
#include "Math/Axis.h"
#include "Math/Color.h"
#include "Math/UnrealMathSSE.h"
#include "Math/Vector.h"
#include "Misc/Optional.h"
#include "MuR/Parameters.h"
#include "Styling/SlateTypes.h"
#include "Templates/SharedPointer.h"
#include "Types/SlateEnums.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FString;
class SMutableTextSearchBox;
class SVerticalBox;
struct FGeometry;
struct FPointerEvent;


/** Delegate that is executed when a parameter value changes */
DECLARE_DELEGATE_OneParam(FOnMutableParameterValueChanged, int32);


/**  This widget displays and allows edition of a set of parameter values for the lower level Mutable Parameters 
* object. It is meant to be used for tools and debugging and it doesn't have any knowledge of Customizable Objects
* and the additional parameter details stored there.
*/
class SMutableParametersWidget : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SMutableParametersWidget) {}

		/** Parameters to show and edit. */
		SLATE_ARGUMENT_DEFAULT(mu::ParametersPtr, Parameters) { nullptr };

		/** Called when any parameter value has changed, with the parameter index as argument.  */
		SLATE_EVENT(FOnMutableParameterValueChanged, OnParametersValueChanged)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// SWidget interface
	void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	// Own interface

	/** Set the image to show in the widget. */
	void SetParameters(const mu::ParametersPtr& InParameters);

private:

	/** Reference to the mutable parameters. */
	mu::ParametersPtr MutableParameters;

	/** Is true, the parameter object has changed and we need to update. */
	bool bIsPendingUpdate = false;

	/** Delegate called when the check box changes state */
	FOnMutableParameterValueChanged OnParametersValueChanged;

	/** Dynamically filled box with per-parameter widgets */
	TSharedPtr<SVerticalBox> ParamBox;

	/** Internal UI callbacks. */
	EVisibility GetParameterVisibility(int32 ParamIndex) const;
	ECheckBoxState GetBoolParameterValue(int32 ParamIndex) const;
	void OnBoolParameterChanged(ECheckBoxState CheckboxState, int32 ParamIndex);
	TOptional<float> GetFloatParameterValue(int32 ParamIndex) const;
	void OnFloatParameterChanged(float Value, int32 ParamIndex);
	FLinearColor GetColorParameterValue(int32 ParamIndex) const;
	FReply OnColorBlockMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, int32 ParamIndex);
	void OnSetColorFromColorPicker(FLinearColor NewColor, int32 ParamIndex);
	TOptional<int32> GetIntParameterValue(int32 ParamIndex) const;
	TOptional<int32> GetIntParameterValueMax(int32 ParamIndex) const;
	void OnIntParameterChanged(int32 Value, int32 ParamIndex, TSharedPtr<SMutableTextSearchBox> );
	void OnIntParameterTextChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo, int32 ParamIndex);

	/** Projector UI callbacks */
	TOptional<FVector::FReal> GetProjectorLocation(EAxis::Type Axis, int32 ParamIndex) const;
	void SetProjectorLocation(FVector::FReal NewValue, ETextCommit::Type, EAxis::Type, bool bCommitted, int32 ParamIndex);
	TOptional<FVector::FReal> GetProjectorScale(EAxis::Type Axis, int32 ParamIndex) const;
	void SetProjectorScale(FVector::FReal NewValue, ETextCommit::Type, EAxis::Type, bool bCommitted, int32 ParamIndex);


};

