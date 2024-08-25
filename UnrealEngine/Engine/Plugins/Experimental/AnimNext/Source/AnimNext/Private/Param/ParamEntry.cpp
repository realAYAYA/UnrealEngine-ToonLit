// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/ParamEntry.h"
#include "Param/ParamHelpers.h"
#include "Param/ParamUtils.h"
#include "Param/ParamResult.h"
#include "Param/ParamCompatibility.h"

namespace UE::AnimNext::Private
{

FParamEntry::FParamEntry(const FParamId& InId, const FParamTypeHandle& InTypeHandle, TArrayView<uint8> InData, bool bInIsReference, bool bInIsMutable)
	: Data(nullptr)
	, Id(InId)
	, TypeHandle(InTypeHandle)
	, Size(InData.Num())
	, Flags(EParamFlags::None)
{
	check(TypeHandle.IsValid());
	check(InData.Num() > 0 && InData.Num() < 0xffff);

	// If we can store our data inside of a ptr, we do
	if (!bInIsReference && InData.Num() <= sizeof(void*))
	{
		FParamHelpers::Copy(InTypeHandle, InData, TArrayView<uint8>(reinterpret_cast<uint8*>(&Data), sizeof(void*)));
		Flags |= EParamFlags::Embedded;
	}
	else
	{
		Data = InData.GetData();
	}

	if (bInIsReference)
	{
		Flags |= EParamFlags::Reference;
	}

	if (bInIsMutable)
	{
		Flags |= EParamFlags::Mutable;
	}
}

FParamEntry::~FParamEntry()
{
	if (Size > 0)
	{
		if (IsEmbedded())
		{
			FParamHelpers::Destroy(TypeHandle, TArrayView<uint8>(reinterpret_cast<uint8*>(&Data), sizeof(void*)));
		}
	}
}

FParamEntry::FParamEntry(const FParamEntry& InOtherParam)
	: Id(InOtherParam.Id)
	, TypeHandle(InOtherParam.TypeHandle)
	, Size(InOtherParam.Size)
	, Flags(InOtherParam.Flags)
{
	if (IsEmbedded())
	{
		FParamHelpers::Copy(InOtherParam.TypeHandle, TConstArrayView<uint8>(reinterpret_cast<const uint8*>(&InOtherParam.Data), sizeof(void*)), TArrayView<uint8>(reinterpret_cast<uint8*>(&Data), sizeof(void*)));
	}
	else
	{
		Data = InOtherParam.Data;
	}
}

FParamEntry& FParamEntry::operator=(const FParamEntry& InOtherParam)
{
	Id = InOtherParam.Id;
	TypeHandle = InOtherParam.TypeHandle;
	Size = InOtherParam.Size;
	Flags = InOtherParam.Flags;

	if (IsEmbedded())
	{
		FParamHelpers::Copy(InOtherParam.TypeHandle, TConstArrayView<uint8>(reinterpret_cast<const uint8*>(&InOtherParam.Data), sizeof(void*)), TArrayView<uint8>(reinterpret_cast<uint8*>(&Data), sizeof(void*)));
	}
	else
	{
		Data = InOtherParam.Data;
	}

	return *this;
}

FParamEntry::FParamEntry(FParamEntry&& InOtherParam) noexcept
	: Id(InOtherParam.Id)
	, TypeHandle(InOtherParam.TypeHandle)
	, Size(InOtherParam.Size)
	, Flags(InOtherParam.Flags)
{
	if (IsEmbedded())
	{
		FParamHelpers::Copy(InOtherParam.TypeHandle, TConstArrayView<uint8>(reinterpret_cast<const uint8*>(&InOtherParam.Data), sizeof(void*)), TArrayView<uint8>(reinterpret_cast<uint8*>(&Data), sizeof(void*)));
	}
	else
	{
		Data = InOtherParam.Data;
	}
}

FParamEntry& FParamEntry::operator=(FParamEntry&& InOtherParam) noexcept
{
	Id = InOtherParam.Id;
	TypeHandle = InOtherParam.TypeHandle;
	Size = InOtherParam.Size;
	Flags = InOtherParam.Flags;

	if (IsEmbedded())
	{
		FParamHelpers::Copy(InOtherParam.TypeHandle, TConstArrayView<uint8>(reinterpret_cast<const uint8*>(&InOtherParam.Data), sizeof(void*)), TArrayView<uint8>(reinterpret_cast<uint8*>(&Data), sizeof(void*)));
	}
	else
	{
		Data = InOtherParam.Data;
	}

	return *this;
}

FParamResult FParamEntry::GetParamData(FParamTypeHandle InTypeHandle, TConstArrayView<uint8>& OutParamData) const
{
	FParamCompatibility Compatibility = UE::AnimNext::FParamUtils::GetCompatibility(InTypeHandle, GetTypeHandle());
	if (!Compatibility.IsCompatible())
	{
		return EParamResult::TypeError;
	}

	OutParamData = GetData();
	return EParamResult::Success;
}

FParamResult FParamEntry::GetParamData(FParamTypeHandle InTypeHandle, TConstArrayView<uint8>& OutParamData, FParamTypeHandle& OutParamTypeHandle, FParamCompatibility InRequiredCompatibility) const
{
	OutParamTypeHandle = GetTypeHandle();

	FParamCompatibility Compatibility = UE::AnimNext::FParamUtils::GetCompatibility(InTypeHandle, OutParamTypeHandle);
	if (Compatibility < InRequiredCompatibility)
	{
		return EParamResult::TypeError;
	}

	OutParamData = GetData();

	if (Compatibility == InRequiredCompatibility)
	{
		return EParamResult::Success | EParamResult::TypeCompatible;
	}

	return EParamResult::Success;
}

FParamResult FParamEntry::GetMutableParamData(FParamTypeHandle InTypeHandle, TArrayView<uint8>& OutParamData)
{
	FParamResult AccessResult = EParamResult::Success;
	FParamCompatibility Compatibility = UE::AnimNext::FParamUtils::GetCompatibility(InTypeHandle, GetTypeHandle());
	if (!Compatibility.IsCompatible())
	{
		AccessResult.Result |= EParamResult::TypeError;
	}

	AccessResult.Result |= !IsMutable() ? EParamResult::MutabilityError : EParamResult::Success;
	if (AccessResult.IsSuccessful())
	{
		OutParamData = GetMutableData();
	}

	return AccessResult;
}


FParamResult FParamEntry::GetMutableParamData(FParamTypeHandle InTypeHandle, TArrayView<uint8>& OutParamData, FParamTypeHandle& OutParamTypeHandle, FParamCompatibility InRequiredCompatibility)
{
	OutParamTypeHandle = GetTypeHandle();

	FParamResult AccessResult = EParamResult::Success;
	FParamCompatibility Compatibility = UE::AnimNext::FParamUtils::GetCompatibility(InTypeHandle, OutParamTypeHandle);
	if (Compatibility < InRequiredCompatibility)
	{
		AccessResult.Result |= EParamResult::TypeError;
	}
	else if (Compatibility == InRequiredCompatibility)
	{
		AccessResult.Result |= EParamResult::TypeCompatible;
	}

	AccessResult.Result |= !IsMutable() ? EParamResult::MutabilityError : EParamResult::Success;
	if (AccessResult.IsSuccessful())
	{
		OutParamData = GetMutableData();
	}

	return AccessResult;
}

}