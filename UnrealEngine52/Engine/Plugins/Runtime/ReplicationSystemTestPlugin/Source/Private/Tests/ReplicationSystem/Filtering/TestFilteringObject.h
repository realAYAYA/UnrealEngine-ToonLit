// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tests/ReplicationSystem/ReplicatedTestObject.h"
#include "TestFilteringObject.generated.h"

UCLASS()
class UTestFilteringObject : public UReplicatedTestObject
{
	GENERATED_BODY()

public:
	void SetFilterOut(bool bFilterOut);

protected:
	virtual void RegisterReplicationFragments(UE::Net::FFragmentRegistrationContext& Fragments, UE::Net::EFragmentRegistrationFlags RegistrationFlags) override;

private:
	// The property name below violates the UE coding standard due to hardcoded prefix in the descriptor builder for testing purposes.
	UPROPERTY(Replicated)
	bool NetTest_FilterOut = false;
};
