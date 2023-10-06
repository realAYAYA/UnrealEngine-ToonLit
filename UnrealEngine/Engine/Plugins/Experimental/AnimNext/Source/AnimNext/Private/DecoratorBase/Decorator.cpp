// Copyright Epic Games, Inc. All Rights Reserved.

#include "DecoratorBase/Decorator.h"

#include "DecoratorBase/DecoratorReader.h"
#include "DecoratorBase/DecoratorRegistry.h"

#if WITH_EDITOR
#include "DecoratorBase/DecoratorWriter.h"
#endif

namespace UE::AnimNext
{
	void FDecorator::SerializeDecoratorSharedData(FArchive& Ar, FAnimNextDecoratorSharedData& SharedData) const
	{
		UScriptStruct* SharedDataStruct = GetDecoratorSharedDataStruct();
		SharedDataStruct->SerializeItem(Ar, &SharedData, nullptr);
	}

#if WITH_EDITOR
	void FDecorator::SaveDecoratorSharedData(FDecoratorWriter& Writer, const TMap<FString, FString>& Properties, FAnimNextDecoratorSharedData& OutSharedData) const
	{
		const UScriptStruct* SharedDataStruct = GetDecoratorSharedDataStruct();

		uint8* SharedData = reinterpret_cast<uint8*>(&OutSharedData);

		// Initialize our output struct with its default values
		SharedDataStruct->InitializeDefaultValue(SharedData);

		// Use UE reflection to iterate over every property
		// We convert every property from its string representation into its binary form
		for (const FProperty* Property = SharedDataStruct->PropertyLink; Property != nullptr; Property = Property->PropertyLinkNext)
		{
			if (const FString* PropertyValue = Properties.Find(Property->GetName()))
			{
				const TCHAR* PropertyValuePtr = **PropertyValue;

				// C-style array properties aren't handled by ExportText, we need to handle it manually
				const bool bIsCArray = Property->ArrayDim > 1;
				if (bIsCArray)
				{
					ensure(PropertyValuePtr[0] == TEXT('('));
					PropertyValuePtr++;
				}

				for (int32 Index = 0; Index < Property->ArrayDim; ++Index)
				{
					void* DataPtr = Property->ContainerPtrToValuePtr<void>(SharedData, Index);
					PropertyValuePtr = Property->ImportText_Direct(PropertyValuePtr, DataPtr, nullptr, PPF_None);

					if (Index + 1 < Property->ArrayDim)
					{
						ensure(PropertyValuePtr[0] == TEXT(','));
						PropertyValuePtr++;
					}
				}

				if (bIsCArray)
				{
					ensure(PropertyValuePtr[0] == TEXT(')'));
					PropertyValuePtr++;
				}
			}
		}
	}
#endif

	FDecoratorStaticInitHook::FDecoratorStaticInitHook(DecoratorConstructorFunc DecoratorConstructor_)
		: DecoratorConstructor(DecoratorConstructor_)
	{
		FDecoratorRegistry::StaticRegister(DecoratorConstructor_);
	}

	FDecoratorStaticInitHook::~FDecoratorStaticInitHook()
	{
		FDecoratorRegistry::StaticUnregister(DecoratorConstructor);
	}
}
