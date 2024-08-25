// Copyright Epic Games, Inc. All Rights Reserved.

#include "Stateless/NiagaraStatelessEmitterData.h"
#include "Stateless/NiagaraStatelessEmitterTemplate.h"
#include "Stateless/NiagaraStatelessSimulationShader.h"

void FNiagaraStatelessEmitterData::FDeleter::operator()(FNiagaraStatelessEmitterData* EmitterData) const
{
	if (IsInRenderingThread())
	{
		delete EmitterData;
	}
	else
	{
		ENQUEUE_RENDER_COMMAND(DeleteNiagaraStatelessEmitterData)
		(
			[EmitterData](FRHICommandListImmediate&)
			{
				delete EmitterData;
			}
		);
	}
}

FNiagaraStatelessEmitterData::~FNiagaraStatelessEmitterData()
{
	check(IsInRenderingThread());
	StaticFloatBuffer.Release();
}

void FNiagaraStatelessEmitterData::InitRenderResources()
{
	ENQUEUE_RENDER_COMMAND(InitNiagaraStatelessEmitterData)
	(
		[this](FRHICommandListImmediate& RHICmdList)
		{
			StaticFloatBuffer.Initialize(RHICmdList, TEXT("NiagaraStatelessEmitterData_StaticFloatBuffer"), sizeof(float), StaticFloatData.Num(), PF_R32_FLOAT, EBufferUsageFlags::Static);
			void* BufferData = RHICmdList.LockBuffer(StaticFloatBuffer.Buffer, 0, StaticFloatBuffer.NumBytes, RLM_WriteOnly);
			FMemory::Memcpy(BufferData, StaticFloatData.GetData(), StaticFloatBuffer.NumBytes);
			RHICmdList.UnlockBuffer(StaticFloatBuffer.Buffer);
		}
	);
}

TShaderRef<NiagaraStateless::FSimulationShader>	FNiagaraStatelessEmitterData::GetShader() const
{
	if (FApp::CanEverRender())
	{
		check(EmitterTemplate);
		return EmitterTemplate->GetSimulationShader();
	}
	return TShaderRef<NiagaraStateless::FSimulationShader>();
}

const FShaderParametersMetadata* FNiagaraStatelessEmitterData::GetShaderParametersMetadata() const
{
	check(EmitterTemplate);
	return EmitterTemplate->GetShaderParametersMetadata();
}

//float FNiagaraStatelessEmitterData::CalculateCompletionAge(int32 InRandomSeed, TConstArrayView<FNiagaraStatelessRuntimeSpawnInfo> RuntimeSpawnInfos) const
//{
//	float CompletionAge = -1.0f;
//	//for (const FNiagaraStatelessSpawnInfo& SpawnInfo : SpawnInfos)
//	//{
//	//	CompletionAge = FMath::Max(CompletionAge, SpawnInfo.AgeEnd);
//	//}
//
//	return CompletionAge >= 0.0f ? CompletionAge + LifetimeRange.Y : 0.0f;
//}

uint32 FNiagaraStatelessEmitterData::CalculateActiveParticles(int32 InRandomSeed, TConstArrayView<FNiagaraStatelessRuntimeSpawnInfo> RuntimeSpawnInfos, TOptional<float> Age, NiagaraStateless::FSpawnInfoShaderParameters* SpawnParameters) const
{
	int32	GpuSpawnIndex = 0;
	uint32	TotalActiveParticles = 0;

	for (const FNiagaraStatelessRuntimeSpawnInfo& SpawnInfo : RuntimeSpawnInfos)
	{
		uint32 SpawnInfoTotalParticles = 0;

		const bool bIsValidForAge = !Age.IsSet() || (Age.GetValue() >= SpawnInfo.SpawnTimeStart && Age.GetValue() < SpawnInfo.SpawnTimeEnd + LifetimeRange.Max);
		if (bIsValidForAge && GpuSpawnIndex < NiagaraStateless::MaxGpuSpawnInfos)
		{
			uint32	NumActive = 0;
			uint32	ParticleOffset = 0;
			float	SpawnRate = 0.0f;
			float	SpawnTimeStart = SpawnInfo.SpawnTimeStart;
			switch (SpawnInfo.Type)
			{
				case ENiagaraStatelessSpawnInfoType::Burst:
					NumActive				= SpawnInfo.Amount;
					break;

				case ENiagaraStatelessSpawnInfoType::Rate:
					if (!Age.IsSet())
					{
						NumActive = FMath::FloorToInt((SpawnInfo.SpawnTimeEnd - SpawnInfo.SpawnTimeStart) * SpawnInfo.Rate);
					}
					else
					{
						const uint32 MaxActive = FMath::FloorToInt((SpawnInfo.SpawnTimeEnd - SpawnInfo.SpawnTimeStart) * SpawnInfo.Rate);
						ParticleOffset = FMath::FloorToInt(FMath::Max(Age.GetValue() - SpawnInfo.SpawnTimeStart - LifetimeRange.Max, 0.0f) * SpawnInfo.Rate);
						ParticleOffset = FMath::Min(ParticleOffset, MaxActive);
						NumActive = FMath::FloorToInt(FMath::Max(Age.GetValue() - SpawnInfo.SpawnTimeStart, 0.0f) * SpawnInfo.Rate);
						NumActive = FMath::Min(NumActive, MaxActive);
						NumActive -= ParticleOffset;
					}
					SpawnRate = 1.0f / SpawnInfo.Rate;
					SpawnTimeStart += SpawnRate;
					break;

				default:
					checkNoEntry();
					break;
			}
			if (NumActive > 0)
			{
				if (SpawnParameters)
				{
					GET_SCALAR_ARRAY_ELEMENT(SpawnParameters->SpawnInfo_NumActive, GpuSpawnIndex)		= NumActive;
					GET_SCALAR_ARRAY_ELEMENT(SpawnParameters->SpawnInfo_ParticleOffset, GpuSpawnIndex)	= ParticleOffset;
					GET_SCALAR_ARRAY_ELEMENT(SpawnParameters->SpawnInfo_UniqueOffset, GpuSpawnIndex)	= SpawnInfo.UniqueOffset;
					GET_SCALAR_ARRAY_ELEMENT(SpawnParameters->SpawnInfo_Time, GpuSpawnIndex)			= SpawnTimeStart;
					GET_SCALAR_ARRAY_ELEMENT(SpawnParameters->SpawnInfo_Rate, GpuSpawnIndex)			= SpawnRate;
					++GpuSpawnIndex;
				}
				TotalActiveParticles += NumActive;
			}
		}
	}

	if (SpawnParameters)
	{
		while (GpuSpawnIndex < NiagaraStateless::MaxGpuSpawnInfos)
		{
			GET_SCALAR_ARRAY_ELEMENT(SpawnParameters->SpawnInfo_NumActive, GpuSpawnIndex)		= 0;
			GET_SCALAR_ARRAY_ELEMENT(SpawnParameters->SpawnInfo_ParticleOffset, GpuSpawnIndex)	= 0;
			GET_SCALAR_ARRAY_ELEMENT(SpawnParameters->SpawnInfo_UniqueOffset, GpuSpawnIndex)	= 0;
			GET_SCALAR_ARRAY_ELEMENT(SpawnParameters->SpawnInfo_Time, GpuSpawnIndex)			= 0.0f;
			GET_SCALAR_ARRAY_ELEMENT(SpawnParameters->SpawnInfo_Rate, GpuSpawnIndex)			= 0.0f;
			++GpuSpawnIndex;
		}
	}
	return TotalActiveParticles;
}

