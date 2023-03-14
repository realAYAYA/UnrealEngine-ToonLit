// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassSettings.h"
#include "MassMovementTypes.h"
#include "MassMovementSettings.generated.h"

UCLASS(config = Mass, defaultconfig, meta = (DisplayName = "Mass Movement"))
class MASSMOVEMENT_API UMassMovementSettings : public UMassModuleSettings
{
	GENERATED_BODY()

public:
	UMassMovementSettings(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
	static const UMassMovementSettings* Get()
	{
		return GetDefault<UMassMovementSettings>();
	}

	TConstArrayView<FMassMovementStyle> GetMovementStyles() const { return MovementStyles; }
	const FMassMovementStyle* GetMovementStyleByID(const FGuid ID) const; 
	
private:

#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

	UPROPERTY(EditAnywhere, config, Category = Movement);
    TArray<FMassMovementStyle> MovementStyles;
};
