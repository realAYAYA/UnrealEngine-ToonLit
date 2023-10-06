// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

class UScriptStruct;

template <typename T>
struct TBaseStructure;

/**
 * Describes a type for which TBaseStructure<T>::Get() returning a UScriptStruct* is defined.
 */
struct CBaseStructureProvider
{
	template <typename T>
	auto Requires(UScriptStruct*& StructRef) -> decltype(
		StructRef = TBaseStructure<T>::Get()
	);
};
