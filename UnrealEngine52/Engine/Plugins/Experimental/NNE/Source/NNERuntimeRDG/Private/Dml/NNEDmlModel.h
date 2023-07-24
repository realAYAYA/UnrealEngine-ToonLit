// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NNEDmlCommon.h"
#include "NNERuntimeRDGModel.h"

#define NNE_USE_D3D12_RESOURCES

struct ID3D12DynamicRHI;

namespace UE::NNERuntimeRDG::Private::Dml
{

class FModelInfo
{
public:

	static FModelInfo* Get();

	FGuid GetGuid();
	int32 GetVersion();

	int32 GetGuidSize();
	int32 GetVersionSize();

	bool ValidateGuidAndVersion(const uint8* InGuid, const uint8* InVersion);

private:

	FModelInfo();

	FGuid	Guid;
	int32	Version;
};

#ifdef NNE_USE_DIRECTML

class FDmlDeviceContext;
class FOperatorDml;

//
//
//
class FModel : public FModelRDG
{
	class FGraphBuilder;
	class FBindingTable;
	class FDebugName;

public:

	FModel();
	~FModel();

	bool Init(TConstArrayView<uint8> ModelData, FDmlDeviceContext* InDevCtx);

protected:

	virtual void AddDispatchOps_RenderThread(FRDGBuilder& GraphBuilder) override;
	virtual int PrepareTensorShapesAndData() override;

private:

	bool InitCompiledOp(TConstArrayView<int32> OpInputIndices, uint64 TensorDataSize);

	FOperatorDml* OpCreate(const FString& Name, TArrayView<const NNECore::Internal::FTensor> InputTensorDesc, TArrayView<const NNECore::Internal::FTensor> OutputTensorDescs, const NNECore::FAttributeMap& Attributes);

	FBufferRHIRef CreateRHIBuffer(FRHICommandListImmediate& RHICmdList, uint32 Size, EBufferUsageFlags Usage, ERHIAccess Access, const TCHAR* DbgName);
	ID3D12Resource* CreateD3D12Buffer(uint32 Size, D3D12_RESOURCE_STATES ResourceState = D3D12_RESOURCE_STATE_COMMON, D3D12_HEAP_TYPE HeapType = D3D12_HEAP_TYPE_DEFAULT, const TCHAR* DebugName = nullptr);

	//Note: This should go into RDG
	static constexpr int32 MaxNumInputs = 512;
	static constexpr int32 MaxNumOutputs = 4;

	using FRHIBufferInputArray = TArray<FRHIBuffer*, TInlineAllocator<MaxNumInputs>>;
	using FRHIBufferOutputArray = TArray<FRHIBuffer*, TInlineAllocator<MaxNumOutputs>>;

	TComPtr<IDMLOperatorInitializer>	OpInit;
	TComPtr<IDMLCompiledOperator>		CompiledOp;
	FDmlDeviceContext*					DevCtx;
	TUniquePtr<FBindingTable>			BindingTable;
	TComPtr<ID3D12DescriptorHeap>		DescHeap;
	uint32								DescCount;
	uint32								DescSize;

	TArray<int32>						ConstantCPUTensorIndices;

#ifdef NNE_USE_D3D12_RESOURCES
	TComPtr<ID3D12Resource>				PersistBuff;
	TComPtr<ID3D12Resource>				TempBuff;
#else
	FBufferRHIRef						PersistBuff;
#endif
	uint64								MemSizeTemp;
	uint64								MemSizePersist;
	ID3D12DynamicRHI*					DynamicRHI;
};

#endif // NNE_USE_DIRECTML

} // namespace UE::NNERuntimeRDG::Private::Dml
