// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "CoreMinimal.h"

namespace UE::Interchange
{
	/**
	 * Replaces any unsupported characters with "_" character, and removes namespace indicator ":" character
	 * #todo_interchange: Replace with SanitizeObjectName from InterchangeManager.cpp
	 */
	INTERCHANGECORE_API FString MakeName(const FString& InName, bool bIsJoint = false);

	class FScopedLambda
	{
	public:
		FScopedLambda(TFunction<void(void)>&& Lambda)
			: _Lambda(Lambda)
		{}

		~FScopedLambda()
		{
			if (_Lambda)
			{
				_Lambda();
			}
		}

	private:
		TFunction<void(void)> _Lambda;
	};
};