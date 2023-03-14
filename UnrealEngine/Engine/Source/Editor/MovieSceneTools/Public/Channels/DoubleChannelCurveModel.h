// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Channels/BezierChannelCurveModel.h"
#include "Channels/MovieSceneDoubleChannel.h"
#include "Containers/ArrayView.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"

class IBufferedCurveModel;
class ISequencer;
class UMovieSceneSection;
class UObject;
struct FKeyHandle;
struct FMovieSceneDoubleChannel;
struct FMovieSceneDoubleValue;
template <typename ChannelType> struct TMovieSceneChannelHandle;

class FDoubleChannelCurveModel : public FBezierChannelCurveModel<FMovieSceneDoubleChannel, FMovieSceneDoubleValue, double>
{
public:
	FDoubleChannelCurveModel(TMovieSceneChannelHandle<FMovieSceneDoubleChannel> InChannel, UMovieSceneSection* InOwningSection, TWeakPtr<ISequencer> InWeakSequencer);

	// FCurveModel
	virtual void CreateKeyProxies(TArrayView<const FKeyHandle> InKeyHandles, TArrayView<UObject*> OutObjects) override;
	virtual TUniquePtr<IBufferedCurveModel> CreateBufferedCurveCopy() const override;
};
