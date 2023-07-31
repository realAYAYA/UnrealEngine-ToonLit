// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "MVVM/ViewModelTypeID.h"
#include "Curves/KeyHandle.h"
#include "Misc/FrameTime.h"

struct FGuid;
struct FPointerEvent;
struct FKeyDrawParams;
struct FGeometry;
class FCurveModel;

namespace UE::Sequencer
{

struct FCachedKeys
{
	/** Handles to each of the keys */
	TArray<FKeyHandle> KeyHandles;
	/** Frame times for each of the keys in a data-dependent time-space */
	TArray<FFrameTime> KeyTimes;

	struct FCachedKey
	{
		FFrameTime Time;
		FKeyHandle Handle = FKeyHandle::Invalid();

		bool IsValid() const;
		void Reset();
	};

	void GetKeysInRange(const TRange<FFrameTime>& Range, TArrayView<const FFrameTime>* OutKeyTimes, TArrayView<const FKeyHandle>* OutHandles) const;
	void GetKeysInRangeWithBounds(const TRange<FFrameTime>& Range, TArrayView<const FFrameTime>* OutKeyTimes, TArrayView<const FKeyHandle>* OutHandles, FCachedKey* OutLeadingKey, FCachedKey* OutTrailingKey) const;
};

class SEQUENCERCORE_API IKeyExtension
{
public:

	UE_SEQUENCER_DECLARE_VIEW_MODEL_TYPE_ID(IKeyExtension)


	virtual ~IKeyExtension(){}

	virtual bool UpdateCachedKeys(TSharedPtr<FCachedKeys>& OutCachedKeys) const = 0;

	virtual bool GetFixedExtents(double& OutFixedMin, double& OutFixedMax) const = 0;

	virtual void DrawKeys(TArrayView<const FKeyHandle> InKeyHandles, TArrayView<FKeyDrawParams> OutKeyDrawParams) = 0;

	virtual TUniquePtr<FCurveModel> CreateCurveModel() = 0;
};



} // namespace UE::Sequencer

