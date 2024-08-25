// Copyright Epic Games, Inc. All Rights Reserved.

#include "Controller/RCCustomControllerUtilities.h"
#include "RCVirtualProperty.h"
#include "RCVirtualPropertyContainer.h"

namespace UE::RCCustomControllers
{
	// A Map to associate a custom controller name to its underlying type
	inline static TMap<FString, EPropertyBagPropertyType> CustomControllerTypes =
		{{CustomTextureControllerName, EPropertyBagPropertyType::String}};
}

bool UE::RCCustomControllers::IsCustomController(const URCVirtualPropertyBase* InController)
{
	return !InController->GetMetadataValue(CustomControllerNameKey).IsEmpty();
}

FString UE::RCCustomControllers::GetCustomControllerTypeName(const URCVirtualPropertyBase* InController)
{
	return InController->GetMetadataValue(CustomControllerNameKey);
}

bool UE::RCCustomControllers::IsValidCustomController(const FName& InCustomControllerTypeName)
{
	return CustomControllerTypes.Contains(InCustomControllerTypeName.ToString());
}

EPropertyBagPropertyType UE::RCCustomControllers::GetCustomControllerType(const FName& InCustomControllerTypeName)
{
	if (IsValidCustomController(InCustomControllerTypeName))
	{
		return CustomControllerTypes[InCustomControllerTypeName.ToString()];
	}

	return EPropertyBagPropertyType::None;
}

EPropertyBagPropertyType UE::RCCustomControllers::GetCustomControllerType(const FString& InCustomControllerTypeName)
{
	return GetCustomControllerType(FName(InCustomControllerTypeName));
}

TMap<FName, FString> UE::RCCustomControllers::GetCustomControllerMetaData(const FString& InCustomControllerTypeName)
{
	TMap<FName, FString> OutMetaData;
	OutMetaData.Emplace(UE::RCCustomControllers::CustomControllerNameKey, InCustomControllerTypeName);

	// Note: this is a good place to additionally edit OutMetaData by manually adding elements here if custom logic is needed
	
	return OutMetaData;
}

FName UE::RCCustomControllers::GetUniqueNameForController(const URCVirtualPropertyInContainer* InController)
{
	const FString& CustomTypeName = UE::RCCustomControllers::GetCustomControllerTypeName(InController);
	if (!CustomTypeName.IsEmpty())
	{
		if (InController->ContainerWeakPtr.IsValid())
		{
			URCVirtualPropertyContainerBase* Container = InController->ContainerWeakPtr.Get();
								
			int32 Index = 0;
			const FString InitialName = CustomTypeName;
			FString FinalName = InitialName;

			for (const TObjectPtr<URCVirtualPropertyBase>& VirtualProperty : Container->VirtualProperties)
			{
				if (VirtualProperty->DisplayName == FinalName)
				{
					Index++;
					if (Index > 0)
					{
						FinalName = InitialName + TEXT("_") + FString::FromInt(Index++);
					}
				}
			}
			
			return FName(FinalName);				
		}
	}

	// Worst case - we return the plain type name.
	return FName(CustomTypeName);
}
