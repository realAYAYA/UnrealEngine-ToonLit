// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"

namespace UE::FieldNotification::Helpers
{
	[[nodiscard]] FORCEINLINE bool IsValidAsField(const UFunction* InFunction)
	{
		return InFunction != nullptr
			&& !InFunction->HasAnyFunctionFlags(FUNC_Net | FUNC_Event)
			&& InFunction->HasAllFunctionFlags(FUNC_BlueprintCallable | FUNC_Const)
			&& InFunction->NumParms == 1
			&& InFunction->GetReturnProperty() != nullptr;
	}

} //namespace
