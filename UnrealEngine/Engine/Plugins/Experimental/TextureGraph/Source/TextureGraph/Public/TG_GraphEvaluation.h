// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Model/Mix/MixInterface.h"
#include "TG_Var.h"

class UTG_Graph;
class UTG_Node;

struct TEXTUREGRAPH_API FTG_VarMap
{
	struct FVarArgument
	{
		FTG_Var* Var;
		FTG_Argument Argument;
	};
	TMap<FName, FVarArgument> VarArguments;

	FTG_Var* GetVar(FName Name) const
	{
		auto Element = VarArguments.Find(Name);
		
		if (Element)
		{
			return Element->Var;
		}

		return nullptr;
	}

	const FTG_Argument* GetVarArgument(FName Name)
	{
		auto Element = VarArguments.Find(Name);
		
		if (Element)
		{
			return &Element->Argument;
		}

		return nullptr;
	}

	void Empty()
	{
		VarArguments.Empty();
	}
};

// 
struct TEXTUREGRAPH_API FTG_EvaluationContext
{
	// The cycle used to call in the concrete TextureGraph engine system
	MixUpdateCyclePtr	Cycle;

	// in and out buckets of vars passed as the arguments to the expression evaluate call
	// For each node/expression, these are populated in the traverse/evaluate call in TG_Graph
	FTG_VarMap			Inputs;
	FTG_VarMap			Outputs;
	
	// These are the Ids of the Vars connected as Param to an upper Graph
	// These are persistent in the scope of a graph evaluation
	TArray<FTG_Id>		ConnectedInputParamIds;
	TArray<FTG_Id>		ConnectedOutputParamIds;

	UTG_Graph*			Graph = nullptr;
	UTG_Node*			CurrentNode = nullptr;

	int32				GraphDepth = 0;
	
	int32				TargetId = 0;

	// For debug, let's log the evaluation call sequence while it happens
	bool				bDoLog = true;

	FORCEINLINE bool	IsTweaking() const
	{
		return Cycle ? Cycle->IsTweaking() : false;
	}
};

class UTG_Pin;
class UTG_Node;
struct TEXTUREGRAPH_API FTG_Evaluation
{
	static const FString GVectorToTextureAutoConv_Name;
	static const FString GColorToTextureAutoConv_Name;
	static const FString GFloatToTextureAutoConv_Name;

	static void EvaluateGraph(UTG_Graph* InGraph, FTG_EvaluationContext* Context);
	static void EvaluateNode(UTG_Node* InNode, FTG_EvaluationContext* Context);

	static void TransferVarToPin(UTG_Pin* InPin, FTG_EvaluationContext* Context);

	// Converter used to introduce transformation from one var to another var when the arguments are compatible but require to be converted
	struct VarConverterInfo
	{
		FTG_Var* InVar = nullptr;
		FTG_Var* OutVar = nullptr;
		FTG_EvaluationContext* Context = nullptr;
	};
	typedef std::function<void(VarConverterInfo& Info)> VarConverter;

	typedef TMap				<FName, VarConverter> ConverterMap;
	static ConverterMap			DefaultConverters;
	static FName				MakeConvertKey(FName From, FName To);

	// Conformer functors used to conform pin values
	struct VarConformerInfo
    {
    	FTG_Var* InVar = nullptr;
    	FTG_Var* OutVar = nullptr;
    	FTG_EvaluationContext* Context = nullptr;
    };
    typedef std::function<bool(VarConformerInfo& Info)> VarConformer;
	
	static bool AreArgumentsCompatible(const FTG_Argument& ArgFrom, const FTG_Argument& ArgTo, FName& ConverterKey);

};

