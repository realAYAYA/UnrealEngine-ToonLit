// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace UE
{
	namespace Slate
	{
		class FontUtils
		{
		public:
			static bool IsAscentDescentOverrideEnabled(const TObjectPtr<UObject const> FontObject);
		};
	}
}