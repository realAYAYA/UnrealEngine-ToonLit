// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowNodeFactory.h"

#include "Dataflow/DataflowNode.h"
#include "Misc/MessageDialog.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogDataflowFactory, Warning, All);

namespace Dataflow
{
	FNodeFactory* FNodeFactory::Instance = nullptr;

	TSharedPtr<FDataflowNode> FNodeFactory::NewNodeFromRegisteredType(FGraph& Graph, const FNewNodeParameters& Param)
	{ 
		if (ClassMap.Contains(Param.Type))
		{
			TUniquePtr<FDataflowNode> Node = ClassMap[Param.Type](Param);
			if(Node->HasValidConnections())
			{
				ParametersMap[Param.Type].ToolTip = Node->GetToolTip();

				return Graph.AddNode(MoveTemp(Node));
			}
			
			const FText ErrorTitle = FText::FromString("Node Factory");
			const FString ErrorMessageString = FString::Printf(TEXT("Cannot create Node %s. Node Type %s is not well defined."), *Node->GetName().ToString(), *Node->GetDisplayName().ToString());
			const FText ErrorMessage = FText::FromString(ErrorMessageString);
			FMessageDialog::Debugf(ErrorMessage, ErrorTitle);
		}
		return TSharedPtr<FDataflowNode>(nullptr);
	}

	void FNodeFactory::RegisterNode(const FFactoryParameters& Parameters, FNewNodeFunction NewFunction)
	{
		bool bRegisterNode = true;
		if (ClassMap.Contains(Parameters.TypeName) || DisplayMap.Contains(Parameters.DisplayName))
		{
			if (ParametersMap[Parameters.TypeName].DisplayName.IsEqual(Parameters.DisplayName))
			{
				UE_LOG(LogDataflowFactory, Warning,
					TEXT("Warning : Dataflow node registration mismatch with type(%s).The \
						nodes have inconsistent display names(%s) vs(%s).There are two nodes \
						with the same type being registered."), *Parameters.TypeName.ToString(),
					*ParametersMap[Parameters.TypeName].DisplayName.ToString(),
					*Parameters.DisplayName.ToString(), *Parameters.TypeName.ToString());
			}
			if (ParametersMap[Parameters.TypeName].Category.IsEqual(Parameters.Category))
			{
				UE_LOG(LogDataflowFactory, Warning,
					TEXT("Warning : Dataflow node registration mismatch with type (%s). The nodes \
						have inconsistent categories names (%s) vs (%s). There are two different nodes \
						with the same type being registered. "), *Parameters.TypeName.ToString(),
					*ParametersMap[Parameters.TypeName].DisplayName.ToString(),
					*Parameters.DisplayName.ToString(), *Parameters.TypeName.ToString());
			}
			if (!ClassMap.Contains(Parameters.TypeName))
			{
				UE_LOG(LogDataflowFactory, Warning,
					TEXT("Warning: Attempted to register node type(%s) with display name (%s) \
						that conflicts with an existing nodes display name (%s)."),
					*Parameters.TypeName.ToString(), *Parameters.DisplayName.ToString(),
					*ParametersMap[Parameters.TypeName].DisplayName.ToString());
			}
		}
		else
		{
			ClassMap.Add(Parameters.TypeName, NewFunction);
			ParametersMap.Add(Parameters.TypeName, Parameters);
			DisplayMap.Add(Parameters.DisplayName, Parameters.TypeName);
		}
	}
}

