// Copyright Epic Games, Inc. All Rights Reserved.
#include "MaterialHLSLGenerator.h"

#if WITH_EDITOR

#include "Materials/MaterialAttributeDefinitionMap.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionCustomOutput.h"
#include "Materials/MaterialExpressionExecBegin.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialFunction.h"
#include "MaterialHLSLTree.h"
#include "Materials/Material.h"
#include "MaterialCachedHLSLTree.h"
#include "Misc/MemStackUtility.h"
#include "ParameterCollection.h"

FMaterialHLSLGenerator::FMaterialHLSLGenerator(UMaterial* Material,
	const FMaterialLayersFunctions* InLayerOverrides,
	UMaterialExpression* InPreviewExpression,
	FMaterialCachedHLSLTree& OutCachedTree)
	: TargetMaterial(Material)
	, LayerOverrides(InLayerOverrides)
	, PreviewExpression(InPreviewExpression)
	, CachedTree(OutCachedTree)
	, bGeneratedResult(false)
{
	FunctionCallStack.Add(&RootFunctionCallEntry);
}

const UMaterial* FMaterialHLSLGenerator::GetTargetMaterial() const
{
	return TargetMaterial;
}

UE::HLSLTree::FTree& FMaterialHLSLGenerator::GetTree() const
{
	return CachedTree.GetTree();
}

UE::Shader::FStructTypeRegistry& FMaterialHLSLGenerator::GetTypeRegistry() const
{
	return CachedTree.GetTypeRegistry();
}

const UE::Shader::FStructType* FMaterialHLSLGenerator::GetMaterialAttributesType() const
{
	return CachedTree.GetMaterialAttributesType();
}

const UE::HLSLTree::FExpression* FMaterialHLSLGenerator::GetMaterialAttributesDefaultExpression() const
{
	using namespace UE::HLSLTree;

	FTree& Tree = GetTree();
	const UE::Shader::FStructType* StructType = GetMaterialAttributesType();
	const FExpression* OutExpression = Tree.NewConstant(CachedTree.GetMaterialAttributesDefaultValue());

	// Some default values are unknown at tree generation time
	{
		const UE::Shader::FStructField* ShadingModelField = StructType->FindFieldByName(*FMaterialAttributeDefinitionMap::GetAttributeName(MP_ShadingModel));
		const FExpression* ShadingModelExpression = Tree.NewExpression<Material::FExpressionDefaultShadingModel>();
		OutExpression = Tree.NewExpression<FExpressionSetStructField>(StructType, ShadingModelField, OutExpression, ShadingModelExpression);
	}

	{
		const UE::Shader::FStructField* SubsurfaceColorField = StructType->FindFieldByName(*FMaterialAttributeDefinitionMap::GetAttributeName(MP_SubsurfaceColor));
		const FExpression* SubsurfaceColorExpression = Tree.NewExpression<Material::FExpressionDefaultSubsurfaceColor>();
		OutExpression = Tree.NewExpression<FExpressionSetStructField>(StructType, SubsurfaceColorField, OutExpression, SubsurfaceColorExpression);
	}

	// Some material attribute defaults aren't compile time constants
	if (ensure(TargetMaterial) && !TargetMaterial->bTangentSpaceNormal)
	{
		const UE::Shader::FStructField* NormalField = StructType->FindFieldByName(*FMaterialAttributeDefinitionMap::GetAttributeName(MP_Normal));
		const UE::Shader::FStructField* TangentField = StructType->FindFieldByName(*FMaterialAttributeDefinitionMap::GetAttributeName(MP_Tangent));

		const FExpression* NormalExpression = Tree.NewExpression<Material::FExpressionExternalInput>(Material::EExternalInput::WorldVertexNormal);
		const FExpression* TangentExpression = Tree.NewExpression<Material::FExpressionExternalInput>(Material::EExternalInput::WorldVertexTangent);

		OutExpression = Tree.NewExpression<FExpressionSetStructField>(StructType, NormalField, OutExpression, NormalExpression);
		OutExpression = Tree.NewExpression<FExpressionSetStructField>(StructType, TangentField, OutExpression, TangentExpression);
	}

	for (int32 CustomUVProperty = MP_CustomizedUVs0; CustomUVProperty <= MP_CustomizedUVs7; ++CustomUVProperty)
	{
		const UE::Shader::FStructField* CustomUVField = StructType->FindFieldByName(*FMaterialAttributeDefinitionMap::GetAttributeName((EMaterialProperty)CustomUVProperty));
		const Material::EExternalInput TexCoordInput = Material::EExternalInput((uint8)Material::EExternalInput::TexCoord0 + uint8(CustomUVProperty - MP_CustomizedUVs0));
		const FExpression* TexCoordExpression = Tree.NewExpression<Material::FExpressionExternalInput>(TexCoordInput);
		OutExpression = Tree.NewExpression<FExpressionSetStructField>(StructType, CustomUVField, OutExpression, TexCoordExpression);
	}

	return OutExpression;
}

UMaterialExpression* FMaterialHLSLGenerator::GetCurrentExpression() const
{
	return Cast<UMaterialExpression>(GetTree().GetCurrentOwner());
}

bool FMaterialHLSLGenerator::Generate()
{
	const bool bResult = InternalGenerate();
	if (!bResult)
	{
		CachedTree.ResultScope = &GetTree().GetRootScope();
		CachedTree.ResultExpression = GetTree().NewExpression<UE::HLSLTree::FExpressionError>(AcquireError());
	}
	return bResult;
}

bool FMaterialHLSLGenerator::InternalGenerate()
{
	using namespace UE::HLSLTree;

	FScope& RootScope = CachedTree.GetTree().GetRootScope();

	bool bResult = false;
	if (TargetMaterial->IsUsingControlFlow())
	{
		UMaterialExpression* BaseExpression = TargetMaterial->GetExpressionExecBegin();
		if (!BaseExpression)
		{
			bResult = Error(TEXT("Missing ExpressionExecBegin"));
		}
		else
		{
			bResult = GenerateStatements(RootScope, BaseExpression);
		}
	}
	else
	{
		bResult = GenerateResult(RootScope);
	}

	if (!bResult)
	{
		return false;
	}

	check(FunctionCallStack.Num() == 1);
	if (!bGeneratedResult)
	{
		return Error(TEXT("Missing connection to material output"));
	}

	if (!CachedTree.GetResultExpression() || !CachedTree.GetResultScope())
	{
		return Error(TEXT("Failed to initialize result"));
	}

	for (const auto& It : StatementMap)
	{
		const UMaterialExpression* Expression = It.Key;
		const FStatementEntry& Entry = It.Value;
		if (Entry.NumInputs != Expression->NumExecutionInputs)
		{
			return Error(TEXT("Invalid number of input connections"));
		}
	}

	if (JoinedScopeStack.Num() != 0)
	{
		return Error(TEXT("Invalid control flow"));
	}

	return GetTree().Finalize();
}

static const UE::HLSLTree::FExpression* CompileMaterialInput(FMaterialHLSLGenerator& Generator,
	UE::HLSLTree::FScope& Scope,
	EMaterialProperty InputProperty,
	UMaterial* Material)
{
	using namespace UE::HLSLTree;

	const FExpression* Expression = nullptr;
	// Not checking Material->IsPropertyActive here because the tree is shared with material instances
	FMaterialInputDescription InputDescription;
	if (Material->GetExpressionInputDescription(InputProperty, InputDescription))
	{
		UE::Shader::FValue DefaultValue = UE::Shader::Cast(FMaterialAttributeDefinitionMap::GetDefaultValue(InputProperty), InputDescription.Type);
		if (InputDescription.bUseConstant)
		{
			if (InputDescription.ConstantValue != DefaultValue)
			{
				Expression = Generator.NewConstant(InputDescription.ConstantValue);
			}
		}
		else
		{
			check(InputDescription.Input);
			Expression = InputDescription.Input->TryAcquireHLSLExpression(Generator, Scope, (int32)InputProperty);
			if (Expression)
			{
				Expression = Generator.GetTree().NewExpression<FExpressionDefaultValue>(Expression, DefaultValue);
			}
		}
	}

	return Expression;
}

bool FMaterialHLSLGenerator::GenerateResult(UE::HLSLTree::FScope& Scope)
{
	using namespace UE::HLSLTree;
	using namespace UE::Shader;

	FFunctionCallEntry* FunctionEntry = FunctionCallStack.Last();

	bool bResult = false;
	if (FunctionEntry->MaterialFunction)
	{
		// Result for function call
		FFunction* HLSLFunction = FunctionEntry->HLSLFunction;
		HLSLFunction->OutputExpressions.Reserve(FunctionEntry->FunctionOutputs.Num());
		for (int32 OutputIndex = 0; OutputIndex < FunctionEntry->FunctionOutputs.Num(); ++OutputIndex)
		{
			UMaterialExpressionFunctionOutput* ExpressionOutput = FunctionEntry->FunctionOutputs[OutputIndex];
			//FOwnerScope TreeOwnerScope(GetTree(), ExpressionOutput);
			FScopedGenerateFunctionOutput GenerateOutputScope(FunctionEntry, OutputIndex);
			HLSLFunction->OutputExpressions.Add(ExpressionOutput->A.TryAcquireHLSLExpression(*this, Scope, 0));
		}
		FunctionEntry->bGeneratedResult = true;
		bResult = true;
	}
	else if (bGeneratedResult)
	{
		return Error(TEXT("Multiple connections to execution output"));
	}
	else
	{
		check(!CachedTree.ResultScope);
		check(!CachedTree.ResultExpression);

		const FExpression* AttributesExpression = nullptr;
		if (TargetMaterial)
		{
			FOwnerScope TreeOwnerScope(GetTree(), TargetMaterial);

			const FStructField* PrevWPOField = CachedTree.GetMaterialAttributesType()->FindFieldByName(TEXT("PrevWorldPositionOffset"));
			const FStructField* ShadingModelField = CachedTree.GetMaterialAttributesType()->FindFieldByName(*FMaterialAttributeDefinitionMap::GetAttributeName(MP_ShadingModel));

			if (TargetMaterial->bUseMaterialAttributes)
			{
				FMaterialInputDescription InputDescription;
				if (TargetMaterial->GetExpressionInputDescription(MP_MaterialAttributes, InputDescription))
				{
					check(InputDescription.Type == UE::Shader::EValueType::Struct);
					AttributesExpression = InputDescription.Input->TryAcquireHLSLExpression(*this, Scope, (int32)MP_MaterialAttributes);

					if (AttributesExpression)
					{
						// Special handling for ShadingModel to fallback to first material shading model if per pixel SM is not allowed
						{
							const FExpression* FallbackExpression = GetTree().NewExpression<Material::FExpressionDefaultShadingModel>();
							FallbackExpression = GetTree().NewExpression<FExpressionSetStructField>(CachedTree.GetMaterialAttributesType(), ShadingModelField, AttributesExpression, FallbackExpression);
							const FExpression* ShadingModelExpressions[] = { AttributesExpression, FallbackExpression };
							AttributesExpression = GetTree().NewExpression<Material::FExpressionFinalShadingModelSwitch>(ShadingModelExpressions);
						}

						const FExpression* PrevAttributesExpression = GetTree().NewExpression<FExpressionDefaultValue>(AttributesExpression, CachedTree.GetMaterialAttributesDefaultValue());

						const FString& WPOName = FMaterialAttributeDefinitionMap::GetAttributeName(MP_WorldPositionOffset);
						const FStructField* WPOField = CachedTree.GetMaterialAttributesType()->FindFieldByName(*WPOName);

						FRequestedType PrevRequestedType(CachedTree.GetMaterialAttributesType(), false);
						PrevRequestedType.SetFieldRequested(WPOField);

						PrevAttributesExpression = GetTree().GetPreviousFrame(PrevAttributesExpression, PrevRequestedType);
						ensure(PrevAttributesExpression);
						const FExpression* PrevWPOExpression = GetTree().NewExpression<FExpressionGetStructField>(CachedTree.GetMaterialAttributesType(), WPOField, PrevAttributesExpression);
						AttributesExpression = GetTree().NewExpression<FExpressionSetStructField>(CachedTree.GetMaterialAttributesType(), PrevWPOField, AttributesExpression, PrevWPOExpression);
					}
				}
			}
			else
			{
				AttributesExpression = GetMaterialAttributesDefaultExpression();
				for (int32 PropertyIndex = 0; PropertyIndex < MP_MAX; ++PropertyIndex)
				{
					const EMaterialProperty Property = (EMaterialProperty)PropertyIndex;

					if (Property == MP_CustomOutput)
					{
						continue;
					}

					// We're only interesting in attributes that map to valid fields
					const UE::Shader::FStructField* AttributeField = CachedTree.GetMaterialAttributesType()->FindFieldByName(*FMaterialAttributeDefinitionMap::GetAttributeName(Property));
					if (AttributeField)
					{
						const FExpression* InputExpression = CompileMaterialInput(*this, Scope, Property, TargetMaterial);
						if (InputExpression)
						{
							if (Property == MP_ShadingModel)
							{
								// Special handling for ShadingModel to fallback to first material shading model if per pixel SM is not allowed
								const FExpression* FallbackExpression = GetTree().NewExpression<Material::FExpressionDefaultShadingModel>();
								const FExpression* ShadingModelExpressions[] = { InputExpression, FallbackExpression };
								InputExpression = GetTree().NewExpression<Material::FExpressionFinalShadingModelSwitch>(ShadingModelExpressions);
							}

							AttributesExpression = GetTree().NewExpression<FExpressionSetStructField>(
								CachedTree.GetMaterialAttributesType(),
								AttributeField,
								AttributesExpression,
								InputExpression,
								Property);

							if (Property == MP_WorldPositionOffset)
							{
								const FExpression* PrevWPOExpression = GetTree().GetPreviousFrame(InputExpression, EValueType::Float3);
								ensure(PrevWPOExpression);
								AttributesExpression = GetTree().NewExpression<FExpressionSetStructField>(CachedTree.GetMaterialAttributesType(), PrevWPOField, AttributesExpression, PrevWPOExpression, Property);
							}
						}
					}
				}
			}

			for (UMaterialExpressionCustomOutput* CustomOutput : CachedTree.MaterialCustomOutputs)
			{
				const int32 NumOutputs = CustomOutput->GetNumOutputs();
				const FString OutputName = CustomOutput->GetFunctionName();
				for (int32 OutputIndex = 0; OutputIndex < NumOutputs; ++OutputIndex)
				{
					TStringBuilder<256> FieldName;
					FieldName.Appendf(TEXT("%s%d"), *OutputName, OutputIndex);
					const FStructField* CustomOutputField = CachedTree.GetMaterialAttributesType()->FindFieldByName(FieldName.ToString());
					check(CustomOutputField);

					const FExpression* CustomOutputExpression = AcquireExpression(Scope, INDEX_NONE, CustomOutput, OutputIndex, FSwizzleParameters());
					AttributesExpression = GetTree().NewExpression<FExpressionSetStructField>(CachedTree.GetMaterialAttributesType(), CustomOutputField, AttributesExpression, CustomOutputExpression);
				}
			}
		}

		if (PreviewExpression)
		{
			if (!PreviewExpressionResult)
			{
				// If we didn't hit the preview expression while generating the material normally, then generate it now
				// Hardcoding output 0 as we don't have the UI to specify any other output
				const int32 OutputIndex = 0;
				PreviewExpressionResult = AcquireExpression(Scope, INDEX_NONE, PreviewExpression, OutputIndex, FSwizzleParameters());
			}

			const FExpression* ExpressionEmissive = GetTree().GetPreview(PreviewExpressionResult);
			if (ExpressionEmissive)
			{
				const FString& EmissiveColorName = FMaterialAttributeDefinitionMap::GetAttributeName(MP_EmissiveColor);
				const FStructField* EmissiveColorField = CachedTree.GetMaterialAttributesType()->FindFieldByName(*EmissiveColorName);

				// Get back into gamma corrected space, as DrawTile does not do this adjustment.
				ExpressionEmissive = GetTree().NewPowClamped(ExpressionEmissive, NewConstant(1.f / 2.2f));

				AttributesExpression = GetMaterialAttributesDefaultExpression();
				AttributesExpression = GetTree().NewExpression<FExpressionSetStructField>(CachedTree.GetMaterialAttributesType(), EmissiveColorField, AttributesExpression, ExpressionEmissive);
			}
		}

		if (AttributesExpression)
		{
			CachedTree.ResultScope = &Scope;
			CachedTree.ResultExpression = AttributesExpression;
			bResult = true;
		}

		bGeneratedResult = true;
	}
	return bResult;
}

UE::HLSLTree::FScope* FMaterialHLSLGenerator::NewScope(UE::HLSLTree::FScope& Scope, EMaterialNewScopeFlag Flags)
{
	UE::HLSLTree::FScope* NewScope = GetTree().NewScope(Scope);
	if (!EnumHasAllFlags(Flags, EMaterialNewScopeFlag::NoPreviousScope))
	{
		NewScope->AddPreviousScope(Scope);
	}

	return NewScope;
}

UE::HLSLTree::FScope* FMaterialHLSLGenerator::NewOwnedScope(UE::HLSLTree::FStatement& Owner)
{
	UE::HLSLTree::FScope* NewScope = GetTree().NewOwnedScope(Owner);
	NewScope->AddPreviousScope(Owner.GetParentScope());
	return NewScope;
}

UE::HLSLTree::FScope* FMaterialHLSLGenerator::NewJoinedScope(UE::HLSLTree::FScope& Scope)
{
	UE::HLSLTree::FScope* NewScope = GetTree().NewScope(Scope);
	JoinedScopeStack.Add(NewScope);
	return NewScope;
}

const UE::HLSLTree::FExpression* FMaterialHLSLGenerator::NewConstant(const UE::Shader::FValue& Value)
{
	return GetTree().NewConstant(Value);
}

const UE::HLSLTree::FExpression* FMaterialHLSLGenerator::NewTexCoord(int32 Index)
{
	using namespace UE::HLSLTree;
	return NewExternalInput(Material::MakeInputTexCoord(Index));
}

const UE::HLSLTree::FExpression* FMaterialHLSLGenerator::NewExternalInput(UE::HLSLTree::Material::EExternalInput Input)
{
	using namespace UE::HLSLTree;
	return GetTree().NewExpression<Material::FExpressionExternalInput>(Input);
}

const UE::HLSLTree::FExpression* FMaterialHLSLGenerator::NewSwizzle(const UE::HLSLTree::FSwizzleParameters& Params, const UE::HLSLTree::FExpression* Input)
{
	return GetTree().NewSwizzle(Params, Input);
}

const UE::HLSLTree::FExpression* FMaterialHLSLGenerator::InternalNewErrorExpression(FStringView Error)
{
	return GetTree().NewExpression<UE::HLSLTree::FExpressionError>(UE::MemStack::AllocateStringView(GetTree().GetAllocator(), Error));
}

bool FMaterialHLSLGenerator::InternalError(FStringView ErrorMessage)
{
	if (CurrentErrorMessage.Len() > 0)
	{
		CurrentErrorMessage.AppendChar(TEXT('\n'));
	}
	CurrentErrorMessage.Append(ErrorMessage);
	return false;
}

FStringView FMaterialHLSLGenerator::AcquireError()
{
	const FStringView ErrorMessage = UE::MemStack::AllocateStringView(GetTree().GetAllocator(), CurrentErrorMessage);
	CurrentErrorMessage.Reset();
	return ErrorMessage;
}

int32 FMaterialHLSLGenerator::FindInputIndex(const FExpressionInput* Input) const
{
	UMaterialExpression* OwnerMaterialExpression = GetCurrentExpression();
	int32 Index = INDEX_NONE;
	if (OwnerMaterialExpression)
	{
		TArrayView<FExpressionInput*> Inputs = OwnerMaterialExpression->GetInputsView();
		Index = Inputs.Find(const_cast<FExpressionInput*>(Input));
	}
	return Index;
}

const UE::HLSLTree::FExpression* FMaterialHLSLGenerator::NewDefaultInputConstant(int32 InputIndex, const UE::Shader::FValue& Value)
{
	using namespace UE::HLSLTree;
	
	const FExpression* Expression = GetTree().NewConstant(Value);

	UObject* InputOwner = GetTree().GetCurrentOwner();
	if (InputOwner && InputIndex != INDEX_NONE)
	{
		const FMaterialConnectionKey Key{ InputOwner, nullptr, InputIndex, INDEX_NONE };
		CachedTree.ConnectionMap.Add(Key, Expression);
	}
	return Expression;
}

const UE::HLSLTree::FExpression* FMaterialHLSLGenerator::NewDefaultInputExternal(int32 InputIndex, UE::HLSLTree::Material::EExternalInput Input)
{
	using namespace UE::HLSLTree;
	
	const FExpression* Expression = NewExternalInput(Input);

	UObject* InputOwner = GetTree().GetCurrentOwner();
	if (InputOwner && InputIndex != INDEX_NONE)
	{
		const FMaterialConnectionKey Key{ InputOwner, nullptr, InputIndex, INDEX_NONE };
		CachedTree.ConnectionMap.Add(Key, Expression);
	}
	return Expression;
}

const UE::HLSLTree::FExpression* FMaterialHLSLGenerator::NewDefaultInputExpression(int32 InputIndex, const UE::HLSLTree::FExpression* Expression)
{
	using namespace UE::HLSLTree;

	UObject* InputOwner = GetTree().GetCurrentOwner();
	if (InputOwner && InputIndex != INDEX_NONE)
	{
		const FMaterialConnectionKey Key{ InputOwner, nullptr, InputIndex, INDEX_NONE };
		CachedTree.ConnectionMap.Add(Key, Expression);
	}
	return Expression;
}

const UE::HLSLTree::FExpression* FMaterialHLSLGenerator::AcquireExpression(UE::HLSLTree::FScope& Scope,
	int32 InputIndex,
	UMaterialExpression* MaterialExpression,
	int32 OutputIndex,
	const UE::HLSLTree::FSwizzleParameters& Swizzle)
{
	using namespace UE::HLSLTree;
	UObject* InputOwner = GetTree().GetCurrentOwner();
	FOwnerScope OwnerScope(GetTree(), MaterialExpression, NeedToPushOwnerExpression());

	const FExpression* Expression;
	{
		FHasher Hasher;
		const FScope* ScopeAddress = &Scope;
		Hasher.AppendData(&ScopeAddress, sizeof(ScopeAddress));
		Hasher.AppendData(&MaterialExpression, sizeof(MaterialExpression));
		Hasher.AppendData(&OutputIndex, sizeof(OutputIndex));

		// Make sure the callstack is the same because results can vary when the containing function
		// is called at different places due to the difference in inputs
		for (int32 Index = FunctionCallStack.Num() - 1; Index >= 0; --Index)
		{
			const FFunctionCallEntry* FunctionCall = FunctionCallStack[Index];
			Hasher.AppendData(&FunctionCall->MaterialFunction, sizeof(FunctionCall->MaterialFunction)); //-V568
			Hasher.AppendData(&FunctionCall->ParameterAssociation, sizeof(FunctionCall->ParameterAssociation));
			Hasher.AppendData(&FunctionCall->ParameterIndex, sizeof(FunctionCall->ParameterIndex));

			for (int32 FunctionInputIndex = 0; FunctionInputIndex < FunctionCall->ConnectedInputs.Num(); ++FunctionInputIndex)
			{
				const FConnectedInput& ConnectedInput = FunctionCall->ConnectedInputs[FunctionInputIndex];
				if (ConnectedInput.Input)
				{
					Hasher.AppendData(&ConnectedInput.Input, sizeof(ConnectedInput.Input)); //-V568
				}
				else
				{
					Hasher.AppendData(&ConnectedInput.Expression, sizeof(ConnectedInput.Expression)); //-V568
				}
			}
		}

		const FXxHash64 KeyHash = Hasher.Finalize();
		const FExpression** Found = GeneratedExpressionMap.Find(KeyHash);
		Expression = Found ? *Found : nullptr;

		check(MaterialExpression);
		if (!Expression && MaterialExpression->GenerateHLSLExpression(*this, Scope, OutputIndex, Expression))
		{
			GeneratedExpressionMap.Add(KeyHash, Expression);
		}
	}

	if (Expression)
	{
		Expression = GetTree().NewSwizzle(Swizzle, Expression);
		if (MaterialExpression == PreviewExpression &&
			!PreviewExpressionResult)
		{
			PreviewExpressionResult = Expression;
		}
		if (InputOwner && InputIndex != INDEX_NONE)
		{
			const FMaterialConnectionKey Key{ InputOwner, MaterialExpression, InputIndex, OutputIndex };
			CachedTree.ConnectionMap.Add(Key, Expression);
		}
	}
	else
	{
		check(!Expression);
		FStringView CurrentError = AcquireError();
		if (!CurrentError.IsEmpty())
		{
			// if we have an error, generate an error expression
			// if not, continue to return nullptr so caller can default missing inputs
			Expression = GetTree().NewExpression<UE::HLSLTree::FExpressionError>(CurrentError);
		}
	}

	return Expression;
}

const UE::HLSLTree::FExpression* FMaterialHLSLGenerator::AcquireFunctionInputExpression(UE::HLSLTree::FScope& Scope, const UMaterialExpressionFunctionInput* MaterialExpression)
{
	using namespace UE::HLSLTree;
	
	// Need to pop because we are going out of the function when processing expressions connected to the inputs
	FFunctionCallEntry* FunctionEntry = FunctionCallStack.Pop(EAllowShrinking::No);
	const FExpression* InputExpression = nullptr;
	int32 InputIndex = INDEX_NONE;

	if (FunctionEntry->MaterialFunction)
	{
		for (int32 Index = 0; Index < FunctionEntry->FunctionInputs.Num(); ++Index)
		{
			if (FunctionEntry->FunctionInputs[Index] == MaterialExpression)
			{
				InputIndex = Index;
				FConnectedInput& ConnectedInput = FunctionEntry->ConnectedInputs[Index];
				// Use GetTracedInput to detect invalid reroute node and attempt to use preview as input if allowed
				if (!ConnectedInput.Expression && ConnectedInput.Input && ConnectedInput.Input->GetTracedInput().Expression)
				{
					// Not using GetTracedInput here because we want the HLSL expression for the reroute node instead of
					// the expression of the redirected node
					ConnectedInput.Expression = ConnectedInput.Input->TryAcquireHLSLExpression(*this, *ConnectedInput.Scope);
				}
				InputExpression = ConnectedInput.Expression;
				break;
			}
		}

		if (InputIndex == INDEX_NONE)
		{
			// Finding a connected input is always expected if we're in a function call
			Error(TEXT("Invalid function input"));
			return nullptr;
		}
	}

	FunctionCallStack.Push(FunctionEntry);

	if (!InputExpression && (MaterialExpression->bUsePreviewValueAsDefault || !FunctionEntry->MaterialFunction))
	{
		// Either we're previewing the material function, or the input isn't connected and we're using preview as default value
		InputExpression = MaterialExpression->Preview.TryAcquireHLSLExpression(*this, Scope, InputIndex);
		if (!InputExpression)
		{
			const FVector4f PreviewValue(MaterialExpression->PreviewValue);
			UE::Shader::FValue DefaultValue;
			switch (MaterialExpression->InputType)
			{
			case FunctionInput_Scalar: DefaultValue = PreviewValue.X; break;
			case FunctionInput_Vector2: DefaultValue = FVector2f(PreviewValue.X, PreviewValue.Y); break;
			case FunctionInput_Vector3: DefaultValue = FVector3f(PreviewValue.X, PreviewValue.Y, PreviewValue.Z); break;
			case FunctionInput_Vector4: DefaultValue = PreviewValue; break;
			case FunctionInput_MaterialAttributes: InputExpression = GetMaterialAttributesDefaultExpression(); break;
			case FunctionInput_Texture2D:
			case FunctionInput_TextureCube:
			case FunctionInput_Texture2DArray:
			case FunctionInput_VolumeTexture:
			case FunctionInput_StaticBool:
			case FunctionInput_Bool:
			case FunctionInput_TextureExternal:
				Errorf(TEXT("Missing Preview connection for function input '%s'"), *MaterialExpression->InputName.ToString());
				return nullptr;
			default:
				Error(TEXT("Unknown input type"));
				return nullptr;
			}

			if (!InputExpression)
			{
				InputExpression = NewConstant(DefaultValue);
			}
		}
	}

	return InputExpression;
}

bool FMaterialHLSLGenerator::GenerateStatements(UE::HLSLTree::FScope& Scope, UMaterialExpression* MaterialExpression)
{
	using namespace UE::HLSLTree;

	FStatementEntry& Entry = StatementMap.FindOrAdd(MaterialExpression);
	check(Entry.NumInputs >= 0);

	if (Entry.NumInputs >= MaterialExpression->NumExecutionInputs)
	{
		return Errorf(TEXT("Bad control flow, found %d inputs out of %d reported"), Entry.NumInputs, MaterialExpression->NumExecutionInputs);
	}
	if (Entry.NumInputs == MaxNumPreviousScopes)
	{
		return Errorf(TEXT("Bad control flow, too many execution inputs"));
	}

	Entry.PreviousScope[Entry.NumInputs++] = &Scope;

	bool bResult = true;
	if (Entry.NumInputs == MaterialExpression->NumExecutionInputs)
	{
		FScope* ScopeToUse = &Scope;
		if (MaterialExpression->NumExecutionInputs > 1u)
		{
			if (JoinedScopeStack.Num() == 0)
			{
				return Error(TEXT("Bad control flow"));
			}

			ScopeToUse = JoinedScopeStack.Pop(EAllowShrinking::No);
			for (int32 i = 0; i < Entry.NumInputs; ++i)
			{
				ScopeToUse->AddPreviousScope(*Entry.PreviousScope[i]);
			}
		}

		FOwnerScope OwnerScope(GetTree(), MaterialExpression, NeedToPushOwnerExpression());
		bResult = MaterialExpression->GenerateHLSLStatements(*this, *ScopeToUse);
		if (!bResult)
		{
			GetTree().NewStatement<FStatementError>(*ScopeToUse, AcquireError());
		}
	}

	return bResult;
}

const UE::HLSLTree::FExpression* FMaterialHLSLGenerator::GenerateMaterialParameter(FName InParameterName,
	const FMaterialParameterMetadata& InParameterMeta,
	EMaterialSamplerType InSamplerType,
	const FGuid& InExternalTextureGuid)
{
	using namespace UE::Shader;

	const FMaterialParameterInfo& ParameterInfo = GetParameterInfo(InParameterName);
	FMaterialParameterMetadata ParameterMeta(InParameterMeta);
	FMaterialParameterMetadata OverrideParameterMeta;
	if (GetParameterOverrideValueForCurrentFunction(InParameterMeta.Value.Type, InParameterName, OverrideParameterMeta))
	{
		ParameterMeta.Value = OverrideParameterMeta.Value;
		ParameterMeta.ExpressionGuid = OverrideParameterMeta.ExpressionGuid;
		ParameterMeta.bUsedAsAtlasPosition = OverrideParameterMeta.bUsedAsAtlasPosition;
		ParameterMeta.ScalarAtlas = OverrideParameterMeta.ScalarAtlas;
		ParameterMeta.ScalarCurve = OverrideParameterMeta.ScalarCurve;
	}

	return GetTree().NewExpression<UE::HLSLTree::Material::FExpressionParameter>(ParameterInfo, ParameterMeta, InSamplerType, InExternalTextureGuid);
}

const UE::HLSLTree::FExpression* FMaterialHLSLGenerator::GenerateFunctionCall(UE::HLSLTree::FScope& Scope,
	UMaterialFunctionInterface* MaterialFunction,
	EMaterialParameterAssociation InParameterAssociation,
	int32 InParameterIndex,
	TArrayView<FConnectedInput> ConnectedInputs,
	int32 OutputIndex)
{
	using namespace UE::HLSLTree;

	if (!MaterialFunction)
	{
		Error(TEXT("Missing material function"));
		return nullptr;
	}

	TArray<FFunctionExpressionInput> FunctionInputs;
	TArray<FFunctionExpressionOutput> FunctionOutputs;
	MaterialFunction->GetInputsAndOutputs(FunctionInputs, FunctionOutputs);

	if (FunctionInputs.Num() != ConnectedInputs.Num())
	{
		Error(TEXT("Mismatched function inputs"));
		return nullptr;
	}

	const UMaterialExpressionFunctionOutput* ExpressionOutput = FunctionOutputs.IsValidIndex(OutputIndex) ? FunctionOutputs[OutputIndex].ExpressionOutput.Get() : nullptr;
	if (!ExpressionOutput)
	{
		Error(TEXT("Invalid function output"));
		return nullptr;
	}

	EMaterialParameterAssociation ParameterAssociation = InParameterAssociation;
	int32 ParameterIndex = InParameterIndex;
	if (InParameterAssociation == GlobalParameter)
	{
		// If this is a global function, inherit the parameter association from the previous function
		const FFunctionCallEntry* PrevFunctionEntry = FunctionCallStack.Last();
		ParameterAssociation = PrevFunctionEntry->ParameterAssociation;
		ParameterIndex = PrevFunctionEntry->ParameterIndex;
	}

	FXxHash64 Hash;
	FFunctionInputArray LocalFunctionInputs;
	{
		FXxHash64Builder Hasher;
		Hasher.Update(&MaterialFunction, sizeof(UMaterialFunctionInterface*));
		Hasher.Update(&ParameterAssociation, sizeof(EMaterialParameterAssociation));
		Hasher.Update(&ParameterIndex, sizeof(int32));

		for (int32 InputIndex = 0; InputIndex < ConnectedInputs.Num(); ++InputIndex)
		{
			// FunctionInputs are the inputs from the UMaterialFunction object
			const FFunctionExpressionInput& FunctionInput = FunctionInputs[InputIndex];
			LocalFunctionInputs.Add(FunctionInput.ExpressionInput);

			// ConnectedInputs are the inputs from the UMaterialFunctionCall object
			// We want to connect the UMaterialExpressionFunctionInput from the UMaterialFunction to whatever UMaterialExpression is passed to the UMaterialFunctionCall
			const FConnectedInput& ConnectedInput = ConnectedInputs[InputIndex];
			if (ConnectedInput.Input)
			{
				Hasher.Update(&ConnectedInput.Input, sizeof(ConnectedInput.Input)); //-V568
			}
			else
			{
				Hasher.Update(&ConnectedInput.Expression, sizeof(ConnectedInput.Expression)); //-V568
			}
		}

		// Hash the callstack too in case this function call is inside a material function.
		// In that case, the addresses of FExpressionInputs won't change even if the owner
		// MF is called at different places with different inputs
		for (int32 Index = FunctionCallStack.Num() - 1; Index >= 0; --Index)
		{
			const FFunctionCallEntry* CallstackEntry = FunctionCallStack[Index];
			Hasher.Update(&CallstackEntry, sizeof(CallstackEntry));
		}
		Hash = Hasher.Finalize();
	}

	const bool bInlineFunction = !MaterialFunction->IsUsingControlFlow();
	TUniquePtr<FFunctionCallEntry>* ExistingFunctionCall = FunctionCallMap.Find(Hash);
	FFunctionCallEntry* FunctionCall = ExistingFunctionCall ? ExistingFunctionCall->Get() : nullptr;
	if (!FunctionCall)
	{
		// Generate an HLSL function object, if this is not an inline function call
		FFunction* HLSLFunction = !bInlineFunction ? GetTree().NewFunction() : nullptr;
		FunctionCall = new FFunctionCallEntry();
		FunctionCall->MaterialFunction = MaterialFunction;
		FunctionCall->ParameterAssociation = ParameterAssociation;
		FunctionCall->ParameterIndex = ParameterIndex;
		FunctionCall->HLSLFunction = HLSLFunction;
		FunctionCall->FunctionInputs = MoveTemp(LocalFunctionInputs);
		FunctionCall->ConnectedInputs = ConnectedInputs;
		FunctionCall->FunctionOutputs.Reserve(FunctionOutputs.Num());
		for (const FFunctionExpressionOutput& Output : FunctionOutputs)
		{
			FunctionCall->FunctionOutputs.Add(Output.ExpressionOutput);
		}

		FunctionCallMap.Emplace(Hash, FunctionCall);

		if (HLSLFunction)
		{
			UMaterialFunction* BaseMaterialFunction = MaterialFunction->GetBaseFunction();
			FunctionCallStack.Add(FunctionCall);
			GenerateStatements(HLSLFunction->GetRootScope(), BaseMaterialFunction->GetExpressionExecBegin());
			verify(FunctionCallStack.Pop() == FunctionCall);
			check(FunctionCall->bGeneratedResult);
		}
	}
	else if (FunctionCall->IsGeneratingOutput(OutputIndex))
	{
		FString CallstackStr;
		for (int32 Index = FunctionCallStack.Num() - 1; Index >= 0; --Index)
		{
			const FFunctionCallEntry* Entry = FunctionCallStack[Index];
			if (Entry && Entry->MaterialFunction)
			{
				CallstackStr.Appendf(TEXT("%s%s"), CallstackStr.Len() > 0 ? TEXT("<<") : TEXT(""), *Entry->MaterialFunction->GetName());
			}
		}
		if (TargetMaterial)
		{
			CallstackStr.Appendf(TEXT("%s%s"), CallstackStr.Len() > 0 ? TEXT("<<") : TEXT(""), *TargetMaterial->GetName());
		}

		return NewErrorExpressionf(
			TEXT("Circle found in material graph. MaterialFunction: %s, output %d. Callstack: %s"),
			*MaterialFunction->GetName(),
			OutputIndex,
			*CallstackStr);
	}

	const FExpression* Result = nullptr;
	FunctionCallStack.Add(FunctionCall);
	if (bInlineFunction)
	{
		FScopedGenerateFunctionOutput GenerateOutputScope(FunctionCall, OutputIndex);
		Result = ExpressionOutput->A.AcquireHLSLExpression(*this, Scope);
	}
	else
	{
		FFunction* HLSLFunction = FunctionCall->HLSLFunction;
		check(HLSLFunction);
		check(HLSLFunction->OutputExpressions.Num() == FunctionOutputs.Num());
		if (HLSLFunction->OutputExpressions[OutputIndex])
		{
			Result = GetTree().NewFunctionCall(Scope, HLSLFunction, OutputIndex);
		}
		else
		{
			Error(TEXT("Invalid function output"));
		}
	}
	verify(FunctionCallStack.Pop() == FunctionCall);
	if (Result)
	{
		Result = GetTree().NewExpression<Material::FExpressionFunctionCall>(Result, MaterialFunction);
	}
	return Result;
}

const UE::HLSLTree::FExpression* FMaterialHLSLGenerator::GenerateBranch(UE::HLSLTree::FScope& Scope,
	const UE::HLSLTree::FExpression* ConditionExpression,
	const UE::HLSLTree::FExpression* TrueExpression,
	const UE::HLSLTree::FExpression* FalseExpression)
{
	using namespace UE::HLSLTree;

	check(ConditionExpression);
	check(TrueExpression);
	check(FalseExpression);

	FXxHash64 Hash;
	{
		FXxHash64Builder Hasher;
		Hasher.Update(&ConditionExpression, sizeof(FExpression*));
		Hasher.Update(&TrueExpression, sizeof(FExpression*));
		Hasher.Update(&FalseExpression, sizeof(FExpression*));
		Hash = Hasher.Finalize();
	}

	FExpression const* const* PrevExpression = BranchMap.Find(Hash);
	if (PrevExpression)
	{
		return *PrevExpression;
	}

	TStringBuilder<64> LocalNameBuilder;
	LocalNameBuilder.Appendf(TEXT("__InternalBranch%d"), BranchMap.Num());
	const FName LocalName(LocalNameBuilder.ToString());

	FFunction* Function = GetTree().NewFunction();
	FStatementIf* IfStatement = GetTree().NewStatement<FStatementIf>(Function->GetRootScope());
	IfStatement->ConditionExpression = ConditionExpression;
	IfStatement->ThenScope = NewOwnedScope(*IfStatement);
	IfStatement->ElseScope = NewOwnedScope(*IfStatement);
	IfStatement->NextScope = NewScope(Function->GetRootScope(), EMaterialNewScopeFlag::NoPreviousScope);
	IfStatement->NextScope->AddPreviousScope(*IfStatement->ThenScope);
	IfStatement->NextScope->AddPreviousScope(*IfStatement->ElseScope);

	GetTree().AssignLocal(*IfStatement->ThenScope, LocalName, TrueExpression);
	GetTree().AssignLocal(*IfStatement->ElseScope, LocalName, FalseExpression);
	const FExpression* ResultExpression = GetTree().AcquireLocal(*IfStatement->NextScope, LocalName);

	Function->OutputExpressions.Add(ResultExpression);
	const FExpression* Result = GetTree().NewFunctionCall(Scope, Function, 0);
	BranchMap.Add(Hash, Result);
	return Result;
}

bool FMaterialHLSLGenerator::GetParameterOverrideValueForCurrentFunction(EMaterialParameterType ParameterType, FName ParameterName, FMaterialParameterMetadata& OutResult) const
{
	bool bResult = false;
	if (!ParameterName.IsNone())
	{
		// Give every function in the callstack on opportunity to override the parameter value
		// Parameters in outer functions take priority
		// For example, if a layer instance calls a function instance that includes an overriden parameter, we want to use the value from the layer instance rather than the function instance
		for (const FFunctionCallEntry* FunctionEntry : FunctionCallStack)
		{
			const UMaterialFunctionInterface* CurrentFunction = FunctionEntry->MaterialFunction;
			if (CurrentFunction)
			{
				if (CurrentFunction->GetParameterOverrideValue(ParameterType, ParameterName, OutResult))
				{
					bResult = true;
					break;
				}
			}
		}
	}
	return bResult;
}

FMaterialParameterInfo FMaterialHLSLGenerator::GetParameterInfo(const FName& ParameterName) const
{
	if (ParameterName.IsNone())
	{
		return FMaterialParameterInfo();
	}

	const FFunctionCallEntry* FunctionEntry = FunctionCallStack.Last();
	return FMaterialParameterInfo(ParameterName, FunctionEntry->ParameterAssociation, FunctionEntry->ParameterIndex);
}

void FMaterialHLSLGenerator::InternalRegisterExpressionData(const FName& Type, const UMaterialExpression* MaterialExpression, void* Data)
{
	const FExpressionDataKey Key(Type, MaterialExpression);
	check(!ExpressionDataMap.Contains(Key));
	ExpressionDataMap.Add(Key, Data);
}

void* FMaterialHLSLGenerator::InternalFindExpressionData(const FName& Type, const UMaterialExpression* MaterialExpression)
{
	const FExpressionDataKey Key(Type, MaterialExpression);
	void** Result = ExpressionDataMap.Find(Key);
	return Result ? *Result : nullptr;
}

int32 FMaterialHLSLGenerator::FindOrAddCustomExpressionOutputStructId(TArrayView<UE::Shader::FStructFieldInitializer> StructFields)
{
	FXxHash64Builder Hasher;
	for (const UE::Shader::FStructFieldInitializer& StructField : StructFields)
	{
		Hasher.Update(StructField.Name.GetData(), StructField.Name.Len() * sizeof(TCHAR));
		const UE::Shader::FType& FieldType = StructField.Type;
		if (FieldType.IsStruct())
		{
			Hasher.Update(&FieldType.StructType->Hash, sizeof(FieldType.StructType->Hash));
		}
		else
		{
			Hasher.Update(&FieldType.ValueType, sizeof(FieldType.ValueType));
		}
	}
	const FXxHash64 KeyHash = Hasher.Finalize();
	int32* Found = CustomExpressionOutputStructIdMap.Find(KeyHash);
	if (Found)
	{
		return *Found;
	}
	else
	{
		const int32 NewId = CustomExpressionOutputStructIdMap.Num();
		CustomExpressionOutputStructIdMap.Add(KeyHash, NewId);
		return NewId;
	}
}

#endif // WITH_EDITOR
