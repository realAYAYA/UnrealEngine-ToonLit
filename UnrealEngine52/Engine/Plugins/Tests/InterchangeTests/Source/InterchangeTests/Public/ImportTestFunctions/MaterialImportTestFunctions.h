// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/EngineTypes.h"
#include "ImportTestFunctionsBase.h"
#include "MaterialImportTestFunctions.generated.h"

class UMaterialInterface;

struct FInterchangeTestFunctionResult;


UCLASS()
class INTERCHANGETESTS_API UMaterialImportTestFunctions : public UImportTestFunctionsBase
{
	GENERATED_BODY()

public:

	// UImportTestFunctionsBase interface
	virtual UClass* GetAssociatedAssetType() const override;

	/** Check whether the expected number of materials are imported */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckImportedMaterialCount(const TArray<UMaterialInterface*>& MaterialInterfaces, int32 ExpectedNumberOfImportedMaterials);

	/** Check whether the expected number of material instances are imported */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckImportedMaterialInstanceCount(const TArray<UMaterialInterface*>& MaterialInterfaces, int32 ExpectedNumberOfImportedMaterialInstances);

	/** Check whether the imported material has the expected shading model */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckShadingModel(const UMaterialInterface* MaterialInterface, EMaterialShadingModel ExpectedShadingModel);

	/** Check whether the imported material has the expected blend mode */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckBlendMode(const UMaterialInterface* MaterialInterface, EBlendMode ExpectedBlendMode);

	/** Check whether the imported material has the expected two-sided setting */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckIsTwoSided(const UMaterialInterface* MaterialInterface, bool ExpectedIsTwoSided);

	/** Check whether the imported material has the expected opacity mask clip value */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckOpacityMaskClipValue(const UMaterialInterface* MaterialInterface, float ExpectedOpacityMaskClipValue);

	/** Check whether the imported material has the expected scalar parameter value */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckScalarParameter(const UMaterialInterface* MaterialInterface, const FString& ParameterName, float ExpectedParameterValue);

	/** Check whether the imported material has the expected vector parameter value */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckVectorParameter(const UMaterialInterface* MaterialInterface, const FString& ParameterName, FLinearColor ExpectedParameterValue);
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "InterchangeTestFunction.h"
#endif
