// Copyright Epic Games, Inc. All Rights Reserved.

#include "TG_GraphEvaluation.h"
#include "TG_Graph.h"
#include "Expressions/TG_Expression.h"
#include "TG_Texture.h"
#include "TG_Variant.h"
#include "Transform/Expressions/T_FlatColorTexture.h"

void FTG_Evaluation::TransferVarToPin(UTG_Pin* InPin, FTG_EvaluationContext* Context)
{
	FTG_Argument Arg = InPin->GetArgument();

	const auto VarId = InPin->GetVarId();
	FTG_Var* Var = Context->Graph->GetVar(VarId);
	check(Var);

	if (Arg.IsInput())
	{
		if (InPin->ConnectionNeedsConversion())
		{
			FTG_Evaluation::VarConverter* Converter = FTG_Evaluation::DefaultConverters.Find(InPin->GetInputVarConverterKey());
			// If converter is null then the var is compatible, no need to convert.
			// Else COnverter is valid and we need to run it 
			if ((*Converter))
			{
				FTG_Evaluation::VarConverterInfo info;
				info.Context = Context;
				info.InVar = Var;
				info.OutVar = InPin->EditConvertedVar(); // Generate the converted version of the connected input into the CoonvertedVar of the Pin
				(*Converter)(info);

				// Since the source var is converted into the converted var
				// we need to pass the self var as the input for the pin
				Var = info.OutVar;
			}
		}

		if (InPin->IsArgVariant() && !InPin->IsConnected())
		{
			auto VariantType = Context->CurrentNode->GetExpressionCommonVariantType();
			Var->EditAs<FTG_Variant>().ResetTypeAs(VariantType);
		}

		if (InPin->NeedsConformance())
		{
	 		FTG_Evaluation::VarConformerInfo info;
	 		info.Context = Context;
	 		info.InVar = Var;
	 		info.OutVar = InPin->EditSelfVar();
			bool result = (InPin->ConformerFunctor)(info);
	 		if(result)
			{
				// Since the source var is conformed into the SelfVar
	            // we need to pass the self var as the input for the pin
	            Var = info.OutVar;
			}
		}

		Context->Inputs.VarArguments.Add(Arg.GetName(), { Var, Arg });
	}
	else if (Arg.IsOutput())
	{
		Context->Outputs.VarArguments.Add(Arg.GetName(), { Var, Arg });
	}
}

void FloatToInt_Converter(FTG_Evaluation::VarConverterInfo& Info)
{
	auto Input = Info.InVar->GetAs<float>();
	Info.OutVar->EditAs<int>() = Input;
}

void FloatToFLinearColor_Converter(FTG_Evaluation::VarConverterInfo& Info)
{
	auto Input = Info.InVar->GetAs<float>();
	Info.OutVar->EditAs<FLinearColor>() = FLinearColor(Input, Input, Input);
}

void FloatToFVector4f_Converter(FTG_Evaluation::VarConverterInfo& Info)
{
	auto Input = Info.InVar->GetAs<float>();
	Info.OutVar->EditAs<FVector4f>() = FLinearColor(Input, Input, Input);
}

void FloatToFVector2f_Converter(FTG_Evaluation::VarConverterInfo& Info)
{
	auto Input = Info.InVar->GetAs<float>();
	Info.OutVar->EditAs<FVector2f>() = FVector2f(Input, Input);
}

void FLinearColorToFVector4f_Converter(FTG_Evaluation::VarConverterInfo& Info)
{
	auto Input = Info.InVar->GetAs<FLinearColor>();
	Info.OutVar->EditAs<FVector4f>() = Input;
}
void FVector4fToFLinearColor_Converter(FTG_Evaluation::VarConverterInfo& Info)
{
	auto Input = Info.InVar->GetAs<FVector4f>();
	Info.OutVar->EditAs<FLinearColor>() = Input;
}

void FLinearColorToFVector2f_Converter(FTG_Evaluation::VarConverterInfo& Info)
{
	auto Input = Info.InVar->GetAs<FLinearColor>();
	Info.OutVar->EditAs<FVector2f>() = FVector2f(Input.R, Input.G);
}
void FVector4fToFVector2f_Converter(FTG_Evaluation::VarConverterInfo& Info)
{
	auto Input = Info.InVar->GetAs<FVector4f>();
	Info.OutVar->EditAs<FVector2f>() = FVector2f(Input);
}


const FString FTG_Evaluation::GVectorToTextureAutoConv_Name = TEXT("_Auto_Conv_Vector_To_Tex_");
const FString FTG_Evaluation::GColorToTextureAutoConv_Name = TEXT("_Auto_Conv_LinearColor_To_Tex_");
const FString FTG_Evaluation::GFloatToTextureAutoConv_Name = TEXT("_Auto_Conv_Float_To_Tex_");

// Produce a BufferDescriptor ideal to store a constant value of the type specified by Variant Type
// The texture generated with the Descriptor will contains enough precision for the consstant to save
BufferDescriptor GetFlatColorDesc(ETG_VariantType InVariantType)
{
	switch (InVariantType)
	{
	case ETG_VariantType::Scalar:
		return T_FlatColorTexture::GetFlatColorDesc(FTG_Evaluation::GFloatToTextureAutoConv_Name, BufferFormat::Half);

	case ETG_VariantType::Color:
		return T_FlatColorTexture::GetFlatColorDesc(FTG_Evaluation::GColorToTextureAutoConv_Name, BufferFormat::Byte);

	case ETG_VariantType::Vector:
		return T_FlatColorTexture::GetFlatColorDesc(FTG_Evaluation::GVectorToTextureAutoConv_Name, BufferFormat::Half);

	default:
		return  BufferDescriptor();
	}
}


void FloatToFTG_Texture_Converter(FTG_Evaluation::VarConverterInfo& Info)
{
	auto Input = Info.InVar->GetAs<float>();
	auto& Output = Info.OutVar->EditAs<FTG_Texture>();
	BufferDescriptor Desc = GetFlatColorDesc(ETG_VariantType::Scalar);
	Output = T_FlatColorTexture::Create(Info.Context->Cycle, Desc, FLinearColor(Input, Input, Input), Info.Context->TargetId);
}

void FLinearColorToFTG_Texture_Converter(FTG_Evaluation::VarConverterInfo& Info)
{
	auto Input = Info.InVar->GetAs<FLinearColor>();
	auto& Output = Info.OutVar->EditAs<FTG_Texture>();
	BufferDescriptor Desc = GetFlatColorDesc(ETG_VariantType::Color);
	Output = T_FlatColorTexture::Create(Info.Context->Cycle, Desc, Input, Info.Context->TargetId);
}

void FVector4fToFTG_Texture_Converter(FTG_Evaluation::VarConverterInfo& Info)
{
	auto Input = Info.InVar->GetAs<FVector4f>();
	auto& Output = Info.OutVar->EditAs<FTG_Texture>();
	BufferDescriptor Desc = GetFlatColorDesc(ETG_VariantType::Vector);
	Output = T_FlatColorTexture::Create(Info.Context->Cycle, Desc, FLinearColor(Input.X, Input.Y, Input.Z, Input.W), Info.Context->TargetId);
}


void FloatToFTG_Variant_Converter(FTG_Evaluation::VarConverterInfo& Info)
{
	auto Input = Info.InVar->GetAs<float>();
	auto& Output = Info.OutVar->EditAs<FTG_Variant>();

	FTG_Variant::EType VariantType = Info.Context->CurrentNode->GetExpressionCommonVariantType();

	switch (VariantType)
	{
	case FTG_Variant::EType::Scalar:
		Output.Data.Set<float>(Input);
		break;
	case FTG_Variant::EType::Color:
		Output.Data.Set<FLinearColor>(FLinearColor((float)Input, (float)Input, (float)Input));
		break;
	case FTG_Variant::EType::Vector:
		Output.Data.Set<FVector4f>(FVector4f((float)Input));
		break;
	case FTG_Variant::EType::Texture:
		BufferDescriptor Desc = GetFlatColorDesc(ETG_VariantType::Scalar);
		auto texture = FTG_Texture(
			T_FlatColorTexture::Create(Info.Context->Cycle, Desc,
				FLinearColor((float)Input, (float)Input, (float)Input),
				Info.Context->TargetId));
		Output.Data.Set<FTG_Texture>(texture);
		break;
	}
}

void FLinearColorToFTG_Variant_Converter(FTG_Evaluation::VarConverterInfo& Info)
{
	auto Input = Info.InVar->GetAs<FLinearColor>();
	auto& Output = Info.OutVar->EditAs<FTG_Variant>();

	FTG_Variant::EType VariantType = Info.Context->CurrentNode->GetExpressionCommonVariantType();

	switch (VariantType)
	{
	case FTG_Variant::EType::Scalar:
	case FTG_Variant::EType::Color:
		Output.Data.Set<FLinearColor>(Input);
		break;
	case FTG_Variant::EType::Vector:
		Output.Data.Set<FVector4f>(FVector4f(Input.R, Input.G, Input.B, Input.A));
		break;
	case FTG_Variant::EType::Texture:
		BufferDescriptor Desc = GetFlatColorDesc(ETG_VariantType::Color);
		auto texture = FTG_Texture(
			T_FlatColorTexture::Create(Info.Context->Cycle, Desc,
				Input,
				Info.Context->TargetId));
		Output.Data.Set<FTG_Texture>(texture);
		break;
	}
}

void FVector4fToFTG_Variant_Converter(FTG_Evaluation::VarConverterInfo& Info)
{
	auto Input = Info.InVar->GetAs<FVector4f>();
	auto& Output = Info.OutVar->EditAs<FTG_Variant>();

	FTG_Variant::EType VariantType = Info.Context->CurrentNode->GetExpressionCommonVariantType();

	switch (VariantType)
	{
	case FTG_Variant::EType::Scalar:
	case FTG_Variant::EType::Color:
	case FTG_Variant::EType::Vector:
		Output.Data.Set<FVector4f>(Input);
		break;
	case FTG_Variant::EType::Texture:
		BufferDescriptor Desc = GetFlatColorDesc(ETG_VariantType::Vector);
		auto texture = FTG_Texture(
			T_FlatColorTexture::Create(Info.Context->Cycle, Desc,
				FLinearColor(Input.X, Input.Y, Input.Z, Input.W),
				Info.Context->TargetId));
		Output.Data.Set<FTG_Texture>(texture);
		break;
	}

	Output.Data.Set<FVector4f>(Input);
}

void FTG_TextureToFTG_Variant_Converter(FTG_Evaluation::VarConverterInfo& Info)
{
	auto Input = Info.InVar->GetAs<FTG_Texture>();
	auto& Output = Info.OutVar->EditAs<FTG_Variant>();
	Output.Data.Set<FTG_Texture>(Input);
}

void FTG_VariantToFloat_Converter(FTG_Evaluation::VarConverterInfo& Info)
{
	auto Input = Info.InVar->GetAs<FTG_Variant>();
	auto& Output = Info.OutVar->EditAs<float>();

	FTG_Variant::EType SourceType = Input.GetType();

	switch (SourceType)
	{
	case FTG_Variant::EType::Texture:
		return;
		break;
	case FTG_Variant::EType::Scalar:
		Output = Input.Data.Get<float>();
		break;
	case FTG_Variant::EType::Color:
		Output = Input.Data.Get<FLinearColor>().R;
		break;
	case FTG_Variant::EType::Vector:
		Output = Input.Data.Get<FVector4f>().X;
		break;
	}
}

void FTG_VariantToFLinearColor_Converter(FTG_Evaluation::VarConverterInfo& Info)
{
	auto Input = Info.InVar->GetAs<FTG_Variant>();
	auto& Output = Info.OutVar->EditAs<FLinearColor>();

	FTG_Variant::EType SourceType = Input.GetType();

	switch (SourceType)
	{
	case FTG_Variant::EType::Texture:
		return;
		break;
	case FTG_Variant::EType::Scalar:
		Output = FLinearColor((float)Input.Data.Get<float>(), (float)Input.Data.Get<float>(), (float)Input.Data.Get<float>());
		break;
	case FTG_Variant::EType::Color:
		Output = Input.Data.Get<FLinearColor>();
		break;
	case FTG_Variant::EType::Vector:
		Output = FLinearColor(Input.Data.Get<FVector4f>().X, Input.Data.Get<FVector4f>().Y, Input.Data.Get<FVector4f>().Z, Input.Data.Get<FVector4f>().W);
		break;
	}
}

void FTG_VariantToFVector4f_Converter(FTG_Evaluation::VarConverterInfo& Info)
{
	auto Input = Info.InVar->GetAs<FTG_Variant>();
	auto& Output = Info.OutVar->EditAs<FVector4f>();

	FTG_Variant::EType SourceType = Input.GetType();

	switch (SourceType)
	{
	case FTG_Variant::EType::Texture:
		return;
		break;
	case FTG_Variant::EType::Scalar:
		Output = FVector4f((float)Input.Data.Get<float>());
		break;
	case FTG_Variant::EType::Color:
		Output = FVector4f(Input.Data.Get<FLinearColor>().R, Input.Data.Get<FLinearColor>().G, Input.Data.Get<FLinearColor>().B, Input.Data.Get<FLinearColor>().A);
		break;
	case FTG_Variant::EType::Vector:
		Output = Input.Data.Get<FVector4f>();
		break;
	}
}


void FTG_VariantToFVector2f_Converter(FTG_Evaluation::VarConverterInfo& Info)
{
	auto Input = Info.InVar->GetAs<FTG_Variant>();
	auto& Output = Info.OutVar->EditAs<FVector2f>();

	FTG_Variant::EType SourceType = Input.GetType();

	switch (SourceType)
	{
	case FTG_Variant::EType::Texture:
		return;
		break;
	case FTG_Variant::EType::Scalar:
		Output = FVector2f((float)Input.Data.Get<float>());
		break;
	case FTG_Variant::EType::Color:
		Output = FVector2f(Input.Data.Get<FLinearColor>().R, Input.Data.Get<FLinearColor>().G);
		break;
	case FTG_Variant::EType::Vector:
		Output = FVector2f(Input.Data.Get<FVector4f>());
		break;
	}
}


void FTG_VariantToFTG_Texture_Converter(FTG_Evaluation::VarConverterInfo& Info)
{
	auto Input = Info.InVar->GetAs<FTG_Variant>();
	auto& Output = Info.OutVar->EditAs<FTG_Texture>();

	FTG_Variant::EType SourceType = Input.GetType();

	FLinearColor Color;

	switch (SourceType)
	{
	case FTG_Variant::EType::Texture:
		Output = Input.Data.Get<FTG_Texture>();
		return;
		break;
	case FTG_Variant::EType::Scalar:
		Color = FLinearColor((float)Input.Data.Get<float>(), (float)Input.Data.Get<float>(), (float)Input.Data.Get<float>());
		break;
	case FTG_Variant::EType::Color:
		Color = Input.Data.Get<FLinearColor>();
		break;
	case FTG_Variant::EType::Vector:
		Color = FLinearColor(Input.Data.Get<FVector4f>().X, Input.Data.Get<FVector4f>().Y, Input.Data.Get<FVector4f>().Z, Input.Data.Get<FVector4f>().W);
		break;
	}
	BufferDescriptor Desc = GetFlatColorDesc(SourceType);
	Output = T_FlatColorTexture::Create(Info.Context->Cycle, Desc, Color, Info.Context->TargetId);

}



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define VAR_CONVERTER(nameFrom, nameTo, function)	{ MakeConvertKey(TEXT(#nameFrom), TEXT(#nameTo)), std::bind(&function, std::placeholders::_1) }
#define VAR_CONVERTER_DEF(nameFrom, nameTo, function)		VAR_CONVERTER(#nameFrom, #nameTo, #function)
#define VAR_CONVERTER_NULL(nameFrom, nameTo)	{ MakeConvertKey(TEXT(#nameFrom), TEXT(#nameTo)), nullptr }



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
FTG_Evaluation::ConverterMap FTG_Evaluation::DefaultConverters
(
	{
		VAR_CONVERTER(float, int, FloatToInt_Converter),
		VAR_CONVERTER(float, int32, FloatToInt_Converter),
		VAR_CONVERTER(float, FLinearColor, FloatToFLinearColor_Converter),
		VAR_CONVERTER(float, FVector4f, FloatToFVector4f_Converter),
		VAR_CONVERTER(float, FTG_Texture, FloatToFTG_Texture_Converter),
		VAR_CONVERTER(float, FVector2f, FloatToFVector2f_Converter),

		VAR_CONVERTER(FLinearColor, FVector4f, FLinearColorToFVector4f_Converter),
		VAR_CONVERTER(FLinearColor, FTG_Texture, FLinearColorToFTG_Texture_Converter),
		VAR_CONVERTER(FLinearColor, FVector2f, FLinearColorToFVector2f_Converter),

		VAR_CONVERTER(FVector4f, FLinearColor, FVector4fToFLinearColor_Converter),
		VAR_CONVERTER(FVector4f, FTG_Texture, FVector4fToFTG_Texture_Converter),
		VAR_CONVERTER(FVector4f, FVector2f, FVector4fToFVector2f_Converter),

		VAR_CONVERTER(float, FTG_Variant, FloatToFTG_Variant_Converter),
		VAR_CONVERTER(FLinearColor, FTG_Variant, FLinearColorToFTG_Variant_Converter),
		VAR_CONVERTER(FVector4f, FTG_Variant, FVector4fToFTG_Variant_Converter),
		VAR_CONVERTER(FTG_Texture, FTG_Variant, FTG_TextureToFTG_Variant_Converter),

		VAR_CONVERTER(FTG_Variant.Scalar, float, FTG_VariantToFloat_Converter),

		VAR_CONVERTER(FTG_Variant.Scalar, FLinearColor, FTG_VariantToFLinearColor_Converter),
		VAR_CONVERTER(FTG_Variant.Color, FLinearColor, FTG_VariantToFLinearColor_Converter),
		VAR_CONVERTER(FTG_Variant.Vector, FLinearColor, FTG_VariantToFLinearColor_Converter),

		VAR_CONVERTER(FTG_Variant.Scalar, FVector4f, FTG_VariantToFVector4f_Converter),
		VAR_CONVERTER(FTG_Variant.Color, FVector4f, FTG_VariantToFVector4f_Converter),
		VAR_CONVERTER(FTG_Variant.Vector, FVector4f, FTG_VariantToFVector4f_Converter),

		VAR_CONVERTER(FTG_Variant.Scalar, FTG_Texture, FTG_VariantToFTG_Texture_Converter),
		VAR_CONVERTER(FTG_Variant.Color, FTG_Texture, FTG_VariantToFTG_Texture_Converter),
		VAR_CONVERTER(FTG_Variant.Vector, FTG_Texture, FTG_VariantToFTG_Texture_Converter),
		VAR_CONVERTER(FTG_Variant.Texture, FTG_Texture, FTG_VariantToFTG_Texture_Converter),

		VAR_CONVERTER(FTG_Variant.Scalar, FVector2f, FTG_VariantToFVector2f_Converter),
		VAR_CONVERTER(FTG_Variant.Color, FVector2f, FTG_VariantToFVector2f_Converter),
		VAR_CONVERTER(FTG_Variant.Vector, FVector2f, FTG_VariantToFVector2f_Converter),


		VAR_CONVERTER_NULL(FTG_Variant.Scalar, FTG_Variant),
		VAR_CONVERTER_NULL(FTG_Variant.Color, FTG_Variant),
		VAR_CONVERTER_NULL(FTG_Variant.Vector, FTG_Variant),
		VAR_CONVERTER_NULL(FTG_Variant.Texture, FTG_Variant),
	}
);

FName FTG_Evaluation::MakeConvertKey(FName From, FName To)
{
	return FName(From.ToString() + TEXT("To") + To.ToString());
}

bool FTG_Evaluation::AreArgumentsCompatible(const FTG_Argument& ArgFrom, const FTG_Argument& ArgTo, FName& ConverterKey)
{
	ConverterKey = FName();
	auto AT = ArgFrom.GetCPPTypeName();
	auto BT = ArgTo.GetCPPTypeName();
	if (AT == BT)
		return true;

	FName FromToName = MakeConvertKey(AT, BT);
	auto Found = DefaultConverters.Find(FromToName);
	if (Found)
	{
		ConverterKey = FromToName;
		return true;
	}

	return false;
}


void FTG_Evaluation::EvaluateGraph(UTG_Graph* InGraph, FTG_EvaluationContext* InContext)
{
	// Entering a new Graph scope, we build a new EvaluationContext
	FTG_EvaluationContext EvalContext;
	EvalContext.Cycle = InContext->Cycle;
	EvalContext.Graph = InGraph;
	EvalContext.GraphDepth = InContext->GraphDepth;
	
	// Gather the external vars which are connected to Params of this graph
	// As inputs
	for (auto V : InContext->Inputs.VarArguments)
	{
		auto ParamId = InGraph->FindParamPinId(V.Key);
		if (ParamId.IsValid())
		{
			auto TheVar = InGraph->GetVar(ParamId);
			if (TheVar)
			{
				(*V.Value.Var).CopyTo(TheVar);
				//TheVar->ShareData(*V.Value.Var);
				EvalContext.ConnectedInputParamIds.Add(ParamId);
			}
		}
	}
	// As outputs
	for (auto V : InContext->Outputs.VarArguments)
	{
		auto ParamId = InGraph->FindParamPinId(V.Key);
		if (ParamId.IsValid())
		{
			EvalContext.ConnectedOutputParamIds.Add(ParamId);
		}
	}

	// The graph evaluation context is initialized and becomes the expression evaluation context
	auto ExpressionEvalContext = &EvalContext;
	InGraph->Traverse([&, ExpressionEvalContext](UTG_Node* n, int32_t i, int32_t l)
		{
			FTG_Evaluation::EvaluateNode(n, ExpressionEvalContext);
		});

	// After evaluation, transfer the output param data to the upper graph's vars
	for (auto V : InContext->Outputs.VarArguments)
	{
		auto ParamId = InGraph->FindParamPinId(V.Key);
		if (ParamId.IsValid())
		{
			auto TheVar = InGraph->GetVar(ParamId);
			if (TheVar)
			{
				V.Value.Var->ShareData(*TheVar);
			}
		}
	}
}

void FTG_Evaluation::EvaluateNode(UTG_Node* InNode, FTG_EvaluationContext* InContext)
{
	const auto Expression = InNode->GetExpression();
	if (Expression)
	{
		InContext->CurrentNode = InNode;

		// Grab the vars from the pins and load them in the context's input and output arrays
		InContext->Inputs.Empty();
		InContext->Outputs.Empty();
		for (auto Pin : InNode->Pins)
		{
			FTG_Evaluation::TransferVarToPin(Pin, InContext);
		}

		// Trigger the evaluation of the expression
		Expression->SetupAndEvaluate(InContext);


		// After the evaluation, notify the postEvaluate for thumbnails
		InNode->GetGraph()->NotifyNodePostEvaluate(InNode, InContext);

		InContext->CurrentNode = nullptr;
	}
}