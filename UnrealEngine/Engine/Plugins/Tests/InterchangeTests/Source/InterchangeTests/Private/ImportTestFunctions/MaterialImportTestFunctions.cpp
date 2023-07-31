// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImportTestFunctions/MaterialImportTestFunctions.h"

#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MaterialImportTestFunctions)

namespace UE::Interchange::Tests::Private
{
	TArray<UMaterialInterface*> FilterByMaterialInterfaceSubclass(const TArray<UMaterialInterface*>& MaterialInterfaces, const TSubclassOf<UMaterialInterface>& MaterialInterfaceSubclass)
	{
		TArray<UMaterialInterface*> FilteredMaterialInterfaces;

		for (UMaterialInterface* MaterialInterface : MaterialInterfaces)
		{
			if (MaterialInterface->IsA(MaterialInterfaceSubclass))
			{
				FilteredMaterialInterfaces.Add(MaterialInterface);
			}
		}

		return FilteredMaterialInterfaces;
	}
}

UClass* UMaterialImportTestFunctions::GetAssociatedAssetType() const
{
	return UMaterialInterface::StaticClass();
}

FInterchangeTestFunctionResult UMaterialImportTestFunctions::CheckImportedMaterialCount(const TArray<UMaterialInterface*>& MaterialInterfaces, int32 ExpectedNumberOfImportedMaterials)
{
	using namespace UE::Interchange::Tests::Private;

	FInterchangeTestFunctionResult Result;
	const int32 NumImportedMaterials = FilterByMaterialInterfaceSubclass(MaterialInterfaces, UMaterial::StaticClass()).Num();

	if (NumImportedMaterials != ExpectedNumberOfImportedMaterials)
	{
		Result.AddError(FString::Printf(TEXT("Expected %d materials, imported %d."), ExpectedNumberOfImportedMaterials, NumImportedMaterials));
	}

	return Result;
}

FInterchangeTestFunctionResult UMaterialImportTestFunctions::CheckImportedMaterialInstanceCount(const TArray<UMaterialInterface*>& MaterialInterfaces, int32 ExpectedNumberOfImportedMaterialInstances)
{
	using namespace UE::Interchange::Tests::Private;

	FInterchangeTestFunctionResult Result;
	const int32 NumImportedMaterialInstances = FilterByMaterialInterfaceSubclass(MaterialInterfaces, UMaterialInstance::StaticClass()).Num();

	if (NumImportedMaterialInstances != ExpectedNumberOfImportedMaterialInstances)
	{
		Result.AddError(FString::Printf(TEXT("Expected %d material instances, imported %d."), ExpectedNumberOfImportedMaterialInstances, NumImportedMaterialInstances));
	}

	return Result;
}

