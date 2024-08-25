// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NNEDmlCommon.h"
#include "NNEModelData.h"
#include "NNERuntimeRDGModel.h"

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

class FModelInstance : public FModelInstanceRDG
{
	class FGraphBuilder;
	class FBindingTable;
	
	friend class FGraphBuilder;

	// Utility class to hold debug names used by D3D
	class FDebugName
	{
		static constexpr int32 Size = 128;

	public:

		FDebugName();
		FDebugName(const FString& InStr);
		FDebugName(FStringView InStr);

		const char* Get() const;

	private:

		char	Str[Size];
		int32	Length;
	};

	// Utility class to help with deferred DML graph building
	struct FGraphOpDesc
	{
		int32			OpIndex;
		int32			InputStart;
		int32			InputCount;
		int32			OutputStart;
		int32			OutputCount;
		FDebugName		DbgName;
	};

public:

	FModelInstance();
	~FModelInstance();

	bool Init(TConstArrayView<uint8> ModelData, FDmlDeviceContext* InDevCtx);

protected:

	virtual void AddDispatchOps_RenderThread(FRDGBuilder& GraphBuilder) override;
	virtual int PrepareTensorShapesAndData() override;

private:

	FOperatorDml* OpCreate(const FOperatorDesc& OpDesc, TConstArrayView<NNE::FTensorDesc> Inputs, TConstArrayView<NNE::FTensorDesc> Outputs, const NNE::FAttributeMap& Attributes);

	bool InitCompiledOp();

	FBufferRHIRef CreateRHIBuffer(FRHICommandListImmediate& RHICmdList, uint32 Size, EBufferUsageFlags Usage, ERHIAccess Access, const TCHAR* DbgName);

	static constexpr int32 MaxNumInputs = 512;
	static constexpr int32 MaxNumOutputs = 4;

	using FRHIBufferInputArray = TArray<FRHIBuffer*, TInlineAllocator<MaxNumInputs>>;
	using FRHIBufferOutputArray = TArray<FRHIBuffer*, TInlineAllocator<MaxNumOutputs>>;

	FDmlDeviceContext*					DevCtx;
	TArray<FOperatorDml*>				Operators;
	TArray<FGraphOpDesc>				GraphOperators;
	TArray<int32>						GraphOpInputIndices;
	TArray<int32>						GraphOpOutputIndices;

	TComPtr<IDMLOperatorInitializer>	OpInit;
	TComPtr<IDMLCompiledOperator>		CompiledOp;
	
	TArray<int32>						ConstantCPUTensorIndices;

	ID3D12DynamicRHI*					DynamicRHI;
	TUniquePtr<FBindingTable>			BindingTable;
	TComPtr<ID3D12DescriptorHeap>		DescHeap;
	uint32								DescCount;
	uint32								DescSize;

	FBufferRHIRef						PersistBuff;
	uint64								MemSizeWeights;
	uint64								MemSizeTemp;
	uint64								MemSizePersist;
};

class FModel : public NNE::IModelRDG
{
public:
	FModel(const TSharedPtr<NNE::FSharedModelData>& InModelData, FDmlDeviceContext* InDevCtx);
	virtual ~FModel() {};

	virtual TSharedPtr<NNE::IModelInstanceRDG> CreateModelInstanceRDG() override;

private:
	TSharedPtr<NNE::FSharedModelData> ModelData;
	FDmlDeviceContext* DevCtx;
};

#endif // NNE_USE_DIRECTML

} // namespace UE::NNERuntimeRDG::Private::Dml
