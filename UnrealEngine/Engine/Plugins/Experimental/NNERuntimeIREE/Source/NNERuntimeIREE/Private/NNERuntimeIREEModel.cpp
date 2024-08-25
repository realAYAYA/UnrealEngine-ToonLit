// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeIREEModel.h"

#ifdef WITH_NNE_RUNTIME_IREE

#include "GenericPlatform/GenericPlatformProcess.h"
#include "HAL/FileManager.h"
#include "Memory/SharedBuffer.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "NNE.h"
#include "NNEModelData.h"
#include "NNEStatus.h"
#include "Serialization/Archive.h"

#if PLATFORM_MICROSOFT
#include "Microsoft/AllowMicrosoftPlatformTypes.h"
#include "Microsoft/AllowMicrosoftPlatformAtomics.h"
#endif // PLATFORM_MICROSOFT
THIRD_PARTY_INCLUDES_START
#include "iree/hal/drivers/local_sync/sync_device.h"
#include "iree/hal/local/loaders/static_library_loader.h"
#include "iree/runtime/call.h"
#include "iree/runtime/instance.h"
#include "iree/runtime/session.h"
#include "iree/vm/bytecode/module.h"
THIRD_PARTY_INCLUDES_END
#if PLATFORM_MICROSOFT
#include "Microsoft/HideMicrosoftPlatformAtomics.h"
#include "Microsoft/HideMicrosoftPlatformTypes.h"
#endif // PLATFORM_MICROSOF

namespace UE::NNERuntimeIREE
{
	namespace Private
	{
		iree_hal_element_types_t NNEToIREEType(ENNETensorDataType Type)
		{
			switch (Type)
			{
			case ENNETensorDataType::None:
				return IREE_HAL_ELEMENT_TYPE_NONE;
				break;
			case ENNETensorDataType::Char:
				return IREE_HAL_ELEMENT_TYPE_UINT_8;
				break;
			case ENNETensorDataType::Boolean:
				return IREE_HAL_ELEMENT_TYPE_BOOL_8;
				break;
			case ENNETensorDataType::Half:
				return IREE_HAL_ELEMENT_TYPE_FLOAT_16;
				break;
			case ENNETensorDataType::Float:
				return IREE_HAL_ELEMENT_TYPE_FLOAT_32;
				break;
			case ENNETensorDataType::Double:
				return IREE_HAL_ELEMENT_TYPE_FLOAT_64;
				break;
			case ENNETensorDataType::Int8:
				return IREE_HAL_ELEMENT_TYPE_INT_8;
				break;
			case ENNETensorDataType::Int16:
				return IREE_HAL_ELEMENT_TYPE_INT_16;
				break;
			case ENNETensorDataType::Int32:
				return IREE_HAL_ELEMENT_TYPE_INT_32;
				break;
			case ENNETensorDataType::Int64:
				return IREE_HAL_ELEMENT_TYPE_INT_64;
				break;
			case ENNETensorDataType::UInt8:
				return IREE_HAL_ELEMENT_TYPE_UINT_8;
				break;
			case ENNETensorDataType::UInt16:
				return IREE_HAL_ELEMENT_TYPE_UINT_16;
				break;
			case ENNETensorDataType::UInt32:
				return IREE_HAL_ELEMENT_TYPE_UINT_32;
				break;
			case ENNETensorDataType::UInt64:
				return IREE_HAL_ELEMENT_TYPE_UINT_64;
				break;
			case ENNETensorDataType::Complex64:
				return IREE_HAL_ELEMENT_TYPE_COMPLEX_FLOAT_64;
				break;
			case ENNETensorDataType::Complex128:
				return IREE_HAL_ELEMENT_TYPE_COMPLEX_FLOAT_128;
				break;
			case ENNETensorDataType::BFloat16:
				return IREE_HAL_ELEMENT_TYPE_BFLOAT_16;
				break;
			default:
				return IREE_HAL_ELEMENT_TYPE_NONE;
				break;
			}
		}

		void PrintIREEError(const FString& InMessage, iree_status_t InStatus)
		{
			iree_host_size_t TrueLength = 0;
			iree_status_format(InStatus, 0, (char*)nullptr, &TrueLength);
			void* ErrorString = FMemory::Malloc(TrueLength + 1);
			((char*)ErrorString)[TrueLength] = (char)0;
			iree_status_format(InStatus, TrueLength, (char*)ErrorString, &TrueLength);
			UE_LOG(LogNNE, Error, TEXT("%s: %s"), *InMessage, *FString(StringCast<TCHAR>(static_cast<const ANSICHAR*>(ErrorString)).Get()));
			FMemory::Free(ErrorString);
		}

		class FInstance
		{
		private:
			iree_runtime_instance_t* Instance;
			static TWeakPtr<FInstance> WeakInstancePtr;
			static FCriticalSection CriticalSection;

			FInstance(iree_runtime_instance_t* InInstance) : Instance(InInstance)
			{
				check(InInstance);
			}

		public:
			~FInstance()
			{
				iree_runtime_instance_release(Instance);
			}

			static TSharedPtr<FInstance> GetInstance()
			{
				FScopeLock ScopeLock(&CriticalSection);

				if (WeakInstancePtr.IsValid())
				{
					return WeakInstancePtr.Pin();
				}

				iree_status_t Status = iree_ok_status();
				if (!iree_status_is_ok(Status))
				{
					iree_status_free(Status);
					return TSharedPtr<FInstance>();
				}

				iree_runtime_instance_options_t InstanceOptions;
				iree_runtime_instance_options_initialize(&InstanceOptions);
				iree_runtime_instance_options_use_all_available_drivers(&InstanceOptions);

				iree_runtime_instance_t* Instance = nullptr;
				Status = iree_runtime_instance_create(&InstanceOptions, iree_allocator_system(), &Instance);
				if (!iree_status_is_ok(Status))
				{
					Private::PrintIREEError("UE::NNERuntimeIREE::Private::FInstance failed to create the instance", Status);

					if (Instance)
					{
						iree_runtime_instance_release(Instance);
					}

					iree_status_free(Status);
					return TSharedPtr<FInstance>();
				}

				TSharedPtr<FInstance> SharedInstance = TSharedPtr<FInstance>(new FInstance(Instance));
				WeakInstancePtr = SharedInstance;

				iree_status_free(Status);
				return SharedInstance;
			}

			bool CreateModule(TConstArrayView<uint8> InVmfbDataView, iree_vm_module_t** OutModule)
			{
				FScopeLock ScopeLock(&CriticalSection);

				check(!InVmfbDataView.IsEmpty());
				check(OutModule);

				iree_status_t Status = iree_ok_status();
				check(iree_status_is_ok(Status));

				const iree_const_byte_span_t ModuleData = iree_make_const_byte_span(InVmfbDataView.GetData(), InVmfbDataView.Num());

				iree_vm_module_t* Module = nullptr;
				Status = iree_vm_bytecode_module_create(iree_runtime_instance_vm_instance(Instance), ModuleData, iree_allocator_null(), GetHostAllocator(), &Module);
				if (!iree_status_is_ok(Status))
				{
					Private::PrintIREEError("UE::NNERuntimeIREE::Private::FInstance failed to create the module", Status);

					if (Module)
					{
						iree_vm_module_release(Module);
					}

					iree_status_free(Status);
					return false;
				}

				*OutModule = Module;

				iree_status_free(Status);
				return true;
			}

			bool CreateSyncDevice(void* InLibraryQueryFuntionPointer, iree_hal_device_t** OutDevice)
			{
				FScopeLock ScopeLock(&CriticalSection);

				check(InLibraryQueryFuntionPointer);
				check(OutDevice);

				iree_status_t Status = iree_ok_status();
				check(iree_status_is_ok(Status));

				iree_allocator_t HostAllocator = GetHostAllocator();

				iree_hal_executable_loader_t* LibraryLoader = nullptr;
				const iree_hal_executable_library_query_fn_t LibraryList[] = { (iree_hal_executable_library_query_fn_t)InLibraryQueryFuntionPointer };
				Status = iree_hal_static_library_loader_create(IREE_ARRAYSIZE(LibraryList), LibraryList, iree_hal_executable_import_provider_null(), HostAllocator, &LibraryLoader);
				if (!iree_status_is_ok(Status))
				{
					Private::PrintIREEError("UE::NNERuntimeIREE::Private::FInstance failed to create the library loader", Status);

					if (LibraryLoader)
					{
						iree_hal_executable_loader_release(LibraryLoader);
					}

					iree_status_free(Status);
					return false;
				}

				iree_hal_allocator_t* DeviceAllocator = nullptr;
				iree_string_view_t Identifier = iree_make_cstring_view("sync");
				Status = iree_hal_allocator_create_heap(Identifier, HostAllocator, HostAllocator, &DeviceAllocator);
				if (!iree_status_is_ok(Status))
				{
					Private::PrintIREEError("UE::NNERuntimeIREE::Private::FInstance failed to create the device allocator", Status);

					if (DeviceAllocator)
					{
						iree_hal_allocator_release(DeviceAllocator);
					}

					iree_hal_executable_loader_release(LibraryLoader);
					iree_status_free(Status);
					return false;
				}

				iree_hal_device_t* Device = nullptr;
				iree_hal_sync_device_params_t DeviceParams;
				iree_hal_sync_device_params_initialize(&DeviceParams);
				Status = iree_hal_sync_device_create(Identifier, &DeviceParams, 1, &LibraryLoader, DeviceAllocator, HostAllocator, &Device);
				if (!iree_status_is_ok(Status))
				{
					Private::PrintIREEError("UE::NNERuntimeIREE::Private::FInstance failed to create the device", Status);

					if (Device)
					{
						iree_hal_device_release(Device);
					}

					iree_hal_allocator_release(DeviceAllocator);
					iree_hal_executable_loader_release(LibraryLoader);
					iree_status_free(Status);
					return false;
				}

				iree_hal_allocator_release(DeviceAllocator);
				iree_hal_executable_loader_release(LibraryLoader);

				*OutDevice = Device;

				iree_status_free(Status);
				return true;
			}

			bool CreateSession(iree_hal_device_t* InDevice, iree_runtime_session_t** OutSession)
			{
				FScopeLock ScopeLock(&CriticalSection);

				check(InDevice);
				check(OutSession);

				iree_status_t Status = iree_ok_status();
				check(iree_status_is_ok(Status));

				iree_runtime_session_options_t SessionOptions;
				iree_runtime_session_options_initialize(&SessionOptions);

				iree_runtime_session_t* Session = nullptr;
				Status = iree_runtime_session_create_with_device(Instance, &SessionOptions, InDevice, GetHostAllocator(), &Session);
				if (!iree_status_is_ok(Status))
				{
					Private::PrintIREEError("UE::NNERuntimeIREE::Private::FInstance failed to create the session", Status);

					if (Session)
					{
						iree_runtime_session_release(Session);
					}

					iree_status_free(Status);
					return false;
				}

				*OutSession = Session;

				iree_status_free(Status);
				return true;
			}

			iree_allocator_t GetHostAllocator()
			{
				FScopeLock ScopeLock(&CriticalSection);
				return iree_runtime_instance_host_allocator(Instance);
			}
		};
		TWeakPtr<FInstance> FInstance::WeakInstancePtr;
		FCriticalSection FInstance::CriticalSection;

		class FModule
		{
		private:
			TSharedRef<FInstance> Instance;
			TSharedRef<UE::NNE::FSharedModelData> ModelData;
			iree_vm_module_t* Module;
			TArray<UE::NNERuntimeIREE::FFunctionMetaData> FunctionMetaData;
			static TMap<FString, TWeakPtr<FModule>> Modules;

			FModule(TSharedRef<FInstance> InInstance, TSharedRef<UE::NNE::FSharedModelData> InModelData, iree_vm_module_t* InModule, TConstArrayView<UE::NNERuntimeIREE::FFunctionMetaData> InFunctionMetaData)
				: Instance(InInstance), ModelData(InModelData), Module(InModule), FunctionMetaData(InFunctionMetaData)
			{
				check(!ModelData->GetView().IsEmpty());
				check(InModule);
				check(!InFunctionMetaData.IsEmpty());
			}

		public:
			~FModule()
			{
				iree_vm_module_release(Module);
			}

			static TSharedPtr<FModule> Make(const FString& InVmfbPath, const FString& InVmfbName, const UNNERuntimeIREEModuleMetaData& InModuleMetaData)
			{
				check(!InVmfbName.IsEmpty());
				check(!InModuleMetaData.FunctionMetaData.IsEmpty());

				FString CombinedPath = FPaths::Combine(InVmfbPath, InVmfbName);
				if (Modules.Contains(CombinedPath) && Modules[CombinedPath].IsValid())
				{
					return Modules[CombinedPath].Pin();
				}

				TUniquePtr<FArchive> Reader = TUniquePtr<FArchive>(IFileManager::Get().CreateFileReader(*CombinedPath, 0));
				if (!Reader)
				{
					UE_LOG(LogNNE, Error, TEXT("UE::NNERuntimeIREE::Private::FModule failed to open the vmfb data file '%s'"), *CombinedPath);
					return TSharedPtr<FModule>();
				}
				int64 DataSize = Reader->TotalSize();
				if (DataSize < 1)
				{
					UE_LOG(LogNNE, Error, TEXT("UE::NNERuntimeIREE::Private::FModule's vmfb data file '%s' is empty"), *CombinedPath);
					return TSharedPtr<FModule>();
				}

				void* Data = FMemory::Malloc(DataSize, IREE_HAL_HEAP_BUFFER_ALIGNMENT);
				Reader->Serialize(Data, DataSize);
				TSharedPtr<UE::NNE::FSharedModelData> ModelData = MakeShared<UE::NNE::FSharedModelData>(FSharedBuffer::TakeOwnership(Data, DataSize, FMemory::Free), IREE_HAL_HEAP_BUFFER_ALIGNMENT);
				if (!ModelData.IsValid())
				{
					return TSharedPtr<FModule>();
				}

				TSharedPtr<FInstance> Instance = FInstance::GetInstance();
				if (!Instance.IsValid())
				{
					return TSharedPtr<FModule>();
				}

				iree_vm_module_t* Module = nullptr;
				if (!Instance->CreateModule(ModelData->GetView(), &Module))
				{
					return TSharedPtr<FModule>();
				}

				TSharedPtr<FModule> Result = TSharedPtr<FModule>(new FModule(Instance.ToSharedRef(), ModelData.ToSharedRef(), Module, InModuleMetaData.FunctionMetaData));
				Modules.Add(CombinedPath, Result);
				return Result;
			}

			bool AppendToSession(iree_runtime_session_t* InSession)
			{
				check(InSession);

				iree_status_t Status = iree_ok_status();
				check(iree_status_is_ok(Status));

				Status = iree_runtime_session_append_module(InSession, Module);
				if (!iree_status_is_ok(Status))
				{
					PrintIREEError("UE::NNERuntimeIREE::Private::FModule failed to append the module to the session", Status);
					iree_status_free(Status);
					return false;
				}

				iree_status_free(Status);
				return true;
			}

			TConstArrayView<UE::NNERuntimeIREE::FFunctionMetaData> GetFunctionMetaDataView()
			{
				return FunctionMetaData;
			}

			bool GetFunctionByName(const FString& InFunctionName, iree_vm_function_t* OutFunction)
			{
				iree_status_t Status = iree_ok_status();
				check(iree_status_is_ok(Status));

				bool bFound = false;
				iree_host_size_t Ordinal = 0;
				iree_vm_function_t Function;
				do 
				{
					Status = iree_vm_module_lookup_function_by_ordinal(Module, IREE_VM_FUNCTION_LINKAGE_EXPORT, Ordinal, &Function);
					if (iree_status_is_ok(Status))
					{
						Ordinal++;
						iree_string_view_t FunctionNameView = iree_vm_function_name(&Function);
						FString FunctionName = FString::ConstructFromPtrSize(FunctionNameView.data, FunctionNameView.size);
						if (FunctionName.Equals(InFunctionName))
						{
							bFound = true;
							break;
						}
					}
				} while (iree_status_is_ok(Status));

				if (!bFound)
				{
					UE_LOG(LogNNE, Error, TEXT("UE::NNERuntimeIREE::Private::FModule failed to find the module function %s"), *InFunctionName);
					iree_status_free(Status);
					return false;
				}

				*OutFunction = Function;
				iree_status_free(Status);
				return true;
			}
		};
		TMap<FString, TWeakPtr<FModule>> FModule::Modules;
	} // Private

	namespace CPU
	{
		namespace Private
		{
			class FLibrary
			{
			private:
				void* Library;
				static TMap<FString, TWeakPtr<FLibrary>> Libraries;

				FLibrary(void* InLibrary) : Library(InLibrary)
				{
					check(InLibrary);
				}

			public:
				~FLibrary()
				{
					FPlatformProcess::FreeDllHandle(Library);
				}

				static TSharedPtr<FLibrary> GetLibrary(const FString& InLibraryPath, const FString& InLibraryName)
				{
					check(!InLibraryName.IsEmpty());

					FString CombinedPath = FPaths::Combine(InLibraryPath, InLibraryName);
					if (Libraries.Contains(CombinedPath) && Libraries[CombinedPath].IsValid())
					{
						return Libraries[CombinedPath].Pin();
					}

#ifdef NNE_RUNTIME_IREE_USE_COMBINED_LIB_PATH
					void* Library = FPlatformProcess::GetDllHandle(*CombinedPath);
					if (!Library)
					{
						UE_LOG(LogNNE, Error, TEXT("UE::NNERuntimeIREE::CPU::Private::FLibrary failed to load the shared library '%s'"), *CombinedPath);
						return TSharedPtr<FLibrary>();
					}
#else
					FPlatformProcess::PushDllDirectory(*InLibraryPath);
					void* Library = FPlatformProcess::GetDllHandle(*InLibraryName);
					FPlatformProcess::PopDllDirectory(*InLibraryPath);
					if (!Library)
					{
						UE_LOG(LogNNE, Error, TEXT("UE::NNERuntimeIREE::CPU::Private::FLibrary failed to load the shared library '%s' from '%s'"), *InLibraryName, *InLibraryPath);
						return TSharedPtr<FLibrary>();
					}
#endif

					TSharedPtr<FLibrary> Result = TSharedPtr<FLibrary>(new FLibrary(Library));
					Libraries.Add(CombinedPath, Result);
					return Result;
				}

				bool GetFunctionPointer(const FString& InFunctionName, void** OutFunctionPointer) const
				{
					void* Result = FPlatformProcess::GetDllExport(Library, *InFunctionName);
					if (Result)
					{
						*OutFunctionPointer = Result;
						return true;
					}
					UE_LOG(LogNNE, Error, TEXT("UE::NNERuntimeIREE::CPU::Private::FLibrary failed to get the function %s"), *InFunctionName);
					return false;
				}
			};
			TMap<FString, TWeakPtr<FLibrary>> FLibrary::Libraries;

			class FDevice
			{
			private:
				TSharedRef<UE::NNERuntimeIREE::Private::FInstance> Instance;
				TSharedRef<FLibrary> Library;
				iree_hal_device_t* Device;
				static TMap<FString, TWeakPtr<FDevice>> Devices;

				FDevice(TSharedRef<UE::NNERuntimeIREE::Private::FInstance> InInstance, TSharedRef<FLibrary> InLibrary, iree_hal_device_t* InDevice) : Instance(InInstance), Library(InLibrary), Device(InDevice)
				{
					check(InDevice);
				}

			public:
				~FDevice()
				{
					iree_hal_device_release(Device);
				}

				static TSharedPtr<FDevice> Make(const FString& InLibraryPath, const FString& InLibraryName, const FString& LibraryQueryFunctionName)
				{
					check(!InLibraryName.IsEmpty());
					check(!LibraryQueryFunctionName.IsEmpty());

					FString CombinedPathPlusFunction = FPaths::Combine(InLibraryPath, InLibraryName) + "::" + LibraryQueryFunctionName;
					if (Devices.Contains(CombinedPathPlusFunction) && Devices[CombinedPathPlusFunction].IsValid())
					{
						return Devices[CombinedPathPlusFunction].Pin();
					}

					TSharedPtr<FLibrary> Library = FLibrary::GetLibrary(InLibraryPath, InLibraryName);
					if (!Library.IsValid())
					{
						return TSharedPtr<FDevice>();
					}

					void* LibraryQueryFunctionPointer = nullptr;
					Library->GetFunctionPointer(LibraryQueryFunctionName, &LibraryQueryFunctionPointer);
					if (!LibraryQueryFunctionPointer)
					{
						return TSharedPtr<FDevice>();
					}

					TSharedPtr<UE::NNERuntimeIREE::Private::FInstance> Instance = UE::NNERuntimeIREE::Private::FInstance::GetInstance();
					if (!Instance.IsValid())
					{
						return TSharedPtr<FDevice>();
					}

					iree_hal_device_t* Device = nullptr;
					if (!Instance->CreateSyncDevice(LibraryQueryFunctionPointer, &Device))
					{
						return TSharedPtr<FDevice>();
					}

					TSharedPtr<FDevice> Result = TSharedPtr<FDevice>(new FDevice(Instance.ToSharedRef(), Library.ToSharedRef(), Device));
					Devices.Add(CombinedPathPlusFunction, Result);
					return Result;
				}

				bool CreateSession(iree_runtime_session_t** OutSession)
				{
					return Instance->CreateSession(Device, OutSession);
				}

				iree_hal_allocator_t* GetDeviceAllocator()
				{
					return iree_hal_device_allocator(Device);
				}
			};
			TMap<FString, TWeakPtr<FDevice>> FDevice::Devices;

			class FSession
			{
			private:
				TSharedRef<FDevice> Device;
				TSharedRef<UE::NNERuntimeIREE::Private::FModule> Module;
				iree_runtime_session_t* Session;
				iree_runtime_call_t Call;
				TArray<UE::NNE::FTensorDesc> InputTensorDescs;
				TArray<UE::NNE::FTensorDesc> OutputTensorDescs;
				TArray<UE::NNE::FTensorShape> InputTensorShapes;
				TArray<UE::NNE::FTensorShape> OutputTensorShapes;

				FSession(TSharedRef<FDevice> InDevice, TSharedRef<UE::NNERuntimeIREE::Private::FModule> InModule, iree_runtime_session_t* InSession, const iree_runtime_call_t& InCall, TConstArrayView<UE::NNE::FTensorDesc> InInputTensorDescs, TConstArrayView<UE::NNE::FTensorDesc> InOutputTensorDescs)
					: Device(InDevice), Module(InModule), Session(InSession), Call(InCall), InputTensorDescs(InInputTensorDescs), OutputTensorDescs(InOutputTensorDescs)
				{
					check(InSession);

					check(!InputTensorDescs.IsEmpty());
					bool bAllConcrete = true;
					for (int32 i = 0; i < InInputTensorDescs.Num(); i++)
					{
						bAllConcrete &= InInputTensorDescs[i].GetShape().IsConcrete();
					}
					if (bAllConcrete)
					{
						for (int32 i = 0; i < InInputTensorDescs.Num(); i++)
						{
							InputTensorShapes.Add(UE::NNE::FTensorShape::MakeFromSymbolic(InInputTensorDescs[i].GetShape()));
						}
					}

					check(!OutputTensorDescs.IsEmpty());
					bAllConcrete = true;
					for (int32 i = 0; i < InOutputTensorDescs.Num(); i++)
					{
						bAllConcrete &= InOutputTensorDescs[i].GetShape().IsConcrete();
					}
					if (bAllConcrete)
					{
						for (int32 i = 0; i < InOutputTensorDescs.Num(); i++)
						{
							OutputTensorShapes.Add(UE::NNE::FTensorShape::MakeFromSymbolic(InOutputTensorDescs[i].GetShape()));
						}
					}
				}

			public:
				using ESetInputTensorShapesStatus = UE::NNE::IModelInstanceCPU::ESetInputTensorShapesStatus;
				using ERunSyncStatus = UE::NNE::IModelInstanceCPU::ERunSyncStatus;

				~FSession()
				{
					iree_runtime_call_deinitialize(&Call);
					iree_runtime_session_release(Session);
				}

				static TSharedPtr<FSession> Make(TSharedRef<FDevice> InDevice, TSharedRef<UE::NNERuntimeIREE::Private::FModule> InModule)
				{
					check(!InModule->GetFunctionMetaDataView().IsEmpty());

					iree_runtime_session_t* Session = nullptr;
					if (!InDevice->CreateSession(&Session))
					{
						return TSharedPtr<FSession>();
					}

					if (!InModule->AppendToSession(Session))
					{
						iree_runtime_session_release(Session);
						return TSharedPtr<FSession>();
					}

					iree_status_t Status = iree_ok_status();
					check(iree_status_is_ok(Status));

					FString MainFunctionName = InModule->GetFunctionMetaDataView()[0].Name;
					iree_vm_function_t MainFunction;
					if (!InModule->GetFunctionByName(MainFunctionName, &MainFunction))
					{
						iree_runtime_session_release(Session);
						iree_status_free(Status);
						return TSharedPtr<FSession>();
					}

					iree_host_size_t NumInputs = 0;
					iree_host_size_t NumOutputs = 0;
					iree_vm_function_signature_t Signature = iree_vm_function_signature(&MainFunction);
					Status = iree_vm_function_call_count_arguments_and_results(&Signature, &NumInputs, &NumOutputs);
					TConstArrayView<UE::NNE::FTensorDesc> InputTensorDescs = InModule->GetFunctionMetaDataView()[0].InputDescs;
					TConstArrayView<UE::NNE::FTensorDesc> OutputTensorDescs = InModule->GetFunctionMetaDataView()[0].OutputDescs;
					if (!iree_status_is_ok(Status) || NumInputs != InputTensorDescs.Num() || NumOutputs != OutputTensorDescs.Num())
					{
						UE_LOG(LogNNE, Error, TEXT("UE::NNERuntimeIREE::CPU::Private::FSession has a function signature mismatch in function %s"), *MainFunctionName);
						iree_runtime_session_release(Session);
						iree_status_free(Status);
						return TSharedPtr<FSession>();
					}

					iree_runtime_call_t Call;
					Status = iree_runtime_call_initialize(Session, MainFunction, &Call);
					if (!iree_status_is_ok(Status))
					{
						UE::NNERuntimeIREE::Private::PrintIREEError("UE::NNERuntimeIREE::CPU::Private::FSession failed to initialize the session call", Status);
						iree_runtime_session_release(Session);
						iree_status_free(Status);
						return TSharedPtr<FSession>();
					}

					TSharedPtr<FSession> Result = TSharedPtr<FSession>(new FSession(InDevice, InModule, Session, Call, InputTensorDescs, OutputTensorDescs));
					iree_status_free(Status);
					return Result;
				}

				TConstArrayView<UE::NNE::FTensorDesc> GetInputTensorDescs() const
				{
					return InputTensorDescs;
				}

				TConstArrayView<UE::NNE::FTensorDesc> GetOutputTensorDescs() const
				{
					return OutputTensorDescs;
				}

				TConstArrayView<UE::NNE::FTensorShape> GetInputTensorShapes() const
				{
					return InputTensorShapes;
				}

				TConstArrayView<UE::NNE::FTensorShape> GetOutputTensorShapes() const
				{
					return OutputTensorShapes;
				}

				ESetInputTensorShapesStatus SetInputTensorShapes(TConstArrayView<UE::NNE::FTensorShape> InInputShapes)
				{
					check(InputTensorDescs.Num() == InInputShapes.Num());
					checkCode(for (int32 i = 0; i < InputTensorDescs.Num(); i++) { check(InInputShapes[i].IsCompatibleWith(InputTensorDescs[i].GetShape())); });
					InputTensorShapes = InInputShapes;
					return ESetInputTensorShapesStatus::Ok;
				}

				ERunSyncStatus RunSyncCPU(TConstArrayView<UE::NNE::FTensorBindingCPU> InInputBindings, TConstArrayView<UE::NNE::FTensorBindingCPU> InOutputBindings)
				{
					check(InInputBindings.Num() == InputTensorShapes.Num());

					iree_status_t Status = iree_ok_status();
					check(iree_status_is_ok(Status));

					iree_runtime_call_reset(&Call);

					for (int32 i = 0; i < InInputBindings.Num(); i++)
					{
						check(InInputBindings[i].SizeInBytes >= InputTensorShapes[i].Volume() * InputTensorDescs[i].GetElementByteSize());
						checkf(FMath::Modulo<uint64>((uint64)InInputBindings[i].Data, IREE_HAL_HEAP_BUFFER_ALIGNMENT) == 0, TEXT("NNERuntimeIREECpu requires input- and output-buffer memory to be aligned with %d bytes"), IREE_HAL_HEAP_BUFFER_ALIGNMENT);

						iree_hal_buffer_view_t* TempBufferView;
						iree_hal_buffer_params_t Params = { 0 };
						Params.type = IREE_HAL_MEMORY_TYPE_DEVICE_LOCAL;
						Params.usage = IREE_HAL_BUFFER_USAGE_DEFAULT;
						iree_hal_dim_t Shape[UE::NNE::FTensorShape::MaxRank];
						for (int32 j = 0; j < InputTensorShapes[i].Rank(); j++)
						{
							Shape[j] = InputTensorShapes[i].GetData()[j];
						}
						ENNETensorDataType NNEType = InputTensorDescs[i].GetDataType();
						iree_hal_element_types_t IREEType = UE::NNERuntimeIREE::Private::NNEToIREEType(NNEType);
						Status = iree_hal_buffer_view_allocate_buffer(
							Device->GetDeviceAllocator(),
							InputTensorShapes[i].Rank(), Shape,
							IREEType, IREE_HAL_ENCODING_TYPE_DENSE_ROW_MAJOR,
							Params,
							iree_make_const_byte_span((void*)InInputBindings[i].Data, InInputBindings[i].SizeInBytes),
							&TempBufferView);
						if (!iree_status_is_ok(Status))
						{
							UE::NNERuntimeIREE::Private::PrintIREEError("UE::NNERuntimeIREE::CPU::Private::FSession failed to allocate the buffer view", Status);
							if (TempBufferView)
							{
								iree_hal_buffer_view_release(TempBufferView);
							}
							iree_status_free(Status);
							return ERunSyncStatus::Fail;
						}

						Status = iree_runtime_call_inputs_push_back_buffer_view(&Call, TempBufferView);
						iree_hal_buffer_view_release(TempBufferView);
						if (!iree_status_is_ok(Status))
						{
							UE::NNERuntimeIREE::Private::PrintIREEError("UE::NNERuntimeIREE::CPU::Private::FSession failed to push the buffer view to the input list", Status);
							iree_status_free(Status);
							return ERunSyncStatus::Fail;
						}
					}

					Status = iree_runtime_call_invoke(&Call, 0);
					if (!iree_status_is_ok(Status))
					{
						UE::NNERuntimeIREE::Private::PrintIREEError("UE::NNERuntimeIREE::CPU::Private::FSession failed to call the model function", Status);
						iree_status_free(Status);
						return ERunSyncStatus::Fail;
					}

					OutputTensorShapes.Reset();
					TArray<iree_hal_buffer_view_t*> BufferViews;
					iree_hal_buffer_view_t* BufferView = nullptr;
					Status = iree_runtime_call_outputs_pop_front_buffer_view(&Call, &BufferView);
					while (iree_status_is_ok(Status))
					{
						iree_host_size_t Rank = iree_hal_buffer_view_shape_rank(BufferView);
						const iree_hal_dim_t* Dims = iree_hal_buffer_view_shape_dims(BufferView);
						uint32 Shape[UE::NNE::FTensorShape::MaxRank];
						for (int32 i = 0; i < FMath::Min((int32)Rank, UE::NNE::FTensorShape::MaxRank); i++)
						{
							Shape[i] = (uint32)Dims[i];
						}
						OutputTensorShapes.Add(UE::NNE::FTensorShape::Make(TConstArrayView<uint32>(Shape, FMath::Min((int32)Rank, UE::NNE::FTensorShape::MaxRank))));

						BufferViews.Add(BufferView);
						Status = iree_runtime_call_outputs_pop_front_buffer_view(&Call, &BufferView);
					}

					bool bCopyResults = true;
					if (InOutputBindings.Num() != OutputTensorShapes.Num())
					{
						bCopyResults = false;
					}
					for (int32 i = 0; i < InOutputBindings.Num() && bCopyResults; i++)
					{
						if (InOutputBindings[i].SizeInBytes < iree_hal_buffer_view_element_size(BufferViews[i]) * iree_hal_buffer_view_element_count(BufferViews[i]))
						{
							bCopyResults = false;
						}
					}
					ERunSyncStatus Result = ERunSyncStatus::Ok;
					if (bCopyResults)
					{
						for (int32 i = 0; i < InOutputBindings.Num(); i++)
						{
							iree_hal_buffer_t* Buffer = iree_hal_buffer_view_buffer(BufferViews[i]);
							if (!Buffer)
							{
								UE_LOG(LogNNE, Error, TEXT("UE::NNERuntimeIREE::CPU::Private::FSession failed to get the result buffer"));
								Result = ERunSyncStatus::Fail;
								break;
							}

							int32 DataSizeInBytes = iree_hal_buffer_view_element_size(BufferViews[i]) * iree_hal_buffer_view_element_count(BufferViews[i]);

							iree_hal_buffer_mapping_t BufferMapping;
							Status = iree_hal_buffer_map_range(Buffer, IREE_HAL_MAPPING_MODE_PERSISTENT, IREE_HAL_MEMORY_ACCESS_READ, 0, DataSizeInBytes, &BufferMapping);
							if (!iree_status_is_ok(Status))
							{
								UE::NNERuntimeIREE::Private::PrintIREEError("UE::NNERuntimeIREE::CPU::Private::FSession failed to map the result buffer", Status);
								Result = ERunSyncStatus::Fail;
								break;
							}
							FMemory::Memcpy(InOutputBindings[i].Data, BufferMapping.contents.data, DataSizeInBytes);
							iree_hal_buffer_unmap_range(&BufferMapping);
						}
					}

					for (int32 i = 0; i < InOutputBindings.Num(); i++)
					{
						iree_hal_buffer_view_release(BufferViews[i]);
					}
					iree_status_free(Status);
					return Result;
				}
			};
		}

		FModelInstance::FModelInstance(TSharedRef<UE::NNERuntimeIREE::CPU::Private::FSession> InSession) : Session(InSession)
		{
			
		}

		TSharedPtr<FModelInstance> FModelInstance::Make(TSharedRef<UE::NNERuntimeIREE::CPU::Private::FDevice> InDevice, TSharedRef<UE::NNERuntimeIREE::Private::FModule> InModule)
		{
			TSharedPtr<Private::FSession> Session = Private::FSession::Make(InDevice, InModule);
			if (!Session.IsValid())
			{
				return TSharedPtr<FModelInstance>();
			}

			return TSharedPtr<FModelInstance>(new FModelInstance(Session.ToSharedRef()));
		}

		TConstArrayView<UE::NNE::FTensorDesc> FModelInstance::GetInputTensorDescs() const
		{
			return Session->GetInputTensorDescs();
		}

		TConstArrayView<UE::NNE::FTensorDesc> FModelInstance::GetOutputTensorDescs() const
		{
			return Session->GetOutputTensorDescs();
		}

		TConstArrayView<UE::NNE::FTensorShape> FModelInstance::GetInputTensorShapes() const
		{
			return Session->GetInputTensorShapes();
		}

		TConstArrayView<UE::NNE::FTensorShape> FModelInstance::GetOutputTensorShapes() const
		{
			return Session->GetOutputTensorShapes();
		}

		FModelInstance::ESetInputTensorShapesStatus FModelInstance::SetInputTensorShapes(TConstArrayView<UE::NNE::FTensorShape> InInputShapes)
		{
			return Session->SetInputTensorShapes(InInputShapes);
		}

		FModelInstance::ERunSyncStatus FModelInstance::RunSync(TConstArrayView<UE::NNE::FTensorBindingCPU> InInputBindings, TConstArrayView<UE::NNE::FTensorBindingCPU> InOutputBindings)
		{
			return Session->RunSyncCPU(InInputBindings, InOutputBindings);
		}

		FModel::FModel(TSharedRef<UE::NNERuntimeIREE::CPU::Private::FDevice> InDevice, TSharedRef<UE::NNERuntimeIREE::Private::FModule> InModule) : Device(InDevice), Module(InModule)
		{

		}

		TSharedPtr<FModel> FModel::Make(const FString& InDirPath, const FString& InSharedLibraryFileName, const FString& InVmfbFileName, const FString& InLibraryQueryFunctionName, const UNNERuntimeIREEModuleMetaData& InModuleMetaData)
		{
			check(!InSharedLibraryFileName.IsEmpty());
			check(!InVmfbFileName.IsEmpty());
			check(!InLibraryQueryFunctionName.IsEmpty());
			check(!InModuleMetaData.FunctionMetaData.IsEmpty());

			TSharedPtr<Private::FDevice> Device = Private::FDevice::Make(InDirPath, InSharedLibraryFileName, InLibraryQueryFunctionName);
			if (!Device.IsValid())
			{
				return TSharedPtr<FModel>();
			}

			TSharedPtr<UE::NNERuntimeIREE::Private::FModule> Module = UE::NNERuntimeIREE::Private::FModule::Make(InDirPath, InVmfbFileName, InModuleMetaData);
			if (!Module.IsValid())
			{
				return TSharedPtr<FModel>();
			}

			return TSharedPtr<FModel>(new FModel(Device.ToSharedRef(), Module.ToSharedRef()));
		}

		TSharedPtr<UE::NNE::IModelInstanceCPU> FModel::CreateModelInstanceCPU()
		{
			return FModelInstance::Make(Device, Module);
		}
	} // CPU
} // UE::NNERuntimeIREE

#endif // WITH_NNE_RUNTIME_IREE