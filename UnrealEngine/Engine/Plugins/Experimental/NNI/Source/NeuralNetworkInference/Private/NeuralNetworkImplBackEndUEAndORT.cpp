// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralNetworkImplBackEndUEAndORT.h"
#include "Async/Async.h"
#include "NeuralEnumClasses.h"
#include "NeuralNetworkInferenceUtils.h"
#include "NeuralNetworkInferenceUtilsGPU.h"
#include "RedirectCoutAndCerrToUeLog.h"
#if WITH_EDITOR
#include "Misc/MessageDialog.h"
#endif //WITH_EDITOR

#if defined(WITH_UE_AND_ORT_SUPPORT) && defined(PLATFORM_WIN64)
	#include "HAL/CriticalSection.h"
	#include "ID3D12DynamicRHI.h"
#endif

//#define WITH_NNI_CPU_NOT_RECOMMENDED // Only for debugging purposes

NNI_THIRD_PARTY_INCLUDES_START
#undef check
#undef TEXT
#ifdef WITH_UE_AND_ORT_SUPPORT
	#ifdef PLATFORM_WIN64
	#include "core/providers/dml/dml_provider_factory.h"
	#endif
	#ifdef WITH_NNI_CPU_NOT_RECOMMENDED
	#include "core/providers/nni_cpu/nni_cpu_provider_factory.h"
	#endif //WITH_NNI_CPU_NOT_RECOMMENDED
#endif //WITH_UE_AND_ORT_SUPPORT
NNI_THIRD_PARTY_INCLUDES_END

#include "ShaderParameterUtils.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphUtils.h"

#ifdef WITH_UE_AND_ORT_SUPPORT

#if defined(PLATFORM_WIN64)

#if WITH_EDITOR && defined(PLATFORM_WIN64) && !UE_BUILD_SHIPPING
#include "Windows/AllowWindowsPlatformTypes.h"
NNI_THIRD_PARTY_INCLUDES_START
	#include <pix3.h>
NNI_THIRD_PARTY_INCLUDES_END
#include "Windows/HideWindowsPlatformTypes.h"
	#define NNIGPUProfileMarker(Name) FNNIGPUProfiler::Instance()->Marker(Name)
#else
	#define NNIGPUProfileMarker(Name)
#endif

#endif // PLATFORM_WIN64

// Helper class to utilize the PIX CPU/GPU debugger on Windows
class FNNIGPUProfiler
{
public:
	static FNNIGPUProfiler* Instance()
	{
		static FNNIGPUProfiler Inst;
		return &Inst;
	}

	class FScopedEvent
	{
	public:

		FScopedEvent(const FString& Name, FColor Color = FColor::Yellow)
		{
			FNNIGPUProfiler::Instance()->EventBegin(Name, Color);
		}

		~FScopedEvent()
		{
			FNNIGPUProfiler::Instance()->EventEnd();
		}
	};

private:
	FNNIGPUProfiler()
	{
#if defined(PLATFORM_WIN64) && defined(USE_PIX) && !defined(UE_BUILD_SHIPPING)
		bIsEnabled = GetID3D12DynamicRHI()->RHIIsPixEnabled();
#else
		bIsEnabled = false;
#endif
	}

public:
	~FNNIGPUProfiler()
	{
	}

	void Marker(const FString& Name, FColor Color = FColor::Yellow)
	{
#if defined(PLATFORM_WIN64) && defined(USE_PIX) && !defined(UE_BUILD_SHIPPING)
		if (bIsEnabled)
		{
			PIXSetMarker(PIX_COLOR(Color.R, Color.G, Color.B), Name.GetCharArray().GetData());
		}
#endif
	}

	void EventBegin(const FString& Name, FColor Color = FColor::Yellow)
	{
#if defined(PLATFORM_WIN64) && defined(USE_PIX) && !defined(UE_BUILD_SHIPPING)
		if (bIsEnabled)
		{
			PIXBeginEvent(PIX_COLOR(Color.R, Color.G, Color.B), Name.GetCharArray().GetData());
		}
#endif
	}

	void EventEnd()
	{
#if defined(PLATFORM_WIN64) && defined(USE_PIX) && !defined(UE_BUILD_SHIPPING)
		if (bIsEnabled)
		{
			PIXEndEvent();
		}
#endif
	}

private:
	bool bIsEnabled;
};



#ifdef PLATFORM_WIN64

/* FPrivateImplBackEndUEAndORT auxiliary class
 *****************************************************************************/
class FPrivateImplBackEndUEAndORT
{
public:
	static IDMLDevice* GetDMLDeviceThreadSafe(ID3D12Device* Device);

private:
	/**
	 * Helper class that maintains a list of created DML Devices for given ID3D12Device
	 */
	class FDMLDeviceList
	{
	public:
		IDMLDevice* GetDMLDevice(ID3D12Device* Device);

	private:
		IDMLDevice* Add(ID3D12Device* Device);

		struct DMLDeviceEntry
		{
			ID3D12Device* Device;
			IDMLDevice* DmlDevice;
		};

		TArray<DMLDeviceEntry> Entries;
	};
};

IDMLDevice* FPrivateImplBackEndUEAndORT::GetDMLDeviceThreadSafe(ID3D12Device* Device)
{
	static FCriticalSection CriticalSection; /* Protects GetDMLDeviceThreadSafe from being called simultaneously from multiple threads. */
	static FDMLDeviceList DMLDeviceList;
	FScopeLock Lock(&CriticalSection);
	return DMLDeviceList.GetDMLDevice(Device);
}

IDMLDevice* FPrivateImplBackEndUEAndORT::FDMLDeviceList::GetDMLDevice(ID3D12Device* Device)
{
	for (size_t c = 0; c < Entries.Num(); ++c)
	{
		if (Entries[c].Device == Device)
		{
			return Entries[c].DmlDevice;
		}
	}

	return Add(Device);
}

IDMLDevice* FPrivateImplBackEndUEAndORT::FDMLDeviceList::Add(ID3D12Device* Device)
{
	// Create new DML Device
	IDMLDevice* DmlDevice = nullptr;

	DML_CREATE_DEVICE_FLAGS DmlCreateFlags = DML_CREATE_DEVICE_FLAG_NONE;

#if !UE_BUILD_SHIPPING
	if (ID3D12DynamicRHI::IsD3DDebugEnabled()
		|| FParse::Param(FCommandLine::Get(), TEXT("d3d12gpuvalidation")) || FParse::Param(FCommandLine::Get(), TEXT("gpuvalidation")))
	{
		DmlCreateFlags |= DML_CREATE_DEVICE_FLAG_DEBUG;
	}
#endif

	HRESULT HResult = DMLCreateDevice1(Device, DmlCreateFlags, DML_FEATURE_LEVEL_2_0, IID_PPV_ARGS(&DmlDevice));

	// Handle the case if Graphics Debug Tools are not installed
	if (HResult == DXGI_ERROR_SDK_COMPONENT_MISSING)
	{
		DmlCreateFlags &= ~DML_CREATE_DEVICE_FLAG_DEBUG;

		HResult = DMLCreateDevice1(Device, DmlCreateFlags, DML_FEATURE_LEVEL_2_0, IID_PPV_ARGS(&DmlDevice));
	}

	if (!DmlDevice)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FDMLDeviceList::Add(): Failed to create DML device, HResult=%0x."), HResult);
		return nullptr;
	}

	Entries.Push(DMLDeviceEntry{ Device, DmlDevice });

	return DmlDevice;
}

#endif // PLATFORM_WIN64
#endif // WITH_UE_AND_ORT_SUPPORT



/* FNeuralNetworkAsyncTask public functions
 *****************************************************************************/

#ifdef WITH_UE_AND_ORT_SUPPORT
UNeuralNetwork::FImplBackEndUEAndORT::FNeuralNetworkAsyncTask::FNeuralNetworkAsyncTask(UNeuralNetwork::FImplBackEndUEAndORT* InBackEnd)
	: BackEnd(InBackEnd)
{}

void UNeuralNetwork::FImplBackEndUEAndORT::FNeuralNetworkAsyncTask::SetRunSessionArgs(const ENeuralSynchronousMode InSyncMode, const ENeuralDeviceType InDeviceType,
	const ENeuralDeviceType InInputDeviceType)
{
	const FScopeLock ResourcesLock(&BackEnd->ResoucesCriticalSection);
	SyncMode = InSyncMode;
	DeviceType = InDeviceType;
	InputDeviceType = InInputDeviceType;
}

void UNeuralNetwork::FImplBackEndUEAndORT::FNeuralNetworkAsyncTask::DoWork()
{
	const FRedirectCoutAndCerrToUeLog RedirectCoutAndCerrToUeLog;
	if (SyncMode == ENeuralSynchronousMode::Asynchronous)
	{
		BackEnd->RunSessionAsync(DeviceType, InputDeviceType);
	}
	else
	{
		BackEnd->RunSessionSync(DeviceType, InputDeviceType);
	}
}
#endif //WITH_UE_AND_ORT_SUPPORT



/* FImplBackEndUEAndORT 'structors
 *****************************************************************************/

UNeuralNetwork::FImplBackEndUEAndORT::FImplBackEndUEAndORT(FOnAsyncRunCompleted& InOutOnAsyncRunCompletedDelegate, ENeuralThreadMode& InDelegateThreadMode,
	FCriticalSection& InResoucesCriticalSection)
#ifdef WITH_UE_AND_ORT_SUPPORT
	: OnAsyncRunCompletedDelegate(InOutOnAsyncRunCompletedDelegate)
	, DelegateThreadMode(InDelegateThreadMode)
	, ResoucesCriticalSection(InResoucesCriticalSection)
#endif
{
}

UNeuralNetwork::FImplBackEndUEAndORT::~FImplBackEndUEAndORT()
{
#ifdef WITH_UE_AND_ORT_SUPPORT
	EnsureAsyncTaskCompletion();
#endif //WITH_UE_AND_ORT_SUPPORT
}



/* FImplBackEndUEAndORT public functions
 *****************************************************************************/

bool UNeuralNetwork::FImplBackEndUEAndORT::ForceCPUIfNoGPU(ENeuralDeviceType& InOutDeviceType)
{
	if (InOutDeviceType != ENeuralDeviceType::CPU)
	{
		if (!IsGPUSupported())
		{
			InOutDeviceType = ENeuralDeviceType::CPU;
			return true;
		}
	}
	return false;
}

bool UNeuralNetwork::FImplBackEndUEAndORT::IsGPUSupported()
{
#if defined(WITH_UE_AND_ORT_SUPPORT) && PLATFORM_WINDOWS
	// Return whether it is DX12
	return RHIGetInterfaceType() == ERHIInterfaceType::D3D12;
#else
	// If not Windows and/or if WITH_UE_AND_ORT_SUPPORT not defined, then this should return false because GPU will not work
	return false;
#endif
}

bool UNeuralNetwork::FImplBackEndUEAndORT::Load(TSharedPtr<FImplBackEndUEAndORT>& InOutImplBackEndUEAndORT,
	FOnAsyncRunCompleted& InOutOnAsyncRunCompletedDelegate, ENeuralThreadMode& InOutDelegateThreadMode, FCriticalSection& InOutResoucesCriticalSection,
	TArray<bool>& OutAreInputTensorSizesVariable, const TArray<uint8>& InModelReadFromFileInBytes, const FString& InModelFullFilePath,
	const ENeuralDeviceType InDeviceType, const ENeuralDeviceType InInputDeviceType, const ENeuralDeviceType InOutputDeviceType)
{
#ifdef WITH_UE_AND_ORT_SUPPORT
#if WITH_EDITOR
	try
#endif //WITH_EDITOR
	{
		const FRedirectCoutAndCerrToUeLog RedirectCoutAndCerrToUeLog;

		// Avoid multi-threaded crashes
		if (InOutImplBackEndUEAndORT.IsValid())
		{
			InOutImplBackEndUEAndORT->EnsureAsyncTaskCompletion();
		}

		// Initialize and configure InOutImplBackEndUEAndORT
		if (!UNeuralNetwork::FImplBackEndUEAndORT::InitializedAndConfigureMembers(InOutImplBackEndUEAndORT, InOutOnAsyncRunCompletedDelegate, InOutDelegateThreadMode,
			InOutResoucesCriticalSection, InModelFullFilePath, InDeviceType))
		{
			UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FImplBackEndUEAndORT::Load(): InitializedAndConfigureMembers failed."));
			return false;
		}

		// Create session from model saved in InModelReadFromFileInBytes (if not empty)
		if (InModelReadFromFileInBytes.Num() > 0)
		{
			// Read model from ModelReadFromFileInBytesVector
			InOutImplBackEndUEAndORT->Session = MakeUnique<Ort::Session>(*InOutImplBackEndUEAndORT->Environment, InModelReadFromFileInBytes.GetData(),
				InModelReadFromFileInBytes.Num(), *InOutImplBackEndUEAndORT->SessionOptions);

#ifdef PLATFORM_WIN64
			InOutImplBackEndUEAndORT->DmlGPUMemoryInfo = MakeUnique<Ort::MemoryInfo>(/*onnxruntime::DML*/ "DML", OrtAllocatorType::OrtDeviceAllocator, /*deviceId*/ 0, OrtMemType::OrtMemTypeDefault);
#endif
		}
		// Else
		else
		{
			UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FImplBackEndUEAndORT::Load(): InModelReadFromFileInBytes was empty."));
			return false;
		}
		
		// Sanity check if device type is CPU and to make sure that input and/or output is also on the CPU
		ENeuralDeviceType InputDeviceType = InInputDeviceType;
		ENeuralDeviceType OutputDeviceType = InOutputDeviceType;
		if (InDeviceType == ENeuralDeviceType::CPU && (InInputDeviceType == ENeuralDeviceType::GPU || InOutputDeviceType == ENeuralDeviceType::GPU))
		{
			UE_LOG(LogNeuralNetworkInference, Warning,
				TEXT("FImplBackEndUEAndORT::Load(): DeviceType is CPU but Input and/or Output is set to GPU, setting all to CPU."));
			InputDeviceType = ENeuralDeviceType::CPU;
			OutputDeviceType = ENeuralDeviceType::CPU;
		}

		if (!InOutImplBackEndUEAndORT->ConfigureTensors(InOutImplBackEndUEAndORT->InputTensors, &OutAreInputTensorSizesVariable, InDeviceType,
			InputDeviceType, OutputDeviceType))
		{
			UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FImplBackEndUEAndORT::Load(): Failed to configure input tensors."));
			return false;
		}

		if (!InOutImplBackEndUEAndORT->ConfigureTensors(InOutImplBackEndUEAndORT->OutputTensors, nullptr, InDeviceType, InputDeviceType, OutputDeviceType))
		{
			UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FImplBackEndUEAndORT::Load(): Failed to configure output tensors."));
			return false;
		}

		// Initializing AsyncTask
		InOutImplBackEndUEAndORT->NeuralNetworkAsyncTask = MakeUnique<FAsyncTask<FNeuralNetworkAsyncTask>>(InOutImplBackEndUEAndORT.Get());
		
		return true;
	}
#if WITH_EDITOR
	catch (const std::exception& Exception)
	{
		UE_LOG(LogNeuralNetworkInference, Error, TEXT("%s"), UTF8_TO_TCHAR(Exception.what()));
		return false;
	}
#endif //WITH_EDITOR

#else //WITH_UE_AND_ORT_SUPPORT
	UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FImplBackEndUEAndORT::Load(): Platform or Operating System not supported yet for UEAndORT"
		" BackEnd. Set BackEnd to ENeuralBackEnd::Auto (recommended) or ENeuralBackEnd::UEOnly for this platform."));
	return false;
#endif //WITH_UE_AND_ORT_SUPPORT
}

void UNeuralNetwork::FImplBackEndUEAndORT::Run(const ENeuralSynchronousMode InSynchronousMode, const ENeuralDeviceType InDeviceType,
	const ENeuralDeviceType InInputDeviceType)
{
#ifdef WITH_UE_AND_ORT_SUPPORT
	if (! bHasRun)
	{
		WarnOnCPUForced(true);
		bHasRun = true;
	}
#if WITH_EDITOR
	try
#endif //WITH_EDITOR
	{
		EnsureAsyncTaskCompletion();
		NeuralNetworkAsyncTask->GetTask().SetRunSessionArgs(InSynchronousMode, InDeviceType, InInputDeviceType);

		// Run UNeuralNetwork
		if (InSynchronousMode == ENeuralSynchronousMode::Synchronous)
		{
			NeuralNetworkAsyncTask->StartSynchronousTask();
		}
		else if (InSynchronousMode == ENeuralSynchronousMode::Asynchronous)
		{
			NeuralNetworkAsyncTask->StartBackgroundTask(); // Alternative: (GThreadPool, EQueuedWorkPriority::Highest);
		}
		else
		{
			UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FImplBackEndUEAndORT::Run(): Unknown SynchronousMode = %d."), (int32)InSynchronousMode);
		}
	}
#if WITH_EDITOR
	catch (const std::exception& Exception)
	{
		UE_LOG(LogNeuralNetworkInference, Error, TEXT("%s"), UTF8_TO_TCHAR(Exception.what()));
	}
#endif //WITH_EDITOR

#else //WITH_UE_AND_ORT_SUPPORT
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FImplBackEndUEAndORT::Run(): Platform or Operating System not suported yet for UEAndORT"
														" BackEnd. Set BackEnd to ENeuralBackEnd::Auto or ENeuralBackEnd::UEOnly for this platform."));
#endif //WITH_UE_AND_ORT_SUPPORT
}



/* FImplBackEndUEAndORT private functions
 *****************************************************************************/

#ifdef WITH_UE_AND_ORT_SUPPORT

void UNeuralNetwork::FImplBackEndUEAndORT::WarnOnCPUForced(bool bInShouldOpenMessageLog)
{
	if (!bIsCPUForced)
	{
		return;
	}

	const FString RHIName = GDynamicRHI->GetName();
#ifdef PLATFORM_WIN64
	const FString ErrorMessage = TEXT("On Windows, only DirectX 12 rendering (\"D3D12\") is compatible with the UEAndORT back end of NeuralNetworkInference (NNI). Instead, \"")
		+ RHIName + TEXT("\" was used. You have the following options:\n\n"
						 "\t1. (Recommended) Switch Unreal Engine to DX12. In order to do that:\n"
						 "\t\t - Go to \"Project Settings\", \"Platforms\", \"Windows\", \"Default RHI\".\n"
						 "\t\t - Select \"DirectX 12\".\n"
						 "\t\t - Restart Unreal Engine.\n"
						 "\t2. Alternatively, switch the network to CPU with UNeuralNetwork::SetDeviceType().\n\n"
						 "Network set to CPU provisionally.");
#else //PLATFORM_WIN64
	const FString ErrorMessage = TEXT("GPU version is not supported for non-Windows platforms yet. Switch the network to CPU with UNeuralNetwork::SetDeviceType() or run from Windows.\n\n"
									  "Network set to CPU provisionally.");
#endif //PLATFORM_WIN64
	UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FImplBackEndUEAndORT::WarnAndSetDeviceToCPUIfDX12NotEnabled(): %s"), *ErrorMessage);
#if WITH_EDITOR
	if (bInShouldOpenMessageLog)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(ErrorMessage));
	}
#endif //WITH_EDITOR
}


void UNeuralNetwork::FImplBackEndUEAndORT::EnsureAsyncTaskCompletion() const
{
	if (NeuralNetworkAsyncTask && !NeuralNetworkAsyncTask->IsDone())
	{
		NeuralNetworkAsyncTask->EnsureCompletion(/*bDoWorkOnThisThreadIfNotStarted*/true);
	}
}

bool UNeuralNetwork::FImplBackEndUEAndORT::InitializedAndConfigureMembers(TSharedPtr<FImplBackEndUEAndORT>& InOutImplBackEndUEAndORT,
	FOnAsyncRunCompleted& InOutOnAsyncRunCompletedDelegate, ENeuralThreadMode& InOutDelegateThreadMode, FCriticalSection& InOutResoucesCriticalSection,
	const FString& InModelFullFilePath, const ENeuralDeviceType InDeviceType)
{
	// Initialize InOutImplBackEndUEAndORT
	if (!InOutImplBackEndUEAndORT.IsValid())
	{
		InOutImplBackEndUEAndORT = MakeShared<FImplBackEndUEAndORT>(InOutOnAsyncRunCompletedDelegate, InOutDelegateThreadMode, InOutResoucesCriticalSection);

		// Set up ORT and create an environment
		const FString LoggingFullFilePath = InModelFullFilePath + TEXT(".txt");
		const auto LoggingFullFilePathAnsiString = StringCast<ANSICHAR>(*LoggingFullFilePath);
		InOutImplBackEndUEAndORT->Environment = MakeUnique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, LoggingFullFilePathAnsiString.Get());

		InOutImplBackEndUEAndORT->Allocator = MakeUnique<Ort::AllocatorWithDefaultOptions>();

		InOutImplBackEndUEAndORT->AllocatorInfo = MakeUnique<Ort::MemoryInfo>(Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU));
	}

	InOutImplBackEndUEAndORT->ClearResources();

	// Configure InOutImplBackEndUEAndORT
	if (!InOutImplBackEndUEAndORT->ConfigureMembers(InDeviceType))
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FImplBackEndUEAndORT::InitializedAndConfigureMembers(): ConfigureMembers failed."));
		return false;
	}

	return true;
}

bool UNeuralNetwork::FImplBackEndUEAndORT::ConfigureMembers(const ENeuralDeviceType InDeviceType)
{
	// Configure Session
	SessionOptions = MakeUnique<Ort::SessionOptions>();

	// Configure number threads
	SessionOptions->SetIntraOpNumThreads(2);
	// Uncomment if you want to change the priority of the threads, by default is TPri_Normal
	SessionOptions->SetPriorityOpThreads(EThreadPriority::TPri_Normal);

	// Configure Provider
	// GPU
	if (InDeviceType == ENeuralDeviceType::GPU)
	{
#ifdef PLATFORM_WIN64
		// To create a DirectML device we need to check that we're using DX12 first
		if (!IsGPUSupported())
		{
			UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FImplBackEndUEAndORT::ConfigureMembers(): UEAndORT back end for GPU needs DX12 enabled."));
			return false;
		}

		// Get adapter's D3D12 device that we would like to share with DirectML execution provider
		ID3D12DynamicRHI* RHI = GetID3D12DynamicRHI();
		const TArray<FD3D12MinimalAdapterDesc> Adapters = RHI->RHIGetAdapterDescs();

		const FD3D12MinimalAdapterDesc&	RHIAdapterDesc = Adapters[0];

		UE_LOG(LogNeuralNetworkInference, Display, TEXT("%d available RHI adapters. NNI using RHI adapter %s with LUID:%0x%0x."),
			Adapters.Num(), RHIAdapterDesc.Desc.Description, RHIAdapterDesc.Desc.AdapterLuid.HighPart, RHIAdapterDesc.Desc.AdapterLuid.LowPart);

		if (Adapters.Num() > 1)
		{
			UE_LOG(LogNeuralNetworkInference, Display, TEXT("All available RHI adapters:"));
			for (int32 AdapterIndex = 0; AdapterIndex < Adapters.Num(); ++AdapterIndex)
			{
				const FD3D12MinimalAdapterDesc& CurrAdapterDesc = Adapters[AdapterIndex];
				UE_LOG(LogNeuralNetworkInference, Display, TEXT("  - Adapter [%d] Name:%s with LUID:%0x%0x."),
					AdapterIndex, CurrAdapterDesc.Desc.Description, CurrAdapterDesc.Desc.AdapterLuid.HighPart, CurrAdapterDesc.Desc.AdapterLuid.LowPart);
			}
		}

		if (Adapters.Num() > 1 || RHIAdapterDesc.NumDeviceNodes > 1)
		{
			UE_LOG(LogNeuralNetworkInference, Warning,
				TEXT("FImplBackEndUEAndORT::ConfigureMembers(): There are multiple (%d) adapters and/or multiple (%d) devices, NNI is currently using only one adapter."),
				Adapters.Num(), RHIAdapterDesc.NumDeviceNodes);
		}

		// Make sure that we have one DMLDevice per D3D12 device
		IDMLDevice* DmlDevice = FPrivateImplBackEndUEAndORT::GetDMLDeviceThreadSafe(RHI->RHIGetDevice(0));

		if (!DmlDevice)
		{
			UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FImplBackEndUEAndORT::ConfigureMembers(): Invalid DML device found."));
			return false;
		}

		// Get a ID3D12CommandQueue as well
		// TODO: Should we create our own queue?
		ID3D12CommandQueue* NativeCmdQ = RHI->RHIGetCommandQueue();

		// ORT GPU (Direct ML)
		SessionOptions->SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL); // ORT_ENABLE_ALL, ORT_ENABLE_EXTENDED, ORT_ENABLE_BASIC, ORT_DISABLE_ALL

		// DML's memory is not byte addressable and hence mem pattern doesn't work.
		SessionOptions->DisableCpuMemArena();

		// Get DML API
		const OrtApi* OrtApi = OrtGetApiBase()->GetApi(ORT_API_VERSION);
		
		OrtApi->GetExecutionProviderApi(/*onnxruntime::DML*/ "DML", ORT_API_VERSION, (const void**) &DmlApi);
		if (!DmlApi)
		{
			UE_LOG(LogNeuralNetworkInference, Warning,
				TEXT("FImplBackEndUEAndORT::ConfigureMembers(): Failed to obtain OrtDmlApi."));
			return false;
		}

		// Set session options
		if (DmlApi->SessionOptionsAppendExecutionProvider_DML1(*SessionOptions.Get(), DmlDevice, NativeCmdQ))
		{
			UE_LOG(LogNeuralNetworkInference, Warning,
				TEXT("FImplBackEndUEAndORT::ConfigureMembers(): Some error occurred when using OrtDmlApi::SessionOptionsAppendExecutionProvider_DML1."));
			return false;
		}
		return true; // @todo: Remove this line when NNI_HLSL is working
#else
		UE_LOG(LogNeuralNetworkInference, Warning,
			TEXT("FImplBackEndUEAndORT::ConfigureMembers(): GPU mode only supported in Windows for now. Please, switch to CPU or to Windows."));

		//SessionOptions->SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_DISABLE_ALL); // ORT_ENABLE_ALL, ORT_ENABLE_EXTENDED, ORT_ENABLE_BASIC, ORT_DISABLE_ALL
		//if (OrtSessionOptionsAppendExecutionProvider_NNI_HLSL(*SessionOptions, 0))
		//{
		//	UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FImplBackEndUEAndORT::ConfigureMembers(): Some error occurred."));
		//	return false;
		//}
#endif //PLATFORM_WIN64
	}
	// CPU
	//else // @todo: Uncomment this line when NNI_HLSL is working
	{
#ifdef WITH_NNI_CPU_NOT_RECOMMENDED
		// NNI CPU (Deprecated)
		SessionOptions->SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_DISABLE_ALL); // ORT_ENABLE_ALL, ORT_ENABLE_EXTENDED, ORT_ENABLE_BASIC, ORT_DISABLE_ALL
		if (OrtSessionOptionsAppendExecutionProvider_NNI_CPU(*SessionOptions))
		{
			UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FImplBackEndUEAndORT::ConfigureMembers(): OrtSessionOptionsAppendExecutionProvider_NNI_CPU failed."));
			return false;
		}
#else
		// ORT CPU
		SessionOptions->SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL); // ORT_ENABLE_ALL, ORT_ENABLE_EXTENDED, ORT_ENABLE_BASIC, ORT_DISABLE_ALL
		SessionOptions->EnableCpuMemArena();
#endif //ORT_CPU
	}

	return true;
}

bool UNeuralNetwork::FImplBackEndUEAndORT::ConfigureTensors(TArray<FNeuralTensor>& OutTensors, TArray<bool>* OutAreInputTensorSizesVariable,
	const ENeuralDeviceType InDeviceType, const ENeuralDeviceType InInputDeviceType, const ENeuralDeviceType InOutputDeviceType)
{
	const bool bIsInput = (OutAreInputTensorSizesVariable != nullptr);
	TArray<const char*> TensorNames;
	TArray<ENeuralDataType> TensorDataTypes;
	TArray<TArray<int64>> TensorSizes;
	TArray<ENeuralTensorType> TensorTypes;

	const uint32 NumberTensors = bIsInput ? Session->GetInputCount() : Session->GetOutputCount();
	if (OutAreInputTensorSizesVariable)
	{
		OutAreInputTensorSizesVariable->SetNum(NumberTensors);
	}
	for (uint32 TensorIndex = 0; TensorIndex < NumberTensors; ++TensorIndex)
	{
		// Get node name
		{
			const char* TensorName = bIsInput ? Session->GetInputName(TensorIndex, *Allocator) : Session->GetOutputName(TensorIndex, *Allocator);
			TensorNames.Emplace(TensorName);
		}

		// Get node type
		Ort::TypeInfo CurrentTypeInfo = bIsInput ? Session->GetInputTypeInfo(TensorIndex) : Session->GetOutputTypeInfo(TensorIndex);

		Ort::TensorTypeAndShapeInfo CurrentTensorInfo = CurrentTypeInfo.GetTensorTypeAndShapeInfo();

		ENeuralDataType TensorDataType;
		{
			const ONNXTensorElementDataType ONNXTensorElementDataTypeEnum = CurrentTensorInfo.GetElementType();
			if (ONNXTensorElementDataTypeEnum == ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT)
			{
				TensorDataType = ENeuralDataType::Float;
			}
			//else if (ONNXTensorElementDataTypeEnum == ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32)
			//{
			//	TensorDataType = ENeuralDataType::Int32;
			//}
			//else if (ONNXTensorElementDataTypeEnum == ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64)
			//{
			//	TensorDataType = ENeuralDataType::Int64;
			//}
			//else if (ONNXTensorElementDataTypeEnum == ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT32)
			//{
			//	TensorDataType = ENeuralDataType::UInt32;
			//}
			//else if (ONNXTensorElementDataTypeEnum == ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT64)
			//{
			//	TensorDataType = ENeuralDataType::UInt64;
			//}
			else
			{
				TensorDataType = ENeuralDataType::None;
				UE_LOG(LogNeuralNetworkInference, Warning,
					TEXT("FImplBackEndUEAndORT::ConfigureTensors(): ONNXTensorElementDataTypeEnum = %d not implemented yet."),
					(int32)ONNXTensorElementDataTypeEnum);
				return false;
			}
		}
		TensorDataTypes.Push(TensorDataType);

		// Get input shapes/dims
		TArray<int64> CurrentTensorSizes;
		{
			for (const int64_t CurrentTensorSize : CurrentTensorInfo.GetShape())
			{
				if (OutAreInputTensorSizesVariable)
				{
					(*OutAreInputTensorSizesVariable)[TensorIndex] |= (CurrentTensorSize < 0);
				}
				// Negative (variable) dimensions not implemented yet
				if (CurrentTensorSize < 0)
				{
					CurrentTensorSizes.Push(1);
					UE_LOG(LogNeuralNetworkInference, Display,
						TEXT("Negative (i.e., variable) dimensions not allowed yet, hard-coded to 1. Let us know if you really need variable dimensions."
							" Keep in mind that fixed sizes might allow additional optimizations and speedup of the network during Run()."));
				}
				else
				{
					CurrentTensorSizes.Push(CurrentTensorSize);
				}
			}
		}
		TensorSizes.Push(CurrentTensorSizes);

		// Set TensorType
		const ENeuralTensorType TensorType = (bIsInput ? ENeuralTensorType::Input : ENeuralTensorType::Output);
		TensorTypes.Push(TensorType);

		CurrentTypeInfo.release();
	}

	return SetTensorsFromNetwork(OutTensors, TensorNames, TensorDataTypes, TensorSizes, TensorTypes, bIsInput, InInputDeviceType, InOutputDeviceType);
}

bool UNeuralNetwork::FImplBackEndUEAndORT::SetTensorsFromNetwork(TArray<FNeuralTensor>& OutTensors, TArray<const char*>& InTensorNames,
	TArray<ENeuralDataType>& InTensorDataTypes, TArray<TArray<int64>>& InSizes, TArray<ENeuralTensorType>& InTensorTypes, const bool bIsInput,
	const ENeuralDeviceType InInputDeviceType, const ENeuralDeviceType InOutputDeviceType)
{
	const int32 TensorNumber = InTensorNames.Num();
	if (InTensorDataTypes.Num() != TensorNumber || InSizes.Num() != TensorNumber)
	{
		UE_LOG(LogNeuralNetworkInference, Warning,
			TEXT("FImplBackEndUEAndORT::SetTensorsFromNetwork(): InTensorNames.Num() == InTensorDataTypes.Num() == InSizes.Num() failed, %d vs. %d vs. %d."),
			InTensorNames.Num(), InTensorDataTypes.Num(), InSizes.Num());
		return false;
	}

	// Swap variables
	TArray<const char*>& TensorNames = (bIsInput ? InputTensorNames : OutputTensorNames);
	Swap(TensorNames, InTensorNames);

	// Note: Switching from/to CPU to/from GPU would cause the FNeuralTensors to be re-initialized. We need to avoid that. For that,
	// we will only re-allocate the tensors...
	// - If bAreTensorsAlreadyCreatedWithRightNames == false, meaning tensors had not been created until now for this network.
	// - And if the existing tensors have the right size, given that SetNumUninitialized() only re-allocates them if their size has changed.

	// Fill bAreTensorsAlreadyCreatedWithRightNames - Check if tensors already created with the right names
	bool bAreTensorsAlreadyCreatedWithRightNames = (OutTensors.Num() == TensorNames.Num());
	if (bAreTensorsAlreadyCreatedWithRightNames)
	{
		for (int32 TensorIndex = 0; TensorIndex < TensorNumber; ++TensorIndex)
		{
			if (OutTensors[TensorIndex].GetName() != ANSI_TO_TCHAR(TensorNames[TensorIndex]))
			{
				bAreTensorsAlreadyCreatedWithRightNames = false;
				break;
			}
		}
	}

	// Assign name to each input/output tensor
	if (!bAreTensorsAlreadyCreatedWithRightNames)
	{
		OutTensors.Empty();
		for (int32 TensorIndex = 0; TensorIndex < TensorNumber; ++TensorIndex)
		{
			const char* TensorName = TensorNames[TensorIndex];
			OutTensors.Emplace(FNeuralTensor(/*NeuralDataType unknown yet*/ ENeuralDataType::None, /*Volume unknown yet*/0, ANSI_TO_TCHAR(TensorName), InTensorTypes[TensorIndex]));
		}
	}
	else
	{
		for (int32 TensorIndex = 0; TensorIndex < TensorNumber; ++TensorIndex)
		{
			OutTensors[TensorIndex].SetTensorType(InTensorTypes[TensorIndex]);
		}
	}

	ensureMsgf(OutTensors.Num() == TensorNumber, TEXT("OutTensors.Num() == TensorNumber failed, %d != %d."), OutTensors.Num(), TensorNumber);

	// Config each TensorIndex
	TArray<Ort::Value>& OrtTensors = (bIsInput ? InputOrtTensors : OutputOrtTensors);
	for (int32 TensorIndex = 0; TensorIndex < TensorNumber; ++TensorIndex)
	{
		if (OrtTensors.Num() <= TensorIndex)
		{
			OrtTensors.Emplace(Ort::Value(nullptr));
		}

#ifdef PLATFORM_WIN64
		if ((bIsInput && InInputDeviceType == ENeuralDeviceType::GPU) || (!bIsInput && InOutputDeviceType == ENeuralDeviceType::GPU))
		{
			// @todo: should we remove this? It's currently used to read memory from GPU to CPU
			OutTensors[TensorIndex].SetNumUninitialized(InTensorDataTypes[TensorIndex], InSizes[TensorIndex]);

			OutTensors[TensorIndex].SetEnableGPU(true);

			// @todo: This requires SetNumUnitialized() to be run, otherwise Size and Volume will be set to 0
			void* D3DResource = nullptr;

			if (!OutTensors[TensorIndex].InitPooledBufferForUEAndORTBackEnd(&D3DResource))
			{
				UE_LOG(LogNeuralNetworkInference, Warning,
					TEXT("FImplBackEndUEAndORT::SetTensorsFromNetwork(): Failed to initialize the pooled buffer."));
				return false;
			}

			// Link tensor with ORT blob
			void* DmlGPUAllocation = LinkTensorResourceToONNXRuntime(DmlApi, DmlGPUMemoryInfo.Get(), OutTensors[TensorIndex], OrtTensors[TensorIndex], D3DResource);
			if  (DmlGPUAllocation == nullptr)
			{
				UE_LOG(LogNeuralNetworkInference, Warning,
					TEXT("FImplBackEndUEAndORT::SetTensorsFromNetwork(): Failed to link the GPU resource to ONNX Runtime."));
				return false;
			}
			DmlGPUResources.Emplace(DmlGPUAllocation);
		}
		else
#endif
		{
			// Pre-allocate TArray (if size is different)
			OutTensors[TensorIndex].SetNumUninitialized(InTensorDataTypes[TensorIndex], InSizes[TensorIndex]);
			// Link tensor with ORT blob
			LinkTensorToONNXRuntime(OutTensors[TensorIndex], OrtTensors[TensorIndex], *AllocatorInfo);
		}
	}

	return true;
}

void UNeuralNetwork::FImplBackEndUEAndORT::LinkTensorToONNXRuntime(FNeuralTensor& InTensor, Ort::Value& OutOrtTensor, Ort::MemoryInfo& InOutAllocatorInfo)
{
	const TArray<int64>& Sizes = InTensor.GetSizes();
	if (Sizes.Num() > 0 && InTensor.Num() > 0)
	{
		const int64 Volume = InTensor.Num();
		const int32 ArrayDimensions = Sizes.Num();

		const ENeuralDataType NeuralDataType = InTensor.GetDataType();
		if (NeuralDataType == ENeuralDataType::Float)
		{
#ifdef _WIN32
			const TArray<int64_t>& SizesInt64t = Sizes;
#else
			checkf(sizeof(int64) == sizeof(int64_t), TEXT("int64 and int64_t should both have the same size."));
			TArray<int64_t> SizesInt64t;
			SizesInt64t.SetNumUninitialized(ArrayDimensions);
			FMemory::Memcpy(SizesInt64t.GetData(), (int64_t*)Sizes.GetData(), sizeof(int64_t) * ArrayDimensions);
#endif //_WIN32
			OutOrtTensor = Ort::Value::CreateTensor<float>(InOutAllocatorInfo, InTensor.GetDataCasted<float>(), Volume, SizesInt64t.GetData(), ArrayDimensions);
		}
		//else if (NeuralDataType == ENeuralDataType::Double)
		//{
		//	OutOrtTensor = Ort::Value::CreateTensor<double>(InOutAllocatorInfo, InTensor.GetDataCasted<double>(), Volume, Sizes.GetData(), ArrayDimensions);
		//}
		else
		{
			UE_LOG(LogNeuralNetworkInference, Warning, TEXT("LinkTensorToONNXRuntime(): Not implemented (yet) for ENeuralDataType = %d."), (int32)NeuralDataType);
		}
	}
}

#ifdef PLATFORM_WIN64
void* UNeuralNetwork::FImplBackEndUEAndORT::LinkTensorResourceToONNXRuntime(OrtDmlApi const* InDmlApi, Ort::MemoryInfo* InMemoryInfo, FNeuralTensor& InOutTensor, Ort::Value& InOutOrtTensor, void* InD3DResource)
{
	if (!InDmlApi)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("LinkTensorResourceToONNXRuntime(): DmlGPUAllocator is not valid."));
		return nullptr;
	}

	void* DmlGPUAllocation = nullptr;
	
	InDmlApi->CreateGPUAllocationFromD3DResource(reinterpret_cast<ID3D12Resource*>(InD3DResource), &DmlGPUAllocation);

	if (!DmlGPUAllocation)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("LinkTensorResourceToONNXRuntime(): DmlGPUAllocation is nullptr."));
		return nullptr;
	}

	const TArray<int64>& Sizes = InOutTensor.GetSizes();
	if (Sizes.Num() > 0 && InOutTensor.Num() > 0)
	{
		const int32 ArrayDimensions = Sizes.Num();

		const ENeuralDataType NeuralDataType = InOutTensor.GetDataType();
		if (NeuralDataType == ENeuralDataType::Float)
		{
#ifdef _WIN32
			const TArray<int64_t>& SizesInt64t = Sizes;
#else
			checkf(sizeof(int64) == sizeof(int64_t), TEXT("int64 and int64_t should both have the same size."));
			TArray<int64_t> SizesInt64t;
			SizesInt64t.SetNumUninitialized(ArrayDimensions);
			FMemory::Memcpy(SizesInt64t.GetData(), (int64_t*)Sizes.GetData(), sizeof(int64_t) * ArrayDimensions);
#endif //_WIN32
			InOutOrtTensor = Ort::Value::CreateTensor(*InMemoryInfo, DmlGPUAllocation, InOutTensor.NumInBytes(), SizesInt64t.GetData(),
				ArrayDimensions, ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT);
		}
		//else if (NeuralDataType == ENeuralDataType::Double)
		//{
		//	InOutOrtTensor = Ort::Value::CreateTensor(DmlGPUAllocator->GetProviderMemoryInfo(), DmlGPUAllocation, InOutTensor.NumInBytes(), SizesInt64t.GetData(),
		//		ArrayDimensions, ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE);
		//}
		else
		{
			UE_LOG(LogNeuralNetworkInference, Warning,
				TEXT("FImplBackEndUEAndORT::LinkTensorToONNXRuntime(): Not implemented (yet) for ENeuralDataType = %d."), (int32)NeuralDataType);
			InDmlApi->FreeGPUAllocation(DmlGPUAllocation);
			return nullptr;
		}
	}

	return DmlGPUAllocation;
}
#endif

void UNeuralNetwork::FImplBackEndUEAndORT::RunSessionAsync(const ENeuralDeviceType InDeviceType, const ENeuralDeviceType InInputDeviceType)
{
	const FScopeLock ResourcesLock(&ResoucesCriticalSection);

	RunSessionImpl(InDeviceType, InInputDeviceType);

	// Execute OnAsyncRunCompletedDelegate on main thread
	if (DelegateThreadMode == ENeuralThreadMode::GameThread)
	{
		if (OnAsyncRunCompletedDelegate.IsBound())
		{
			std::atomic<bool> IsComputeFinished(false);
			AsyncTask(ENamedThreads::GameThread, [this, &IsComputeFinished]
			{
				OnAsyncRunCompletedDelegate.ExecuteIfBound();
				IsComputeFinished = true;
			});
			while (!IsComputeFinished)
			{
				FPlatformProcess::Sleep(0.1e-3);
			}
		}
	}
	else if (DelegateThreadMode == ENeuralThreadMode::AnyThread)
	{
		OnAsyncRunCompletedDelegate.ExecuteIfBound();
	}
	else
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FImplBackEndUEAndORT::RunSessionAsync(): Unknown DelegateThreadMode = %d."), (int32)DelegateThreadMode);
	}
}

void UNeuralNetwork::FImplBackEndUEAndORT::RunSessionSync(const ENeuralDeviceType InDeviceType, const ENeuralDeviceType InInputDeviceType)
{
	RunSessionImpl(InDeviceType, InInputDeviceType);
}

// Used when uploading tensors to GPU
// NOTE: Upload parameter is not yet used, we plan to use it in the future
BEGIN_SHADER_PARAMETER_STRUCT(FUploadTensorParameters, )
RDG_BUFFER_ACCESS(Upload, ERHIAccess::CopySrc)
RDG_BUFFER_ACCESS(Input, ERHIAccess::CopyDest)
END_SHADER_PARAMETER_STRUCT()

void UNeuralNetwork::FImplBackEndUEAndORT::RunSessionImpl(const ENeuralDeviceType InDeviceType, const ENeuralDeviceType InInputDeviceType)
{
	if (InDeviceType == ENeuralDeviceType::GPU)
	{
		// Copy data to GPU (if required)
		bool bNeedsGPUCopy = false;

		for (FNeuralTensor& InputTensor : InputTensors)
		{
			if (InInputDeviceType == ENeuralDeviceType::GPU)
			{
				bNeedsGPUCopy = true;

				ENQUEUE_RENDER_COMMAND(UploadTensorToGPU)([InputTensor](FRHICommandListImmediate& RHICmdList)
					{
						FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("UploadTensorToGPU"));

						// Set parameters
						const TRefCountPtr<FRDGPooledBuffer>& PooledBuffer = InputTensor.GetPooledBuffer();
						FRDGBufferRef InputBufferRef = GraphBuilder.RegisterExternalBuffer(PooledBuffer);

						FUploadTensorParameters* UploadParameters = GraphBuilder.AllocParameters<FUploadTensorParameters>();

						UploadParameters->Input = InputBufferRef;

						GraphBuilder.AddPass(
							RDG_EVENT_NAME("UNeuralNetwork-UploadTensor-%s", *InputTensor.GetName()),
							FUploadTensorParameters::FTypeInfo::GetStructMetadata(),
							UploadParameters,
							ERDGPassFlags::Copy | ERDGPassFlags::NeverCull,
							[UploadParameters](FRHICommandListImmediate& RHICmdList)
							{
								FRHIBuffer* InputBuffer = UploadParameters->Input->GetRHI();

								// NOTE: We're using UAVMask to trigger the UAV barrier in RDG
								RHICmdList.Transition(FRHITransitionInfo(InputBuffer, ERHIAccess::CopyDest, ERHIAccess::UAVMask));
							}
						);

						GraphBuilder.Execute();
					});
			}
		}

		if (bNeedsGPUCopy)
		{
			ENQUEUE_RENDER_COMMAND(FlushUploadTensorToGPU)([this](FRHICommandListImmediate& RHICmdList)
				{
					FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("FImplBackEndUEAndORT-FlushUploadTensorsToGPU"));

					RHICmdList.SubmitCommandsHint();
					RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);

					GraphBuilder.Execute();
				});

			// TODO: Remove this sync point and move session run to render thread
			FNeuralNetworkInferenceUtils::WaitUntilRHIFinished();
		}

		// Profiling init
		FNNIGPUProfiler::Instance()->EventBegin("FImplBackEndUEAndORT-SessionRun");
	}

	// Actual ORT network run
	Session->Run(Ort::RunOptions{ nullptr },
		InputTensorNames.GetData(), &InputOrtTensors[0], InputTensorNames.Num(),
		OutputTensorNames.GetData(), &OutputOrtTensors[0], OutputTensorNames.Num());

	// Profiling end
	if (InDeviceType == ENeuralDeviceType::GPU)
	{
		FNNIGPUProfiler::Instance()->EventEnd();
	}
}

void UNeuralNetwork::FImplBackEndUEAndORT::ClearResources()
{
#ifdef PLATFORM_WIN64
	if (DmlApi)
	{
		const int32 Num = DmlGPUResources.Num();
		for (int32 Index = 0; Index < Num; ++Index)
		{
			DmlApi->FreeGPUAllocation(DmlGPUResources[Index]);
		}

		DmlGPUResources.Reset(0);
		
		for (FInferenceContext& Context : Contexts)
		{
			Context.ReleaseDMLAllocations(DmlApi);
		}
		
		DmlApi = nullptr;
	}
#endif //PLATFORM_WIN64
}

bool UNeuralNetwork::FImplBackEndUEAndORT::FInferenceContext::Init(
	Ort::Session* InSession, 
	Ort::AllocatorWithDefaultOptions* InAllocator, 
	Ort::MemoryInfo* InAllocatorInfo,
	ENeuralDeviceType InInputDeviceType, 
	ENeuralDeviceType InOutputDeviceType)
{
	// Current assumptions here (all these things can be fixed with more work): 
	// Float data types only
	// No variable dimensions

	ENeuralTensorType TensorTypes[] = { ENeuralTensorType::Input, ENeuralTensorType::Output };

	for (ENeuralTensorType TensorType : TensorTypes)
	{
		const bool bInputTensors = TensorType == ENeuralTensorType::Input;
		const uint32 NumTensors = bInputTensors ? InSession->GetInputCount() : InSession->GetOutputCount();

		for (uint32 TensorIndex = 0; TensorIndex < NumTensors; ++TensorIndex)
		{
			const char* TensorName = bInputTensors ? InSession->GetInputName(TensorIndex, *InAllocator) : InSession->GetOutputName(TensorIndex, *InAllocator);
			Ort::TypeInfo TypeInfo = bInputTensors ? InSession->GetInputTypeInfo(TensorIndex) : InSession->GetOutputTypeInfo(TensorIndex);
			const Ort::TensorTypeAndShapeInfo TensorInfo = TypeInfo.GetTensorTypeAndShapeInfo();

			ENeuralDataType TensorDataType = ENeuralDataType::None;
			{
				const ONNXTensorElementDataType ONNXDataType = TensorInfo.GetElementType();
				if (ONNXDataType == ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT)
				{
					TensorDataType = ENeuralDataType::Float;
				}
				else
				{
					UE_LOG(LogNeuralNetworkInference, Warning,
						TEXT("FInferenceContext::Init(): ONNXDataType = %d not implemented yet."), (int32)ONNXDataType);
				}
			}

			TArray<int64> TensorSizes;
			for (int64 TensorSize : TensorInfo.GetShape())
			{
				if (TensorSize < 0)
				{
					TensorSize = 1;
					UE_LOG(LogNeuralNetworkInference, Warning,
						TEXT("FInferenceContext::Init(): Negative (i.e., variable) dimensions not allowed yet, hard-coded to 1."
							" Let us know if you really need variable dimensions."
							" Keep in mind that fixed sizes might allow additional optimizations and speedup of the network during Run()."));
				}

				TensorSizes.Add(TensorSize);
			}

			if (bInputTensors)
			{
				InputTensorNames.Emplace(TensorName);
				InputTensors.Emplace(FNeuralTensor(TensorDataType, TensorSizes, ANSI_TO_TCHAR(TensorName), TensorType));
				InputOrtTensors.Emplace(Ort::Value(nullptr));
				
				if (InInputDeviceType == ENeuralDeviceType::CPU)
				{
					LinkTensorToONNXRuntime(InputTensors[TensorIndex], InputOrtTensors[TensorIndex], *InAllocatorInfo);
				}
				else
				{
					InputTensors[TensorIndex].SetEnableGPU(true);
				}
			}
			else
			{
				OutputTensorNames.Emplace(TensorName);
				OutputTensors.Emplace(FNeuralTensor(TensorDataType, TensorSizes, ANSI_TO_TCHAR(TensorName), TensorType));
				OutputOrtTensors.Emplace(Ort::Value(nullptr));

				if (InOutputDeviceType == ENeuralDeviceType::CPU)
				{
					LinkTensorToONNXRuntime(OutputTensors[TensorIndex], OutputOrtTensors[TensorIndex], *InAllocatorInfo);
				}
				else
				{
					OutputTensors[TensorIndex].SetEnableGPU(true);
				}
			}

			TypeInfo.release();
		}
	}

	return true;
}

void UNeuralNetwork::FImplBackEndUEAndORT::FInferenceContext::UpdateGPUAllocations(FRDGBuilder& GraphBuilder)
{
	for (int32 TensorIndex = 0; TensorIndex < OutputOrtTensors.Num(); ++TensorIndex)
	{
		OutputTensors[TensorIndex].InitPooledBufferForUEAndORTBackEnd_RenderThread(GraphBuilder);
	}
}

void UNeuralNetwork::FImplBackEndUEAndORT::FInferenceContext::Release()
{
	InputTensorNames.Reset();
	InputTensors.Reset();
	InputOrtTensors.Reset();
	OutputTensorNames.Reset();
	OutputTensors.Reset();
	OutputOrtTensors.Reset();
}

#ifdef PLATFORM_WIN64

void UNeuralNetwork::FImplBackEndUEAndORT::FInferenceContext::BindDMLAllocations(OrtDmlApi const* InDmlApi, Ort::MemoryInfo* InMemoryInfo)
{
	ReleaseDMLAllocations(InDmlApi);
	
	OutputDmlAllocation.SetNum(OutputOrtTensors.Num());
	for (int32 TensorIndex = 0; TensorIndex < OutputOrtTensors.Num(); ++TensorIndex)
	{
		void* D3DResource = FNeuralNetworkInferenceUtilsGPU::GetD3D12Resource(OutputTensors[TensorIndex].GetPooledBuffer()->GetRHI());
		OutputDmlAllocation[TensorIndex] = LinkTensorResourceToONNXRuntime(InDmlApi, InMemoryInfo, OutputTensors[TensorIndex], OutputOrtTensors[TensorIndex], D3DResource);
	}
}

void UNeuralNetwork::FImplBackEndUEAndORT::FInferenceContext::ReleaseDMLAllocations(OrtDmlApi const* InDmlApi)
{
	for (int32 TensorIndex = 0; TensorIndex < OutputDmlAllocation.Num(); ++TensorIndex)
	{
		InDmlApi->FreeGPUAllocation(OutputDmlAllocation[TensorIndex]);
	}
	OutputDmlAllocation.Reset();
}

#endif // PLATFORM_WIN64

#endif //WITH_UE_AND_ORT_SUPPORT

int32 UNeuralNetwork::FImplBackEndUEAndORT::CreateInferenceContext(ENeuralDeviceType InInputDeviceType, ENeuralDeviceType InOutputDeviceType)
{
#ifdef WITH_UE_AND_ORT_SUPPORT
	const int32 Handle = Contexts.Emplace();
	Contexts[Handle].Init(Session.Get(), Allocator.Get(), AllocatorInfo.Get(), InInputDeviceType, InOutputDeviceType);
	return Handle;
#else
	UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FImplBackEndUEAndORT::CreateInferenceContext(): Platform or Operating System not suported yet for UEAndORT"
		" BackEnd. Set BackEnd to ENeuralBackEnd::Auto or ENeuralBackEnd::UEOnly for this platform."));
	return -1;
#endif
}

void UNeuralNetwork::FImplBackEndUEAndORT::DestroyInferenceContext(int32 InContextHandle)
{
#ifdef WITH_UE_AND_ORT_SUPPORT
	check(Contexts.IsValidIndex(InContextHandle));
#ifdef PLATFORM_WIN64
	Contexts[InContextHandle].ReleaseDMLAllocations(DmlApi);
#endif
	Contexts[InContextHandle].Release();
	Contexts.RemoveAt(InContextHandle);
#endif
}

void UNeuralNetwork::FImplBackEndUEAndORT::Run(int32 InContextHandle)
{
#ifdef WITH_UE_AND_ORT_SUPPORT
	FInferenceContext& Context = Contexts[InContextHandle];

	Session.Get()->Run(
		Ort::RunOptions{ nullptr },
		Context.InputTensorNames.GetData(), &Context.InputOrtTensors[0], Context.InputTensorNames.Num(),
		Context.OutputTensorNames.GetData(), &Context.OutputOrtTensors[0], Context.OutputTensorNames.Num());
#endif
}

void UNeuralNetwork::FImplBackEndUEAndORT::Run(FRDGBuilder& GraphBuilder, int32 InContextHandle)
{
#ifdef WITH_UE_AND_ORT_SUPPORT
#if WITH_EDITOR
	try
#endif //WITH_EDITOR
	{
		Contexts[InContextHandle].UpdateGPUAllocations(GraphBuilder);

#ifdef PLATFORM_WIN64
		Contexts[InContextHandle].BindDMLAllocations(DmlApi, DmlGPUMemoryInfo.Get());
#endif

		AddPass(GraphBuilder, RDG_EVENT_NAME("OrtRun"), [InSession = Session.Get(), &Context = Contexts[InContextHandle]](FRHICommandListImmediate&)
		{
			InSession->Run(
				Ort::RunOptions{ nullptr },
				Context.InputTensorNames.GetData(), & Context.InputOrtTensors[0], Context.InputTensorNames.Num(),
				Context.OutputTensorNames.GetData(), & Context.OutputOrtTensors[0], Context.OutputTensorNames.Num());
		});
	}
#if WITH_EDITOR
	catch (const std::exception& Exception)
	{
		UE_LOG(LogNeuralNetworkInference, Error, TEXT("%s"), UTF8_TO_TCHAR(Exception.what()));
	}
#endif //WITH_EDITOR
#endif //WITH_UE_AND_ORT_SUPPORT
}

FNeuralTensor& UNeuralNetwork::FImplBackEndUEAndORT::GetInputTensorForContext(int32 InContextHandle, int32 InTensorIndex)
{
#ifdef WITH_UE_AND_ORT_SUPPORT
	check(Contexts.IsValidIndex(InContextHandle));
	return Contexts[InContextHandle].InputTensors[InTensorIndex];
#else
	return InputTensors[InTensorIndex];
#endif
}

FNeuralTensor& UNeuralNetwork::FImplBackEndUEAndORT::GetOutputTensorForContext(int32 InContextHandle, int32 InTensorIndex)
{
#ifdef WITH_UE_AND_ORT_SUPPORT
	check(Contexts.IsValidIndex(InContextHandle));
	return Contexts[InContextHandle].OutputTensors[InTensorIndex];
#else
	return OutputTensors[InTensorIndex];
#endif
}
