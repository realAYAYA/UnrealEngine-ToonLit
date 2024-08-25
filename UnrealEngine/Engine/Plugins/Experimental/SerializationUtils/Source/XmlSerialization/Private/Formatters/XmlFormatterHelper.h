// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/Function.h"

class FProperty;
class FStructuredArchiveRecord;
class UObject;

namespace UE::XmlSerialization::Private::FormatterHelper
{
	void SerializeObject(FStructuredArchiveRecord& InRootRecord, UObject* InObject
		, const TFunction<bool(FProperty*)>& ShouldSkipProperty = TFunction<bool(FProperty*)>());
}