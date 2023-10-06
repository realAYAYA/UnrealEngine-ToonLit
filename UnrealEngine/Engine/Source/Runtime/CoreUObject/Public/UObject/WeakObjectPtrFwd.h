// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	WeakObjectPtrFwd.h: Weak object pointer forward declarations
=============================================================================*/

#pragma once

#include "UObject/WeakObjectPtrTemplatesFwd.h"

struct FWeakObjectPtr;

template <typename T> struct TIsPODType;
template <typename T> struct TIsWeakPointerType;
template <typename T> struct TIsZeroConstructType;

template<> struct TIsPODType<FWeakObjectPtr> { enum { Value = true }; };
template<> struct TIsZeroConstructType<FWeakObjectPtr> { enum { Value = true }; };
template<> struct TIsWeakPointerType<FWeakObjectPtr> { enum { Value = true }; };
