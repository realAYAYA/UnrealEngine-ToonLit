// Copyright Epic Games, Inc. All Rights Reserved.

#include "Stateless/NiagaraStatelessEmitterDataBuildContext.h"

#include "NiagaraParameterBinding.h"
#include "NiagaraParameterStore.h"

uint32 FNiagaraStatelessEmitterDataBuildContext::AddStaticData(TConstArrayView<float> FloatData)
{
	for (int32 i=0; i <= StaticFloatData.Num() - FloatData.Num(); ++i)
	{
		if (FMemory::Memcmp(StaticFloatData.GetData() + i, FloatData.GetData(), FloatData.Num() * sizeof(float)) == 0)
		{
			return i;
		}
	}

	// Add new
	const uint32 OutIndex = StaticFloatData.AddUninitialized(FloatData.Num());
	FMemory::Memcpy(StaticFloatData.GetData() + OutIndex, FloatData.GetData(), FloatData.Num() * sizeof(float));
	return OutIndex;
}

uint32 FNiagaraStatelessEmitterDataBuildContext::AddStaticData(TConstArrayView<FVector2f> FloatData)
{
	return AddStaticData(MakeArrayView(reinterpret_cast<const float*>(FloatData.GetData()), FloatData.Num() * 2));
}

uint32 FNiagaraStatelessEmitterDataBuildContext::AddStaticData(TConstArrayView<FVector3f> FloatData)
{
	return AddStaticData(MakeArrayView(reinterpret_cast<const float*>(FloatData.GetData()), FloatData.Num() * 3));
}

uint32 FNiagaraStatelessEmitterDataBuildContext::AddStaticData(TConstArrayView<FVector4f> FloatData)
{
	return AddStaticData(MakeArrayView(reinterpret_cast<const float*>(FloatData.GetData()), FloatData.Num() * 4));
}

uint32 FNiagaraStatelessEmitterDataBuildContext::AddStaticData(TConstArrayView<FLinearColor> FloatData)
{
	return AddStaticData(MakeArrayView(reinterpret_cast<const float*>(FloatData.GetData()), FloatData.Num() * 4));
}

int32 FNiagaraStatelessEmitterDataBuildContext::AddRendererBinding(const FNiagaraVariableBase& Variable)
{
	int32 DataOffset = INDEX_NONE;
	if (Variable.IsValid())
	{
		FNiagaraVariable Var(Variable);
		RendererBindings.AddParameter(Var, false, false, &DataOffset);
		DataOffset /= sizeof(uint32);
	}

	return DataOffset;
}

int32 FNiagaraStatelessEmitterDataBuildContext::AddRendererBinding(const FNiagaraParameterBinding& Binding)
{
	int32 DataOffset = INDEX_NONE;
	if (Binding.ResolvedParameter.IsValid())
	{
		FNiagaraVariable Var(Binding.ResolvedParameter);
		RendererBindings.AddParameter(Var, false, false, &DataOffset);
		DataOffset /= sizeof(uint32);
	}

	return DataOffset;
}

int32 FNiagaraStatelessEmitterDataBuildContext::AddRendererBinding(const FNiagaraParameterBindingWithValue& Binding)
{
	int32 DataOffset = INDEX_NONE;
	if (Binding.ResolvedParameter.IsValid())
	{
		FNiagaraVariable Var(Binding.ResolvedParameter);
		RendererBindings.AddParameter(Var, false, false, &DataOffset);

		TConstArrayView<uint8> DefaultValue = Binding.GetDefaultValueArray();
		if (DefaultValue.Num() > 0)
		{
			check(DataOffset != INDEX_NONE);
			check(DefaultValue.Num() == Var.GetSizeInBytes());
			RendererBindings.SetParameterData(DefaultValue.GetData(), DataOffset, DefaultValue.Num());
		}

		DataOffset /= sizeof(uint32);
	}

	return DataOffset;
}
