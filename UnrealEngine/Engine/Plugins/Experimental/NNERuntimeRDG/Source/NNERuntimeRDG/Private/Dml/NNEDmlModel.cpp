// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNEDmlModel.h"

#ifdef NNE_USE_DIRECTML
#include "NNEDmlOperator.h"

#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "NNERenderGraphUtils.h"

#include "ID3D12DynamicRHI.h"
#endif

// Register CVar for enabling / disabling DirectML meta commands
static int32 GNNEDmlMetaCommands = 1;
static FAutoConsoleVariableRef CVarNNEDmlMetaCommands(
	TEXT("nne.dml.MetaCommands"),
	GNNEDmlMetaCommands,
	TEXT("Use DirectML meta commands in the NNERuntimeRDGDml (default: 1)"),
	ECVF_Scalability);

namespace UE::NNERuntimeRDG::Private::Dml
{

DECLARE_STATS_GROUP(TEXT("NNEDmlModelInstance"), STATGROUP_NNEDmlModelInstance, STATCAT_Advanced);
DECLARE_MEMORY_STAT(TEXT("MemSizeWeights GPU"), STAT_MemSizeWeights, STATGROUP_NNEDmlModelInstance);
DECLARE_MEMORY_STAT(TEXT("MemSizePersistent GPU"), STAT_MemSizePersist, STATGROUP_NNEDmlModelInstance);
DECLARE_MEMORY_STAT(TEXT("MemSizeTemp GPU"), STAT_MemSizeTemp, STATGROUP_NNEDmlModelInstance);
DECLARE_DWORD_COUNTER_STAT(TEXT("InferenceCount"), STAT_InferenceCount, STATGROUP_NNEDmlModelInstance);

DECLARE_GPU_STAT_NAMED(NNE_DmlModelInstance_Dispatch_GPU, TEXT("NNE_DmlModelInstance_Dispatch"));
DECLARE_GPU_STAT_NAMED(NNE_DmlModelInstance_DispatchD3D_GPU, TEXT("NNE_DmlModelInstance_DispatchD3D"));

// Empty tensor is marked by -1
static constexpr int32 GEmptyTensorIdx = -1;

FModelInfo* FModelInfo::Get()
{
	static FModelInfo Inst;

	return &Inst;
}

FModelInfo::FModelInfo()
	: Guid((int32)'R', (int32)'D', (int32)'G', (int32)'D')
	, Version(0x5)
{
}


FGuid FModelInfo::GetGuid()
{
	return Guid;
}


int32 FModelInfo::GetVersion()
{
	return Version;
}


int32 FModelInfo::GetGuidSize()
{
	return sizeof(Guid);
}


int32 FModelInfo::GetVersionSize()
{
	return sizeof(Version);
}


bool FModelInfo::ValidateGuidAndVersion(const uint8* InGuid, const uint8* InVersion)
{
	bool bResult;
	
	bResult = FGenericPlatformMemory::Memcmp(InGuid, &Guid, GetGuidSize()) == 0;
	bResult &= FGenericPlatformMemory::Memcmp(InVersion, &Version, GetVersionSize()) == 0;

	return bResult;
}

#ifdef NNE_USE_DIRECTML

inline FModelInstance::FDebugName::FDebugName()
{
	Str[0] = '\0';
	Length = 0;
}

inline FModelInstance::FDebugName::FDebugName(const FString& InStr)
{
	FTCHARToUTF8 Conv(*InStr);

	Length = FMath::Min(Conv.Length() + 1, Size - 1);
	FCStringAnsi::Strncpy(Str, Conv.Get(), Length);
	Str[Length] = '\0';
}

inline FModelInstance::FDebugName::FDebugName(FStringView InStr)
{
	FTCHARToUTF8 Conv(InStr.GetData());

	Length = FMath::Min(Conv.Length() + 1, Size - 1);
	FCStringAnsi::Strncpy(Str, Conv.Get(), Length);
	Str[Length] = '\0';
}

inline const char* FModelInstance::FDebugName::Get() const
{
	return Str;
}

/**
* Utility class to help out with binding DML resources when operator is initialized and dispatched
*/
class FModelInstance::FBindingTable
{
public:

	/**
	* Initialize the binding table from the model
	* Note: This is internal class and can only be called from FModelInstance, therefore FModelInstance is always valid
	*/
	bool Init(FModelInstance* InModel)
	{
		Model = InModel;
		DynamicRHI = InModel->DynamicRHI;

		return true;
	}

	/**
	* Bind requried resources for DML operator initialization
	*/
	void Bind(IDMLOperatorInitializer* InOpInit, TConstArrayView<FRHIBuffer*> InputBuffers, FRHIBuffer* InPersistBuff, FRHIBuffer* InTempBuff = nullptr)
	{
		Reset(InOpInit);

		TArray<DML_BUFFER_BINDING, TInlineAllocator<MaxNumInputs>> Inputs;

		for (FRHIBuffer* Buffer : InputBuffers)
		{
			if (Buffer)
			{
				Inputs.Emplace(MakeBind(Buffer));
			}
			else
			{
				Inputs.Add({});
			}
		}

		DML_BUFFER_ARRAY_BINDING	InputBindArray{ Inputs.Num(), Inputs.GetData() };
		DML_BINDING_DESC			InputBindArrayDesc{ DML_BINDING_TYPE_BUFFER_ARRAY, &InputBindArray };
		
		BindingTable->BindInputs(1, &InputBindArrayDesc);
		
		DML_BUFFER_BINDING			PersistBind {};
		DML_BINDING_DESC			PersistBindDesc{ DML_BINDING_TYPE_BUFFER, &PersistBind };

		if (InPersistBuff)
		{
			PersistBind = MakeBind(InPersistBuff);
		}

		BindingTable->BindOutputs(1, &PersistBindDesc);

		DML_BUFFER_BINDING			TempBind{};
		DML_BINDING_DESC			TempBindDesc{ DML_BINDING_TYPE_BUFFER, &TempBind };

		if (InTempBuff)
		{
			TempBind = MakeBind(InTempBuff);

			BindingTable->BindTemporaryResource(&TempBindDesc);
		}
	}

	/**
	* Bind requried resources for DML operator dispatch
	*/ 
	void Bind(IDMLCompiledOperator* Op, TConstArrayView<FRHIBuffer*> InputBuffers, TConstArrayView<FRHIBuffer*> OutputBuffers, FRHIBuffer* InPersistBuff = nullptr, FRHIBuffer* InTempBuff = nullptr)
	{
		Reset(Op);

		for (FRHIBuffer* Buffer : InputBuffers)
		{
			AddBind(Buffer, InputBinds, InputBindDescs);
		}

		for (FRHIBuffer* Buffer : OutputBuffers)
		{
			AddBind(Buffer, OutputBinds, OutputBindDescs);
		}

		BindingTable->BindInputs(InputBinds.Num(), InputBindDescs.GetData());
		BindingTable->BindOutputs(OutputBinds.Num(), OutputBindDescs.GetData());

		DML_BUFFER_BINDING			PersistBind;
		DML_BINDING_DESC			PersistBindDesc{ DML_BINDING_TYPE_BUFFER, &PersistBind };

		if (InPersistBuff)
		{
			PersistBind =  MakeBind(InPersistBuff);MakeBind(InPersistBuff);

			BindingTable->BindPersistentResource(&PersistBindDesc);
		}

		DML_BUFFER_BINDING			TempBind{};
		DML_BINDING_DESC			TempBindDesc{ DML_BINDING_TYPE_BUFFER, &TempBind };

		if (InTempBuff)
		{
			TempBind = MakeBind(InTempBuff);

			BindingTable->BindTemporaryResource(&TempBindDesc);
		}
	}

	IDMLBindingTable* Get()
	{
		return BindingTable;
	}

private:

	bool Reset(IDMLDispatchable* Disp)
	{
		InputBinds.Reset();
		InputBindDescs.Reset();
		OutputBinds.Reset();
		OutputBindDescs.Reset();
		
		DML_BINDING_PROPERTIES BindingProps = Disp->GetBindingProperties();
		DML_BINDING_TABLE_DESC Desc{};

		Desc.Dispatchable = Disp;
		Desc.CPUDescriptorHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(Model->DescHeap->GetCPUDescriptorHandleForHeapStart(), 0, Model->DescSize);
		Desc.GPUDescriptorHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(Model->DescHeap->GetGPUDescriptorHandleForHeapStart(), 0, Model->DescSize);
		Desc.SizeInDescriptors = Model->DescCount;

		HRESULT Res;

		if (!BindingTable)
		{
			Res = Model->DevCtx->Device->CreateBindingTable(&Desc, DML_PPV_ARGS(&BindingTable));
			if (!BindingTable)
			{
				UE_LOG(LogNNE, Error, TEXT("Failed to create DML binding table, res:%d"), Res);
				return false;
			}
		}
		else
		{
			BindingTable->Reset(&Desc);
		}

		return true;
	}

	template<class TBindingArray, class TDescArray>
	void AddBind(FRHIBuffer* Buffer, TBindingArray& Bindings, TDescArray& Descs)
	{
		if (Buffer)
		{
			DML_BUFFER_BINDING& Bind = Bindings.Add_GetRef(MakeBind(Buffer));
			Descs.Add({ DML_BINDING_TYPE_BUFFER, &Bind });
		}
		else
		{
			DML_BUFFER_BINDING& Bind = Bindings.Add_GetRef({});
			Descs.Add({ DML_BINDING_TYPE_NONE, nullptr });
		}
	}

	DML_BUFFER_BINDING MakeBind(FRHIBuffer* Buffer)
	{
		ID3D12Resource* Resource = DynamicRHI->RHIGetResource(Buffer);

		check(Resource->GetDesc().Width <= Buffer->GetSize());

		return DML_BUFFER_BINDING{ Resource, 0, Buffer->GetSize() };
	}

	TComPtr<IDMLBindingTable>										BindingTable;
	TArray<DML_BUFFER_BINDING, TInlineAllocator<MaxNumInputs>>		InputBinds;
	TArray<DML_BINDING_DESC, TInlineAllocator<MaxNumInputs>>		InputBindDescs;
	TArray<DML_BUFFER_BINDING, TInlineAllocator<MaxNumOutputs>>		OutputBinds;
	TArray<DML_BINDING_DESC, TInlineAllocator<MaxNumOutputs>>		OutputBindDescs;
	ID3D12DynamicRHI*												DynamicRHI;
	FModelInstance*													Model;
};

/**
* Helper class for building DML graph
* Note: This is internal class and can only be called from FModelInstance, therefore FModelInstance is always valid
*/
class FModelInstance::FGraphBuilder
{
private:

	enum class EEdgeType
	{
		Input,
		Output,
		Intermediate
	};

	struct FEdge
	{
		EEdgeType	Type;
		int32		TensorIdx{ -1 };
		int32		NodeSrc{ -1 };
		int32		NodeSrcOutput{ -1 };
		int32		NodeDst{ -1 };
		int32		NodeDstInput{ -1 };
		
		FEdge(EEdgeType InType)
			: Type(InType)
		{
		}

		FEdge& SetTensorIdx(int32 Value)
		{
			TensorIdx = Value;
			return *this;
		}

		FEdge& SetNodeSrc(int32 Value)
		{
			NodeSrc = Value;
			return *this;
		}

		FEdge& SetNodeSrcOutput(int32 Value)
		{
			NodeSrcOutput = Value;
			return *this;
		}

		FEdge& SetNodeDst(int32 Value)
		{
			NodeDst = Value;
			return *this;
		}

		FEdge& SetNodeDstInput(int32 Value)
		{
			NodeDstInput = Value;
			return *this;
		}
	};

public:

	IDMLCompiledOperator* Compile(const FModelInstance* InModel)
	{
		IDMLDevice*				Device = InModel->DevCtx->Device;
		TComPtr<IDMLDevice1>	Device1;

		Device1.FromQueryInterface(__uuidof(IDMLDevice1), Device);
		check(Device1);
		
		if (!Device1)
		{
			return nullptr;
		}

		if (!AddEdges(InModel))
		{
			return nullptr;
		}

		TArray<DML_INPUT_GRAPH_EDGE_DESC>			InputEdges;
		TArray<DML_OUTPUT_GRAPH_EDGE_DESC>			OutputEdges;
		TArray<DML_INTERMEDIATE_GRAPH_EDGE_DESC>	IntermediateEdges;
		TArray<FDebugName>							DbgInputNames;
		TArray<FDebugName>							DbgIntermediateNames;
		TArray<FDebugName>							DbgOutputNames;

		DbgInputNames.Reserve(InModel->AllSymbolicTensorDescs.Num());
		DbgIntermediateNames.Reserve(InModel->AllSymbolicTensorDescs.Num());

		for (const FEdge& Edge : Edges)
		{
			if (Edge.Type == EEdgeType::Input)
			{
				DML_INPUT_GRAPH_EDGE_DESC& Input = InputEdges.Add_GetRef({});

				Input.GraphInputIndex = Edge.NodeSrcOutput;
				Input.ToNodeIndex = Edge.NodeDst;
				Input.ToNodeInputIndex = Edge.NodeDstInput;
				
				const FDebugName& DbgName = DbgInputNames.Add_GetRef(InModel->AllSymbolicTensorDescs[Edge.TensorIdx].GetName());
				Input.Name = DbgName.Get();
			}
			else if (Edge.Type == EEdgeType::Output)
			{
				DML_OUTPUT_GRAPH_EDGE_DESC&	Output = OutputEdges.Add_GetRef({});

				Output.GraphOutputIndex = Edge.NodeDstInput;
				Output.FromNodeIndex = Edge.NodeSrc;
				Output.FromNodeOutputIndex = Edge.NodeSrcOutput;

				const FDebugName& DbgName = DbgOutputNames.Add_GetRef(InModel->AllSymbolicTensorDescs[Edge.TensorIdx].GetName());
				Output.Name = DbgName.Get();
			}
			else if (Edge.Type == EEdgeType::Intermediate)
			{
				DML_INTERMEDIATE_GRAPH_EDGE_DESC& Intermediate = IntermediateEdges.Add_GetRef({});

				Intermediate.FromNodeIndex = Edge.NodeSrc;
				Intermediate.FromNodeOutputIndex = Edge.NodeSrcOutput;
				Intermediate.ToNodeIndex = Edge.NodeDst;
				Intermediate.ToNodeInputIndex = Edge.NodeDstInput;

				const FDebugName& DbgName = DbgIntermediateNames.Add_GetRef(InModel->AllSymbolicTensorDescs[Edge.TensorIdx].GetName());
				Intermediate.Name = DbgName.Get();
			}
		}
			
		TArray<DML_GRAPH_NODE_DESC>		Nodes;
		TArray<DML_GRAPH_EDGE_DESC>		InputEdgeDescs;
		TArray<DML_GRAPH_EDGE_DESC>		OutputEdgeDescs;
		TArray<DML_GRAPH_EDGE_DESC>		IntermediateEdgeDescs;

		Nodes.SetNumUninitialized(Operators.Num());

		for (int32 Idx = 0; Idx < Operators.Num(); ++Idx)
		{
			DML_GRAPH_NODE_DESC& NodeDesc = Nodes[Idx];

			NodeDesc.Type = DML_GRAPH_NODE_TYPE_OPERATOR;
			NodeDesc.Desc = &Operators[Idx];
		}

		for (int32 Idx = 0; Idx < InputEdges.Num(); ++Idx)
		{
			DML_GRAPH_EDGE_DESC& Edge = InputEdgeDescs.Add_GetRef({});

			Edge.Type = DML_GRAPH_EDGE_TYPE_INPUT;
			Edge.Desc = &InputEdges[Idx];
		}

		for (int32 Idx = 0; Idx < OutputEdges.Num(); ++Idx)
		{
			DML_GRAPH_EDGE_DESC& Edge = OutputEdgeDescs.Add_GetRef({});

			Edge.Type = DML_GRAPH_EDGE_TYPE_OUTPUT;
			Edge.Desc = &OutputEdges[Idx];				
		}

		for (int32 Idx = 0; Idx < IntermediateEdges.Num(); ++Idx)
		{
			DML_GRAPH_EDGE_DESC& Edge = IntermediateEdgeDescs.Add_GetRef({});

			Edge.Type = DML_GRAPH_EDGE_TYPE_INTERMEDIATE;
			Edge.Desc = &IntermediateEdges[Idx];
		}
			
		DML_GRAPH_DESC	Graph = DML_GRAPH_DESC{};

		Graph.InputCount = NumInputs;
		Graph.OutputCount = OutputEdges.Num();
		Graph.NodeCount = Operators.Num();
		Graph.Nodes = Nodes.GetData();
		Graph.InputEdgeCount = InputEdgeDescs.Num();
		Graph.InputEdges = InputEdgeDescs.GetData();
		Graph.OutputEdgeCount = OutputEdgeDescs.Num();
		Graph.OutputEdges = OutputEdgeDescs.GetData();
		Graph.IntermediateEdgeCount = IntermediateEdgeDescs.Num();
		Graph.IntermediateEdges = IntermediateEdgeDescs.GetData();

		IDMLCompiledOperator* Op = nullptr;
		HRESULT Res;
		DML_EXECUTION_FLAGS	DmlExecFlags = DML_EXECUTION_FLAG_NONE;

		if (!GNNEDmlMetaCommands)
		{
			DmlExecFlags = DML_EXECUTION_FLAG_DISABLE_META_COMMANDS;
		}

		Res = Device1->CompileGraph(&Graph, DmlExecFlags, DML_PPV_ARGS(&Op));
		if (FAILED(Res))
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to compile DML graph"));
			Op = nullptr;
		};

		return Op;
	}

private:

	bool AddEdges(const FModelInstance* InModel)
	{
		Edges.Reset();
		TensorInConnCounts.Reset();
		TensorOutConnCounts.Reset();
		Operators.Reset();
		NumInputs = 0;
		NumOutputs = 0;

		TensorInConnCounts.SetNumZeroed(InModel->TensorIdxSpan);
		TensorOutConnCounts.SetNumZeroed(InModel->TensorIdxSpan);
		Operators.Reset(InModel->GraphOperators.Num());

		for (int32 Idx : InModel->GraphOpInputIndices)
		{
			if (Idx != GEmptyTensorIdx)
			{
				TensorInConnCounts[Idx] += 1;
			}
		}

		for (int32 Idx : InModel->GraphOpOutputIndices)
		{
			TensorOutConnCounts[Idx] += 1;
		}

		if (!AddInputEdges(InModel->InputTensorIndices, InModel))
		{
			UE_LOG(LogNNE, Error, TEXT("DMLGraphBuilder failed to add input tensors"));
			return false;
		}
		
		if (!AddInputEdges(InModel->WeightTensorIndices, InModel))
		{
			UE_LOG(LogNNE, Error, TEXT("DMLGraphBuilder failed to add weight tensors"));
			return false;
		}
		
		if (!AddOutputEdges(InModel))
		{
			UE_LOG(LogNNE, Error, TEXT("DMLGraphBuilder failed to add output tensors"));
			return false;
		}

		if (!AddIntermediateEdges(InModel))
		{
			UE_LOG(LogNNE, Error, TEXT("DMLGraphBuilder failed to add intermediate tensors"));
			return false;
		}

		if (!AddOperators(InModel))
		{
			UE_LOG(LogNNE, Error, TEXT("DMLGraphBuilder failed to add operators"));
			return false;
		}

		// Validate edges
		for (const FEdge& Edge : Edges)
		{
			if (Edge.Type == EEdgeType::Input)
			{
				if (Edge.NodeSrcOutput == -1 || Edge.NodeDst == -1 || Edge.NodeDstInput == -1)
				{
					UE_LOG(LogNNE, Error, TEXT("DMLGraphBuilder error invalid input graph edge detected for tensor:%s"), *InModel->AllSymbolicTensorDescs[Edge.TensorIdx].GetName());
					return false;
				}
			}
			else if (Edge.Type == EEdgeType::Output)
			{
				if (Edge.NodeDstInput == -1 || Edge.NodeSrc == -1 || Edge.NodeSrcOutput == -1)
				{
					UE_LOG(LogNNE, Error, TEXT("DMLGraphBuilder error invalid output graph edge detected for tensor:%s"), *InModel->AllSymbolicTensorDescs[Edge.TensorIdx].GetName());
					return false;
				}
			}
			else if (Edge.Type == EEdgeType::Intermediate)
			{
				if (Edge.NodeSrc == -1 || Edge.NodeSrcOutput == -1 || Edge.NodeDst == -1 || Edge.NodeDstInput == -1)
				{
					UE_LOG(LogNNE, Error, TEXT("DMLGraphBuilder error invalid intermediate graph edge detected for tensor:%s"), *InModel->AllSymbolicTensorDescs[Edge.TensorIdx].GetName());
					return false;
				}
			}
		}
		
		return true;
	}

	bool AddInputEdges(TConstArrayView<int32> InTensorIndices, const FModelInstance* InModel)
	{
		for (int32 Idx = 0; Idx < InTensorIndices.Num(); ++Idx)
		{
			const int32 TensorIdx = InTensorIndices[Idx];
			const int32 ConnCount = TensorInConnCounts[TensorIdx];

			if (ConnCount == 0)
			{
				if (InModel->ConstantCPUTensorIndices.Find(TensorIdx) == INDEX_NONE)
				{
					UE_LOG(LogNNE, Error, TEXT("DmlGraphBuilder tensor:%d has no connections"), *InModel->AllSymbolicTensorDescs[TensorIdx].GetName());
					return false;
				}
			}

			// Check if tensor is a constant CPU
			const int32 ConstantCPUIdx = InModel->ConstantCPUTensorIndices.Find(TensorIdx);

			if (ConstantCPUIdx == INDEX_NONE)
			{
				for (int32 ConnIdx = 0; ConnIdx < ConnCount; ++ConnIdx)
				{
					AddInputEdge(TensorIdx);
				}
			
				++NumInputs;
			}
		}

		return true;
	}

	bool AddOutputEdges(const FModelInstance* InModel)
	{
		for (int32 Idx = 0; Idx < InModel->OutputTensorIndices.Num(); ++Idx)
		{
			const int32 TensorIdx = InModel->OutputTensorIndices[Idx];
			const int32 ConnCount = TensorOutConnCounts[TensorIdx];

			if (ConnCount == 0)
			{
				UE_LOG(LogNNE, Error, TEXT("DmlGraphBuilder tensor:%d has no connections"), *InModel->AllSymbolicTensorDescs[TensorIdx].GetName());
				return false;
			}

			for (int32 ConnIdx = 0; ConnIdx < ConnCount; ++ConnIdx)
			{
				AddOutputEdge(TensorIdx);
			}
		}

		return true;
	}

	bool AddIntermediateEdges(const FModelInstance* InModel)
	{
		for (int32 Idx = 0; Idx < InModel->IntermediateTensorIndices.Num(); ++Idx)
		{
			const int32 TensorIdx = InModel->IntermediateTensorIndices[Idx];
			const int32 InputConnCount = TensorInConnCounts[TensorIdx];
			const int32 OutputConnCount = TensorOutConnCounts[TensorIdx];

			if (InputConnCount == 0 || OutputConnCount == 0)
			{
				UE_LOG(LogNNE, Error, TEXT("DmlGraphBuilder tensor:%d has no connections"), *InModel->AllSymbolicTensorDescs[TensorIdx].GetName());
				return false;
			}

			for (int32 ConnIdx = 0; ConnIdx < InputConnCount; ++ConnIdx)
			{
				AddIntermediateEdge(TensorIdx, -1, -1);
			}
		}

		return true;
	}

	bool AddOperators(const FModelInstance* InModel)
	{
		for (const FGraphOpDesc& CurrOp : InModel->GraphOperators) 
		{
			DML_OPERATOR_GRAPH_NODE_DESC&	OpDesc = Operators.Add_GetRef({});
			FOperatorDml*					Op = InModel->Operators[CurrOp.OpIndex];
			
			OpDesc.Operator = Op->GetOperator();
			OpDesc.Name = CurrOp.DbgName.Get();

			const int32		NodeIdx = Operators.Num() - 1;
			int32			InputConnIdx = 0;

			for (int32 Idx = 0; Idx < CurrOp.InputCount; ++Idx)
			{
				const int32 TensorIdx = InModel->GraphOpInputIndices[Idx + CurrOp.InputStart];

				if (TensorIdx == GEmptyTensorIdx)
				{
					continue;
				}

				// Filter out the constant CPU inputs from the graph node inputs
				if (InModel->ConstantCPUTensorIndices.Find(TensorIdx) != INDEX_NONE)
				{
					continue;
				}

				if (!ConnectEdgeDst(TensorIdx, NodeIdx, InputConnIdx))
				{
					UE_LOG(LogNNE, Error, TEXT("DmlGraphBuilder failed to connect intermediate edge dst for tensor:%s"), *InModel->AllSymbolicTensorDescs[TensorIdx].GetName());
					return false;
				}

				++InputConnIdx;
			}

			for (int32 Idx = 0; Idx < CurrOp.OutputCount; ++Idx)
			{
				const int32 TensorIdx = InModel->GraphOpOutputIndices[Idx + CurrOp.OutputStart];
				int32 ConnCount = TensorInConnCounts[TensorIdx];

				if (ConnCount == 0)
				{
					// This is for output edges, they don't have input connections
					ConnCount = TensorOutConnCounts[TensorIdx];
				}

				for (int32 ConnIdx = 0; ConnIdx < ConnCount; ++ConnIdx)
				{
					if (!ConnectEdgeSrc(TensorIdx, NodeIdx, Idx))
					{
						UE_LOG(LogNNE, Error, TEXT("DmlGraphBuilder failed to connect intermediate edge src for tensor:%s"), *InModel->AllSymbolicTensorDescs[TensorIdx].GetName());
						return false;
					}
				}
			}
		}

		return true;
	}

	void AddInputEdge(int32 TensorIdx)
	{
		Edges.Emplace(
			FEdge(EEdgeType::Input)
				.SetTensorIdx(TensorIdx)
				.SetNodeSrcOutput(NumInputs)
		);
	}

	void AddOutputEdge(int32 TensorIdx)
	{
		Edges.Emplace(
			FEdge(EEdgeType::Output)
				.SetTensorIdx(TensorIdx)
				.SetNodeDstInput(NumOutputs)
		);

		++NumOutputs;
	}

	void AddIntermediateEdge(int32 TensorIdx, int32 NodeSrc, int32 NodeSrcOutput)
	{
		Edges.Emplace(
			FEdge(EEdgeType::Intermediate)
				.SetTensorIdx(TensorIdx)
				.SetNodeSrc(NodeSrc)
				.SetNodeSrcOutput(NodeSrcOutput)
			);
	}

	bool ConnectEdgeDst(int32 TensorIdx, int32 NodeDst, int32 NodeDstInput)
	{
		FEdge* StartEdge = Edges.FindByPredicate(
				[TensorIdx](const FEdge& Curr)
				{
					return Curr.TensorIdx == TensorIdx;
				}
		);

		bool bFoundEdge = false;

		for (FEdge* Curr = StartEdge; Curr->TensorIdx == TensorIdx; ++Curr)
		{
			if (Curr->NodeDst == -1 && Curr->NodeDstInput == -1)
			{
				Curr->NodeDst = NodeDst;
				Curr->NodeDstInput = NodeDstInput;
				bFoundEdge = true;
				break;
			}
		}

		checkf(bFoundEdge, TEXT("ConnectEdgeDst() has failed"));
		return bFoundEdge;
	}

	bool ConnectEdgeSrc(int32 TensorIdx, int32 NodeSrc, int32 NodeSrcOutput)
	{
		FEdge* StartEdge =
			Edges.FindByPredicate(
				[TensorIdx](const FEdge& Curr)
				{
					return Curr.TensorIdx == TensorIdx;
				}
		);

		bool bFoundEdge = false;

		for (FEdge* Curr = StartEdge; Curr->TensorIdx == TensorIdx; ++Curr)
		{
			if (Curr->NodeSrc == -1 && Curr->NodeSrcOutput == -1)
			{
				Curr->NodeSrc = NodeSrc;
				Curr->NodeSrcOutput = NodeSrcOutput;
				bFoundEdge = true;
				break;
			}
		}

		checkf(bFoundEdge, TEXT("ConnectEdgeSrc() has failed"));
		return bFoundEdge;
	}

	TArray<FEdge>							Edges;
	TArray<int32>							TensorInConnCounts;
	TArray<int32>							TensorOutConnCounts;
	TArray<DML_OPERATOR_GRAPH_NODE_DESC>	Operators;
	int32									NumInputs;
	int32									NumOutputs;
};

static constexpr EBufferUsageFlags	WeightBuffUsage = BUF_UnorderedAccess;
static constexpr ERHIAccess			WeightBuffAccess = ERHIAccess::CopyDest;

static constexpr EBufferUsageFlags	PersistBuffFlags = BUF_Static | BUF_ShaderResource | BUF_UnorderedAccess;
static constexpr ERHIAccess			PersistBuffAccess = ERHIAccess::UAVMask;

static constexpr EBufferUsageFlags	TempBuffFlags = BUF_Static | BUF_ShaderResource | BUF_UnorderedAccess;
static constexpr ERHIAccess			TempBuffAccess = ERHIAccess::UAVMask;

FModelInstance::FModelInstance()
{
}

FModelInstance::~FModelInstance()
{
	NNE_TRACE_EVENT_SCOPED(NNE_DmlModel_<dtor>);

	DEC_MEMORY_STAT_BY(STAT_MemSizeWeights, MemSizeWeights);
	DEC_MEMORY_STAT_BY(STAT_MemSizeTemp, MemSizeTemp);
	DEC_MEMORY_STAT_BY(STAT_MemSizePersist, MemSizePersist);
	
	FEvent* Signal = FGenericPlatformProcess::GetSynchEventFromPool(false);

	ENQUEUE_RENDER_COMMAND(NNE_DmlModel_ReleaseResources)
	(
		[this, &Signal](FRHICommandListImmediate& RHICmdList)
		{
			NNE_TRACE_EVENT_SCOPED(NNE_DmlModel_ReleaseResources_RT);

			// Need to wait for GPU to finish to not release resources possibly still in use
			// This needs proper rework!!!
			RHICmdList.BlockUntilGPUIdle();

			Signal->Trigger();
		}
	);

	Signal->Wait();
	FGenericPlatformProcess::ReturnSynchEventToPool(Signal);

	for (FOperatorDml* Op : Operators)
	{
		delete Op;
	}
}

bool FModelInstance::Init(TConstArrayView<uint8> ModelData, FDmlDeviceContext* InDevCtx)
{
	check(Operators.IsEmpty());
	ConstantCPUTensorIndices.Reset();
	
	check(ModelData.Num() > 0);
	FNNERuntimeFormat	Format;
	int32 GuidSize = FModelInfo::Get()->GetGuidSize();
	int32 VersionSize = FModelInfo::Get()->GetVersionSize();

	if (!LoadModel(ModelData, Format, GuidSize + VersionSize))
	{
		return false;
	}

	DevCtx = InDevCtx;
	DynamicRHI = GetID3D12PlatformDynamicRHI();

	HRESULT Res = DevCtx->Device->CreateOperatorInitializer(0, nullptr, DML_PPV_ARGS(&OpInit));
	if (!OpInit)
	{
		UE_LOG(LogNNE, Error, TEXT("Failed to create DML operator initializer"));
		return false;
	}

	GraphOperators.Reset();
	GraphOpInputIndices.Reset();
	GraphOpOutputIndices.Reset();
	MemSizeWeights = 0;

	// Loop over all operators in the model and create them
	for (int32 Idx = 0; Idx < Format.Operators.Num(); ++Idx)
	{
		const FString TypeName = Format.Operators[Idx].TypeName;

		FGraphOpDesc				OpDesc;
		TArray<NNE::FTensorDesc>	OpInputTensors;
		TArray<NNE::FTensorDesc>	OpOutputTensors;
		NNE::FAttributeMap			AttributeMap;

		OpDesc.InputStart = GraphOpInputIndices.Num();
		OpDesc.OutputStart = GraphOpOutputIndices.Num();

		for (int32 InputTensorIndex : Format.Operators[Idx].InTensors)
		{
			const int32 WeightTensorIdx = WeightTensorIndices.Find(InputTensorIndex);

			if (WeightTensorIdx >= 0)
			{
				TConstArrayView<uint8> TensorData = WeightTensorRDGs[WeightTensorIdx].GetPreparedData<uint8>();
				
				MemSizeWeights += Align(TensorData.Num(), DML_MINIMUM_BUFFER_TENSOR_ALIGNMENT);
			}

			if (Format.Tensors[InputTensorIndex].Type != ENNEFormatTensorType::Empty)
			{
				OpInputTensors.Emplace(AllSymbolicTensorDescs[InputTensorIndex]);
				GraphOpInputIndices.Emplace(InputTensorIndex);
			}
			else
			{
				OpInputTensors.Emplace(NNE::FTensorDesc::Make(TEXT(""), {}, ENNETensorDataType::None));
				GraphOpInputIndices.Emplace(GEmptyTensorIdx);
			}
		}

		for (int32 OutputTensorIndex : Format.Operators[Idx].OutTensors)
		{
			OpOutputTensors.Emplace(AllSymbolicTensorDescs[OutputTensorIndex]);
			GraphOpOutputIndices.Emplace(OutputTensorIndex);
		}

		for (const FNNEFormatAttributeDesc& Desc : Format.Operators[Idx].Attributes)
		{
			AttributeMap.SetAttribute(Desc.Name, Desc.Value);
		}

		FOperatorDml* Op = OpCreate({{TypeName, Format.Operators[Idx].DomainName}, Format.Operators[Idx].Version}, OpInputTensors, OpOutputTensors, AttributeMap);

		if (!Op)
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to create DML operator:%s"), *TypeName);
			return false;
		}

		OpDesc.OpIndex = Operators.Num();
		Operators.Add(Op);

		// Remap inputs
		TConstArrayView<int32> OpRemappedInputs = Op->GetRemappedInputs();

		for (int32 InputIdx = 0; InputIdx < OpRemappedInputs.Num(); ++InputIdx)
		{
			GraphOpInputIndices[OpDesc.InputStart + InputIdx] = OpRemappedInputs[InputIdx];
		}

		OpDesc.InputCount = OpInputTensors.Num();
		OpDesc.OutputCount = OpOutputTensors.Num();
		OpDesc.DbgName = TypeName;

		GraphOperators.Emplace(OpDesc);

		// Add constant CPU tensor indices
		TConstArrayView<int32> OpConstantCPUInputs = Op->GetConstantCPUInputs();

		for (int32 InputIdx = OpConstantCPUInputs.Num() - 1; InputIdx >= 0; --InputIdx)
		{
			const int32 ConstIdx = OpConstantCPUInputs[InputIdx];
			const int32 TensorIdx = GraphOpInputIndices[OpDesc.InputStart + ConstIdx];

			if (TensorIdx != GEmptyTensorIdx)
			{
				ConstantCPUTensorIndices.AddUnique(TensorIdx);
			}
		}
	}

	return true;	
}

bool FModelInstance::InitCompiledOp()
{
	NNE_TRACE_EVENT_SCOPED(NNE_DmlModelInstance_InitCompiledOp);

	HRESULT					Res;
	IDMLDevice*				Device = DevCtx->Device;
	IDMLCompiledOperator*	CompiledOps[] = { CompiledOp };
	
	Res = OpInit->Reset(UE_ARRAY_COUNT(CompiledOps), CompiledOps);
	if (FAILED(Res))
	{
		UE_LOG(LogNNE, Error, TEXT("Failed to reset DML operator initializer"));
		return false;
	}

	DML_BINDING_PROPERTIES InitBindProps = OpInit->GetBindingProperties();
	DML_BINDING_PROPERTIES ExecBindProps = CompiledOp->GetBindingProperties();

	DescCount = FMath::Max(InitBindProps.RequiredDescriptorCount, ExecBindProps.RequiredDescriptorCount);
	
	D3D12_DESCRIPTOR_HEAP_DESC	HeapDesc = {};

	HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	HeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	HeapDesc.NumDescriptors = DescCount;

	Res = DevCtx->D3D12Device->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(&DescHeap));
	if (!DescHeap)
	{
		UE_LOG(LogNNE, Error, TEXT("Failed to create descriptor heap, res:%x"), Res);
		return false;
	}

	DescSize = DevCtx->D3D12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	BindingTable.Reset(new FBindingTable());
	if (!BindingTable->Init(this))
	{
		return false;
	}

	MemSizeTemp = ExecBindProps.TemporaryResourceSize;
	MemSizePersist = ExecBindProps.PersistentResourceSize;

	FEvent* Signal = FGenericPlatformProcess::GetSynchEventFromPool(false);
	
	ENQUEUE_RENDER_COMMAND(NNE_DmlModelInstance_InitCompiledOp)
	(
		[
			this, 
			Signal, 
			MemSizeInitTemp = InitBindProps.TemporaryResourceSize
		]
		(FRHICommandListImmediate& RHICmdList)
		{
			NNE_TRACE_EVENT_SCOPED(NNE_DmlModelInstance_InitCompiledOp_RT);

			FRHIBufferInputArray	Inputs;

			for (int32 InputIdx : InputTensorIndices)
			{
				if (ConstantCPUTensorIndices.Find(InputIdx) == INDEX_NONE)
				{
					Inputs.Emplace(nullptr);
				}
			}

			TArray<CD3DX12_RESOURCE_BARRIER, TInlineAllocator<MaxNumInputs>>	Barriers;
			FBufferRHIRef	UploadBuff;

			if (MemSizeWeights)
			{
				UploadBuff = CreateRHIBuffer(RHICmdList, MemSizeWeights, BUF_ShaderResource | BUF_Dynamic | BUF_FastVRAM, ERHIAccess::CopySrc, TEXT("NNE_DmlModelInstance_UploadBuffer"));
				check(UploadBuff);
				UploadBuff->DisableLifetimeExtension();

				uint8*	UploadBuffPtr = static_cast<uint8*>(RHICmdList.LockBuffer(UploadBuff, 0, MemSizeWeights, RLM_WriteOnly));
				uint64	UploadOffset = 0;
				
				for (int32 WeightIdx = 0; WeightIdx < WeightTensorIndices.Num(); ++WeightIdx)
				{
					const int32 TensorIdx = WeightTensorIndices[WeightIdx];
					if (ConstantCPUTensorIndices.Find(TensorIdx) != -1)
					{
						continue;
					}
					
					const FTensorRDG&		Tensor = WeightTensorRDGs[WeightIdx];
					TConstArrayView<uint8>	TensorData = Tensor.GetPreparedData<uint8>();

					FBufferRHIRef	WeightBuff;
					const uint32	WeightBuffSize = Util::AlignBufferSize(TensorData.Num());
					
					WeightBuff = CreateRHIBuffer(RHICmdList, WeightBuffSize, WeightBuffUsage, WeightBuffAccess, *Tensor.GetName());
					check(WeightBuff);
					WeightBuff->DisableLifetimeExtension();
					
					FMemory::Memcpy(UploadBuffPtr + UploadOffset, TensorData.GetData(), TensorData.Num());
					RHICmdList.CopyBufferRegion(WeightBuff, 0, UploadBuff, UploadOffset, TensorData.Num());
					UploadOffset += Align(TensorData.Num(), DML_MINIMUM_BUFFER_TENSOR_ALIGNMENT);

					Inputs.Emplace(WeightBuff);
				
					Barriers.Emplace(
						CD3DX12_RESOURCE_BARRIER::Transition(
							DynamicRHI->RHIGetResource(WeightBuff),
							D3D12_RESOURCE_STATE_COPY_DEST,
							D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
					);
				}

				RHICmdList.UnlockBuffer(UploadBuff);

				INC_MEMORY_STAT_BY(STAT_MemSizeWeights, MemSizeWeights);
			}

			if (MemSizePersist)
			{
				PersistBuff = CreateRHIBuffer(RHICmdList, MemSizePersist, PersistBuffFlags, PersistBuffAccess, TEXT("NNE_DmlModelInstance_PeristBuff"));
				check(PersistBuff.IsValid());
				INC_MEMORY_STAT_BY(STAT_MemSizePersist, MemSizePersist);
			}

			if (MemSizeTemp)
			{
				INC_MEMORY_STAT_BY(STAT_MemSizeTemp, MemSizeTemp);
			}

			FBufferRHIRef InitTempBuff;

			if (MemSizeInitTemp)
			{
				InitTempBuff = CreateRHIBuffer(RHICmdList, MemSizeInitTemp, TempBuffFlags, TempBuffAccess, TEXT("NNE_DmlModelInstance_InitTempBuff"));
				InitTempBuff->DisableLifetimeExtension();
				check(InitTempBuff.IsValid());
			}

			RHICmdList.EnqueueLambda(
				[this, Inputs = MoveTemp(Inputs), Barriers = MoveTemp(Barriers), InitTempBuff = MoveTemp(InitTempBuff)](FRHICommandListImmediate& RHICmdList)
				{
					NNE_TRACE_EVENT_SCOPED(NNE_DmlModelInstance_InitCompiledOpD3D_RHI);

					ID3D12GraphicsCommandList* D3DCmdList = nullptr;

					D3DCmdList = DynamicRHI->RHIGetGraphicsCommandList(DevCtx->DeviceIndex);
					D3DCmdList->SetName(TEXT("NNE_DmlModelInstance_InitCompiledOp_CmdList"));

					BindingTable->Bind(OpInit, Inputs, PersistBuff, InitTempBuff);
			
					D3DCmdList = DynamicRHI->RHIGetGraphicsCommandList(DevCtx->DeviceIndex);

					if (!Barriers.IsEmpty())
					{
						D3DCmdList->ResourceBarrier(Barriers.Num(), Barriers.GetData());
					}
					D3DCmdList->SetDescriptorHeaps(1, &DescHeap);
					DevCtx->CmdRec->RecordDispatch(D3DCmdList, OpInit, BindingTable->Get());

					DynamicRHI->RHIFinishExternalComputeWork(DevCtx->DeviceIndex, D3DCmdList);
				}
			);
			
			// Since the operator initializer and the compiled operator share GPU resources (currently the descriptor heap),
			// we have to wait for the initializer on the GPU to finish, before we can dispatch the compiled operator!
			RHICmdList.BlockUntilGPUIdle();

			Signal->Trigger();
		}
	);

	Signal->Wait();
	FGenericPlatformProcess::ReturnSynchEventToPool(Signal);

	return true;
}

BEGIN_SHADER_PARAMETER_STRUCT(FDmlModelDispatchPassParameters, )
	RDG_BUFFER_ACCESS_ARRAY(InputBuffers)
	RDG_BUFFER_ACCESS_ARRAY(OutputBuffers)
END_SHADER_PARAMETER_STRUCT()

void FModelInstance::AddDispatchOps_RenderThread(FRDGBuilder& GraphBuilder)
{
	NNE_TRACE_EVENT_SCOPED(NNE_DmlModelInstance_Dispatch);
	INC_DWORD_STAT(STAT_InferenceCount);

	FDmlModelDispatchPassParameters* DispatchParams = GraphBuilder.AllocParameters<FDmlModelDispatchPassParameters>();

	for (int32 Idx = 0; Idx < InputTensorIndices.Num(); ++Idx)
	{
		DispatchParams->InputBuffers.Emplace(AllTensorRDGRefs[InputTensorIndices[Idx]]->GetBuffer(), ERHIAccess::UAVCompute);
	}

	int32 NumWeightTensors = 0;

	for (int32 Idx = 0; Idx < WeightTensorIndices.Num(); ++Idx)
	{
		if (ConstantCPUTensorIndices.Find(WeightTensorIndices[Idx]) == INDEX_NONE)
		{
			++NumWeightTensors;
		}
	}

	for (int32 Idx = 0; Idx < OutputTensorIndices.Num(); ++Idx)
	{
		DispatchParams->OutputBuffers.Emplace(AllTensorRDGRefs[OutputTensorIndices[Idx]]->GetBuffer(), ERHIAccess::UAVCompute);
	}

	RDG_EVENT_SCOPE(GraphBuilder, "NNE_DmlModelInstance_Dispatch_Pass");
	RDG_GPU_STAT_SCOPE_VERBOSE(GraphBuilder, NNE_DmlModelInstance_Dispatch_GPU, TEXT("NNE_DmlModelInstance_Dispatch_GPU"));
	RDG_CSV_STAT_EXCLUSIVE_SCOPE(GraphBuilder, NNE_DmlModelInstance_Dispatch);

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("NNE_DmlModelInstance_Dispatch_Pass"),
		DispatchParams,
		ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
		[this, DispatchParams, NumWeightTensors](FRHICommandListImmediate& RHICmdList)
		{
			NNE_TRACE_EVENT_SCOPED(NNE_DmlModelInstance_Dispatch_Pass);

			FRHIBufferInputArray	RHIInputBuffers;
			FRHIBufferOutputArray	RHIOutputBuffers;

			for (FRDGBuffer* RDGBuffer : DispatchParams->InputBuffers)
			{
				RDGBuffer->MarkResourceAsUsed();
				RHIInputBuffers.Add(RDGBuffer->GetRHI());
			}

			for (int32 Idx = 0; Idx < NumWeightTensors; ++Idx)
			{
				RHIInputBuffers.Add(nullptr);
			}

			for (FRDGBuffer* RDGBuffer : DispatchParams->OutputBuffers)
			{
				RDGBuffer->MarkResourceAsUsed();
				RHIOutputBuffers.Add(RDGBuffer->GetRHI());
			}

			FBufferRHIRef	TempBuff = nullptr;

			if (MemSizeTemp)
			{
				TempBuff = CreateRHIBuffer(RHICmdList, MemSizeTemp, TempBuffFlags, TempBuffAccess, TEXT("NNE_DmlModelInstance_TempBuff"));
				check(TempBuff.IsValid());
				TempBuff->DisableLifetimeExtension();
			}

			{
				SCOPED_GPU_STAT(RHICmdList, NNE_DmlModelInstance_DispatchD3D_GPU)

				RHICmdList.EnqueueLambda(
					[this, InputBuffers = MoveTemp(RHIInputBuffers), OutputBuffers = MoveTemp(RHIOutputBuffers), TempBuff = MoveTemp(TempBuff)](FRHICommandListImmediate& RHICmdList)
					{
						NNE_TRACE_EVENT_SCOPED(NNE_DmlModelInstance_DispatchD3D_RHI);

						TArray<CD3DX12_RESOURCE_BARRIER, TInlineAllocator<MaxNumInputs + MaxNumOutputs>>	PreBarriers;
						TArray<CD3DX12_RESOURCE_BARRIER, TInlineAllocator<MaxNumOutputs * 2>>				PostBarriers;

						for (FRHIBuffer* Buffer : InputBuffers)
						{
							if (!Buffer)
							{
								continue;
							}

							ID3D12Resource* Resource = DynamicRHI->RHIGetResource(Buffer);

							//Note: We should not assume COPY_DEST state
							PreBarriers.Emplace(
								CD3DX12_RESOURCE_BARRIER::Transition(
									Resource,
									D3D12_RESOURCE_STATE_COPY_DEST,
									D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
							);
						}

						for (FRHIBuffer* Buffer : OutputBuffers)
						{
							ID3D12Resource* Resource = DynamicRHI->RHIGetResource(Buffer);

							if ((Buffer->GetUsage() & EBufferUsageFlags::SourceCopy) == EBufferUsageFlags::SourceCopy)
							{
								PreBarriers.Emplace(
									CD3DX12_RESOURCE_BARRIER::Transition(
										Resource,
										D3D12_RESOURCE_STATE_COPY_SOURCE,
										D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
								);

								PostBarriers.Emplace(
									CD3DX12_RESOURCE_BARRIER::Transition(
										Resource,
										D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
										D3D12_RESOURCE_STATE_COPY_SOURCE)
								);
							}
						}

						PostBarriers.Add(CD3DX12_RESOURCE_BARRIER::UAV(nullptr));

						BindingTable->Bind(CompiledOp, InputBuffers, OutputBuffers, PersistBuff, TempBuff);
						
						ID3D12GraphicsCommandList* D3DCmdList = nullptr;

						D3DCmdList = DynamicRHI->RHIGetGraphicsCommandList(DevCtx->DeviceIndex);
						D3DCmdList->SetName(TEXT("NNE_DmlModelInstance_Dispatch_CmdList"));
						D3DCmdList->SetDescriptorHeaps(1, &DescHeap);
						D3DCmdList->ResourceBarrier(PreBarriers.Num(), PreBarriers.GetData());
						DevCtx->CmdRec->RecordDispatch(D3DCmdList, CompiledOp, BindingTable->Get());
						D3DCmdList->ResourceBarrier(PostBarriers.Num(), PostBarriers.GetData());
						
						DynamicRHI->RHIFinishExternalComputeWork(DevCtx->DeviceIndex, D3DCmdList);
					}
				);
			}
		}
	);
}

/**
* Create an instance of FOperatorDml
* Note: The IDMLOperator (member of FOperatorDml) is still not created
*/
FOperatorDml* FModelInstance::OpCreate(const FOperatorDesc& OpDesc, TConstArrayView<NNE::FTensorDesc> Inputs, TConstArrayView<NNE::FTensorDesc> Outputs, const NNE::FAttributeMap& Attributes)
{
	FOperatorRegistryDml::OperatorCreateFunc CreateFn = FOperatorRegistryDml::Get()->OpFind(OpDesc);

	if (!CreateFn)
	{
		UE_LOG(LogNNE, Error, TEXT("Failed to find DML operator: %s"), *OpDesc.GetFullName());
		return nullptr;
	}

	FOperatorDml* Op = CreateFn();

	if (!Op->Initialize(Inputs, Outputs, Attributes))
	{
		delete Op;

		UE_LOG(LogNNE, Error, TEXT("Failed to initialize DML operator: %s"), *OpDesc.GetFullName());
		return nullptr;
	}

	return Op;
}

FBufferRHIRef FModelInstance::CreateRHIBuffer(FRHICommandListImmediate& RHICmdList, uint32 Size, EBufferUsageFlags Usage, ERHIAccess Access, const TCHAR* DbgName)
{
	FBufferRHIRef Buff = nullptr;

	if (Size)
	{
		FRHIResourceCreateInfo CreateInfo(DbgName);
		
		Buff = RHICmdList.CreateBuffer(Size, Usage, 1, Access, CreateInfo);
	}

	check(Buff);
	return Buff;
}

int FModelInstance::PrepareTensorShapesAndData()
{
	check(AllTensorRDGRefs.Num() == AllSymbolicTensorDescs.Num());

	if (Operators.Num() == 0)
	{
		UE_LOG(LogNNE, Error, TEXT("No operators in model"));
		return -1;
	}

	// Loop through all operators:
	// 1. Run shape inference with PrepareOutputs()
	// 2. Create DML operator
	TArray<NNE::Internal::FTensorRef>	OpInputs;
	TArray<NNE::Internal::FTensorRef>	OpOutputs;

	for (const FGraphOpDesc& OpDesc : GraphOperators)
	{
		OpInputs.Reset();
		OpOutputs.Reset();

		FOperatorDml* Op = Operators[OpDesc.OpIndex];

		for (int32 Idx = 0; Idx < OpDesc.InputCount; ++Idx)
		{
			const int32 InputIdx = GraphOpInputIndices[OpDesc.InputStart + Idx];

			if (InputIdx != GEmptyTensorIdx)
			{
				OpInputs.Emplace(AllTensorRDGRefs[InputIdx]);
			}
		}

		for (int32 Idx = 0; Idx < OpDesc.OutputCount; ++Idx)
		{
			const int32 OutputIdx = GraphOpOutputIndices[OpDesc.OutputStart + Idx];
			OpOutputs.Emplace(AllTensorRDGRefs[OutputIdx]);
		}

		if (Op->PrepareOutputs(OpInputs, OpOutputs) != 0)
		{
			return -1;
		}

		if (!Op->Create(DevCtx->Device, OpInputs, OpOutputs))
		{
			return -1;
		}
	}

	FGraphBuilder DmlGraphBuilder;

	CompiledOp = DmlGraphBuilder.Compile(this);
	if (!CompiledOp)
	{
		UE_LOG(LogNNE, Error, TEXT("Failed to compile DML graph"));
		return -1;
	}

	if (!InitCompiledOp())
	{
		return -1;
	}

	return 0;
}

TSharedPtr<NNE::IModelInstanceRDG> FModel::CreateModelInstanceRDG()
{
	FModelInstance* ModelInstance = new FModelInstance();

	check(ModelData.IsValid());
	if (!ModelInstance->Init(ModelData->GetView(), DevCtx))
	{
		delete ModelInstance;
		return TSharedPtr<NNE::IModelInstanceRDG>();
	}

	NNE::IModelInstanceRDG* IModelInstance = static_cast<NNE::IModelInstanceRDG*>(ModelInstance);
	return TSharedPtr<NNE::IModelInstanceRDG>(IModelInstance);
}

FModel::FModel(const TSharedPtr<NNE::FSharedModelData>& InModelData, FDmlDeviceContext* InDevCtx)
	: ModelData(InModelData), DevCtx(InDevCtx)
{
}

#endif // NNE_USE_DIRECTML

} // namespace UE::NNERuntimeRDG::Private::Dml
