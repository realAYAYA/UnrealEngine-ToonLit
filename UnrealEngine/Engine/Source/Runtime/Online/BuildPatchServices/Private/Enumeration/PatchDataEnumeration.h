// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "Interfaces/IPatchDataEnumeration.h"
#include "BuildPatchSettings.h"

namespace BuildPatchServices
{
	class FPatchDataEnumerationFactory
	{
	public:
		static IPatchDataEnumeration* Create(const FPatchDataEnumerationConfiguration& Configuration);
	};
}
