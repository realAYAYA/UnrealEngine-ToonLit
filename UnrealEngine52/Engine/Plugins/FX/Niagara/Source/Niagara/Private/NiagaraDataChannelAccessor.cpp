// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataChannelAccessor.h"

#include "CoreMinimal.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"
#include "NiagaraDataChannel.h"
#include "NiagaraDataChannelHandler.h"

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

bool UNiagaraDataChannelReader::InitAccess(UActorComponent* OwningComponent)
{
	Data = nullptr;
	check(Owner);

	if (OwningComponent == nullptr)
	{
		UE_LOG(LogNiagara, Warning, TEXT("Call to UNiagaraDataChannelReader::InitAccess did not provide a valid Owning Componet."));
		return false;
	}

	Data = Owner->GetGameData(OwningComponent);
	return true;
}

int32 UNiagaraDataChannelReader::Num()const
{
	if (Data.IsValid())
	{
		return Data->Num();
	}
	return 0;
}

template<typename T>
T UNiagaraDataChannelReader::ReadData(const FNiagaraVariableBase& Var, int32 Index)const
{
	if (ensure(Data.IsValid()))
	{
		if (FNiagaraDataChannelVariableBuffer* VarBuffer = Data->FindVariableBuffer(Var))
		{
			T OutData;
			VarBuffer->Read<T>(Index, OutData);
			return OutData;
		}
	}
	return T();
}

double UNiagaraDataChannelReader::ReadFloat(FName VarName, int32 Index)const
{
	return ReadData<double>(FNiagaraVariableBase(NiagaraDataChannelAccesorLocal::GetDoubleDef(), VarName), Index);
}

FVector2D UNiagaraDataChannelReader::ReadVector2D(FName VarName, int32 Index)const
{
	return ReadData<FVector2D>(FNiagaraVariableBase(NiagaraDataChannelAccesorLocal::GetVector2DDef(), VarName), Index);
}

FVector UNiagaraDataChannelReader::ReadVector(FName VarName, int32 Index)const
{
	return ReadData<FVector>(FNiagaraVariableBase(NiagaraDataChannelAccesorLocal::GetVectorDef(), VarName), Index);
}

FVector4 UNiagaraDataChannelReader::ReadVector4(FName VarName, int32 Index)const
{
	return ReadData<FVector4>(FNiagaraVariableBase(NiagaraDataChannelAccesorLocal::GetVector4Def(), VarName), Index);
}

FQuat UNiagaraDataChannelReader::ReadQuat(FName VarName, int32 Index)const
{
	return ReadData<FQuat>(FNiagaraVariableBase(NiagaraDataChannelAccesorLocal::GetQuatDef(), VarName), Index);
}

FLinearColor UNiagaraDataChannelReader::ReadLinearColor(FName VarName, int32 Index)const
{
	return ReadData<FLinearColor>(FNiagaraVariableBase(FNiagaraTypeDefinition::GetColorDef(), VarName), Index);
}

int32 UNiagaraDataChannelReader::ReadInt(FName VarName, int32 Index)const
{
	return ReadData<int32>(FNiagaraVariableBase(FNiagaraTypeDefinition::GetIntDef(), VarName), Index);
}

bool UNiagaraDataChannelReader::ReadBool(FName VarName, int32 Index)const
{
	return ReadData<bool>(FNiagaraVariableBase(FNiagaraTypeDefinition::GetBoolDef(), VarName), Index);
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

bool UNiagaraDataChannelWriter::InitWrite(UActorComponent* OwningComponent, int32 Count, bool bVisibleToGame, bool bVisibleToCPU, bool bVisibleToGPU)
{
	if( OwningComponent == nullptr )
	{
		UE_LOG(LogNiagara, Warning, TEXT("Call to UNiagaraDataChannelWriter::InitWrite did not provide a valid Owning Componet."));
		return false;
	}

	if (Count == 0)
	{
		UE_LOG(LogNiagara, Warning, TEXT("Call to UNiagaraDataChannelWriter::InitWrite with Count == 0. Ignored."));
		return false;
	}

	check(Owner);

	Data = Owner->GetDataChannel()->CreateGameData();

	FNiagaraDataChannelPublishRequest PublishRequest;
	PublishRequest.bVisibleToGame = bVisibleToGame;
	PublishRequest.bVisibleToCPUSims = bVisibleToCPU;
	PublishRequest.bVisibleToGPUSims = bVisibleToGPU;
	PublishRequest.GameData = Data;
	Owner->Publish(PublishRequest);

	Data->SetNum(Count);

	return true;
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
	WriteData(FNiagaraVariableBase(FNiagaraTypeDefinition::GetBoolDef(), VarName), Index, InData);
}

void UNiagaraDataChannelWriter::WriteSpawnInfo(FName VarName, int32 Index, FNiagaraSpawnInfo InData)
{
	WriteData(FNiagaraVariableBase(FNiagaraTypeDefinition(FNiagaraSpawnInfo::StaticStruct()), VarName), Index, InData);
}

