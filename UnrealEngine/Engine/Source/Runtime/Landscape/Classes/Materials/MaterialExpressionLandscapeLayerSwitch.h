// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "MaterialExpressionIO.h"
#include "MaterialValueType.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionLandscapeLayerSwitch.generated.h"

class UTexture;
struct FMaterialParameterInfo;

UCLASS(collapsecategories, hidecategories=Object, MinimalAPI)
class UMaterialExpressionLandscapeLayerSwitch : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	FExpressionInput LayerUsed;

	UPROPERTY()
	FExpressionInput LayerNotUsed;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionLandscapeLayerSwitch)
	FName ParameterName;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionLandscapeLayerSwitch)
	uint32 PreviewUsed:1;

public:

	//~ Begin UObject Interface
	LANDSCAPE_API virtual void Serialize(FStructuredArchive::FRecord Record) override;
	//~ End UObject Interface

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	LANDSCAPE_API virtual bool IsResultMaterialAttributes(int32 OutputIndex) override;
	LANDSCAPE_API virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual bool CanRenameNode() const override { return true; }
	LANDSCAPE_API virtual FString GetEditableName() const override;
	LANDSCAPE_API virtual void SetEditableName(const FString& NewName) override;
	LANDSCAPE_API virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	LANDSCAPE_API virtual bool MatchesSearchQuery(const TCHAR* SearchQuery) override;
	virtual bool GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const override;
#endif
	LANDSCAPE_API virtual UObject* GetReferencedTexture() const override;
	LANDSCAPE_API virtual ReferencedTextureArray GetReferencedTextures() const override;
	virtual bool CanReferenceTexture() const override { return true; }
#if WITH_EDITOR
	virtual uint32 GetInputType(int32 InputIndex) override {return MCT_Unknown;}
	virtual uint32 GetOutputType(int32 InputIndex) override {return MCT_Unknown;}

	/**
	 * Gets the landscape layer names
	 */
	LANDSCAPE_API virtual void GetLandscapeLayerNames(TArray<FName>& OutLayers) const override;
#endif
	//~ End UMaterialExpression Interface

	//~ Begin UObject Interface
	/**
	 * Do any object-specific cleanup required immediately after loading an object,
	 * and immediately after any undo/redo.
	 */
	LANDSCAPE_API virtual void PostLoad() override;
	//~ End UObject Interface
};



