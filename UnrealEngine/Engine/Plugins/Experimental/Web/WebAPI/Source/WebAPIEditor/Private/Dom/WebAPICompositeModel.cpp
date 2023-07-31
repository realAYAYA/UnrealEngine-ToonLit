// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dom/WebAPICompositeModel.h"
#include "Dom/WebAPIType.h"

namespace UE::WebAPI::WebAPIModelCompositionType
{
	const FString& ToString(const EWebAPIModelCompositionType& InEnumValue)
	{
		static TMap<EWebAPIModelCompositionType, FString> EnumNames = {
			{ EWebAPIModelCompositionType::Single, TEXT("Single") },
			{ EWebAPIModelCompositionType::Multiple, TEXT("Multiple") },
			{ EWebAPIModelCompositionType::All, TEXT("All") }
		};
 
		return EnumNames[InEnumValue];
	}

	const EWebAPIModelCompositionType& FromString(const FString& InStringValue)
	{
		static TMap<FString, EWebAPIModelCompositionType> EnumValues = {
			{ TEXT("Single"), EWebAPIModelCompositionType::Single },
			{ TEXT("Multiple"), EWebAPIModelCompositionType::Multiple },
			{ TEXT("All"), EWebAPIModelCompositionType::All },
		};

		return EnumValues[InStringValue];
	}
};

FString UWebAPICompositeModel::GetSortKey() const
{
	return Name.ToString(true);
}

void UWebAPICompositeModel::SetNamespace(const FString& InNamespace)
{
	Super::SetNamespace(InNamespace);

	if(Name.HasTypeInfo() && !Name.TypeInfo->bIsBuiltinType)
	{
		Name.TypeInfo->Namespace = InNamespace;
	}
}
