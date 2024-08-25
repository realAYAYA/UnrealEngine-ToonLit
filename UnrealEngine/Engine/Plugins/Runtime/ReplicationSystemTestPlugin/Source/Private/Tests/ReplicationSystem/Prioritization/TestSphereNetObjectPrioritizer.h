// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationSystem/Prioritization/SphereNetObjectPrioritizer.h"
#include "TestSphereNetObjectPrioritizer.generated.h"

UCLASS()
class USphereNetObjectPrioritizerForTest : public USphereNetObjectPrioritizer
{
	GENERATED_BODY()

public:
	USphereNetObjectPrioritizerForTest();

	struct FPrioritizeParams
	{
		UE::Net::FReplicationView View;
		TArrayView<const FVector> InPositions;
		TArrayView<float> OutPriorities;
	};

	// Method for testing purposes.
	void PrioritizeWithParams(FPrioritizeParams& Params);

	const USphereNetObjectPrioritizerConfig* GetConfig() const
	{
		return Config.Get();
	}
};
