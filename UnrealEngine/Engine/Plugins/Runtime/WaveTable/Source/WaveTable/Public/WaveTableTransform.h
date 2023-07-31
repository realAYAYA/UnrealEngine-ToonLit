// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Curves/CurveFloat.h"
#include "Curves/RichCurve.h"
#include "HAL/Platform.h"
#include "Templates/Function.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "WaveTableSettings.h"

#include "WaveTableTransform.generated.h"


USTRUCT(BlueprintType)
struct WAVETABLE_API FWaveTableTransform
{
	GENERATED_USTRUCT_BODY()

	/** The curve to apply when transforming the output. */
	UPROPERTY(EditAnywhere, Category = Input, BlueprintReadWrite, meta = (DisplayName = "Curve Type"))
	EWaveTableCurve Curve = EWaveTableCurve::Linear;

	/** When curve set to log, exponential or exponential inverse, value is factor 'b' in following equations with output 'y' and input 'x':
	 *  Exponential: y = x * 10^-b(1-x)
	 *  Exponential (Inverse): y = ((x - 1) * 10^(-bx)) + 1
	 *  Logarithmic: y = b * log(x) + 1
	 */
	UPROPERTY(EditAnywhere, Category = Input, BlueprintReadWrite, meta = (DisplayName = "Exponential Scalar", ClampMin = "0.1", ClampMax = "10.0", UIMin = "0.1", UIMax = "10.0"))
	float Scalar = 2.5;

	/** Custom curve to apply if output curve type is set to 'Custom.' */
	UPROPERTY()
	FRichCurve CurveCustom;

	/** Asset curve reference to apply if output curve type is set to 'Shared.' */
	UPROPERTY(EditAnywhere, meta = (DisplayName = "Asset"), Category = Curve, BlueprintReadWrite)
	TObjectPtr<UCurveFloat> CurveShared = nullptr;

	UPROPERTY()
	TArray<float> WaveTable;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = WaveTable)
	FWaveTableSettings WaveTableSettings;
#endif // WITH_EDITORONLY_DATA

	// Applies transform to provided index value, setting it to the corresponding value.
	void Apply(float& InOutValue, bool bInBipolar = false) const;

#if WITH_EDITOR
	// Copies a single, period [0.0, 1.0] to a WaveTable using this transform's curve. The
	// size of the resulting WaveTable is the provided ArrayView's size. If curve is itself
	// set to 'File' and internally cached as WaveTable, will re-sample using max value
	// interpolation. Does *not* apply destructive edits for using WaveTableSettings PCM data.
	void CopyToWaveTable(TArrayView<float> InOutTable, bool bInBipolar) const;

	// Caches WaveTable for curve using the provided array length as resolution.
	// Applies destructive edits as provided by WaveTableSettings if curve set to 'File'.
	void CreateWaveTable(TArray<float>& InOutTable, bool bInBipolar) const;
#endif // WITH_EDITOR

	/** Caches curve data.  Should a shared curve be selected, curve points are copied locally
	  * to CurveCustom and Curve type is set accordingly.  Can be used to both avoid keeping a
	  * uobject reference should the transform be used on a non-game thread and potentially to
	  * further customize a referenced curve locally.
	  */
	void CacheCurve();

private:
	/** Clamps & applies transform to provided values as bipolar signal*/
	void SampleCurveBipolar(TArrayView<float> InOutValues) const;

	/** Clamps & applies transform to provided values as unipolar signal */
	void SampleCurveUnipolar(TArrayView<float> InOutValues) const;
};
