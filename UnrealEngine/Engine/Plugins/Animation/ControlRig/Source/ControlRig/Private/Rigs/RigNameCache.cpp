// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rigs/RigNameCache.h"

#if WITH_EDITOR
#include "ControlRig.h"
TAutoConsoleVariable<int32> CVarRigNameCacheMaxSize(TEXT("a.ControlRig.NameCacheMaxSize"), 512, TEXT("Change to control how many names are cached per rig instance."));
#endif

////////////////////////////////////////////////////////////////////////////////
// FRigNameOp
////////////////////////////////////////////////////////////////////////////////

FRigNameOp FRigNameOp::Concat(const FName& InA, const FName& InB)
{
	FRigNameOp Op;
	Op.Type = ERigNameOp::Concat;
	Op.A = GetTypeHash(InA);
	Op.B = GetTypeHash(InB);
	return Op;
}

FRigNameOp FRigNameOp::Left(const FName& InA, const uint32 InCount)
{
	FRigNameOp Op;
	Op.Type = ERigNameOp::Left;
	Op.A = GetTypeHash(InA);
	Op.B = InCount;
	return Op;
}

FRigNameOp FRigNameOp::Right(const FName& InA, const uint32 InCount)
{
	FRigNameOp Op;
	Op.Type = ERigNameOp::Right;
	Op.A = GetTypeHash(InA);
	Op.B = InCount;
	return Op;
}

FRigNameOp FRigNameOp::LeftChop(const FName& InA, const uint32 InCount)
{
	FRigNameOp Op;
	Op.Type = ERigNameOp::LeftChop;
	Op.A = GetTypeHash(InA);
	Op.B = InCount;
	return Op;
}

FRigNameOp FRigNameOp::RightChop(const FName& InA, const uint32 InCount)
{
	FRigNameOp Op;
	Op.Type = ERigNameOp::RightChop;
	Op.A = GetTypeHash(InA);
	Op.B = InCount;
	return Op;
}

FRigNameOp FRigNameOp::Replace(const FName& InA, const FName& InB, const FName& InC, const ESearchCase::Type InSearchCase)
{
	FRigNameOp Op;
	Op.Type = InSearchCase == ESearchCase::CaseSensitive ? ERigNameOp::ReplaceCase : ERigNameOp::ReplaceNoCase;
	Op.A = GetTypeHash(InA);
	Op.B = GetTypeHash(InB);
	Op.C = GetTypeHash(InC);
	return Op;
}

FRigNameOp FRigNameOp::EndsWith(const FName& InA, const FName& InB, const ESearchCase::Type InSearchCase)
{
	FRigNameOp Op;
	Op.Type = InSearchCase == ESearchCase::CaseSensitive ? ERigNameOp::EndsWithCase : ERigNameOp::EndsWithNoCase;
	Op.A = GetTypeHash(InA);
	Op.B = GetTypeHash(InB);
	return Op;
}

FRigNameOp FRigNameOp::StartsWith(const FName& InA, const FName& InB, const ESearchCase::Type InSearchCase)
{
	FRigNameOp Op;
	Op.Type = InSearchCase == ESearchCase::CaseSensitive ? ERigNameOp::StartsWithCase : ERigNameOp::StartsWithNoCase;
	Op.A = GetTypeHash(InA);
	Op.B = GetTypeHash(InB);
	return Op;
}

FRigNameOp FRigNameOp::Contains(const FName& InA, const FName& InB, const ESearchCase::Type InSearchCase)
{
	FRigNameOp Op;
	Op.Type = InSearchCase == ESearchCase::CaseSensitive ? ERigNameOp::ContainsCase : ERigNameOp::ContainsNoCase;
	Op.A = GetTypeHash(InA);
	Op.B = GetTypeHash(InB);
	return Op;
}

////////////////////////////////////////////////////////////////////////////////
// FRigNameCache
////////////////////////////////////////////////////////////////////////////////

FName FRigNameCache::Concat(const FName& InA, const FName& InB)
{
	if(InA.IsNone())
	{
		return InB;
	}

	if(InB.IsNone())
	{
		return InA;
	}
	
	const FRigNameOp Op = FRigNameOp::Concat(InA, InB);
	if(const FName* ExistingName = NameCache.Find(Op))
	{
		return *ExistingName;
	}
	const FName NewName = *(InA.ToString() + InB.ToString());

	if(CheckCacheSize())
	{
		NameCache.Add(Op, NewName);
	}
	return NewName;
}

FName FRigNameCache::Left(const FName& InA, const uint32 InCount)
{
	if(InCount == 0 || InA.IsNone())
	{
		return NAME_None;
	}

	if(InCount >= InA.GetStringLength())
	{
		return InA;
	}
	
	const FRigNameOp Op = FRigNameOp::Left(InA, InCount);
	if(const FName* ExistingName = NameCache.Find(Op))
	{
		return *ExistingName;
	}
	const FName NewName = *InA.ToString().Left(InCount);

	if(CheckCacheSize())
	{
		NameCache.Add(Op, NewName);
	}
	
	return NewName;
}

FName FRigNameCache::Right(const FName& InA, const uint32 InCount)
{
	if(InCount == 0 || InA.IsNone())
	{
		return NAME_None;
	}

	if(InCount >= InA.GetStringLength())
	{
		return InA;
	}

	const FRigNameOp Op = FRigNameOp::Right(InA, InCount);
	if(const FName* ExistingName = NameCache.Find(Op))
	{
		return *ExistingName;
	}
	const FName NewName = *InA.ToString().Right(InCount);

	if(CheckCacheSize())
	{
		NameCache.Add(Op, NewName);
	}
	
	return NewName;
}

FName FRigNameCache::LeftChop(const FName& InA, const uint32 InCount)
{
	if(InCount == 0 || InA.IsNone())
	{
		return InA;
	}
	
	const FRigNameOp Op = FRigNameOp::LeftChop(InA, InCount);
	if(const FName* ExistingName = NameCache.Find(Op))
	{
		return *ExistingName;
	}
	const FName NewName = *InA.ToString().LeftChop(InCount);

	if(CheckCacheSize())
	{
		NameCache.Add(Op, NewName);
	}
	
	return NewName;
}

FName FRigNameCache::RightChop(const FName& InA, const uint32 InCount)
{
	if(InCount == 0 || InA.IsNone())
	{
		return InA;
	}

	const FRigNameOp Op = FRigNameOp::RightChop(InA, InCount);
	if(const FName* ExistingName = NameCache.Find(Op))
	{
		return *ExistingName;
	}
	const FName NewName = *InA.ToString().RightChop(InCount);

	if(CheckCacheSize())
	{
		NameCache.Add(Op, NewName);
	}
	
	return NewName;
}

FName FRigNameCache::Replace(const FName& InA, const FName& InB, const FName& InC, const ESearchCase::Type InSearchCase)
{
	if(InA.IsNone() || InB.IsNone())
	{
		return InA;
	}

	const FRigNameOp Op = FRigNameOp::Replace(InA, InB, InC, InSearchCase);
	if(const FName* ExistingName = NameCache.Find(Op))
	{
		return *ExistingName;
	}

	FString NewString(TEXT(""));
	if (!InC.IsNone())
	{
		NewString = *InC.ToString();
	}
	const FName NewName = *InA.ToString().Replace(*InB.ToString(), *NewString, InSearchCase);

	if(CheckCacheSize())
	{
		NameCache.Add(Op, NewName);
	}
	
	return NewName;
}

bool FRigNameCache::EndsWith(const FName& InA, const FName& InB, const ESearchCase::Type InSearchCase)
{
	if(InA.IsNone() || InB.IsNone())
	{
		return false;
	}
	
	const FRigNameOp Op = FRigNameOp::EndsWith(InA, InB, InSearchCase);
	if(const bool* ExistingBool = BoolCache.Find(Op))
	{
		return *ExistingBool;
	}
	const bool bResult = InA.ToString().EndsWith(InB.ToString(), InSearchCase);

	if(CheckCacheSize())
	{
		BoolCache.Add(Op, bResult);
	}
	
	return bResult;
}

bool FRigNameCache::StartsWith(const FName& InA, const FName& InB, const ESearchCase::Type InSearchCase)
{
	if(InA.IsNone() || InB.IsNone())
	{
		return false;
	}

	const FRigNameOp Op = FRigNameOp::StartsWith(InA, InB, InSearchCase);
	if(const bool* ExistingBool = BoolCache.Find(Op))
	{
		return *ExistingBool;
	}
	const bool bResult = InA.ToString().StartsWith(InB.ToString(), InSearchCase);

	if(CheckCacheSize())
	{
		BoolCache.Add(Op, bResult);
	}
	
	return bResult;
}

bool FRigNameCache::Contains(const FName& InA, const FName& InB, const ESearchCase::Type InSearchCase)
{
	if(InA.IsNone() || InB.IsNone())
	{
		return false;
	}

	const FRigNameOp Op = FRigNameOp::Contains(InA, InB, InSearchCase);
	if(const bool* ExistingBool = BoolCache.Find(Op))
	{
		return *ExistingBool;
	}
	const bool bResult = InA.ToString().Contains(InB.ToString(), InSearchCase);

	if(CheckCacheSize())
	{
		BoolCache.Add(Op, bResult);
	}
	
	return bResult;
}

TArray<FRigNameOp> FRigNameCache::GetNameOps() const
{
	TArray<FRigNameOp> Result;
	NameCache.GenerateKeyArray(Result);
	return Result;
}

TArray<FName> FRigNameCache::GetNameValues() const
{
	TArray<FName> Result;
	NameCache.GenerateValueArray(Result);
	return Result;
}

TArray<FRigNameOp> FRigNameCache::GetBoolOps() const
{
	TArray<FRigNameOp> Result;
	BoolCache.GenerateKeyArray(Result);
	return Result;
}

TArray<bool> FRigNameCache::GetBoolValues() const
{
	TArray<bool> Result;
	BoolCache.GenerateValueArray(Result);
	return Result;
}

#if WITH_EDITOR
bool FRigNameCache::CheckCacheSize() const
{
	if(CVarRigNameCacheMaxSize.GetValueOnAnyThread() == NameCache.Num() || CVarRigNameCacheMaxSize.GetValueOnAnyThread() == BoolCache.Num())
	{
		UE_LOG(LogControlRig, Warning, TEXT("FRigNameCache exceeded maximum size of %d. You can change it using the 'a.ControlRig.NameCacheMaxSize' console variable."), CVarRigNameCacheMaxSize.GetValueOnAnyThread());
		return false;
	}
	return true;
}
#endif
