// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Iris/DataStream/DataStream.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "DataStreamDefinitions.generated.h"

USTRUCT()
struct FDataStreamDefinition
{
	GENERATED_BODY()

	// Data stream identifier
	UPROPERTY()
	FName DataStreamName;

	// UClass name used to create the UDataStream
	UPROPERTY()
	FName ClassName;		

	// UClass used to create the UDataStream
	UPROPERTY()
	TObjectPtr<UClass> Class = nullptr;

	// Default send status when created.
	UPROPERTY()
	EDataStreamSendStatus DefaultSendStatus = EDataStreamSendStatus::Send;

	// Whether the DataStream should be auto created for each connection. If not then CreateStream need be called manually.
	UPROPERTY()
	bool bAutoCreate = false;
};

UCLASS(transient, config=Engine)
class UDataStreamDefinitions : public UObject
{
	GENERATED_BODY()

protected:
	UDataStreamDefinitions();

protected:
	friend class UDataStreamManager;

	void FixupDefinitions();
	const FDataStreamDefinition* FindDefinition(const FName Name) const;
	void GetAutoCreateStreamNames(TArray<FName>& OutStreamNames) const;

private:
	UPROPERTY(Config)
	TArray<FDataStreamDefinition> DataStreamDefinitions;

	bool bFixupComplete;

// For testing purposes only
#if WITH_AUTOMATION_WORKER
public:
	inline TArray<FDataStreamDefinition>& ReadWriteDataStreamDefinitions() { return DataStreamDefinitions; }
	inline bool& ReadWriteFixupComplete() { return bFixupComplete; }
#endif
};
