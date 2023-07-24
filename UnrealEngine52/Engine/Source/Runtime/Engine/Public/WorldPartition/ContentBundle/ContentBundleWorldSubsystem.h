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
class UCanvas;

#if WITH_EDITOR
class URuntimeHashExternalStreamingObjectBase;
class UContentBundleDuplicateForPIEHelper;
#endif

UCLASS()
class ENGINE_API UContentBundleManager : public UObject
{
	GENERATED_BODY()

	friend class FContentBundleClient;

public:
	UContentBundleManager();

	void Initialize();
	void Deinitialize();

	bool CanInject() const;

	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

#if WITH_EDITOR
	bool GetEditorContentBundle(TArray<TSharedPtr<FContentBundleEditor>>& OutContentBundles);
	TSharedPtr<FContentBundleEditor> GetEditorContentBundle(const UContentBundleDescriptor* Descriptor, const UWorld* ContentBundleWorld) const;
	TSharedPtr<FContentBundleEditor> GetEditorContentBundle(const FGuid& ContentBundleGuid) const;

	UContentBundleDuplicateForPIEHelper* GetPIEDuplicateHelper() const { return PIEDuplicateHelper; }
#endif

	const FContentBundleBase* GetContentBundle(const UWorld* InWorld, const FGuid& Guid) const;
	void DrawContentBundlesStatus(const UWorld* InWorld, UCanvas* Canvas, FVector2D& Offset) const;

private:
#if WITH_EDITOR
	bool TryInject(FContentBundleClient& Client);
	void Remove(FContentBundleClient& Client);
#endif

	uint32 GetContentBundleContainerIndex(const UWorld* InjectedWorld) const;
	const TUniquePtr<FContentBundleContainer>* GetContentBundleContainer(const UWorld* InjectedWorld) const;
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