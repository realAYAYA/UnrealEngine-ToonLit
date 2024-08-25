// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "PhysicsInterfaceTypesCore.h"

class FChaosUserDefinedEntity
{
public:
	FChaosUserDefinedEntity() = delete;
	FChaosUserDefinedEntity(FName InEntityTypeName) : EntityTypeName(InEntityTypeName) {};

	FName GetEntityTypeName() {
		return EntityTypeName;
	}
private:	
	FName EntityTypeName;
};

// This is used to add your own Unreal Engine agnostic user entities to Chaos Physics Results
struct FChaosUserEntityAppend : public FChaosUserData
{
	FChaosUserEntityAppend() { Type = EChaosUserDataType::ChaosUserEntity; Payload = this; };
	FChaosUserData* ChaosUserData; //  Chaos data to access the physics body properties
	FChaosUserDefinedEntity* UserDefinedEntity;
};
