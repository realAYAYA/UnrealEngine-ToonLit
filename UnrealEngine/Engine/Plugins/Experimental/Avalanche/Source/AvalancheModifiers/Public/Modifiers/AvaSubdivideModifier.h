// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaGeometryBaseModifier.h"
#include "AvaSubdivideModifier.generated.h"

UENUM()
enum class EAvaSubdivisionType : uint8
{
	Selective,
	Uniform,
	PN
};

UCLASS(MinimalAPI, BlueprintType)
class UAvaSubdivideModifier : public UAvaGeometryBaseModifier
{
	GENERATED_BODY()

public:
	static constexpr int MinSubdivideCuts = 1;
    static constexpr int MaxSubdivideCuts = 15;

	AVALANCHEMODIFIERS_API void SetCuts(int32 InCuts);
	int32 GetCuts() const
	{
		return Cuts;
	}

	AVALANCHEMODIFIERS_API void SetRecomputeNormals(bool bInRecomputeNormals);
	bool GetRecomputeNormals() const
	{
		return bRecomputeNormals;
	}

	AVALANCHEMODIFIERS_API void SetType(EAvaSubdivisionType InType);
	EAvaSubdivisionType GetType() const
	{
		return Type;
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

	void OnOptionsChanged();

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetCuts", Getter="GetCuts", Category="Subdivide", meta=(ClampMin="1", ClampMax="15", AllowPrivateAccess="true"))
	int32 Cuts = 2;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetRecomputeNormals", Getter="GetRecomputeNormals", Category="Subdivide", meta=(EditCondition="Type == EAvaSubdivisionType::PN", EditConditionHides, AllowPrivateAccess="true"))
	bool bRecomputeNormals = true;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetType", Getter="GetType", Category="Subdivide", meta=(AllowPrivateAccess="true"))
	EAvaSubdivisionType Type = EAvaSubdivisionType::Uniform;
};
