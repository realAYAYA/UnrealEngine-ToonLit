// Copyright Epic Games, Inc. All Rights Reserved.

#include "ISMPartition/ISMComponentBatcher.h"
#include "Components/StaticMeshComponent.h"
#include "Serialization/ArchiveCrc32.h"
#include "Misc/TransformUtilities.h"
#include "Templates/TypeHash.h"

void FISMComponentBatcher::Add(const UActorComponent* InComponent)
{
	AddInternal(InComponent, TOptional<TFunctionRef<FTransform(const FTransform&)>>());
}

void FISMComponentBatcher::Add(const UActorComponent* InComponent, TFunctionRef<FTransform(const FTransform&)> InTransformFunc)
{
	AddInternal(InComponent, InTransformFunc);
}

void FISMComponentBatcher::AddInternal(const UActorComponent* InComponent, TOptional<TFunctionRef<FTransform(const FTransform&)>> InTransformFunc)
{
	Hash = 0; // Invalidate
	int32 NewNumCustomDataFloats = 0;
	int32 NewNumInstances = 0;

	// Compute number of instances & custom data float to add
	if (const UInstancedStaticMeshComponent* ISMC = Cast<UInstancedStaticMeshComponent>(InComponent))
	{
		NewNumCustomDataFloats = ISMC->NumCustomDataFloats;
		NewNumInstances = ISMC->GetInstanceCount();
	}
	else if (const UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(InComponent))
	{
		NewNumCustomDataFloats = SMC->GetCustomPrimitiveData().Data.Num();
		NewNumInstances = 1;
	}

	if (NewNumCustomDataFloats > NumCustomDataFloats)
	{
		TArray<float> NewInstancesCustomData;
		NewInstancesCustomData.AddZeroed(NewNumCustomDataFloats * NumInstances);

		for (int32 InstanceIdx = 0; InstanceIdx < NumInstances; ++InstanceIdx)
		{
			int32 SrcCustomDataOffset = NumCustomDataFloats * InstanceIdx;
			int32 DestCustomDataOffset = NewNumCustomDataFloats * InstanceIdx;
			for (int32 CustomDataIdx = 0; CustomDataIdx < NumCustomDataFloats; ++CustomDataIdx)
			{
				NewInstancesCustomData[DestCustomDataOffset + CustomDataIdx] = InstancesCustomData[SrcCustomDataOffset + CustomDataIdx];
			}
		}

		InstancesCustomData = MoveTemp(NewInstancesCustomData);
		NumCustomDataFloats = NewNumCustomDataFloats;
	}

	NumInstances += NewNumInstances;
	InstancesTransformsWS.Reserve(NumInstances);
	InstancesCustomData.Reserve(NumInstances * NumCustomDataFloats);

	// Add instances
	if (const UInstancedStaticMeshComponent* ISMC = Cast<UInstancedStaticMeshComponent>(InComponent))
	{
		// Add random seeds
		RandomSeeds.Add({ NumInstances, ISMC->InstancingRandomSeed });
		for (const FInstancedStaticMeshRandomSeed& AdditionalRandomSeed : ISMC->AdditionalRandomSeeds)
		{
			RandomSeeds.Add({ NumInstances + AdditionalRandomSeed.StartInstanceIndex, AdditionalRandomSeed.RandomSeed });
		}

		// Add each instance
		for (int32 InstanceIdx = 0; InstanceIdx < ISMC->GetInstanceCount(); InstanceIdx++)
		{
			// Add instance transform
			FTransform InstanceTransformWS;
			ISMC->GetInstanceTransform(InstanceIdx, InstanceTransformWS, /*bWorldSpace*/ true);
			if (InTransformFunc.IsSet())
			{
				InstanceTransformWS = InTransformFunc->operator()(InstanceTransformWS);
			}
			InstancesTransformsWS.Add(InstanceTransformWS);

			// Add per instance custom data, if any
			if (NumCustomDataFloats > 0)
			{
				if (ISMC->NumCustomDataFloats > 0)
				{
					TConstArrayView<float> InstanceCustomData(&ISMC->PerInstanceSMCustomData[InstanceIdx * ISMC->NumCustomDataFloats], ISMC->NumCustomDataFloats);
					InstancesCustomData.Append(InstanceCustomData);
				}

				InstancesCustomData.AddDefaulted(NumCustomDataFloats - ISMC->NumCustomDataFloats);
			}
		}
	}
	else if (const UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(InComponent))
	{
		FTransform InstanceTransformWS = SMC->GetComponentTransform();
		if (InTransformFunc.IsSet())
		{
			InstanceTransformWS = InTransformFunc->operator()(InstanceTransformWS);
		}

		// Add transform
		InstancesTransformsWS.Add(InstanceTransformWS);

		// Add custom data
		InstancesCustomData.Append(SMC->GetCustomPrimitiveData().Data);
		InstancesCustomData.AddDefaulted(NumCustomDataFloats - SMC->GetCustomPrimitiveData().Data.Num());
	}
}

void FISMComponentBatcher::InitComponent(UInstancedStaticMeshComponent* ISMComponent) const
{
	ISMComponent->NumCustomDataFloats = NumCustomDataFloats;
	ISMComponent->AddInstances(InstancesTransformsWS, /*bShouldReturnIndices*/false, /*bWorldSpace*/true);
	ISMComponent->PerInstanceSMCustomData = InstancesCustomData;

	if (!RandomSeeds.IsEmpty())
	{
		ISMComponent->InstancingRandomSeed = RandomSeeds[0].RandomSeed;

		if (RandomSeeds.Num() > 1)
		{
			ISMComponent->AdditionalRandomSeeds = TArrayView<FInstancedStaticMeshRandomSeed>(&const_cast<FISMComponentBatcher*>(this)->RandomSeeds[1], RandomSeeds.Num() - 1);
		}
	}		
}

void FISMComponentBatcher::ComputeHash() const
{
	uint32 CRC = 0;
	for (const FTransform& InstanceTransform : InstancesTransformsWS)
	{
		CRC = HashCombine(TransformUtilities::GetRoundedTransformCRC32(InstanceTransform), CRC);
	}
	
	FArchiveCrc32 Ar(CRC);
	FISMComponentBatcher& This = *const_cast<FISMComponentBatcher*>(this);

	Ar << This.InstancesCustomData;
	Ar << This.RandomSeeds;

	Hash = Ar.GetCrc();
}
