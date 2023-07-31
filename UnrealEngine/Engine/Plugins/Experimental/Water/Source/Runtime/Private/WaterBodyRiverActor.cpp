// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaterBodyRiverActor.h"
#include "WaterBodyRiverComponent.h"
#include "Components/SplineMeshComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WaterBodyRiverActor)

#if WITH_EDITOR
#include "WaterIconHelper.h"
#endif

// ----------------------------------------------------------------------------------

AWaterBodyRiver::AWaterBodyRiver(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	WaterBodyType = EWaterBodyType::River;
}

void AWaterBodyRiver::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITORONLY_DATA
	if (GetLinkerCustomVersion(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::WaterBodyComponentRefactor)
	{
		UWaterBodyRiverComponent* RiverComponent = CastChecked<UWaterBodyRiverComponent>(WaterBodyComponent);
		RiverComponent->SetLakeTransitionMaterial(LakeTransitionMaterial_DEPRECATED);
		RiverComponent->SetOceanTransitionMaterial(OceanTransitionMaterial_DEPRECATED);
		if (RiverGenerator_DEPRECATED)
		{
			RiverComponent->SplineMeshComponents = MoveTemp(RiverGenerator_DEPRECATED->SplineMeshComponents);
			for (USplineMeshComponent* SplineMeshComponent : RiverComponent->SplineMeshComponents)
			{
				if (SplineMeshComponent)
				{
					SplineMeshComponent->SetupAttachment(RiverComponent);
				}
			}
		}
	}
#endif // WITH_EDITORONLY_DATA
}

// ----------------------------------------------------------------------------------

UDEPRECATED_RiverGenerator::UDEPRECATED_RiverGenerator(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}
