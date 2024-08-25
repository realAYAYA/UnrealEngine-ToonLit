// Copyright Epic Games, Inc. All Rights Reserved.

#include "DecoratorBase/Decorator.h"

#include "DecoratorBase/DecoratorRegistry.h"

namespace UE::AnimNext
{
	void FDecorator::SerializeDecoratorSharedData(FArchive& Ar, FAnimNextDecoratorSharedData& SharedData) const
	{
		UScriptStruct* SharedDataStruct = GetDecoratorSharedDataStruct();
		SharedDataStruct->SerializeItem(Ar, &SharedData, nullptr);
	}

	FDecoratorLatentPropertyMemoryLayout FDecorator::GetLatentPropertyMemoryLayoutImpl(
		FName PropertyName,
		uint32 PropertyIndex,
		TArray<FDecoratorLatentPropertyMemoryLayout>& LatentPropertyMemoryLayouts) const
	{
		check(LatentPropertyMemoryLayouts.IsValidIndex(PropertyIndex));
		if (LatentPropertyMemoryLayouts[PropertyIndex].Size == 0)
		{
			// This is a new entry, initialize it
			// No need for locking, this is a deterministic write
			const UScriptStruct* SharedDataStruct = GetDecoratorSharedDataStruct();
			const FProperty* Property = SharedDataStruct->FindPropertyByName(PropertyName);
			check(Property != nullptr);

			LatentPropertyMemoryLayouts[PropertyIndex].Alignment = Property->GetMinAlignment();

			// Ensure alignment is visible before we write the size to avoid torn reads
			FPlatformMisc::MemoryBarrier();

			LatentPropertyMemoryLayouts[PropertyIndex].Size = Property->GetSize();
		}
		
		return LatentPropertyMemoryLayouts[PropertyIndex];
	}

	TArray<FDecoratorInterfaceUID> FDecorator::BuildDecoratorInterfaceList(
		const TConstArrayView<FDecoratorInterfaceUID>& SuperInterfaces,
		std::initializer_list<FDecoratorInterfaceUID> InterfaceList)
	{
		TArray<FDecoratorInterfaceUID> Result;
		Result.Reserve(SuperInterfaces.Num() + InterfaceList.size());

		Result.Append(SuperInterfaces);

		for (FDecoratorInterfaceUID InterfaceID : InterfaceList)
		{
			Result.AddUnique(InterfaceID);
		}

		Result.Shrink();
		Result.Sort();
		return Result;
	}

#if WITH_EDITOR
	void FDecorator::SaveDecoratorSharedData(const TFunction<FString(FName PropertyName)>& GetDecoratorProperty, FAnimNextDecoratorSharedData& OutSharedData) const
	{
		const UScriptStruct* SharedDataStruct = GetDecoratorSharedDataStruct();

		uint8* SharedData = reinterpret_cast<uint8*>(&OutSharedData);

		// Initialize our output struct with its default values
		SharedDataStruct->InitializeDefaultValue(SharedData);

		// Use UE reflection to iterate over every property
		// We convert every property from its string representation into its binary form
		for (const FProperty* Property = SharedDataStruct->PropertyLink; Property != nullptr; Property = Property->PropertyLinkNext)
		{
			// No need to skip editor only properties since serialization will take care of that afterwards

			const FString PropertyValue = GetDecoratorProperty(Property->GetFName());
			if (PropertyValue.Len() != 0)
			{
				const TCHAR* PropertyValuePtr = *PropertyValue;

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

	TArray<FLatentPropertyMetadata> FDecorator::GetLatentPropertyHandles(
		bool bFilterEditorOnly,
		const TFunction<uint16(FName PropertyName)>& GetDecoratorLatentPropertyIndex) const
	{
		const UStruct* BaseStruct = GetDecoratorSharedDataStruct();

		// The property linked list on UScriptStruct iterates over the properties starting in the derived type
		// but with latent properties, the base type should be the first to be visited.
		// Gather our struct hierarchy from most derived to base
		TArray<const UStruct*> StructHierarchy;

		do
		{
			StructHierarchy.Add(BaseStruct);
			BaseStruct = BaseStruct->GetSuperStruct();
		}
		while (BaseStruct != nullptr);

		TArray<FLatentPropertyMetadata> LatentPropertyHandles;

		// Gather our latent properties from base to most derived
		for (auto It = StructHierarchy.rbegin(); It != StructHierarchy.rend(); ++It)
		{
			const UStruct* SharedDataStruct = *It;
			for (const FField* Field = SharedDataStruct->ChildProperties; Field != nullptr; Field = Field->Next)
			{
				const FProperty* Property = CastField<FProperty>(Field);

				if (bFilterEditorOnly && Property->IsEditorOnlyProperty())
				{
					continue;	// Skip editor only properties if we don't need them
				}

				// By default, properties are latent
				// However, there are exceptions:
				//     - Properties marked as hidden are not visible in the editor and cannot be hooked up manually
				//     - Properties marked as inline are only visible in the details panel and cannot be hooked up to another node
				//     - Properties of decorator handle type are never lazy since they just encode graph connectivity
				const bool bIsPotentiallyLatent =
					!Property->HasMetaData(TEXT("Hidden")) &&
					!Property->HasMetaData(TEXT("Inline")) &&
					Property->GetCPPType() != TEXT("FAnimNextDecoratorHandle");

				if (!bIsPotentiallyLatent)
				{
					continue;	// Skip non-latent properties
				}

				FLatentPropertyMetadata Metadata;
				Metadata.Name = Property->GetFName();
				Metadata.RigVMIndex = GetDecoratorLatentPropertyIndex(Property->GetFName());

				// Always false for now, we don't support freezing yet
				Metadata.bCanFreeze = false;

				LatentPropertyHandles.Add(Metadata);
			}
		}

		return LatentPropertyHandles;
	}
#endif

	FDecoratorStaticInitHook::FDecoratorStaticInitHook(DecoratorConstructorFunc InDecoratorConstructor)
		: DecoratorConstructor(InDecoratorConstructor)
	{
		FDecoratorRegistry::StaticRegister(InDecoratorConstructor);
	}

	FDecoratorStaticInitHook::~FDecoratorStaticInitHook()
	{
		FDecoratorRegistry::StaticUnregister(DecoratorConstructor);
	}
}
