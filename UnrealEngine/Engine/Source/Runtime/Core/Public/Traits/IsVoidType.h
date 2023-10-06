// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// HEADER_UNIT_SKIP - Deprecated

template<typename T> struct TIsVoidTypeBase { enum { Value = false }; };
template<> struct TIsVoidTypeBase<void> { enum { Value = true }; };
template<> struct TIsVoidTypeBase<void const> { enum { Value = true }; };
template<> struct TIsVoidTypeBase<void volatile> { enum { Value = true }; };
template<> struct TIsVoidTypeBase<void const volatile> { enum { Value = true }; };

/**
 * TIsVoidType
 */
template<typename T> struct UE_DEPRECATED(5.2, "TIsVoidType has been deprecated, please use std::is_void instead.") TIsVoidType : TIsVoidTypeBase<T> {};
