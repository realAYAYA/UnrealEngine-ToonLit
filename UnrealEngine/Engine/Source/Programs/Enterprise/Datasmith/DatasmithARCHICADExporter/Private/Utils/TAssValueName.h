// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AddonTools.h"

BEGIN_NAMESPACE_UE_AC

// Base class for value, name associations
class FAssValueName
{
  public:
	// Specificator to throw or not when value or name are invalid
	typedef enum
	{
		kDontThrow,
		kThrowInvalid
	} EThrow;

	// Association structure
	struct SAssValueName
	{
		int			  Value;
		const utf8_t* Name;
	};

	// Return the name for the name for the specified value
	static const utf8_t* GetName(const SAssValueName InAssValueName[], int InValue, EThrow InThrowInvalid = kDontThrow);

	// Return the value for the specified name
	static int GetValue(const SAssValueName InAssValueName[], const utf8_t* InName, EThrow InThrowInvalid = kDontThrow);
};

// Template class for specific enum type
template < class EnumType > class TAssEnumName : FAssValueName
{
  public:
	// Association table
	static SAssValueName AssEnumName[];

	// Return the name for the name for the specified value
	static const utf8_t* GetName(EnumType InValue, EThrow InThrowInvalid = kDontThrow)
	{
		return FAssValueName::GetName(AssEnumName, InValue, InThrowInvalid);
	}

	// Return the value for the specified name
	static EnumType GetValue(const utf8_t* InName, EThrow InThrowInvalid = kDontThrow)
	{
		return EnumType(FAssValueName::GetValue(AssEnumName, InName, InThrowInvalid));
	}
};

template < class EnumType > FAssValueName::SAssValueName TAssEnumName< EnumType >::AssEnumName[1];

// Macros to simplify usage with enum values
#define ValueName(v) \
	{                \
		v, #v        \
	}
#define EnumName(c, e) \
	{                  \
		c::e, #e       \
	}
#define EnumEnd(e) \
	{              \
		e, nullptr \
	}

END_NAMESPACE_UE_AC
