// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Async/Future.h"
#include "CoreMinimal.h"
#include "Misc/FrameRate.h"
#include "UObject/Interface.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "InterchangeAnimationTrackSetNode.h"
#include "InterchangeCommonAnimationPayload.h"

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
	 * Get animation payload data for the specified payload key.
	 * It return an array of FRichCurve (Rich curve are float curve we can interpolate) or array of "Step" curve or an array of Baked Transformations, depending on Type
	 *
	 * @param PayLoadKey.UniqueId - The key to retrieve the a particular payload contain into the specified source data.
	 * @param PayLoadKey.Type - The type of animation data to be retrieved (Type is provided by the Translator upon initialization of PayLoadKeys)
	 * @return - The resulting PayloadData as a TFuture point by the PayloadKey. The TOptional will not be set if there is an error retrieving the payload.
	 * 
	 */
	virtual TFuture<TOptional<UE::Interchange::FAnimationPayloadData>> GetAnimationPayloadData(const FInterchangeAnimationPayLoadKey& PayLoadKey, const double BakeFrequency = 0, const double RangeStartSecond = 0, const double RangeStopSecond = 0) const = 0;
};


