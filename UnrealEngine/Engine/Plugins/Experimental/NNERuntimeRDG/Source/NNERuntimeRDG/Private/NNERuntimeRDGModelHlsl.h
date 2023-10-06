// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NNERuntimeRDGModel.h"
#include "NNETypes.h"

class FRDGBuilder;

namespace UE::NNERuntimeRDG::Private::Hlsl
{
struct FOperatorHlsl;

class FModelInstance : public FModelInstanceRDG
{
	
public:

	~FModelInstance() = default;

	bool Init(TConstArrayView<uint8> ModelData);

protected:

	virtual int PrepareTensorShapesAndData() override;
	virtual bool PrepareModelRDG(FRDGBuilder& RDGBuilder) override;
	virtual void AddDispatchOps_RenderThread(FRDGBuilder& GraphBuilder) override;

	bool PrepareWeights();

private:

	TArray<FOperatorHlsl*>	Operators;
	TArray<TRefCountPtr<FRDGPooledBuffer>> WeightsExternalRDGResources;
	TArray<TRefCountPtr<FRDGPooledBuffer>> ConstantsExternalRDGResources;
};

class FModel : public NNE::IModelRDG
{
public:
	FModel(TConstArrayView<uint8> ModelData);
	virtual ~FModel() {};

	virtual TUniquePtr<UE::NNE::IModelInstanceRDG> CreateModelInstance() override;

private:
	TArray<uint8> ModelData;
};

} // namespace UE::NNERuntimeRDG::Private::Hlsl