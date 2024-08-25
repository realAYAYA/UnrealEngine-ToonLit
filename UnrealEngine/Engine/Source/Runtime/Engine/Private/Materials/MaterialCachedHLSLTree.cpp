// Copyright Epic Games, Inc. All Rights Reserved.

#include "MaterialCachedHLSLTree.h"
#include "Stats/StatsTrace.h"

#if WITH_EDITOR

#include "Materials/Material.h"
#include "Materials/MaterialAttributeDefinitionMap.h"
#include "Materials/MaterialExpressionCustomOutput.h"
#include "Materials/MaterialExpressionMaterialAttributeLayers.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "MaterialHLSLGenerator.h"
#include "MaterialHLSLTree.h"
#include "HLSLTree/HLSLTreeEmit.h"

DECLARE_STATS_GROUP(TEXT("Material Memory"), STATGROUP_MaterialMemory, STATCAT_Advanced);
DECLARE_MEMORY_STAT(TEXT("Material HLSLTree Memory"), STAT_MaterialMemory_HLSLTree, STATGROUP_MaterialMemory);

const FMaterialCachedHLSLTree FMaterialCachedHLSLTree::EmptyTree;

FMaterialCachedHLSLTree::FMaterialCachedHLSLTree()
	: TypeRegistry(Allocator)
{
}

FMaterialCachedHLSLTree::~FMaterialCachedHLSLTree()
{
	const SIZE_T AllocatedSize = GetAllocatedSize();
	DEC_MEMORY_STAT_BY(STAT_MaterialMemory_HLSLTree, AllocatedSize);
}

SIZE_T FMaterialCachedHLSLTree::GetAllocatedSize() const
{
	return Allocator.GetByteCount();
}

static UE::Shader::EValueType GetShaderType(EMaterialValueType MaterialType)
{
	switch (MaterialType)
	{
	case MCT_Float1: return UE::Shader::EValueType::Float1;
	case MCT_Float2: return UE::Shader::EValueType::Float2;
	case MCT_Float3: return UE::Shader::EValueType::Float3;
	case MCT_Float4: return UE::Shader::EValueType::Float4;
	case MCT_Float: return UE::Shader::EValueType::Float1;
	case MCT_StaticBool: return UE::Shader::EValueType::Bool1;
	case MCT_Bool: return UE::Shader::EValueType::Bool1;
	case MCT_MaterialAttributes: return UE::Shader::EValueType::Struct;
	case MCT_ShadingModel: return UE::Shader::EValueType::Int1;
	case MCT_LWCScalar: return UE::Shader::EValueType::Double1;
	case MCT_LWCVector2: return UE::Shader::EValueType::Double2;
	case MCT_LWCVector3: return UE::Shader::EValueType::Double3;
	case MCT_LWCVector4: return UE::Shader::EValueType::Double4;
	default:return UE::Shader::EValueType::Void;
	}
}

bool FMaterialCachedHLSLTree::GenerateTree(UMaterial* Material, const FMaterialLayersFunctions* LayerOverrides, UMaterialExpression* PreviewExpression)
{
	for (UMaterialExpression* Expression : Material->GetExpressions())
	{
		if (UMaterialExpressionCustomOutput* CustomOutput = Cast<UMaterialExpressionCustomOutput>(Expression))
		{
			if (!CustomOutput->HasCustomSourceOutput())
			{
				// We don't want anything with HasCustomSourceOutput() here (VertexInterpolators)
				MaterialCustomOutputs.Add(CustomOutput);
			}
		}
		else if (UMaterialExpressionMaterialFunctionCall* FunctionCall = Cast<UMaterialExpressionMaterialFunctionCall>(Expression))
		{
			// TODO: avoid updating the same function call multiple times
			FunctionCall->UpdateFromFunctionResource();
		}
		else if (UMaterialExpressionMaterialAttributeLayers* LayersExpression = Cast<UMaterialExpressionMaterialAttributeLayers>(Expression))
		{
			const FMaterialLayersFunctions& MaterialLayers = LayerOverrides ? *LayerOverrides : LayersExpression->DefaultLayers;

			for (int32 LayerIndex = 0; LayerIndex < MaterialLayers.Layers.Num(); ++LayerIndex)
			{
				UMaterialFunctionInterface* LayerFunction = MaterialLayers.Layers[LayerIndex];

				if (LayerFunction && MaterialLayers.EditorOnly.LayerStates[LayerIndex])
				{
					LayerFunction->UpdateFromFunctionResource();
				}
			}

			for (int32 BlendIndex = 0; BlendIndex < MaterialLayers.Blends.Num(); ++BlendIndex)
			{
				UMaterialFunctionInterface* BlendFunction = MaterialLayers.Blends[BlendIndex];
				const int32 LayerIndex = BlendIndex + 1;

				if (BlendFunction
					&& MaterialLayers.Layers.IsValidIndex(LayerIndex)
					&& MaterialLayers.Layers[LayerIndex]
					&& MaterialLayers.EditorOnly.LayerStates[LayerIndex])
				{
					BlendFunction->UpdateFromFunctionResource();
				}
			}
		}
	}

	TArray<UE::Shader::FStructFieldInitializer, TInlineAllocator<MP_MAX + 16>> MaterialAttributeFields;

	const TArray<FGuid>& OrderedVisibleAttributes = FMaterialAttributeDefinitionMap::GetOrderedVisibleAttributeList();
	for (const FGuid& AttributeID : OrderedVisibleAttributes)
	{
		const FString& PropertyName = FMaterialAttributeDefinitionMap::GetAttributeName(AttributeID);
		const EMaterialValueType PropertyType = FMaterialAttributeDefinitionMap::GetValueType(AttributeID);
		const UE::Shader::EValueType ValueType = GetShaderType(PropertyType);

		if (ValueType != UE::Shader::EValueType::Void &&
			ValueType != UE::Shader::EValueType::Struct)
		{
			MaterialAttributeFields.Emplace(PropertyName, ValueType);

			const UE::Shader::FValue DefaultValue = UE::Shader::Cast(FMaterialAttributeDefinitionMap::GetDefaultValue(AttributeID), ValueType);
			MaterialAttributesDefaultValue.Component.Append(DefaultValue.Component);
		}
	}

	// Make sure this array won't resize because TStringBuilder holds pointers to its inline storage
	TArray<TStringBuilder<64>> CustomOutputNames;
	int32 TotalNumOutputs = 0;

	for (UMaterialExpressionCustomOutput* CustomOutput : MaterialCustomOutputs)
	{
		TotalNumOutputs += CustomOutput->GetNumOutputs();
	}
	CustomOutputNames.Empty(TotalNumOutputs);

	for (UMaterialExpressionCustomOutput* CustomOutput : MaterialCustomOutputs)
	{
		const int32 NumOutputs = CustomOutput->GetNumOutputs();
		const FString OutputName = CustomOutput->GetFunctionName();

		check(!CustomOutput->ShouldCompileBeforeAttributes()); // not supported yet, looks like this isn't currently being used

		for (int32 OutputIndex = 0; OutputIndex < NumOutputs; ++OutputIndex)
		{
			const UE::Shader::EValueType ValueType = CustomOutput->GetCustomOutputType(OutputIndex);
			FStringBuilderBase& FormattedName = CustomOutputNames.AddDefaulted_GetRef();
			FormattedName.Appendf(TEXT("%s%d"), *OutputName, OutputIndex);
			MaterialAttributeFields.Emplace(FormattedName.ToView(), ValueType);

			const UE::Shader::FValue DefaultValue(ValueType);
			MaterialAttributesDefaultValue.Component.Append(DefaultValue.Component);
		}
	}

	MaterialAttributeFields.Emplace(TEXT("PrevWorldPositionOffset"), UE::Shader::EValueType::Float3);
	MaterialAttributesDefaultValue.Component.Append({ 0.0f, 0.0f, 0.0f });

	UE::Shader::FStructTypeInitializer MaterialAttributesInitializer;
	MaterialAttributesInitializer.Name = TEXT("FMaterialAttributes");
	MaterialAttributesInitializer.Fields = MaterialAttributeFields;
	MaterialAttributesType = TypeRegistry.NewType(MaterialAttributesInitializer);

	check(MaterialAttributesDefaultValue.Component.Num() == MaterialAttributesType->ComponentTypes.Num());
	MaterialAttributesDefaultValue.Type = MaterialAttributesType;

	VTPageTableResultType = TypeRegistry.NewExternalType(TEXT("VTPageTableResult"));

	HLSLTree = UE::HLSLTree::FTree::Create(Allocator);

	FMaterialHLSLGenerator Generator(Material, LayerOverrides, PreviewExpression, *this);
	const bool bResult = Generator.Generate();
	if (bResult)
	{
		const SIZE_T AllocatedSize = GetAllocatedSize();
		INC_MEMORY_STAT_BY(STAT_MaterialMemory_HLSLTree, AllocatedSize);
	}
	return bResult;
}

void FMaterialCachedHLSLTree::SetRequestedFields(const UE::HLSLTree::FEmitContext& Context, UE::HLSLTree::FRequestedType& OutRequestedType) const
{
	using namespace UE::HLSLTree;

	const EShaderFrequency ShaderFrequency = Context.ShaderFrequency;

	for (UMaterialExpressionCustomOutput* CustomOutput : MaterialCustomOutputs)
	{
		if (CustomOutput->GetShaderFrequency() != ShaderFrequency)
		{
			continue;
		}

		const int32 NumOutputs = CustomOutput->GetNumOutputs();
		const FString OutputName = CustomOutput->GetFunctionName();

		for (int32 OutputIndex = 0; OutputIndex < NumOutputs; ++OutputIndex)
		{
			TStringBuilder<256> FieldName;
			FieldName.Appendf(TEXT("%s%d"), *OutputName, OutputIndex);
			const UE::Shader::FStructField* CustomOutputField = GetMaterialAttributesType()->FindFieldByName(FieldName.ToString());
			check(CustomOutputField);
			OutRequestedType.SetFieldRequested(CustomOutputField);
		}
	}

	int32 NumCustomUVs = 0;
	if (ShaderFrequency == SF_Vertex)
	{
		const Material::FEmitData& EmitData = Context.FindData<Material::FEmitData>();
		for (int32 TexCoordIndex = Material::MaxNumTexCoords - 1; TexCoordIndex >= 0; --TexCoordIndex)
		{
			if (EmitData.IsExternalInputUsed(SF_Pixel, Material::MakeInputTexCoord(TexCoordIndex)))
			{
				NumCustomUVs = TexCoordIndex + 1;
				break;
			}
		}
	}

	const TArray<FGuid>& OrderedVisibleAttributes = FMaterialAttributeDefinitionMap::GetOrderedVisibleAttributeList();
	for (const FGuid& AttributeID : OrderedVisibleAttributes)
	{
		if (FMaterialAttributeDefinitionMap::GetShaderFrequency(AttributeID) == ShaderFrequency)
		{
			const EMaterialProperty Property = FMaterialAttributeDefinitionMap::GetProperty(AttributeID);
			if (Property >= MP_CustomizedUVs0 && Property <= MP_CustomizedUVs7 && Property - MP_CustomizedUVs0 >= NumCustomUVs)
			{
				continue;
			}

			const FString& FieldName = FMaterialAttributeDefinitionMap::GetAttributeName(AttributeID);
			const UE::Shader::FStructField* Field = MaterialAttributesType->FindFieldByName(*FieldName);
			if (Field)
			{
				OutRequestedType.SetFieldRequested(Field);
			}
		}
	}
}

void FMaterialCachedHLSLTree::EmitSharedCode(FStringBuilderBase& OutCode) const
{
	using namespace UE::Shader;
	for (UMaterialExpressionCustomOutput* CustomOutput : MaterialCustomOutputs)
	{
		const int32 NumOutputs = CustomOutput->GetNumOutputs();
		const FString OutputName = CustomOutput->GetFunctionName();
		const EShaderFrequency ShaderFrequency = CustomOutput->GetShaderFrequency();

		// TODO [jonathan.bard] : Add support for UMaterialExpressionCustomOutput::GetMaxOutputs and UMaterialExpressionCustomOutput::AllowMultipleCustomOutputs, these should generate compilation errors. 
		//  Not sure how errors are reported by FMaterialCachedHLSLTree ATM...

		if (CustomOutput->NeedsCustomOutputDefines())
		{
			OutCode.Appendf(TEXT("#define NUM_MATERIAL_OUTPUTS_%s %d\n"), *OutputName.ToUpper(), NumOutputs);
		}

		for (int32 OutputIndex = 0; OutputIndex < NumOutputs; ++OutputIndex)
		{
			const EValueType ValueType = CustomOutput->GetCustomOutputType(OutputIndex);
			const FValueTypeDescription ValueTypeDesc = GetValueTypeDescription(ValueType);
			const bool bIsLWC = ValueTypeDesc.ComponentType == EValueComponentType::Double;

			OutCode.Appendf(TEXT("#define HAVE_%s%d 1\n"), *OutputName, OutputIndex);

			OutCode.Appendf(TEXT("%s %s%d%s(FMaterial%sParameters Parameters) { return Parameters.MaterialAttributes.%s%d; }\n"),
				ValueTypeDesc.Name,
				*OutputName, OutputIndex,
				bIsLWC ? TEXT("_LWC") : TEXT(""),
				ShaderFrequency == SF_Pixel ? TEXT("Pixel") : TEXT("Vertex"),
				*OutputName, OutputIndex);

			if (bIsLWC)
			{
				// Add a wrapper with no suffix to return a non-LWC type
				const EValueType NonLWCType = MakeValueType(EValueComponentType::Float, ValueTypeDesc.NumComponents);
				const FValueTypeDescription NonLWCTypeDesc = GetValueTypeDescription(NonLWCType);
				OutCode.Appendf(TEXT("%s %s%d(FMaterial%sParameters Parameters) { return LWCToFloat(%s%d_LWC(Parameters)); }\n"),
					NonLWCTypeDesc.Name,
					*OutputName, OutputIndex,
					ShaderFrequency == SF_Vertex ? TEXT("Vertex") : TEXT("Pixel"),
					*OutputName, OutputIndex);
			}
			else
			{
				// Add a wrapper with LWC suffix to return a LWC type
				const EValueType LWCType = MakeValueType(EValueComponentType::Double, ValueTypeDesc.NumComponents);
				const FValueTypeDescription LWCTypeDesc = GetValueTypeDescription(LWCType);
				OutCode.Appendf(TEXT("%s %s%d_LWC(FMaterial%sParameters Parameters) { return LWCPromote(%s%d(Parameters)); }\n"),
					LWCTypeDesc.Name,
					*OutputName, OutputIndex,
					ShaderFrequency == SF_Vertex ? TEXT("Vertex") : TEXT("Pixel"),
					*OutputName, OutputIndex);
			}
		}
		OutCode.Append(TEXT("\n"));
	}
}

bool FMaterialCachedHLSLTree::IsAttributeUsed(UE::HLSLTree::FEmitContext& Context,
	UE::HLSLTree::FEmitScope& Scope,
	const UE::HLSLTree::FPreparedType& ResultType,
	EMaterialProperty Property) const
{
	using namespace UE::HLSLTree;
	using namespace UE::Shader;

	const FStructField* PropertyField = GetMaterialAttributesType()->FindFieldByName(*FMaterialAttributeDefinitionMap::GetAttributeName(Property));
	if (!PropertyField)
	{
		return false;
	}

	FRequestedType RequestedType(GetMaterialAttributesType(), false);
	RequestedType.SetFieldRequested(PropertyField);

	const int32 NumComponents = PropertyField->GetNumComponents();
	const EExpressionEvaluation Evaluation = ResultType.GetFieldEvaluation(Scope, RequestedType, PropertyField->ComponentIndex, NumComponents);
	if (Evaluation == EExpressionEvaluation::None)
	{
		return false;
	}
	else if (Evaluation == EExpressionEvaluation::Constant || Evaluation == EExpressionEvaluation::ConstantZero)
	{
		const FValue& DefaultValue = GetMaterialAttributesDefaultValue();
		const FValue ConstantValue = ResultExpression->GetValueConstant(Context, Scope, RequestedType);
		if (DefaultValue.GetType() != ConstantValue.GetType())
		{
			ensure(false); // expected types to match
			return true;
		}

		for (int32 Index = 0; Index < NumComponents; ++Index)
		{
			const int32 ComponentIndex = PropertyField->ComponentIndex + Index;
			if (DefaultValue.Component[ComponentIndex].Packed != ConstantValue.Component[ComponentIndex].Packed)
			{
				// Non-default value, flag as used
				return true;
			}
		}
		return false;
	}

	return true;
}

#endif // WITH_EDITOR
