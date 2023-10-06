// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "InterchangePipelineBase.h"
#include "InterchangeSourceData.h"
#include "Nodes/InterchangeBaseNodeContainer.h"

#include "Engine/DeveloperSettings.h"
#include "Templates/Tuple.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/SoftObjectPath.h"

#include "InterchangeMaterialXPipeline.generated.h"

class UInterchangeDatasmithPbrMaterialNode;
class UInterchangeFactoryBaseNode;
class UInterchangeMaterialFactoryNode;
class UInterchangeMaterialInstanceFactoryNode;
class UInterchangeShaderGraphNode;
class UInterchangeMaterialInstanceNode;

class UMaterialFunction;
class UMaterialInterface;

UENUM(BlueprintType)
enum class EInterchangeMaterialXShaders : uint8
{
	/** Default settings for Autodesk's Standard Surface shader	*/
	StandardSurface,

	/** Standard Surface shader used for translucency	*/
	StandardSurfaceTransmission,

	/** Shader used for unlit surface*/
	SurfaceUnlit,

	/** Default settings for USD's Surface shader	*/
	UsdPreviewSurface,

	MaxShaderCount
};

UCLASS(config = Interchange, meta = (DisplayName = "Interchange MaterialX"))
class INTERCHANGEPIPELINES_API UMaterialXPipelineSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, config, Category = "MaterialXPredefined", meta = (DisplayName = "MaterialX Predefined Surface Shaders"))
	TMap<EInterchangeMaterialXShaders, FSoftObjectPath> PredefinedSurfaceShaders;

	bool AreRequiredPackagesLoaded();

	FString GetAssetPathString(EInterchangeMaterialXShaders ShaderType) const;

private:

#if WITH_EDITOR
	friend class FInterchangeMaterialXPipelineSettingsCustomization;

	static bool ShouldFilterAssets(UMaterialFunction* Asset, const TSet<FName>& Inputs, const TSet<FName>& Outputs);

	static TSet<FName> StandardSurfaceInputs;
	static TSet<FName> StandardSurfaceOutputs;
	static TSet<FName> TransmissionSurfaceInputs;
	static TSet<FName> TransmissionSurfaceOutputs;
	static TSet<FName> SurfaceUnlitInputs;
	static TSet<FName> SurfaceUnlitOutputs;
	static TSet<FName> UsdPreviewSurfaceInputs;
	static TSet<FName> UsdPreviewSurfaceOutputs;
#endif // WITH_EDITOR
};

UCLASS(BlueprintType)
class INTERCHANGEPIPELINES_API UInterchangeMaterialXPipeline : public UInterchangePipelineBase
{
	GENERATED_BODY()

	UInterchangeMaterialXPipeline();

public:
	
	TObjectPtr<UMaterialXPipelineSettings> MaterialXSettings;

protected:
	virtual void AdjustSettingsForContext(EInterchangePipelineContext ImportType, TObjectPtr<UObject> ReimportAsset) override;
	virtual void ExecutePipeline(UInterchangeBaseNodeContainer* BaseNodeContainer, const TArray<UInterchangeSourceData*>& SourceDatas) override;

	virtual bool CanExecuteOnAnyThread(EInterchangePipelineTask PipelineTask) override
	{
		// This pipeline creates UObjects and assets. Not safe to execute outside of main thread.
		return true;
	}

private:
	static TMap<FString, EInterchangeMaterialXShaders> PathToEnumMapping;
};