// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EntitySystem/MovieScenePropertyMetaDataTraits.h"
#include "EntitySystem/MovieScenePropertyRegistry.h"
#include "EntitySystem/MovieSceneComponentRegistry.h"

namespace UE
{
namespace MovieScene
{


template<typename ...MetaDataTypes, int ...Indices>
void TPropertyMetaDataComponentsImpl<TIntegerSequence<int, Indices...>, TPropertyMetaData<MetaDataTypes...>>::Initialize(FComponentRegistry* ComponentRegistry, MakeTCHARPtr<MetaDataTypes>... DebugNames)
{
	int Tmp[] = {
		(ComponentRegistry->NewComponentType(static_cast<TComponentTypeID<MetaDataTypes>*>(&Values[Indices]), DebugNames, EComponentTypeFlags::CopyToOutput), 0)..., 0
	};
	(void)Tmp;
}


} // namespace MovieScene
} // namespace UE


