// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedStorageID.h"
#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedStateExtension.h"

namespace UE
{
namespace MovieScene
{


template<typename StorageType>
TAutoRegisterPreAnimatedStorageID<StorageType>::TAutoRegisterPreAnimatedStorageID()
	: TPreAnimatedStorageID<StorageType>{ FPreAnimatedStateExtension::template RegisterStorage<StorageType>() }
{}


} // namespace MovieScene
} // namespace UE
