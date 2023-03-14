// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EntitySystem/MovieScenePropertyTraits.h"
#include "EntitySystem/MovieSceneEntityIDs.h"

namespace UE
{
namespace MovieScene
{

struct FComponentRegistry;

template<typename ...MetaDataTypes, int ...Indices>
struct TPropertyMetaDataComponentsImpl<TIntegerSequence<int, Indices...>, TPropertyMetaData<MetaDataTypes...>>
{
	template<typename T> using MakeTCHARPtr = const TCHAR*;

	TArrayView<const FComponentTypeID> GetTypes() const
	{
		return MakeArrayView(Values);
	}

	template<int Index>
	auto GetType() const
	{
		return MakeTuple(Values[Indices].template ReinterpretCast<MetaDataTypes>()...).template Get<Index>();
	}

	// #include "EntitySystem/MovieScenePropertyMetaDataTraits.inl" for definition
	//
	void Initialize(FComponentRegistry* ComponentRegistry, MakeTCHARPtr<MetaDataTypes>... DebugNames);

private:

	static constexpr int32 SIZE = sizeof...(MetaDataTypes);
	FComponentTypeID Values[SIZE];
};


template<typename ...MetaDataTypes>
struct TPropertyMetaDataComponents<TPropertyMetaData<MetaDataTypes...>> : TPropertyMetaDataComponentsImpl<TMakeIntegerSequence<int, sizeof...(MetaDataTypes)>, TPropertyMetaData<MetaDataTypes...>>
{};


template<>
struct TPropertyMetaDataComponents<TPropertyMetaData<>>
{
	TArrayView<const FComponentTypeID> GetTypes() const
	{
		return TArrayView<const FComponentTypeID>();
	}

	template<int Index> void GetType() const
	{
	}
};


} // namespace MovieScene
} // namespace UE


