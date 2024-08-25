// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataInterface/NiagaraDataInterfaceDataChannelWrite.h"

#include "NiagaraModule.h"
#include "Stats/Stats.h"
#include "NiagaraCommon.h"
#include "NiagaraShared.h"

#include "NiagaraSimCache.h"
#include "NiagaraSystem.h"
#include "NiagaraWorldManager.h"
#include "NiagaraSystemInstance.h"

#include "NiagaraDataChannel.h"
#include "NiagaraDataChannelHandler.h"
#include "NiagaraDataChannelManager.h"

#if WITH_EDITOR
#include "INiagaraEditorOnlyDataUtlities.h"
#include "Modules/ModuleManager.h"
#endif

#define LOCTEXT_NAMESPACE "NiagaraDataInterfaceDataChannelWrite"

DECLARE_CYCLE_STAT(TEXT("NDIDataChannelWrite Write"), STAT_NDIDataChannelWrite_Write, STATGROUP_NiagaraDataChannels);
DECLARE_CYCLE_STAT(TEXT("NDIDataChannelWrite Append"), STAT_NDIDataChannelWrite_Append, STATGROUP_NiagaraDataChannels);
DECLARE_CYCLE_STAT(TEXT("NDIDataChannelWrite Tick"), STAT_NDIDataChannelWrite_Tick, STATGROUP_NiagaraDataChannels);
DECLARE_CYCLE_STAT(TEXT("NDIDataChannelWrite PostTick"), STAT_NDIDataChannelWrite_PostTick, STATGROUP_NiagaraDataChannels);
DECLARE_CYCLE_STAT(TEXT("NDIDataChannelWrite PreStageTick"), STAT_NDIDataChannelWrite_PreStageTick, STATGROUP_NiagaraDataChannels);
DECLARE_CYCLE_STAT(TEXT("NDIDataChannelWrite PostStageTick"), STAT_NDIDataChannelWrite_PostStageTick, STATGROUP_NiagaraDataChannels);

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

	//Shared pointer to the actual data we'll be pushing into for this data channel.
	FNiagaraDataChannelDataPtr DataChannelData;

	/** Local dataset we write into. 
	TODO: alternate write modes.
		- Crit sec access to the data channel buffer and write direct?
		- 
	*/
	FNiagaraDataSet* Data = nullptr;

	TArray<FNDIDataChannel_FuncToDataSetBindingPtr, TInlineAllocator<8>> FunctionToDatSetBindingInfo;

	//Atomic uint for tracking num instances of the target data buffer when writing from multiple threads in the VM.
	std::atomic<uint32> AtomicNumInstances;

	~FNDIDataChannelWriteInstanceData()
	{
		if (Data && DataChannelData.IsValid())
		{
			DataChannelData->RemovePublishRequests(Data);
		}

		/** We defer the deletion of the dataset to the RT to be sure all in-flight RT commands have finished using it.*/
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

		//In non test/shipping builds we gather and log and missing parameters that cause us to fail to find correct bindings.
		TArray<FNiagaraVariableBase> MissingParams;

		//Grab the correct function binding infos for this DI.
		const FNDIDataChannelCompiledData& CompiledData = Interface->GetCompiledData();
		FunctionToDatSetBindingInfo.Reset(CompiledData.GetFunctionInfo().Num());
		for (const FNDIDataChannelFunctionInfo& FuncInfo : CompiledData.GetFunctionInfo())
		{
			FunctionToDatSetBindingInfo.Add(FNDIDataChannelLayoutManager::Get().GetLayoutInfo(FuncInfo, Data->GetCompiledData(), MissingParams));			
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
					if (UNiagaraDataChannelHandler* NewChannelHandler = WorldMan->GetDataChannelManager().FindDataChannelHandler(Interface->Channel))
					{
						DataChannelPtr = NewChannelHandler;
						DataChannel = NewChannelHandler;

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
						//In non test/shipping builds we gather and log and missing parameters that cause us to fail to find correct bindings.
						TArray<FNiagaraVariableBase> MissingParams;
						const FNDIDataChannelCompiledData& CompiledData = Interface->GetCompiledData();
						for (const FNDIDataChannelFunctionInfo& FuncInfo : CompiledData.GetFunctionInfo())
						{
							FNDIDataChannelLayoutManager::Get().GetLayoutInfo(FuncInfo, DataChannelPtr->GetDataChannel()->GetCompiledData(ENiagaraSimTarget::CPUSim), MissingParams);
						}

						if (MissingParams.Num() > 0)
						{
							FString MissingParamsString;
							for (FNiagaraVariableBase& MissingParam : MissingParams)
							{
								MissingParamsString += FString::Printf(TEXT("%s %s\n"), *MissingParam.GetType().GetName(), *MissingParam.GetName().ToString());
							}

							UE_LOG(LogNiagara, Warning, TEXT("Niagara Data Channel Writer Interface is trying to write parameters that do not exist in this channel.\nIt's likely that the Data Channel Definition has been changed and this system needs to be updated.\nData Channel: %s\nSystem: %s\nComponent:%s\nMissing Parameters:\n%s\n")
								, *DataChannel->GetDataChannel()->GetName()
								, *Instance->GetSystem()->GetPathName()
								, *Instance->GetAttachComponent()->GetPathName()
								, *MissingParamsString);
						}
#endif
					}
					else
					{
						UE_LOG(LogNiagara, Warning, TEXT("Failed to find or add Naigara DataChannel Channel: %s"), *Interface->Channel.GetName());
						return false;
					}
				}
			}

			if (DataChannelPtr)
			{
				if (DataChannelData == nullptr || Interface->bUpdateDestinationDataEveryTick)
				{
					FNiagaraDataChannelSearchParameters SearchParams(Instance->GetAttachComponent());
					DataChannelData = DataChannelPtr->FindData(SearchParams, ENiagaraResourceAccess::WriteOnly);
				}
			}
		}		

		//Verify our function info.
		if(!ensure(Interface->GetCompiledData().GetFunctionInfo().Num() == FunctionToDatSetBindingInfo.Num()))
		{
			UE_LOG(LogNiagara, Warning, TEXT("Invalid Bindings for Niagara Data Interface Data Channel Write: %s"), *Interface->Channel.GetName());
			return false;			
		}

		for(const auto& Binding : FunctionToDatSetBindingInfo)
		{
			if(Binding.IsValid() == false)
			{
				UE_LOG(LogNiagara, Warning, TEXT("Invalid Bindings for Niagara Data Interface Data Channel Write: %s"), *Interface->Channel.GetName());
				return false;			
			}
		}

		return true;
	}

	void PreStageTick(UNiagaraDataInterfaceDataChannelWrite* Interface, FNiagaraSystemInstance* Instance)
	{
		if (FNiagaraDataBuffer* CurrBuff = Data->GetCurrentData())
		{
			FNiagaraDataBuffer& DestBuff = Data->BeginSimulate(true);
		}

		//TODO: Currently allocating for every stage using this DI whether it's a write or not. We should limit this to write functions.
		AtomicNumInstances = 0;
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
	}

	void PostStageTick(UNiagaraDataInterfaceDataChannelWrite* Interface, FNiagaraSystemInstance* Instance)
	{
		if (Data && Data->GetDestinationData())
		{
			//The count here can overrun the num allocated but we should never actually write beyond the max allocated.
			uint32 WrittenInstances = AtomicNumInstances.load(std::memory_order_seq_cst);
			WrittenInstances = FMath::Min(WrittenInstances, Data->GetDestinationData()->GetNumInstancesAllocated());
			Data->GetDestinationData()->SetNumInstances(WrittenInstances);
			Data->EndSimulate();

			if (GbDebugDumpWriter)
			{
				FNiagaraDataBuffer& Buffer = Data->GetCurrentDataChecked();
				Buffer.Dump(0, Buffer.GetNumInstances(), FString::Printf(TEXT("=== Data Channle Write: %d Elements --> %s ==="), Buffer.GetNumInstances(), *Interface->Channel.GetName()));
			}

			if (DataChannelData && Interface->ShouldPublish() && Data->GetCurrentData()->GetNumInstances() > 0)
			{
				FNiagaraDataChannelPublishRequest PublishRequest(Data->GetCurrentData());
				PublishRequest.bVisibleToGame = Interface->bPublishToGame;
				PublishRequest.bVisibleToCPUSims = Interface->bPublishToCPU;
				PublishRequest.bVisibleToGPUSims = Interface->bPublishToGPU;
				PublishRequest.LwcTile = Instance->GetLWCTile();
#if WITH_NIAGARA_DEBUGGER
				PublishRequest.DebugSource = FString::Format(TEXT("{0} ({1})"), {Instance->GetSystem()->GetName(), GetPathNameSafe(Interface)});
#endif
				DataChannelData->Publish(PublishRequest);
			}
		}
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
 	return false;
}

void UNiagaraDataInterfaceDataChannelWrite::PreStageTick(FNDICpuPostStageContext& Context)
{
	if (INiagaraModule::DataChannelsEnabled() == false)
	{
		return;
	}

	SCOPE_CYCLE_COUNTER(STAT_NDIDataChannelWrite_PreStageTick);
	check(Context.SystemInstance);
	FNDIDataChannelWriteInstanceData* InstanceData = static_cast<FNDIDataChannelWriteInstanceData*>(Context.PerInstanceData);
	if (!InstanceData)
	{
		return;
	}

	InstanceData->PreStageTick(this, Context.SystemInstance);
}

void UNiagaraDataInterfaceDataChannelWrite::PostStageTick(FNDICpuPostStageContext& Context)
{
	if (INiagaraModule::DataChannelsEnabled() == false)
	{
		return;
	}

	SCOPE_CYCLE_COUNTER(STAT_NDIDataChannelWrite_PostStageTick);
	check(Context.PerInstanceData);
	FNDIDataChannelWriteInstanceData* InstanceData = static_cast<FNDIDataChannelWriteInstanceData*>(Context.PerInstanceData);
	if (!Context.SystemInstance)
	{
		return;
	}
	
	InstanceData->PostStageTick(this, Context.SystemInstance);
}

void UNiagaraDataInterfaceDataChannelWrite::ProvidePerInstanceDataForRenderThread(void* DataForRenderThread, void* PerInstanceData, const FNiagaraSystemInstanceID& SystemInstance)
{
	//TODO:
	//FNDIDataChannelProxy::ProvidePerInstanceDataForRenderThread(DataForRenderThread, PerInstanceData, SystemInstance);
}

bool UNiagaraDataInterfaceDataChannelWrite::HasTickGroupPostreqs() const
{
	if (Channel && Channel->Get())
	{
		return Channel->Get()->ShouldEnforceTickGroupReadWriteOrder();
	}
	return false;
}

ETickingGroup UNiagaraDataInterfaceDataChannelWrite::CalculateFinalTickGroup(const void* PerInstanceData) const
{
	if(Channel && Channel->Get() && Channel->Get()->ShouldEnforceTickGroupReadWriteOrder())
	{
		return Channel->Get()->GetFinalWriteTickGroup();
	}
	return NiagaraLastTickGroup;
}

#if WITH_EDITORONLY_DATA

void UNiagaraDataInterfaceDataChannelWrite::PostCompile()
{
	UNiagaraSystem* OwnerSystem = GetTypedOuter<UNiagaraSystem>();
	CompiledData.Init(OwnerSystem, this);
}

#endif



#if WITH_EDITOR	

void UNiagaraDataInterfaceDataChannelWrite::GetFeedback(UNiagaraSystem* InAsset, UNiagaraComponent* InComponent, TArray<FNiagaraDataInterfaceError>& OutErrors, TArray<FNiagaraDataInterfaceFeedback>& OutWarnings, TArray<FNiagaraDataInterfaceFeedback>& OutInfo)
{
	INiagaraModule& NiagaraModule = FModuleManager::GetModuleChecked<INiagaraModule>("Niagara");
	const INiagaraEditorOnlyDataUtilities& EditorOnlyDataUtilities = NiagaraModule.GetEditorOnlyDataUtilities();
	UNiagaraDataInterface* RuntimeInstanceOfThis = InAsset && EditorOnlyDataUtilities.IsEditorDataInterfaceInstance(this)
		? EditorOnlyDataUtilities.GetResolvedRuntimeInstanceForEditorDataInterfaceInstance(*InAsset, *this)
		: this;

	UNiagaraDataInterfaceDataChannelWrite* RuntimeDI = Cast<UNiagaraDataInterfaceDataChannelWrite>(RuntimeInstanceOfThis);

	if (!RuntimeDI)
	{
		return;
	}

	Super::GetFeedback(InAsset, InComponent, OutErrors, OutWarnings, OutInfo);

	if (Channel == nullptr)
	{
		OutErrors.Emplace(LOCTEXT("DataChannelMissingFmt", "Data Channel Interface has no valid Data Channel."),
			LOCTEXT("DataChannelMissingErrorSummaryFmt", "Missing Data Channel."),
			FNiagaraDataInterfaceFix());	

		return;
	}

	if(ShouldPublish() == false)
	{
		OutErrors.Emplace(FText::Format(LOCTEXT("DataChannelDoesNotPublishtErrorFmt", "Data Channel {0} does not publish it's data to the Game, CPU Simulations or GPU simulations."), FText::FromName(Channel.GetFName())),
			LOCTEXT("DataChannelDoesNotPublishErrorSummaryFmt", "Data Channel DI does not publish."),
			FNiagaraDataInterfaceFix());
	}

	if (const UNiagaraDataChannel* DataChannel = RuntimeDI->Channel->Get())
	{
		//Ensure the data channel contains all the parameters this function is requesting.
		TConstArrayView<FNiagaraDataChannelVariable> ChannelVars = DataChannel->GetVariables();
		for (const FNDIDataChannelFunctionInfo& FuncInfo : RuntimeDI->GetCompiledData().GetFunctionInfo())
		{
			TArray<FNiagaraVariableBase> MissingParams;

			auto VerifyChannelContainsParams = [&](const TArray<FNiagaraVariableBase>& Parameters)
			{
				for (const FNiagaraVariableBase& FuncParam : Parameters)
				{
					bool bParamFound = false;
					for (const FNiagaraDataChannelVariable& ChannelVar : ChannelVars)
					{
						FNiagaraVariable SWCVar(ChannelVar);

						//We have to convert each channel var to SWC for comparison with the function variables as there is no reliable way to go back from the SWC function var to the originating LWC var.
						if (ChannelVar.GetType().IsEnum() == false)
						{
							UScriptStruct* ChannelSWCStruct = FNiagaraTypeHelper::GetSWCStruct(ChannelVar.GetType().GetScriptStruct());
							if (ChannelSWCStruct)
							{
								FNiagaraTypeDefinition SWCType(ChannelSWCStruct, FNiagaraTypeDefinition::EAllowUnfriendlyStruct::Deny);
								SWCVar = FNiagaraVariable(SWCType, ChannelVar.GetName());
							}
						}

						if (SWCVar == FuncParam)
						{
							bParamFound = true;
							break;
						}
					}

					if (bParamFound == false)
					{
						MissingParams.Add(FuncParam);
					}
				}
			};
			VerifyChannelContainsParams(FuncInfo.Inputs);
			VerifyChannelContainsParams(FuncInfo.Outputs);

			if (MissingParams.Num() > 0)
			{
				FTextBuilder Builder;
				Builder.AppendLineFormat(LOCTEXT("FuncParamMissingFromDataChannelWriteErrorFmt", "Accessing variables that do not exist in Data Channel {0}."), FText::FromName(Channel.GetFName()));
				for (FNiagaraVariableBase& Param : MissingParams)
				{
					Builder.AppendLineFormat(LOCTEXT("FuncParamMissingFromDataChannelWriteErrorLineFmt", "{0} {1}"), Param.GetType().GetNameText(), FText::FromName(Param.GetName()));
				}

				OutErrors.Emplace(Builder.ToText(), LOCTEXT("FuncParamMissingFromDataChannelWriteErrorSummaryFmt", "Data Channel DI function is accessing invalid parameters."), FNiagaraDataInterfaceFix());
			}
		}
	}
	else
	{
		OutErrors.Emplace(FText::Format(LOCTEXT("DataChannelDoesNotExistErrorFmt", "Data Channel {0} does not exist. It may have been deleted."), FText::FromName(Channel.GetFName())),
			LOCTEXT("DataChannelDoesNotExistErrorSummaryFmt", "Data Channel DI is accesssinga a Data Channel that doesn't exist."),
			FNiagaraDataInterfaceFix());
	}
}

void UNiagaraDataInterfaceDataChannelWrite::ValidateFunction(const FNiagaraFunctionSignature& Function, TArray<FText>& OutValidationErrors)
{
	Super::ValidateFunction(Function, OutValidationErrors);

	//It would be great to be able to validate the parameters on the function calls here but this is only called on the DI CDO. We don't have the context of which data channel we'll be accessing.
	//The translator should have all the required data to use the actual DIs when validating functions. We just need to do some wrangling to pull it from the pre compiled data correctly.
	//This would probably also allow us to actually call hlsl generation functions on the actual DIs rather than their CDOs. Which would allow for a bunch of better optimized code gen for things like fluids.
	//TODO!!!
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
			Channel == OtherTyped->Channel &&
			bUpdateDestinationDataEveryTick == OtherTyped->bUpdateDestinationDataEveryTick)
		{
			return true;
		}
	}
	return false;
}

UObject* UNiagaraDataInterfaceDataChannelWrite::SimCacheBeginWrite(UObject* SimCache, FNiagaraSystemInstance* NiagaraSystemInstance, const void* OptionalPerInstanceData, FNiagaraSimCacheFeedbackContext& FeedbackContext) const
{
	return NewObject<UNDIDataChannelWriteSimCacheData>(SimCache);
}

bool UNiagaraDataInterfaceDataChannelWrite::SimCacheWriteFrame(UObject* StorageObject, int FrameIndex, FNiagaraSystemInstance* SystemInstance, const void* OptionalPerInstanceData, FNiagaraSimCacheFeedbackContext& FeedbackContext) const
{
	if (!Channel || !Channel->Get())
	{
		FeedbackContext.Errors.Add(TEXT("Missing data channel asset for data channel writer DI"));
		return false;
	}
	if (OptionalPerInstanceData == nullptr)
	{
		FeedbackContext.Errors.Add(TEXT("Missing per instance data for data channel writer DI"));
		return false;
	}
	// put data from instance data into the storage object
	const FNDIDataChannelWriteInstanceData* InstanceData = static_cast<const FNDIDataChannelWriteInstanceData*>(OptionalPerInstanceData);
	if (UNDIDataChannelWriteSimCacheData* Storage = Cast<UNDIDataChannelWriteSimCacheData>(StorageObject))
	{
		ensure(Storage->FrameData.Num() == FrameIndex);
		Storage->DataChannelReference = Channel.Get();
		FNDIDataChannelWriteSimCacheFrame& FrameData = Storage->FrameData.AddDefaulted_GetRef();
		
		if (InstanceData->DataChannelData && ShouldPublish() && InstanceData->Data && InstanceData->Data->GetCurrentData() && InstanceData->Data->GetCurrentData()->GetNumInstances() > 0)
		{
			FNiagaraDataChannelGameData GameData;
			GameData.Init(Channel->Get());
			GameData.AppendFromDataSet(InstanceData->Data->GetCurrentData(), SystemInstance->GetLWCTile());
			
			FrameData.NumElements = GameData.Num();
			for (const FNiagaraDataChannelVariableBuffer& VarBuffer : GameData.GetVariableBuffers())
			{
				FNDIDataChannelWriteSimCacheFrameBuffer& FrameBuffer = FrameData.VariableData.AddDefaulted_GetRef();
				FrameBuffer.Size = VarBuffer.Size;
				FrameBuffer.Data = VarBuffer.Data;
			}
			const FNiagaraDataChannelGameDataLayout& Layout = Channel->Get()->GetGameDataLayout();
			for (const TPair<FNiagaraVariableBase, int32>& VarPair : Layout.VariableIndices)
			{
				FrameData.VariableData[VarPair.Value].SourceVar = VarPair.Key;
			}
			
			FrameData.bVisibleToGame = bPublishToGame;
			FrameData.bVisibleToCPUSims = bPublishToCPU;
			FrameData.bVisibleToGPUSims = bPublishToGPU;
		}
		return true;
	} 
	return false;
}

bool UNiagaraDataInterfaceDataChannelWrite::SimCacheReadFrame(UObject* StorageObject, int FrameA, int FrameB, float Interp, FNiagaraSystemInstance* SystemInstance, void* OptionalPerInstanceData)
{
	if (Channel == nullptr || Channel->Get() == nullptr)
	{
		return false;
	}

	FNiagaraDataChannelDataPtr DataChannelData;
	if (FNiagaraWorldManager* WorldMan = FNiagaraWorldManager::Get(SystemInstance->GetWorld()))
	{
		if (UNiagaraDataChannelHandler* Handler = WorldMan->GetDataChannelManager().FindDataChannelHandler(Channel->Get()))
		{
			FNiagaraDataChannelSearchParameters SearchParams(SystemInstance->GetAttachComponent());
			DataChannelData = Handler->FindData(SearchParams, ENiagaraResourceAccess::WriteOnly);
		}
	}
	if (!DataChannelData.IsValid())
	{
		return false;
	}
	
	if (UNDIDataChannelWriteSimCacheData* Storage = Cast<UNDIDataChannelWriteSimCacheData>(StorageObject))
	{
		if (Storage->FrameData.IsValidIndex(FrameA))
		{
			FNDIDataChannelWriteSimCacheFrame& Frame = Storage->FrameData[FrameA];
			FNiagaraDataChannelPublishRequest PublishRequest;
			PublishRequest.bVisibleToGame = Frame.bVisibleToGame;
			PublishRequest.bVisibleToCPUSims = Frame.bVisibleToCPUSims;
			PublishRequest.bVisibleToGPUSims = Frame.bVisibleToGPUSims;
#if WITH_NIAGARA_DEBUGGER
			PublishRequest.DebugSource = FString::Format(TEXT("{0} (Sim cache {1})"), {SystemInstance->GetSystem()->GetName(), GetPathNameSafe(StorageObject->GetOuter())});
#endif

			PublishRequest.GameData = MakeShared<FNiagaraDataChannelGameData>();
			PublishRequest.GameData->Init(Channel->Get());
			PublishRequest.GameData->SetNum(Frame.NumElements);
			for (int32 i = 0; i < Frame.VariableData.Num(); i++)
			{
				const FNDIDataChannelWriteSimCacheFrameBuffer& Buffer = Frame.VariableData[i];
				PublishRequest.GameData->SetFromSimCache(Buffer.SourceVar, Buffer.Data, Buffer.Size);
			}
			
			DataChannelData->Publish(PublishRequest);
			return true;
		}
	}
	return false;
}

void UNiagaraDataInterfaceDataChannelWrite::SimCachePostReadFrame(void* OptionalPerInstanceData, FNiagaraSystemInstance* SystemInstance)
{
	// send data to data channel
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
		DestTyped->Channel = Channel;
		DestTyped->CompiledData = CompiledData;
		DestTyped->bUpdateDestinationDataEveryTick = bUpdateDestinationDataEveryTick;
		return true;
	}

	return false;
}

#if WITH_EDITORONLY_DATA
void UNiagaraDataInterfaceDataChannelWrite::GetFunctionsInternal(TArray<FNiagaraFunctionSignature>& OutFunctions) const
{
	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = NDIDataChannelWriteLocal::NumName;
		Sig.Description = LOCTEXT("NumFunctionDescription", "Returns the current number of DataChannel accessible by this interface.");
		Sig.bMemberFunction = true;
		Sig.bExperimental = true;
		Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("DataChannel interface")));
		Sig.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Num")));
		OutFunctions.Add(Sig);
	}

	static FNiagaraVariable EmitVar(FNiagaraTypeDefinition::GetBoolDef(), TEXT("Emit"));
	EmitVar.SetValue(FNiagaraBool(true));

	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = NDIDataChannelWriteLocal::WriteName;
		Sig.Description = LOCTEXT("WriteFunctionDescription", "Writes DataChannel data at a specific index.  Values in the DataChannel that are not written here are set to their defaults. Returns success if an DataChannel was written to.");
		Sig.bMemberFunction = true;
		Sig.bRequiresExecPin = true;
		Sig.bExperimental = true;
		Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("DataChannel interface")));
		Sig.AddInput(EmitVar);
		Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Index")));
		Sig.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("Success")));
		Sig.RequiredInputs = Sig.Inputs.Num();//The user defines what we write in the graph.
		OutFunctions.Add(Sig);
	}
	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = NDIDataChannelWriteLocal::AppendName;
		Sig.Description = LOCTEXT("AppendFunctionDescription", "Appends a new DataChannel to the end of the DataChannel array and writes the specified values. Values in the DataChannel that are not written here are set to their defaults. Returns success if an DataChannel was successfully pushed.");
		Sig.bMemberFunction = true;
		Sig.bRequiresExecPin = true;
		Sig.bExperimental = true;
		Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("DataChannel interface")));
		Sig.AddInput(EmitVar);
		Sig.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("Success")));
		Sig.RequiredInputs = Sig.Inputs.Num();//The user defines what we write in the graph.
		OutFunctions.Add(Sig);
	}
}
#endif

void UNiagaraDataInterfaceDataChannelWrite::GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction& OutFunc)
{
	if (BindingInfo.Name == NDIDataChannelWriteLocal::NumName)
	{
		OutFunc = FVMExternalFunction::CreateLambda([this](FVectorVMExternalFunctionContext& Context) { this->Num(Context); });
	}
	else
	{
		int32 FuncIndex = CompiledData.FindFunctionInfoIndex(BindingInfo.Name, BindingInfo.VariadicInputs, BindingInfo.VariadicOutputs);
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
}

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
	FNDIInputParam<FNiagaraBool> InEmit(Context);
	FNDIInputParam<int32> InIndex(Context);

	const FNDIDataChannelFunctionInfo& FuncInfo = CompiledData.GetFunctionInfo()[FuncIdx];
	const FNDIDataChannel_FunctionToDataSetBinding* BindingInfo = InstData->FunctionToDatSetBindingInfo.IsValidIndex(FuncIdx) ? InstData->FunctionToDatSetBindingInfo[FuncIdx].Get() : nullptr;
	FNDIVariadicInputHandler<16> VariadicInputs(Context, BindingInfo);//TODO: Make static / avoid allocation

	FNDIOutputParam<FNiagaraBool> OutSuccess(Context);

	std::atomic<uint32>& AtomicNumInstances = InstData->AtomicNumInstances;

	bool bAllFailedFallback = true;
	if (InstData->Data && BindingInfo && INiagaraModule::DataChannelsEnabled())
	{
		if(FNiagaraDataBuffer* Data = InstData->Data->GetDestinationData())
		{			
			bAllFailedFallback = false;
			uint32 MaxLocalIndex = 0;
			int32 NumAllocated = (int32)Data->GetNumInstancesAllocated();
			for (int32 i = 0; i < Context.GetNumInstances(); ++i)
			{
				int32 Index = InIndex.GetAndAdvance();
				bool bEmit = InEmit.GetAndAdvance() && Index >= 0 && Index < NumAllocated;

				MaxLocalIndex = bEmit ? FMath::Max((uint32)Index, MaxLocalIndex) : MaxLocalIndex;

				bool bSuccess = false;

				//TODO: Optimize case where emit is constant
				//TODO: Optimize for runs of sequential true emits.
				auto FloatFunc = [Data, Index](const FNDIDataChannelRegisterBinding& VMBinding, FNDIInputParam<float>& FloatData)
				{
					if (VMBinding.DataSetRegisterIndex != INDEX_NONE)
						*Data->GetInstancePtrFloat(VMBinding.DataSetRegisterIndex, (uint32)Index) = FloatData.GetAndAdvance();
				};
				auto IntFunc = [Data, Index](const FNDIDataChannelRegisterBinding& VMBinding, FNDIInputParam<int32>& IntData)
				{
					if (VMBinding.DataSetRegisterIndex != INDEX_NONE)
						*Data->GetInstancePtrInt32(VMBinding.DataSetRegisterIndex, (uint32)Index) = IntData.GetAndAdvance();
				};
				auto HalfFunc = [Data, Index](const FNDIDataChannelRegisterBinding& VMBinding, FNDIInputParam<FFloat16>& HalfData)
				{
					if (VMBinding.DataSetRegisterIndex != INDEX_NONE)
						*Data->GetInstancePtrHalf(VMBinding.DataSetRegisterIndex, (uint32)Index) = HalfData.GetAndAdvance();
				};

				bSuccess = VariadicInputs.Process(bEmit, 1, BindingInfo, FloatFunc, IntFunc, HalfFunc);

				if (OutSuccess.IsValid())
				{
					OutSuccess.SetAndAdvance(bSuccess);
				}
			}

			//Update the shared instance count with an updated max.
			bool bAtomicNumInstancesWritten = false;
			uint32 CurrNumInstances = AtomicNumInstances;
			while(CurrNumInstances < MaxLocalIndex && !AtomicNumInstances.compare_exchange_weak(CurrNumInstances, MaxLocalIndex))
			{
				CurrNumInstances = AtomicNumInstances;
			}
		}
	}

	if (bAllFailedFallback)
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
	FNDIInputParam<FNiagaraBool> InEmit(Context);

	const FNDIDataChannelFunctionInfo& FuncInfo = CompiledData.GetFunctionInfo()[FuncIdx];
	const FNDIDataChannel_FunctionToDataSetBinding* BindingInfo = InstData->FunctionToDatSetBindingInfo.IsValidIndex(FuncIdx) ? InstData->FunctionToDatSetBindingInfo[FuncIdx].Get() : nullptr;
	FNDIVariadicInputHandler<16> VariadicInputs(Context, BindingInfo);//TODO: Make static / avoid allocation

	FNDIOutputParam<FNiagaraBool> OutSuccess(Context);

	std::atomic<uint32>& AtomicNumInstances = InstData->AtomicNumInstances;

	bool bAllFailedFallback = true;
	if (InstData->Data && BindingInfo &&  INiagaraModule::DataChannelsEnabled())
	{
		if(FNiagaraDataBuffer* Data = InstData->Data->GetDestinationData())
		{
			//Get the total number to emit.
			//Allows going via a faster write path if we're emiting every instance.
			//Aslo needed to update the atomic num instances and get our start index for writing.
			uint32 LocalNumToEmit = 0;
			if(InEmit.IsConstant())
			{
				bool bEmit = InEmit.GetAndAdvance();
				LocalNumToEmit = bEmit ? Context.GetNumInstances() : 0;
			}
			else
			{
				for (int32 i = 0; i < Context.GetNumInstances(); ++i)
				{
					if (InEmit.GetAndAdvance())
					{
						++LocalNumToEmit;
					}
				}
			}			

			if (LocalNumToEmit > 0)
			{
				uint32 NumAllocated = Data->GetNumInstancesAllocated();
				InEmit.Reset();

				//Update the shared atomic instance count and grab the current index at which we can write.
				uint32 CurrNumInstances = AtomicNumInstances.fetch_add(LocalNumToEmit);

				bAllFailedFallback = false;

				bool bEmitAll = LocalNumToEmit == Context.GetNumInstances();

				if(bEmitAll)
				{
					//limit the number to emit so we do not write over the end of the buffers.
					uint32 MaxWriteCount = NumAllocated - FMath::Min(CurrNumInstances, NumAllocated);
					LocalNumToEmit = FMath::Min(LocalNumToEmit, MaxWriteCount);

					//If we're writing all instances then we can do a memcpy instead of slower loop copies.
					bool bSuccess = false;
					uint32 Index = CurrNumInstances;
					auto FloatFunc = [Data, Index, LocalNumToEmit](const FNDIDataChannelRegisterBinding& VMBinding, FNDIInputParam<float>& FloatData)
					{
						if (VMBinding.DataSetRegisterIndex != INDEX_NONE)
						{
							float* Dest = Data->GetInstancePtrFloat(VMBinding.DataSetRegisterIndex, Index);
							if (FloatData.IsConstant())
							{
								float Value = FloatData.GetAndAdvance();
								for(uint32 i=0; i<LocalNumToEmit; ++i){ Dest[i] = Value; }
							}
							else
							{
								 const float* Src = FloatData.Data.GetDest();
								 FMemory::Memcpy(Dest, Src, LocalNumToEmit * sizeof(float));
							}
						}
					};
					auto IntFunc = [Data, Index, LocalNumToEmit](const FNDIDataChannelRegisterBinding& VMBinding, FNDIInputParam<int32>& IntData)
					{
						if (VMBinding.DataSetRegisterIndex != INDEX_NONE)
						{
							int32* Dest = Data->GetInstancePtrInt32(VMBinding.DataSetRegisterIndex, Index);
							if (IntData.IsConstant())
							{
								int32 Value = IntData.GetAndAdvance();
								for (uint32 i = 0; i < LocalNumToEmit; ++i) { Dest[i] = Value; }
							}
							else
							{
								const int32* Src = IntData.Data.GetDest();
								FMemory::Memcpy(Dest, Src, LocalNumToEmit * sizeof(int32));
							}
						}
					};
					auto HalfFunc = [Data, Index, LocalNumToEmit](const FNDIDataChannelRegisterBinding& VMBinding, FNDIInputParam<FFloat16>& HalfData)
					{
						if (VMBinding.DataSetRegisterIndex != INDEX_NONE)
						{
							FFloat16* Dest = Data->GetInstancePtrHalf(VMBinding.DataSetRegisterIndex, Index);
							if (HalfData.IsConstant())
							{
								FFloat16 Value = HalfData.GetAndAdvance();
								for (uint32 i = 0; i < LocalNumToEmit; ++i) { Dest[i] = Value; }
							}
							else
							{
								const FFloat16* Src = HalfData.Data.GetDest();
								FMemory::Memcpy(Dest, Src, LocalNumToEmit * sizeof(FFloat16));
							}
						}
					};
					bSuccess = VariadicInputs.Process(true, Context.GetNumInstances(), BindingInfo, FloatFunc, IntFunc, HalfFunc);

					if (OutSuccess.IsValid())
					{
						for (int32 i = 0; i < Context.GetNumInstances(); ++i)
						{
							OutSuccess.SetAndAdvance(bSuccess);
						}
					}
				}
				else
				{
					for (int32 i = 0; i < Context.GetNumInstances() && CurrNumInstances < NumAllocated; ++i)
					{						
						uint32 Index = CurrNumInstances;

						bool bEmit = InEmit.GetAndAdvance();
						bool bSuccess = false;

						if(bEmit)
						{
							++CurrNumInstances;
						}

						auto FloatFunc = [Data, Index](const FNDIDataChannelRegisterBinding& VMBinding, FNDIInputParam<float>& FloatData)
						{
							if (VMBinding.DataSetRegisterIndex != INDEX_NONE)
								*Data->GetInstancePtrFloat(VMBinding.DataSetRegisterIndex, Index) = FloatData.GetAndAdvance();
						};
						auto IntFunc = [Data, Index](const FNDIDataChannelRegisterBinding& VMBinding, FNDIInputParam<int32>& IntData)
						{
							if (VMBinding.DataSetRegisterIndex != INDEX_NONE)
								*Data->GetInstancePtrInt32(VMBinding.DataSetRegisterIndex, Index) = IntData.GetAndAdvance();
						};
						auto HalfFunc = [Data, Index](const FNDIDataChannelRegisterBinding& VMBinding, FNDIInputParam<FFloat16>& HalfData)
						{
							if (VMBinding.DataSetRegisterIndex != INDEX_NONE)
								*Data->GetInstancePtrHalf(VMBinding.DataSetRegisterIndex, Index) = HalfData.GetAndAdvance();
						};
						bSuccess = VariadicInputs.Process(bEmit, 1, BindingInfo, FloatFunc, IntFunc, HalfFunc);

						if (OutSuccess.IsValid())
						{
							OutSuccess.SetAndAdvance(bSuccess);
						}
					}
				}
			}
		}
	}

	if(bAllFailedFallback)
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

	return true;
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
