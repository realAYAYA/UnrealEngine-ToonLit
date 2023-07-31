// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

enum class ERigNameOp : uint8
{
	None,
	Concat,
	Left,
    Right,
    LeftChop,
	RightChop,
	ReplaceCase,
	ReplaceNoCase,
	EndsWithCase,
	EndsWithNoCase,
    StartsWithCase,
    StartsWithNoCase,
    ContainsCase,
    ContainsNoCase
};

struct CONTROLRIG_API FRigNameOp
{
public:

	FORCEINLINE FRigNameOp()
	: A(INDEX_NONE)
	, B(INDEX_NONE)
	, C(INDEX_NONE)
	, Type(ERigNameOp::None)
	{}

	FORCEINLINE static uint32 GetTypeHash(const FName& InName)
	{
		return HashCombine(InName.GetComparisonIndex().ToUnstableInt(), uint32(InName.GetNumber()));
	}
	
	static FRigNameOp Concat(const FName& InA, const FName& InB);
	static FRigNameOp Left(const FName& InA, const uint32 InCount);
	static FRigNameOp Right(const FName& InA, const uint32 InCount);
	static FRigNameOp LeftChop(const FName& InA, const uint32 InCount);
	static FRigNameOp RightChop(const FName& InA, const uint32 InCount);
	static FRigNameOp Replace(const FName& InA, const FName& InB, const FName& InC, const ESearchCase::Type InSearchCase);
    static FRigNameOp EndsWith(const FName& InA, const FName& InB, const ESearchCase::Type InSearchCase);
    static FRigNameOp StartsWith(const FName& InA, const FName& InB, const ESearchCase::Type InSearchCase);
    static FRigNameOp Contains(const FName& InA, const FName& InB, const ESearchCase::Type InSearchCase);

	friend FORCEINLINE uint32 GetTypeHash(const FRigNameOp& Op)
	{
		return HashCombine(Op.A, HashCombine(Op.B, HashCombine(Op.C, (uint32)Op.Type)));
	}

	FORCEINLINE bool operator ==(const FRigNameOp& Other) const
	{
		return Type == Other.Type && A == Other.A && B == Other.B && C == Other.C;
	}

	FORCEINLINE bool operator !=(const FRigNameOp& Other) const
	{
		return Type != Other.Type || A != Other.A || B != Other.B || C != Other.C;
	}

	FORCEINLINE bool operator <(const FRigNameOp& Other) const
	{
		if (Type < Other.Type)
		{
			return true;
		}
		if(A < Other.A)
		{
			return true;
		}
		if(B < Other.B)
		{
			return true;
		}
		return C < Other.C;
	}

	FORCEINLINE bool operator >(const FRigNameOp& Other) const
	{
		if (Type > Other.Type)
		{
			return true;
		}
		if(A > Other.A)
		{
			return true;
		}
		if(B > Other.B)
		{
			return true;
		}
		return C > Other.C;
	}


private:

	uint32 A;
	uint32 B;
	uint32 C;
	ERigNameOp Type;
};

class CONTROLRIG_API FRigNameCache
{
public:

	FORCEINLINE void Reset()
	{
		NameCache.Reset();
		BoolCache.Reset();
	}

	FName Concat(const FName& InA, const FName& InB);
	FName Left(const FName& InA, const uint32 InCount);
	FName Right(const FName& InA, const uint32 InCount);
	FName LeftChop(const FName& InA, const uint32 InCount);
	FName RightChop(const FName& InA, const uint32 InCount);
	FName Replace(const FName& InA, const FName& InB, const FName& InC, const ESearchCase::Type InSearchCase);
	bool EndsWith(const FName& InA, const FName& InB, const ESearchCase::Type InSearchCase);
	bool StartsWith(const FName& InA, const FName& InB, const ESearchCase::Type InSearchCase);
	bool Contains(const FName& InA, const FName& InB, const ESearchCase::Type InSearchCase);

	FORCEINLINE const TMap<FRigNameOp, FName>& GetNameCache() const { return NameCache; }
	FORCEINLINE const TMap<FRigNameOp, bool>&  GetBoolCache() const { return BoolCache; }
	TArray<FRigNameOp> GetNameOps() const;
	TArray<FName> GetNameValues() const;
	TArray<FRigNameOp> GetBoolOps() const;
	TArray<bool> GetBoolValues() const;

private:

#if WITH_EDITOR
	bool CheckCacheSize() const;
#else
	FORCEINLINE bool CheckCacheSize() const { return true; } 
#endif

	TMap<FRigNameOp, FName> NameCache;
	TMap<FRigNameOp, bool> BoolCache;
};
