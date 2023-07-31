// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/CString.h"
#include "Misc/GeneratedTypeName.h"
#include "Templates/RemoveReference.h"

#include "Online/OnlineMeta.h"

namespace UE::Online {

struct FOnlineTypeName
{
	const TCHAR* Name = nullptr;

	bool operator==(const FOnlineTypeName& Other) const
	{
		return FCString::Strcmp(Name, Other.Name) == 0;
	}

	bool operator!=(const FOnlineTypeName& Other) const
	{
		return !operator==(Other);
	}
};

inline uint32 GetTypeHash(const FOnlineTypeName& TypeName)
{
	return ::GetTypeHash(TypeName.Name);
}

template <typename T>
struct TOnlineTypeInfo
{
	static inline FOnlineTypeName GetTypeName()
	{
		return FOnlineTypeName{ GetGeneratedTypeName<std::remove_reference_t<T>>() };
	}

	static inline bool IsA(FOnlineTypeName TypeName)
	{
		if (TypeName == GetTypeName())
		{
			return true;
		}

		if constexpr (TModels<Meta::CSuperDefined, T>::Value)
		{
			return TOnlineTypeInfo<typename T::Super>::IsA(TypeName);
		}

		return false;
	}
};

/* UE::Online */ }
