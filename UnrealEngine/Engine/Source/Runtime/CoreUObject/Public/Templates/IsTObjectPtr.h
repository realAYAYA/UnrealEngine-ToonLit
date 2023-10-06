// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

template <typename T>
struct TObjectPtr;

template <typename T>
struct TIsTObjectPtr
{
	enum { Value = false };
};

template <typename T> struct TIsTObjectPtr<               TObjectPtr<T>> { enum { Value = true }; };
template <typename T> struct TIsTObjectPtr<const          TObjectPtr<T>> { enum { Value = true }; };
template <typename T> struct TIsTObjectPtr<      volatile TObjectPtr<T>> { enum { Value = true }; };
template <typename T> struct TIsTObjectPtr<const volatile TObjectPtr<T>> { enum { Value = true }; };
