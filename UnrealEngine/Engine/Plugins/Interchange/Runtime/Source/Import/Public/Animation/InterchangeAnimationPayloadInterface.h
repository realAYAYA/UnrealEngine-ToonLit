// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Animation/InterchangeAnimationPayload.h"
#include "Misc/FrameRate.h"
#include "UObject/Interface.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "InterchangeAnimationPayloadInterface.generated.h"

UINTERFACE()
class INTERCHANGEIMPORT_API UInterchangeAnimationPayloadInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Animation payload interface. Derive from this interface if your payload can import skeletal mesh
 */
class INTERCHANGEIMPORT_API IInterchangeAnimationPayloadInterface
{
	GENERATED_BODY()
public:

	/**
	 * Get animation payload data for the specified payload key. It return an array of FRichCurve (Rich curve are float curve we can interpolate).
	 *
	 * @param PayLoadKey - The key to retrieve the a particular payload contain into the specified source data.
	 * @return - The resulting PayloadData as a TFuture point by the PayloadKey. The TOptional will not be set if there is an error retrieving the payload.
	 * 
	 */
	virtual TFuture<TOptional<UE::Interchange::FAnimationCurvePayloadData>> GetAnimationCurvePayloadData(const FString& PayLoadKey) const = 0;

	/**
	 * Get animation payload data for the specified payload key. It return an array of "Step" curve data (step curve are a combination of Key time and Key Value, no interpolation data)
	 *
	 * @param PayLoadKey - The key to retrieve the a particular payload contain into the specified source data.
	 * @return - The resulting PayloadData as a TFuture point by the PayloadKey. The TOptional will not be set if there is an error retrieving the payload.
	 *
	 */
	virtual TFuture<TOptional<UE::Interchange::FAnimationStepCurvePayloadData>> GetAnimationStepCurvePayloadData(const FString& PayLoadKey) const = 0;

	virtual TFuture<TOptional<UE::Interchange::FAnimationBakeTransformPayloadData>> GetAnimationBakeTransformPayloadData(const FString& PayLoadKey, const double BakeFrequency, const double RangeStartSecond, const double RangeStopSecond) const = 0;
};


