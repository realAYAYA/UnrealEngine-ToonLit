// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IAssetIndexer.h"

class UBlueprint;
struct FMemberReference;
class FSearchSerializer;
class UBlueprint;

class FBlueprintIndexer : public IAssetIndexer
{
public:
	virtual FString GetName() const override { return TEXT("Blueprint"); }
	virtual int32 GetVersion() const override;
	virtual void IndexAsset(const UObject* InAssetObject, FSearchSerializer& Serializer) const override;

private:
	void IndexClassDefaultObject(const UBlueprint* InBlueprint, FSearchSerializer& Serializer) const;
	void IndexComponents(const UBlueprint* InBlueprint, FSearchSerializer& Serializer) const;
	void IndexGraphs(const UBlueprint* InBlueprint, FSearchSerializer& Serializer) const;
	void IndexMemberReference(FSearchSerializer& Serializer, const FMemberReference& MemberReference, const FString& MemberType) const;
};