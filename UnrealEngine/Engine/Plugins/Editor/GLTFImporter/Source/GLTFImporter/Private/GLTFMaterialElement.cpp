// Copyright Epic Games, Inc. All Rights Reserved.

#include "GLTFMaterialElement.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "MaterialEditingLibrary.h"
#include "MaterialEditorUtilities.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionThinTranslucentMaterialOutput.h"
#include "Materials/MaterialExpressionClearCoatNormalCustomOutput.h"

namespace GLTFImporterImpl
{
	template <typename T>
	T* NewMaterialExpression(UObject* Parent)
	{
		check(Parent != nullptr);

		T* Expression                      = NewObject<T>(Parent);
		Expression->MaterialExpressionGuid = FGuid::NewGuid();
		Expression->bCollapsed             = true;

		if (Parent->IsA<UMaterial>())
		{
			Cast<UMaterial>(Parent)->GetExpressionCollection().AddExpression(Expression);
		}
		else if (Parent->IsA<UMaterialFunction>())
		{
			Cast<UMaterialFunction>(Parent)->GetExpressionCollection().AddExpression(Expression);
		}

		return Expression;
	}

	template <typename T>
	T* NewMaterialExpressionParameter(UObject* Parent, const FString& Name)
	{
		T* Expression              = NewMaterialExpression<T>(Parent);
		Expression->ExpressionGUID = FGuid::NewGuid();
		Expression->ParameterName  = *Name;
		return Expression;
	}

	UMaterialExpression* NewMaterialExpression(UObject* MaterialOrFunction, UClass* MaterialExpressionClass)
	{
		if (UMaterial* Material = Cast<UMaterial>(MaterialOrFunction))
		{
			return UMaterialEditingLibrary::CreateMaterialExpression(Material, MaterialExpressionClass);
		}
		else if (UMaterialFunction* MaterialFunction = Cast<UMaterialFunction>(MaterialOrFunction))
		{
			return UMaterialEditingLibrary::CreateMaterialExpressionInFunction(MaterialFunction, MaterialExpressionClass);
		}

		return nullptr;
	}

	UClass* FindClass(const TCHAR* ClassName)
	{
		check(ClassName);

		if (UClass* Result = FindFirstObject<UClass>(ClassName, EFindFirstObjectOptions::None, ELogVerbosity::Warning, TEXT("GLTFImporter")))
		{
			return Result;
		}

		if (UObjectRedirector* RenamedClassRedirector = FindFirstObject<UObjectRedirector>(ClassName, EFindFirstObjectOptions::None, ELogVerbosity::Warning, TEXT("GLTFImporter")))
		{
			return CastChecked<UClass>(RenamedClassRedirector->DestinationObject);
		}

		return nullptr;
	}

	UMaterialExpression* CreateTextureExpression(const GLTF::FMaterialExpression* Expression, UMaterial* UnrealMaterial)
	{
		check(Expression->GetType() == GLTF::EMaterialExpressionType::Texture);
		const GLTF::FMaterialExpressionTexture& TextureExpression = *static_cast<const GLTF::FMaterialExpressionTexture*>(Expression);

		UMaterialExpressionTextureSampleParameter2D* MaterialExpression =
		    NewMaterialExpressionParameter<UMaterialExpressionTextureSampleParameter2D>(UnrealMaterial, TextureExpression.GetName());

		if (const FGLTFTextureElement* TextureElement = static_cast<const FGLTFTextureElement*>(TextureExpression.GetTexture()))
		{
			UTexture* Texture               = TextureElement->Texture;
			MaterialExpression->Group       = TextureExpression.GetGroupName();
			MaterialExpression->Texture     = Texture;
			MaterialExpression->SamplerType = UMaterialExpressionTextureBase::GetSamplerTypeForTexture(Texture);
		}

		return MaterialExpression;
	}

	UMaterialExpression* CreateTextureCoordinateExpression(const GLTF::FMaterialExpression* Expression, UMaterial* UnrealMaterial)
	{
		check(Expression->GetType() == GLTF::EMaterialExpressionType::TextureCoordinate);
		const GLTF::FMaterialExpressionTextureCoordinate& TextureCoordinateExpression =
		    *static_cast<const GLTF::FMaterialExpressionTextureCoordinate*>(Expression);

		UMaterialExpressionTextureCoordinate* MaterialExpression = NewMaterialExpression<UMaterialExpressionTextureCoordinate>(UnrealMaterial);
		MaterialExpression->CoordinateIndex                      = TextureCoordinateExpression.GetCoordinateIndex();

		return MaterialExpression;
	}

	UMaterialExpression* CreateGenericExpression(const GLTF::FMaterialExpression* Expression, UMaterial* UnrealMaterial)
	{
		check(Expression->GetType() == GLTF::EMaterialExpressionType::Generic);
		const GLTF::FMaterialExpressionGeneric& GenericExpression = *static_cast<const GLTF::FMaterialExpressionGeneric*>(Expression);

		UClass* ExpressionClass = FindClass(*(FString(TEXT("MaterialExpression")) + GenericExpression.GetExpressionName()));

		if (!ExpressionClass)
		{
			ensure(false);
			return nullptr;
		}

		UMaterialExpression* MaterialExpression = NewMaterialExpression(UnrealMaterial, ExpressionClass);
		if (!MaterialExpression)
		{
			return nullptr;
		}

		if (UMaterialExpressionTextureBase* TextureExpression = Cast<UMaterialExpressionTextureBase>(MaterialExpression))
		{
			TextureExpression->AutoSetSampleType();
		}

		return MaterialExpression;
	}

	UMaterialExpression* CreateFunctionCallExpression(const GLTF::FMaterialExpression* Expression, UMaterial* UnrealMaterial)
	{
		check(Expression->GetType() == GLTF::EMaterialExpressionType::FunctionCall);
		const GLTF::FMaterialExpressionFunctionCall& FunctionCall = *static_cast<const GLTF::FMaterialExpressionFunctionCall*>(Expression);

		const FSoftObjectPath       MaterialFunctionObjectPath(FString(FunctionCall.GetFunctionPathName()));
		UMaterialFunctionInterface* MaterialFunction = Cast<UMaterialFunctionInterface>(MaterialFunctionObjectPath.TryLoad());

		UMaterialExpressionMaterialFunctionCall* MaterialExpression = NewMaterialExpression<UMaterialExpressionMaterialFunctionCall>(UnrealMaterial);
		MaterialExpression->SetMaterialFunction(MaterialFunction);
		MaterialExpression->UpdateFromFunctionResource();

		return MaterialExpression;
	}

	UMaterialExpression* CreateScalarExpression(const GLTF::FMaterialExpression* Expression, UMaterial* UnrealMaterial)
	{
		check(Expression->GetType() == GLTF::EMaterialExpressionType::ConstantScalar);
		const GLTF::FMaterialExpressionScalar& ScalarExpression = *static_cast<const GLTF::FMaterialExpressionScalar*>(Expression);

		UMaterialExpression* Result = nullptr;
		if (FCString::Strlen(ScalarExpression.GetName()) == 0)
		{
			UMaterialExpressionConstant* MaterialExpression = NewMaterialExpression<UMaterialExpressionConstant>(UnrealMaterial);
			MaterialExpression->R                           = ScalarExpression.GetScalar();

			Result = MaterialExpression;
		}
		else
		{
			UMaterialExpressionScalarParameter* MaterialExpression =
			    NewMaterialExpressionParameter<UMaterialExpressionScalarParameter>(UnrealMaterial, ScalarExpression.GetName());
			MaterialExpression->DefaultValue = ScalarExpression.GetScalar();
			MaterialExpression->Group        = ScalarExpression.GetGroupName();

			Result = MaterialExpression;
		}

		return Result;
	}

	UMaterialExpression* CreateColorExpression(const GLTF::FMaterialExpression* Expression, UMaterial* UnrealMaterial)
	{
		check(Expression->GetType() == GLTF::EMaterialExpressionType::ConstantColor);
		const GLTF::FMaterialExpressionColor& ColorExpression = *static_cast<const GLTF::FMaterialExpressionColor*>(Expression);

		UMaterialExpression* Result = nullptr;
		if (FCString::Strlen(ColorExpression.GetName()) == 0)
		{
			UMaterialExpressionConstant3Vector* MaterialExpression = NewMaterialExpression<UMaterialExpressionConstant3Vector>(UnrealMaterial);
			MaterialExpression->Constant                           = ColorExpression.GetColor();

			Result = MaterialExpression;
		}
		else
		{
			UMaterialExpressionVectorParameter* MaterialExpression =
			    NewMaterialExpressionParameter<UMaterialExpressionVectorParameter>(UnrealMaterial, ColorExpression.GetName());
			MaterialExpression->DefaultValue = ColorExpression.GetColor();
			MaterialExpression->Group        = ColorExpression.GetGroupName();

			Result = MaterialExpression;
		}

		return Result;
	}
}

FGLTFMaterialElement::FGLTFMaterialElement(UMaterial* Material)
    : GLTF::FMaterialElement(Material->GetName())
    , Material(Material)
{
	check(Material);
}

int FGLTFMaterialElement::GetBlendMode() const
{
	return Material->BlendMode;
}

void FGLTFMaterialElement::SetBlendMode(int InBlendMode)
{
	Material->BlendMode = static_cast<EBlendMode>(InBlendMode);
}

bool FGLTFMaterialElement::GetTwoSided() const
{
	return Material->IsTwoSided();
}

void FGLTFMaterialElement::SetTwoSided(bool bTwoSided)
{
	Material->TwoSided = bTwoSided;
}

void FGLTFMaterialElement::SetShadingModel(GLTF::EGLTFMaterialShadingModel InShadingModel)
{
	EMaterialShadingModel MaterialShadingModel;

	switch (InShadingModel)
	{
		case GLTF::EGLTFMaterialShadingModel::ClearCoat:
			MaterialShadingModel = EMaterialShadingModel::MSM_ClearCoat;
			break;
		case GLTF::EGLTFMaterialShadingModel::Subsurface:
			MaterialShadingModel = EMaterialShadingModel::MSM_Subsurface;
			break;
		case GLTF::EGLTFMaterialShadingModel::ThinTranslucent:
			MaterialShadingModel = EMaterialShadingModel::MSM_ThinTranslucent;
			break;
		default: MaterialShadingModel = EMaterialShadingModel::MSM_DefaultLit;

	};
	Material->SetShadingModel(MaterialShadingModel);
}

void FGLTFMaterialElement::SetTranslucencyLightingMode(int InLightingMode)
{
	Material->TranslucencyLightingMode = static_cast<ETranslucencyLightingMode>( InLightingMode );
}

void FGLTFMaterialElement::Finalize()
{
	check(!bIsFinal);

	TArray<TStrongObjectPtr<UMaterialExpression> > MaterialExpressions;
	CreateExpressions(MaterialExpressions);

	UMaterialEditorOnlyData* MaterialEditorOnly = Material->GetEditorOnlyData();
	ConnectInput(BaseColor, MaterialExpressions, MaterialEditorOnly->BaseColor);
	ConnectInput(Metallic, MaterialExpressions, MaterialEditorOnly->Metallic);
	ConnectInput(Specular, MaterialExpressions, MaterialEditorOnly->Specular);
	ConnectInput(Roughness, MaterialExpressions, MaterialEditorOnly->Roughness);
	ConnectInput(EmissiveColor, MaterialExpressions, MaterialEditorOnly->EmissiveColor);
	ConnectInput(Opacity, MaterialExpressions, MaterialEditorOnly->Opacity);
	ConnectInput(Refraction, MaterialExpressions, MaterialEditorOnly->Refraction);
	ConnectInput(Normal, MaterialExpressions, MaterialEditorOnly->Normal);
	ConnectInput(AmbientOcclusion, MaterialExpressions, MaterialEditorOnly->AmbientOcclusion);
	ConnectInput(ClearCoat, MaterialExpressions, MaterialEditorOnly->ClearCoat);
	ConnectInput(ClearCoatRoughness, MaterialExpressions, MaterialEditorOnly->ClearCoatRoughness);

	// Handle transmission materials (they add a special output node to the graph)
	if (ThinTranslucentMaterialOutput)
	{
		const int32 ThinTranslucentExpressionIndex = Expressions.Find(ThinTranslucentMaterialOutput);
		
		if (ThinTranslucentExpressionIndex != INDEX_NONE && MaterialExpressions[ThinTranslucentExpressionIndex].IsValid())
		{
			UMaterialExpression& ThinTranslucentMaterialExpression = *MaterialExpressions[ThinTranslucentExpressionIndex];

			GLTF::FMaterialExpressionInput* ThinTranslucentInput = ThinTranslucentMaterialOutput->GetInput(0);

			if (ThinTranslucentInput)
			{
				ConnectInput(*ThinTranslucentInput, MaterialExpressions, *ThinTranslucentMaterialExpression.GetInput(0));
			}
		}
	}

	if (ClearCoatBottomNormalOutput)
	{
		const int32 ClearCoatBottomNormalOutputIndex = Expressions.Find(ClearCoatBottomNormalOutput);

		if (ClearCoatBottomNormalOutputIndex != INDEX_NONE && MaterialExpressions[ClearCoatBottomNormalOutputIndex].IsValid())
		{
			UMaterialExpression& ClearCoatBottomNormalMaterialExpression = *MaterialExpressions[ClearCoatBottomNormalOutputIndex];

			GLTF::FMaterialExpressionInput* ClearCoatBottomNormalInput = ClearCoatBottomNormalOutput->GetInput(0);

			if (ClearCoatBottomNormalInput)
			{
				ConnectInput(*ClearCoatBottomNormalInput, MaterialExpressions, *ClearCoatBottomNormalMaterialExpression.GetInput(0));
			}
		}
	}

	UMaterialEditingLibrary::LayoutMaterialExpressions(Material);

	Material->MarkPackageDirty();
	Material->PostEditChange();
	FAssetRegistryModule::AssetCreated(Material);

	bIsFinal = true;
}

void FGLTFMaterialElement::CreateExpressions(TArray<TStrongObjectPtr<UMaterialExpression> >& MaterialExpressions)
{
	MaterialExpressions.Empty(Expressions.Num());

	for (GLTF::FMaterialExpression* Expression : Expressions)
	{
		using namespace GLTFImporterImpl;

		check(Expression);

		UMaterialExpression* MaterialExpression = nullptr;
		switch (Expression->GetType())
		{
			case GLTF::EMaterialExpressionType::Texture:
				MaterialExpression = CreateTextureExpression(Expression, Material);
				break;
			case GLTF::EMaterialExpressionType::TextureCoordinate:
				MaterialExpression = CreateTextureCoordinateExpression(Expression, Material);
				break;
			case GLTF::EMaterialExpressionType::Generic:
				MaterialExpression = CreateGenericExpression(Expression, Material);
				break;
			case GLTF::EMaterialExpressionType::FunctionCall:
				MaterialExpression = CreateFunctionCallExpression(Expression, Material);
				break;
			case GLTF::EMaterialExpressionType::ConstantScalar:
				MaterialExpression = CreateScalarExpression(Expression, Material);
				break;
			case GLTF::EMaterialExpressionType::ConstantColor:
				MaterialExpression = CreateColorExpression(Expression, Material);
				break;
			default:
				break;
		}

		check(MaterialExpression);

		MaterialExpressions.Add(TStrongObjectPtr<UMaterialExpression>(MaterialExpression));

		if (MaterialExpression->GetClass() == UMaterialExpressionThinTranslucentMaterialOutput::StaticClass())
		{
			ThinTranslucentMaterialOutput = Expression;
		}

		if (MaterialExpression->GetClass() == UMaterialExpressionClearCoatNormalCustomOutput::StaticClass())
		{
			ClearCoatBottomNormalOutput = Expression;
		}
	}
}

void FGLTFMaterialElement::ConnectInput(const GLTF::FMaterialExpressionInput&                 ExpressionInput,
                                        const TArray<TStrongObjectPtr<UMaterialExpression> >& MaterialExpressions,
                                        FExpressionInput&                                     MaterialInput) const
{
	ConnectExpression(ExpressionInput.GetExpression(), Expressions, MaterialExpressions, MaterialInput, ExpressionInput.GetOutputIndex());
}

void FGLTFMaterialElement::ConnectExpression(const GLTF::FMaterialExpression*                      ExpressionPtr,        //
                                             const TArray<GLTF::FMaterialExpression*>&             Expressions,          //
                                             const TArray<TStrongObjectPtr<UMaterialExpression> >& MaterialExpressions,  //
                                             FExpressionInput&                                     ExpressionInput,      //
                                             int32                                                 OutputIndex)
{
	check(Expressions.Num() == MaterialExpressions.Num());

	if (ExpressionPtr == nullptr)
		return;

	GLTF::FMaterialExpression& Expression      = *const_cast<GLTF::FMaterialExpression*>(ExpressionPtr);  // safe as we dont modify it
	const int32                ExpressionIndex = Expressions.Find(&Expression);
	check(ExpressionIndex != INDEX_NONE);
	if (!MaterialExpressions.IsValidIndex(ExpressionIndex))
	{
		check(false);
		return;
	}

	TStrongObjectPtr<UMaterialExpression> MaterialExpression = MaterialExpressions[ExpressionIndex];
	MaterialExpression->ConnectExpression(&ExpressionInput, OutputIndex);

	for (int32 InputIndex = 0; InputIndex < Expression.GetInputCount(); ++InputIndex)
	{
		ConnectExpression(Expression.GetInput(InputIndex)->GetExpression(), Expressions, MaterialExpressions,
		                  *MaterialExpression->GetInput(InputIndex), Expression.GetInput(InputIndex)->GetOutputIndex());
	}
}
