// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraDataInterfaceArray.h"
#include "NiagaraDataInterfaceArrayImpl.h"
#include "NiagaraDataInterfaceArrayInt.generated.h"

template<>
struct FNDIArrayImplHelper<int32> : public FNDIArrayImplHelperBase<int32>
{
	typedef int32 TVMArrayType;

	static constexpr bool bSupportsAtomicOps = true;

	static constexpr TCHAR const* HLSLVariableType = TEXT("int");
	static constexpr EPixelFormat ReadPixelFormat = PF_R32_SINT;
	static constexpr TCHAR const* ReadHLSLBufferType = TEXT("int");
	static constexpr TCHAR const* ReadHLSLBufferRead = TEXT("Value = BUFFER_NAME[Index];");
	static constexpr EPixelFormat RWPixelFormat = PF_R32_SINT;
	static constexpr TCHAR const* RWHLSLBufferType = TEXT("int");
	static constexpr TCHAR const* RWHLSLBufferRead = TEXT("Value = BUFFER_NAME[Index];");
	static constexpr TCHAR const* RWHLSLBufferWrite = TEXT("BUFFER_NAME[Index] = Value;");

	static const FNiagaraTypeDefinition& GetTypeDefinition() { return FNiagaraTypeDefinition::GetIntDef(); }
	static const int32 GetDefaultValue() { return 0; }

	static int32 AtomicAdd(int32* Dest, int32 Value)
	{
		return FPlatformAtomics::InterlockedAdd(Dest, Value);
	}
	static int32 AtomicMin(int32* Dest, int32 Value)
	{
		int32 PrevValue = *Dest;
		while (PrevValue > Value)
		{
			PrevValue = FPlatformAtomics::InterlockedCompareExchange(Dest, Value, PrevValue);
		}
		return PrevValue;
	}
	static int32 AtomicMax(int32* Dest, int32 Value)
	{
		int32 PrevValue = *Dest;
		while (PrevValue < Value)
		{
			PrevValue = FPlatformAtomics::InterlockedCompareExchange(Dest, Value, PrevValue);
		}
		return PrevValue;
	}
};

template<>
struct FNDIArrayImplHelper<uint8> : public FNDIArrayImplHelperBase<uint8>
{
	typedef int32 TVMArrayType;

	static constexpr TCHAR const* HLSLVariableType = TEXT("int");
	static constexpr EPixelFormat ReadPixelFormat = PF_R8_UINT;
	static constexpr TCHAR const* ReadHLSLBufferType = TEXT("int");
	static constexpr TCHAR const* ReadHLSLBufferRead = TEXT("Value = BUFFER_NAME[Index];");
	static constexpr EPixelFormat RWPixelFormat = PF_R8_UINT;
	static constexpr TCHAR const* RWHLSLBufferType = TEXT("int");
	static constexpr TCHAR const* RWHLSLBufferRead = TEXT("Value = BUFFER_NAME[Index];");
	static constexpr TCHAR const* RWHLSLBufferWrite = TEXT("BUFFER_NAME[Index] = Value;");

	static const FNiagaraTypeDefinition& GetTypeDefinition() { return FNiagaraTypeDefinition::GetIntDef(); }
	static const int32 GetDefaultValue() { return 0; }

	static void CopyCpuToCpuMemory(uint8* Dest, const uint8* Src, int32 NumElements)
	{
		FMemory::Memcpy(Dest, Src, NumElements * sizeof(uint8));
	}

	static void CopyCpuToCpuMemory(uint8* Dest, const int32* Src, int32 NumElements)
	{
		for (int32 i = 0; i < NumElements; ++i)
		{
			Dest[i] = uint8(Src[i]);
		}
	}

	static void CopyCpuToCpuMemory(int32* Dest, const uint8* Src, int32 NumElements)
	{
		for (int32 i = 0; i < NumElements; ++i)
		{
			Dest[i] = int32(Src[i]);
		}
	}
};

template<>
struct FNDIArrayImplHelper<bool> : public FNDIArrayImplHelperBase<bool>
{
	typedef FNiagaraBool TVMArrayType;

	static constexpr TCHAR const* HLSLVariableType = TEXT("bool");
	static constexpr EPixelFormat ReadPixelFormat = PF_R8_UINT;
	static constexpr TCHAR const* ReadHLSLBufferType = TEXT("uint");
	static constexpr TCHAR const* ReadHLSLBufferRead = TEXT("Value = BUFFER_NAME[Index];");
	static constexpr EPixelFormat RWPixelFormat = PF_R8_UINT;
	static constexpr TCHAR const* RWHLSLBufferType = TEXT("uint");
	static constexpr TCHAR const* RWHLSLBufferRead = TEXT("Value = BUFFER_NAME[Index];");
	static constexpr TCHAR const* RWHLSLBufferWrite = TEXT("BUFFER_NAME[Index] = Value;");

	static const FNiagaraTypeDefinition& GetTypeDefinition() { return FNiagaraTypeDefinition::GetBoolDef(); }
	static const FNiagaraBool GetDefaultValue() { return FNiagaraBool::False; }

	static void CopyCpuToGpuMemory(void* Dest, const bool* Src, int32 NumElements)
	{
		uint8* TypedDest = reinterpret_cast<uint8*>(Dest);
		while (NumElements--)
		{
			*TypedDest++ = *Src++ == false ? 0 : 0xff;
		}
	}

	static void CopyGpuToCpuMemory(void* Dest, const void* Src, int32 NumElements)
	{
		FNiagaraBool* TypedDest = reinterpret_cast<FNiagaraBool*>(Dest);
		const uint8* TypedSrc = reinterpret_cast<const uint8*>(Src);
		while (NumElements--)
		{
			*TypedDest++ = *TypedSrc++ == 0 ? false : true;
		}
	}
};

UCLASS(EditInlineNew, Category = "Array", meta = (DisplayName = "Int32 Array"), Blueprintable, BlueprintType)
class NIAGARA_API UNiagaraDataInterfaceArrayInt32 : public UNiagaraDataInterfaceArray
{
public:
	GENERATED_BODY()
	NDIARRAY_GENERATE_BODY(UNiagaraDataInterfaceArrayInt32, int32, IntData)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Array")
	TArray<int32> IntData;
};

UCLASS(EditInlineNew, Category = "Array", meta = (DisplayName = "UInt8 Array"), Blueprintable, BlueprintType)
class NIAGARA_API UNiagaraDataInterfaceArrayUInt8 : public UNiagaraDataInterfaceArray
{
public:
	GENERATED_BODY()
	NDIARRAY_GENERATE_BODY_LWC(UNiagaraDataInterfaceArrayUInt8, uint8, IntData)

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Array")
	TArray<int32> IntData;
#endif

	UPROPERTY()
	TArray<uint8> InternalIntData;
};

UCLASS(EditInlineNew, Category = "Array", meta = (DisplayName = "Bool Array"), Blueprintable, BlueprintType)
class NIAGARA_API UNiagaraDataInterfaceArrayBool : public UNiagaraDataInterfaceArray
{
public:
	GENERATED_BODY()
	NDIARRAY_GENERATE_BODY(UNiagaraDataInterfaceArrayBool, bool, BoolData)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Array")
	TArray<bool> BoolData;
};
