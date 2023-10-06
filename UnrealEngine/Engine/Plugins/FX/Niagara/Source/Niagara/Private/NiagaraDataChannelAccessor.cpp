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

namespace NiagaraDataChannelAccesorLocal
{
	//TODO: Move into FNiagaraTypeDefinition.
	static FNiagaraTypeDefinition DoubleDef;
	static FNiagaraTypeDefinition Vector2DDef;
	static FNiagaraTypeDefinition VectorDef;
	static FNiagaraTypeDefinition Vector4Def;
	static FNiagaraTypeDefinition QuatDef;

	void InitDefs()
	{
		static UPackage* CoreUObjectPkg = FindObjectChecked<UPackage>(nullptr, TEXT("/Script/CoreUObject"));
		static UScriptStruct* Vector2Struct = FindObjectChecked<UScriptStruct>(CoreUObjectPkg, TEXT("Vector2D"));
		static UScriptStruct* Vector3Struct = FindObjectChecked<UScriptStruct>(CoreUObjectPkg, TEXT("Vector"));
		static UScriptStruct* Vector4Struct = FindObjectChecked<UScriptStruct>(CoreUObjectPkg, TEXT("Vector4"));
		static UScriptStruct* Quat4Struct = FindObjectChecked<UScriptStruct>(CoreUObjectPkg, TEXT("Quat"));
		DoubleDef = FNiagaraTypeDefinition(FNiagaraDouble::StaticStruct(), FNiagaraTypeDefinition::EAllowUnfriendlyStruct::Allow);
		Vector2DDef = FNiagaraTypeDefinition(Vector2Struct, FNiagaraTypeDefinition::EAllowUnfriendlyStruct::Allow);
		VectorDef = FNiagaraTypeDefinition(Vector3Struct, FNiagaraTypeDefinition::EAllowUnfriendlyStruct::Allow);
		Vector4Def = FNiagaraTypeDefinition(Vector4Struct, FNiagaraTypeDefinition::EAllowUnfriendlyStruct::Allow);
		QuatDef = FNiagaraTypeDefinition(Quat4Struct, FNiagaraTypeDefinition::EAllowUnfriendlyStruct::Allow);
	}

	const FNiagaraTypeDefinition& GetDoubleDef()
	{
		if(DoubleDef.IsValid() == false){ InitDefs(); }
		return DoubleDef;
	}

	const FNiagaraTypeDefinition& GetVector2DDef()
	{
		if (Vector2DDef.IsValid() == false) { InitDefs(); }
		return Vector2DDef;
	}

	const FNiagaraTypeDefinition& GetVectorDef()
	{
		if (VectorDef.IsValid() == false) { InitDefs(); }
		return VectorDef;
	}

	const FNiagaraTypeDefinition& GetVector4Def()
	{
		if (Vector4Def.IsValid() == false) { InitDefs(); }
		return Vector4Def;
	}

	const FNiagaraTypeDefinition& GetQuatDef()
	{
		if (QuatDef.IsValid() == false) { InitDefs(); }
		return QuatDef;
	}
}

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
		return Data->GetGameData()->Num();
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

double UNiagaraDataChannelReader::ReadFloat(FName VarName, int32 Index)const
{
	double RetVal = 0.0f;
	ReadData<double>(FNiagaraVariableBase(NiagaraDataChannelAccesorLocal::GetDoubleDef(), VarName), Index, RetVal);
	return RetVal;
}

FVector2D UNiagaraDataChannelReader::ReadVector2D(FName VarName, int32 Index)const
{
	FVector2D RetVal = FVector2D::ZeroVector;
	ReadData<FVector2D>(FNiagaraVariableBase(NiagaraDataChannelAccesorLocal::GetVector2DDef(), VarName), Index, RetVal);
	return RetVal;
}

FVector UNiagaraDataChannelReader::ReadVector(FName VarName, int32 Index)const
{
	FVector RetVal = FVector::ZeroVector;
	ReadData<FVector>(FNiagaraVariableBase(NiagaraDataChannelAccesorLocal::GetVectorDef(), VarName), Index, RetVal);
	return RetVal;
}

FVector4 UNiagaraDataChannelReader::ReadVector4(FName VarName, int32 Index)const
{
	FVector4 RetVal = FVector4(0.0f);
	ReadData<FVector4>(FNiagaraVariableBase(NiagaraDataChannelAccesorLocal::GetVector4Def(), VarName), Index, RetVal);
	return RetVal;
}

FQuat UNiagaraDataChannelReader::ReadQuat(FName VarName, int32 Index)const
{
	FQuat RetVal = FQuat::Identity;
	ReadData<FQuat>(FNiagaraVariableBase(NiagaraDataChannelAccesorLocal::GetQuatDef(), VarName), Index, RetVal);
	return RetVal;
}

FLinearColor UNiagaraDataChannelReader::ReadLinearColor(FName VarName, int32 Index)const
{
	FLinearColor RetVal = FLinearColor::White;
	ReadData<FLinearColor>(FNiagaraVariableBase(FNiagaraTypeDefinition::GetColorDef(), VarName), Index, RetVal);
	return RetVal;
}

int32 UNiagaraDataChannelReader::ReadInt(FName VarName, int32 Index)const
{
	int32 RetVal = 0;
	ReadData<int32>(FNiagaraVariableBase(FNiagaraTypeDefinition::GetIntDef(), VarName), Index, RetVal);
	return RetVal;
}

bool UNiagaraDataChannelReader::ReadBool(FName VarName, int32 Index)const
{
	bool RetVal = false;
	ReadData<bool>(FNiagaraVariableBase(FNiagaraTypeDefinition::GetBoolDef(), VarName), Index, RetVal);
	return RetVal;
}

FVector UNiagaraDataChannelReader::ReadPosition(FName VarName, int32 Index)const
{
	FVector RetVal = FVector::ZeroVector;
	ReadData<FVector>(FNiagaraVariableBase(FNiagaraTypeDefinition::GetPositionDef(), VarName), Index, RetVal);
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

bool UNiagaraDataChannelWriter::InitWrite(FNiagaraDataChannelSearchParameters SearchParams, int32 Count, bool bVisibleToGame, bool bVisibleToCPU, bool bVisibleToGPU)
{
	if (Count == 0)
	{
		UE_LOG(LogNiagara, Warning, TEXT("Call to UNiagaraDataChannelWriter::InitWrite with Count == 0. Ignored."));
		return false;
	}

	check(Owner);

	FNiagaraDataChannelDataPtr DestData = Owner->FindData(SearchParams, ENiagaraResourceAccess::WriteOnly);
	if(DestData)
	{
		//TODO- Dont create a whole new game Data here. Rather grab a pre-made staging/input One from the Data PTR With the relevant publish Flags.
		Data = Owner->GetDataChannel()->CreateGameData();
		Data->SetNum(Count);

		FNiagaraDataChannelPublishRequest PublishRequest;
		PublishRequest.bVisibleToGame = bVisibleToGame;
		PublishRequest.bVisibleToCPUSims = bVisibleToCPU;
		PublishRequest.bVisibleToGPUSims = bVisibleToGPU;
		PublishRequest.GameData = Data;
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
	WriteData(FNiagaraVariableBase(NiagaraDataChannelAccesorLocal::GetDoubleDef(), VarName), Index, InData);
}

void UNiagaraDataChannelWriter::WriteVector2D(FName VarName, int32 Index, FVector2D InData)
{
	WriteData(FNiagaraVariableBase(NiagaraDataChannelAccesorLocal::GetVector2DDef(), VarName), Index, InData);
}

void UNiagaraDataChannelWriter::WriteVector(FName VarName, int32 Index, FVector InData)
{
	WriteData(FNiagaraVariableBase(NiagaraDataChannelAccesorLocal::GetVectorDef(), VarName), Index, InData);
}

void UNiagaraDataChannelWriter::WriteVector4(FName VarName, int32 Index, FVector4 InData)
{
	WriteData(FNiagaraVariableBase(NiagaraDataChannelAccesorLocal::GetVector4Def(), VarName), Index, InData);
}

void UNiagaraDataChannelWriter::WriteQuat(FName VarName, int32 Index, FQuat InData)
{
	WriteData(FNiagaraVariableBase(NiagaraDataChannelAccesorLocal::GetQuatDef(), VarName), Index, InData);
}

void UNiagaraDataChannelWriter::WriteLinearColor(FName VarName, int32 Index, FLinearColor InData)
{
	WriteData(FNiagaraVariableBase(FNiagaraTypeDefinition::GetColorDef(), VarName), Index, InData);
}

void UNiagaraDataChannelWriter::WriteInt(FName VarName, int32 Index, int32 InData)
{
	WriteData(FNiagaraVariableBase(FNiagaraTypeDefinition::GetIntDef(), VarName), Index, InData);
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
	if (Data->Num() > 0)
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
