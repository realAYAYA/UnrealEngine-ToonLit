// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "RenderCommandFence.h"
#include "Rendering/NaniteResources.h"
#include "UObject/ObjectMacros.h"
#include "StaticMeshResources.h"

#include "NaniteDisplacedMesh.generated.h"

class FNaniteBuildAsyncCacheTask;
class UNaniteDisplacedMesh;
class UTexture;

USTRUCT(BlueprintType)
struct FNaniteDisplacedMeshDisplacementMap
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Displacement)
	TObjectPtr<class UTexture2D> Texture;

	UPROPERTY(EditAnywhere, Category = Displacement)
	float Magnitude;

	UPROPERTY(EditAnywhere, Category = Displacement)
	float Center;

	FNaniteDisplacedMeshDisplacementMap()
		: Texture(nullptr)
		, Magnitude(0.0f)
		, Center(0.0f)
	{}

	FNaniteDisplacedMeshDisplacementMap(const FNaniteDisplacedMeshDisplacementMap& Other)
		: Texture(Other.Texture)
		, Magnitude(Other.Magnitude)
		, Center(Other.Center)
	{}

	bool operator==(const FNaniteDisplacedMeshDisplacementMap& Other) const
	{
		return Texture		== Other.Texture
			&& Magnitude	== Other.Magnitude
			&& Center		== Other.Center;
	}

	bool operator!=(const FNaniteDisplacedMeshDisplacementMap& Other) const
	{
		return !(*this == Other);
	}
};

USTRUCT(BlueprintType)
struct NANITEDISPLACEDMESH_API FNaniteDisplacedMeshParams
{
	GENERATED_BODY()

#if WITH_EDITORONLY_DATA

	UPROPERTY(EditAnywhere, Category = Mesh)
	TObjectPtr<class UStaticMesh> BaseMesh;

	UPROPERTY(EditAnywhere, Category = Mesh)
	float RelativeError;

	UPROPERTY(EditAnywhere, Category = Texture)
	TArray<FNaniteDisplacedMeshDisplacementMap> DisplacementMaps;

	/** Default settings. */
	FNaniteDisplacedMeshParams()
		: BaseMesh(nullptr)
		, RelativeError(0.03f)
	{}

	FNaniteDisplacedMeshParams(const FNaniteDisplacedMeshParams& Other)
		: BaseMesh(Other.BaseMesh)
		, RelativeError(Other.RelativeError)
		, DisplacementMaps(Other.DisplacementMaps)
	{}

	/** Equality operator. */
	bool operator==(const FNaniteDisplacedMeshParams& Other) const
	{
		if( BaseMesh != Other.BaseMesh ||
			RelativeError != Other.RelativeError ||
			DisplacementMaps.Num() != Other.DisplacementMaps.Num() )
		{
			return false;
		}

		for( int32 i = 0; i < DisplacementMaps.Num(); i++ )
		{
			if( DisplacementMaps[i] != Other.DisplacementMaps[i] )
				return false;
		}

		return true;
	}

	/** Inequality operator. */
	bool operator!=(const FNaniteDisplacedMeshParams& Other) const
	{
		return !(*this == Other);
	}

	// Does the settings would result in the creation of some rendering data
	bool IsDisplacementRequired() const;
#endif // WITH_EDITORONLY_DATA
};

struct FNaniteData
{
	Nanite::FResources Resources;

	// Material section information that matches displaced mesh.
	FStaticMeshSectionArray MeshSections;
};

UCLASS()
class NANITEDISPLACEDMESH_API UNaniteDisplacedMesh : public UObject, public IInterface_AsyncCompilation
{
	GENERATED_BODY()

	friend class FNaniteBuildAsyncCacheTask;

public:
	UNaniteDisplacedMesh(const FObjectInitializer& Init);

	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;
	virtual bool IsReadyForFinishDestroy() override;
	virtual bool NeedsLoadForTargetPlatform(const ITargetPlatform* TargetPlatform) const override;

	/** Does the NaniteDisplacedMesh provide some additional value for that platform */
	static bool IsSupportedByTargetPlatform(const ITargetPlatform* TargetPlatform);

	void InitResources();
	void ReleaseResources();

	bool HasValidNaniteData() const;

	inline Nanite::FResources* GetNaniteData()
	{
		return &Data.Resources;
	}

	inline const Nanite::FResources* GetNaniteData() const
	{
		return &Data.Resources;
	}

	inline const FStaticMeshSectionArray& GetMeshSections() const
	{
		return Data.MeshSections;
	}

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void BeginCacheForCookedPlatformData(const ITargetPlatform* TargetPlatform) override;
	virtual bool IsCachedCookedPlatformDataLoaded(const ITargetPlatform* TargetPlatform) override;
	virtual void ClearAllCachedCookedPlatformData() override;

	/** Returns whether or not the asset is currently being compiled */
	bool IsCompiling() const override;

	/** Try to cancel any pending async tasks.
	 *  Returns true if there is no more async tasks pending, false otherwise.
	 */
	bool TryCancelAsyncTasks();

	/** Returns false if there is currently an async task running */
	bool IsAsyncTaskComplete() const;

	/** Make sure all async tasks are completed before returning */
	void FinishAsyncTasks();

private:
	friend class FNaniteDisplacedMeshCompilingManager;
	void Reschedule(FQueuedThreadPool* InThreadPool, EQueuedWorkPriority InPriority);
#endif

public:
#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Parameters, meta=(EditCondition = "bIsEditable"))
	FNaniteDisplacedMeshParams Parameters;

	/**
	 * Was this asset created by a procedural tool?
	 * This flag is generally set by tool that created the asset.
	 * It's used to tell the users that they shouldn't modify the asset by themselves.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = Asset, AdvancedDisplay)
	bool bIsEditable = true;
#endif

private:
	bool bIsInitialized = false;

	// Data used to render this displaced mesh with Nanite.
	FNaniteData Data;

	FRenderCommandFence ReleaseResourcesFence;

#if WITH_EDITOR
	FIoHash CreateDerivedDataKeyHash(const ITargetPlatform* TargetPlatform);
	FIoHash BeginCacheDerivedData(const ITargetPlatform* TargetPlatform);
	bool PollCacheDerivedData(const FIoHash& KeyHash) const;
	void EndCacheDerivedData(const FIoHash& KeyHash);

	/** Synchronously cache and return derived data for the target platform. */
	FNaniteData& CacheDerivedData(const ITargetPlatform* TargetPlatform);

	FIoHash DataKeyHash;
	TMap<FIoHash, TUniquePtr<FNaniteData>> DataByPlatformKeyHash;
	TMap<FIoHash, TPimplPtr<FNaniteBuildAsyncCacheTask>> CacheTasksByKeyHash;

	DECLARE_MULTICAST_DELEGATE(FOnNaniteDisplacedMeshRenderingDataChanged);
	FOnNaniteDisplacedMeshRenderingDataChanged OnRenderingDataChanged;
#endif

public:
#if WITH_EDITOR
	typedef FOnNaniteDisplacedMeshRenderingDataChanged::FDelegate FOnRebuild;
	FDelegateHandle RegisterOnRenderingDataChanged(const FOnRebuild& Delegate);
	void UnregisterOnRenderingDataChanged(void* Unregister);
	void UnregisterOnRenderingDataChanged(FDelegateHandle Handle);

	void NotifyOnRenderingDataChanged();
#endif

private:
	friend class UGeneratedNaniteDisplacedMeshEditorSubsystem;
#if WITH_EDITOR
	DECLARE_EVENT_OneParam(UNaniteDisplacedMesh, FOnNaniteDisplacmentMeshDependenciesChanged, UNaniteDisplacedMesh*);
	static FOnNaniteDisplacmentMeshDependenciesChanged OnDependenciesChanged;
#endif
};

