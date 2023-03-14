// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GLTFMaterialExpressions.h"

class UMaterialExpression;
class UMaterial;
class UTexture;
struct FExpressionInput;

class FGLTFTextureElement : public GLTF::ITextureElement
{
public:
	UTexture* Texture;

	FGLTFTextureElement(UTexture& Texture)
	    : Texture(&Texture)
	{
	}
};

class FGLTFMaterialElement : public GLTF::FMaterialElement
{
public:
	FGLTFMaterialElement(UMaterial* Material);

	virtual int  GetBlendMode() const override;
	virtual void SetBlendMode(int InBlendMode) override;
	virtual bool GetTwoSided() const override;
	virtual void SetTwoSided(bool bTwoSided) override;
	virtual void SetShadingModel(GLTF::EGLTFMaterialShadingModel InShadingModel);
	virtual void SetTranslucencyLightingMode(int InLightingMode) override;
	virtual void Finalize() override;

	UMaterial* GetMaterial() const;

private:
	void CreateExpressions(TArray<TStrongObjectPtr<UMaterialExpression> >& MaterialExpressions);

	void ConnectInput(const GLTF::FMaterialExpressionInput&                 ExpressionInput,
	                  const TArray<TStrongObjectPtr<UMaterialExpression> >& MaterialExpressions, FExpressionInput& MaterialInput) const;

	static void ConnectExpression(const GLTF::FMaterialExpression*                      Expression,           //
	                              const TArray<GLTF::FMaterialExpression*>&             Expressions,          //
	                              const TArray<TStrongObjectPtr<UMaterialExpression> >& MaterialExpressions,  //
	                              FExpressionInput&                                     ExpressionInput,      //
	                              int32                                                 OutputIndex);

private:
	UMaterial* Material;
};

inline UMaterial* FGLTFMaterialElement::GetMaterial() const
{
	return Material;
}
