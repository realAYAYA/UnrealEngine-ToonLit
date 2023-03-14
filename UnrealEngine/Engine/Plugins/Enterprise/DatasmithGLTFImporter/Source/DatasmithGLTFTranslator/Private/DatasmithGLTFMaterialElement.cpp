// Copyright Epic Games, Inc. All Rights Reserved.

#include "DatasmithGLTFMaterialElement.h"

#include "DatasmithMaterialElements.h"
#include "DatasmithSceneFactory.h"

namespace DatasmithGLTFImporterImpl
{
	template <typename T>
	void SetParameter(const GLTF::FMaterialExpressionParameter& ParameterExpression, T& NewExpression)
	{
		NewExpression.SetName(ParameterExpression.GetName());
		NewExpression.SetGroupName(ParameterExpression.GetGroupName());
	}
}

FDatasmithGLTFMaterialElement::FDatasmithGLTFMaterialElement(const TSharedPtr<IDatasmithUEPbrMaterialElement>& MaterialElement)
    : GLTF::FMaterialElement(MaterialElement->GetName())
    , MaterialElement(MaterialElement)
{
	check(MaterialElement);
}

int FDatasmithGLTFMaterialElement::GetBlendMode() const
{
	return MaterialElement->GetBlendMode();
}

void FDatasmithGLTFMaterialElement::SetBlendMode(int InBlendMode)
{
	MaterialElement->SetBlendMode(InBlendMode);
}

bool FDatasmithGLTFMaterialElement::GetTwoSided() const
{
	return MaterialElement->GetTwoSided();
}

void FDatasmithGLTFMaterialElement::SetTwoSided(bool bTwoSided)
{
	MaterialElement->SetTwoSided(bTwoSided);
}

void FDatasmithGLTFMaterialElement::SetShadingModel(GLTF::EGLTFMaterialShadingModel InShadingModel)
{
	EDatasmithShadingModel DatasmithShadingModel;

	switch (InShadingModel)
	{
		case GLTF::EGLTFMaterialShadingModel::Unlit: 
			DatasmithShadingModel = EDatasmithShadingModel::Unlit; 
			break;
		case GLTF::EGLTFMaterialShadingModel::ClearCoat: 
			DatasmithShadingModel = EDatasmithShadingModel::ClearCoat; 
			break;
		case GLTF::EGLTFMaterialShadingModel::Subsurface:
			DatasmithShadingModel = EDatasmithShadingModel::Subsurface;
			break;
		case GLTF::EGLTFMaterialShadingModel::ThinTranslucent:
			DatasmithShadingModel = EDatasmithShadingModel::ThinTranslucent;
			break;
		case GLTF::EGLTFMaterialShadingModel::DefaultLit:
			DatasmithShadingModel = EDatasmithShadingModel::DefaultLit;
			break;
		default: DatasmithShadingModel = EDatasmithShadingModel::DefaultLit;
	}

	MaterialElement->SetShadingModel(DatasmithShadingModel);
}

void FDatasmithGLTFMaterialElement::SetTranslucencyLightingMode(int InLightingMode)
{
	MaterialElement->SetTranslucencyLightingMode(InLightingMode);
}

void FDatasmithGLTFMaterialElement::Finalize()
{
	check(!bIsFinal);

	TArray<IDatasmithMaterialExpression*> MaterialExpressions;
	CreateExpressions(MaterialExpressions);
	ConnectInput(BaseColor, MaterialExpressions, MaterialElement->GetBaseColor());
	ConnectInput(Metallic, MaterialExpressions, MaterialElement->GetMetallic());
	ConnectInput(Specular, MaterialExpressions, MaterialElement->GetSpecular());
	ConnectInput(Roughness, MaterialExpressions, MaterialElement->GetRoughness());
	ConnectInput(EmissiveColor, MaterialExpressions, MaterialElement->GetEmissiveColor());
	ConnectInput(Opacity, MaterialExpressions, MaterialElement->GetOpacity());
	ConnectInput(Refraction, MaterialExpressions, MaterialElement->GetRefraction());
	ConnectInput(Normal, MaterialExpressions, MaterialElement->GetNormal());
	ConnectInput(AmbientOcclusion, MaterialExpressions, MaterialElement->GetAmbientOcclusion());
	ConnectInput(ClearCoat, MaterialExpressions, MaterialElement->GetClearCoat());
	ConnectInput(ClearCoatRoughness, MaterialExpressions, MaterialElement->GetClearCoatRoughness());

	if (ThinTranslucentMaterialOutput)
	{
		const int32 ThinTranslucentExpressionIndex = Expressions.Find(ThinTranslucentMaterialOutput);
		check(ThinTranslucentExpressionIndex != INDEX_NONE);
		IDatasmithMaterialExpression* ThinTranslucentMaterialExpression = MaterialExpressions[ThinTranslucentExpressionIndex];

		GLTF::FMaterialExpressionInput* ThinTranslucentInput = ThinTranslucentMaterialOutput->GetInput(0);

		if (ThinTranslucentInput && ThinTranslucentMaterialExpression)
		{
			ConnectInput(*ThinTranslucentInput, MaterialExpressions, *ThinTranslucentMaterialExpression->GetInput(0));
		}
	}

	if (ClearCoatBottomNormalOutput)
	{
		const int32 ClearCoatBottomNormalOutputIndex = Expressions.Find(ClearCoatBottomNormalOutput);
		check(ClearCoatBottomNormalOutputIndex != INDEX_NONE);
		IDatasmithMaterialExpression* ClearCoatBottomNormalMaterialExpression = MaterialExpressions[ClearCoatBottomNormalOutputIndex];

		GLTF::FMaterialExpressionInput* ClearCoatBottomNormalInput = ClearCoatBottomNormalOutput->GetInput(0);

		if (ClearCoatBottomNormalInput && ClearCoatBottomNormalMaterialExpression)
		{
			ConnectInput(*ClearCoatBottomNormalInput, MaterialExpressions, *ClearCoatBottomNormalMaterialExpression->GetInput(0));
		}
	}

	bIsFinal = true;
}

void FDatasmithGLTFMaterialElement::CreateExpressions(TArray<IDatasmithMaterialExpression*>& MaterialExpressions)
{
	MaterialExpressions.Empty(Expressions.Num());

	for (GLTF::FMaterialExpression* Expression : Expressions)
	{
		using namespace DatasmithGLTFImporterImpl;

		check(Expression);
		switch (Expression->GetType())
		{
			case GLTF::EMaterialExpressionType::Texture:
			{
				const GLTF::FMaterialExpressionTexture& TextureExpression = *static_cast<const GLTF::FMaterialExpressionTexture*>(Expression);
				const IDatasmithTextureElement*         TextureElement =
				    static_cast<const FDatasmithGLTFTextureElement*>(TextureExpression.GetTexture())->GetTexture();

				IDatasmithMaterialExpressionTexture* NewExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionTexture>();
				NewExpression->SetTexturePathName(TextureElement->GetName());
				SetParameter(TextureExpression, *NewExpression);
				MaterialExpressions.Add(NewExpression);
			}
			break;
			case GLTF::EMaterialExpressionType::TextureCoordinate:
			{
				const GLTF::FMaterialExpressionTextureCoordinate& TextureExpression =
				    *static_cast<const GLTF::FMaterialExpressionTextureCoordinate*>(Expression);
				IDatasmithMaterialExpressionTextureCoordinate* NewExpression =
				    MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionTextureCoordinate>();
				NewExpression->SetCoordinateIndex(TextureExpression.GetCoordinateIndex());

				MaterialExpressions.Add(NewExpression);
			}
			break;
			case GLTF::EMaterialExpressionType::Generic:
			{
				GLTF::FMaterialExpressionGeneric* GenericExpression = static_cast<GLTF::FMaterialExpressionGeneric*>(Expression);
				IDatasmithMaterialExpressionGeneric* NewExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
				const FString ExprName(GenericExpression->GetExpressionName());
				NewExpression->SetExpressionName(*ExprName);

				// Check for special expressions
				if (ExprName == "ClearCoatNormalCustomOutput")
				{
					ClearCoatBottomNormalOutput = GenericExpression;
				}
				else if (ExprName == "ThinTranslucentMaterialOutput")
				{
					ThinTranslucentMaterialOutput = GenericExpression;
				}

				for (const TPair<FString, bool>& NameValue : GenericExpression->GetBoolProperties())
				{
					TSharedPtr<IDatasmithKeyValueProperty> PropertyPtr = FDatasmithSceneFactory::CreateKeyValueProperty(*NameValue.Key);
					PropertyPtr->SetPropertyType(EDatasmithKeyValuePropertyType::Bool);
					PropertyPtr->SetValue(NameValue.Value ? TEXT("True") : TEXT("False"));
					NewExpression->AddProperty(PropertyPtr);
				}

				for (const TPair<FString, float>& NameValue : GenericExpression->GetFloatProperties())
				{
					TSharedPtr<IDatasmithKeyValueProperty> PropertyPtr = FDatasmithSceneFactory::CreateKeyValueProperty(*NameValue.Key);
					PropertyPtr->SetPropertyType(EDatasmithKeyValuePropertyType::Float);
					PropertyPtr->SetValue(*FString::SanitizeFloat(NameValue.Value));
					NewExpression->AddProperty(PropertyPtr);
				}

				MaterialExpressions.Add(NewExpression);
			}
			break;
			case GLTF::EMaterialExpressionType::FunctionCall:
			{
				const GLTF::FMaterialExpressionFunctionCall& GenericExpression =
				    *static_cast<const GLTF::FMaterialExpressionFunctionCall*>(Expression);
				IDatasmithMaterialExpressionFunctionCall* NewExpression =
				    MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionFunctionCall>();
				NewExpression->SetFunctionPathName(GenericExpression.GetFunctionPathName());
				MaterialExpressions.Add(NewExpression);
			}
			break;
			case GLTF::EMaterialExpressionType::ConstantScalar:
			{
				const GLTF::FMaterialExpressionScalar& ScalarExpression = *static_cast<const GLTF::FMaterialExpressionScalar*>(Expression);
				IDatasmithMaterialExpressionScalar*    NewExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
				NewExpression->GetScalar()                           = ScalarExpression.GetScalar();
				SetParameter(ScalarExpression, *NewExpression);

				MaterialExpressions.Add(NewExpression);
			}
			break;
			case GLTF::EMaterialExpressionType::ConstantColor:
			{
				const GLTF::FMaterialExpressionColor& ColorExpression = *static_cast<const GLTF::FMaterialExpressionColor*>(Expression);
				IDatasmithMaterialExpressionColor*    NewExpression   = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionColor>();
				NewExpression->GetColor()                             = ColorExpression.GetColor();
				SetParameter(ColorExpression, *NewExpression);

				MaterialExpressions.Add(NewExpression);
			}
			break;
			default:
				check(false);
				break;
		}
	}
}

void FDatasmithGLTFMaterialElement::ConnectInput(const GLTF::FMaterialExpressionInput&        ExpressionInput,
                                                 const TArray<IDatasmithMaterialExpression*>& MaterialExpressions,
                                                 IDatasmithExpressionInput&                   MaterialInput) const
{
	ConnectExpression(ExpressionInput.GetExpression(), Expressions, MaterialExpressions, MaterialInput, ExpressionInput.GetOutputIndex());
}

void FDatasmithGLTFMaterialElement::ConnectExpression(const GLTF::FMaterialExpression*             ExpressionPtr,        //
                                                      const TArray<GLTF::FMaterialExpression*>&    Expressions,          //
                                                      const TArray<IDatasmithMaterialExpression*>& MaterialExpressions,  //
                                                      IDatasmithExpressionInput&                   ExpressionInput,      //
                                                      int32                                        OutputIndex)
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

	IDatasmithMaterialExpression* MaterialExpression = MaterialExpressions[ExpressionIndex];
	MaterialExpression->ConnectExpression(ExpressionInput, OutputIndex);

	for (int32 InputIndex = 0; InputIndex < Expression.GetInputCount(); ++InputIndex)
	{
		ConnectExpression(Expression.GetInput(InputIndex)->GetExpression(), Expressions, MaterialExpressions,
		                  *MaterialExpression->GetInput(InputIndex), Expression.GetInput(InputIndex)->GetOutputIndex());
	}
}
