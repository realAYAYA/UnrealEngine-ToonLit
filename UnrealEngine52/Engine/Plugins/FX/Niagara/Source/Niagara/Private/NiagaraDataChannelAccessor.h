// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraDataChannelCommon.h"
#include "NiagaraDataChannelAccessor.generated.h"


/** 
Initial simple API for reading and writing data in a data channel from game code / BP. 
Likely to be replaced in the near future with a custom BP node and a helper struct.
*/


UCLASS(Experimental, BlueprintType)
class UNiagaraDataChannelReader : public UObject
{
	GENERATED_BODY()
private:

	FNiagaraDataChannelGameDataPtr Data = nullptr;

	template<typename T>
	T ReadData(const FNiagaraVariableBase& Var, int32 Index)const;

public:

	UPROPERTY()
	TObjectPtr<UNiagaraDataChannelHandler> Owner;
	
	/** Call before each access to the data channel to grab the correct data to read. */
	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "Niagara DataChannel"))
	bool InitAccess(UActorComponent* OwningComponent);

	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "Niagara DataChannel"))
	int32 Num()const;

	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "Niagara DataChannel"))
	double ReadFloat(FName VarName, int32 Index)const;

	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "Niagara DataChannel"))
	FVector2D ReadVector2D(FName VarName, int32 Index)const;

	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "Niagara DataChannel"))
	FVector ReadVector(FName VarName, int32 Index)const;

	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "Niagara DataChannel"))
	FVector4 ReadVector4(FName VarName, int32 Index)const;

	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "Niagara DataChannel"))
	FQuat ReadQuat(FName VarName, int32 Index)const;

	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "Niagara DataChannel"))
	FLinearColor ReadLinearColor(FName VarName, int32 Index)const;

	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "Niagara DataChannel"))
	int32 ReadInt(FName VarName, int32 Index)const;

	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "Niagara DataChannel"))
	bool ReadBool(FName VarName, int32 Index)const;
};

UCLASS(Experimental, BlueprintType)
class UNiagaraDataChannelWriter : public UObject
{
	GENERATED_BODY()
private:

	FNiagaraDataChannelGameDataPtr Data = nullptr;

	template<typename T>
	void WriteData(const FNiagaraVariableBase& Var, int32 Index, const T& InData);

public:

	UPROPERTY()
	TObjectPtr<UNiagaraDataChannelHandler> Owner;
	
	/** Call before each batch of writes to allocate the data we'll be writing to. */
	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "niagara DataChannel"))
	bool InitWrite(UActorComponent* OwningComponent, int32 Count, bool bVisibleToGame=true, bool bVisibleToCPU=true, bool bVisibleToGPU=true);

	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "niagara DataChannel"))
	int32 Num()const;

	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "niagara DataChannel"))
	void WriteFloat(FName VarName, int32 Index, double InData);
	
	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "niagara DataChannel"))
	void WriteVector2D(FName VarName, int32 Index, FVector2D InData);

	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "niagara DataChannel"))
	void WriteVector(FName VarName, int32 Index, FVector InData);

	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "niagara DataChannel"))
	void WriteVector4(FName VarName, int32 Index, FVector4 InData);

	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "niagara DataChannel"))
	void WriteQuat(FName VarName, int32 Index, FQuat InData);

	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "niagara DataChannel"))
	void WriteLinearColor(FName VarName, int32 Index, FLinearColor InData);
	
	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "niagara DataChannel"))
	void WriteInt(FName VarName, int32 Index, int32 InData);

	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "niagara DataChannel"))
	void WriteBool(FName VarName, int32 Index, bool InData);

	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "niagara DataChannel"))
	void WriteSpawnInfo(FName VarName, int32 Index, FNiagaraSpawnInfo InData);
};