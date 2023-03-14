// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "NiagaraCommon.h"
#include "NiagaraTypes.h"
#include "NiagaraDataSet.h"

template<typename TType>
struct FNiagaraDataSetAccessorTypeInfo;

template<typename TType>
struct FNiagaraDataSetAccessor : public FNiagaraDataSetAccessorTypeInfo<TType>::TAccessorBaseClass
{
	static_assert(!TIsUECoreVariant<TType, double>::Value, "Double core variant. Must be float type!");
	FORCEINLINE FNiagaraDataSetAccessor<TType>() {}
	FORCEINLINE explicit FNiagaraDataSetAccessor<TType>(const FNiagaraDataSet& DataSet, const FName VariableName) { FNiagaraDataSetAccessorTypeInfo<TType>::TAccessorBaseClass::Init(DataSet.GetCompiledData(), VariableName); }
	FORCEINLINE explicit FNiagaraDataSetAccessor<TType>(const FNiagaraDataSetCompiledData& DataSetCompiledData, const FName VariableName) { FNiagaraDataSetAccessorTypeInfo<TType>::TAccessorBaseClass::Init(DataSetCompiledData, VariableName); }
};

//////////////////////////////////////////////////////////////////////////
// Float / Half readers

template<typename TType>
struct FNiagaraDataSetReaderFloat
{
	static_assert(!TIsUECoreVariant<TType, double>::Value, "Double core variant. Must be float type!");
	static constexpr bool bSupportsHalf = FNiagaraDataSetAccessorTypeInfo<TType>::bSupportsHalf;

	FORCEINLINE FNiagaraDataSetReaderFloat() {}

	explicit FNiagaraDataSetReaderFloat(FNiagaraDataBuffer* DataBuffer, bool bInIsFloat, int32 ComponentIndex)
		: bIsFloat(bInIsFloat)
	{
		if (DataBuffer != nullptr && ComponentIndex != INDEX_NONE)
		{
			checkf(bInIsFloat || bSupportsHalf, TEXT("Must be float if we do not support halfs, how has this happened?"));
			bIsValid = true;
			for (int i = 0; i < FNiagaraDataSetAccessorTypeInfo<TType>::NumElements; ++i)
			{
				ComponentData[i] = bIsFloat ? DataBuffer->GetComponentPtrFloat(ComponentIndex + i) : DataBuffer->GetComponentPtrHalf(ComponentIndex + i);
			}
			NumInstances = DataBuffer->GetNumInstances();
		}
	}

	FORCEINLINE bool IsValid() const { return bIsValid; }

	FORCEINLINE TType operator[](int32 Index) const
	{
		return Get(Index);
	}

	FORCEINLINE TType Get(int Index) const
	{
		check(bIsValid && Index <= NumInstances);
		TType Value;
		if (!bSupportsHalf || bIsFloat)
		{
			for (int i = 0; i < FNiagaraDataSetAccessorTypeInfo<TType>::NumElements; ++i)
			{
				reinterpret_cast<float*>(&Value)[i] = reinterpret_cast<const float*>(ComponentData[i])[Index];
			}
		}
		else
		{
			for (int i = 0; i < FNiagaraDataSetAccessorTypeInfo<TType>::NumElements; ++i)
			{
				reinterpret_cast<float*>(&Value)[i] = FPlatformMath::LoadHalf(&reinterpret_cast<const uint16*>(ComponentData[i])[Index]);
			}
		}
		return Value;
	}

	FORCEINLINE TType GetSafe(int Index, const TType& Default) const
	{
		if (bIsValid && (Index < NumInstances))
		{
			return Get(Index);
		}
		return Default;
	}

	FORCEINLINE float GetMaxElement(int32 ElementIndex) const
	{
		check(IsValid());
		check(ElementIndex < FNiagaraDataSetAccessorTypeInfo<TType>::NumElements);
		if (!bSupportsHalf || bIsFloat)
		{
			float MaxValue = -FLT_MAX;
			for (int i = 0; i < NumInstances; ++i)
			{
				MaxValue = FMath::Max(MaxValue, reinterpret_cast<const float*>(ComponentData[ElementIndex])[i]);
			}
			return MaxValue;
		}
		else
		{
			int16 MaxValue = static_cast<int16>(0xfc00u);
			for (int i = 0; i < NumInstances; ++i)
			{
				MaxValue = FMath::Max(MaxValue, reinterpret_cast<const int16*>(ComponentData[ElementIndex])[i]);
			}
			return FPlatformMath::LoadHalf((uint16*)&MaxValue);
		}
	}

	FORCEINLINE void GetMinMaxElement(int32 ElementIndex, float& MinValueOut, float& MaxValueOut) const
	{
		check(IsValid());
		check(ElementIndex < FNiagaraDataSetAccessorTypeInfo<TType>::NumElements);
		if (!bSupportsHalf || bIsFloat)
		{
			float MinValue = FLT_MAX;
			float MaxValue = -FLT_MAX;
			for (int i = 0; i < NumInstances; ++i)
			{
				MinValue = FMath::Min(MinValue, reinterpret_cast<const float*>(ComponentData[ElementIndex])[i]);
				MaxValue = FMath::Max(MaxValue, reinterpret_cast<const float*>(ComponentData[ElementIndex])[i]);
			}
			MinValueOut = MinValue;
			MaxValueOut = MaxValue;
		}
		else
		{
			int16 MinValue = static_cast<int16>(0x7c00u);
			int16 MaxValue = static_cast<int16>(0xfc00u);
			for (int i = 0; i < NumInstances; ++i)
			{
				MinValue = FMath::Min(MinValue, reinterpret_cast<const int16*>(ComponentData[ElementIndex])[i]);
				MaxValue = FMath::Max(MaxValue, reinterpret_cast<const int16*>(ComponentData[ElementIndex])[i]);
			}
			MinValueOut = FPlatformMath::LoadHalf((uint16*)&MinValue);
			MaxValueOut = FPlatformMath::LoadHalf((uint16*)&MaxValue);
		}
	}

	FORCEINLINE TType GetMax() const
	{
		TType Value;
		for (int i = 0; i < FNiagaraDataSetAccessorTypeInfo<TType>::NumElements; ++i)
		{
			reinterpret_cast<float*>(&Value)[i] = GetMaxElement(i);
		}
		return Value;
	}

	FORCEINLINE void GetMinMax(TType& Min, TType& Max) const
	{
		for (int i = 0; i < FNiagaraDataSetAccessorTypeInfo<TType>::NumElements; ++i)
		{
			GetMinMaxElement(i, reinterpret_cast<float*>(&Min)[i], reinterpret_cast<float*>(&Max)[i]);
		}
	}

private:
	bool bIsValid = false;
	bool bIsFloat = true;
	int32 NumInstances = 0;
	const uint8* ComponentData[FNiagaraDataSetAccessorTypeInfo<TType>::NumElements] = {};
};

//template<typename TType>
//struct FNiagaraDataSetWriterFloat
//{
//};

template<typename TType>
struct FNiagaraDataSetAccessorFloat
{
	static_assert(!TIsUECoreVariant<TType, double>::Value, "Double core variant. Must be float type!");
	FORCEINLINE FNiagaraDataSetAccessorFloat() {}
	FORCEINLINE explicit FNiagaraDataSetAccessorFloat(const FNiagaraDataSet& DataSet, const FName VariableName) { Init(DataSet.GetCompiledData(), VariableName); }
	FORCEINLINE explicit FNiagaraDataSetAccessorFloat(const FNiagaraDataSetCompiledData& DataSetCompiledData, const FName VariableName) { Init(DataSetCompiledData, VariableName); }
	FORCEINLINE explicit FNiagaraDataSetAccessorFloat(const FNiagaraDataSetCompiledData* DataSetCompiledData, const FName VariableName) { Init(DataSetCompiledData, VariableName); }
	FORCEINLINE void Init(const FNiagaraDataSet& DataSet, const FName VariableName) { Init(DataSet.GetCompiledData(), VariableName); }

	void Init(const FNiagaraDataSetCompiledData* DataSetCompiledData, const FName VariableName)
	{
		if (DataSetCompiledData != nullptr)
		{
			Init(*DataSetCompiledData, VariableName);
		}
		else
		{
			bIsFloat = true;
			ComponentIndex = INDEX_NONE;
		}
	}

	void Init(const FNiagaraDataSetCompiledData& DataSetCompiledData, const FName VariableName)
	{
		ComponentIndex = INDEX_NONE;

		const int32 FloatVariableIndex = DataSetCompiledData.Variables.IndexOfByPredicate(FNiagaraVariableMatch(FNiagaraDataSetAccessorTypeInfo<TType>::GetFloatType(), VariableName));
		if (FloatVariableIndex != INDEX_NONE)
		{
			bIsFloat = true;
			ComponentIndex = DataSetCompiledData.VariableLayouts[FloatVariableIndex].FloatComponentStart;
			return;
		}

		if (FNiagaraDataSetAccessorTypeInfo<TType>::bSupportsAlternateType)
		{
			const int32 AlternateVariableIndex = DataSetCompiledData.Variables.IndexOfByPredicate(FNiagaraVariableMatch(FNiagaraDataSetAccessorTypeInfo<TType>::GetAlternateType(), VariableName));
			if (AlternateVariableIndex != INDEX_NONE)
			{
				bIsFloat = true;
				ComponentIndex = DataSetCompiledData.VariableLayouts[AlternateVariableIndex].FloatComponentStart;
				return;
			}
		}
		
		if (FNiagaraDataSetAccessorTypeInfo<TType>::bSupportsHalf)
		{
			const int32 HalfVariableIndex = DataSetCompiledData.Variables.IndexOfByPredicate(FNiagaraVariableMatch(FNiagaraDataSetAccessorTypeInfo<TType>::GetHalfType(), VariableName));
			if (HalfVariableIndex != INDEX_NONE)
			{
				bIsFloat = false;
				ComponentIndex = DataSetCompiledData.VariableLayouts[HalfVariableIndex].HalfComponentStart;
			}
		}
	}

	FORCEINLINE bool IsValid() const { return ComponentIndex != INDEX_NONE; }

	FORCEINLINE static FNiagaraDataSetReaderFloat<TType> CreateReader(const FNiagaraDataSet& DataSet, const FName VariableName) { return FNiagaraDataSetAccessorFloat<TType>(DataSet, VariableName).GetReader(DataSet); }
	//FORCEINLINE static FNiagaraDataSetWriterFloat<TType> CreateWriter(const FNiagaraDataSet& DataSet, const FName VariableName) { return FNiagaraDataSetAccessorFloat<TType>(DataSet, VariableName).GetWriter(DataSet); }

	FORCEINLINE FNiagaraDataSetReaderFloat<TType> GetReader(const FNiagaraDataSet& DataSet) const { return FNiagaraDataSetReaderFloat<TType>(DataSet.GetCurrentData(), bIsFloat, ComponentIndex); }
	//FORCEINLINE FNiagaraDataSetWriterFloat<TType> GetWriter(const FNiagaraDataSet& DataSet) const { return FNiagaraDataSetWriterFloat<TType>(DataSet.GetDestinationData(), bIsFloat, ComponentIndex); }

private:
	bool bIsFloat = true;
	int32 ComponentIndex = INDEX_NONE;
};

template<>
struct FNiagaraDataSetAccessorTypeInfo<float>
{
	using TAccessorBaseClass = FNiagaraDataSetAccessorFloat<float>;

	static constexpr bool bSupportsHalf = true;
	static constexpr bool bSupportsAlternateType = false;
	static constexpr int32 NumElements = 1;
	static const FNiagaraTypeDefinition& GetFloatType() { return FNiagaraTypeDefinition::GetFloatDef(); }
	static const FNiagaraTypeDefinition& GetHalfType() { return FNiagaraTypeDefinition::GetHalfDef(); }
	static const FNiagaraTypeDefinition& GetAlternateType() { return FNiagaraTypeDefinition::GetFloatDef(); }
};

template<>
struct FNiagaraDataSetAccessorTypeInfo<FVector2f>
{
	using TAccessorBaseClass = FNiagaraDataSetAccessorFloat<FVector2f>;

	static constexpr bool bSupportsHalf = true;
	static constexpr bool bSupportsAlternateType = false;
	static constexpr int32 NumElements = 2;
	static const FNiagaraTypeDefinition& GetFloatType() { return FNiagaraTypeDefinition::GetVec2Def(); }
	static const FNiagaraTypeDefinition& GetHalfType() { return FNiagaraTypeDefinition::GetHalfVec2Def(); }
	static const FNiagaraTypeDefinition& GetAlternateType() { return FNiagaraTypeDefinition::GetVec2Def(); }
};

template<>
struct FNiagaraDataSetAccessorTypeInfo<FVector3f>
{
	using TAccessorBaseClass = FNiagaraDataSetAccessorFloat<FVector3f>;

	static constexpr bool bSupportsHalf = true;
	static constexpr bool bSupportsAlternateType = false;
	static constexpr int32 NumElements = 3;
	static const FNiagaraTypeDefinition& GetFloatType() { return FNiagaraTypeDefinition::GetVec3Def(); }
	static const FNiagaraTypeDefinition& GetHalfType() { return FNiagaraTypeDefinition::GetHalfVec3Def(); }
	static const FNiagaraTypeDefinition& GetAlternateType() { return FNiagaraTypeDefinition::GetVec3Def(); }
};

template<>
struct FNiagaraDataSetAccessorTypeInfo<FNiagaraPosition>
{
	using TAccessorBaseClass = FNiagaraDataSetAccessorFloat<FNiagaraPosition>;

	static constexpr bool bSupportsHalf = true;
	static constexpr bool bSupportsAlternateType = true;
	static constexpr int32 NumElements = 3;
	static const FNiagaraTypeDefinition& GetFloatType() { return FNiagaraTypeDefinition::GetPositionDef(); }
	static const FNiagaraTypeDefinition& GetHalfType() { return FNiagaraTypeDefinition::GetHalfVec3Def(); }
	static const FNiagaraTypeDefinition& GetAlternateType() { return FNiagaraTypeDefinition::GetVec3Def(); }
};

template<>
struct FNiagaraDataSetAccessorTypeInfo<FVector4f>
{
	using TAccessorBaseClass = FNiagaraDataSetAccessorFloat<FVector4f>;

	static constexpr bool bSupportsHalf = true;
	static constexpr bool bSupportsAlternateType = false;
	static constexpr int32 NumElements = 4;
	static const FNiagaraTypeDefinition& GetFloatType() { return FNiagaraTypeDefinition::GetVec4Def(); }
	static const FNiagaraTypeDefinition& GetHalfType() { return FNiagaraTypeDefinition::GetHalfVec4Def(); }
	static const FNiagaraTypeDefinition& GetAlternateType() { return FNiagaraTypeDefinition::GetVec4Def(); }
};

template<>
struct FNiagaraDataSetAccessorTypeInfo<FLinearColor>
{
	using TAccessorBaseClass = FNiagaraDataSetAccessorFloat<FLinearColor>;

	static constexpr bool bSupportsHalf = false;
	static constexpr bool bSupportsAlternateType = false;
	static constexpr int32 NumElements = 4;
	static const FNiagaraTypeDefinition& GetFloatType() { return FNiagaraTypeDefinition::GetColorDef(); }
	static const FNiagaraTypeDefinition& GetHalfType() { check(false); return FNiagaraTypeDefinition::GetHalfVec4Def(); }
	static const FNiagaraTypeDefinition& GetAlternateType() { return FNiagaraTypeDefinition::GetColorDef(); }
};

template<>
struct FNiagaraDataSetAccessorTypeInfo<FQuat4f>
{
	using TAccessorBaseClass = FNiagaraDataSetAccessorFloat<FQuat4f>;

	static constexpr bool bSupportsHalf = false;
	static constexpr bool bSupportsAlternateType = false;
	static constexpr int32 NumElements = 4;
	static const FNiagaraTypeDefinition& GetFloatType() { return FNiagaraTypeDefinition::GetQuatDef(); }
	static const FNiagaraTypeDefinition& GetHalfType() { check(false); return FNiagaraTypeDefinition::GetQuatDef(); }
	static const FNiagaraTypeDefinition& GetAlternateType() { return FNiagaraTypeDefinition::GetQuatDef(); }
};

//////////////////////////////////////////////////////////////////////////
// Integer readers

template<typename TType>
struct FNiagaraDataSetReaderInt32
{
	FORCEINLINE FNiagaraDataSetReaderInt32() { }

	explicit FNiagaraDataSetReaderInt32(FNiagaraDataBuffer* DataBuffer, int32 ComponentIndex)
	{
		if (DataBuffer != nullptr && ComponentIndex != INDEX_NONE)
		{
			bIsValid = true;
			for (int i = 0; i < FNiagaraDataSetAccessorTypeInfo<TType>::NumElements; ++i)
			{
				ComponentData[i] = DataBuffer->GetComponentPtrInt32(ComponentIndex + i);
			}
			NumInstances = DataBuffer->GetNumInstances();
		}
	}

	FORCEINLINE bool IsValid() const { return bIsValid; }

	FORCEINLINE TType operator[](int32 Index) const
	{
		return Get(Index);
	}

	FORCEINLINE TType Get(int Index) const
	{
		check(bIsValid && Index <= NumInstances);
		TType Value;
		for (int i = 0; i < FNiagaraDataSetAccessorTypeInfo<TType>::NumElements; ++i)
		{
			reinterpret_cast<int32*>(&Value)[i] = reinterpret_cast<const int32*>(ComponentData[i])[Index];
		}
		return Value;
	}

	FORCEINLINE TType GetSafe(int Index, const TType& Default) const
	{
		if (bIsValid && (Index < NumInstances))
		{
			return Get(Index);
		}
		return Default;
	}

private:
	bool bIsValid = false;
	int32 NumInstances = 0;
	const uint8* ComponentData[FNiagaraDataSetAccessorTypeInfo<TType>::NumElements] = {};
};

template<typename TType>
struct FNiagaraDataSetWriterInt32
{
	FORCEINLINE FNiagaraDataSetWriterInt32() { }

	explicit FNiagaraDataSetWriterInt32(FNiagaraDataBuffer* DataBuffer, int32 ComponentIndex)
	{
		if (DataBuffer != nullptr && ComponentIndex != INDEX_NONE)
		{
			bIsValid = true;
			for (int i = 0; i < FNiagaraDataSetAccessorTypeInfo<TType>::NumElements; ++i)
			{
				ComponentData[i] = DataBuffer->GetComponentPtrInt32(ComponentIndex + i);
			}
			NumInstances = DataBuffer->GetNumInstances();
		}
	}

	FORCEINLINE bool IsValid() const { return bIsValid; }

	FORCEINLINE void Set(int Index, const TType& Value)
	{
		check(bIsValid && Index <= NumInstances);
		for (int i = 0; i < FNiagaraDataSetAccessorTypeInfo<TType>::NumElements; ++i)
		{
			reinterpret_cast<int32*>(ComponentData[i])[Index] = reinterpret_cast<const int32*>(&Value)[i];
		}
	}

	FORCEINLINE void SetSafe(int Index, const TType& Value)
	{
		if (bIsValid && Index <= NumInstances)
		{
			for (int i = 0; i < FNiagaraDataSetAccessorTypeInfo<TType>::NumElements; ++i)
			{
				reinterpret_cast<int32*>(ComponentData[i])[Index] = reinterpret_cast<const int32*>(&Value)[i];
			}
		}
	}

private:
	bool bIsValid = false;
	int32 NumInstances = 0;
	uint8* ComponentData[FNiagaraDataSetAccessorTypeInfo<TType>::NumElements] = {};
};

//template<typename TType>
//struct FNiagaraDataSetWriterInt
//{
//};

template<typename TType>
struct FNiagaraDataSetAccessorInt32
{
	FORCEINLINE FNiagaraDataSetAccessorInt32() {}
	FORCEINLINE explicit FNiagaraDataSetAccessorInt32(const FNiagaraDataSet& DataSet, const FName VariableName) { Init(DataSet.GetCompiledData(), VariableName); }
	FORCEINLINE explicit FNiagaraDataSetAccessorInt32(const FNiagaraDataSetCompiledData& DataSetCompiledData, const FName VariableName) { Init(DataSetCompiledData, VariableName); }
	FORCEINLINE explicit FNiagaraDataSetAccessorInt32(const FNiagaraDataSetCompiledData* DataSetCompiledData, const FName VariableName) { Init(DataSetCompiledData, VariableName); }
	FORCEINLINE void Init(const FNiagaraDataSet& DataSet, const FName VariableName) { Init(DataSet.GetCompiledData(), VariableName); }

	FORCEINLINE void Init(const FNiagaraDataSetCompiledData* DataSetCompiledData, const FName VariableName)
	{
		if (DataSetCompiledData != nullptr)
		{
			Init(*DataSetCompiledData, VariableName);
		}
		else
		{
			ComponentIndex = INDEX_NONE;
		}
	}

	void Init(const FNiagaraDataSetCompiledData& DataSetCompiledData, const FName VariableName)
	{
		ComponentIndex = INDEX_NONE;

		const int32 VariableIndex = DataSetCompiledData.Variables.IndexOfByPredicate(FNiagaraVariableMatch(FNiagaraDataSetAccessorTypeInfo<TType>::GetIntType(), VariableName));
		if (VariableIndex != INDEX_NONE)
		{
			ComponentIndex = DataSetCompiledData.VariableLayouts[VariableIndex].Int32ComponentStart;
		}
	}

	FORCEINLINE bool IsValid() const { return ComponentIndex != INDEX_NONE; }

	FORCEINLINE static FNiagaraDataSetReaderInt32<TType> CreateReader(const FNiagaraDataSet& DataSet, const FName VariableName) { return FNiagaraDataSetAccessorInt32<TType>(DataSet, VariableName).GetReader(DataSet); }
	FORCEINLINE static FNiagaraDataSetWriterInt32<TType> CreateWriter(const FNiagaraDataSet& DataSet, const FName VariableName) { return FNiagaraDataSetAccessorInt32<TType>(DataSet, VariableName).GetWriter(DataSet); }

	FORCEINLINE FNiagaraDataSetReaderInt32<TType> GetReader(const FNiagaraDataSet& DataSet) const { return FNiagaraDataSetReaderInt32<TType>(DataSet.GetCurrentData(), ComponentIndex); }
	FORCEINLINE FNiagaraDataSetWriterInt32<TType> GetWriter(const FNiagaraDataSet& DataSet) const { return FNiagaraDataSetWriterInt32<TType>(DataSet.GetDestinationData(), ComponentIndex); }

private:
	int32 ComponentIndex = INDEX_NONE;
};

template<>
struct FNiagaraDataSetAccessorTypeInfo<int32>
{
	using TAccessorBaseClass = FNiagaraDataSetAccessorInt32<int32>;

	static constexpr int32 NumElements = 1;
	static const FNiagaraTypeDefinition& GetIntType() { return FNiagaraTypeDefinition::GetIntDef(); }
};

template<>
struct FNiagaraDataSetAccessorTypeInfo<FNiagaraBool>
{
	using TAccessorBaseClass = FNiagaraDataSetAccessorInt32<FNiagaraBool>;

	static constexpr int32 NumElements = 1;
	static const FNiagaraTypeDefinition& GetIntType() { return FNiagaraTypeDefinition::GetBoolDef(); }
};

template<>
struct FNiagaraDataSetAccessorTypeInfo<ENiagaraExecutionState>
{
	using TAccessorBaseClass = FNiagaraDataSetAccessorInt32<ENiagaraExecutionState>;

	static constexpr int32 NumElements = 1;
	static FNiagaraTypeDefinition GetIntType() { return FNiagaraTypeDefinition(FNiagaraTypeDefinition::GetExecutionStateEnum()); }
};

//////////////////////////////////////////////////////////////////////////
// Structure readers


#if defined(_MSC_VER) && USING_CODE_ANALYSIS
#pragma warning(disable:6294) // Ill-defined for-loop:  initial condition does not satisfy test.  Loop body not executed.
#endif

template<typename TType>
struct FNiagaraDataSetReaderStruct
{
	FORCEINLINE FNiagaraDataSetReaderStruct() {}

	explicit FNiagaraDataSetReaderStruct(FNiagaraDataBuffer* DataBuffer, int32 FloatComponentIndex, int32 Int32ComponentIndex)
	{
		bIsValid  = DataBuffer != nullptr;
		bIsValid &= (FNiagaraDataSetAccessorTypeInfo<TType>::NumFloatComponents == 0) || (FloatComponentIndex != INDEX_NONE);
		bIsValid &= (FNiagaraDataSetAccessorTypeInfo<TType>::NumInt32Components == 0) || (Int32ComponentIndex != INDEX_NONE);
		if (bIsValid)
		{
			int32 ComponentIndex = 0;
			for (int i = 0; i < FNiagaraDataSetAccessorTypeInfo<TType>::NumFloatComponents; ++i)
			{
				ComponentData[ComponentIndex++] = DataBuffer->GetComponentPtrFloat(FloatComponentIndex + i);
			}
			for (int i = 0; i < FNiagaraDataSetAccessorTypeInfo<TType>::NumInt32Components; ++i)
			{
				ComponentData[ComponentIndex++] = DataBuffer->GetComponentPtrInt32(Int32ComponentIndex + i);
			}
			NumInstances = DataBuffer->GetNumInstances();
		}
	}

	FORCEINLINE bool IsValid() const { return bIsValid; }

	FORCEINLINE TType operator[](int32 Index) const
	{
		return Get(Index);
	}

	FORCEINLINE TType Get(int Index) const
	{
		check(bIsValid && Index <= NumInstances);
		return FNiagaraDataSetAccessorTypeInfo<TType>::ReadStruct(
			Index,
			FNiagaraDataSetAccessorTypeInfo<TType>::NumFloatComponents > 0 ? reinterpret_cast<const float*const*>(ComponentData) : nullptr,
			FNiagaraDataSetAccessorTypeInfo<TType>::NumInt32Components > 0 ? reinterpret_cast<const int32*const*>(ComponentData + FNiagaraDataSetAccessorTypeInfo<TType>::NumFloatComponents) : nullptr
		);
	}

	FORCEINLINE TType GetSafe(int Index, const TType& Default) const
	{
		if (bIsValid && (Index < NumInstances))
		{
			return Get(Index);
		}
		return Default;
	}

private:
	bool bIsValid = false;
	int32 NumInstances = 0;
	const uint8* ComponentData[FNiagaraDataSetAccessorTypeInfo<TType>::NumFloatComponents + FNiagaraDataSetAccessorTypeInfo<TType>::NumInt32Components] = {};
};

#if defined(_MSC_VER) && USING_CODE_ANALYSIS
#pragma warning(default:6294)
#endif

template<typename TType>
struct FNiagaraDataSetAccessorStruct
{
	FORCEINLINE FNiagaraDataSetAccessorStruct() {}
	FORCEINLINE explicit FNiagaraDataSetAccessorStruct(const FNiagaraDataSet& DataSet, const FName VariableName) { Init(DataSet.GetCompiledData(), VariableName); }
	FORCEINLINE explicit FNiagaraDataSetAccessorStruct(const FNiagaraDataSetCompiledData& DataSetCompiledData, const FName VariableName) { Init(DataSetCompiledData, VariableName); }
	FORCEINLINE explicit FNiagaraDataSetAccessorStruct(const FNiagaraDataSetCompiledData* DataSetCompiledData, const FName VariableName) { Init(DataSetCompiledData, VariableName); }
	FORCEINLINE void Init(const FNiagaraDataSet& DataSet, const FName VariableName) { Init(DataSet.GetCompiledData(), VariableName); }

	void Init(const FNiagaraDataSetCompiledData* DataSetCompiledData, const FName VariableName)
	{
		if (DataSetCompiledData != nullptr)
		{
			Init(*DataSetCompiledData, VariableName);
		}
		else
		{
			bIsValid = false;
			FloatComponentIndex = INDEX_NONE;
			Int32ComponentIndex = INDEX_NONE;
		}
	}

	void Init(const FNiagaraDataSetCompiledData& DataSetCompiledData, const FName VariableName)
	{
		bIsValid = false;

		const int32 VariableIndex = DataSetCompiledData.Variables.IndexOfByPredicate(FNiagaraVariableMatch(FNiagaraDataSetAccessorTypeInfo<TType>::GetStructType(), VariableName));
		if (VariableIndex != INDEX_NONE)
		{
			bIsValid = true;
			FloatComponentIndex = DataSetCompiledData.VariableLayouts[VariableIndex].FloatComponentStart;
			Int32ComponentIndex = DataSetCompiledData.VariableLayouts[VariableIndex].Int32ComponentStart;
		}
	}

	FORCEINLINE bool IsValid() const { return bIsValid; }

	FORCEINLINE static FNiagaraDataSetReaderStruct<TType> CreateReader(const FNiagaraDataSet& DataSet, const FName VariableName) { return FNiagaraDataSetAccessorStruct<TType>(DataSet, VariableName).GetReader(DataSet); }
	//FORCEINLINE static FNiagaraDataSetWriterStruct<TType> CreateWriter(const FNiagaraDataSet& DataSet, const FName VariableName) { return FNiagaraDataSetAccessorStruct<TType>(DataSet, VariableName).GetWriter(DataSet); }

	FORCEINLINE FNiagaraDataSetReaderStruct<TType> GetReader(const FNiagaraDataSet& DataSet) const { return FNiagaraDataSetReaderStruct<TType>(DataSet.GetCurrentData(), FloatComponentIndex, Int32ComponentIndex); }
	//FNiagaraDataSetReaderStruct<TType> GetWriter(const FNiagaraDataSet& DataSet) { return FNiagaraDataSetReaderStruct<TType>(DataSet.GetDestinationData(), FloatComponentIndex, Int32ComponentIndex); }

private:
	bool bIsValid = false;
	int32 FloatComponentIndex = INDEX_NONE;
	int32 Int32ComponentIndex = INDEX_NONE;
};

template<>
struct FNiagaraDataSetAccessorTypeInfo<FNiagaraSpawnInfo>
{
	using TAccessorBaseClass = FNiagaraDataSetAccessorStruct<FNiagaraSpawnInfo>;

	static constexpr int32 NumFloatComponents = 2;
	static constexpr int32 NumInt32Components = 2;
	static FNiagaraTypeDefinition GetStructType() { return FNiagaraTypeDefinition(FNiagaraSpawnInfo::StaticStruct()); }

	static FNiagaraSpawnInfo ReadStruct(int32 InstanceIndex, const float*const* FloatComponents, const int32*const* Int32Components)
	{
		FNiagaraSpawnInfo SpawnInfo;
		SpawnInfo.Count			= Int32Components[0][InstanceIndex];
		SpawnInfo.InterpStartDt	= FloatComponents[0][InstanceIndex];
		SpawnInfo.IntervalDt	= FloatComponents[1][InstanceIndex];
		SpawnInfo.SpawnGroup	= Int32Components[1][InstanceIndex];
		return SpawnInfo;
	}
};

template<>
struct FNiagaraDataSetAccessorTypeInfo<FNiagaraID>
{
	using TAccessorBaseClass = FNiagaraDataSetAccessorStruct<FNiagaraID>;

	static constexpr int32 NumFloatComponents = 0;
	static constexpr int32 NumInt32Components = 2;
	static FNiagaraTypeDefinition GetStructType() { return FNiagaraTypeDefinition(FNiagaraID::StaticStruct()); }

	static FNiagaraID ReadStruct(int32 InstanceIndex, const float*const* FloatComponents, const int32*const* Int32Components)
	{
		FNiagaraID NiagaraID;
		NiagaraID.Index			= Int32Components[0][InstanceIndex];
		NiagaraID.AcquireTag	= Int32Components[1][InstanceIndex];
		return NiagaraID;
	}
};
