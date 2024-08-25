// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaGeometryBaseModifier.h"
#include "AvaBevelModifier.generated.h"

UCLASS(MinimalAPI, BlueprintType)
class UAvaBevelModifier : public UAvaGeometryBaseModifier
{
	GENERATED_BODY()

public:
	static inline const FName BevelPolygroupLayerName = TEXT("Bevel");
	static constexpr float MinInset = 0;
	static constexpr int32 MinIterations = 1;
	static constexpr int32 MaxIterations = 3;

	AVALANCHEMODIFIERS_API void SetInset(float InBevel);
	float GetInset() const
	{
		return Inset;
	}

	AVALANCHEMODIFIERS_API void SetIterations(int32 InIterations);
	int32 GetIterations() const
	{
		return Iterations;
	}

protected:
	//~ Begin UObject
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject

	//~ Begin UActorModifierCoreBase
	virtual void OnModifierCDOSetup(FActorModifierCoreMetadata& InMetadata) override;
	virtual void Apply() override;
	//~ End UActorModifierCoreBase

	void OnInsetChanged();
	void OnIterationsChanged();

	float GetMaxBevel() const;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetInset", Getter="GetInset", Category="Bevel", meta=(ClampMin="0.0", AllowPrivateAccess="true"))
	float Inset = 1.0f;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetIterations", Getter="GetIterations", Category="Bevel", meta=(ClampMin="1", ClampMax="3", AllowPrivateAccess="true"))
	int32 Iterations = 1;
};
