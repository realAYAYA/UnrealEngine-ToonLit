// Copyright Epic Games, Inc. All Rights Reserved.

#include "Expressions/Filter/TG_Expression_Levels.h"
#include "FxMat/MaterialManager.h"
#include "Job/JobBatch.h"
#include "Transform/Expressions/T_Color.h"

void FTG_LevelsSettings_VarPropertySerialize(FTG_Var::VarPropertySerialInfo& Info)
{
	FTG_Var::FGeneric_Struct_Serializer< FTG_LevelsSettings>(Info);
}

template <> FString TG_Var_LogValue(FTG_LevelsSettings& Value)
{
	FString LogMessage = Value.ToString();
	return LogMessage;
}

template <> void TG_Var_SetValueFromString(FTG_LevelsSettings& Value, const FString& StrVal)
{
	Value.InitFromString(StrVal);
}

bool FTG_LevelsSettings::SetHigh(float InValue)
{
	float NewValue = std::min(1.f, std::max(InValue, Low));
	if (NewValue != High)
	{
		float CurveExponent = EvalMidExponent();
		High = NewValue;
		return SetMidFromMidExponent(CurveExponent);
	}
	return false;
}

bool FTG_LevelsSettings::SetLow(float InValue)
{
	float NewValue = std::max(0.f, std::min(InValue, High));
	if (NewValue != Low)
	{
		float CurveExponent = EvalMidExponent();
		Low = NewValue;
		return SetMidFromMidExponent(CurveExponent);
	}
	return false;
}

bool FTG_LevelsSettings::SetMid(float InValue)
{
	float NewValue = std::max(Low, std::min(InValue, High));
	if (NewValue != Mid)
	{
		Mid = NewValue;
		return true;
	}
	return false;
}

float FTG_LevelsSettings::EvalRange(float Val) const
{
	return std::max(0.f, std::min((Val - Low)/(High - Low), 1.f));
}
float FTG_LevelsSettings::EvalRangeInv(float Val) const
{
	return Val * (High - Low) + Low;
}

float FTG_LevelsSettings::EvalMidExponent() const
{
	// 0.5 = EvalRange(Mid) ^ Exponent
	// thus
	float MidRanged = EvalRange(Mid);
	MidRanged = std::max(0.001f, std::min(9.999f, MidRanged));

	return log(0.5) / log(MidRanged);
}

bool FTG_LevelsSettings::SetMidFromMidExponent(float InExponent)
{
	// 0.5 = EvalRange(Mid) ^ Exponent
	// thus
	float NewValue = EvalRangeInv(pow(0.5, 1.0/InExponent));

	if (NewValue != Mid)
	{
		Mid = NewValue;
		return true;
	}
	return false;
}

void UTG_Expression_Levels::Evaluate(FTG_EvaluationContext* InContext)
{
	Super::Evaluate(InContext);

	const RenderMaterial_FXPtr RenderMaterial = TextureGraphEngine::GetMaterialManager()->CreateMaterial_FX<VSH_Simple, FSH_Levels>(TEXT("T_Levels"));

	check(RenderMaterial);

	if (!Input)
	{
		Output = FTG_Texture::GetBlack();
		return;
	}
	
	JobUPtr RenderJob = std::make_unique<Job>(InContext->Cycle->GetMix(), InContext->TargetId, std::static_pointer_cast<BlobTransform>(RenderMaterial));

	RenderJob
		->AddArg(ARG_BLOB(Input, "Input"))
		->AddArg(ARG_FLOAT(Levels.Low, "MinValue"))
		->AddArg(ARG_FLOAT(Levels.High, "MaxValue"))
		->AddArg(ARG_FLOAT(Levels.EvalMidExponent(), "Gamma"))
		;

	const FString Name = TEXT("Levels"); 
	BufferDescriptor Desc = Output.GetBufferDescriptor();

	Output = RenderJob->InitResult(Name, &Desc);
	InContext->Cycle->AddJob(InContext->TargetId, std::move(RenderJob));
}
