// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGData.h"
#include "PCGDebug.h"
#include "PCGElement.h"
#include "PCGPin.h"
#include "Elements/PCGActorSelector.h"
#include "Tests/Determinism/PCGDeterminismSettings.h"

#include "Algo/AllOf.h"

#include "PCGSettings.generated.h"

class UPCGComponent;
class UPCGPin;
struct FPCGPinProperties;
struct FPropertyChangedEvent;

class UPCGGraph;
class UPCGNode;
class UPCGSettings;

using FPCGSettingsAndCulling = TPair<TSoftObjectPtr<const UPCGSettings>, bool>;
using FPCGSelectionKeyToSettingsMap = TMap<FPCGSelectionKey, TArray<FPCGSettingsAndCulling>>;

namespace PCGSettings
{
	// A key is culled if and only if all the settings are culled.
	inline bool IsKeyCulled(const TArray<FPCGSettingsAndCulling>& SettingsAndCulling)
	{
		return Algo::AllOf(SettingsAndCulling, [](const FPCGSettingsAndCulling& SettingsAndCullingPair) { return SettingsAndCullingPair.Value; });
	}
}

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
	Param,
	HierarchicalGeneration,
	ControlFlow,
	PointOps,
	GraphParameters,
	Reroute
};

#if WITH_EDITOR
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPCGSettingsChanged, UPCGSettings*, EPCGChangeType);
#endif

// Dummy struct to bypass the UHT limitation for array of arrays.
USTRUCT(meta=(Hidden))
struct FPCGPropertyAliases
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FName> Aliases;
};

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

	// Map of all aliases for a given property, using its Index (to avoid name clashes within the same path)
	UPROPERTY()
	TMap<int32, FPCGPropertyAliases> MapOfAliases;

	// If this flag is true, Label will be the full property path.
	UPROPERTY()
	bool bHasNameClash = false;

	bool HasAliases() const { return !MapOfAliases.IsEmpty(); }

	// Transient
	TArray<const FProperty*> Properties;

	FString GetPropertyPath() const;

	TArray<FName> GenerateAllPossibleAliases() const;

#if WITH_EDITOR
	FString GetDisplayPropertyPath() const;
	PCG_API FText GetDisplayPropertyPathText() const;
#endif // WITH_EDITOR
};

/**
* Pre-configured settings info
* Will be passed to the settings to pre-configure the settings on creation.
* Example: Maths operations: Add, Mul, etc...
*/
USTRUCT(BlueprintType)
struct FPCGPreConfiguredSettingsInfo
{
	GENERATED_BODY()

	FPCGPreConfiguredSettingsInfo() = default;

	explicit FPCGPreConfiguredSettingsInfo(int32 InIndex, FText InLabel = FText{})
		: PreconfiguredIndex(InIndex)
		, Label(std::move(InLabel))
	{}

#if WITH_EDITOR
	FPCGPreConfiguredSettingsInfo(int32 InIndex, FText InLabel, FText InTooltip)
		: PreconfiguredIndex(InIndex)
		, Label(std::move(InLabel))
		, Tooltip(std::move(InTooltip))
	{}
#endif // WITH_EDITOR

	/* Index used by the settings to know which preconfigured settings it needs to set. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "")
	int32 PreconfiguredIndex = -1;

	/* Label for the exposed asset. Can also be used instead of the index, if it is easier to deal with strings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "")
	FText Label;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "")
	FText Tooltip;
#endif // WITH_EDITORONLY_DATA
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

	/** Whether this element can be disabled. */
	virtual bool CanBeDisabled() const { return true; }

	/** Whether this element supports Debug and Inspect features. */
	virtual bool CanBeDebugged() const { return true; }

#if WITH_EDITOR
	FOnPCGSettingsChanged OnSettingsChangedDelegate;
#endif

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Debug, meta = (EditCondition = bDisplayDebuggingProperties, EditConditionHides, HideEditConditionToggle))
	bool bEnabled = true;

	UPROPERTY(Transient, EditAnywhere, BlueprintReadWrite, Category = Debug, meta = (EditCondition = bDisplayDebuggingProperties, EditConditionHides, HideEditConditionToggle))
	bool bDebug = false;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Debug, meta = (ShowOnlyInnerProperties))
	FPCGDebugVisualizationSettings DebugSettings;

	/** If a debugger is attached, triggers a breakpoint inside IPCGElement::Execute(). Editor only. Transient. */
	UPROPERTY(Transient, DuplicateTransient, EditAnywhere, BlueprintReadWrite, Category = Debug, AdvancedDisplay, meta = (DisplayName = "Break In Debugger", EditCondition = bDisplayDebuggingProperties, EditConditionHides, HideEditConditionToggle))
	bool bBreakDebugger = false;

	// This can be set false by inheriting nodes to hide the debugging properties.
	UPROPERTY(Transient, meta = (EditCondition = false, EditConditionHides))
	bool bDisplayDebuggingProperties = true;
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
	virtual void AddToCrc(FArchiveCrc32& Ar, bool bFullDataCrc) const override;
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
	
	/** Return the concatenation of InputPinProperties and FillOverridableParamsPins */
	TArray<FPCGPinProperties> AllInputPinProperties() const;

	/** For symmetry reason, do the same with output pins. For now forward just the call to OutputPinProperties */
	TArray<FPCGPinProperties> AllOutputPinProperties() const;

	/** If the node has any dynamic pins that can change based on input or settings */
	virtual bool HasDynamicPins() const { return false; }

	/** Whether this node can deactivate its output pins during execution, which can dynamically cull downstream nodes. */
	virtual bool OutputPinsCanBeDeactivated() const { return false; }

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
	/** Puts node title on node body, reducing overall node size */
	virtual bool ShouldDrawNodeCompact() const { return false; }
	/** Returns the icon to use instead of text in compact node form */
	virtual bool GetCompactNodeIcon(FName& OutCompactNodeIcon) const { return false; }
	/** Returns whether the user can directly interact with the node name */
	virtual bool CanUserEditTitle() const { return true; }

	/** UpdatePins will kick off invalid edges, so this is useful for moving edges around in case of pin changes. */
	virtual void ApplyDeprecationBeforeUpdatePins(UPCGNode* InOutNode, TArray<TObjectPtr<UPCGPin>>& InputPins, TArray<TObjectPtr<UPCGPin>>& OutputPins);
	/** Any final migration/recovery that can be done after pins are finalized. This function should also set DataVersion to LatestVersion. */
	virtual void ApplyDeprecation(UPCGNode* InOutNode);

	/** If settings require structural changes, this will apply them */
	virtual void ApplyStructuralDeprecation(UPCGNode* InOutNode);

	virtual FName GetDefaultNodeName() const { return NAME_None; }
	virtual FText GetDefaultNodeTitle() const { return FText::FromName(GetDefaultNodeName()); }

	/** List of extra aliases that will be added to the node list in the Editor.Useful when we rename a node, but we still want the user to find the old one. */
	virtual TArray<FText> GetNodeTitleAliases() const { return {}; }

	virtual FText GetNodeTooltipText() const { return FText::GetEmpty(); }
	virtual FLinearColor GetNodeTitleColor() const { return FLinearColor::White; }
	virtual EPCGSettingsType GetType() const { return EPCGSettingsType::Generic; }

	/** Can override the label style for a pin. Return false if no override is available. */
	virtual bool GetPinLabelStyle(const UPCGPin* InPin, FName& OutLabelStyle) const { return false; }

	/** Can override to add a custom icon next to the pin label (and an optional tooltip). Return false if no override is available. */
	virtual bool GetPinExtraIcon(const UPCGPin* InPin, FName& OutExtraIcon, FText& OutTooltip) const;

	/** Derived classes must implement this to communicate dependencies that are known statically. */
	virtual void GetStaticTrackedKeys(FPCGSelectionKeyToSettingsMap& OutKeysToSettings, TArray<TObjectPtr<const UPCGGraph>>& OutVisitedGraphs) const {}

	/** Derived classes must implement this to communicate that they might have dynamic dependencies. */
	virtual bool CanDynamicallyTrackKeys() const { return false; }

	/** Override this class to provide an UObject to jump to in case of double click on node
	 *  ie. returning a blueprint instance will open the given blueprint in its editor.
	 *  By default, it will return the underlying class, to try to jump to its header in code
     */
	virtual UObject* GetJumpTargetForDoubleClick() const;

	/** Return preconfigured info that will be filled in the editor palette action, allowing to create pre-configured settings */
	virtual TArray<FPCGPreConfiguredSettingsInfo> GetPreconfiguredInfo() const { return {}; }

	/** If there are preconfigured info, we can skip the default settings and only expose pre-configured actions in the editor palette */
	virtual bool OnlyExposePreconfiguredSettings() const { return false; }

	/** If there are preconfigured info, decide if they are grouped in the palette in a folder with the node name, or if they are not grouped. */
	virtual bool GroupPreconfiguredSettings() const { return true; }

	/** Perform post-operations when an editor node is copied */
	virtual void PostPaste();
#endif // WITH_EDITOR

	/** Derived classes can implement this to expose additional information or context, such as an asset in use by the node. */
	virtual FString GetAdditionalTitleInformation() const { return FString(); }

	/** Display generated title line as primary title (example: BP nodes display the blueprint name as the primary title). */
	virtual bool HasFlippedTitleLines() const { return false; }

	/** Returns true if the given property is overridden by graph parameters */
	virtual bool IsPropertyOverriddenByPin(const FProperty* InProperty) const;

	/** Returns true if the base property, given by name, is overridden by graph parameters */
	virtual bool IsPropertyOverriddenByPin(FName PropertyName) const;

	/** Returns true if the property, given by chain of property names, is overridden by graph parameters */
	virtual bool IsPropertyOverriddenByPin(const TArrayView<const FName>& PropertyNameChain) const;

	virtual void ApplyPreconfiguredSettings(const FPCGPreConfiguredSettingsInfo& PreconfigureInfo) {}

	UE_DEPRECATED(5.4, "AdditionalTaskName is deprecated and is replaced with GetAdditionalTitleInformation.")
	virtual FName AdditionalTaskName() const { return FName(GetAdditionalTitleInformation()); }

	/** By default a node does not specify any pin requirements, and will execute if it has no non-advanced pins or if it has any active
	* connection to any pin. Set the Required status on pin properties to specify which pins *must* have active connections to avoid getting culled.
	*/
	virtual bool IsInputPinRequiredByExecution(const UPCGPin* InPin) const { return InPin && InPin->Properties.IsRequiredPin(); }

	/** Returns true if InPin is in use by node (assuming node enabled). Can be used to communicate when a pin is not in use to user. */
	virtual bool IsPinUsedByNodeExecution(const UPCGPin* InPin) const { return true; }

	/** True if we know prior to execution that the given pin will be active (not on inactive branch). */
	virtual bool IsPinStaticallyActive(const FName& OutputPinLabel) const { return true; }

	/** True if we can safely cull this node & task if it has unwired non-advanced inputs. Counter-example: BP nodes may have side effects and always execute
	* if not on an inactive branch.
	*/
	virtual bool CanCullTaskIfUnwired() const { return true; }

	/** Returns true if only the first input edge is used from the primary pin when the node is disabled. */
	virtual bool OnlyPassThroughOneEdgeWhenDisabled() const { return false; }

	/** By default, the first non-Advanced pin that supports pass-through will be selected. */
	virtual bool DoesPinSupportPassThrough(UPCGPin* InPin) const;

	/** Returns the current pin types, which can either be the static types from the pin properties, or a dynamic type based on connected edges.
	* By default we set output pin types to the union of the default input pin incident edge types, if it is dynamic and the default input exists.
	*/
	virtual EPCGDataType GetCurrentPinTypes(const UPCGPin* InPin) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta=(EditCondition=bUseSeed, EditConditionHides, PCG_Overridable))
	int Seed = 0xC35A9631; // Default seed is a random prime number, but will be overriden for new settings based on the class type name hash, making each settings class have a different default seed.

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TSet<FString> TagsAppliedOnOutput_DEPRECATED;

	UPROPERTY()
	EPCGSettingsExecutionMode ExecutionMode_DEPRECATED = EPCGSettingsExecutionMode::Enabled;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Debug, AdvancedDisplay, meta = (DisplayName="Determinism", NoResetToDefault))
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

	// Holds the original settings used to duplicate this object if it was overridden
	const UPCGSettings* OriginalSettings = nullptr;

protected:
	// Returns an array of all the input pin properties. You should not add manually a "params" pin, it is handled automatically by FillOverridableParamsPins
	virtual TArray<FPCGPinProperties> InputPinProperties() const;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const;

	virtual FPCGElementPtr CreateElement() const PURE_VIRTUAL(UPCGSettings::CreateElement, return nullptr;);

	/** An additional custom version number that external system users can use to track versions. This version will be serialized into the asset and will be provided by UserDataVersion after load. */
	virtual FGuid GetUserCustomVersionGuid() { return FGuid(); }

	/** Can be overriden by child class if they ever got renamed to avoid changing the default seed for this one. Otherwise default is hash of the class name. */
	virtual uint32 GetTypeNameHash() const;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	UE_DEPRECATED(5.4, "IsStructuralProperty is deprecated, return EPCGChangeType::Structural from GetChangeTypeForProperty instead.")
	virtual bool IsStructuralProperty(const FName& InPropertyName) const { return false; }

	/** Gets the change impact for a given property. Can be used to signal structural or cosmetic node changes for example - calls the named version by default. */
	virtual EPCGChangeType GetChangeTypeForProperty(FPropertyChangedEvent& PropertyChangedEvent) const;

	/** Gets the change impact for a given property. Can be used to signal structural or cosmetic node changes for example. */
	virtual EPCGChangeType GetChangeTypeForProperty(const FName& InPropertyName) const;

	/** Method that can be called to dirty the cache data from this settings objects if the operator== does not allow to detect changes */
	void DirtyCache();

	// We need the more complex function (with PropertyChain) to detect child properties in structs, if they are overridable
	virtual bool CanEditChange(const FEditPropertyChain& InPropertyChain) const override;

	// Passthrough for the simpler method, to avoid modifying the child settings already overriding this method.
	virtual bool CanEditChange(const FProperty* InProperty) const override;
#endif

	// By default, settings won't use a seed. Set this bool to true in the child ctor to allow edition and use it.
	UPROPERTY(VisibleAnywhere, Transient, Category = Settings, meta = (EditCondition = false, EditConditionHides))
	bool bUseSeed = false;

	/** Methods to remove boilerplate code across settings */
	TArray<FPCGPinProperties> DefaultPointInputPinProperties() const;
	TArray<FPCGPinProperties> DefaultPointOutputPinProperties() const;

public:
#if WITH_EDITORONLY_DATA
	/** The version number of the data after load and after any data migration. */
	int32 DataVersion = -1;

	/** If a custom version guid was provided through GetUserCustomVersionGuid(), this field will hold the version number after load and after any data migration. */
	int32 UserDataVersion = -1;
#endif // WITH_EDITORONLY_DATA

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
	virtual bool RequiresDataFromPreTask() const { return false; }
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
	virtual void PostEditImport() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	// ~End UObject interface

	void SetSettings(UPCGSettings* InSettings);

protected:
#if WITH_EDITOR
	void OnSettingsChanged(UPCGSettings* InSettings, EPCGChangeType ChangeType);

	void PostSettingsChanged();
#endif

#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient, VisibleAnywhere, Category = Instance)
	TObjectPtr<UPCGSettings> OriginalSettings = nullptr; // Transient just for display
#endif

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Instance, meta = (EditInline))
	TObjectPtr<UPCGSettings> Settings = nullptr;
};

/** Trivial / Pass-through settings used for input/output nodes */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGTrivialSettings : public UPCGSettings
{
	GENERATED_BODY()
	
public:
	UPCGTrivialSettings();

protected:
	//~UPCGSettings implementation
	virtual FPCGElementPtr CreateElement() const override;
};

class FPCGTrivialElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool IsPassthrough(const UPCGSettings* InSettings) const override { return true; }
};