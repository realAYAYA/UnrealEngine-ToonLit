// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

struct FMovieSceneBindingProxy;

UE_DEPRECATED(5.1, "Use FMovieSceneBindingProxy instead.") 
typedef FMovieSceneBindingProxy FSequencerBindingProxy;

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "MovieSceneBindingProxy.h"
#include "UObject/ObjectMacros.h"
#endif
