// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InterchangePipelineBase.h"
#include "InterchangeSourceData.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "InterchangeGenericMaterialPipeline.generated.h"

class UInterchangeBaseMaterialFactoryNode;
class UInterchangeFunctionCallShaderNode;
class UInterchangeGenericTexturePipeline;
class UInterchangeShaderGraphNode;
class UInterchangeShaderNode;
class UInterchangeMaterialFactoryNode;
class UInterchangeMaterialExpressionFactoryNode;
class UInterchangeMaterialFunctionFactoryNode;
class UInterchangeMaterialInstanceFactoryNode;
class UInterchangeResult;
class UMaterialFunction;
class UInterchangeTextureNode;

namespace UE::Interchange::Materials::HashUtils
{
	class FDuplicateMaterialHelper;
}

UENUM(BlueprintType)
enum class EInterchangeMaterialImportOption : uint8
{
	/** Import all materials from the source as material assets. */
	ImportAsMaterials,
	/** Import all materials from the source as material instance assets. */
	ImportAsMaterialInstances,
};

UENUM(BlueprintType)
enum class EInterchangeMaterialSearchLocation : uint8
{
	/** Search for existing material in local import folder only. */
	Local,
	/** Search for existing material recursively from parent folder. */
	UnderParent,
	/** Search for existing material recursively from root folder. */
	UnderRoot,
	/** Search for existing material in all assets folders. */
	AllAssets,
	/** Do not search for existing existing materials */
	DoNotSearch,
};

UCLASS(BlueprintType, editinlinenew)
class INTERCHANGEPIPELINES_API UInterchangeGenericMaterialPipeline : public UInterchangePipelineBase
{
	GENERATED_BODY()

public:
	UInterchangeGenericMaterialPipeline();

	/** The name of the pipeline that will be display in the import dialog. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials", meta = (StandAlonePipelineProperty = "True", PipelineInternalEditionData = "True"))
	FString PipelineDisplayName;

	/** If enabled, imports the material assets found in the sources. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials")
	bool bImportMaterials = true;

	/** Specify where we should search for existing materials when importing.*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials")
	EInterchangeMaterialSearchLocation SearchLocation = EInterchangeMaterialSearchLocation::Local;

	/** If set, and there is only one asset and one source, the imported asset will be given this name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials", meta = (StandAlonePipelineProperty = "True", AlwaysResetToDefault = "True"))
	FString AssetName;

	/** Determines what kind of material assets should be created for the imported materials. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials", Meta=(EditCondition="bImportMaterials"))
	EInterchangeMaterialImportOption MaterialImport = EInterchangeMaterialImportOption::ImportAsMaterials;
	
	/** If set, reference materials along with respective material instances are created. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials", Meta = (EditCondition = "bImportMaterials"))
	bool bIdentifyDuplicateMaterials = false;

	/** If set, additional material instances are created for reference/parent materials. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials", Meta=(EditCondition="bIdentifyDuplicateMaterials"))
	bool bCreateMaterialInstanceForParent = false;

	/** Optional material used as the parent when importing materials as instances. If no parent material is specified, one will be automatically selected during the import process. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials", Meta= (EditCondition="bImportMaterials && MaterialImport==EInterchangeMaterialImportOption::ImportAsMaterialInstances", AllowedClasses="/Script/Engine.MaterialInterface"))
	FSoftObjectPath ParentMaterial;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Instanced, Category = "Textures")
	TObjectPtr<UInterchangeGenericTexturePipeline> TexturePipeline;

	/** BEGIN UInterchangePipelineBase overrides */
	virtual void PreDialogCleanup(const FName PipelineStackName) override;
	virtual bool IsSettingsAreValid(TOptional<FText>& OutInvalidReason) const override;
	virtual void AdjustSettingsForContext(EInterchangePipelineContext ImportType, TObjectPtr<UObject> ReimportAsset) override;
#if WITH_EDITOR
	virtual void FilterPropertiesFromTranslatedData(UInterchangeBaseNodeContainer* InBaseNodeContainer) override;
	virtual bool IsPropertyChangeNeedRefresh(const FPropertyChangedEvent& PropertyChangedEvent) override;
#endif //WITH_EDITOR

protected:
	virtual void ExecutePipeline(UInterchangeBaseNodeContainer* InBaseNodeContainer, const TArray<UInterchangeSourceData*>& InSourceDatas, const FString& ContentBasePath) override;
	virtual void ExecutePostFactoryPipeline(const UInterchangeBaseNodeContainer* BaseNodeContainer, const FString& NodeKey, UObject* CreatedAsset, bool bIsAReimport) override;
	virtual void ExecutePostImportPipeline(const UInterchangeBaseNodeContainer* BaseNodeContainer, const FString& NodeKey, UObject* CreatedAsset, bool bIsAReimport) override;
	virtual void SetReimportSourceIndex(UClass* ReimportObjectClass, const int32 SourceFileIndex) override;
	/** END UInterchangePipelineBase overrides */

	UPROPERTY()
	TObjectPtr<UInterchangeBaseNodeContainer> BaseNodeContainer;

	TArray<const UInterchangeSourceData*> SourceDatas;
	
private:
	/** Material translated assets nodes */
	TArray<UInterchangeShaderGraphNode*> MaterialNodes;
	
	/** Material factory assets nodes */
	TArray<UInterchangeBaseMaterialFactoryNode*> MaterialFactoryNodes;

	UInterchangeBaseMaterialFactoryNode* CreateBaseMaterialFactoryNode(const UInterchangeBaseNode* MaterialNode, TSubclassOf<UInterchangeBaseMaterialFactoryNode> NodeType, bool bAddMaterialInstanceSuffix = false);
	UInterchangeMaterialFactoryNode* CreateMaterialFactoryNode(const UInterchangeShaderGraphNode* ShaderGraphNode);
	UInterchangeMaterialFunctionFactoryNode* CreateMaterialFunctionFactoryNode(const UInterchangeShaderGraphNode* FunctionCallShaderNode);
	UInterchangeMaterialInstanceFactoryNode* CreateMaterialInstanceFactoryNode(const UInterchangeShaderGraphNode* ShaderGraphNode);
	
	/** True if the shader graph has a clear coat input. */
	bool HasClearCoat(const UInterchangeShaderGraphNode* ShaderGraphNode) const;
	UE_DEPRECATED(5.3, "Deprecated. Use HasClearCoat.")
	bool IsClearCoatModel(const UInterchangeShaderGraphNode* ShaderGraphNode) const
	{
		return HasClearCoat(ShaderGraphNode);
	}

	/** True if the shader graph has a sheen color input. */
	bool HasSheen(const UInterchangeShaderGraphNode* ShaderGraphNode) const;
	UE_DEPRECATED(5.3, "Deprecated. Use HasSheen.")
	bool IsSheenModel(const UInterchangeShaderGraphNode* ShaderGraphNode) const
	{
		return HasSheen(ShaderGraphNode);
	}

	/** True if the shader graph has a subsurface color input. */
	bool HasSubsurface(const UInterchangeShaderGraphNode* ShaderGraphNode) const;
	UE_DEPRECATED(5.3, "Deprecated. Use HasSubsurface.")
	bool IsSubsurfaceModel(const UInterchangeShaderGraphNode* ShaderGraphNode) const
	{
		return HasSubsurface(ShaderGraphNode);
	}

	/** True if the shader graph has a transmission color input. */
	bool HasThinTranslucency(const UInterchangeShaderGraphNode* ShaderGraphNode) const;
	UE_DEPRECATED(5.3, "Deprecated. Use HasThinTranslucency.")
	bool IsThinTranslucentModel(const UInterchangeShaderGraphNode* ShaderGraphNode) const
	{
		return HasThinTranslucency(ShaderGraphNode);
	}

	/** True if the shader graph has a base color input (Metallic/Roughness model). */
	bool IsMetalRoughModel(const UInterchangeShaderGraphNode* ShaderGraphNode) const;
	UE_DEPRECATED(5.3, "Deprecated. Use IsMetalRoughModel and IsSpecGlossModel to identify the correct PBR model.")
	bool IsPBRModel(const UInterchangeShaderGraphNode* ShaderGraphNode) const
	{
		return IsMetalRoughModel(ShaderGraphNode);
	}

	/** True if the shader graph has diffuse color and specular inputs. */
	bool IsPhongModel(const UInterchangeShaderGraphNode* ShaderGraphNode) const;

	/** True if the shader graph has a diffuse color input. */
	bool IsLambertModel(const UInterchangeShaderGraphNode* ShaderGraphNode) const;

	/** True if the shader graph has the surface unlit's shader type name. */
	bool IsSurfaceUnlitModel(const UInterchangeShaderGraphNode* ShaderGraphNode) const;

	/** True if the shader graph has an unlit color input. */
	bool IsUnlitModel(const UInterchangeShaderGraphNode* ShaderGraphNode) const;

	/** True if the shader graph has specular color and glossiness scalar inputs. */
	bool IsSpecGlossModel(const UInterchangeShaderGraphNode* ShaderGraphNode) const;

	bool HandlePhongModel(const UInterchangeShaderGraphNode* ShaderGraphNode, UInterchangeMaterialFactoryNode* MaterialFactoryNode);
	bool HandleLambertModel(const UInterchangeShaderGraphNode* ShaderGraphNode, UInterchangeMaterialFactoryNode* MaterialFactoryNode);
	bool HandleMetalRoughnessModel(const UInterchangeShaderGraphNode* ShaderGraphNode, UInterchangeMaterialFactoryNode* MaterialFactoryNode);
	bool HandleClearCoat(const UInterchangeShaderGraphNode* ShaderGraphNode, UInterchangeMaterialFactoryNode* MaterialFactoryNode);
	bool HandleSubsurface(const UInterchangeShaderGraphNode* ShaderGraphNode, UInterchangeMaterialFactoryNode* MaterialFactoryNode);
	bool HandleSheen(const UInterchangeShaderGraphNode* ShaderGraphNode, UInterchangeMaterialFactoryNode* MaterialFactoryNode);
	bool HandleThinTranslucent(const UInterchangeShaderGraphNode* ShaderGraphNode, UInterchangeMaterialFactoryNode* MaterialFactoryNode);
	void HandleCommonParameters(const UInterchangeShaderGraphNode* ShaderGraphNode, UInterchangeMaterialFactoryNode* MaterialFactoryNode);
	bool HandleBxDFInput(const UInterchangeShaderGraphNode* ShaderGraphNode, UInterchangeMaterialFactoryNode* MaterialFactoryNode);
	bool HandleUnlitModel(const UInterchangeShaderGraphNode* ShaderGraphNode, UInterchangeMaterialFactoryNode* MaterialFactoryNode);
	bool HandleSpecGlossModel(const UInterchangeShaderGraphNode* ShaderGraphNode, UInterchangeMaterialFactoryNode* MaterialFactoryNode);
	bool HandleSubstrate(const UInterchangeShaderGraphNode* ShaderGraphNode, UInterchangeMaterialFactoryNode* MaterialFactoryNode);

	void HandleFlattenNormalNode(const UInterchangeShaderNode* ShaderNode, UInterchangeBaseMaterialFactoryNode* MaterialFactoryNode, UInterchangeMaterialExpressionFactoryNode* FlattenNormalFactoryNode);
	void HandleNormalFromHeightMapNode(const UInterchangeShaderNode* ShaderNode, UInterchangeBaseMaterialFactoryNode* MaterialFactoryNode, UInterchangeMaterialExpressionFactoryNode* NormalFromHeightMapFactoryNode);
	void HandleMakeFloat3Node(const UInterchangeShaderNode* ShaderNode, UInterchangeBaseMaterialFactoryNode* MaterialFactoryNode, UInterchangeMaterialExpressionFactoryNode* MakeFloat3FactoryNode);
	void HandleTextureNode(const UInterchangeTextureNode* TextureNode, UInterchangeBaseMaterialFactoryNode* MaterialFactoryNode, UInterchangeMaterialExpressionFactoryNode* TextureBaseFactoryNode, const FString& ExpressionClassName, bool bIsAParameter);
	void HandleTextureObjectNode(const UInterchangeShaderNode* ShaderNode, UInterchangeBaseMaterialFactoryNode* MaterialFactoryNode, UInterchangeMaterialExpressionFactoryNode* TextureObjectFactoryNode);
	void HandleTextureSampleNode(const UInterchangeShaderNode* ShaderNode, UInterchangeBaseMaterialFactoryNode* MaterialFactoryNode, UInterchangeMaterialExpressionFactoryNode* TextureSampleFactoryNode);
	void HandleTextureSampleBlurNode(const UInterchangeShaderNode* ShaderNode, UInterchangeBaseMaterialFactoryNode* MaterialFactoryNode, UInterchangeMaterialExpressionFactoryNode* TextureSampleFactoryNode);
	void HandleTextureCoordinateNode(const UInterchangeShaderNode* ShaderNode, UInterchangeBaseMaterialFactoryNode* MaterialFactoryNode, UInterchangeMaterialExpressionFactoryNode*& TextureSampleFactoryNode);
	void HandleLerpNode(const UInterchangeShaderNode* ShaderNode, UInterchangeBaseMaterialFactoryNode* MaterialFactoryNode, UInterchangeMaterialExpressionFactoryNode* LerpFactoryNode);
	void HandleMaskNode(const UInterchangeShaderNode* ShaderNode, UInterchangeBaseMaterialFactoryNode* MaterialFactoryNode, UInterchangeMaterialExpressionFactoryNode* MaskFactoryNode);
	void HandleTimeNode(const UInterchangeShaderNode* ShaderNode, UInterchangeBaseMaterialFactoryNode* MaterialFactoryNode, UInterchangeMaterialExpressionFactoryNode* TimeFactoryNode);
	void HandleTransformPositionNode(const UInterchangeShaderNode* ShaderNode, UInterchangeBaseMaterialFactoryNode* MaterialFactoryNode, UInterchangeMaterialExpressionFactoryNode* TransformPositionFactoryNode);
	void HandleTransformVectorNode(const UInterchangeShaderNode* ShaderNode, UInterchangeBaseMaterialFactoryNode* MaterialFactoryNode, UInterchangeMaterialExpressionFactoryNode* TransformVectorFactoryNode);
	void HandleNoiseNode(const UInterchangeShaderNode* ShaderNode, UInterchangeBaseMaterialFactoryNode* MaterialFactoryNode, UInterchangeMaterialExpressionFactoryNode* NoiseFactoryNode);
	void HandleVectorNoiseNode(const UInterchangeShaderNode* ShaderNode, UInterchangeBaseMaterialFactoryNode* MaterialFactoryNode, UInterchangeMaterialExpressionFactoryNode* NoiseFactoryNode);
	void HandleSwizzleNode(const UInterchangeShaderNode* ShaderNode, UInterchangeBaseMaterialFactoryNode* MaterialFactoryNode, UInterchangeMaterialExpressionFactoryNode* SwizzleFactoryNode);
	void HandleScalarParameterNode(const UInterchangeShaderNode* ShaderNode, UInterchangeMaterialExpressionFactoryNode* ScalarParameterFactoryNode);
	void HandleVectorParameterNode(const UInterchangeShaderNode* ShaderNode, UInterchangeMaterialExpressionFactoryNode* VectorParameterFactoryNode);
	void HandleStaticBooleanParameterNode(const UInterchangeShaderNode* ShaderNode, UInterchangeMaterialExpressionFactoryNode* StaticBooleanParameterFactoryNode);

	UInterchangeMaterialExpressionFactoryNode* CreateMaterialExpressionForShaderNode(UInterchangeBaseMaterialFactoryNode* MaterialFactoryNode, const UInterchangeShaderNode* ShaderNode, const FString& ParentUid);
	TTuple<UInterchangeMaterialExpressionFactoryNode*, FString> CreateMaterialExpressionForInput(UInterchangeBaseMaterialFactoryNode* MaterialFactoryNode, const UInterchangeShaderNode* ShaderNode, const FString& InputName, const FString& ParentUid);

	UInterchangeMaterialExpressionFactoryNode* CreateExpressionNode(const FString& ExpressionName, const FString& ParentUid, UClass* MaterialExpressionClass);
	
	UInterchangeMaterialExpressionFactoryNode* HandleFloatInput(const UInterchangeShaderNode* ShaderNode, const FString& InputName, const FString& ParentUid, bool bIsAParameter);
	UInterchangeMaterialExpressionFactoryNode* CreateConstantExpression(const UInterchangeShaderNode* ShaderNode, const FString& InputName, const FString& ParentUid);
	UInterchangeMaterialExpressionFactoryNode* CreateScalarParameterExpression(const UInterchangeShaderNode* ShaderNode, const FString& InputName, const FString& ParentUid);
	UInterchangeMaterialExpressionFactoryNode* HandleLinearColorInput(const UInterchangeShaderNode* ShaderNode, const FString& InputName, const FString& ParentUid, bool bIsAParameter);
	UInterchangeMaterialExpressionFactoryNode* CreateConstant3VectorExpression(const UInterchangeShaderNode* ShaderNode, const FString& InputName, const FString& ParentUid);
	UInterchangeMaterialExpressionFactoryNode* CreateVectorParameterExpression(const UInterchangeShaderNode* ShaderNode, const FString& InputName, const FString& ParentUid);
	UInterchangeMaterialExpressionFactoryNode* CreateStaticBooleanParameterExpression(const UInterchangeShaderNode* ShaderNode, const FString& InputName, const FString& ParentUid);
	UInterchangeMaterialExpressionFactoryNode* CreateVector2ParameterExpression(const UInterchangeShaderNode* ShaderNode, const FString& InputName, const FString& ParentUid);
	UInterchangeMaterialExpressionFactoryNode* CreateFunctionCallExpression(const UInterchangeShaderNode* ShaderNode, const FString& MaterialExpressionUid, UInterchangeBaseMaterialFactoryNode* MaterialFactoryNode);

	/**
	 * Visits a given shader node and its connections to find its strongest value.
	 * The goal is to simplify a branch of a node graph to a single value, to be used for material instancing.
	 */
	void VisitShaderGraphNode(const UInterchangeShaderGraphNode* ShaderGraphNode, UInterchangeMaterialInstanceFactoryNode* MaterialInstanceFactoryNode) const;
	void VisitShaderNode(const UInterchangeShaderNode* ShaderNode, UInterchangeMaterialInstanceFactoryNode* MaterialInstanceFactoryNode, TSet<const UInterchangeShaderNode*> & VisitedNodes) const;
	void VisitShaderInput(const UInterchangeShaderNode* ShaderNode, UInterchangeMaterialInstanceFactoryNode* MaterialInstanceFactoryNode, const FString& InputName, TSet<const UInterchangeShaderNode*> & VisitedNodes) const;

	void VisitScalarParameterNode(const UInterchangeShaderNode* ShaderNode, UInterchangeMaterialInstanceFactoryNode* MaterialInstanceFactoryNode) const;
	void VisitTextureSampleNode(const UInterchangeShaderNode* ShaderNode, UInterchangeMaterialInstanceFactoryNode* MaterialInstanceFactoryNode) const;
	void VisitVectorParameterNode(const UInterchangeShaderNode* ShaderNode, UInterchangeMaterialInstanceFactoryNode* MaterialInstanceFactoryNode) const;

	FString GetTextureUidAttributeFromShaderNode(const UInterchangeShaderNode* ShaderNode, FName ParameterName, bool& OutIsAParameter) const;
	FString CreateInputKey(const FString& InputName, bool bIsAParameter) const;
private:
	friend class UE::Interchange::Materials::HashUtils::FDuplicateMaterialHelper;

	enum class EMaterialInputType : uint8
	{
		Unknown,
		Color,
		Vector,
		Scalar
	};
	friend FString LexToString(UInterchangeGenericMaterialPipeline::EMaterialInputType);

	struct FMaterialCreationContext
	{
		EMaterialInputType InputTypeBeingProcessed = EMaterialInputType::Color;
	} MaterialCreationContext;

	struct FMaterialExpressionCreationContext
	{
		FString OutputName; // The name of the output we will be connecting from
	};

	TArray<FMaterialExpressionCreationContext> MaterialExpressionCreationContextStack;

	using FParameterMaterialInputType = TTuple<FString, EMaterialInputType>;

	const UInterchangeBaseNode * AttributeStorageNode = nullptr;
};
