// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "MultiUserReplicationStream.h"
#include "Replication/Data/ReplicationStream.h"
#include "MultiUserReplicationClientPreset.generated.h"

/**
 * 
 */
UCLASS()
class MULTIUSERREPLICATIONEDITOR_API UMultiUserReplicationClientPreset : public UObject
{
	GENERATED_BODY()
public:

	/** The stream ID that is used by all Multi-User streams. */
	static constexpr FGuid MultiUserStreamID { 0xAAAAAAAA, 0xBBBBBBBB, 0xCCCCCCCC, 0xDDDDDDDD };

	/** The stream this client is managing */
	UPROPERTY(Instanced)
	TObjectPtr<UMultiUserReplicationStream> Stream;

	UMultiUserReplicationClientPreset();

	/** Resets the contents everything to defaults. Empties the stream */
	void ClearClient();

	/** Generates a description that can be sent to the MU server. */
	FConcertReplicationStream GenerateDescription() const;
};
