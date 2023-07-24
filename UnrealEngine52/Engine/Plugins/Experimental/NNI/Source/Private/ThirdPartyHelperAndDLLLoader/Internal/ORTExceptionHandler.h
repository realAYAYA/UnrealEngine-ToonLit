// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include <string>

class THIRDPARTYHELPERANDDLLLOADER_API FORTExceptionHandler
{
public:
	static void ThrowPseudoException(const std::string& InString, const int32 InCode);
};
