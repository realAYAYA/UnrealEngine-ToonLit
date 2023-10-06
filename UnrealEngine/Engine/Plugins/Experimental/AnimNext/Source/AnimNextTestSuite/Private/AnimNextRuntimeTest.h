// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "UObject/Class.h"
#include "UObject/UnrealType.h"

#include "DecoratorBase/DecoratorUID.h"
#include "DecoratorBase/NodeTemplate.h"

namespace UE::AnimNext
{
	// Builds a node template from a list of decorator UID
	const FNodeTemplate* BuildNodeTemplate(const TArray<FDecoratorUID>& NodeTemplateDecoratorList, TArray<uint8>& NodeTemplateBuffer);

	// Converts a property value into its string representation using UE reflection
	template<class DecoratorSharedDataType, typename PropertyType>
	static FString ToString(const FString& PropertyName, PropertyType PropertyValue)
	{
		const UScriptStruct* SharedDataStruct = DecoratorSharedDataType::StaticStruct();

		for (const FProperty* Property = SharedDataStruct->PropertyLink; Property != nullptr; Property = Property->PropertyLinkNext)
		{
			if (Property->GetName() == PropertyName)
			{
				void* PropertyDefaults = Property->AllocateAndInitializeValue();

				FString Result;

				if constexpr (std::is_pointer_v<PropertyType>)
				{
					// C-style array properties aren't handled by ExportText, we need to handle it manually

					const bool bIsCArray = Property->ArrayDim > 1;
					if (bIsCArray)
					{
						Result += TEXT("(");
					}

					for (int32 Index = 0; Index < Property->ArrayDim; ++Index)
					{
						Property->ExportText_Direct(Result, PropertyValue + Index, (PropertyType)PropertyDefaults + Index, nullptr, PPF_None, nullptr);

						if (Index + 1 < Property->ArrayDim)
						{
							Result += TEXT(",");
						}
					}

					if (bIsCArray)
					{
						Result += TEXT(")");
					}
				}
				else
				{
					Property->ExportText_Direct(Result, &PropertyValue, PropertyDefaults, nullptr, PPF_None);
				}

				Property->DestroyAndFreeValue(PropertyDefaults);

				return Result;
			}
		}

		return FString();
	}
}

#endif
