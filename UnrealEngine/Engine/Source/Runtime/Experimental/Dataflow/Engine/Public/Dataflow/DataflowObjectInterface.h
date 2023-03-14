// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "Dataflow/DataflowGraph.h"
#include "Dataflow/DataflowNodeParameters.h"
#include "GeometryCollection/ManagedArrayCollection.h"

class UDataflow;
class UObject;

namespace Dataflow
{
	template<class Base = FContextSingle>
	class DATAFLOWENGINE_API TEngineContext : public Base
	{
	public:
		TEngineContext(UObject* InOwner, UDataflow* InGraph, float InTime, FString InType)
				: Base(InTime, StaticType().Append(InType))
				, Owner(InOwner)
				, Graph(InGraph)
		{}
	
		static FString StaticType()
		{
			return "TEngineContext";
		}
	
		UObject* Owner = nullptr;
		UDataflow* Graph = nullptr;
	};

	template class TEngineContext<FContextSingle>;
	template class TEngineContext<FContextThreaded>;

	typedef TEngineContext<FContextSingle> FEngineContext;
	typedef TEngineContext<FContextThreaded> FEngineContextThreaded;

}