// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Parameters.h"
#include "MuR/Ptr.h"
#include "Templates/SharedPointer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"

enum class ECheckBoxState : uint8;
namespace ESelectInfo { enum Type : int; }
namespace ETextCommit { enum Type : int; }

class FString;
class SSearchableComboBox;
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

	/** Map from ParamIndexInObject to the int param's selector options */
	TMap<int32, TSharedPtr<TArray<TSharedPtr<FString>>>> IntParameterOptions;

	/** Check if the Parameter index provided is from a parameter that controls the amount of values another parameter/s
	 * should have exposed in the UI. If the parameter does so then the redraw of the parameters widget will be scheduled
	 * for the next update.
	 * @param InParameterIndex The index of the parameter we changed it's value and want to check if that should or not
	 * trigger the Parameter's Widget redraw.
	 */
	void ScheduleUpdateIfRequired(const int32& InParameterIndex);
	
	/** Parameter slate generation methods. */

	/** Generate a new slate for the provided parameter index and attaches it to the also provided Horizontal box slate.
	 * @param ParamIndex The index of the mutable parameter to represent using a slate object.
	 * @param ParameterHorizontalBox The Horizontal box slate object to contain the newly generated slate object representing the parameter.
	 * @param RangeIndex Range object pointing at the parameters' target dimension and position. Providing a nullptr is expected for parameters with
	 * a single value (not a multivalue parameter)
	 * This object can also represent single value parameters but must be provided in either case.
	 */
	void GenerateAndAttachParameterSlate(const int32 ParamIndex, TSharedPtr<SHorizontalBox> ParameterHorizontalBox, mu::RangeIndexPtrConst RangeIndex);
	
	/** Internal UI callbacks. */
	EVisibility GetParameterVisibility(int32 ParamIndex) const;
	ECheckBoxState GetBoolParameterValue(int32 ParamIndex, mu::RangeIndexPtrConst RangeIndex) const;
	void OnBoolParameterChanged(ECheckBoxState CheckboxState, int32 ParamIndex, mu::RangeIndexPtrConst RangeIndex);
	TOptional<float> GetFloatParameterValue(int32 ParamIndex, mu::RangeIndexPtrConst RangeIndex) const;
	void OnFloatParameterChanged(float Value, int32 ParamIndex, mu::RangeIndexPtrConst RangeIndex);
	void OnFloatParameterCommitted(float Value, ETextCommit::Type CommitType, int32 ParamIndex);
	FLinearColor GetColorParameterValue(int32 ParamIndex, mu::RangeIndexPtrConst RangeIndex) const;
	FReply OnColorBlockMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, int32 ParamIndex, mu::RangeIndexPtrConst RangeIndex);
	void OnSetColorFromColorPicker(FLinearColor NewColor, int32 ParamIndex, mu::RangeIndexPtrConst RangeIndex);
	TOptional<int32> GetIntParameterValue(int32 ParamIndex,  mu::RangeIndexPtrConst RangeIndex) const;
	TOptional<int32> GetIntParameterValueMax(int32 ParamIndex) const;
	void OnIntParameterChanged(int32 Value, int32 ParamIndex, TSharedPtr<SSearchableComboBox> Combo,  mu::RangeIndexPtrConst RangeIndex);
	void OnIntParameterTextChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo, int32 ParamIndex, mu::RangeIndexPtrConst RangeIndex);
	TSharedRef<SWidget> OnGenerateWidgetIntParameter(TSharedPtr<FString> InItem) const;

	/** Projector UI callbacks */
	TOptional<FVector::FReal> GetProjectorLocation(EAxis::Type Axis, int32 ParamIndex,  mu::RangeIndexPtrConst RangeIndex) const;
	void SetProjectorLocation(FVector::FReal NewValue, ETextCommit::Type, EAxis::Type, bool bCommitted, int32 ParamIndex,  mu::RangeIndexPtrConst RangeIndex);
	TOptional<FVector::FReal> GetProjectorScale(EAxis::Type Axis, int32 ParamIndex, mu::RangeIndexPtrConst RangeIndex) const;
	void SetProjectorScale(FVector::FReal NewValue, ETextCommit::Type, EAxis::Type, bool bCommitted, int32 ParamIndex, mu::RangeIndexPtrConst RangeIndex);

};
