// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaGeometryBaseModifier.h"
#include "AvaBendModifier.generated.h"

UCLASS(MinimalAPI, BlueprintType)
class UAvaBendModifier : public UAvaGeometryBaseModifier
{
	GENERATED_BODY()

public:
	AVALANCHEMODIFIERS_API void SetAngle(float InAngle);
	float GetAngle() const
	{
		return Angle;
	}

	AVALANCHEMODIFIERS_API void SetExtent(float InExtent);
	float GetExtent() const
	{
		return Extent;
	}

	AVALANCHEMODIFIERS_API void SetBendPosition(const FVector& InBendPosition);
	const FVector& GetBendPosition() const
	{
		return BendPosition;
	}

	AVALANCHEMODIFIERS_API void SetBendRotation(const FRotator& InBendRotation);
	const FRotator& GetBendRotation() const
	{
		return BendRotation;
	}

	AVALANCHEMODIFIERS_API void SetSymmetricExtents(bool bInSymmetricExtents);
	bool GetSymmetricExtents() const
	{
		return bSymmetricExtents;
	}

	AVALANCHEMODIFIERS_API void SetBidirectional(bool bInBidirectional);
	bool GetBidirectional() const
	{
		return bBidirectional;
	}

protected:
	//~ Begin UObject
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject

	//~ Begin UActorModifierCoreBase
	virtual void OnModifierCDOSetup(FActorModifierCoreMetadata& InMetadata) override;
	virtual void Apply() override;
	//~ End UActorModifierCoreBase

	void OnBendTransformChanged();
	void OnBendOptionChanged();

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetBendPosition", Getter="GetBendPosition", Category="Bend", meta=(AllowPrivateAccess="true"))
	FVector BendPosition = FVector::ZeroVector;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetBendRotation", Getter="GetBendRotation", Category="Bend", meta=(AllowPrivateAccess="true"))
	FRotator BendRotation = FRotator::ZeroRotator;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetAngle", Getter="GetAngle", Category="Bend", meta=(ClampMin="-360.0", ClampMax="360.0", AllowPrivateAccess="true"))
	float Angle = 25;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetExtent", Getter="GetExtent", Category="Bend", meta=(ClampMin="0", ClampMax="1.0", AllowPrivateAccess="true"))
	float Extent = 1.0;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetSymmetricExtents", Getter="GetSymmetricExtents", Category="Bend", meta=(AllowPrivateAccess="true"))
	bool bSymmetricExtents = true;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetBidirectional", Getter="GetBidirectional", Category="Bend", meta=(AllowPrivateAccess="true"))
	bool bBidirectional = false;

	UPROPERTY()
	float ModifiedMeshMaxExtent = 50;
};
