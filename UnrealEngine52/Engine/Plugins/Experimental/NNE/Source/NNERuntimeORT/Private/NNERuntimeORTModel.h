// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NNECoreModelBase.h"
#include "NNECoreRuntimeGPU.h"
#include "NNECoreTensor.h"
#include "NNECoreTypes.h"
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

	class FModelORT : public NNECore::Internal::FModelBase<NNECore::IModelGPU>
	{

	public:
		FModelORT();
		FModelORT(Ort::Env* InORTEnvironment, const FRuntimeConf& InRuntimeConf);
		virtual ~FModelORT() = default;

		virtual int SetInputTensorShapes(TConstArrayView<NNECore::FTensorShape> InInputShapes) override;
		virtual int RunSync(TConstArrayView<NNECore::FTensorBindingGPU> InInputBindings, TConstArrayView<NNECore::FTensorBindingGPU> InOutputBindings) override;

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

		TArray<const char*> InputTensorNames;
		TArray<const char*> OutputTensorNames;

		TArray<NNECore::Internal::FTensor> InputTensors;
		TArray<NNECore::Internal::FTensor> OutputTensors;
	
		virtual bool InitializedAndConfigureMembers();
		bool ConfigureTensors(const bool InIsInput);

		/**
		 * Statistics-related members used for GetLastRunTimeMSec(), GetRunStatistics(), GetInputMemoryTransferStats(), ResetStats().
		 */
		UE::NNEProfiling::Internal::FStatisticsEstimator RunStatisticsEstimator;
		UE::NNEProfiling::Internal::FStatisticsEstimator InputTransferStatisticsEstimator;

	};

#if PLATFORM_WINDOWS
	class FModelORTDml : public FModelORT
	{
	public:
		FModelORTDml() {}
		FModelORTDml(Ort::Env* InORTEnvironment, const FRuntimeConf& InRuntimeConf) : FModelORT(InORTEnvironment, InRuntimeConf) {}
		virtual ~FModelORTDml() = default;
	private:
		virtual bool InitializedAndConfigureMembers();
	};
#endif
	
} // UE::NNERuntimeORT::Private