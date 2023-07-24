// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataChannel.h"

#include "CoreMinimal.h"
#include "NiagaraWorldManager.h"
#include "NiagaraDataChannelHandler.h"
#include "Engine/Engine.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraDataChannel)

#define LOCTEXT_NAMESPACE "NiagaraDataChannels"

//////////////////////////////////////////////////////////////////////////

void FNiagaraDataChannelGameDataLayout::Init(const TArray<FNiagaraVariable>& Variables)
{
	VariableIndices.Reset();
	LwcConverters.Reserve(Variables.Num());
	for (const FNiagaraVariable& Var : Variables)
	{
		int32& VarIdx = VariableIndices.Add(Var);
		VarIdx = VariableIndices.Num() - 1;

		FNiagaraLwcStructConverter& Converter = LwcConverters.AddDefaulted_GetRef();
		Converter = FNiagaraTypeRegistry::GetStructConverter(Var.GetType());
	}
}

//////////////////////////////////////////////////////////////////////////

void FNiagaraDataChannelGameData::Init(const UNiagaraDataChannel* InDataChannel)
{
	check(InDataChannel);
	DataChannel = InDataChannel;

	const FNiagaraDataChannelGameDataLayout& Layout = DataChannel->GetGameDataLayout();
	VariableData.Empty();
	VariableData.SetNum(Layout.VariableIndices.Num());
	for(const auto& Pair : Layout.VariableIndices)
	{
		int32 Index = Pair.Value;
		VariableData[Index].Init(Pair.Key);
	}
}

void FNiagaraDataChannelGameData::Empty()
{
	NumElements = 0;
	for (FNiagaraDataChannelVariableBuffer& VarData : VariableData)
	{
		VarData.Empty();
	}
}

void FNiagaraDataChannelGameData::Reset()
{
	NumElements = 0;
	for (FNiagaraDataChannelVariableBuffer& VarData : VariableData)
	{
		VarData.Reset();
	}
}

void FNiagaraDataChannelGameData::SetNum(int32 NewNum)
{
	NumElements = NewNum;
	for (auto& Buffer : VariableData)
	{
		Buffer.SetNum(NewNum);
	}
}

FNiagaraDataChannelVariableBuffer* FNiagaraDataChannelGameData::FindVariableBuffer(const FNiagaraVariableBase& Var)
{
	const FNiagaraDataChannelGameDataLayout& Layout = DataChannel->GetGameDataLayout();
	if(const int32* Idx = Layout.VariableIndices.Find(Var))
	{
		return &VariableData[*Idx];
	}
	return nullptr;
}

void FNiagaraDataChannelGameData::WriteToDataSet(FNiagaraDataBuffer* DestBuffer, int32 DestStartIdx)
{
	const FNiagaraDataSetCompiledData& CompiledData = DestBuffer->GetOwner()->GetCompiledData();

	static TArray<uint8> LWCConversionBuffer;

	int32 NumInstances = NumElements;

	if (NumInstances == 0)
	{
		return;
	}
	
	DestBuffer->SetNumInstances(NumInstances);

	const FNiagaraDataChannelGameDataLayout& Layout = DataChannel->GetGameDataLayout();

	for (const TPair<FNiagaraVariableBase, int32>& VarIndexPair : Layout.VariableIndices)
	{
		const FNiagaraVariableBase& Var = VarIndexPair.Key;
		int32 VarIndex = VarIndexPair.Value;
		FNiagaraDataChannelVariableBuffer& VarBuffer = VariableData[VarIndex];
		uint8* SrcData = VarBuffer.Data.GetData();
		
		FNiagaraVariableBase SimVar = Var;

		//Convert from LWC types to Niagara Simulation Types where required.
		if (FNiagaraTypeHelper::IsLWCType(Var.GetType()))
		{
			SimVar.SetType(FNiagaraTypeDefinition(FNiagaraTypeHelper::FindNiagaraFriendlyTopLevelStruct(CastChecked<UScriptStruct>(Var.GetType().GetStruct()), ENiagaraStructConversion::Simulation)));
		}

		int32 SimVarIndex = CompiledData.Variables.IndexOfByKey(SimVar);
		if (SimVarIndex == INDEX_NONE)
		{
			continue; //Did not find this variable in the dataset. Warn?
		}

		//Convert from LWC types to Niagara Simulation Types where required.
// 		if (FNiagaraTypeHelper::IsLWCType(Var.GetType()))
// 		{
// 			LWCConversionBuffer.SetNumUninitialized(SimVar.GetSizeInBytes() * NumInstances, false);
// 			
// 			//TODO: Can we just avoid all this and convert any doubles direct to floats when we transfer the date into the dest buffer?
// 			FNiagaraLwcStructConverter& Converter = LwcConverters[VarIndex];
// 			Converter.ConvertDataToSimulation(LWCConversionBuffer.GetData(), SrcData, NumInstances);
// 			SrcData = LWCConversionBuffer.GetData();
// 		}

		int32 SrcVarSize = Var.GetSizeInBytes();
		int32 DestVarSize = SimVar.GetSizeInBytes();
		const FNiagaraVariableLayoutInfo& SimLayout = CompiledData.VariableLayouts[SimVarIndex];

		int32 FloatCompIdx = SimLayout.FloatComponentStart;
		int32 IntCompIdx = SimLayout.Int32ComponentStart;
		int32 HalfCompIdx = SimLayout.HalfComponentStart;

		TFunction<void(UScriptStruct*, UScriptStruct*)> WriteData;
		WriteData = [&](UScriptStruct* SrcStruct, UScriptStruct* DestStruct)
		{
			TFieldIterator<FProperty> SrcPropertyIt(SrcStruct, EFieldIteratorFlags::IncludeSuper);
			TFieldIterator<FProperty> DestPropertyIt(DestStruct, EFieldIteratorFlags::IncludeSuper);
			for (; SrcPropertyIt; ++SrcPropertyIt, ++DestPropertyIt)
			{
				FProperty* SrcProperty = *SrcPropertyIt;
				FProperty* DestProperty = *DestPropertyIt;
				int32 SrcOffset = SrcProperty->GetOffset_ForInternal();

				//Convert any LWC doubles to floats. //TODO: Insert LWCTile... probably need to explicitly check for vectors etc.
				if (SrcProperty->IsA(FDoubleProperty::StaticClass()))
				{
					check(DestProperty->IsA(FFloatProperty::StaticClass()));
					float* Dest = (float*)DestBuffer->GetComponentPtrFloat(FloatCompIdx++);
					//Write all instances for this component.
					for (int32 i = 0; i < NumInstances; ++i)
					{
						double* Src = (double*)((SrcData + i * SrcVarSize) + SrcOffset);
						*Dest++ = *Src;
					}
				}
				else if (SrcProperty->IsA(FFloatProperty::StaticClass()))
				{
					float* Dest = (float*)DestBuffer->GetComponentPtrFloat(FloatCompIdx++);

					//Write all instances for this component.
					for (int32 i = 0; i < NumInstances; ++i)
					{
						float* Src = (float*)((SrcData + i * SrcVarSize) + SrcOffset);
						*Dest++ = *Src;
					}
				}
				else if (SrcProperty->IsA(FUInt16Property::StaticClass()))
				{
					FFloat16* Dest = (FFloat16*)DestBuffer->GetComponentPtrHalf(HalfCompIdx++);

					//Write all instances for this component.
					for (int32 i = 0; i < NumInstances; ++i)
					{
						FFloat16* Src = (FFloat16*)((SrcData + i * SrcVarSize) + SrcOffset);
						*Dest++ = *Src;						
					}
				}
				else if (SrcProperty->IsA(FIntProperty::StaticClass()) || SrcProperty->IsA(FBoolProperty::StaticClass()))
				{
					int32* Dest = (int32*)DestBuffer->GetComponentPtrInt32(IntCompIdx++);

					//Write all instances for this component.
					for (int32 i = 0; i < NumInstances; ++i)
					{
						int32* Src = (int32*)((SrcData + i * SrcVarSize) + SrcOffset);
						*Dest++ = *Src;						
					}
				}
				//Should be able to support double easily enough
				else if (FStructProperty* StructProp = CastField<FStructProperty>(SrcProperty))
				{
					FStructProperty* DestStructProp = CastField<FStructProperty>(DestProperty);
					WriteData(StructProp->Struct, DestStructProp->Struct);
				}
				else
				{
					checkf(false, TEXT("Property(%s) Class(%s) is not a supported type"), *SrcProperty->GetName(), *SrcProperty->GetClass()->GetName());
				}
			}
		};
		WriteData(Var.GetType().GetScriptStruct(), SimVar.GetType().GetScriptStruct());
	}
}
void FNiagaraDataChannelGameData::AppendFromGameData(const FNiagaraDataChannelGameData& GameData)
{
	check(GetDataChannel() == GameData.GetDataChannel());

	NumElements += GameData.Num();
	TConstArrayView<FNiagaraDataChannelVariableBuffer> SrcBuffers = GameData.GetVariableBuffers();
	for(int32 i=0; i < SrcBuffers.Num(); ++i)
	{
		const FNiagaraDataChannelVariableBuffer& SrcBuffer = SrcBuffers[i];
		FNiagaraDataChannelVariableBuffer& DestBuffer = VariableData[i];
		DestBuffer.Data.Append(SrcBuffer.Data);
	}
}

void FNiagaraDataChannelGameData::AppendFromDataSet(const FNiagaraDataBuffer* SrcBuffer, FVector3f LwcTile)
{
	const FNiagaraDataSetCompiledData& CompiledData = SrcBuffer->GetOwner()->GetCompiledData();

	static TArray<uint8> LWCConversionBuffer;

	int32 NumInstances = SrcBuffer->GetNumInstances();
	int32 OrigNumDataChannel = NumElements;
	NumElements += NumInstances;

	const FNiagaraDataChannelGameDataLayout& Layout = DataChannel->GetGameDataLayout();
	for (const TPair<FNiagaraVariableBase, int32>& VarIndexPair : Layout.VariableIndices)
	{
		const FNiagaraVariableBase& Var = VarIndexPair.Key;
		int32 VarIndex = VarIndexPair.Value;
		FNiagaraDataChannelVariableBuffer& VarBuffer = VariableData[VarIndex];

		int32 VarSize = Var.GetSizeInBytes();

		VarBuffer.SetNum(NumElements);

		uint8* DestDataBase = VarBuffer.Data.GetData();
		uint8* DestData = DestDataBase;
		
		FNiagaraVariableBase SimVar = Var;
		if (FNiagaraTypeHelper::IsLWCType(Var.GetType()))
		{
			SimVar.SetType(FNiagaraTypeDefinition(FNiagaraTypeHelper::FindNiagaraFriendlyTopLevelStruct(Var.GetType().GetScriptStruct(), ENiagaraStructConversion::Simulation)));
		}
		
		int32 SimVarIndex = CompiledData.Variables.IndexOfByKey(SimVar);
		if (SimVarIndex == INDEX_NONE)
		{
			continue; //Did not find this variable in the dataset. Warn?
		}

		//Convert from Niagara Simulation to LWC types where required.
// 		if (FNiagaraTypeHelper::IsLWCType(Var.GetType()))
// 		{
// 			LWCConversionBuffer.SetNumUninitialized(Var.GetSizeInBytes() * NumInstances, false);
// 
// 			//Write instead to our intermediate conversion buffer and do the LWC convert after.
// 			DestData = LWCConversionBuffer.GetData();
// 		}

		const FNiagaraVariableLayoutInfo& SimLayout = CompiledData.VariableLayouts[SimVarIndex];
		
		int32 FloatCompIdx = SimLayout.FloatComponentStart;
		int32 IntCompIdx = SimLayout.Int32ComponentStart;
		int32 HalfCompIdx = SimLayout.HalfComponentStart;
		
		TFunction<void(UScriptStruct*, UScriptStruct*)> ReadData;
		ReadData = [&](UScriptStruct* SrcStruct, UScriptStruct* DestStruct)
		{
			TFieldIterator<FProperty> SrcPropertyIt(SrcStruct, EFieldIteratorFlags::IncludeSuper);
			TFieldIterator<FProperty> DestPropertyIt(DestStruct, EFieldIteratorFlags::IncludeSuper);			
			for (; SrcPropertyIt; ++SrcPropertyIt, ++DestPropertyIt)
			{
				FProperty* SrcProperty = *SrcPropertyIt;
				FProperty* DestProperty = *DestPropertyIt;
				DestData = DestDataBase + DestProperty->GetOffset_ForInternal();
				if (DestPropertyIt->IsA(FDoubleProperty::StaticClass()))//TODO: Insert LWCTile... probably need to explicitly check for vectors etc.
				{
					double* Dest = (double*)(DestData);
					float* Src = (float*)SrcBuffer->GetComponentPtrFloat(FloatCompIdx++);

					//Write all instances for this component.
					for (int32 i = 0; i < NumInstances; ++i)
					{
						Dest = (double*)(DestData + VarSize * i);
						*Dest = *Src++;
					}
				}
				else if (DestPropertyIt->IsA(FFloatProperty::StaticClass()))
				{
					float* Dest = (float*)(DestData);
					float* Src = (float*)SrcBuffer->GetComponentPtrFloat(FloatCompIdx++);

					//Write all instances for this component.
					for (int32 i = 0; i < NumInstances; ++i)
					{
						Dest = (float*)(DestData + VarSize * i);
						*Dest = *Src++;
					}
				}
				else if (DestPropertyIt->IsA(FUInt16Property::StaticClass()))
				{
					FFloat16* Dest = (FFloat16*)(DestData);
					FFloat16* Src = (FFloat16*)SrcBuffer->GetComponentPtrHalf(HalfCompIdx++);

					//Write all instances for this component.
					for (int32 i = 0; i < NumInstances; ++i)
					{
						Dest = (FFloat16*)(DestData + VarSize * i);
						*Dest = *Src++;
					}
				}
				else if (DestPropertyIt->IsA(FIntProperty::StaticClass()) || DestPropertyIt->IsA(FBoolProperty::StaticClass()))
				{
					int32* Dest = (int32*)(DestData);
					int32* Src = (int32*)SrcBuffer->GetComponentPtrInt32(IntCompIdx++);

					//Write all instances for this component.
					for (int32 i = 0; i < NumInstances; ++i)
					{
						Dest = (int32*)(DestData + VarSize * i);
						*Dest = *Src++;
					}
				}
				//Should be able to support double easily enough
				else if (FStructProperty* SrcStructProp = CastField<FStructProperty>(SrcProperty))
				{
					FStructProperty* DestStructProp = CastField<FStructProperty>(DestProperty);
					ReadData(SrcStructProp->Struct, DestStructProp->Struct);
				}
				else
				{
					checkf(false, TEXT("Property(%s) Class(%s) is not a supported type"), *SrcProperty->GetName(), *SrcProperty->GetClass()->GetName());
				}
			}
		};
		ReadData(SimVar.GetType().GetScriptStruct(), Var.GetType().GetScriptStruct());

		//For LWC types we're written into an intermediate structure for the sim var. Now convert that into LWC proper.
		if (FNiagaraTypeHelper::IsLWCType(Var.GetType()))
		{
			const FNiagaraLwcStructConverter& Converter = Layout.LwcConverters[VarIndex];
			VarBuffer.SetNum(NumElements);
			//TODO: Incorporate the LWC Tile
			Converter.ConvertDataFromSimulation(VarBuffer.Data.GetData() + Var.GetSizeInBytes() * OrigNumDataChannel, LWCConversionBuffer.GetData(), NumInstances);
		}
	}
}

//////////////////////////////////////////////////////////////////////////

FNiagaraWorldDataChannelStore::~FNiagaraWorldDataChannelStore()
{
	Empty();
}

void FNiagaraWorldDataChannelStore::Empty()
{
	GameData.Reset();

	/** We defer the deletion of the dataset to the RT to be sure all in-flight RT commands have finished using it.*/
	ENQUEUE_RENDER_COMMAND(FDeleteContextCommand)(
		[CPUDataChannelDataSet = CPUSimData, GPUDataChannelDataSet = GPUSimData](FRHICommandListImmediate& RHICmdList)
		{
			if (CPUDataChannelDataSet != nullptr)
			{
				delete CPUDataChannelDataSet;
			}
			if (GPUDataChannelDataSet != nullptr)
			{
				delete GPUDataChannelDataSet;
			}
		}
	);
	CPUSimData = nullptr;
	GPUSimData = nullptr;
}

void FNiagaraWorldDataChannelStore::Init(UNiagaraDataChannelHandler* Owner)
{
	GameData = Owner->GetDataChannel()->CreateGameData();

	CPUSimData = new FNiagaraDataSet();
	GPUSimData = new FNiagaraDataSet();

	const FNiagaraDataSetCompiledData& CompiledData = Owner->GetDataChannel()->GetCompiledData(ENiagaraSimTarget::CPUSim);
	const FNiagaraDataSetCompiledData& CompiledDataGPU = Owner->GetDataChannel()->GetCompiledData(ENiagaraSimTarget::GPUComputeSim);
	CPUSimData->Init(&CompiledData);
	GPUSimData->Init(&CompiledDataGPU);
}

void FNiagaraWorldDataChannelStore::Tick(UNiagaraDataChannelHandler* Owner)
{

}


//////////////////////////////////////////////////////////////////////////

namespace NiagaraDataChannel
{
	FNiagaraTypeDefinition GetFVectorDef()
	{
		static UPackage* CoreUObjectPkg = FindObjectChecked<UPackage>(nullptr, TEXT("/Script/CoreUObject"));
		static UScriptStruct* VectorStruct = FindObjectChecked<UScriptStruct>(CoreUObjectPkg, TEXT("Vector"));
		return FNiagaraTypeDefinition(VectorStruct, FNiagaraTypeDefinition::EAllowUnfriendlyStruct::Allow);
	}
	
	FNiagaraTypeDefinition GetDoubleDef()
	{
		static UPackage* NiagaraPkg = FindObjectChecked<UPackage>(nullptr, TEXT("/Script/Niagara"));
		static UScriptStruct* DoubleStruct = FindObjectChecked<UScriptStruct>(NiagaraPkg, TEXT("NiagaraDouble"));
		return FNiagaraTypeDefinition(DoubleStruct, FNiagaraTypeDefinition::EAllowUnfriendlyStruct::Allow);
	}

	FNiagaraTypeDefinition GetFQuatDef()
	{
		static UPackage* CoreUObjectPkg = FindObjectChecked<UPackage>(nullptr, TEXT("/Script/CoreUObject"));
		static UScriptStruct* Struct = FindObjectChecked<UScriptStruct>(CoreUObjectPkg, TEXT("Quat"));
		return FNiagaraTypeDefinition(Struct, FNiagaraTypeDefinition::EAllowUnfriendlyStruct::Allow);
	}
	
	FNiagaraTypeDefinition GetFVector2DDef()
	{
		static UPackage* CoreUObjectPkg = FindObjectChecked<UPackage>(nullptr, TEXT("/Script/CoreUObject"));
		static UScriptStruct* VectorStruct = FindObjectChecked<UScriptStruct>(CoreUObjectPkg, TEXT("Vector2D"));
		return FNiagaraTypeDefinition(VectorStruct, FNiagaraTypeDefinition::EAllowUnfriendlyStruct::Allow);
	}

	FNiagaraTypeDefinition GetFVector4Def()
	{
		static UPackage* CoreUObjectPkg = FindObjectChecked<UPackage>(nullptr, TEXT("/Script/CoreUObject"));
		static UScriptStruct* Vector4Struct = FindObjectChecked<UScriptStruct>(CoreUObjectPkg, TEXT("Vector4"));
		return FNiagaraTypeDefinition(Vector4Struct, FNiagaraTypeDefinition::EAllowUnfriendlyStruct::Allow);
	}
}

void UNiagaraDataChannel::PostLoad()
{
	Super::PostLoad();

	for (int i = 0; i < Variables.Num(); i++)
	{
		// fix up variables deserialized with wrong type
		// TODO (mga) find a better solution than this
		FNiagaraVariable& Var = Variables[i];
		if (Var.GetType() == FNiagaraTypeDefinition::GetVec3Def())
		{
			Var.SetType(NiagaraDataChannel::GetFVectorDef());
		}
		else if (Var.GetType() == FNiagaraTypeDefinition::GetFloatDef())
		{
			Var.SetType(NiagaraDataChannel::GetDoubleDef());
		}
		else if (Var.GetType() == FNiagaraTypeDefinition::GetQuatDef())
		{
			Var.SetType(NiagaraDataChannel::GetFQuatDef());
		}
		else if (Var.GetType() == FNiagaraTypeDefinition::GetVec2Def())
		{
			Var.SetType(NiagaraDataChannel::GetFVector2DDef());
		}
		else if (Var.GetType() == FNiagaraTypeDefinition::GetVec4Def())
		{
			Var.SetType(NiagaraDataChannel::GetFVector4Def());
		}
	}

	//TODO: Can serialize?
	GameDataLayout.Init(Variables);

	FNiagaraWorldManager::ForAllWorldManagers(
		[DataChannel = this](FNiagaraWorldManager& WorldMan)
		{
			WorldMan.GetDataChannelManager().InitDataChannel(DataChannel, true);
		});
}

void UNiagaraDataChannel::BeginDestroy()
{
	Super::BeginDestroy();

	FNiagaraWorldManager::ForAllWorldManagers(
		[DataChannel = this](FNiagaraWorldManager& WorldMan)
		{
			WorldMan.WaitForAsyncWork();
			WorldMan.GetDataChannelManager().RemoveDataChannel(DataChannel->GetChannelName());
		});
}

#if WITH_EDITOR

void UNiagaraDataChannel::PreEditChange(FProperty* PropertyAboutToChange)
{
	//We'll do this for all channels inside the definitions pre edit change.
	// 
// 	FNiagaraWorldManager::ForAllWorldManagers(
// 		[DataChannel = this](FNiagaraWorldManager& WorldMan)
// 		{
// 			WorldMan.WaitForAsyncWork();
// 			WorldMan.GetDataChannelManager().RemoveDataChannel(DataChannel->GetChannelName());
// 		});
}

void UNiagaraDataChannel::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	check(IsInGameThread());

	//Refresh compiled data
	CompiledData.Empty();
	CompiledDataGPU.Empty();
	GetCompiledData(ENiagaraSimTarget::CPUSim);
	GetCompiledData(ENiagaraSimTarget::GPUComputeSim);

	GameDataLayout.Init(Variables);

	OnChangedDelegate.Broadcast(this);

	//We'll reinit this channel with the world managers etc in the PostEditChange of the data channel definitions.
}

#endif//WITH_EDITOR

const FNiagaraDataSetCompiledData& UNiagaraDataChannel::GetCompiledData(ENiagaraSimTarget SimTarget)const
{
	if(SimTarget == ENiagaraSimTarget::CPUSim)
	{
		if (CompiledData.Variables.Num() != Variables.Num())
		{
			//Build the compiled data from the current variables but convert to Simulation types.
			CompiledData.Empty();
			CompiledData.SimTarget = ENiagaraSimTarget::CPUSim;
			for (FNiagaraVariableBase Var : Variables)
			{
				Var.SetType(FNiagaraTypeDefinition(FNiagaraTypeHelper::FindNiagaraFriendlyTopLevelStruct(Var.GetType().GetScriptStruct(), ENiagaraStructConversion::Simulation)));
				CompiledData.Variables.Add(Var);
			}
			CompiledData.BuildLayout();
		}
		return CompiledData;
	}
	else
	{
		if (CompiledDataGPU.Variables.Num() != Variables.Num())
		{
			check(SimTarget == ENiagaraSimTarget::GPUComputeSim);
			//Build the compiled data from the current variables but convert to Simulation types.
			CompiledDataGPU.Empty();
			CompiledDataGPU.SimTarget = ENiagaraSimTarget::GPUComputeSim;
			for (FNiagaraVariableBase Var : Variables)
			{
				Var.SetType(FNiagaraTypeDefinition(FNiagaraTypeHelper::FindNiagaraFriendlyTopLevelStruct(Var.GetType().GetScriptStruct(), ENiagaraStructConversion::Simulation)));
				CompiledDataGPU.Variables.Add(Var);
			}
			CompiledDataGPU.BuildLayout();
		}
		return CompiledDataGPU;
	}
}

FNiagaraDataChannelGameDataPtr UNiagaraDataChannel::CreateGameData()const
{
	FNiagaraDataChannelGameDataPtr NewGameData = MakeShared<FNiagaraDataChannelGameData>();
	NewGameData->Init(this);
	return NewGameData;
}

bool UNiagaraDataChannel::IsValid()const
{
	return ChannelName != NAME_None && Variables.Num() > 0 && CompiledData.Variables.Num() == Variables.Num() && CompiledDataGPU.Variables.Num() == Variables.Num();
}

//////////////////////////////////////////////////////////////////////////

UNiagaraDataChannelLibrary::UNiagaraDataChannelLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UNiagaraDataChannelHandler* UNiagaraDataChannelLibrary::GetNiagaraDataChannel(const UObject* WorldContextObject, FName Channel)
{
	UWorld* World = (WorldContextObject != nullptr) ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (World)
	{
		if (FNiagaraWorldManager* WorldMan = FNiagaraWorldManager::Get(World))
		{
			return WorldMan->GetDataChannelManager().FindDataChannelHandler(Channel);
		}
	}
	return nullptr;
}


#undef LOCTEXT_NAMESPACE