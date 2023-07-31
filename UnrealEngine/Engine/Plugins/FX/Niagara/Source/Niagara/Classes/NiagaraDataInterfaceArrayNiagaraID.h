// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraDataInterfaceArray.h"
#include "NiagaraDataInterfaceArrayImpl.h"
#include "NiagaraDataInterfaceArrayNiagaraID.generated.h"

template<>
struct FNDIArrayImplHelper<FNiagaraID> : public FNDIArrayImplHelperBase<FNiagaraID>
{
	typedef FNiagaraID TVMArrayType;

	static constexpr TCHAR const* HLSLVariableType = TEXT("NiagaraID");
	static constexpr EPixelFormat ReadPixelFormat = PF_R32_SINT;		// Should be int2 but we don't have it
	static constexpr TCHAR const* ReadHLSLBufferType = TEXT("int");
	static constexpr TCHAR const* ReadHLSLBufferRead = TEXT("Value.Index = BUFFER_NAME[Index * 2 + 0]; Value.AcquireTag = BUFFER_NAME[Index * 2 + 1];");
	static constexpr EPixelFormat RWPixelFormat = PF_R32_SINT;
	static constexpr TCHAR const* RWHLSLBufferType = TEXT("int");
	static constexpr TCHAR const* RWHLSLBufferRead = TEXT("Value.Index = BUFFER_NAME[Index * 2 + 0]; Value.AcquireTag = BUFFER_NAME[Index * 2 + 1];");
	static constexpr TCHAR const* RWHLSLBufferWrite = TEXT("BUFFER_NAME[Index * 2 + 0] = Value.Index; BUFFER_NAME[Index * 2 + 1] = Value.AcquireTag;");

	static const FNiagaraTypeDefinition GetTypeDefinition() { return FNiagaraTypeDefinition(FNiagaraID::StaticStruct()); }
	static const FNiagaraID GetDefaultValue() { return FNiagaraID(); }
};

UCLASS(EditInlineNew, Category = "Array", meta = (DisplayName = "NiagaraID Array", Experimental), Blueprintable, BlueprintType)
class NIAGARA_API UNiagaraDataInterfaceArrayNiagaraID : public UNiagaraDataInterfaceArray
{
public:
	GENERATED_BODY()
	NDIARRAY_GENERATE_BODY(UNiagaraDataInterfaceArrayNiagaraID, FNiagaraID, IntData)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Array")
	TArray<FNiagaraID> IntData;
};

