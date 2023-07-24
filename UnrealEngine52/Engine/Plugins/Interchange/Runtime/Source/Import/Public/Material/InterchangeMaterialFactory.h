// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InterchangeFactoryBase.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "Materials/MaterialFunction.h"

#include "InterchangeMaterialFactory.generated.h"

class UInterchangeBaseMaterialFactoryNode;
class UInterchangeMaterialExpressionFactoryNode;
class UMaterial;
class UMaterialExpression;
class UMaterialExpressionMaterialFunctionCall;
class UMaterialInstance;

UCLASS(BlueprintType)
class INTERCHANGEIMPORT_API UInterchangeMaterialFactory : public UInterchangeFactoryBase
{
	GENERATED_BODY()

public:
	//////////////////////////////////////////////////////////////////////////
	// Interchange factory base interface begin

	virtual UClass* GetFactoryClass() const override;
	virtual EInterchangeFactoryAssetType GetFactoryAssetType() override { return EInterchangeFactoryAssetType::Materials; }
	virtual UObject* ImportAssetObject_GameThread(const FImportAssetObjectParams& Arguments) override;
	virtual UObject* ImportAssetObject_Async(const FImportAssetObjectParams& Arguments) override;
	virtual void SetupObject_GameThread(const FSetupObjectParams& Arguments) override;
	virtual bool GetSourceFilenames(const UObject* Object, TArray<FString>& OutSourceFilenames) const override;
	virtual bool SetSourceFilename(const UObject* Object, const FString& SourceFilename, int32 SourceIndex) const override;

	// Interchange factory base interface end
	//////////////////////////////////////////////////////////////////////////
private:
#if WITH_EDITOR
	void SetupMaterial(UMaterial* Material, const FImportAssetObjectParams& Arguments, const UInterchangeBaseMaterialFactoryNode* MaterialFactoryNode);
#endif // #if WITH_EDITOR

	void SetupMaterialInstance(UMaterialInstance* MaterialInstance, const UInterchangeBaseNodeContainer* NodeContainer, const UInterchangeBaseMaterialFactoryNode* MaterialFactoryNode);
};

UCLASS(BlueprintType)
class INTERCHANGEIMPORT_API UInterchangeMaterialFunctionFactory : public UInterchangeFactoryBase
{
	GENERATED_BODY()

public:
	//////////////////////////////////////////////////////////////////////////
	// Interchange factory base interface begin

	virtual UClass* GetFactoryClass() const override;
	virtual EInterchangeFactoryAssetType GetFactoryAssetType() override { return EInterchangeFactoryAssetType::Materials; }
	virtual UObject* ImportAssetObject_GameThread(const FImportAssetObjectParams& Arguments) override;
	virtual UObject* ImportAssetObject_Async(const FImportAssetObjectParams& Arguments) override;
	virtual void SetupObject_GameThread(const FSetupObjectParams& Arguments) override;

	// Interchange factory base interface end
	//////////////////////////////////////////////////////////////////////////

private:
#if WITH_EDITOR
	void SetupMaterial(class UMaterialFunction* Material, const FImportAssetObjectParams& Arguments, const class UInterchangeMaterialFunctionFactoryNode* MaterialFactoryNode);
#endif
};

/**
 * This class intends to avoid a race condition when importing 
 * several material functions at once, for example when importing the MaterialX file standard_surface_chess_set
 * Only one instance of a material function can update it
 */
class FInterchangeImportMaterialAsyncHelper
{
	FInterchangeImportMaterialAsyncHelper() = default;

public:

	FInterchangeImportMaterialAsyncHelper(const FInterchangeImportMaterialAsyncHelper&) = delete;
	FInterchangeImportMaterialAsyncHelper& operator=(const FInterchangeImportMaterialAsyncHelper&) = delete;

	static FInterchangeImportMaterialAsyncHelper& GetInstance();

#if WITH_EDITOR
	void UpdateFromFunctionResource(UMaterialExpressionMaterialFunctionCall* MaterialFunctionCall);

	void UpdateFromFunctionResource(UMaterialFunctionInterface* MaterialFunction);
#endif

private:

	FCriticalSection UpdatedMaterialFunctionCallsLock;
	FCriticalSection UpdatedMaterialFunctionsLock;
};