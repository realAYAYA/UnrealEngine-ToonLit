// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MaterialShared.h: Shared material definitions.
=============================================================================*/

#pragma once

template<typename T>
struct TMaterialRecursionGuard
{
	inline TMaterialRecursionGuard() = default;
	inline TMaterialRecursionGuard(const TMaterialRecursionGuard& Parent)
		: Value(nullptr)
		, PreviousLink(&Parent)
	{
	}

	inline void Set(const T* InValue)
	{
		check(Value == nullptr);
		Value = InValue;
	}

	inline bool Contains(const T* InValue)
	{
		TMaterialRecursionGuard const* Link = this;
		do
		{
			if (Link->Value == InValue)
			{
				return true;
			}
			Link = Link->PreviousLink;
		} while (Link);
		return false;
	}

private:
	const T* Value = nullptr;
	TMaterialRecursionGuard const* PreviousLink = nullptr;
};
