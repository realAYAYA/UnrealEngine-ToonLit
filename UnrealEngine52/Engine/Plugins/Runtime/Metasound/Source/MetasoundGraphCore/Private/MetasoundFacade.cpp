// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundFacade.h"

namespace Metasound
{
	FNodeFacade::FFactory::FFactory(FCreateOperatorFunction InCreateFunc)
	:	CreateFunc(InCreateFunc)
	{
	}

	FNodeFacade::FFactory::FFactory(FOriginalCreateOperatorFunction InCreateFunc)
	:	CreateFunc(WrapOriginalCreateOperatorFunction(InCreateFunc))
	{
	}


	TUniquePtr<IOperator> FNodeFacade::FFactory::CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
	{
		return CreateFunc(InParams, OutResults);
	}

	FNodeFacade::FFactory::FCreateOperatorFunction FNodeFacade::FFactory::WrapOriginalCreateOperatorFunction(FOriginalCreateOperatorFunction InCreateFunc)
	{
		return [InCreateFunc](const FBuildOperatorParams& InParams, FBuildResults& OutResults)
		{
			FDataReferenceCollection InputDataReferences = InParams.InputData.ToDataReferenceCollection();
			FCreateOperatorParams DeprecatedParams{ InParams.Node, InParams.OperatorSettings, InputDataReferences, InParams.Environment, InParams.Builder };
			return InCreateFunc(DeprecatedParams, OutResults.Errors);
		};
	}

	const FVertexInterface& FNodeFacade::GetVertexInterface() const
	{
		return VertexInterface;
	}

	bool FNodeFacade::SetVertexInterface(const FVertexInterface& InInterface)
	{
		return InInterface == VertexInterface;
	}

	bool FNodeFacade::IsVertexInterfaceSupported(const FVertexInterface& InInterface) const
	{
		return InInterface == VertexInterface;
	}

	/** Return a reference to the default operator factory. */
	FOperatorFactorySharedRef FNodeFacade::GetDefaultOperatorFactory() const
	{
		return Factory;
	}
}
