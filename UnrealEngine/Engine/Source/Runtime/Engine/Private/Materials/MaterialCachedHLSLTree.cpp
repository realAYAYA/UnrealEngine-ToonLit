// Copyright Epic Games, Inc. All Rights Reserved.

#include "MaterialCachedHLSLTree.h"

#if WITH_EDITOR

#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustomOutput.h"
#include "MaterialHLSLGenerator.h"

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
	const EMaterialShadingModel DefaultShadingModel = Material->GetShadingModels().GetFirstShadingModel();

	for (UMaterialExpression* Expression : Material->GetExpressions())
	{
		UMaterialExpressionCustomOutput* CustomOutput = Cast<UMaterialExpressionCustomOutput>(Expression);
		// We don't want anything with HasCustomSourceOutput() here (VertexInterpolators)
		if (CustomOutput && !CustomOutput->HasCustomSourceOutput())
		{
			MaterialCustomOutputs.Add(CustomOutput);
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

			if (PropertyType == MCT_ShadingModel)
			{
				check(ValueType == UE::Shader::EValueType::Int1);
				MaterialAttributesDefaultValue.Component.Add((int32)DefaultShadingModel);
			}
			else
			{
				const UE::Shader::FValue DefaultValue = UE::Shader::Cast(FMaterialAttributeDefinitionMap::GetDefaultValue(AttributeID), ValueType);
				MaterialAttributesDefaultValue.Component.Append(DefaultValue.Component);
			}
		}
	}

	TArray<TStringBuilder<256>, TInlineAllocator<4>> CustomOutputNames;
	CustomOutputNames.Reserve(MaterialCustomOutputs.Num());
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

void FMaterialCachedHLSLTree::SetRequestedFields(EShaderFrequency ShaderFrequency, UE::HLSLTree::FRequestedType& OutRequestedType) const
{
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

	const TArray<FGuid>& OrderedVisibleAttributes = FMaterialAttributeDefinitionMap::GetOrderedVisibleAttributeList();
	for (const FGuid& AttributeID : OrderedVisibleAttributes)
	{
		if (FMaterialAttributeDefinitionMap::GetShaderFrequency(AttributeID) == ShaderFrequency)
		{
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

		if (CustomOutput->NeedsCustomOutputDefines())
		{
			OutCode.Appendf(TEXT("#define NUM_MATERIAL_OUTPUTS_%s %d\n"), *OutputName.ToUpper(), NumOutputs);
		}

		for (int32 OutputIndex = 0; OutputIndex < NumOutputs; ++OutputIndex)
		{
			const EValueType ValueType = CustomOutput->GetCustomOutputType(OutputIndex);
			const FValueTypeDescription ValueTypeDesc = GetValueTypeDescription(ValueType);

			OutCode.Appendf(TEXT("#define HAVE_%s%d 1\n"), *OutputName, OutputIndex);

			OutCode.Appendf(TEXT("%s %s%d(FMaterial%sParameters Parameters) { return Parameters.MaterialAttributes.%s%d; }\n"),
				ValueTypeDesc.Name,
				*OutputName, OutputIndex,
				ShaderFrequency == SF_Pixel ? TEXT("Pixel") : TEXT("Vertex"),
				*OutputName, OutputIndex);
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
