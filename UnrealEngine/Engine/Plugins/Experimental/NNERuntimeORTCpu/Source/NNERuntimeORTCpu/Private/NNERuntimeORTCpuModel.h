// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NNEModelBase.h"
#include "NNERuntimeCPU.h"
#include "NNETensor.h"
#include "NNETypes.h"
#include "NNEProfilingStatistics.h"

#include "NNEThirdPartyWarningDisabler.h"
NNE_THIRD_PARTY_INCLUDES_START
#undef check
#undef TEXT
#include "core/session/onnxruntime_cxx_api.h"
NNE_THIRD_PARTY_INCLUDES_END

namespace UE::NNERuntimeORTCpu::Private
{
	struct FRuntimeConf
	{
		uint32 NumberOfThreads = 2;
		GraphOptimizationLevel OptimizationLevel = GraphOptimizationLevel::ORT_ENABLE_ALL;
		EThreadPriority ThreadPriority = EThreadPriority::TPri_Normal;
	};

	class FModelInstanceCPU : public NNE::Internal::FModelInstanceBase<NNE::IModelInstanceCPU>
	{

	public:
		FModelInstanceCPU();
		FModelInstanceCPU(Ort::Env* InORTEnvironment, const FRuntimeConf& InRuntimeConf);
		virtual ~FModelInstanceCPU() {};

		virtual int SetInputTensorShapes(TConstArrayView<NNE::FTensorShape> InInputShapes) override;
		virtual int RunSync(TConstArrayView<NNE::FTensorBindingCPU> InInputBindings, TConstArrayView<NNE::FTensorBindingCPU> InOutputBindings) override;

		bool Init(TConstArrayView<uint8> ModelData);
		bool IsLoaded() const;
		float GetLastRunTimeMSec() const;
		NNEProfiling::Internal::FStatistics GetRunStatistics() const;
		NNEProfiling::Internal::FStatistics GetInputMemoryTransferStats() const;
		void ResetStats();

	private:

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

		TArray<Ort::AllocatedStringPtr> InputTensorNames;
		TArray<Ort::AllocatedStringPtr> OutputTensorNames;
		TArray<const char*> InputTensorNamePtrs;
		TArray<const char*> OutputTensorNamePtrs;

		TArray<NNE::Internal::FTensor> InputTensors;
		TArray<NNE::Internal::FTensor> OutputTensors;

		bool InitializedAndConfigureMembers();
		bool ConfigureTensors(const bool InIsInput);

		/**
		 * Statistics-related members used for GetLastRunTimeMSec(), GetRunStatistics(), GetInputMemoryTransferStats(), ResetStats().
		 */
		UE::NNEProfiling::Internal::FStatisticsEstimator RunStatisticsEstimator;
		UE::NNEProfiling::Internal::FStatisticsEstimator InputTransferStatisticsEstimator;

	};

	class FModelCPU : public NNE::IModelCPU
	{
	public:
		FModelCPU(Ort::Env* InORTEnvironment, TConstArrayView<uint8> ModelData);
		virtual ~FModelCPU() {};

		virtual TUniquePtr<UE::NNE::IModelInstanceCPU> CreateModelInstance() override;

	private:
		Ort::Env* ORTEnvironment;
		TArray<uint8> ModelData;
	};
	
} // UE::NNERuntimeORTCpu::Private