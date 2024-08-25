// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataChannelAccessor.h"

#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"

#include "NiagaraDataChannelPublic.h"
#include "NiagaraDataChannelCommon.h"
#include "NiagaraDataChannel.h"
#include "NiagaraDataChannelHandler.h"

#include "NiagaraWorldManager.h"

//////////////////////////////////////////////////////////////////////////

bool UNiagaraDataChannelReader::InitAccess(FNiagaraDataChannelSearchParameters SearchParams, bool bReadPreviousFrameData)
{
	Data = nullptr;
	bReadingPreviousFrame = bReadPreviousFrameData;
	check(Owner);

	Data = Owner->FindData(SearchParams, ENiagaraResourceAccess::ReadOnly);
	return Data.IsValid();
}

int32 UNiagaraDataChannelReader::Num()const
{
	if (Data.IsValid())
	{
		return bReadingPreviousFrame ? Data->GetGameData()->PrevNum() : Data->GetGameData()->Num();
	}
	return 0;
}

template<typename T>
bool UNiagaraDataChannelReader::ReadData(const FNiagaraVariableBase& Var, int32 Index, T& OutData)const
{
	if (ensure(Data.IsValid()))
	{
		if (FNiagaraDataChannelVariableBuffer* VarBuffer = Data->GetGameData()->FindVariableBuffer(Var))
		{
			return VarBuffer->Read<T>(Index, OutData, bReadingPreviousFrame);
		}
	}
	return false;
}

double UNiagaraDataChannelReader::ReadFloat(FName VarName, int32 Index, bool& IsValid)const
{
	double RetVal = 0.0f;
	IsValid = ReadData<double>(FNiagaraVariableBase(FNiagaraTypeHelper::GetDoubleDef(), VarName), Index, RetVal);
	return RetVal;
}

FVector2D UNiagaraDataChannelReader::ReadVector2D(FName VarName, int32 Index, bool& IsValid)const
{
	FVector2D RetVal = FVector2D::ZeroVector;
	IsValid = ReadData<FVector2D>(FNiagaraVariableBase(FNiagaraTypeHelper::GetVector2DDef(), VarName), Index, RetVal);
	return RetVal;
}

FVector UNiagaraDataChannelReader::ReadVector(FName VarName, int32 Index, bool& IsValid)const
{
	FVector RetVal = FVector::ZeroVector;
	IsValid = ReadData<FVector>(FNiagaraVariableBase(FNiagaraTypeHelper::GetVectorDef(), VarName), Index, RetVal);
	return RetVal;
}

FVector4 UNiagaraDataChannelReader::ReadVector4(FName VarName, int32 Index, bool& IsValid)const
{
	FVector4 RetVal = FVector4(0.0f);
	IsValid = ReadData<FVector4>(FNiagaraVariableBase(FNiagaraTypeHelper::GetVector4Def(), VarName), Index, RetVal);
	return RetVal;
}

FQuat UNiagaraDataChannelReader::ReadQuat(FName VarName, int32 Index, bool& IsValid)const
{
	FQuat RetVal = FQuat::Identity;
	IsValid = ReadData<FQuat>(FNiagaraVariableBase(FNiagaraTypeHelper::GetQuatDef(), VarName), Index, RetVal);
	return RetVal;
}

FLinearColor UNiagaraDataChannelReader::ReadLinearColor(FName VarName, int32 Index, bool& IsValid)const
{
	FLinearColor RetVal = FLinearColor::White;
	IsValid = ReadData<FLinearColor>(FNiagaraVariableBase(FNiagaraTypeDefinition::GetColorDef(), VarName), Index, RetVal);
	return RetVal;
}

int32 UNiagaraDataChannelReader::ReadInt(FName VarName, int32 Index, bool& IsValid)const
{
	int32 RetVal = 0;
	IsValid = ReadData<int32>(FNiagaraVariableBase(FNiagaraTypeDefinition::GetIntDef(), VarName), Index, RetVal);
	return RetVal;
}

uint8 UNiagaraDataChannelReader::ReadEnum(FName VarName, int32 Index, bool& IsValid) const
{
	return static_cast<uint8>(ReadInt(VarName, Index, IsValid));
}

bool UNiagaraDataChannelReader::ReadBool(FName VarName, int32 Index, bool& IsValid)const
{
	FNiagaraBool RetVal(false);
	IsValid = ReadData<FNiagaraBool>(FNiagaraVariableBase(FNiagaraTypeDefinition::GetBoolDef(), VarName), Index, RetVal);
	return RetVal;
}

FVector UNiagaraDataChannelReader::ReadPosition(FName VarName, int32 Index, bool& IsValid)const
{
	FVector RetVal = FVector::ZeroVector;
	IsValid = ReadData<FVector>(FNiagaraVariableBase(FNiagaraTypeDefinition::GetPositionDef(), VarName), Index, RetVal);
	return RetVal;
}

FNiagaraID UNiagaraDataChannelReader::ReadID(FName VarName, int32 Index, bool& IsValid) const
{
	FNiagaraID RetVal;
	IsValid = ReadData<FNiagaraID>(FNiagaraVariableBase(FNiagaraTypeDefinition::GetIDDef(), VarName), Index, RetVal);
	return RetVal;
}

FNiagaraSpawnInfo UNiagaraDataChannelReader::ReadSpawnInfo(FName VarName, int32 Index, bool& IsValid) const
{
	FNiagaraSpawnInfo RetVal;
	IsValid = ReadData<FNiagaraSpawnInfo>(FNiagaraVariableBase(FNiagaraTypeDefinition(FNiagaraSpawnInfo::StaticStruct()), VarName), Index, RetVal);
	return RetVal;
}

//////////////////////////////////////////////////////////////////////////

template<typename T>
void UNiagaraDataChannelWriter::WriteData(const FNiagaraVariableBase& Var, int32 Index, const T& InData)
{
	if (ensure(Data.IsValid()))
	{
		if (FNiagaraDataChannelVariableBuffer* VarBuffer = Data->FindVariableBuffer(Var))
		{
			VarBuffer->Write<T>(Index, InData);
		}
	}
}

bool UNiagaraDataChannelWriter::InitWrite(FNiagaraDataChannelSearchParameters SearchParams, int32 Count, bool bVisibleToGame, bool bVisibleToCPU, bool bVisibleToGPU, const FString& DebugSource)
{
	if (Count == 0)
	{
		UE_LOG(LogNiagara, Warning, TEXT("Call to UNiagaraDataChannelWriter::InitWrite with Count == 0. Ignored."));
		return false;
	}

	check(Owner);

	if(FNiagaraDataChannelDataPtr DestData = Owner->FindData(SearchParams, ENiagaraResourceAccess::WriteOnly))
	{
		//TODO- Dont create a whole new game Data here. Rather grab a pre-made staging/input One from the Data PTR With the relevant publish Flags.
		Data = Owner->GetDataChannel()->CreateGameData();
		Data->SetNum(Count);

		FNiagaraDataChannelPublishRequest PublishRequest;
		PublishRequest.bVisibleToGame = bVisibleToGame;
		PublishRequest.bVisibleToCPUSims = bVisibleToCPU;
		PublishRequest.bVisibleToGPUSims = bVisibleToGPU;
		PublishRequest.GameData = Data;
#if !UE_BUILD_SHIPPING
		PublishRequest.DebugSource = DebugSource;
#endif
		DestData->Publish(PublishRequest);
		return true;
	}

	return false;
}

int32 UNiagaraDataChannelWriter::Num()const
{
	if (Data.IsValid())
	{
		return Data->Num();
	}
	return 0;
}

void UNiagaraDataChannelWriter::WriteFloat(FName VarName, int32 Index, double InData)
{
	WriteData(FNiagaraVariableBase(FNiagaraTypeHelper::GetDoubleDef(), VarName), Index, InData);
}

void UNiagaraDataChannelWriter::WriteVector2D(FName VarName, int32 Index, FVector2D InData)
{
	WriteData(FNiagaraVariableBase(FNiagaraTypeHelper::GetVector2DDef(), VarName), Index, InData);
}

void UNiagaraDataChannelWriter::WriteVector(FName VarName, int32 Index, FVector InData)
{
	WriteData(FNiagaraVariableBase(FNiagaraTypeHelper::GetVectorDef(), VarName), Index, InData);
}

void UNiagaraDataChannelWriter::WriteVector4(FName VarName, int32 Index, FVector4 InData)
{
	WriteData(FNiagaraVariableBase(FNiagaraTypeHelper::GetVector4Def(), VarName), Index, InData);
}

void UNiagaraDataChannelWriter::WriteQuat(FName VarName, int32 Index, FQuat InData)
{
	WriteData(FNiagaraVariableBase(FNiagaraTypeHelper::GetQuatDef(), VarName), Index, InData);
}

void UNiagaraDataChannelWriter::WriteLinearColor(FName VarName, int32 Index, FLinearColor InData)
{
	WriteData(FNiagaraVariableBase(FNiagaraTypeDefinition::GetColorDef(), VarName), Index, InData);
}

void UNiagaraDataChannelWriter::WriteInt(FName VarName, int32 Index, int32 InData)
{
	WriteData(FNiagaraVariableBase(FNiagaraTypeDefinition::GetIntDef(), VarName), Index, InData);
}

void UNiagaraDataChannelWriter::WriteEnum(FName VarName, int32 Index, uint8 InData)
{
	WriteInt(VarName, Index, InData);
}

void UNiagaraDataChannelWriter::WriteBool(FName VarName, int32 Index, bool InData)
{
	WriteData(FNiagaraVariableBase(FNiagaraTypeDefinition::GetBoolDef(), VarName), Index, FNiagaraBool(InData));
}

void UNiagaraDataChannelWriter::WriteSpawnInfo(FName VarName, int32 Index, FNiagaraSpawnInfo InData)
{
	WriteData(FNiagaraVariableBase(FNiagaraTypeDefinition(FNiagaraSpawnInfo::StaticStruct()), VarName), Index, InData);
}

void UNiagaraDataChannelWriter::WritePosition(FName VarName, int32 Index, FVector InData)
{
	WriteData(FNiagaraVariableBase(FNiagaraTypeDefinition::GetPositionDef(), VarName), Index, InData);
}

void UNiagaraDataChannelWriter::WriteID(FName VarName, int32 Index, FNiagaraID InData)
{
	WriteData(FNiagaraVariableBase(FNiagaraTypeDefinition::GetIDDef(), VarName), Index, InData);
}


//////////////////////////////////////////////////////////////////////////

void FNiagaraDataChannelGameDataWriterBase::BeginWrite()
{
	if(Data)
	{
		Data->BeginFrame();
	}
}

void FNiagaraDataChannelGameDataWriterBase::Publish(FNiagaraDataChannelDataPtr& Destination)
{
	if (Data && Data->Num() > 0)
	{
		FNiagaraDataChannelPublishRequest PublishRequest;
		PublishRequest.bVisibleToCPUSims = true;
		PublishRequest.bVisibleToGPUSims = true;
		PublishRequest.GameData = Data;
		Destination->Publish(PublishRequest);
	}
}

void FNiagaraDataChannelGameDataWriterBase::SetNum(int32 Num)
{
	if (Data)
	{
		Data->SetNum(Num);
	}
}

void FNiagaraDataChannelGameDataWriterBase::Reserve(int32 Num)
{
	if (Data)
	{
		Data->Reserve(Num);
	}
}

int32 FNiagaraDataChannelGameDataWriterBase::Add(int32 Count)
{
	if (Data)
	{
		return Data->Add(Count);
	}
	return INDEX_NONE;
}

//////////////////////////////////////////////////////////////////////////

FNiagaraDataChannelDataPtr FNiagaraDataChannelGameDataGroupedWriterBase::FindDataChannelData(UWorld* World, const UNiagaraDataChannel* NDC, const FNiagaraDataChannelSearchParameters& ItemSearchParams)
{
	if(World == nullptr || NDC == nullptr)
	{
		return nullptr;
	}

	if (FNiagaraWorldManager* WorldMan = FNiagaraWorldManager::Get(World))
	{
		if (UNiagaraDataChannelHandler* NDCHandler = WorldMan->FindDataChannelHandler(NDC))
		{
			return NDCHandler->FindData(ItemSearchParams, ENiagaraResourceAccess::WriteOnly);
		}
	}
	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
