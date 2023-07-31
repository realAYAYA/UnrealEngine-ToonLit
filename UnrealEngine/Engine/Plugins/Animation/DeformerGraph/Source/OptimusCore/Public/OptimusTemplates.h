// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// FIXME: This should really be a part of Array.h
template<typename T, typename Allocator>
static inline uint32 GetTypeHash(const TArray<T, Allocator>& A)
{
	uint32 Hash = GetTypeHash(A.Num());
	for (const auto& V : A)
	{
		Hash = HashCombine(Hash, GetTypeHash(V));
	}
	return Hash;
}
