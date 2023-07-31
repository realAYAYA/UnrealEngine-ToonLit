// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMovieSceneObjectSpawner.h"
#include "ControlRigObjectSpawner.generated.h"

UCLASS()
class UControlRigObjectHolder : public UObject
{
	GENERATED_BODY()

public:

	UPROPERTY()
	TArray<TObjectPtr<UObject>> Objects;
};

class CONTROLRIG_API FControlRigObjectSpawner : public IMovieSceneObjectSpawner
{
public:

	static TSharedRef<IMovieSceneObjectSpawner> CreateObjectSpawner();

	FControlRigObjectSpawner();
	~FControlRigObjectSpawner();

	// IMovieSceneObjectSpawner interface
	virtual UClass* GetSupportedTemplateType() const override;
	virtual UObject* SpawnObject(FMovieSceneSpawnable& Spawnable, FMovieSceneSequenceIDRef TemplateID, IMovieScenePlayer& Player) override;
	virtual void DestroySpawnedObject(UObject& Object) override;

protected:
	TWeakObjectPtr<UControlRigObjectHolder> ObjectHolderPtr;
};