// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"
#include "Containers/Array.h"
#include "GameplayTagContainer.h"
#include "InternalMinimalReplicationTagCountMapNetSerializer.generated.h"

struct FMinimalReplicationTagCountMap;

USTRUCT()
struct FMinimalReplicationTagCountMapForNetSerializer
{
	GENERATED_BODY()

public:
	void CopyReplicatedFieldsFrom(const FMinimalReplicationTagCountMap& TagCountMap);	
	void AssignReplicatedFieldsTo(FMinimalReplicationTagCountMap& TagCountMap) const;

	SIZE_T GetTagCount() const;
	void ClampTagCount(SIZE_T MaxTagCount);

private:
	UPROPERTY()
	TArray<FGameplayTag> Tags;
};

inline SIZE_T FMinimalReplicationTagCountMapForNetSerializer::GetTagCount() const
{
	return static_cast<uint32>(Tags.Num());
}