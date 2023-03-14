// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataInterface_Wrapper.h"

FName UDataInterface_Wrapper::GetReturnTypeNameImpl() const
{
	// We mimic the return type of our output
	return Output.GetInterface() ? Output.GetInterface()->GetReturnTypeName() : NAME_None;
}

const UScriptStruct* UDataInterface_Wrapper::GetReturnTypeStructImpl() const
{
	// We mimic the return type of our output
	return Output.GetInterface() ? Output.GetInterface()->GetReturnTypeStruct() : nullptr;
}

bool UDataInterface_Wrapper::GetDataImpl(const UE::DataInterface::FContext& Context) const
{
	// Parameterize output with inputs
	//const UE::DataInterface::FContext ParameterizedContext = Context.WithParameters(Inputs);
	//return IDataInterface::StaticGetDataRaw(Output, GetReturnTypeName(), ParameterizedContext, Result);
	return false;
}
