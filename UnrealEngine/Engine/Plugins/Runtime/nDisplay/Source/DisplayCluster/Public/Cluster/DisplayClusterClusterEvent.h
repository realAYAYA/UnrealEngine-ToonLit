// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "DisplayClusterClusterEvent.generated.h"

constexpr const auto DisplayClusterResetSyncType = TEXT("nDCReset");


//////////////////////////////////////////////////////////////////////////////////////////////
// Common cluster event data
//////////////////////////////////////////////////////////////////////////////////////////////
USTRUCT(BlueprintType)
struct FDisplayClusterClusterEventBase
{
	GENERATED_BODY()

public:
	FDisplayClusterClusterEventBase()
		: bIsSystemEvent(false)
		, bShouldDiscardOnRepeat(true)
	{ }

public:
	// Is nDisplay internal event (should never be true for end users)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Is Sytem Event. 'True' is reserved for nDisplay internals."), Category = "NDisplay")
	bool bIsSystemEvent = false;

	// Should older events with the same Name/Type/Category (for JSON) or ID (for binary) be discarded if a new one received
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDisplay")
	bool bShouldDiscardOnRepeat = true;
};


//////////////////////////////////////////////////////////////////////////////////////////////
// Cluster event JSON
//////////////////////////////////////////////////////////////////////////////////////////////
USTRUCT(BlueprintType)
struct FDisplayClusterClusterEventJson
	: public FDisplayClusterClusterEventBase
{
	GENERATED_BODY()

public:
	// Event name (used for discarding outdated events)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDisplay")
	FString Name;

	// Event type (used for discarding outdated events)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDisplay")
	FString Type;

	// Event category (used for discarding outdated events)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDisplay")
	FString Category;

public:
	// Event parameters
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDisplay")
	TMap<FString, FString> Parameters;

public:
	FString SerializeToString() const;
	bool    DeserializeFromString(const FString& Arch);

private:
	FString SerializeParametersToString() const;
	bool    DeserializeParametersFromString(const FString& Arch);
};


//////////////////////////////////////////////////////////////////////////////////////////////
// Cluster event BINARY
//////////////////////////////////////////////////////////////////////////////////////////////
USTRUCT(BlueprintType)
struct FDisplayClusterClusterEventBinary
	: public FDisplayClusterClusterEventBase
{
	GENERATED_BODY()

public:
	// Event ID (used for discarding outdated events)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDisplay")
	int32 EventId = -1;

	// Binary event data
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDisplay")
	TArray<uint8> EventData;

public:
	void SerializeToByteArray(TArray<uint8>& Arch) const;
	bool DeserializeFromByteArray(const TArray<uint8>& Arch);
};
