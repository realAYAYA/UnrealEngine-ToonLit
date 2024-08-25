// Copyright Epic Games, Inc. All Rights Reserved.

#include "expressions/Input/TG_Expression_Color.h"



FTG_SignaturePtr UTG_Expression_Color::BuildInputParameterSignature() const
{
	FTG_Signature::FInit SignatureInit = GetSignatureInitArgsFromClass();
	return MakeShared<FTG_Signature>(SignatureInit);
};

FTG_SignaturePtr UTG_Expression_Color::BuildInputConstantSignature() const
{
	FTG_Signature::FInit SignatureInit = GetSignatureInitArgsFromClass();
	for (auto& arg : SignatureInit.Arguments)
	{
		if (arg.IsInput() && arg.IsParam())
		{
			arg.ArgumentType = arg.ArgumentType.Unparamed();
			arg.ArgumentType.SetNotConnectable();
		}
	}
	return MakeShared<FTG_Signature>(SignatureInit);
};

void UTG_Expression_Color::Evaluate(FTG_EvaluationContext* InContext)
{
	Super::Evaluate(InContext);
	
	ValueOut = Color;
}

void UTG_Expression_Color::SetTitleName(FName NewName)
{
	GetParentNode()->GetInputPin("Color")->SetAliasName(NewName);
}

FName UTG_Expression_Color::GetTitleName() const
{
	return GetParentNode()->GetInputPin("Color")->GetAliasName();
}