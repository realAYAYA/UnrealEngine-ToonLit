// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"

#include "WorldPartition/ContentBundle/ContentBundleContainer.h"

#include "ContentBundleWorldSubsystem.generated.h"

class FContentBundle;
class UContentBundleDescriptor;
class FContentBundleClient;
class FContentBundleEditor;
class UWorldPartition;

#if WITH_EDITOR
class URuntimeHashExternalStreamingObjectBase;
class UContentBundleDuplicateForPIEHelper;
#endif

UCLASS()
class ENGINE_API UContentBundleManager : public UObject
{
	GENERATED_BODY()

public:
	UContentBundleManager();

	void Initialize();
	void Deinitialize();

	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

#if WITH_EDITOR
	bool GetEditorContentBundle(TArray<TSharedPtr<FContentBundleEditor>>& OutContentBundles);
	TSharedPtr<FContentBundleEditor> GetEditorContentBundle(const UContentBundleDescriptor* Descriptor, const UWorld* ContentBundleWorld) const;

	UContentBundleDuplicateForPIEHelper* GetPIEDuplicateHelper() const { return PIEDuplicateHelper; }
#endif

private:
	uint32 GetContentBundleContainerIndex(const UWorld* InjectedWorld);
	TUniquePtr<FContentBundleContainer>* GetContentBundleContainer(const UWorld* InjectedWorld);

	void OnWorldPartitionInitialized(UWorldPartition* WorldPartition);
	void OnWorldPartitionUninitialized(UWorldPartition* WorldPartition);

	TArray<TUniquePtr<FContentBundleContainer>> ContentBundleContainers;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UContentBundleDuplicateForPIEHelper> PIEDuplicateHelper;
#endif
};



UCLASS()
class UContentBundleDuplicateForPIEHelper : public UObject
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	void Initialize();
	void Deinitialize();

	bool StoreContentBundleStreamingObect(const FContentBundleEditor& ContentBundleEditor, URuntimeHashExternalStreamingObjectBase* StreamingObject);
	URuntimeHashExternalStreamingObjectBase* RetrieveContentBundleStreamingObject(const FContentBundle& ContentBundle) const;

	uint32 GetStreamingObjectCount() { return StreamingObjects.Num(); }
	void Clear() { StreamingObjects.Empty(); }
#endif

private:
	void OnPIEEnded(const bool bIsSimulating);

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TMap<FGuid, TObjectPtr<URuntimeHashExternalStreamingObjectBase>> StreamingObjects;
#endif

};