// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ExternalDataLayerUID.generated.h"

struct FAssetData;

USTRUCT()
struct ENGINE_API FExternalDataLayerUID
{
	GENERATED_BODY()

public:
	bool IsValid() const;
	operator uint32() const { return Value; }

	FString ToString() const;
#if WITH_EDITOR
	static bool Parse(const FString& InUIDString, FExternalDataLayerUID& OutUID);
#endif

private:
#if WITH_EDITOR
	static FExternalDataLayerUID NewUID();
#endif

	UPROPERTY(VisibleAnywhere, Category = "External Data Layer", AdvancedDisplay)
	uint32 Value = 0;

	friend uint32 GetTypeHash(const FExternalDataLayerUID& InUID) { return InUID.Value; }
	friend class UExternalDataLayerAsset;
};