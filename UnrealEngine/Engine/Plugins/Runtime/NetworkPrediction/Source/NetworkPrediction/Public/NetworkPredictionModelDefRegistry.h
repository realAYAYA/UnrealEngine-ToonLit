// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NetworkPredictionLog.h"
#include "NetworkPredictionModelDef.h"
#include "NetworkPredictionCheck.h"

// Basic ModelDef type registry. ModelDefs are registered and assigned an ID (their ModelDef::ID) based on ModelDef::SortPriority.
// This ID is used as indices into the various NP service arrays
class FNetworkPredictionModelDefRegistry
{
public:

	NETWORKPREDICTION_API static FNetworkPredictionModelDefRegistry& Get()
	{
		return Singleton;
	}

	template<typename ModelDef>
	void RegisterType()
	{
		bFinalized = false; // New type must re-finalize

		if (!npEnsure(ModelDefList.Contains(&ModelDef::ID) == false))
		{
			return;
		}

		FTypeInfo TypeInfo =
		{
			&ModelDef::ID,				 // Must include NP_MODEL_BODY()
			ModelDef::GetSortPriority(), // Must implement ::GetSortPriorty()
			ModelDef::GetName()			 // Must implement ::GetName()
		};

		ModelDefList.Emplace(TypeInfo);
	}

	void FinalizeTypes()
	{
		if (bFinalized)
		{
			return;
		}

		bFinalized = true;
		ModelDefList.Sort([](const FTypeInfo& LHS, const FTypeInfo& RHS) -> bool
		{
			if (LHS.SortPriority == RHS.SortPriority)
			{
				UE_LOG(LogNetworkPrediction, Log, TEXT("ModelDefs %s and %s have same sort priority. Using lexical sort as backup"), LHS.Name, RHS.Name);
				int32 StrCmpResult = FCString::Strcmp(LHS.Name, RHS.Name);
				npEnsureMsgf(StrCmpResult != 0, TEXT("Duplicate ModelDefs appear to have been registered."));
				return StrCmpResult > 0;
			}

			return LHS.SortPriority < RHS.SortPriority; 
		});

		int32 Count = 1;
		for (FTypeInfo& TypeInfo : ModelDefList)
		{
			*TypeInfo.IDPtr = Count++;
		}
	}

private:

	NETWORKPREDICTION_API static FNetworkPredictionModelDefRegistry Singleton;

	struct FTypeInfo
	{
		bool operator==(const FModelDefId* OtherIDPtr) const
		{
			return IDPtr == OtherIDPtr;
		}

		FModelDefId* IDPtr = nullptr;
		int32 SortPriority;
		const TCHAR* Name;
	};

	TArray<FTypeInfo> ModelDefList;
	
	bool bFinalized = false;
};

template<typename ModelDef>
struct TNetworkPredictionModelDefRegisterHelper
{
	TNetworkPredictionModelDefRegisterHelper()
	{
		FNetworkPredictionModelDefRegistry::Get().RegisterType<ModelDef>();
	}
};

// Helper to register ModelDef type.
// Sets static ID to 0 (invalid) and calls global registration function
#define NP_MODEL_REGISTER(X) \
	FModelDefId X::ID=0; \
	static TNetworkPredictionModelDefRegisterHelper<X> NetModelAr_##X = TNetworkPredictionModelDefRegisterHelper<X>();