// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NNEModelBase.h"
#include "NNERuntimeGPU.h"
#include "NNETensor.h"
#include "NNETypes.h"
#include "NNEProfilingStatistics.h"

#include "NNEThirdPartyWarningDisabler.h"
NNE_THIRD_PARTY_INCLUDES_START
#undef check
#undef TEXT
#include "core/session/onnxruntime_cxx_api.h"
NNE_THIRD_PARTY_INCLUDES_END

namespace UE::NNERuntimeORT::Private
{
	struct FRuntimeConf
	{
		uint32 NumberOfThreads = 2;
		GraphOptimizationLevel OptimizationLevel = GraphOptimizationLevel::ORT_ENABLE_ALL;
		EThreadPriority ThreadPriority = EThreadPriority::TPri_Normal;
	};

	class FModelInstanceORT : public NNE::Internal::FModelInstanceBase<NNE::IModelInstanceGPU>
	{

	public:
		FModelInstanceORT();
		FModelInstanceORT(Ort::Env* InORTEnvironment, const FRuntimeConf& InRuntimeConf);
		virtual ~FModelInstanceORT() = default;

		virtual int SetInputTensorShapes(TConstArrayView<NNE::FTensorShape> InInputShapes) override;
		virtual int RunSync(TConstArrayView<NNE::FTensorBindingGPU> InInputBindings, TConstArrayView<NNE::FTensorBindingGPU> InOutputBindings) override;

		bool Init(TConstArrayView<uint8> ModelData);
		bool IsLoaded() const;
		float GetLastRunTimeMSec() const;
		NNEProfiling::Internal::FStatistics GetRunStatistics() const;
		NNEProfiling::Internal::FStatistics GetInputMemoryTransferStats() const;
		void ResetStats();

	protected:

		bool bIsLoaded = false;
		bool bHasRun = false;

		FRuntimeConf RuntimeConf;

		/** ORT-related variables */
		Ort::Env* ORTEnvironment;
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

		/**
		 * Statistics-related members used for GetLastRunTimeMSec(), GetRunStatistics(), GetInputMemoryTransferStats(), ResetStats().
		 */
		UE::NNEProfiling::Internal::FStatisticsEstimator RunStatisticsEstimator;
		UE::NNEProfiling::Internal::FStatisticsEstimator InputTransferStatisticsEstimator;

	};

#if PLATFORM_WINDOWS
	class FModelInstanceORTDml : public FModelInstanceORT
	{
	public:
		FModelInstanceORTDml() {}
		FModelInstanceORTDml(Ort::Env* InORTEnvironment, const FRuntimeConf& InRuntimeConf) : FModelInstanceORT(InORTEnvironment, InRuntimeConf) {}
		virtual ~FModelInstanceORTDml() = default;
	private:
		virtual bool InitializedAndConfigureMembers() override;
	};

	class FModelInstanceORTCuda : public FModelInstanceORT
	{
	public:
		FModelInstanceORTCuda() {}
		FModelInstanceORTCuda(Ort::Env* InORTEnvironment, const FRuntimeConf& InRuntimeConf) : FModelInstanceORT(InORTEnvironment, InRuntimeConf) {}
		virtual ~FModelInstanceORTCuda() = default;
	private:
		virtual bool InitializedAndConfigureMembers() override;
	};

	class FModelORTDml : public NNE::IModelGPU
	{
	public:
		FModelORTDml(Ort::Env* InORTEnvironment, TConstArrayView<uint8> ModelData);
		virtual ~FModelORTDml() {};

		virtual TUniquePtr<UE::NNE::IModelInstanceGPU> CreateModelInstance() override;

	private:
		Ort::Env* ORTEnvironment;
		TArray<uint8> ModelData;
	};

	class FModelORTCuda : public NNE::IModelGPU
	{
	public:
		FModelORTCuda(Ort::Env* InORTEnvironment, TConstArrayView<uint8> ModelData);
		virtual ~FModelORTCuda() {};

		virtual TUniquePtr<UE::NNE::IModelInstanceGPU> CreateModelInstance() override;

	private:
		Ort::Env* ORTEnvironment;
		TArray<uint8> ModelData;
	};
#endif
	
} // UE::NNERuntimeORT::Private