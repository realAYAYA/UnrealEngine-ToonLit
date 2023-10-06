// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/LightWeightInstanceStaticMeshManager.h"
#include "MassEntityConfigAsset.h"
#include "MassEntityTemplate.h"
#include "MassLWIStaticMeshManager.generated.h"


struct MASSLWI_API FMassLWIManagerRegistrationHandle
{
	explicit FMassLWIManagerRegistrationHandle(const int32 InRegisteredIndex = INDEX_NONE)
		: RegisteredIndex(InRegisteredIndex)
	{}

	operator int32() const { return RegisteredIndex; }
	bool IsValid() const { return RegisteredIndex != INDEX_NONE; }

private:
	const int32 RegisteredIndex = INDEX_NONE;
};

UCLASS()
class MASSLWI_API AMassLWIStaticMeshManager : public ALightWeightInstanceStaticMeshManager
{
	GENERATED_BODY()

public:
	bool IsRegisteredWithMass() const { return MassRegistrationHandle.IsValid(); }

	virtual void TransferDataToMass(FMassEntityManager& EntityManager);
	virtual void StoreMassDataInActor(FMassEntityManager& EntityManager);
	FMassLWIManagerRegistrationHandle GetMassRegistrationHandle() const { return MassRegistrationHandle; }
	void MarkRegisteredWithMass(const FMassLWIManagerRegistrationHandle RegistrationIndex);
	void MarkUnregisteredWithMass();

	// Returns the index of the light weight instance associated with InEntity if one exists; otherwise we return INDEX_NONE
	int32 FindIndexForEntity(const FMassEntityHandle Entity) const;

	static AMassLWIStaticMeshManager* GetMassLWIManagerForEntity(const FMassEntityManager& EntityManager, const FMassEntityHandle Entity);

protected:
	// AActor API
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	// End AActor API

	virtual void CreateMassTemplate(FMassEntityManager& EntityManager);


	FMassLWIManagerRegistrationHandle MassRegistrationHandle;
	FMassEntityTemplateID MassTemplateID;
	TSharedPtr<FMassEntityTemplate> FinalizedTemplate;
	FMassArchetypeHandle TargetArchetype;
	TArray<FMassEntityHandle> Entities;
};
