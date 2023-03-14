// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dataflow/DataflowGraph.h"
#include "ChaosLog.h"

struct FDataflowNode;
struct FDataflowConnection;


namespace Dataflow
{

	struct DATAFLOWCORE_API FNewNodeParameters {
		FGuid Guid;
		FName Type;
		FName Name;
	};

	struct DATAFLOWCORE_API FFactoryParameters {
		FFactoryParameters() {}
		FFactoryParameters(FName InTypeName, FName InDisplayName, FName InCategory, FString InTags, FString InToolTip)
		: TypeName(InTypeName), DisplayName(InDisplayName), Category(InCategory), Tags(InTags), ToolTip(InToolTip) {}

		FName TypeName = FName("");
		FName DisplayName = FName("");
		FName Category = FName("");
		FString Tags = FString("");
		FString ToolTip = FString("");

		bool IsValid() const {
			return !TypeName.ToString().IsEmpty() && !DisplayName.ToString().IsEmpty();
		}
	};

	//
	//
	//
	class DATAFLOWCORE_API FNodeFactory
	{
		typedef TFunction<TUniquePtr<FDataflowNode> (const FNewNodeParameters&)> FNewNodeFunction;

		// All Maps indexed by TypeName
		TMap<FName, FNewNodeFunction > ClassMap;		// [TypeName] -> NewNodeFunction
		TMap<FName, FFactoryParameters > ParametersMap;	// [TypeName] -> Parameters
		TMap<FName, FName > DisplayMap;					// [DisplayName] -> TypeName

		static FNodeFactory* Instance;
		FNodeFactory() {}

	public:
		~FNodeFactory() { delete Instance; }

		static FNodeFactory* GetInstance()
		{
			if (!Instance)
			{
				Instance = new FNodeFactory();
			}
			return Instance;
		}

		void RegisterNode(const FFactoryParameters& Parameters, FNewNodeFunction NewFunction)
		{
			bool bRegisterNode = true;
			if (ClassMap.Contains(Parameters.TypeName) || DisplayMap.Contains(Parameters.DisplayName))
			{
				if (ParametersMap[Parameters.TypeName].DisplayName.IsEqual(Parameters.DisplayName) )
				{
					UE_LOG(LogChaos, Warning, 
						TEXT("Warning : Dataflow node registration mismatch with type(%s).The \
						nodes have inconsistent display names(%s) vs(%s).There are two nodes \
						with the same type being registered."), *Parameters.TypeName.ToString(),
						*ParametersMap[Parameters.TypeName].DisplayName.ToString(), 
						*Parameters.DisplayName.ToString(), *Parameters.TypeName.ToString());
				}
				if (ParametersMap[Parameters.TypeName].Category.IsEqual(Parameters.Category))
				{
					UE_LOG(LogChaos, Warning, 
						TEXT("Warning : Dataflow node registration mismatch with type (%s). The nodes \
						have inconsistent categories names (%s) vs (%s). There are two different nodes \
						with the same type being registered. "), *Parameters.TypeName.ToString(),
						*ParametersMap[Parameters.TypeName].DisplayName.ToString(), 
						*Parameters.DisplayName.ToString(),*Parameters.TypeName.ToString());
				}
				if (!ClassMap.Contains(Parameters.TypeName))
				{
					UE_LOG(LogChaos, Warning,
						TEXT("Warning: Attempted to register node type(%s) with display name (%s) \
						that conflicts with an existing nodes display name (%s)."), 
						*Parameters.TypeName.ToString(),*Parameters.DisplayName.ToString(), 
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

		FName TypeNameFromDisplayName(const FName& DisplayName)
		{
			if (DisplayMap.Contains(DisplayName))
			{
				return DisplayMap[DisplayName];
			}
			return "";
		}

		FFactoryParameters GetParameters(FName InTypeName) 
		{
			if (ParametersMap.Contains(InTypeName))
			{
				return ParametersMap[InTypeName];
			}
			return FFactoryParameters();
		}

		TSharedPtr<FDataflowNode> NewNodeFromRegisteredType(FGraph& Graph, const FNewNodeParameters& Param);

		template<class T> TSharedPtr<T> NewNode(FGraph& Graph, const FNewNodeParameters& Param)
		{
			return Graph.AddNode(new T(Param.Name, Param.Guid));
		}

		TArray<FFactoryParameters> RegisteredParameters() const
		{
			TArray<FFactoryParameters> RetVal;
			for (auto Elem : ParametersMap) RetVal.Add(Elem.Value);
			return RetVal;
		}

	};

}

