// Copyright Epic Games, Inc. All Rights Reserved.

#include "expressions/Input/TG_Expression_Scalar.h"


FTG_SignaturePtr UTG_Expression_Scalar::BuildInputParameterSignature() const
{
	FTG_Signature::FInit SignatureInit = GetSignatureInitArgsFromClass();
	return MakeShared<FTG_Signature>(SignatureInit);
};

FTG_SignaturePtr UTG_Expression_Scalar::BuildInputConstantSignature() const
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

void UTG_Expression_Scalar::Evaluate(FTG_EvaluationContext* InContext)
{
	Super::Evaluate(InContext);

	// The Value is updated either as an input or as a setting and then becomes the output for this expression
	// The pin out is named "ValueOut"
	ValueOut = Scalar;
}

void UTG_Expression_Scalar::SetTitleName(FName NewName)
{
	GetParentNode()->GetInputPin(GET_MEMBER_NAME_CHECKED(UTG_Expression_Scalar, Scalar))->SetAliasName(NewName);
}

FName UTG_Expression_Scalar::GetTitleName() const
{
	return GetParentNode()->GetInputPin(GET_MEMBER_NAME_CHECKED(UTG_Expression_Scalar, Scalar))->GetAliasName();
}