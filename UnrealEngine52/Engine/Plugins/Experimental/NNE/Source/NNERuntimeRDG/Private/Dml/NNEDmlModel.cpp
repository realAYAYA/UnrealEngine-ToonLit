// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNEDmlModel.h"

#ifdef NNE_USE_DIRECTML
#include "NNEDmlOperator.h"

#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"

#include "ID3D12DynamicRHI.h"
#endif

namespace UE::NNERuntimeRDG::Private::Dml
{

FModelInfo* FModelInfo::Get()
{
	static FModelInfo Inst;

	return &Inst;
}

FModelInfo::FModelInfo()
	: Guid((int32)'R', (int32)'D', (int32)'G', (int32)'D')
	, Version(0x1)
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

//
//
//
class FModel::FDebugName
{
	static constexpr int32 Size = 128;

public:

	FDebugName()
	{
		Str[0] = '\0';
		Length = 0;
	}

	FDebugName(const FString& InStr)
	{
		FTCHARToUTF8 Conv(*InStr);

		Length = Conv.Length() + 1 < Size ? Conv.Length() + 1 : Size - 1;
		FCStringAnsi::Strncpy(Str, Conv.Get(), Length);
		Str[Length] = '\0';
	}

	FDebugName(FStringView InStr)
	{
		FTCHARToUTF8 Conv(InStr.GetData());

		Length = Conv.Length() + 1 < Size ? Conv.Length() + 1 : Size - 1;
		FCStringAnsi::Strncpy(Str, Conv.Get(), Length);
		Str[Length] = '\0';
	}

	~FDebugName()
	{
	}

	const char* Get() const
	{
		return Str;
	}

private:

	char	Str[Size];
	int32	Length;
};

//
//
//
class FModel::FBindingTable
{
public:

	bool Init(FModel* InModel)
	{
		Model = InModel;
		DynamicRHI = InModel->DynamicRHI;

		return true;
	}

#ifdef NNE_USE_D3D12_RESOURCES
	void Bind(IDMLOperatorInitializer* OpInit, TConstArrayView<FRHIBuffer*> InputBuffers, ID3D12Resource* PersistBuff, ID3D12Resource* TempBuff = nullptr)
#else
	void Bind(IDMLOperatorInitializer* OpInit, TConstArrayView<FRHIBuffer*> InputBuffers, FRHIBuffer* PersistBuff, FRHIBuffer* TempBuff = nullptr)
#endif
	{
		Reset(OpInit);

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

		if (PersistBuff)
		{
#ifdef NNE_USE_D3D12_RESOURCES
			PersistBind = DML_BUFFER_BINDING { PersistBuff, 0, PersistBuff->GetDesc().Width };
#else
			PersistBind = MakeBind(PersistBuff);
#endif
		}

		BindingTable->BindOutputs(1, &PersistBindDesc);

		DML_BUFFER_BINDING			TempBind{};
		DML_BINDING_DESC			TempBindDesc{ DML_BINDING_TYPE_BUFFER, &TempBind };

		if (TempBuff)
		{
#ifdef NNE_USE_D3D12_RESOURCES
			TempBind = { TempBuff, 0, TempBuff->GetDesc().Width };
#else
			TempBind = MakeBind(TempBuff);
#endif
			BindingTable->BindTemporaryResource(&TempBindDesc);
		}
	}

#ifdef NNE_USE_D3D12_RESOURCES
	void Bind(IDMLCompiledOperator* Op, TConstArrayView<FRHIBuffer*> InputBuffers, TConstArrayView<FRHIBuffer*> OutputBuffers, ID3D12Resource* PersistBuff = nullptr, ID3D12Resource* TempBuff = nullptr)
#else
	
	void Bind(IDMLCompiledOperator* Op, TConstArrayView<FRHIBuffer*> InputBuffers, TConstArrayView<FRHIBuffer*> OutputBuffers, FRHIBuffer* PersistBuff = nullptr, FRHIBuffer* TempBuff = nullptr)
#endif
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

		if (PersistBuff)
		{
#ifdef NNE_USE_D3D12_RESOURCES
			PersistBind = { PersistBuff, 0, PersistBuff->GetDesc().Width };
#else
			PersistBind =  MakeBind(PersistBuff);MakeBind(PersistBuff);
#endif
			BindingTable->BindPersistentResource(&PersistBindDesc);
		}

		DML_BUFFER_BINDING			TempBind{};
		DML_BINDING_DESC			TempBindDesc{ DML_BINDING_TYPE_BUFFER, &TempBind };

		if (TempBuff)
		{
			
#ifdef NNE_USE_D3D12_RESOURCES
			TempBind = { TempBuff, 0, TempBuff->GetDesc().Width };
#else
			TempBind = MakeBind(TempBuff);
#endif
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
				UE_LOG(LogNNE, Warning, TEXT("Failed to create DML binding table, res:%d"), Res);
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

		return DML_BUFFER_BINDING{ Resource, 0, Buffer->GetSize() };
	}

	TComPtr<IDMLBindingTable>										BindingTable;
	TArray<DML_BUFFER_BINDING, TInlineAllocator<MaxNumInputs>>		InputBinds;
	TArray<DML_BINDING_DESC, TInlineAllocator<MaxNumInputs>>		InputBindDescs;
	TArray<DML_BUFFER_BINDING, TInlineAllocator<MaxNumOutputs>>		OutputBinds;
	TArray<DML_BINDING_DESC, TInlineAllocator<MaxNumOutputs>>		OutputBindDescs;
	ID3D12DynamicRHI*												DynamicRHI;
	FModel*															Model;
};

//
//
//
class FModel::FGraphBuilder
{
public:

	struct FOpDesc
	{
		FOperatorDml*	Op;
		int32			InputStart;
		int32			InputCount;
		int32			OutputStart;
		int32			OutputCount;
		FDebugName		DbgName;
	};

	struct FGraphDesc
	{
		TConstArrayView<NNECore::Internal::FTensor>	AllTensors;
		TConstArrayView<int32>		InputIndices;
		TConstArrayView<int32>		OutputIndices;
		TConstArrayView<int32>		WeightIndices;
		TConstArrayView<int32>		ConstantCPUIndices;
		TConstArrayView<int32>		IntermediateIndices;
		TConstArrayView<FTensorRDG>	WeightTensors;
		TConstArrayView<FOpDesc>	Operators;
		TConstArrayView<int32>		OpInputIndices;
		TConstArrayView<int32>		OpOutputIndices;
	};
	
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

	IDMLCompiledOperator* Compile(FDmlDeviceContext* DevCtx, const FGraphDesc& InGraph)
	{
		IDMLDevice*				Device = DevCtx->Device;
		TComPtr<IDMLDevice1>	Device1;

		Device1.FromQueryInterface(__uuidof(IDMLDevice1), DevCtx->Device);
		check(Device1);
		if (!Device1)
		{
			return nullptr;
		}

		if (!AddEdges(InGraph))
		{
			return nullptr;
		}

		TArray<DML_INPUT_GRAPH_EDGE_DESC>			InputEdges;
		TArray<DML_OUTPUT_GRAPH_EDGE_DESC>			OutputEdges;
		TArray<DML_INTERMEDIATE_GRAPH_EDGE_DESC>	IntermediateEdges;
		TArray<FDebugName>							DbgInputNames;
		TArray<FDebugName>							DbgIntermediateNames;
		TArray<FDebugName>							DbgOutputNames;

		DbgInputNames.Reserve(InGraph.AllTensors.Num());
		DbgIntermediateNames.Reserve(InGraph.AllTensors.Num());

		for (const FEdge& Edge : Edges)
		{
			if (Edge.Type == EEdgeType::Input)
			{
				DML_INPUT_GRAPH_EDGE_DESC& Input = InputEdges.Add_GetRef({});

				Input.GraphInputIndex = Edge.NodeSrcOutput;
				Input.ToNodeIndex = Edge.NodeDst;
				Input.ToNodeInputIndex = Edge.NodeDstInput;
				
				const FDebugName& DbgName = DbgInputNames.Add_GetRef(InGraph.AllTensors[Edge.TensorIdx].GetName());
				Input.Name = DbgName.Get();
			}
			else if (Edge.Type == EEdgeType::Output)
			{
				DML_OUTPUT_GRAPH_EDGE_DESC&	Output = OutputEdges.Add_GetRef({});

				Output.GraphOutputIndex = Edge.NodeDstInput;
				Output.FromNodeIndex = Edge.NodeSrc;
				Output.FromNodeOutputIndex = Edge.NodeSrcOutput;

				const FDebugName& DbgName = DbgOutputNames.Add_GetRef(InGraph.AllTensors[Edge.TensorIdx].GetName());
				Output.Name = DbgName.Get();
			}
			else if (Edge.Type == EEdgeType::Intermediate)
			{
				DML_INTERMEDIATE_GRAPH_EDGE_DESC& Intermediate = IntermediateEdges.Add_GetRef({});

				Intermediate.FromNodeIndex = Edge.NodeSrc;
				Intermediate.FromNodeOutputIndex = Edge.NodeSrcOutput;
				Intermediate.ToNodeIndex = Edge.NodeDst;
				Intermediate.ToNodeInputIndex = Edge.NodeDstInput;

				const FDebugName& DbgName = DbgIntermediateNames.Add_GetRef(InGraph.AllTensors[Edge.TensorIdx].GetName());
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
			
		Res = Device1->CompileGraph(&Graph, DML_EXECUTION_FLAG_NONE, DML_PPV_ARGS(&Op));
		if (FAILED(Res))
		{
			UE_LOG(LogNNE, Warning, TEXT("Error:Failed to compile DML graph"));
			Op = nullptr;
		};

		return Op;
	}

private:

	bool AddEdges(const FGraphDesc& InGraph)
	{
		Edges.Reset();
		TensorInConnCounts.Reset();
		TensorOutConnCounts.Reset();
		Operators.Reset();
		NumInputs = 0;
		NumOutputs = 0;

		TensorInConnCounts.SetNumZeroed(InGraph.AllTensors.Num());
		TensorOutConnCounts.SetNumZeroed(InGraph.AllTensors.Num());
		Operators.Reset(InGraph.Operators.Num());

		for (int32 Idx : InGraph.OpInputIndices)
		{
			TensorInConnCounts[Idx] += 1;
		}

		for (int32 Idx : InGraph.OpOutputIndices)
		{
			TensorOutConnCounts[Idx] += 1;
		}

		if (!AddInputEdges(InGraph.InputIndices, InGraph))
		{
			UE_LOG(LogNNE, Warning, TEXT("DMLGraphBuilder failed to add input tensors"));
			return false;
		}
		
		if (!AddInputEdges(InGraph.WeightIndices, InGraph))
		{
			UE_LOG(LogNNE, Warning, TEXT("DMLGraphBuilder failed to add weight tensors"));
			return false;
		}
		
		if (!AddOutputEdges(InGraph))
		{
			UE_LOG(LogNNE, Warning, TEXT("DMLGraphBuilder failed to add output tensors"));
			return false;
		}

		if (!AddIntermediateEdges(InGraph))
		{
			UE_LOG(LogNNE, Warning, TEXT("DMLGraphBuilder failed to add intermediate tensors"));
			return false;
		}

		if (!AddOperators(InGraph))
		{
			UE_LOG(LogNNE, Warning, TEXT("DMLGraphBuilder failed to add operators"));
			return false;
		}

		// Validate edges
		for (const FEdge& Edge : Edges)
		{
			if (Edge.Type == EEdgeType::Input)
			{
				if (Edge.NodeSrcOutput == -1 || Edge.NodeDst == -1 || Edge.NodeDstInput == -1)
				{
					UE_LOG(LogNNE, Warning, TEXT("DMLGraphBuilder error invalid input graph edge detected for tensor:%s"), *InGraph.AllTensors[Edge.TensorIdx].GetName());
					return false;
				}
			}
			else if (Edge.Type == EEdgeType::Output)
			{
				if (Edge.NodeDstInput == -1 || Edge.NodeSrc == -1 || Edge.NodeSrcOutput == -1)
				{
					UE_LOG(LogNNE, Warning, TEXT("DMLGraphBuilder error invalid output graph edge detected for tensor:%s"), *InGraph.AllTensors[Edge.TensorIdx].GetName());
					return false;
				}
			}
			else if (Edge.Type == EEdgeType::Intermediate)
			{
				if (Edge.NodeSrc == -1 || Edge.NodeSrcOutput == -1 || Edge.NodeDst == -1 || Edge.NodeDstInput == -1)
				{
					UE_LOG(LogNNE, Warning, TEXT("DMLGraphBuilder error invalid intermediate graph edge detected for tensor:%s"), *InGraph.AllTensors[Edge.TensorIdx].GetName());
					return false;
				}
			}
		}
		
		return true;
	}

	bool AddInputEdges(TConstArrayView<int32> InTensorIndices, const FGraphDesc& InGraph)
	{
		for (int32 Idx = 0; Idx < InTensorIndices.Num(); ++Idx)
		{
			const int32 TensorIdx = InTensorIndices[Idx];
			const int32 ConnCount = TensorInConnCounts[TensorIdx];

			if (ConnCount == 0)
			{
				if (InGraph.ConstantCPUIndices.Find(TensorIdx) == INDEX_NONE)
				{
					UE_LOG(LogNNE, Warning, TEXT("DmlGraphBuilder tensor:%d has no connections"), *InGraph.AllTensors[TensorIdx].GetName());
					return false;
				}
			}

			for (int32 ConnIdx = 0; ConnIdx < ConnCount; ++ConnIdx)
			{
				AddInputEdge(TensorIdx);
			}

			if (InGraph.ConstantCPUIndices.Find(TensorIdx) == INDEX_NONE)
			{
				++NumInputs;
			}
		}

		return true;
	}

	bool AddOutputEdges(const FGraphDesc& InGraph)
	{
		for (int32 Idx = 0; Idx < InGraph.OutputIndices.Num(); ++Idx)
		{
			const int32 TensorIdx = InGraph.OutputIndices[Idx];
			const int32 ConnCount = TensorOutConnCounts[TensorIdx];

			if (ConnCount == 0)
			{
				UE_LOG(LogNNE, Warning, TEXT("DmlGraphBuilder tensor:%d has no connections"), *InGraph.AllTensors[TensorIdx].GetName());
				return false;
			}

			for (int32 ConnIdx = 0; ConnIdx < ConnCount; ++ConnIdx)
			{
				AddOutputEdge(TensorIdx);
			}
		}

		return true;
	}

	bool AddIntermediateEdges(const FGraphDesc& InGraph)
	{
		for (int32 Idx = 0; Idx < InGraph.IntermediateIndices.Num(); ++Idx)
		{
			const int32 TensorIdx = InGraph.IntermediateIndices[Idx];
			const int32 InputConnCount = TensorInConnCounts[TensorIdx];
			const int32 OutputConnCount = TensorOutConnCounts[TensorIdx];

			if (InputConnCount == 0 || OutputConnCount == 0)
			{
				UE_LOG(LogNNE, Warning, TEXT("DmlGraphBuilder tensor:%d has no connections"), *InGraph.AllTensors[TensorIdx].GetName());
				return false;
			}

			for (int32 ConnIdx = 0; ConnIdx < InputConnCount; ++ConnIdx)
			{
				AddIntermediateEdge(TensorIdx, -1, -1);
			}
		}

		return true;
	}

	bool AddOperators(const FGraphDesc& InGraph)
	{
		for (const FOpDesc& CurrOp : InGraph.Operators) 
		{
			DML_OPERATOR_GRAPH_NODE_DESC& OpDesc = Operators.Add_GetRef({});

			OpDesc.Operator = CurrOp.Op->GetOperator();
			OpDesc.Name = CurrOp.DbgName.Get();

			const int32 NodeIdx = Operators.Num() - 1;

			for (int32 Idx = 0; Idx < CurrOp.InputCount; ++Idx)
			{
				const int32 TensorIdx = InGraph.OpInputIndices[Idx + CurrOp.InputStart];

				if (!ConnectEdgeDst(TensorIdx, NodeIdx, Idx))
				{
					UE_LOG(LogNNE, Warning, TEXT("DmlGraphBuilder failed to connect intermediate edge dst for tensor:%s"), *InGraph.AllTensors[TensorIdx].GetName());
					return false;
				}
			}

			for (int32 Idx = 0; Idx < CurrOp.OutputCount; ++Idx)
			{
				const int32 TensorIdx = InGraph.OpOutputIndices[Idx + CurrOp.OutputStart];
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
						UE_LOG(LogNNE, Warning, TEXT("DmlGraphBuilder failed to connect intermediate edge src for tensor:%s"), *InGraph.AllTensors[TensorIdx].GetName());
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

//
//
//
FModel::FModel()
{
}

//
//
//
FModel::~FModel()
{
}

//
//
//
bool FModel::Init(TConstArrayView<uint8> ModelData, FDmlDeviceContext* InDevCtx)
{
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
		UE_LOG(LogNNE, Warning, TEXT("Error:Failed to create DML operator initializer"));
		return false;
	}

	// DirectML requires all tensors to be concrete
	// Notes: to handle dynamic tensor desc, op should init from symbolic shapes
	TArray<NNECore::Internal::FTensor>	Tensors;

	Tensors.Reset(AllSymbolicTensorDescs.Num());
	for (const NNECore::FTensorDesc& TensorDesc : AllSymbolicTensorDescs)
	{
		Tensors.Emplace(NNECore::Internal::FTensor::MakeFromSymbolicDesc(TensorDesc));
	}

	TArray<FGraphBuilder::FOpDesc>	DmlGraphOperators;
	TArray<int32>					OpInputIndices;
	TArray<int32>					OpOutputIndices;
	uint64							TensorDataSize = 0;

	// Loop over all operators in the model and create them
	for (int32 Idx = 0; Idx < Format.Operators.Num(); ++Idx)
	{
		const FString TypeName = Format.Operators[Idx].TypeName;

		FGraphBuilder::FOpDesc					OpDesc;
		TArray<NNECore::Internal::FTensor>		OpInputTensors;
		TArray<NNECore::Internal::FTensor>		OpOutputTensors;
		NNECore::FAttributeMap					AttributeMap;

		OpDesc.InputStart = OpInputIndices.Num();
		OpDesc.OutputStart = OpOutputIndices.Num();

		for (int32 InputTensorIndex : Format.Operators[Idx].InTensors)
		{
			const int32 WeightTensorIdx = WeightTensorIndices.Find(InputTensorIndex);

			if (WeightTensorIdx >= 0)
			{
				TConstArrayView<uint8> TensorData = OpInputTensors.Emplace_GetRef(WeightTensorRDGs[WeightTensorIdx]).GetPreparedData<uint8>();
				
				TensorDataSize += Align(TensorData.Num(), DML_MINIMUM_BUFFER_TENSOR_ALIGNMENT);
			}
			else
			{
				NNECore::FTensorDesc SymbolicTensorDesc = AllSymbolicTensorDescs[InputTensorIndex];
				// Notes: to handle dynamic tensor desc, op should init from symbolic shapes
				OpInputTensors.Emplace(NNECore::Internal::FTensor::MakeFromSymbolicDesc(SymbolicTensorDesc));
			}

			OpInputIndices.Emplace(InputTensorIndex);
		}

		for (int32 OutputTensorIndex : Format.Operators[Idx].OutTensors)
		{
			NNECore::FTensorDesc SymbolicTensorDesc = AllSymbolicTensorDescs[OutputTensorIndex];
			// Notes: to handle dynamic tensor desc, op should init from symbolic shapes
			OpOutputTensors.Emplace(NNECore::Internal::FTensor::MakeFromSymbolicDesc(SymbolicTensorDesc));
			OpOutputIndices.Emplace(OutputTensorIndex);
		}

		for (const FNNEFormatAttributeDesc& Desc : Format.Operators[Idx].Attributes)
		{
			AttributeMap.SetAttribute(Desc.Name, Desc.Value);
		}

		OpDesc.Op = OpCreate(TypeName, OpInputTensors, OpOutputTensors, AttributeMap);

		if (!OpDesc.Op)
		{
			UE_LOG(LogNNE, Warning, TEXT("Error:Failed to create operator:%s"), *TypeName);
			return false;
		}

		// Remap inputs
		TConstArrayView<int32> OpRemappedInputs = OpDesc.Op->GetRemappedInputs();

		for (int32 InputIdx = 0; InputIdx < OpRemappedInputs.Num(); ++InputIdx)
		{
			OpInputIndices[OpDesc.InputStart + InputIdx] = OpRemappedInputs[InputIdx];
		}

		// Filter out the constant CPU inputs from the graph node inputs
		TConstArrayView<int32> OpConstantCPUInputs = OpDesc.Op->GetConstantCPUInputs();

		for (int32 InputIdx = OpConstantCPUInputs.Num() - 1; InputIdx >= 0; --InputIdx)
		{
			const int32 ConstIdx = OpConstantCPUInputs[InputIdx];
			const int32 TensorIdx = OpInputIndices[OpDesc.InputStart + ConstIdx];

			OpInputTensors.RemoveAt(ConstIdx);
			OpInputIndices.RemoveAt(OpDesc.InputStart + ConstIdx);

			ConstantCPUTensorIndices.AddUnique(TensorIdx);
		}

		OpDesc.InputCount = OpInputTensors.Num();
		OpDesc.OutputCount = OpOutputTensors.Num();
		OpDesc.DbgName = TypeName;

		DmlGraphOperators.Emplace(OpDesc);
	}

	FGraphBuilder				DmlGraphBuilder;
	FGraphBuilder::FGraphDesc	DmlGraphDesc;

	DmlGraphDesc.AllTensors = Tensors;
	DmlGraphDesc.InputIndices = InputTensorIndices;
	DmlGraphDesc.OutputIndices = OutputTensorIndices;
	DmlGraphDesc.WeightIndices = WeightTensorIndices;
	DmlGraphDesc.ConstantCPUIndices = ConstantCPUTensorIndices;
	DmlGraphDesc.IntermediateIndices = IntermediateTensorIndices;
	DmlGraphDesc.WeightTensors = WeightTensorRDGs;
	DmlGraphDesc.Operators = DmlGraphOperators;
	DmlGraphDesc.OpInputIndices = OpInputIndices;
	DmlGraphDesc.OpOutputIndices = OpOutputIndices;

	CompiledOp = DmlGraphBuilder.Compile(DevCtx, DmlGraphDesc);
	if (!CompiledOp)
	{
		return false;
	}

	return InitCompiledOp(OpInputIndices, TensorDataSize);
}

//
//
//
bool FModel::InitCompiledOp(TConstArrayView<int32> OpInputIndices, uint64 TensorDataSize)
{
	static constexpr EBufferUsageFlags	WeightBuffUsage = BUF_UnorderedAccess;
	static constexpr ERHIAccess			WeightBuffAccess = ERHIAccess::UAVMask;

	static constexpr EBufferUsageFlags	PersistBuffFlags = BUF_Static | BUF_ShaderResource | BUF_UnorderedAccess;
	static constexpr ERHIAccess			PersistBuffAccess = ERHIAccess::UAVMask;

	static constexpr EBufferUsageFlags	TempBuffFlags = BUF_Volatile | BUF_UnorderedAccess;
	static constexpr ERHIAccess			TempBuffAccess = ERHIAccess::UAVMask;

	HRESULT					Res;
	IDMLDevice*				Device = DevCtx->Device;
	IDMLCompiledOperator*	CompiledOps[] = { CompiledOp };
	
	Res = OpInit->Reset(UE_ARRAY_COUNT(CompiledOps), CompiledOps);
	if (FAILED(Res))
	{
		UE_LOG(LogNNE, Warning, TEXT("Error:Failed to reset DirectML operator initializer"));
		return false;
	}

	DML_BINDING_PROPERTIES InitBindProps = OpInit->GetBindingProperties();
	DML_BINDING_PROPERTIES ExecBindProps = CompiledOp->GetBindingProperties();

	DescCount = std::max(InitBindProps.RequiredDescriptorCount, ExecBindProps.RequiredDescriptorCount);
	
	D3D12_DESCRIPTOR_HEAP_DESC	HeapDesc = {};

	HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	HeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	HeapDesc.NumDescriptors = DescCount;

	Res = DevCtx->D3D12Device->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(&DescHeap));
	if (!DescHeap)
	{
		UE_LOG(LogNNE, Warning, TEXT("Failed to create descriptor heap, res:%x"), Res);
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

	ENQUEUE_RENDER_COMMAND(FModel_SetTensorData)
	(
		[
			this, 
			Signal, 
			InitTempMemSize = InitBindProps.TemporaryResourceSize,
			TensorDataSize
		]
		(FRHICommandListImmediate& RHICmdList)
		{
			FRHIBufferInputArray	Inputs;

			for (int32 InputIdx : InputTensorIndices)
			{
				if (ConstantCPUTensorIndices.Find(InputIdx) == INDEX_NONE)
				{
					Inputs.Emplace(nullptr);
				}
			}

			TArray<CD3DX12_RESOURCE_BARRIER, TInlineAllocator<MaxNumInputs>>	Barriers;
			FGPUFenceRHIRef	UploadFence = nullptr;

			if (TensorDataSize)
			{
				UploadFence = RHICreateGPUFence(TEXT("FInferenceModel_UploadFence"));
				FRHIBuffer*		UploadBuff = CreateRHIBuffer(RHICmdList, TensorDataSize, BUF_ShaderResource | BUF_Dynamic | BUF_FastVRAM, ERHIAccess::CopySrc, TEXT("FInferenceModel_UploadBuffer"));
				uint8*			UploadBuffPtr = static_cast<uint8*>(RHICmdList.LockBuffer(UploadBuff, 0, TensorDataSize, RLM_WriteOnly_NoOverwrite));
				uint64			UploadOffset = 0;

				for (int32 WeightIdx = 0; WeightIdx < WeightTensorIndices.Num(); ++WeightIdx)
				{
					int32 TensorIdx = WeightTensorIndices[WeightIdx];
					if (ConstantCPUTensorIndices.Find(TensorIdx) != -1)
					{
						continue;
					}
					
					const FTensorRDG&		Tensor = WeightTensorRDGs[WeightIdx];
					TConstArrayView<uint8>	TensorData = Tensor.GetPreparedData<uint8>();

					FBufferRHIRef WeightBuff;
					WeightBuff = CreateRHIBuffer(RHICmdList, TensorData.Num(), WeightBuffUsage, WeightBuffAccess, *Tensor.GetName());
				
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
				RHICmdList.WriteGPUFence(UploadFence);
			}

			if (MemSizePersist)
			{
#ifdef NNE_USE_D3D12_RESOURCES
				PersistBuff = CreateD3D12Buffer(MemSizePersist);
#else		
				PersistBuff = CreateRHIBuffer(RHICmdList, MemSizePersist, PersistBuffFlags, PersistBuffAccess, TEXT("FModelDml_PeristBuff"));
#endif
			}

			if (MemSizeTemp)
			{
#ifdef NNE_USE_D3D12_RESOURCES
				TempBuff = CreateD3D12Buffer(MemSizeTemp);
#else
				TempBuff = CreateRHIBuffer(RHICmdList, MemSizeTemp, TempBuffFlags, TempBuffAccess, TEXT("FModelDml_TempBuff"));
#endif
			}

#ifdef NNE_USE_D3D12_RESOURCES
			ID3D12Resource* InitTempBuff = nullptr;
#else
			FBufferRHIRef InitTempBuff;
#endif

			if (InitTempMemSize)
			{
#ifdef NNE_USE_D3D12_RESOURCES
				InitTempBuff = CreateD3D12Buffer(InitTempMemSize);
#else
				TempBuff = CreateRHIBuffer(RHICmdList, InitTempMemSize, TempBuffFlags, TempBuffAccess, TEXT("FModelDml_InitTempBuff"));
#endif
			}

			RHICmdList.EnqueueLambda(
				[this, Inputs, Barriers, InitTempBuff, UploadFence](FRHICommandListImmediate& RHICmdList)
				{
					while (UploadFence && UploadFence->NumPendingWriteCommands.GetValue() > 0)
					{
						FPlatformProcess::Sleep(0.001);
					}

					ID3D12GraphicsCommandList* D3DCmdList = nullptr;

					D3DCmdList = DynamicRHI->RHIGetGraphicsCommandList(DevCtx->DeviceIndex);

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
			
			RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
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

//
//
//
void FModel::AddDispatchOps_RenderThread(FRDGBuilder& GraphBuilder)
{
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

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("FDmlModelDispatch"),
		DispatchParams,
		ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
		[this, DispatchParams, NumWeightTensors](FRHICommandListImmediate& RHICmdList)
		{
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

			RHICmdList.EnqueueLambda(
				[this, InputBuffers = MoveTemp(RHIInputBuffers), OutputBuffers = MoveTemp(RHIOutputBuffers)](FRHICommandListImmediate& RHICmdList)
				{
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
									D3D12_RESOURCE_STATE_COMMON,
									D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
							);

							PostBarriers.Emplace(
								CD3DX12_RESOURCE_BARRIER::Transition(
									Resource,
									D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
									D3D12_RESOURCE_STATE_COPY_SOURCE)
							);
						}

						PostBarriers.Add(CD3DX12_RESOURCE_BARRIER::UAV(Resource));
					}

					//Note: We should use this instead of NNE_USE_D3D12_RESOURCES
					//FBufferRHIRef TempBuff = MemSizeTemp ? CreateRHIBuffer(RHICmdList, MemSizeTemp, TempBuffUsage, TempBuffAccess, TEXT("FModel_Dispatch_TempBuff")) : nullptr;

					BindingTable->Bind(CompiledOp, InputBuffers, OutputBuffers, PersistBuff, TempBuff);

					ID3D12GraphicsCommandList* D3DCmdList = nullptr;

					D3DCmdList = DynamicRHI->RHIGetGraphicsCommandList(DevCtx->DeviceIndex);
					D3DCmdList->SetDescriptorHeaps(1, &DescHeap);
					D3DCmdList->ResourceBarrier(PreBarriers.Num(), PreBarriers.GetData());
					DevCtx->CmdRec->RecordDispatch(D3DCmdList, CompiledOp, BindingTable->Get());
					D3DCmdList->ResourceBarrier(PostBarriers.Num(), PostBarriers.GetData());

					DynamicRHI->RHIFinishExternalComputeWork(DevCtx->DeviceIndex, D3DCmdList);
				}
			);
		}
	);
}

//
// Create operator
//
FOperatorDml* FModel::OpCreate(const FString& OpName, TArrayView<const NNECore::Internal::FTensor> InputTensorDescs, TArrayView<const NNECore::Internal::FTensor> OutputTensorDescs, const NNECore::FAttributeMap& Attributes)
{
	FOperatorRegistryDml::OperatorCreateFunc CreateFn = FOperatorRegistryDml::Get()->OpFind(OpName);

	if (!CreateFn)
	{
		UE_LOG(LogNNE, Warning, TEXT("Dml MLOperatorRegistry failed to find operator:%s"), *OpName);
		return nullptr;
	}

	FOperatorDml* Op = CreateFn();

	if (!Op->Initialize(DevCtx->Device, InputTensorDescs, OutputTensorDescs, Attributes))
	{
		delete Op;

		UE_LOG(LogNNE, Warning, TEXT("Error:Failed to initialize operator:%s"), *OpName);
		return nullptr;
	}

	Op->GetOperator()->SetName(*OpName);

	return Op;
}

FBufferRHIRef FModel::CreateRHIBuffer(FRHICommandListImmediate& RHICmdList, uint32 Size, EBufferUsageFlags Usage, ERHIAccess Access, const TCHAR* DbgName)
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

ID3D12Resource* FModel::CreateD3D12Buffer(uint32 Size, D3D12_RESOURCE_STATES ResourceState, D3D12_HEAP_TYPE HeapType, const TCHAR* DebugName)
{
	ID3D12Resource* Resource = nullptr;

	D3D12_RESOURCE_DESC ResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(Size,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	CD3DX12_HEAP_PROPERTIES HeapProps = CD3DX12_HEAP_PROPERTIES(HeapType);
	HRESULT Res;

	Res = DevCtx->D3D12Device->CreateCommittedResource(
		&HeapProps,
		D3D12_HEAP_FLAG_NONE,
		&ResourceDesc,
		ResourceState,
		nullptr,
		IID_PPV_ARGS(&Resource));

	if (FAILED(Res))
	{
		UE_LOG(LogNNE, Warning, TEXT("Error:FInferenceModel failed to create D3D12 resource"));
		return nullptr;
	}

	if (Resource && DebugName)
	{
		Resource->SetName(DebugName);
	}

	return Resource;
}

//
//
//
int FModel::PrepareTensorShapesAndData()
{
	for (NNECore::FTensorDesc SymbolicTensorDesc : AllSymbolicTensorDescs)
	{
		if (!SymbolicTensorDesc.GetShape().IsConcrete())
		{
			UE_LOG(LogNNE, Warning, TEXT("DML runtime does not support model with variable shapes yet."));
			return -1;
		}
	}

	return 0;
}

#endif // NNE_USE_DIRECTML

} // namespace UE::NNERuntimeRDG::Private::Dml


