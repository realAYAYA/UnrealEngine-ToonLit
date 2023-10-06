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
	class TEngineContext : public Base
	{
	public:
		DATAFLOW_CONTEXT_INTERNAL(Base, TEngineContext);

		TEngineContext(UObject* InOwner, UDataflow* InGraph, FTimestamp InTimestamp)
				: Base(InTimestamp)
				, Owner(InOwner)
				, Graph(InGraph)
		{}
	
		UObject* Owner = nullptr;
		UDataflow* Graph = nullptr;
	};

	template class TEngineContext<FContextSingle>;
	template class TEngineContext<FContextThreaded>;

	typedef TEngineContext<FContextSingle> FEngineContext;
	typedef TEngineContext<FContextThreaded> FEngineContextThreaded;

}
