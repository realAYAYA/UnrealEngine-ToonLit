// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

typedef uint16 FBoneIndexType;


struct FBoneIndexBase
{
	FBoneIndexBase() : BoneIndex(INDEX_NONE) {}

	FORCEINLINE int32 GetInt() const { return BoneIndex; }

	FORCEINLINE bool IsRootBone() const { return BoneIndex == 0; }

	FORCEINLINE bool IsValid() const { return BoneIndex != INDEX_NONE; }

	FORCEINLINE explicit operator int32() const { return BoneIndex; }

	FORCEINLINE explicit operator bool() const { return IsValid(); }

	friend FORCEINLINE uint32 GetTypeHash(const FBoneIndexBase& Index) { return GetTypeHash(Index.BoneIndex); }

protected:
	int32 BoneIndex;
};

FORCEINLINE int32 GetIntFromComp(const int32 InComp)
{
	return InComp;
}

FORCEINLINE int32 GetIntFromComp(const FBoneIndexBase& InComp)
{
	return InComp.GetInt();
}

template<class RealBoneIndexType>
struct FBoneIndexWithOperators : public FBoneIndexBase
{
	// BoneIndexType
	FORCEINLINE friend bool operator==(const RealBoneIndexType& Lhs, const RealBoneIndexType& Rhs)
	{
		return GetIntFromComp(Lhs) == GetIntFromComp(Rhs);
	}

#if !PLATFORM_COMPILER_HAS_GENERATED_COMPARISON_OPERATORS
	FORCEINLINE friend bool operator!=(const RealBoneIndexType& Lhs, const RealBoneIndexType& Rhs)
	{
		return GetIntFromComp(Lhs) != GetIntFromComp(Rhs);
	}
#endif // !PLATFORM_COMPILER_HAS_GENERATED_COMPARISON_OPERATORS

	FORCEINLINE friend bool operator>(const RealBoneIndexType& Lhs, const RealBoneIndexType& Rhs)
	{
		return GetIntFromComp(Lhs) > GetIntFromComp(Rhs);
	}

	FORCEINLINE friend bool operator>=(const RealBoneIndexType& Lhs, const RealBoneIndexType& Rhs)
	{
		return GetIntFromComp(Lhs) >= GetIntFromComp(Rhs);
	}

	FORCEINLINE friend bool operator<(const RealBoneIndexType& Lhs, const RealBoneIndexType& Rhs)
	{
		return GetIntFromComp(Lhs) < GetIntFromComp(Rhs);
	}

	FORCEINLINE friend bool operator<=(const RealBoneIndexType& Lhs, const RealBoneIndexType& Rhs)
	{
		return GetIntFromComp(Lhs) <= GetIntFromComp(Rhs);
	}

	// FBoneIndexType
	FORCEINLINE friend bool operator==(const RealBoneIndexType& Lhs, const int32 Rhs)
	{
		return GetIntFromComp(Lhs) == GetIntFromComp(Rhs);
	}

#if !PLATFORM_COMPILER_HAS_GENERATED_COMPARISON_OPERATORS
	FORCEINLINE friend bool operator!=(const RealBoneIndexType& Lhs, const int32 Rhs)
	{
		return GetIntFromComp(Lhs) != GetIntFromComp(Rhs);
	}
#endif // !PLATFORM_COMPILER_HAS_GENERATED_COMPARISON_OPERATORS

	FORCEINLINE friend bool operator>(const RealBoneIndexType& Lhs, const int32 Rhs)
	{
		return GetIntFromComp(Lhs) > GetIntFromComp(Rhs);
	}

	FORCEINLINE friend bool operator>=(const RealBoneIndexType& Lhs, const int32 Rhs)
	{
		return GetIntFromComp(Lhs) >= GetIntFromComp(Rhs);
	}

	FORCEINLINE friend bool operator<(const RealBoneIndexType& Lhs, const int32 Rhs)
	{
		return GetIntFromComp(Lhs) < GetIntFromComp(Rhs);
	}

	FORCEINLINE friend bool operator<=(const RealBoneIndexType& Lhs, const int32 Rhs)
	{
		return GetIntFromComp(Lhs) <= GetIntFromComp(Rhs);
	}

#if !PLATFORM_COMPILER_HAS_GENERATED_COMPARISON_OPERATORS
	FORCEINLINE friend bool operator==(const int32 Lhs, const RealBoneIndexType& Rhs)
	{
		return Rhs == Lhs;
	}
	
	FORCEINLINE friend bool operator!=(const int32 Lhs, const RealBoneIndexType& Rhs)
	{
		return Rhs != Lhs;
	}

	FORCEINLINE friend bool operator>(const int32 Lhs, const RealBoneIndexType& Rhs)
	{
		return Rhs < Lhs;
	}

	FORCEINLINE friend bool operator>=(const int32 Lhs, const RealBoneIndexType& Rhs)
	{
		return Rhs <= Lhs;
	}

	FORCEINLINE friend bool operator<(const int32 Lhs, const RealBoneIndexType& Rhs)
	{
		return Rhs > Lhs;
	}

	FORCEINLINE friend bool operator<=(const int32 Lhs, const RealBoneIndexType& Rhs)
	{
		return Rhs >= Lhs;
	}
#endif // !PLATFORM_COMPILER_HAS_GENERATED_COMPARISON_OPERATORS

	RealBoneIndexType& operator++()
	{
		++BoneIndex;
		return *((RealBoneIndexType*)this);
	}

	RealBoneIndexType& operator--()
	{
		--BoneIndex;
		return *((RealBoneIndexType*)this);
	}

	const RealBoneIndexType& operator=(const RealBoneIndexType& Rhs)
	{
		BoneIndex = Rhs.BoneIndex;
		return Rhs;
	}
};

// This represents a compact pose bone index. A compact pose is held by a bone container and can have a different ordering than either the skeleton or skeletal mesh.
struct FCompactPoseBoneIndex : public FBoneIndexWithOperators < FCompactPoseBoneIndex >
{
public:
	explicit FCompactPoseBoneIndex(int32 InBoneIndex) { BoneIndex = InBoneIndex; }
};

// This represents a skeletal mesh bone index which may differ from the skeleton bone index it corresponds to.
struct FMeshPoseBoneIndex : public FBoneIndexWithOperators < FMeshPoseBoneIndex >
{
public:
	explicit FMeshPoseBoneIndex(int32 InBoneIndex) { BoneIndex = InBoneIndex; }
};

// This represents a skeleton bone index which may differ from the skeletal mesh bone index it corresponds to.
struct FSkeletonPoseBoneIndex : public FBoneIndexWithOperators < FSkeletonPoseBoneIndex >
{
public:
	explicit FSkeletonPoseBoneIndex(int32 InBoneIndex) { BoneIndex = InBoneIndex; }
};

template <typename ValueType>
struct TCompactPoseBoneIndexMapKeyFuncs : public TDefaultMapKeyFuncs<const FCompactPoseBoneIndex, ValueType, false>
{
	static FORCEINLINE FCompactPoseBoneIndex			GetSetKey(TPair<FCompactPoseBoneIndex, ValueType> const& Element) { return Element.Key; }
	static FORCEINLINE uint32							GetKeyHash(FCompactPoseBoneIndex const& Key) { return GetTypeHash(Key.GetInt()); }
	static FORCEINLINE bool								Matches(FCompactPoseBoneIndex const& A, FCompactPoseBoneIndex const& B) { return (A.GetInt() == B.GetInt()); }
};