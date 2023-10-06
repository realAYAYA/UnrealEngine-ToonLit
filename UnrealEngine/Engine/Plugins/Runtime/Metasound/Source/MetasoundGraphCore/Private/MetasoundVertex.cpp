// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundVertex.h"

#include "CoreMinimal.h"

namespace Metasound
{
	bool operator==(const FInputDataVertex& InLHS, const FInputDataVertex& InRHS)
	{
		return (InLHS.VertexName == InRHS.VertexName) && (InLHS.DataTypeName == InRHS.DataTypeName);
	}

	bool operator!=(const FInputDataVertex& InLHS, const FInputDataVertex& InRHS)
	{
		return !(InLHS == InRHS);
	}

	bool operator<(const FInputDataVertex& InLHS, const FInputDataVertex& InRHS)
	{
		if (InLHS.VertexName == InRHS.VertexName)
		{
			return InLHS.DataTypeName.FastLess(InRHS.DataTypeName);
		}
		else
		{
			return InLHS.VertexName.FastLess(InRHS.VertexName);
		}
	}

	bool operator==(const FOutputDataVertex& InLHS, const FOutputDataVertex& InRHS)
	{
		return (InLHS.VertexName == InRHS.VertexName) && (InLHS.DataTypeName == InRHS.DataTypeName);
	}

	bool operator!=(const FOutputDataVertex& InLHS, const FOutputDataVertex& InRHS)
	{
		return !(InLHS == InRHS);
	}

	bool operator<(const FOutputDataVertex& InLHS, const FOutputDataVertex& InRHS)
	{
		if (InLHS.VertexName == InRHS.VertexName)
		{
			return InLHS.DataTypeName.FastLess(InRHS.DataTypeName);
		}
		else
		{
			return InLHS.VertexName.FastLess(InRHS.VertexName);
		}
	}

	bool operator==(const FEnvironmentVertex& InLHS, const FEnvironmentVertex& InRHS)
	{
		return (InLHS.VertexName == InRHS.VertexName);
	}

	bool operator!=(const FEnvironmentVertex& InLHS, const FEnvironmentVertex& InRHS)
	{
		return !(InLHS == InRHS);
	}

	bool operator<(const FEnvironmentVertex& InLHS, const FEnvironmentVertex& InRHS)
	{
		return InLHS.VertexName.FastLess(InRHS.VertexName);
	}

	FVertexInterface::FVertexInterface(const FInputVertexInterface& InInputs, const FOutputVertexInterface& InOutputs)
	:	InputInterface(InInputs)
	,	OutputInterface(InOutputs)
	{
	}

	FVertexInterface::FVertexInterface(const FInputVertexInterface& InInputs, const FOutputVertexInterface& InOutputs, const FEnvironmentVertexInterface& InEnvironmentVariables)
	:	InputInterface(InInputs)
	,	OutputInterface(InOutputs)
	,	EnvironmentInterface(InEnvironmentVariables)
	{
	}

	const FInputVertexInterface& FVertexInterface::GetInputInterface() const
	{
		return InputInterface;
	}

	FInputVertexInterface& FVertexInterface::GetInputInterface()
	{
		return InputInterface;
	}

	const FInputDataVertex& FVertexInterface::GetInputVertex(const FVertexName& InKey) const
	{
		return InputInterface[InKey];
	}

	bool FVertexInterface::ContainsInputVertex(const FVertexName& InKey) const
	{
		return InputInterface.Contains(InKey);
	}

	const FOutputVertexInterface& FVertexInterface::GetOutputInterface() const
	{
		return OutputInterface;
	}

	FOutputVertexInterface& FVertexInterface::GetOutputInterface()
	{
		return OutputInterface;
	}

	const FOutputDataVertex& FVertexInterface::GetOutputVertex(const FVertexName& InName) const
	{
		return OutputInterface[InName];
	}

	bool FVertexInterface::ContainsOutputVertex(const FVertexName& InName) const
	{
		return OutputInterface.Contains(InName);
	}

	const FEnvironmentVertexInterface& FVertexInterface::GetEnvironmentInterface() const
	{
		return EnvironmentInterface;
	}

	FEnvironmentVertexInterface& FVertexInterface::GetEnvironmentInterface()
	{
		return EnvironmentInterface;
	}

	const FEnvironmentVertex& FVertexInterface::GetEnvironmentVertex(const FVertexName& InKey) const
	{
		return EnvironmentInterface[InKey];
	}

	bool FVertexInterface::ContainsEnvironmentVertex(const FVertexName& InKey) const
	{
		return EnvironmentInterface.Contains(InKey);
	}

	bool operator==(const FVertexInterface& InLHS, const FVertexInterface& InRHS)
	{
		const bool bIsEqual = (InLHS.InputInterface == InRHS.InputInterface) && 
			(InLHS.OutputInterface == InRHS.OutputInterface) && 
			(InLHS.EnvironmentInterface == InRHS.EnvironmentInterface);

		return bIsEqual;
	}

	bool operator!=(const FVertexInterface& InLHS, const FVertexInterface& InRHS)
	{
		return !(InLHS == InRHS);
	}
}

FString LexToString(Metasound::EVertexAccessType InAccessType)
{
	using namespace Metasound;

	switch (InAccessType)
	{
		case EVertexAccessType::Value:
			return TEXT("Value");

		case EVertexAccessType::Reference:
		default:
			return TEXT("Reference");
	}
}
