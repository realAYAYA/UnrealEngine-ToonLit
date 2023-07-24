// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"

#include "NiagaraVariant.generated.h"

struct FNiagaraCompileHashVisitor;
class UNiagaraDataInterface;

UENUM()
enum class ENiagaraVariantMode
{
	None,
	Object,
	DataInterface,
	Bytes
};

USTRUCT()
struct NIAGARA_API FNiagaraVariant
{
	GENERATED_BODY()

	FNiagaraVariant();
	FNiagaraVariant(const FNiagaraVariant& Other);
	explicit FNiagaraVariant(UNiagaraDataInterface* InDataInterface);
	explicit FNiagaraVariant(UObject* InObject);
	explicit FNiagaraVariant(const TArray<uint8>& InBytes);
	FNiagaraVariant(const void* InBytes, int32 Size);

	UObject* GetUObject() const;
	void SetUObject(UObject* InObject);

	UNiagaraDataInterface* GetDataInterface() const;
	void SetDataInterface(UNiagaraDataInterface* InDataInterface);

	void AllocateBytes(int32 InCount);
	void SetBytes(const uint8* InBytes, int32 InCount);
	uint8* GetBytes() const;
	int32 GetNumBytes() const;

	bool IsValid() const { return CurrentMode != ENiagaraVariantMode::None; }
	ENiagaraVariantMode GetMode() const { return CurrentMode; }

	bool operator==(const FNiagaraVariant& Other) const;
	bool operator!=(const FNiagaraVariant& Other) const;

	bool AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const;

private:
	UPROPERTY(EditAnywhere, Category=Variant, Instanced)
	TObjectPtr<UObject> Object;

	UPROPERTY(EditAnywhere, Category=Variant, Instanced)
	TObjectPtr<UNiagaraDataInterface> DataInterface;

	UPROPERTY(EditAnywhere, Category=Variant)
	TArray<uint8> Bytes;

	UPROPERTY(EditAnywhere, Category=Variant)
	ENiagaraVariantMode CurrentMode;
};