// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraStatelessCommon.h"
#include "NiagaraStatelessDistribution.h"

struct FNiagaraParameterBinding;
struct FNiagaraParameterBindingWithValue;
struct FNiagaraParameterStore;

class FNiagaraStatelessEmitterDataBuildContext
{
	static constexpr uint32 StatelessDistributionFlag_Random	= 0x00000001;
	static constexpr uint32 StatelessDistributionFlag_Uniform	= 0x00000002;
	static constexpr uint32 StatelessDistributionFlag_Binding	= 0x00000004;

public:
	UE_NONCOPYABLE(FNiagaraStatelessEmitterDataBuildContext);

	FNiagaraStatelessEmitterDataBuildContext(FNiagaraParameterStore& InRendererBindings, TArray<uint8>& InBuiltData, TArray<float>& InStaticFloatData)
		: RendererBindings(InRendererBindings)
		, BuiltData(InBuiltData)
		, StaticFloatData(InStaticFloatData)
	{
	}

	uint32 AddStaticData(TConstArrayView<float> FloatData);
	uint32 AddStaticData(TConstArrayView<FVector2f> FloatData);
	uint32 AddStaticData(TConstArrayView<FVector3f> FloatData);
	uint32 AddStaticData(TConstArrayView<FVector4f> FloatData);
	uint32 AddStaticData(TConstArrayView<FLinearColor> FloatData);

	template<typename T>
	T* AllocateBuiltData()
	{
		static_assert(TIsTrivial<T>::Value, "Only trivial types can be used for built data");

		const int32 Align = BuiltData.Num() % alignof(T);
		const int32 Offset = BuiltData.AddZeroed(sizeof(T) + Align);
		void* NewData = BuiltData.GetData() + Offset + Align;
		return new(NewData) T();
	}

	template<typename T>
	T& GetTransientBuildData()
	{
		TUniquePtr<FTransientObject>& TransientObj = TransientBuildData.FindOrAdd(T::GetName());
		if (TransientObj.IsValid() == false)
		{
			TransientObj.Reset(new TTransientObject<T>);
		}
		return *reinterpret_cast<T*>(TransientObj->GetObject());
	}

	// Adds a binding to the renderer parameter store
	// This allows you to read the parameter data inside the simulation process
	// The returned value is INDEX_NONE is the variables is index otherwise the offset in DWORDs
	int32 AddRendererBinding(const FNiagaraVariableBase& Variable);
	int32 AddRendererBinding(const FNiagaraParameterBinding& Binding);
	int32 AddRendererBinding(const FNiagaraParameterBindingWithValue& Binding);	

	// Adds an distribution into the LUT if enabled
	template<typename TType>
	FUintVector3 AddDistribution(ENiagaraDistributionMode Mode, TConstArrayView<TType> Values, bool bEnabled)
	{
		FUintVector3 Parameters = FUintVector3::ZeroValue;
		if (bEnabled && Values.Num() > 0)
		{
			switch (Mode)
			{
				case ENiagaraDistributionMode::Binding:				checkNoEntry(); break;
				case ENiagaraDistributionMode::UniformConstant:		Parameters.X = StatelessDistributionFlag_Random | StatelessDistributionFlag_Uniform; break;
				case ENiagaraDistributionMode::NonUniformConstant:	Parameters.X = StatelessDistributionFlag_Random; break;
				case ENiagaraDistributionMode::UniformRange:		Parameters.X = StatelessDistributionFlag_Random | StatelessDistributionFlag_Uniform; break;
				case ENiagaraDistributionMode::NonUniformRange:		Parameters.X = StatelessDistributionFlag_Random; break;
				case ENiagaraDistributionMode::UniformCurve:		Parameters.X = StatelessDistributionFlag_Uniform; break;
				case ENiagaraDistributionMode::NonUniformCurve:		Parameters.X = 0; break;
				default:											checkNoEntry(); break;
			}

			Parameters.Y = AddStaticData(Values);
			reinterpret_cast<float&>(Parameters.Z) = Values.Num() - 1;
		}
		return Parameters;
	}

	// Adds a distribution into the LUT if enabled and returns the packed information to send to the shader
	template<typename TDistribution>
	FUintVector3 AddDistribution(const TDistribution& Distribution, bool bEnabled)
	{
		FUintVector3 Parameters = FUintVector3::ZeroValue;
		if ( bEnabled )
		{
			if (Distribution.Mode == ENiagaraDistributionMode::Binding)
			{
				const int32 ParameterOffset = AddRendererBinding(Distribution.ParameterBinding);
				if (ParameterOffset >= 0)
				{
					Parameters.X = StatelessDistributionFlag_Binding;
					Parameters.Y = ParameterOffset;
					reinterpret_cast<float&>(Parameters.Z) = 1.0f;
				}
			}
			else
			{
				Parameters = AddDistribution(Distribution.Mode, MakeArrayView(Distribution.Values), bEnabled);
			}
		}
		return Parameters;
	}

	template<typename TRange, typename TDistribution, typename TDefaultValue>
	TRange ConvertDistributionToRangeHelper(const TDistribution& Distribution, const TDefaultValue& DefaultValue, bool bEnabled)
	{
		TRange Range(DefaultValue);
		if (bEnabled)
		{
			if (Distribution.Mode == ENiagaraDistributionMode::Binding)
			{
				Range.ParameterOffset = AddRendererBinding(Distribution.ParameterBinding);
			}
			else
			{
				Range = Distribution.CalculateRange(DefaultValue);
			}
		}
		return Range;
	}

	FNiagaraStatelessRangeFloat   ConvertDistributionToRange(const FNiagaraDistributionFloat& Distribution, float DefaultValue, bool bEnabled = true) { return ConvertDistributionToRangeHelper<FNiagaraStatelessRangeFloat>(Distribution, DefaultValue, bEnabled); }
	FNiagaraStatelessRangeVector2 ConvertDistributionToRange(const FNiagaraDistributionVector2& Distribution, const FVector2f& DefaultValue, bool bEnabled = true) { return ConvertDistributionToRangeHelper<FNiagaraStatelessRangeVector2>(Distribution, DefaultValue, bEnabled); }
	FNiagaraStatelessRangeVector3 ConvertDistributionToRange(const FNiagaraDistributionVector3& Distribution, const FVector3f& DefaultValue, bool bEnabled = true) { return ConvertDistributionToRangeHelper<FNiagaraStatelessRangeVector3>(Distribution, DefaultValue, bEnabled); }
	FNiagaraStatelessRangeColor   ConvertDistributionToRange(const FNiagaraDistributionColor& Distribution, const FLinearColor& DefaultValue, bool bEnabled = true) { return ConvertDistributionToRangeHelper<FNiagaraStatelessRangeColor>(Distribution, DefaultValue, bEnabled); }

	FNiagaraStatelessRangeFloat   ConvertDistributionToRange(const FNiagaraDistributionRangeFloat& Distribution, float DefaultValue, bool bEnabled = true) { return ConvertDistributionToRangeHelper<FNiagaraStatelessRangeFloat>(Distribution, DefaultValue, bEnabled); }
	FNiagaraStatelessRangeVector2 ConvertDistributionToRange(const FNiagaraDistributionRangeVector2& Distribution, const FVector2f& DefaultValue, bool bEnabled = true) { return ConvertDistributionToRangeHelper<FNiagaraStatelessRangeVector2>(Distribution, DefaultValue, bEnabled); }
	FNiagaraStatelessRangeVector3 ConvertDistributionToRange(const FNiagaraDistributionRangeVector3& Distribution, const FVector3f& DefaultValue, bool bEnabled = true) { return ConvertDistributionToRangeHelper<FNiagaraStatelessRangeVector3>(Distribution, DefaultValue, bEnabled); }
	FNiagaraStatelessRangeColor   ConvertDistributionToRange(const FNiagaraDistributionRangeColor& Distribution, const FLinearColor& DefaultValue, bool bEnabled = true) { return ConvertDistributionToRangeHelper<FNiagaraStatelessRangeColor>(Distribution, DefaultValue, bEnabled); }
	FNiagaraStatelessRangeInt     ConvertDistributionToRange(const FNiagaraDistributionRangeInt& Distribution, int32 DefaultValue, bool bEnabled = true) { return ConvertDistributionToRangeHelper<FNiagaraStatelessRangeInt>(Distribution, DefaultValue, bEnabled); }

private:
	FNiagaraParameterStore& RendererBindings;
	TArray<uint8>&			BuiltData;
	TArray<float>&			StaticFloatData;

	struct FTransientObject
	{
		virtual ~FTransientObject() = default;
		virtual void* GetObject() = 0;
	};

	template <typename T>
	struct TTransientObject final : FTransientObject
	{
		template <typename... TArgs>
		FORCEINLINE TTransientObject(TArgs&&... Args) : TheObject(Forward<TArgs&&>(Args)...) {}
		virtual void* GetObject() { return &TheObject; }

		T TheObject;
	};

	TMap<FName, TUniquePtr<FTransientObject>>	TransientBuildData;
};
