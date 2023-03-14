// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GLTFMaterialExpressions.h"

class IDatasmithUEPbrMaterialElement;
class IDatasmithTextureElement;
class IDatasmithMaterialExpression;
class IDatasmithExpressionInput;

class FDatasmithGLTFTextureElement : public GLTF::ITextureElement
{
public:
	FDatasmithGLTFTextureElement(const TSharedPtr<IDatasmithTextureElement>& Texture)
	    : Texture(Texture)
	{
		check(Texture);
	}
	IDatasmithTextureElement* GetTexture() const;

private:
	TSharedPtr<IDatasmithTextureElement> Texture;
};

class FDatasmithGLTFMaterialElement : public GLTF::FMaterialElement
{
public:
	FDatasmithGLTFMaterialElement(const TSharedPtr<IDatasmithUEPbrMaterialElement>& MaterialElement);

	virtual int  GetBlendMode() const override;
	virtual void SetBlendMode(int InBlendMode) override;
	virtual bool GetTwoSided() const override;
	virtual void SetTwoSided(bool bTwoSided) override;
	virtual void SetShadingModel(GLTF::EGLTFMaterialShadingModel InShadingModel);
	virtual void SetTranslucencyLightingMode(int InLightingMode) override;
	virtual void Finalize() override;

	TSharedRef<IDatasmithUEPbrMaterialElement> GetMaterial() const;

private:
	void CreateExpressions(TArray<IDatasmithMaterialExpression*>& MaterialExpressions);

	void ConnectInput(const GLTF::FMaterialExpressionInput& ExpressionInput, const TArray<IDatasmithMaterialExpression*>& MaterialExpressions,
	                  IDatasmithExpressionInput& MaterialInput) const;

	static void ConnectExpression(const GLTF::FMaterialExpression*             Expression,           //
	                              const TArray<GLTF::FMaterialExpression*>&    Expressions,          //
	                              const TArray<IDatasmithMaterialExpression*>& MaterialExpressions,  //
	                              IDatasmithExpressionInput&                   ExpressionInput,      //
	                              int32                                        OutputIndex);

private:
	TSharedPtr<IDatasmithUEPbrMaterialElement> MaterialElement;
};

inline IDatasmithTextureElement* FDatasmithGLTFTextureElement::GetTexture() const
{
	return Texture.Get();
}

inline TSharedRef<IDatasmithUEPbrMaterialElement> FDatasmithGLTFMaterialElement::GetMaterial() const
{
	return MaterialElement.ToSharedRef();
}
