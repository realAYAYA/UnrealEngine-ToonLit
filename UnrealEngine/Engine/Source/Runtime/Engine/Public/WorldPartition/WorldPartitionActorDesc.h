// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_EDITOR
#include "CoreMinimal.h"
#include "PropertyPairsMap.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/TopLevelAssetPath.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "UObject/WeakObjectPtr.h"
#include "Templates/SubclassOf.h"
#include "Misc/Guid.h"

// Struct used to create actor descriptor
struct FWorldPartitionActorDescInitData
{
	UClass* NativeClass;
	FName PackageName;
	FSoftObjectPath ActorPath;
	TArray<uint8> SerializedData;
};

class AActor;
class UActorDescContainer;
class IStreamingGenerationErrorHandler;
struct FActorContainerID;

enum class EContainerClusterMode : uint8
{
	Partitioned, // Per Actor Partitioning
};

template <typename T, class F>
inline bool CompareUnsortedArrays(const TArray<T>& Array1, const TArray<T>& Array2, F Func)
{
	if (Array1.Num() == Array2.Num())
	{
		TArray<T> SortedArray1(Array1);
		TArray<T> SortedArray2(Array2);
		SortedArray1.Sort(Func);
		SortedArray2.Sort(Func);
		return SortedArray1 == SortedArray2;
	}
	return false;
}

template <typename T>
inline bool CompareUnsortedArrays(const TArray<T>& Array1, const TArray<T>& Array2)
{
	return CompareUnsortedArrays(Array1, Array2, [](const T& A, const T& B) { return A < B; });
}

template <>
inline bool CompareUnsortedArrays(const TArray<FName>& Array1, const TArray<FName>& Array2)
{
	return CompareUnsortedArrays(Array1, Array2, [](const FName& A, const FName& B) { return A.LexicalLess(B); });
}

/**
 * Represents a potentially unloaded actor (editor-only)
 */
class ENGINE_API FWorldPartitionActorDesc
{
	friend class AActor;
	friend class UWorldPartition;
	friend class FActorDescContainerCollection;
	friend struct FWorldPartitionHandleImpl;
	friend struct FWorldPartitionReferenceImpl;
	friend struct FWorldPartitionActorDescUtils;

public:
	virtual ~FWorldPartitionActorDesc() {}

	inline const FGuid& GetGuid() const { return Guid; }
	
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.1, "GetClass is deprecated, GetBaseClass or GetNativeClass should be used instead.")
	inline FName GetClass() const { return GetNativeClass().ToFName(); }
	UE_DEPRECATED(5.1, "GetActorPath is deprecated, GetActorSoftPath should be used instead.")
	inline FName GetActorPath() const { return ActorPath.ToFName(); }
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	inline FTopLevelAssetPath GetBaseClass() const { return BaseClass; }
	inline FTopLevelAssetPath GetNativeClass() const { return NativeClass; }
	inline UClass* GetActorNativeClass() const { return ActorNativeClass; }
	inline FVector GetOrigin() const { return GetBounds().GetCenter(); }
	inline FName GetRuntimeGrid() const { return RuntimeGrid; }
	inline bool GetIsSpatiallyLoaded() const { return bIsForcedNonSpatiallyLoaded ? false : bIsSpatiallyLoaded; }
	inline bool GetIsSpatiallyLoadedRaw() const { return bIsSpatiallyLoaded; }
	inline bool GetActorIsEditorOnly() const { return bActorIsEditorOnly; }
	inline bool GetActorIsRuntimeOnly() const { return bActorIsRuntimeOnly; }

	UE_DEPRECATED(5.1, "SetIsSpatiallyLoadedRaw is deprecated and should not be used.")
	inline void SetIsSpatiallyLoadedRaw(bool bNewIsSpatiallyLoaded) { bIsSpatiallyLoaded = bNewIsSpatiallyLoaded; }

	UE_DEPRECATED(5.1, "GetLevelBoundsRelevant is deprecated.")
	inline bool GetLevelBoundsRelevant() const { return false; }

	inline bool GetActorIsHLODRelevant() const { return bActorIsHLODRelevant; }
	inline FName GetHLODLayer() const { return HLODLayer; }
	inline const TArray<FName>& GetDataLayers() const { return DataLayers; }
	inline const TArray<FName>& GetDataLayerInstanceNames() const { return DataLayerInstanceNames; }
	inline const TArray<FName>& GetTags() const { return Tags; }
	inline void SetDataLayerInstanceNames(const TArray<FName>& InDataLayerInstanceNames) { DataLayerInstanceNames = InDataLayerInstanceNames; }
	inline FName GetActorPackage() const { return ActorPackage; }
	inline FSoftObjectPath GetActorSoftPath() const { return ActorPath; }
	inline FName GetActorLabel() const { return ActorLabel; }
	inline FName GetFolderPath() const { return FolderPath; }
	inline const FGuid& GetFolderGuid() const { return FolderGuid; }
	FBox GetBounds() const;
	inline const FGuid& GetParentActor() const { return ParentActor; }
	inline bool IsUsingDataLayerAsset() const { return bIsUsingDataLayerAsset; }
	inline void AddProperty(FName PropertyName, FName PropertyValue = NAME_None) { Properties.AddProperty(PropertyName, PropertyValue); }
	inline bool GetProperty(FName PropertyName, FName* PropertyValue) const { return Properties.GetProperty(PropertyName, PropertyValue); }
	inline bool HasProperty(FName PropertyName) const { return Properties.HasProperty(PropertyName); }

	FName GetActorName() const;
	FName GetActorLabelOrName() const;
	FName GetDisplayClassName() const;

	virtual bool IsContainerInstance() const { return false; }
	virtual FName GetLevelPackage() const { return NAME_None; }
	virtual bool GetContainerInstance(const UActorDescContainer*& OutLevelContainer, FTransform& OutLevelTransform, EContainerClusterMode& OutClusterMode) const { return false; }

	FGuid GetContentBundleGuid() const;

	virtual const FGuid& GetSceneOutlinerParent() const { return GetParentActor(); }
	virtual bool IsResaveNeeded() const { return false; }
	virtual bool IsRuntimeRelevant(const FActorContainerID& InContainerID) const { return true; }
	virtual void CheckForErrors(IStreamingGenerationErrorHandler* ErrorHandler) const;

	bool operator==(const FWorldPartitionActorDesc& Other) const
	{
		return Guid == Other.Guid;
	}

	friend uint32 GetTypeHash(const FWorldPartitionActorDesc& Key)
	{
		return GetTypeHash(Key.Guid);
	}

protected:
	inline uint32 IncSoftRefCount() const
	{
		return ++SoftRefCount;
	}

	inline uint32 DecSoftRefCount() const
	{
		check(SoftRefCount > 0);
		return --SoftRefCount;
	}

	inline uint32 IncHardRefCount() const
	{
		return ++HardRefCount;
	}

	inline uint32 DecHardRefCount() const
	{
		check(HardRefCount > 0);
		return --HardRefCount;
	}

public:
	inline uint32 GetSoftRefCount() const
	{
		return SoftRefCount;
	}

	inline uint32 GetHardRefCount() const
	{
		return HardRefCount;
	}

	const TArray<FGuid>& GetReferences() const
	{
		return References;
	}

	UActorDescContainer* GetContainer() const
	{
		return Container;
	}

	virtual void SetContainer(UActorDescContainer* InContainer)
	{
		check(!Container || !InContainer);
		Container = InContainer;
	}

	enum class EToStringMode : uint8
	{
		Guid,
		Compact,
		Full
	};

	FString ToString(EToStringMode Mode = EToStringMode::Compact) const;

	bool IsLoaded(bool bEvenIfPendingKill=false) const;
	AActor* GetActor(bool bEvenIfPendingKill=true, bool bEvenIfUnreachable=false) const;
	AActor* Load() const;
	virtual void Unload();

	UE_DEPRECATED(5.1, "ShouldBeLoadedByEditorCells is deprecated, GetActorIsRuntimeOnly should be used instead.")
	bool ShouldBeLoadedByEditorCells() const { return !GetActorIsRuntimeOnly(); }

	virtual void Init(const AActor* InActor);
	virtual void Init(const FWorldPartitionActorDescInitData& DescData);

	virtual bool Equals(const FWorldPartitionActorDesc* Other) const;

	void SerializeTo(TArray<uint8>& OutData);

	void TransformInstance(const FString& From, const FString& To, const FTransform& InstanceTransform);

	using FActorDescDeprecator = TFunction<void(FArchive&, FWorldPartitionActorDesc*)>;
	static void RegisterActorDescDeprecator(TSubclassOf<AActor> ActorClass, const FActorDescDeprecator& Deprecator);

protected:
	FWorldPartitionActorDesc();

	virtual void TransferFrom(const FWorldPartitionActorDesc* From)
	{
		Container = From->Container;
		SoftRefCount = From->SoftRefCount;
		HardRefCount = From->HardRefCount;
		bIsForcedNonSpatiallyLoaded = From->bIsForcedNonSpatiallyLoaded;
	}

	virtual void TransferWorldData(const FWorldPartitionActorDesc* From)
	{
		BoundsLocation = From->BoundsLocation;
		BoundsExtent = From->BoundsExtent;
	}

	virtual void Serialize(FArchive& Ar);

	// Persistent
	FGuid							Guid;
	FTopLevelAssetPath				BaseClass;
	FTopLevelAssetPath				NativeClass;
	FName							ActorPackage;
	FSoftObjectPath					ActorPath;
	FName							ActorLabel;
	FVector							BoundsLocation;
	FVector							BoundsExtent;
	FName							RuntimeGrid;
	bool							bIsSpatiallyLoaded;
	bool							bActorIsEditorOnly;
	bool							bActorIsRuntimeOnly;
	bool							bActorIsHLODRelevant;
	bool							bIsUsingDataLayerAsset; // Used to know if DataLayers array represents DataLayers Asset paths or the FNames of the deprecated version of Data Layers
	FName							HLODLayer;
	TArray<FName>					DataLayers;
	TArray<FGuid>					References;
	TArray<FName>					Tags;
	FPropertyPairsMap				Properties;
	FName							FolderPath;
	FGuid							FolderGuid;
	FGuid							ParentActor; // Used to validate settings against parent (to warn on layer/placement compatibility issues)
	FGuid							ContentBundleGuid;
	
	// Transient
	mutable uint32					SoftRefCount;
	mutable uint32					HardRefCount;
	UClass*							ActorNativeClass;
	mutable TWeakObjectPtr<AActor>	ActorPtr;
	UActorDescContainer*			Container;
	TArray<FName>					DataLayerInstanceNames;
	bool							bIsForcedNonSpatiallyLoaded;

	static TMap<TSubclassOf<AActor>, FActorDescDeprecator> Deprecators;
};
#endif
