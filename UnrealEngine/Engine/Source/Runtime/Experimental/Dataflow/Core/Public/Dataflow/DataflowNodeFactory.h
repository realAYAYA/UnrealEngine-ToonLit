// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dataflow/DataflowGraph.h"

struct FDataflowNode;
struct FDataflowConnection;

namespace Dataflow
{

	struct FNewNodeParameters {
		FGuid Guid;
		FName Type;
		FName Name;
		UObject* OwningObject = nullptr;
	};

	struct FFactoryParameters {
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
	class FNodeFactory
	{
		typedef TFunction<TUniquePtr<FDataflowNode> (const FNewNodeParameters&)> FNewNodeFunction;

		// All Maps indexed by TypeName
		TMap<FName, FNewNodeFunction > ClassMap;		// [TypeName] -> NewNodeFunction
		TMap<FName, FFactoryParameters > ParametersMap;	// [TypeName] -> Parameters
		TMap<FName, FName > DisplayMap;					// [DisplayName] -> TypeName

		DATAFLOWCORE_API static FNodeFactory* Instance;
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

		DATAFLOWCORE_API void RegisterNode(const FFactoryParameters& Parameters, FNewNodeFunction NewFunction);

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

		DATAFLOWCORE_API TSharedPtr<FDataflowNode> NewNodeFromRegisteredType(FGraph& Graph, const FNewNodeParameters& Param);

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

