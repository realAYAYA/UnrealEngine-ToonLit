// Copyright Epic Games, Inc. All Rights Reserved.

#include "TAssValueName.h"

#include <stdio.h>
#include <string.h>
#include <stdexcept>

BEGIN_NAMESPACE_UE_AC

const utf8_t* FAssValueName::GetName(const SAssValueName InAssValueName[], int InValue, EThrow InThrowInvalid)
{
	for (const SAssValueName* AssValueName = InAssValueName; AssValueName->Name; AssValueName++)
		if (AssValueName->Value == InValue)
		{
			return AssValueName->Name;
		}
	if (InThrowInvalid == kThrowInvalid)
	{
		throw std::domain_error("FAssValueName::GetName - Invalid value");
	}
	static char buffer[64] = {};
	snprintf(buffer, sizeof(buffer), "Unknown value %d", InValue);
	return buffer;
}

int FAssValueName::GetValue(const SAssValueName InAssValueName[], const utf8_t* InName, EThrow InThrowInvalid)
{
	const SAssValueName* AssValueName = InAssValueName - 1;
	while ((++AssValueName)->Name)
		if (strcmp(AssValueName->Name, InName) == 0)
		{
			break;
		}
	if (AssValueName->Name == nullptr && InThrowInvalid == kThrowInvalid)
	{
		throw std::domain_error("FAssValueName::GetValue - Invalid name");
	}
	return AssValueName->Value;
}

END_NAMESPACE_UE_AC
