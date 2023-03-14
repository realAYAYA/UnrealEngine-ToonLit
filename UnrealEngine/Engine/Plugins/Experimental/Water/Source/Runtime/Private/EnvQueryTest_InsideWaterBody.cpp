// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnvQueryTest_InsideWaterBody.h"
#include "UObject/UObjectHash.h"
#include "GameFramework/Volume.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"
#include "WaterBodyComponent.h"
#include "WaterBodyManager.h"
#include "WaterSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EnvQueryTest_InsideWaterBody)

UEnvQueryTest_InsideWaterBody::UEnvQueryTest_InsideWaterBody(const FObjectInitializer& ObjectInitializer) 
	: Super(ObjectInitializer)
{
	Cost = EEnvTestCost::High;
	ValidItemType = UEnvQueryItemType_VectorBase::StaticClass();
	SetWorkOnFloatValues(false);
}

void UEnvQueryTest_InsideWaterBody::RunTest(FEnvQueryInstance& QueryInstance) const
{	
	BoolValue.BindData(QueryInstance.Owner.Get(), QueryInstance.QueryID);
	const bool bWantsInside = BoolValue.GetValue();

	for (FEnvQueryInstance::ItemIterator It(this, QueryInstance); It; ++It)
	{
		const FVector ItemLocation = GetItemLocation(QueryInstance, It.GetIndex());

		bool bInside = false;
		FWaterBodyManager::ForEachWaterBodyComponent(GetWorld(), [this, ItemLocation, &bInside](UWaterBodyComponent* WaterBodyComponent)
		{
			EWaterBodyQueryFlags QueryFlags = EWaterBodyQueryFlags::ComputeImmersionDepth;
			if (bIncludeWaves)
			{
				QueryFlags |= EWaterBodyQueryFlags::IncludeWaves;

				if (bSimpleWaves)
				{
					QueryFlags |= EWaterBodyQueryFlags::SimpleWaves;
				}
			}
			
			if (bIgnoreExclusionVolumes)
			{
				QueryFlags |= EWaterBodyQueryFlags::IgnoreExclusionVolumes;
			}

			const FWaterBodyQueryResult QueryResult = WaterBodyComponent->QueryWaterInfoClosestToWorldLocation(ItemLocation, QueryFlags);
			if (QueryResult.IsInWater())
			{
				bInside = true;
				return false;
		}

			return true;
		});

		It.SetScore(TestPurpose, FilterType, bInside, bWantsInside);
	}
}

FText UEnvQueryTest_InsideWaterBody::GetDescriptionDetails() const
{
	return DescribeBoolTestParams("inside water body");
}

