// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextInterface_Wrapper.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNextInterface_Wrapper)

FName UAnimNextInterface_Wrapper::GetReturnTypeNameImpl() const
{
	// We mimic the return type of our output
	return Output.GetInterface() ? Output.GetInterface()->GetReturnTypeName() : NAME_None;
}

const UScriptStruct* UAnimNextInterface_Wrapper::GetReturnTypeStructImpl() const
{
	// We mimic the return type of our output
	return Output.GetInterface() ? Output.GetInterface()->GetReturnTypeStruct() : nullptr;
}

bool UAnimNextInterface_Wrapper::GetDataImpl(const UE::AnimNext::Interface::FContext& Context) const
{
	// Parameterize output with inputs
	//const UE::AnimNext::Interface::FContext ParameterizedContext = Context.WithParameters(Inputs);
	//return IAnimNextInterface::StaticGetDataRaw(Output, GetReturnTypeName(), ParameterizedContext, Result);
	return false;
}
