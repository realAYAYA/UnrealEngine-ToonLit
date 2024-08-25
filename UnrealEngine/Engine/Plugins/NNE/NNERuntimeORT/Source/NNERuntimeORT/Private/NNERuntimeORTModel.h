// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NNEModelBase.h"
#include "NNEModelData.h"
#include "NNERuntimeCPU.h"
#include "NNERuntimeGPU.h"
#include "NNETensor.h"
#include "NNETypes.h"
#include "NNEUtilitiesORTIncludeHelper.h"

namespace UE::NNERuntimeORT::Private
{
	struct FRuntimeConf
	{
		GraphOptimizationLevel OptimizationLevel = GraphOptimizationLevel::ORT_ENABLE_ALL;
	};

	template <class ModelInterface, class TensorBinding>
	class FModelInstanceORTBase : public NNE::Internal::FModelInstanceBase<ModelInterface>
	{

	public:
		FModelInstanceORTBase(const FRuntimeConf& InRuntimeConf, TSharedPtr<Ort::Env> InEnvironment);
		virtual ~FModelInstanceORTBase() = default;

		virtual typename ModelInterface::ESetInputTensorShapesStatus SetInputTensorShapes(TConstArrayView<NNE::FTensorShape> InInputShapes) override;

		bool Init(TConstArrayView<uint8> ModelData);

		typename ModelInterface::ERunSyncStatus RunSync(TConstArrayView<TensorBinding> InInputBindings, TConstArrayView<TensorBinding> InOutputBindings) override;

	protected:

		FRuntimeConf RuntimeConf;

		/** ORT-related variables */
		TSharedPtr<Ort::Env> Environment;
		TUniquePtr<Ort::Session> Session;
		TUniquePtr<Ort::AllocatorWithDefaultOptions> Allocator;
		TUniquePtr<Ort::SessionOptions> SessionOptions;
		TUniquePtr<Ort::MemoryInfo> AllocatorInfo;

		/** IO ORT-related variables */
		TArray<ONNXTensorElementDataType> InputTensorsORTType;
		TArray<ONNXTensorElementDataType> OutputTensorsORTType;

		TArray<Ort::AllocatedStringPtr> InputTensorNameValues;
		TArray<Ort::AllocatedStringPtr> OutputTensorNameValues;
		TArray<char*> InputTensorNames;
		TArray<char*> OutputTensorNames;

		TArray<NNE::Internal::FTensor> InputTensors;
		TArray<NNE::Internal::FTensor> OutputTensors;
	
		virtual bool InitializedAndConfigureMembers();
		bool ConfigureTensors(const bool InIsInput);
	};

	class FModelInstanceORTCpu : public FModelInstanceORTBase<NNE::IModelInstanceCPU, NNE::FTensorBindingCPU>
	{
	public:
		FModelInstanceORTCpu(const FRuntimeConf& InRuntimeConf, TSharedPtr<Ort::Env> InEnvironment) : FModelInstanceORTBase(InRuntimeConf, InEnvironment) {}
		virtual ~FModelInstanceORTCpu() = default;

	private:
		virtual bool InitializedAndConfigureMembers() override;
	};

	class FModelORTCpu : public NNE::IModelCPU
	{
	public:
		FModelORTCpu(TSharedPtr<Ort::Env> InEnvironment, const TSharedPtr<UE::NNE::FSharedModelData>& InModelData);
		virtual ~FModelORTCpu() = default;

		virtual TSharedPtr<NNE::IModelInstanceCPU> CreateModelInstanceCPU() override;

	private:
		TSharedPtr<Ort::Env> Environment;
		TSharedPtr<UE::NNE::FSharedModelData> ModelData;
	};

#if PLATFORM_WINDOWS
	class FModelInstanceORTDml : public FModelInstanceORTBase<NNE::IModelInstanceGPU, NNE::FTensorBindingGPU>
	{
	public:
		FModelInstanceORTDml(const FRuntimeConf& InRuntimeConf, TSharedPtr<Ort::Env> InEnvironment) : FModelInstanceORTBase(InRuntimeConf, InEnvironment) {}
		virtual ~FModelInstanceORTDml() = default;

	private:
		virtual bool InitializedAndConfigureMembers() override;
	};

	class FModelORTDml : public NNE::IModelGPU
	{
	public:
		FModelORTDml(TSharedPtr<Ort::Env> InEnvironment, const TSharedPtr<UE::NNE::FSharedModelData>& InModelData);
		virtual ~FModelORTDml() = default;

		virtual TSharedPtr<NNE::IModelInstanceGPU> CreateModelInstanceGPU() override;

	private:
		TSharedPtr<Ort::Env> Environment;
		TSharedPtr<UE::NNE::FSharedModelData> ModelData;
	};
#endif //PLATFORM_WINDOWS
	
} // UE::NNERuntimeORT::Private