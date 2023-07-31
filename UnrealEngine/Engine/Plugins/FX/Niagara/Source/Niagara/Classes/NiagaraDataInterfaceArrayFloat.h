// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraDataInterfaceArray.h"
#include "NiagaraDataInterfaceArrayImpl.h"
#include "NiagaraDataInterfaceArrayFloat.generated.h"

template<>
struct FNDIArrayImplHelper<float> : public FNDIArrayImplHelperBase<float>
{
	typedef float TVMArrayType;

	static constexpr TCHAR const* HLSLVariableType = TEXT("float");
	static constexpr EPixelFormat ReadPixelFormat = PF_R32_FLOAT;
	static constexpr TCHAR const* ReadHLSLBufferType = TEXT("float");
	static constexpr TCHAR const* ReadHLSLBufferRead = TEXT("Value = BUFFER_NAME[Index];");
	static constexpr EPixelFormat RWPixelFormat = PF_R32_FLOAT;
	static constexpr TCHAR const* RWHLSLBufferType = TEXT("float");
	static constexpr TCHAR const* RWHLSLBufferRead = TEXT("Value = BUFFER_NAME[Index];");
	static constexpr TCHAR const* RWHLSLBufferWrite = TEXT("BUFFER_NAME[Index] = Value;");

	static const FNiagaraTypeDefinition& GetTypeDefinition() { return FNiagaraTypeDefinition::GetFloatDef(); }
	static const float GetDefaultValue() { return 0.0f; }
};

template<>
struct FNDIArrayImplHelper<FVector2f> : public FNDIArrayImplHelperBase<FVector2f>
{
	typedef FVector2f TVMArrayType;

	static constexpr TCHAR const* HLSLVariableType = TEXT("float2");
	static constexpr EPixelFormat ReadPixelFormat = PF_G32R32F;
	static constexpr TCHAR const* ReadHLSLBufferType = TEXT("float2");
	static constexpr TCHAR const* ReadHLSLBufferRead = TEXT("Value = float2(BUFFER_NAME[Index]);");
	static constexpr EPixelFormat RWPixelFormat = PF_R32_FLOAT;
	static constexpr TCHAR const* RWHLSLBufferType = TEXT("float");
	static constexpr TCHAR const* RWHLSLBufferRead = TEXT("Value = float2(BUFFER_NAME[Index * 2 + 0], BUFFER_NAME[Index * 2 + 1]);");
	static constexpr TCHAR const* RWHLSLBufferWrite = TEXT("BUFFER_NAME[Index * 2 + 0] = Value.x, BUFFER_NAME[Index * 2 + 1] = Value.y;");

	static const FNiagaraTypeDefinition& GetTypeDefinition() { return FNiagaraTypeDefinition::GetVec2Def(); }
	static const FVector2f GetDefaultValue() { return FVector2f::ZeroVector; }

	static void CopyCpuToCpuMemory(FVector2f* Dest, const FVector2f* Src, int32 NumElements)
	{
		FMemory::Memcpy(Dest, Src, NumElements * sizeof(FVector2f));
	}

	static void CopyCpuToCpuMemory(FVector2f* Dest, const FVector2D* Src, int32 NumElements)
	{
		for ( int32 i=0; i < NumElements; ++i )
		{
			Dest[i] = FVector2f(Src[i]);
		}
	}

	static void CopyCpuToCpuMemory(FVector2D* Dest, const FVector2f* Src, int32 NumElements)
	{
		for (int32 i = 0; i < NumElements; ++i)
		{
			Dest[i] = FVector2D(Src[i]);
		}
	}
};

template<>
struct FNDIArrayImplHelper<FVector3f> : public FNDIArrayImplHelperBase<FVector3f>
{
	typedef FVector3f TVMArrayType;

	static constexpr TCHAR const* HLSLVariableType = TEXT("float3");
	static constexpr EPixelFormat ReadPixelFormat = PF_R32_FLOAT;		// Lack of float3 format
	static constexpr TCHAR const* ReadHLSLBufferType = TEXT("float");
	static constexpr TCHAR const* ReadHLSLBufferRead = TEXT("Value = float3(BUFFER_NAME[Index * 3 + 0], BUFFER_NAME[Index * 3 + 1], BUFFER_NAME[Index * 3 + 2]);");
	static constexpr EPixelFormat RWPixelFormat = PF_R32_FLOAT;
	static constexpr TCHAR const* RWHLSLBufferType = TEXT("float");
	static constexpr TCHAR const* RWHLSLBufferRead = TEXT("Value = float3(BUFFER_NAME[Index * 3 + 0], BUFFER_NAME[Index * 3 + 1], BUFFER_NAME[Index * 3 + 2]);");
	static constexpr TCHAR const* RWHLSLBufferWrite = TEXT("BUFFER_NAME[Index * 3 + 0] = Value.x, BUFFER_NAME[Index * 3 + 1] = Value.y, BUFFER_NAME[Index * 3 + 2] = Value.z;");

	static const FNiagaraTypeDefinition& GetTypeDefinition() { return FNiagaraTypeDefinition::GetVec3Def(); }
	static const FVector3f GetDefaultValue() { return FVector3f::ZeroVector; }

	static void CopyCpuToCpuMemory(FVector3f* Dest, const FVector3f* Src, int32 NumElements)
	{
		FMemory::Memcpy(Dest, Src, NumElements * sizeof(FVector3f));
	}

	static void CopyCpuToCpuMemory(FVector3f* Dest, const FVector* Src, int32 NumElements)
	{
		for (int32 i = 0; i < NumElements; ++i)
		{
			Dest[i] = FVector3f(Src[i]);
		}
	}

	static void CopyCpuToCpuMemory(FVector* Dest, const FVector3f* Src, int32 NumElements)
	{
		for (int32 i = 0; i < NumElements; ++i)
		{
			Dest[i] = FVector(Src[i]);
		}
	}
};

template<>
struct FNDIArrayImplHelper<FNiagaraPosition> : public FNDIArrayImplHelper<FVector3f>
{
	typedef FNiagaraPosition TVMArrayType;

	static const FNiagaraTypeDefinition& GetTypeDefinition() { return FNiagaraTypeDefinition::GetPositionDef(); }
	static const FNiagaraPosition GetDefaultValue() { return FVector3f::ZeroVector; }

	static void CopyCpuToCpuMemory(FVector3f* Dest, const FNiagaraPosition* Src, int32 NumElements)
	{
		for (int32 i = 0; i < NumElements; ++i)
		{
			Dest[i] = Src[i];
		}
	}
};

template<>
struct FNDIArrayImplHelper<FVector4f> : public FNDIArrayImplHelperBase<FVector4f>
{
	typedef FVector4f TVMArrayType;

	static constexpr TCHAR const* HLSLVariableType = TEXT("float4");
	static constexpr EPixelFormat ReadPixelFormat = PF_A32B32G32R32F;
	static constexpr TCHAR const* ReadHLSLBufferType = TEXT("float4");
	static constexpr TCHAR const* ReadHLSLBufferRead = TEXT("Value = BUFFER_NAME[Index];");
	static constexpr EPixelFormat RWPixelFormat = PF_R32_FLOAT;
	static constexpr TCHAR const* RWHLSLBufferType = TEXT("float");
	static constexpr TCHAR const* RWHLSLBufferRead = TEXT("Value = float4(BUFFER_NAME[Index * 4 + 0], BUFFER_NAME[Index * 4 + 1], BUFFER_NAME[Index * 4 + 2], BUFFER_NAME[Index * 4 + 3]);");
	static constexpr TCHAR const* RWHLSLBufferWrite = TEXT("BUFFER_NAME[Index * 4 + 0] = Value.x, BUFFER_NAME[Index * 4 + 1] = Value.y, BUFFER_NAME[Index * 4 + 2] = Value.z, BUFFER_NAME[Index * 4 + 3] = Value.w;");

	static const FNiagaraTypeDefinition& GetTypeDefinition() { return FNiagaraTypeDefinition::GetVec4Def(); }
	static const FVector4f GetDefaultValue() { return FVector4f(ForceInitToZero); }

	static void CopyCpuToCpuMemory(FVector4f* Dest, const FVector4f* Src, int32 NumElements)
	{
		FMemory::Memcpy(Dest, Src, NumElements * sizeof(FVector4f));
	}

	static void CopyCpuToCpuMemory(FVector4f* Dest, const FVector4* Src, int32 NumElements)
	{
		for (int32 i = 0; i < NumElements; ++i)
		{
			Dest[i] = FVector4f(Src[i]);
		}
	}

	static void CopyCpuToCpuMemory(FVector4* Dest, const FVector4f* Src, int32 NumElements)
	{
		for (int32 i = 0; i < NumElements; ++i)
		{
			Dest[i] = FVector4(Src[i]);
		}
	}
};

template<>
struct FNDIArrayImplHelper<FLinearColor> : public FNDIArrayImplHelperBase<FLinearColor>
{
	typedef FLinearColor TVMArrayType;

	static constexpr TCHAR const* HLSLVariableType = TEXT("float4");
	static constexpr EPixelFormat ReadPixelFormat = PF_A32B32G32R32F;
	static constexpr TCHAR const* ReadHLSLBufferType = TEXT("float4");
	static constexpr TCHAR const* ReadHLSLBufferRead = TEXT("Value = BUFFER_NAME[Index];");
	static constexpr EPixelFormat RWPixelFormat = PF_R32_FLOAT;
	static constexpr TCHAR const* RWHLSLBufferType = TEXT("float");
	static constexpr TCHAR const* RWHLSLBufferRead = TEXT("Value = float4(BUFFER_NAME[Index * 4 + 0], BUFFER_NAME[Index * 4 + 1], BUFFER_NAME[Index * 4 + 2], BUFFER_NAME[Index * 4 + 3]);");
	static constexpr TCHAR const* RWHLSLBufferWrite = TEXT("BUFFER_NAME[Index * 4 + 0] = Value.x, BUFFER_NAME[Index * 4 + 1] = Value.y, BUFFER_NAME[Index * 4 + 2] = Value.z, BUFFER_NAME[Index * 4 + 3] = Value.w;");

	static const FNiagaraTypeDefinition& GetTypeDefinition() { return FNiagaraTypeDefinition::GetColorDef(); }
	static const FLinearColor GetDefaultValue() { return FLinearColor::White; }
};

template<>
struct FNDIArrayImplHelper<FQuat4f> : public FNDIArrayImplHelperBase<FQuat4f>
{
	typedef FQuat4f TVMArrayType;

	static constexpr TCHAR const* HLSLVariableType = TEXT("float4");
	static constexpr EPixelFormat ReadPixelFormat = PF_A32B32G32R32F;
	static constexpr TCHAR const* ReadHLSLBufferType = TEXT("float4");
	static constexpr TCHAR const* ReadHLSLBufferRead = TEXT("Value = BUFFER_NAME[Index];");
	static constexpr EPixelFormat RWPixelFormat = PF_R32_FLOAT;
	static constexpr TCHAR const* RWHLSLBufferType = TEXT("float");
	static constexpr TCHAR const* RWHLSLBufferRead = TEXT("Value = float4(BUFFER_NAME[Index * 4 + 0], BUFFER_NAME[Index * 4 + 1], BUFFER_NAME[Index * 4 + 2], BUFFER_NAME[Index * 4 + 3]);");
	static constexpr TCHAR const* RWHLSLBufferWrite = TEXT("BUFFER_NAME[Index * 4 + 0] = Value.x, BUFFER_NAME[Index * 4 + 1] = Value.y, BUFFER_NAME[Index * 4 + 2] = Value.z, BUFFER_NAME[Index * 4 + 3] = Value.w;");

	static const FNiagaraTypeDefinition& GetTypeDefinition() { return FNiagaraTypeDefinition::GetQuatDef(); }
	static const FQuat4f GetDefaultValue() { return FQuat4f::Identity; }

	static void CopyCpuToCpuMemory(FQuat4f* Dest, const FQuat4f* Src, int32 NumElements)
	{
		FMemory::Memcpy(Dest, Src, NumElements * sizeof(FQuat4f));
	}

	static void CopyCpuToCpuMemory(FQuat4f* Dest, const FQuat* Src, int32 NumElements)
	{
		for (int32 i = 0; i < NumElements; ++i)
		{
			Dest[i] = FQuat4f(Src[i]);
		}
	}

	static void CopyCpuToCpuMemory(FQuat* Dest, const FQuat4f* Src, int32 NumElements)
	{
		for (int32 i = 0; i < NumElements; ++i)
		{
			Dest[i] = FQuat(Src[i]);
		}
	}
};

UCLASS(EditInlineNew, Category = "Array", meta = (DisplayName = "Float Array"), Blueprintable, BlueprintType)
class NIAGARA_API UNiagaraDataInterfaceArrayFloat : public UNiagaraDataInterfaceArray
{
public:
	GENERATED_BODY()
	NDIARRAY_GENERATE_BODY(UNiagaraDataInterfaceArrayFloat, float, FloatData)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Array")
	TArray<float> FloatData;
};

UCLASS(EditInlineNew, Category = "Array", meta = (DisplayName = "Vector 2D Array"), Blueprintable, BlueprintType)
class NIAGARA_API UNiagaraDataInterfaceArrayFloat2 : public UNiagaraDataInterfaceArray
{
public:
	GENERATED_BODY()
	NDIARRAY_GENERATE_BODY_LWC(UNiagaraDataInterfaceArrayFloat2, FVector2f, FloatData)

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Array")
	TArray<FVector2D> FloatData;
#endif

	UPROPERTY()
	TArray<FVector2f> InternalFloatData;
};

UCLASS(EditInlineNew, Category = "Array", meta = (DisplayName = "Vector Array"), Blueprintable, BlueprintType)
class NIAGARA_API UNiagaraDataInterfaceArrayFloat3 : public UNiagaraDataInterfaceArray
{
public:
	GENERATED_BODY()
	NDIARRAY_GENERATE_BODY_LWC(UNiagaraDataInterfaceArrayFloat3, FVector3f, FloatData)

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Array")
	TArray<FVector> FloatData;
#endif

	UPROPERTY()
	TArray<FVector3f> InternalFloatData;
};

UCLASS(EditInlineNew, Category = "Array", meta = (DisplayName = "Position Array"), Blueprintable, BlueprintType)
class NIAGARA_API UNiagaraDataInterfaceArrayPosition : public UNiagaraDataInterfaceArray
{
public:
	GENERATED_BODY()
	NDIARRAY_GENERATE_BODY(UNiagaraDataInterfaceArrayPosition, FNiagaraPosition, PositionData)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Array")
	TArray<FNiagaraPosition> PositionData;
};

UCLASS(EditInlineNew, Category = "Array", meta = (DisplayName = "Vector 4 Array"), Blueprintable, BlueprintType)
class NIAGARA_API UNiagaraDataInterfaceArrayFloat4 : public UNiagaraDataInterfaceArray
{
public:
	GENERATED_BODY()
	NDIARRAY_GENERATE_BODY_LWC(UNiagaraDataInterfaceArrayFloat4, FVector4f, FloatData)

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Array")
	TArray<FVector4> FloatData;
#endif

	UPROPERTY()
	TArray<FVector4f> InternalFloatData;
};

UCLASS(EditInlineNew, Category = "Array", meta = (DisplayName = "Color Array"), Blueprintable, BlueprintType)
class NIAGARA_API UNiagaraDataInterfaceArrayColor : public UNiagaraDataInterfaceArray
{
public:
	GENERATED_BODY()
	NDIARRAY_GENERATE_BODY(UNiagaraDataInterfaceArrayColor, FLinearColor, ColorData)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Array")
	TArray<FLinearColor> ColorData;
};

UCLASS(EditInlineNew, Category = "Array", meta = (DisplayName = "Quaternion Array"), Blueprintable, BlueprintType)
class NIAGARA_API UNiagaraDataInterfaceArrayQuat : public UNiagaraDataInterfaceArray
{
public:
	GENERATED_BODY()
	NDIARRAY_GENERATE_BODY_LWC(UNiagaraDataInterfaceArrayQuat, FQuat4f, QuatData)

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Array")
	TArray<FQuat> QuatData;
#endif

	UPROPERTY()
	TArray<FQuat4f> InternalQuatData;
};
