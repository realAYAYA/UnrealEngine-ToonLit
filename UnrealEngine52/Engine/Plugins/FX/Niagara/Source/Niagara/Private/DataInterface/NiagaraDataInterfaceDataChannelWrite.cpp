// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataInterfaceDataChannelWrite.h"

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "NiagaraCommon.h"
#include "NiagaraShared.h"

#include "NiagaraWorldManager.h"
#include "NiagaraSystemInstance.h"

#include "NiagaraSystemImpl.h"

#include "NiagaraDataChannel.h"
#include "NiagaraDataChannelHandler.h"
#include "NiagaraDataChannelDefinitions.h"


#define LOCTEXT_NAMESPACE "NiagaraDataInterfaceDataChannelWrite"

DECLARE_CYCLE_STAT(TEXT("NDIDataChannelWrite Write"), STAT_NDIDataChannelWrite_Write, STATGROUP_NiagaraDataChannels);
DECLARE_CYCLE_STAT(TEXT("NDIDataChannelWrite Append"), STAT_NDIDataChannelWrite_Append, STATGROUP_NiagaraDataChannels);
DECLARE_CYCLE_STAT(TEXT("NDIDataChannelWrite Tick"), STAT_NDIDataChannelWrite_Tick, STATGROUP_NiagaraDataChannels);
DECLARE_CYCLE_STAT(TEXT("NDIDataChannelWrite PostTick"), STAT_NDIDataChannelWrite_PostTick, STATGROUP_NiagaraDataChannels);

int32 GbDebugDumpWriter = 0;
static FAutoConsoleVariableRef CVarDebugDumpWriterDI(
	TEXT("fx.Niagara.DataChannels.DebugDumpWriterDI"),
	GbDebugDumpWriter,
	TEXT(" \n"),
	ECVF_Default
);

namespace NDIDataChannelWriteLocal
{
	static const TCHAR* CommonShaderFile = TEXT("/Plugin/FX/Niagara/Private/DataChannel/NiagaraDataInterfaceDataChannelCommon.ush");
	static const TCHAR* TemplateShaderFile_Common = TEXT("/Plugin/FX/Niagara/Private/DataChannel/NiagaraDataInterfaceDataChannelTemplateCommon.ush");
	static const TCHAR* TemplateShaderFile_Write = TEXT("/Plugin/FX/Niagara/Private/DataChannel/NiagaraDataInterfaceDataChannelTemplate_Write.ush");
	static const TCHAR* TemplateShaderFile_Append = TEXT("/Plugin/FX/Niagara/Private/DataChannel/NiagaraDataInterfaceDataChannelTemplate_Append.ush");

	static const FName NumName(TEXT("Num"));
	static const FName WriteName(TEXT("Write"));
	static const FName AppendName(TEXT("Append"));

	static const FName SpawnName(TEXT("Spawn"));

	const TCHAR* GetFunctionTemplate(FName FunctionName)
	{
		if (FunctionName == WriteName) return TemplateShaderFile_Write;
		if (FunctionName == AppendName) return TemplateShaderFile_Append;

		return nullptr;
	}
}

//////////////////////////////////////////////////////////////////////////
//FNDIDataChannelWriteCompiledData

bool FNDIDataChannelWriteCompiledData::Init(UNiagaraSystem* System, UNiagaraDataInterfaceDataChannelWrite* OwnerDI)
{
	FunctionInfo.Reset();

	DataLayout.Empty();

	GatherAccessInfo(System, OwnerDI);

	for (FNDIDataChannelFunctionInfo& FuncInfo : FunctionInfo)
	{
		for (FNiagaraVariableBase& Param : FuncInfo.Inputs)
		{
			DataLayout.Variables.AddUnique(Param);
		}
	}

	DataLayout.BuildLayout();

	return true;
}

//FNDIDataChannelWriteCompiledData END
//////////////////////////////////////////////////////////////////////////




/**
 The data channel write interface allows one Niagara System to write out arbitrary data to be later read by some other Niagara System or Game code/BP.

 Currently this is done by writing the data to a local buffer and then copying into a global buffer when the data channel next ticks.
 In the future we may add alternatives to this that allow for less copying etc.
 Though for now this method allows the system to work without any synchronization headaches for the Read/Write or data races accessing a shared buffer concurrently etc.

 Write DIs can also write in "Local" mode, which means their data is defined by whatever they write rather than any predefined

*/
struct FNDIDataChannelWriteInstanceData
{
	/** Pointer to the world DataChannel Channel we'll push our DataChannel into. Can be null if DI is not set to publish it's DataChannel. */
	TWeakObjectPtr<UNiagaraDataChannelHandler> DataChannel;

	FNiagaraDataSet* Data = nullptr;

	bool bNeedsLastFrameData = true;

	TArray<FNDIDataChannel_FuncToDataSetBindingPtr, TInlineAllocator<8>> FunctionToDatSetBindingInfo;

	~FNDIDataChannelWriteInstanceData()
	{
		if (Data && DataChannel.IsValid())
		{
			DataChannel->RemovePublishRequests(Data);
		}

		/** We defer the deletion of the datase to the RT to be sure all in-flight RT commands have finished using it.*/
		ENQUEUE_RENDER_COMMAND(FDeleteContextCommand)(
			[DataChannelDataSet = Data](FRHICommandListImmediate& RHICmdList)
			{
				if (DataChannelDataSet != nullptr)
				{
					delete DataChannelDataSet;
				}
			}
		);
		Data = nullptr;
	}

	bool Init(UNiagaraDataInterfaceDataChannelWrite* Interface, FNiagaraSystemInstance* Instance)
	{
		Data = new FNiagaraDataSet();
		Data->Init(&Interface->GetCompiledData().DataLayout);

		//Grab the correct function binding infos for this DI.
		const FNDIDataChannelCompiledData& CompiledData = Interface->GetCompiledData();
		FunctionToDatSetBindingInfo.Reset(CompiledData.GetFunctionInfo().Num());
		for (const FNDIDataChannelFunctionInfo& FuncInfo : CompiledData.GetFunctionInfo())
		{
			FunctionToDatSetBindingInfo.Add(FNDIDataChannelLayoutManager::Get().GetLayoutInfo(FuncInfo, Data->GetCompiledData()));
		}

		return true;
	}

	bool Tick(UNiagaraDataInterfaceDataChannelWrite* Interface, FNiagaraSystemInstance* Instance)
	{
		if (Interface->ShouldPublish())
		{
			UNiagaraDataChannelHandler* DataChannelPtr = DataChannel.Get();
			if (DataChannelPtr == nullptr)
			{
				UWorld* World = Instance->GetWorld();
				if (FNiagaraWorldManager* WorldMan = FNiagaraWorldManager::Get(World))
				{
					if (UNiagaraDataChannelHandler* NewChannelHandler = WorldMan->GetDataChannelManager().FindDataChannelHandler(Interface->Channel.ChannelName))
					{
						DataChannelPtr = NewChannelHandler;
						DataChannel = NewChannelHandler;
					}
					else
					{
						UE_LOG(LogNiagara, Warning, TEXT("Failed to find or add Naigara DataChannel Channel: %s"), *Interface->Channel.ChannelName.ToString());
						return false;
					}
				}
			}
		}

		if(FNiagaraDataBuffer* CurrBuff = Data->GetCurrentData())
		{	
			if (bNeedsLastFrameData)
			{
				CurrBuff->AddReadRef();//Ensure we keep the previous data when grabbing a new dest buffer.
			}

			FNiagaraDataBuffer& DestBuff = Data->BeginSimulate(true);

			if (bNeedsLastFrameData && CurrBuff)
			{
				CurrBuff->ReleaseReadRef();
			}
		}

		if (Interface->AllocationMode == ENiagaraDataChannelAllocationMode::Static)
		{
			Data->GetDestinationData()->Allocate(Interface->AllocationCount);
		}
		//else if (Interface->AllocationMode == ENiagaraDataChannelAllocationMode::PerInstance)
		//{
			//TODO: 
			//Have to count up the current size of all users of this DI.
			//Can this be done in DI tick? Or do we need a pre stage API once we do sim stages on CPU?
			//Also adds extra wrinkle if we have multiple stages or emitters using the same writer DI. 
			//Maybe need a pre pass to gather all users and their allocation sizes this frame.
		//}
		else
		{
			check(0);
		}

		//Verify our function info.
		if(!ensure(Interface->GetCompiledData().GetFunctionInfo().Num() == FunctionToDatSetBindingInfo.Num()))
		{
			UE_LOG(LogNiagara, Warning, TEXT("Invalid Bindings for Niagara Data Interface Data Channel Write: %s"), *Interface->Channel.ChannelName.ToString());
			return false;			
		}

		for(auto& Binding : FunctionToDatSetBindingInfo)
		{
			if(Binding.IsValid() == false)
			{
				UE_LOG(LogNiagara, Warning, TEXT("Invalid Bindings for Niagara Data Interface Data Channel Write: %s"), *Interface->Channel.ChannelName.ToString());
				return false;			
			}
		}

		return true;
	}

	bool PostTick(UNiagaraDataInterfaceDataChannelWrite* Interface, FNiagaraSystemInstance* Instance)
	{
		if (Data && Data->GetDestinationData())
		{
			Data->EndSimulate();

			if (GbDebugDumpWriter)
			{
				FNiagaraDataBuffer& Buffer = Data->GetCurrentDataChecked();
				Buffer.Dump(0, Buffer.GetNumInstances(), FString::Printf(TEXT("=== Data Channle Write: %d Elements --> %s ==="), Buffer.GetNumInstances(), *Interface->Channel.ChannelName.ToString()));
			}

			if (Interface->ShouldPublish() && Data->GetCurrentData()->GetNumInstances() > 0)
			{
				if (UNiagaraDataChannelHandler* Channel = DataChannel.Get())
				{
					FNiagaraDataChannelPublishRequest PublishRequest(Data->GetCurrentData());
					PublishRequest.bVisibleToGame = Interface->bPublishToGame;
					PublishRequest.bVisibleToCPUSims = Interface->bPublishToCPU;
					PublishRequest.bVisibleToGPUSims = Interface->bPublishToGPU;
					PublishRequest.Data = Data->GetCurrentData();
					PublishRequest.LwcTile = Instance->GetLWCTile();
					Channel->Publish(PublishRequest);
				}
			}
		}
		return true;
	}
};

bool UNiagaraDataInterfaceDataChannelWrite::InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)
{
	FNDIDataChannelWriteInstanceData* InstanceData = new (PerInstanceData) FNDIDataChannelWriteInstanceData;

	//If data channels are disabled we just skip and return ok so that systems can continue to function.
	if (INiagaraModule::DataChannelsEnabled() == false)
	{
		return false;
	}

	if (InstanceData->Init(this, SystemInstance) == false)
	{
		return false;
	}

	return true;
}

void UNiagaraDataInterfaceDataChannelWrite::DestroyPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)
{
	FNDIDataChannelWriteInstanceData* InstanceData = static_cast<FNDIDataChannelWriteInstanceData*>(PerInstanceData);
	InstanceData->~FNDIDataChannelWriteInstanceData();

	// 	ENQUEUE_RENDER_COMMAND(RemoveProxy)
	// 	(
	// 		[RT_Proxy = GetProxyAs<FNDIDataChannelProxy>(), InstanceID = SystemInstance->GetId()](FRHICommandListImmediate& CmdList)
	// 		{
	// 			RT_Proxy->SystemInstancesToInstanceData_RT.Remove(InstanceID);
	// 		}
	// 	);
}


UNiagaraDataInterfaceDataChannelWrite::UNiagaraDataInterfaceDataChannelWrite(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
{
	//TODO:
	//Proxy.Reset(new FNDIDataChannelProxy());
}

void UNiagaraDataInterfaceDataChannelWrite::PostInitProperties()
{
	Super::PostInitProperties();

	if (HasAnyFlags(RF_ClassDefaultObject) && INiagaraModule::DataChannelsEnabled())
	{
		ENiagaraTypeRegistryFlags Flags = ENiagaraTypeRegistryFlags::AllowAnyVariable | ENiagaraTypeRegistryFlags::AllowParameter;
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()), Flags);
	}
}


int32 UNiagaraDataInterfaceDataChannelWrite::PerInstanceDataSize() const
{
	return sizeof(FNDIDataChannelWriteInstanceData);
}

bool UNiagaraDataInterfaceDataChannelWrite::PerInstanceTick(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float DeltaSeconds)
{
	if (INiagaraModule::DataChannelsEnabled() == false)
	{
		return true;
	}
	
	SCOPE_CYCLE_COUNTER(STAT_NDIDataChannelWrite_Tick);
	check(SystemInstance);
	FNDIDataChannelWriteInstanceData* InstanceData = static_cast<FNDIDataChannelWriteInstanceData*>(PerInstanceData);
	if (!InstanceData)
	{
		return true;
	}

	if (InstanceData->Tick(this, SystemInstance) == false)
	{
		return true;
	}

	return false;
}

bool UNiagaraDataInterfaceDataChannelWrite::PerInstanceTickPostSimulate(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float DeltaSeconds)
{
	if (INiagaraModule::DataChannelsEnabled() == false)
	{
		return true;
	}

	SCOPE_CYCLE_COUNTER(STAT_NDIDataChannelWrite_PostTick);
	check(SystemInstance);
	FNDIDataChannelWriteInstanceData* InstanceData = static_cast<FNDIDataChannelWriteInstanceData*>(PerInstanceData);
	if (!InstanceData)
	{
		return true;
	}

	if (InstanceData->PostTick(this, SystemInstance) == false)
	{
		return true;
	}
	return false;
}

void UNiagaraDataInterfaceDataChannelWrite::ProvidePerInstanceDataForRenderThread(void* DataForRenderThread, void* PerInstanceData, const FNiagaraSystemInstanceID& SystemInstance)
{
	//TODO:
	//FNDIDataChannelProxy::ProvidePerInstanceDataForRenderThread(DataForRenderThread, PerInstanceData, SystemInstance);
}

#if WITH_EDITORONLY_DATA

void UNiagaraDataInterfaceDataChannelWrite::PostCompile()
{
	UNiagaraSystem* OwnerSystem = GetTypedOuter<UNiagaraSystem>();
	CompiledData.Init(OwnerSystem, this);

	if (const UNiagaraDataChannel* DataChannel = UNiagaraDataChannelDefinitions::FindDataChannel(Channel.ChannelName))
	{
		OwnerSystem->RegisterDataChannelUse(DataChannel);
	}
}

#endif

bool UNiagaraDataInterfaceDataChannelWrite::Equals(const UNiagaraDataInterface* Other)const
{
	if (const UNiagaraDataInterfaceDataChannelWrite* OtherTyped = CastChecked<UNiagaraDataInterfaceDataChannelWrite>(Other))
	{
		if (Super::Equals(Other) &&
			AllocationMode == OtherTyped->AllocationMode &&
			AllocationCount == OtherTyped->AllocationCount &&
			bPublishToGame == OtherTyped->bPublishToGame &&
			bPublishToCPU == OtherTyped->bPublishToCPU &&
			bPublishToGPU == OtherTyped->bPublishToGPU &&
			Channel.ChannelName == OtherTyped->Channel.ChannelName)
		{
			return true;
		}
	}
	return false;
}

bool UNiagaraDataInterfaceDataChannelWrite::CopyToInternal(UNiagaraDataInterface* Destination)const
{
	if (!Super::CopyToInternal(Destination))
	{
		return false;
	}

	if (UNiagaraDataInterfaceDataChannelWrite* DestTyped = CastChecked<UNiagaraDataInterfaceDataChannelWrite>(Destination))
	{
		DestTyped->AllocationMode = AllocationMode;
		DestTyped->AllocationCount = AllocationCount;
		DestTyped->bPublishToGame = bPublishToGame;
		DestTyped->bPublishToCPU = bPublishToCPU;
		DestTyped->bPublishToGPU = bPublishToGPU;
		DestTyped->Channel.ChannelName = Channel.ChannelName;
		DestTyped->CompiledData = CompiledData;
		return true;
	}

	return false;
}

void UNiagaraDataInterfaceDataChannelWrite::GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)
{
	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = NDIDataChannelWriteLocal::NumName;
#if WITH_EDITORONLY_DATA
		Sig.Description = LOCTEXT("NumFunctionDescription", "Returns the current number of DataChannel accessible by this interface.");
#endif
		Sig.bMemberFunction = true;
		Sig.bExperimental = true;
		Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("DataChannel interface")));
		Sig.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Num")));
		OutFunctions.Add(Sig);
	}
	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = NDIDataChannelWriteLocal::WriteName;
#if WITH_EDITORONLY_DATA
		Sig.Description = LOCTEXT("WriteFunctionDescription", "Writes DataChannel data at a specific index.  Values in the DataChannel that are not written here are set to their defaults. Returns success if an DataChannel was written to.");
#endif
		Sig.bMemberFunction = true;
		Sig.bRequiresExecPin = true;
		Sig.bExperimental = true;
		Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("DataChannel interface")));
		Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("Emit")));
		Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Index")));
		Sig.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("Success")));
		Sig.RequiredInputs = Sig.Inputs.Num();//The user defines what we write in the graph.
		OutFunctions.Add(Sig);
	}
	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = NDIDataChannelWriteLocal::AppendName;
#if WITH_EDITORONLY_DATA
		Sig.Description = LOCTEXT("AppendFunctionDescription", "Appends a new DataChannel to the end of the DataChannel array and writes the specified values. Values in the DataChannel that are not written here are set to their defaults. Returns success if an DataChannel was successfully pushed.");
#endif
		Sig.bMemberFunction = true;
		Sig.bRequiresExecPin = true;
		Sig.bExperimental = true;
		Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("DataChannel interface")));
		Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("Emit")));
		Sig.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("Success")));
		Sig.RequiredInputs = Sig.Inputs.Num();//The user defines what we write in the graph.
		OutFunctions.Add(Sig);
	}
}

void UNiagaraDataInterfaceDataChannelWrite::GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction& OutFunc)
{
	if (BindingInfo.Name == NDIDataChannelWriteLocal::NumName)
	{
		OutFunc = FVMExternalFunction::CreateLambda([this](FVectorVMExternalFunctionContext& Context) { this->Num(Context); });
	}
	else
	{
		int32 FuncIndex = CompiledData.FindFunctionInfoIndex(BindingInfo.Name, BindingInfo.VariadicInputs, BindingInfo.VariadicOutputs);

		if (FuncIndex != INDEX_NONE)
		{
			if (BindingInfo.Name == NDIDataChannelWriteLocal::WriteName)
			{
				OutFunc = FVMExternalFunction::CreateLambda([this, FuncIndex](FVectorVMExternalFunctionContext& Context) { this->Write(Context, FuncIndex); });
			}
			else if (BindingInfo.Name == NDIDataChannelWriteLocal::AppendName)
			{
				OutFunc = FVMExternalFunction::CreateLambda([this, FuncIndex](FVectorVMExternalFunctionContext& Context) { this->Append(Context, FuncIndex); });
			}
			else
			{
				UE_LOG(LogTemp, Display, TEXT("Could not find data interface external function in %s. Received Name: %s"), *GetPathNameSafe(this), *BindingInfo.Name.ToString());
			}
		}
		else
		{
			UE_LOG(LogTemp, Display, TEXT("Could not find data interface external function in %s. Received Name: %s"), *GetPathNameSafe(this), *BindingInfo.Name.ToString());
		}
	}
}

/** Handles any number of variadic parameter inputs. */
template<int32 EXPECTED_NUM_INPUTS>
struct FNDIVariadicInputHandler
{
	TArray<FNDIInputParam<float>, TInlineAllocator<EXPECTED_NUM_INPUTS>> FloatInputs;
	TArray<FNDIInputParam<int32>, TInlineAllocator<EXPECTED_NUM_INPUTS>> IntInputs;
	TArray<FNDIInputParam<FFloat16>, TInlineAllocator<EXPECTED_NUM_INPUTS>> HalfInputs;

	FNDIVariadicInputHandler(FVectorVMExternalFunctionContext& Context, const FNDIDataChannel_FuncToDataSetBindingPtr& BindingPtr)
	{
		FloatInputs.Reserve(BindingPtr->NumFloatComponents);
		IntInputs.Reserve(BindingPtr->NumInt32Components);
		HalfInputs.Reserve(BindingPtr->NumHalfComponents);
		for (FNDIDataChannelRegisterBinding& VMBinding : BindingPtr->VMRegisterBindings)
		{
			if (VMBinding.DataType == (int32)ENiagaraBaseTypes::Float)
			{
				FloatInputs.Emplace(Context);
			}
			else if (VMBinding.DataType == (int32)ENiagaraBaseTypes::Int32 || VMBinding.DataType == (int32)ENiagaraBaseTypes::Bool)
			{
				IntInputs.Emplace(Context);
			}
			else if (VMBinding.DataType == (int32)ENiagaraBaseTypes::Half)
			{
				HalfInputs.Emplace(Context);
			}
			else
			{
				checkf(false, TEXT("Didn't find a binding for this function input. Likely an error in the building of the binding data and the Float/Int/Half Bindings arrays."));
			}
		}
	}

	void Advance()
	{
		for (FNDIInputParam<float>& Input : FloatInputs) { Input.Advance(); }
		for (FNDIInputParam<int32>& Input : IntInputs) { Input.Advance(); }
		for (FNDIInputParam<FFloat16>& Input : HalfInputs) { Input.Advance(); }
	}

	bool Process(FNiagaraDataBuffer* Data, uint32 Index, const FNDIDataChannel_FuncToDataSetBindingPtr& BindingInfo)
	{
		if (Data && BindingInfo)
		{
			//TODO: Optimize for long runs of writes to reduce binding/lookup overhead.
			if (Index < Data->GetNumInstances())
			{
				for (FNDIDataChannelRegisterBinding& VMBinding : BindingInfo->VMRegisterBindings)
				{
					if (VMBinding.DataType == (int32)ENiagaraBaseTypes::Float)
					{
						if (VMBinding.DataSetRegisterIndex != INDEX_NONE)
						{
							*Data->GetInstancePtrFloat(VMBinding.DataSetRegisterIndex, (uint32)Index) = FloatInputs[VMBinding.FunctionRegisterIndex].Get();
						}
					}
					else if (VMBinding.DataType == (int32)ENiagaraBaseTypes::Int32 || VMBinding.DataType == (int32)ENiagaraBaseTypes::Bool)
					{
						if (VMBinding.DataSetRegisterIndex != INDEX_NONE)
						{
							*Data->GetInstancePtrInt32(VMBinding.DataSetRegisterIndex, (uint32)Index) = IntInputs[VMBinding.FunctionRegisterIndex].Get();
						}
					}
					else if (VMBinding.DataType == (int32)ENiagaraBaseTypes::Half)
					{
						if (VMBinding.DataSetRegisterIndex != INDEX_NONE)
						{
							*Data->GetInstancePtrHalf(VMBinding.DataSetRegisterIndex, (uint32)Index) = HalfInputs[VMBinding.FunctionRegisterIndex].Get();
						}
					}
					else
					{
						checkf(false, TEXT("Didn't find a binding for this function input. Likely an error in the building of the binding data and the Float/Int/Half Bindings arrays."));
					}
				}

				return true;
			}
		}
		return false;
	}
};

void UNiagaraDataInterfaceDataChannelWrite::Num(FVectorVMExternalFunctionContext& Context)
{
	VectorVM::FUserPtrHandler<FNDIDataChannelWriteInstanceData> InstData(Context);

	FNDIOutputParam<int32> OutNum(Context);

	FNiagaraDataBuffer* Buffer = InstData->Data->GetDestinationData();	
	int32 Num = 0;
	if (Buffer && INiagaraModule::DataChannelsEnabled())
	{
		Num = (int32)Buffer->GetNumInstances();
	}

	for (int32 i = 0; i < Context.GetNumInstances(); ++i)
	{
		OutNum.SetAndAdvance(Num);
	}
}

void UNiagaraDataInterfaceDataChannelWrite::Write(FVectorVMExternalFunctionContext& Context, int32 FuncIdx)
{
	SCOPE_CYCLE_COUNTER(STAT_NDIDataChannelWrite_Write);
	VectorVM::FUserPtrHandler<FNDIDataChannelWriteInstanceData> InstData(Context);
	FNDIInputParam<bool> InEmit(Context);
	FNDIInputParam<int32> InIndex(Context);

	const FNDIDataChannelFunctionInfo& FuncInfo = CompiledData.GetFunctionInfo()[FuncIdx];
	const FNDIDataChannel_FuncToDataSetBindingPtr BindingInfo = InstData->FunctionToDatSetBindingInfo.IsValidIndex(FuncIdx) ? InstData->FunctionToDatSetBindingInfo[FuncIdx] : nullptr;
	FNDIVariadicInputHandler<16> VariadicInputs(Context, BindingInfo);//TODO: Make static / avoid allocation

	FNDIOutputParam<bool> OutSuccess(Context);

	if (InstData->Data && INiagaraModule::DataChannelsEnabled())
	{
		FNiagaraDataBuffer* Buffer = InstData->Data->GetDestinationData();

		for (int32 i = 0; i < Context.GetNumInstances(); ++i)
		{
			int32 Index = InIndex.GetAndAdvance();

			bool bEmit = InEmit.GetAndAdvance();

			bool bSuccess = false;

			//TODO: Optimize case where emit is constant
			//TODO: Optimize for runs of sequential true emits.
			if (bEmit)
			{
				bSuccess = VariadicInputs.Process(Buffer, Index, BindingInfo);
			}

			if (OutSuccess.IsValid())
			{
				OutSuccess.SetAndAdvance(bSuccess);
			}

			VariadicInputs.Advance();
		}
	}
	else
	{
		for (int32 i = 0; i < Context.GetNumInstances(); ++i)
		{
			if (OutSuccess.IsValid())
			{
				OutSuccess.SetAndAdvance(false);
			}
		}
	}
}

void UNiagaraDataInterfaceDataChannelWrite::Append(FVectorVMExternalFunctionContext& Context, int32 FuncIdx)
{
	SCOPE_CYCLE_COUNTER(STAT_NDIDataChannelWrite_Append);
	VectorVM::FUserPtrHandler<FNDIDataChannelWriteInstanceData> InstData(Context);
	FNDIInputParam<bool> InEmit(Context);

	const FNDIDataChannelFunctionInfo& FuncInfo = CompiledData.GetFunctionInfo()[FuncIdx];
	const FNDIDataChannel_FuncToDataSetBindingPtr BindingInfo = InstData->FunctionToDatSetBindingInfo.IsValidIndex(FuncIdx) ? InstData->FunctionToDatSetBindingInfo[FuncIdx] : nullptr;
	FNDIVariadicInputHandler<16> VariadicInputs(Context, BindingInfo);//TODO: Make static / avoid allocation

	FNDIOutputParam<bool> OutSuccess(Context);

	if (InstData->Data && INiagaraModule::DataChannelsEnabled())
	{
		FNiagaraDataBuffer* Buffer = InstData->Data->GetDestinationData();
		for (int32 i = 0; i < Context.GetNumInstances(); ++i)
		{
			bool bEmit = InEmit.GetAndAdvance();

			bool bSuccess = false;

			//TODO: Optimize case where emit is constant
			//TODO: Optimize for runs of sequential true emits.
			if (bEmit && Buffer)
			{
				int32 Num = Buffer->GetNumInstances();
				if ((int32)Buffer->GetNumInstancesAllocated() > Num)
				{
					Buffer->SetNumInstances(Num + 1);
					bSuccess = VariadicInputs.Process(Buffer, Num, BindingInfo);
				}
			}

			if (OutSuccess.IsValid())
			{
				OutSuccess.SetAndAdvance(bSuccess);
			}

			VariadicInputs.Advance();
		}
	}
	else
	{
		for (int32 i = 0; i < Context.GetNumInstances(); ++i)
		{
			if (OutSuccess.IsValid())
			{
				OutSuccess.SetAndAdvance(false);
			}
		}
	}
}

#if WITH_EDITORONLY_DATA
bool UNiagaraDataInterfaceDataChannelWrite::AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const
{
	//TODO: GPU Writes.
	// 
// 	bool bSuccess = Super::AppendCompileHash(InVisitor);
// 	bSuccess &= InVisitor->UpdateString(TEXT("UNiagaraDataInterfaceDataChannelCommon"), GetShaderFileHash(NDIDataChannelWriteLocal::CommonShaderFile, EShaderPlatform::SP_PCD3D_SM5).ToString());
// 	bSuccess &= InVisitor->UpdateString(TEXT("UNiagaraDataInterfaceDataChannelWrite_Common"), GetShaderFileHash(NDIDataChannelWriteLocal::TemplateShaderFile_Common, EShaderPlatform::SP_PCD3D_SM5).ToString());
// 	bSuccess &= InVisitor->UpdateString(TEXT("UNiagaraDataInterfaceDataChannelWrite_Write"), GetShaderFileHash(NDIDataChannelWriteLocal::TemplateShaderFile_Write, EShaderPlatform::SP_PCD3D_SM5).ToString());
// 	bSuccess &= InVisitor->UpdateString(TEXT("UNiagaraDataInterfaceDataChannelWrite_Append"), GetShaderFileHash(NDIDataChannelWriteLocal::TemplateShaderFile_Append, EShaderPlatform::SP_PCD3D_SM5).ToString());
// 
// 	bSuccess &= InVisitor->UpdateShaderParameters<NDIDataChannelWriteLocal::FShaderParameters>();
// 	return bSuccess;

	return false;
}

void UNiagaraDataInterfaceDataChannelWrite::GetCommonHLSL(FString& OutHLSL)
{
	//TODO: GPU Writes.
	// 
// 	Super::GetCommonHLSL(OutHLSL);
// 	OutHLSL.Append(TEXT("\n//Niagara Data Channel Write Interface Common Code.\n"));
// 	OutHLSL.Appendf(TEXT("#include \"%s\"\n"), NDIDataChannelWriteLocal::CommonShaderFile);
}

bool UNiagaraDataInterfaceDataChannelWrite::GetFunctionHLSL(FNiagaraDataInterfaceHlslGenerationContext& HlslGenContext, FString& OutHLSL)
{
	//TODO: GPU Writes.
	// 
// 	return	HlslGenContext.GetFunctionInfo().DefinitionName == GET_FUNCTION_NAME_CHECKED(UNiagaraDataInterfaceDataChannelWrite, Num) ||
// 		HlslGenContext.GetFunctionInfo().DefinitionName == GET_FUNCTION_NAME_CHECKED(UNiagaraDataInterfaceDataChannelWrite, Write) ||
// 		HlslGenContext.GetFunctionInfo().DefinitionName == GET_FUNCTION_NAME_CHECKED(UNiagaraDataInterfaceDataChannelWrite, Append);

	return false;
}

void UNiagaraDataInterfaceDataChannelWrite::GetParameterDefinitionHLSL(FNiagaraDataInterfaceHlslGenerationContext& HlslGenContext, FString& OutHLSL)
{
	Super::GetParameterDefinitionHLSL(HlslGenContext, OutHLSL);

	//TODO: GPU Writes. 
	//TODO: ADD VARIADIC PARAM HANDLING SIMILAR TO READ DI
}

#endif
void UNiagaraDataInterfaceDataChannelWrite::BuildShaderParameters(FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const
{
	//ShaderParametersBuilder.AddNestedStruct<FShaderParameters>();
}
void UNiagaraDataInterfaceDataChannelWrite::SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const
{
}

#undef LOCTEXT_NAMESPACE