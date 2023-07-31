// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGCommon.h"
#include "PCGData.h"
#include "PCGElement.h"
#include "PCGDebug.h"
#include "Tests/Determinism/PCGDeterminismSettings.h"

#include "PCGSettings.generated.h"

class UPCGGraph;
class UPCGNode;
class UPCGSettings;

typedef TMap<FName, TSet<TWeakObjectPtr<const UPCGSettings>>> FPCGTagToSettingsMap;

UENUM()
enum class EPCGSettingsExecutionMode : uint8
{
	Enabled,
	Debug,
	Isolated,
	Disabled
};

UENUM()
enum class EPCGSettingsType : uint8
{
	InputOutput,
	Spatial,
	Density,
	Blueprint,
	Metadata,
	Filter,
	Sampler,
	Spawner,
	Subgraph,
	Debug,
	Generic,
	Param
};

#if WITH_EDITOR
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPCGSettingsChanged, UPCGSettings*, EPCGChangeType);
#endif

/**
* Base class for settings-as-data in the PCG framework
*/
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGSettings : public UPCGData
{
	GENERATED_BODY()

public:
	// ~Begin UPCGData interface
	virtual EPCGDataType GetDataType() const override { return EPCGDataType::Settings | Super::GetDataType(); }
	// ~End UPCGData interface

	// TODO: check if we need this to be virtual, we don't really need if we're always caching
	/*virtual*/ FPCGElementPtr GetElement() const;
	virtual UPCGNode* CreateNode() const;

	virtual TArray<FPCGPinProperties> InputPinProperties() const;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const;

	bool operator==(const UPCGSettings& Other) const;
	uint32 GetCrc32() const;
	bool UseSeed() const { return bUseSeed; }

#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const { return NAME_None; }
	virtual FLinearColor GetNodeTitleColor() const { return FLinearColor::White; }
	virtual EPCGSettingsType GetType() const { return EPCGSettingsType::Generic; }
	/** Derived classes must implement this to communicate dependencies on external actors */
	virtual void GetTrackedActorTags(FPCGTagToSettingsMap& OutTagToSettings, TArray<TObjectPtr<const UPCGGraph>>& OutVisitedGraphs) const {}
	/** Override this class to provide an UObject to jump to in case of double click on node
	 *  ie. returning a blueprint instance will open the given blueprint in its editor.
	 *  By default, it will return the underlying class, to try to jump to its header in code
     */
	virtual UObject* GetJumpTargetForDoubleClick() const;
#endif

	/** Derived classes can implement this to expose additional name information in the logs */
	virtual FName AdditionalTaskName() const { return NAME_None; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta=(EditCondition=bUseSeed, EditConditionHides))
	int Seed = 0xC35A9631; // random prime number

	/** TODO: Remove this - Placeholder feature until we have a nodegraph */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Tags")
	TSet<FString> FilterOnTags;

	/** TODO: Remove this - Placeholder feature until we have a nodegraph */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Tags")
	bool bPassThroughFilteredOutInputs = true;

	/** TODO: Remove this - Placeholder feature until we have a nodegraph */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Tags")
	TSet<FString> TagsAppliedOnOutput;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Debug)
	EPCGSettingsExecutionMode ExecutionMode = EPCGSettingsExecutionMode::Enabled;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Debug, meta = (ShowOnlyInnerProperties))
	FPCGDebugVisualizationSettings DebugSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Determinism, meta = (ShowOnlyInnerProperties))
	FPCGDeterminismSettings DeterminismSettings;
#endif

#if WITH_EDITOR
	FOnPCGSettingsChanged OnSettingsChangedDelegate;
#endif

protected:
	virtual FPCGElementPtr CreateElement() const PURE_VIRTUAL(UPCGSettings::CreateElement, return nullptr;);

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual bool IsStructuralProperty(const FName& InPropertyName) const { return false; }

	/** Method that can be called to dirty the cache data from this settings objects if the operator== does not allow to detect changes */
	void DirtyCache();
#endif

	// By default, settings won't use a seed. Set this bool at true in the child ctor to allow edition and use it.
	UPROPERTY(VisibleAnywhere, Transient, Category = Settings, meta = (EditCondition = false, EditConditionHides))
	bool bUseSeed = false;

	/** Methods to remove boilerplate code across settings */
	TArray<FPCGPinProperties> DefaultPointOutputPinProperties() const;

private:
	mutable FPCGElementPtr CachedElement;
	mutable FCriticalSection CacheLock;
};

/** Trivial / Pass-through settings used for input/output nodes */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGTrivialSettings : public UPCGSettings
{
	GENERATED_BODY()

protected:
	//~UPCGSettings implementation
	virtual FPCGElementPtr CreateElement() const override;
};

class PCG_API FPCGTrivialElement : public FSimplePCGElement
{
public:
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return true; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool IsPassthrough() const override { return true; }
};
