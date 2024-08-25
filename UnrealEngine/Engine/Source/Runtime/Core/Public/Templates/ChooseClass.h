// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

/** Chooses between two different classes based on a boolean. */
template<bool Predicate,typename TrueClass,typename FalseClass>
class TChooseClass;

template<typename TrueClass,typename FalseClass>
class UE_DEPRECATED(5.4, "TChooseClass has been deprecated, please use std::conditional instead.") TChooseClass<true,TrueClass,FalseClass>
{
public:
	typedef TrueClass Result;
};

template<typename TrueClass,typename FalseClass>
class UE_DEPRECATED(5.4, "TChooseClass has been deprecated, please use std::conditional instead.") TChooseClass<false,TrueClass,FalseClass>
{
public:
	typedef FalseClass Result;
};
