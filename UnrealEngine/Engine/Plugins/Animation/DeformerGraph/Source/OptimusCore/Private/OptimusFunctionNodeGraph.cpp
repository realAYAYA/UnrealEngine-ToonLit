// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimusFunctionNodeGraph.h"
#include "OptimusFunctionNodeGraphHeader.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(OptimusFunctionNodeGraph)

FName UOptimusFunctionNodeGraph::AccessSpecifierPublicName = TEXT("Public");
FName UOptimusFunctionNodeGraph::AccessSpecifierPrivateName = TEXT("Private");


UOptimusFunctionNodeGraph::UOptimusFunctionNodeGraph()
{
}

FString UOptimusFunctionNodeGraph::GetNodeName() const
{
	return GetName();
}

TArray<FName> UOptimusFunctionNodeGraph::GetAccessSpecifierOptions() const
{
	return {AccessSpecifierPublicName, AccessSpecifierPrivateName};
}

FOptimusFunctionNodeGraphHeader UOptimusFunctionNodeGraph::GetHeader() const
{
	FOptimusFunctionNodeGraphHeader Header;

	Header.GraphPath = this; 
	Header.FunctionName = GetFName();
	Header.Category = Category;

	return Header;
}


#if WITH_EDITOR


bool UOptimusFunctionNodeGraph::CanEditChange(const FProperty* InProperty) const
{
	if (InProperty == StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UOptimusNodeSubGraph, InputBindings)))
	{
		return false;
	}
	else if (InProperty == StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UOptimusNodeSubGraph, OutputBindings)))
	{
		return false;
	}

	return true;
}

#endif
