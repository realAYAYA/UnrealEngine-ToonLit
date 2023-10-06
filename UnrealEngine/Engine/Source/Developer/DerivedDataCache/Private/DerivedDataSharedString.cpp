// Copyright Epic Games, Inc. All Rights Reserved.

#include "DerivedDataSharedString.h"

#include "Misc/StringBuilder.h"
#include "Serialization/CompactBinary.h"
#include "Serialization/CompactBinarySerialization.h"

namespace UE::DerivedData
{

bool LoadFromCompactBinary(FCbFieldView Field, FUtf8SharedString& OutString)
{
	OutString = Field.AsString();
	return !Field.HasError();
}

bool LoadFromCompactBinary(FCbFieldView Field, FWideSharedString& OutString)
{
	TWideStringBuilder<512> String;
	if (LoadFromCompactBinary(Field, String))
	{
		OutString = String;
		return true;
	}
	OutString.Reset();
	return false;
}

} // UE::DerivedData
