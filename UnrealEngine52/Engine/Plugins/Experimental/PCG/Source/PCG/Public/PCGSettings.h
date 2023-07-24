// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGData.h"
#include "PCGDebug.h"
#include "PCGElement.h"
#include "Tests/Determinism/PCGDeterminismSettings.h"

#include "PCGSettings.generated.h"

class UPCGComponent;
class UPCGPin;
struct FPCGPinProperties;
struct FPropertyChangedEvent;

class UPCGGraph;
class UPCGNode;
class UPCGSettings;

using FPCGSettingsAndCulling = TPair<TWeakObjectPtr<const UPCGSettings>, bool>;
using FPCGTagToSettingsMap = TMap<FName, TSet<FPCGSettingsAndCulling>>;

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

USTRUCT()
struct FPCGSettingsOverridableParam
{
	GENERATED_BODY()

	UPROPERTY()
	FName Label = NAME_None;

	UPROPERTY()
	TArray<FName> PropertiesNames;

	UPROPERTY()
	TObjectPtr<const UStruct> PropertyClass;

	// Transient
	TArray<const FProperty*> Properties;
};


UCLASS(Abstract)
class PCG_API UPCGSettingsInterface : public UPCGData
{
	GENERATED_BODY()

public:
	virtual UPCGSettings* GetSettings() PURE_VIRTUAL(UPCGSettingsInterface::GetSettings, return nullptr;);
	virtual const UPCGSettings* GetSettings() const PURE_VIRTUAL(UPCGSettingsInterface::GetSettings, return nullptr;);

	bool IsInstance() const;
	/** Dedicated method to change enable state because some nodes have more complex behavior on enable/disable (such as subgraphs) */
	void SetEnabled(bool bInEnabled);

#if WITH_EDITOR
	FOnPCGSettingsChanged OnSettingsChangedDelegate;
#endif

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Debug)
	bool bEnabled = true;

	UPROPERTY(Transient, EditAnywhere, BlueprintReadWrite, Category = Debug)
	bool bDebug = false;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Debug, meta = (ShowOnlyInnerProperties))
	FPCGDebugVisualizationSettings DebugSettings;
#endif
};

/**
* Base class for settings-as-data in the PCG framework
*/
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGSettings : public UPCGSettingsInterface
{
	GENERATED_BODY()

	friend class FPCGSettingsObjectCrc32;
	friend class UPCGSettingsInterface;

public:
	// ~Begin UPCGData interface
	virtual EPCGDataType GetDataType() const override { return EPCGDataType::Settings; }
	// ~End UPCGData interface

	// ~Begin UPCGSettingsInterface interface
	virtual UPCGSettings* GetSettings() { return this; }
	virtual const UPCGSettings* GetSettings() const { return this; }
	// ~End UPCGSettingsInterface interface

	//~Begin UObject interface
	virtual void PostLoad() override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostSaveRoot(FObjectPostSaveRootContext ObjectSaveContext) override;
	virtual void PostInitProperties() override;
#if WITH_EDITOR
	virtual void PostEditUndo() override;
#endif // WITH_EDITOR
	//~End UObject interface

	// TODO: check if we need this to be virtual, we don't really need if we're always caching
	/*virtual*/ FPCGElementPtr GetElement() const;
	virtual UPCGNode* CreateNode() const;
	
	/** Return the concatenation of InputPinPropertiesand FillOverridableParamsPins */
	TArray<FPCGPinProperties> AllInputPinProperties() const;

	/** For symmetry reason, do the same with output pins. For now forward just the call to OutputPinProperties */
	TArray<FPCGPinProperties> AllOutputPinProperties() const;

	/** If the node has any dynamic pins that can change based on input or settings */
	virtual bool HasDynamicPins() const { return false; }

	/** Bitwise union of the allowed types of each incident edge on pin. Returns None if no common bits, or no edges. */
	EPCGDataType GetTypeUnionOfIncidentEdges(const FName& PinLabel) const;

	// Internal functions, should not be used by any user.
	// Return a different subset for for input/output pin properties, in case of a default object.
	virtual TArray<FPCGPinProperties> DefaultInputPinProperties() const;
	virtual TArray<FPCGPinProperties> DefaultOutputPinProperties() const;

	bool operator==(const UPCGSettings& Other) const;
	
	bool UseSeed() const { return bUseSeed; }

	// Get the seed, combined with optional PCGComponent seed
	int GetSeed(const UPCGComponent* InSourceComponent = nullptr) const;

	/** Get the Crc value precomputed for these settings. */
	const FPCGCrc& GetCachedCrc() const { return CachedCrc; }

#if WITH_EDITOR
	/** UpdatePins will kick off invalid edges, so this is useful for moving edges around in case of pin changes. */
	virtual void ApplyDeprecationBeforeUpdatePins(UPCGNode* InOutNode, TArray<TObjectPtr<UPCGPin>>& InputPins, TArray<TObjectPtr<UPCGPin>>& OutputPins);
	/** Any final migration/recovery that can be done after pins are finalized. This function should also set DataVersion to LatestVersion. */
	virtual void ApplyDeprecation(UPCGNode* InOutNode);

	virtual FName GetDefaultNodeName() const { return NAME_None; }
	virtual FText GetDefaultNodeTitle() const { return FText::FromName(GetDefaultNodeName()); }
	virtual FText GetNodeTooltipText() const { return FText::GetEmpty(); }
	virtual FLinearColor GetNodeTitleColor() const { return FLinearColor::White; }
	virtual EPCGSettingsType GetType() const { return EPCGSettingsType::Generic; }
	/** Derived classes must implement this to communicate dependencies on external actors */
	virtual void GetTrackedActorTags(FPCGTagToSettingsMap& OutTagToSettings, TArray<TObjectPtr<const UPCGGraph>>& OutVisitedGraphs) const {}
	/** Override this class to provide an UObject to jump to in case of double click on node
	 *  ie. returning a blueprint instance will open the given blueprint in its editor.
	 *  By default, it will return the underlying class, to try to jump to its header in code
     */
	virtual UObject* GetJumpTargetForDoubleClick() const;
	virtual bool IsPropertyOverriddenByPin(const FProperty* InProperty) const;
#endif

	/** Derived classes can implement this to expose additional name information in the logs */
	virtual FName AdditionalTaskName() const { return NAME_None; }

	/** Returns true if InPin is in use by node (assuming node enabled). Can be used to communicate when a pin is not in use to user. */
	virtual bool IsPinUsedByNodeExecution(const UPCGPin* InPin) const { return true; }

	/** Returns true if only the first input edge is used from the primary pin when the node is disabled. */
	virtual bool OnlyPassThroughOneEdgeWhenDisabled() const { return false; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta=(EditCondition=bUseSeed, EditConditionHides, PCG_Overridable))
	int Seed = 0xC35A9631; // random prime number

	/** Warning - this is deprecated and will be removed soon since we have a Filter By Tag node for this specific purpose */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tags")
	TSet<FString> FilterOnTags;

	/** Warning - this is deprecated and will be removed soon since we have a Filter By Tag node for this specific purpose */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tags")
	bool bPassThroughFilteredOutInputs = true;

	/** Applies the specified tags on the output data. Note - this might be replaced by a dedicated Tagging node */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tags")
	TSet<FString> TagsAppliedOnOutput;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	EPCGSettingsExecutionMode ExecutionMode_DEPRECATED = EPCGSettingsExecutionMode::Enabled;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Determinism, meta = (ShowOnlyInnerProperties))
	FPCGDeterminismSettings DeterminismSettings;
#endif

#if WITH_EDITORONLY_DATA
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = AssetInfo, AssetRegistrySearchable)
	bool bExposeToLibrary = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = AssetInfo, AssetRegistrySearchable)
	FText Category;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = AssetInfo, AssetRegistrySearchable)
	FText Description;
#endif

protected:
	// Returns an array of all the input pin properties. You should not add manually a "params" pin, it is handled automatically by FillOverridableParamsPins
	virtual TArray<FPCGPinProperties> InputPinProperties() const;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const;

	virtual FPCGElementPtr CreateElement() const PURE_VIRTUAL(UPCGSettings::CreateElement, return nullptr;);

	/** An additional custom version number that external system users can use to track versions. This version will be serialized into the asset and will be provided by UserDataVersion after load. */
	virtual FGuid GetUserCustomVersionGuid() { return FGuid(); }

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual bool IsStructuralProperty(const FName& InPropertyName) const { return false; }

	/** Method that can be called to dirty the cache data from this settings objects if the operator== does not allow to detect changes */
	void DirtyCache();

	virtual bool CanEditChange(const FProperty* InProperty) const override;
#endif

	// By default, settings won't use a seed. Set this bool to true in the child ctor to allow edition and use it.
	UPROPERTY(VisibleAnywhere, Transient, Category = Settings, meta = (EditCondition = false, EditConditionHides))
	bool bUseSeed = false;

	/** Methods to remove boilerplate code across settings */
	TArray<FPCGPinProperties> DefaultPointInputPinProperties() const;
	TArray<FPCGPinProperties> DefaultPointOutputPinProperties() const;

#if WITH_EDITOR
public:
	/** The version number of the data after load and after any data migration. */
	int32 DataVersion = -1;

	/** If a custom version guid was provided through GetUserCustomVersionGuid(), this field will hold the version number after load and after any data migration. */
	int32 UserDataVersion = -1;
#endif

private:
	mutable FPCGElementPtr CachedElement;
	mutable FCriticalSection CacheLock;

	// Overridable param section
public:
	/** List of all the overridable params available for this settings. */
	virtual const TArray<FPCGSettingsOverridableParam>& OverridableParams() const { return CachedOverridableParams; }

	/** Check if we have some override. Can be overriden to force params pin for example */
	virtual bool HasOverridableParams() const { return !CachedOverridableParams.IsEmpty(); }

	/** Check if we need to hook the output of the pre-task to this. One use is to compute overrides in the subgraph element and pass the overrides as data, to all nodes that needs it. */
	virtual bool ShouldHookToPreTask() const { return false; }
protected:
	/** Iterate over OverridableParams to automatically add param pins to the list. */
	void FillOverridableParamsPins(TArray<FPCGPinProperties>& OutPins) const;

#if WITH_EDITOR
	/** Can be overridden to add more custom params (like in BP). */
	virtual TArray<FPCGSettingsOverridableParam> GatherOverridableParams() const;
#endif // WITH_EDITOR

	/** Called after intialization to construct the list of all overridable params. 
	* In Editor, it will first gather all the overridable properties names and labels, based on their metadata.
	* Can request a "reset" if something changed in the settings.
	* And then, in Editor and Runtime, will gather the FProperty*.
	*/
	void InitializeCachedOverridableParams(bool bReset = false);

	/**
	* There is a weird issue where the BP class is not set correctly in some Server cases.
	* We can try to recover if the PropertyClass is null.
	*/
	virtual void FixingOverridableParamPropertyClass(FPCGSettingsOverridableParam& Param) const;

	/** Needs to be serialized since property metadata (used to populate this array) is not available at runtime. */
	UPROPERTY()
	TArray<FPCGSettingsOverridableParam> CachedOverridableParams;

private:
	/** Calculate Crc for these settings and save it. */
	void CacheCrc();

	/** The cached Crc for these settings. */
	FPCGCrc CachedCrc;
};

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGSettingsInstance : public UPCGSettingsInterface
{
	GENERATED_BODY()

public:
	// ~Begin UPCGSettingsInterface
	virtual UPCGSettings* GetSettings() { return Settings.Get(); }
	virtual const UPCGSettings* GetSettings() const { return Settings.Get(); }
	// ~End UPCGSettingsInterface

	// ~Begin UObject interface
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	// ~End UObject interface

	void SetSettings(UPCGSettings* InSettings);

#if WITH_EDITOR
	void OnSettingsChanged(UPCGSettings* InSettings, EPCGChangeType ChangeType);
#endif

#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient, VisibleAnywhere, Category = Instance)
	TObjectPtr<UPCGSettings> OriginalSettings = nullptr; // Transient just for display
#endif

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Instance, meta = (EditInline))
	TObjectPtr<UPCGSettings> Settings = nullptr;
};

/** Trivial / Pass-through settings used for input/output nodes */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGTrivialSettings : public UPCGSettings
{
	GENERATED_BODY()
	
public:
	UPCGTrivialSettings();

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
	virtual bool IsPassthrough(const UPCGSettings* InSettings) const override { return true; }
};
