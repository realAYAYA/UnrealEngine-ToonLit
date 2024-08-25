// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGNode.generated.h"

class UPCGNode;
class UPCGPin;
enum class EPCGChangeType : uint8;
enum class EPCGNodeTitleType : uint8;
struct FPCGPinProperties;

class UPCGSettings;
class UPCGSettingsInterface;
class UPCGGraph;
class UPCGEdge;
class IPCGElement;

#if WITH_EDITOR
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPCGNodeChanged, UPCGNode*, EPCGChangeType);
#endif

UCLASS(ClassGroup = (Procedural))
class PCG_API UPCGNode : public UObject
{
	GENERATED_BODY()

	friend class UPCGGraph;
	friend class UPCGEdge;
	friend class FPCGGraphCompiler;

public:
	UPCGNode(const FObjectInitializer& ObjectInitializer);
	
	/** ~Begin UObject interface */
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;

#if WITH_EDITOR
	virtual void PreEditUndo() override;
	virtual void PostEditUndo() override;
	/** ~End UObject interface */

	/** UpdatePins will kick off invalid edges, so this is useful for making pin changes graceful. */
	void ApplyDeprecationBeforeUpdatePins();

	/** Used to be able to force deprecation when things need to be deprecated at the graph level. */
	void ApplyDeprecation();

	/** If a node does require structural changes, this will apply them */
	virtual void ApplyStructuralDeprecation();

	virtual void RebuildAfterPaste();
#endif

	/** Returns the owning graph */
	UFUNCTION(BlueprintCallable, Category = Node)
	UPCGGraph* GetGraph() const;

	/** Adds an edge in the owning graph to the given "To" node. */
	UFUNCTION(BlueprintCallable, Category = Node)
	UPCGNode* AddEdgeTo(FName FromPinLabel, UPCGNode* To, FName ToPinLabel);

	/** Removes an edge originating from this node */
	UFUNCTION(BlueprintCallable, Category = Node)
	bool RemoveEdgeTo(FName FromPinLable, UPCGNode* To, FName ToPinLabel);

	/** Get title for node of specified type. */
	FText GetNodeTitle(EPCGNodeTitleType TitleType) const;

	/** Whether user has renamed the node. */
	bool HasAuthoredTitle() const { return NodeTitle != NAME_None; }

	/** Title to use if no title is authored. */
	FText GetDefaultTitle() const;

	/** Authored part of node title (like "Create Attribute 1"). */
	FText GetAuthoredTitleLine() const;

	/** Whether to flip the order of the title lines - display generated title first and authored second. */
	bool HasFlippedTitleLines() const;

	/** Generated part of node title, not user editable (like "MyValue = 5.0"). */
	FText GetGeneratedTitleLine() const;

#if WITH_EDITOR
	/** Tooltip that describes node functionality and other information. */
	FText GetNodeTooltipText() const;
#endif

	/** Returns all the input pin properties */
	TArray<FPCGPinProperties> InputPinProperties() const;

	/** Returns all the output pin properties */
	TArray<FPCGPinProperties> OutputPinProperties() const;

	/** Returns true if the input pin is connected */
	bool IsInputPinConnected(const FName& Label) const;

	/** Returns true if the output pin is connected */
	bool IsOutputPinConnected(const FName& Label) const;

	/** Returns true if the node has an instance of the settings (e.g. does not own the settings) */
	bool IsInstance() const;

	/** Returns the settings interface (settings or instance of settings) on this node */
	UPCGSettingsInterface* GetSettingsInterface() const { return SettingsInterface.Get(); }

	/** Changes the default settings in the node */
	void SetSettingsInterface(UPCGSettingsInterface* InSettingsInterface, bool bUpdatePins = true);

	/** Returns the settings this node holds (either directly or through an instance) */
	UFUNCTION(BlueprintCallable, Category = Node)
	UPCGSettings* GetSettings() const;

	/** Triggers some uppdates after creating a new node and changing its settings */
	void UpdateAfterSettingsChangeDuringCreation();

	UPCGPin* GetInputPin(const FName& Label);
	const UPCGPin* GetInputPin(const FName& Label) const;
	UPCGPin* GetOutputPin(const FName& Label);
	const UPCGPin* GetOutputPin(const FName& Label) const;
	bool HasInboundEdges() const;
	int32 GetInboundEdgesNum() const;

	/** Allow to change the name of a pin, to keep edges connected. You need to make sure that the underlying settings are also updated, otherwise, it will be overwritten next time the settings are updated */
	void RenameInputPin(const FName& InOldLabel, const FName& InNewLabel, bool bInBroadcastUpdate = true);
	void RenameOutputPin(const FName& InOldLabel, const FName& InNewLabel, bool bInBroadcastUpdate = true);

	/** Pin from which data is passed through when this node is disabled. */
	virtual const UPCGPin* GetPassThroughInputPin() const;

	/** Pin to which data is passed through when this node is disabled. */
	virtual const UPCGPin* GetPassThroughOutputPin() const;

	/** A node will be executed (not culled) if at least one required-pin is connected to at least one active upstream pin. */
	virtual bool IsInputPinRequiredByExecution(const UPCGPin* InPin) const;

	/** True if the pin is being used by the node. UI will gray out unused pins. */
	virtual bool IsPinUsedByNodeExecution(const UPCGPin* InPin) const;

	/** True if the edge is being used by the node. UI will gray out unused pins. */
	virtual bool IsEdgeUsedByNodeExecution(const UPCGEdge* InEdge) const;

	/** Returns the first connected pin on the node */
	const UPCGPin* GetFirstConnectedInputPin() const;

	const TArray<TObjectPtr<UPCGPin>>& GetInputPins() const { return InputPins; }
	const TArray<TObjectPtr<UPCGPin>>& GetOutputPins() const { return OutputPins; }

	/** Recursively follow downstream edges and call UpdatePins on each node that has dynamic pins. */
	EPCGChangeType PropagateDynamicPinTypes(TSet<UPCGNode*>& TouchedNodes, const UPCGNode* FromNode = nullptr);

#if WITH_EDITOR
	/** Transfer all editor only properties to the other node */
	void TransferEditorProperties(UPCGNode* OtherNode) const;
#endif // WITH_EDITOR

#if WITH_EDITOR
	UFUNCTION(BlueprintCallable, Category = Node)
	void GetNodePosition(int32& OutPositionX, int32& OutPositionY) const;

	UFUNCTION(BlueprintCallable, Category = Node)
	void SetNodePosition(int32 InPositionX, int32 InPositionY);
#endif

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UPCGSettings> DefaultSettings_DEPRECATED; 
#endif

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Node)
	FName NodeTitle = NAME_None;

#if WITH_EDITORONLY_DATA
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Node)
	FLinearColor NodeTitleColor = FLinearColor::White;
#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR
	FOnPCGNodeChanged OnNodeChangedDelegate;
#endif

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	int32 PositionX;

	UPROPERTY()
	int32 PositionY;

	UPROPERTY()
	FString NodeComment;

	UPROPERTY()
	uint8 bCommentBubblePinned : 1;

	UPROPERTY()
	uint8 bCommentBubbleVisible : 1;
#endif // WITH_EDITORONLY_DATA

protected:
	/** Updates pins based on node settings. Attempts to migrate pins via matching. Broadcasts node change events for affected nodes. */
	EPCGChangeType UpdatePins();
	/** Updates pins based on node settings PinAllocator creates new pin objects. Attempts to migrate pins via matching. Broadcasts node change events for affected nodes. */
	EPCGChangeType UpdatePins(TFunctionRef<UPCGPin* (UPCGNode*)> PinAllocator);

	// When we create a new graph, we initialize the input/output nodes as default, with default pins.
	// Those default pins are not serialized, therefore if we change the default pins, combined with the use
	// of recycling objects in Unreal, can lead to pins that are garbage or even worse: valid pins but not the right
	// one, potentially making the edges connecting wrong pins together!
	// That is why we have a specific function to create default pins, and we have to make sure that those
	// default pins are always created the same way.
	void CreateDefaultPins(TFunctionRef<UPCGPin* (UPCGNode*)> PinAllocator);

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	void OnSettingsChanged(UPCGSettings* InSettings, EPCGChangeType ChangeType);
#endif

	/** Note: do not set this property directly from code, use SetSettingsInterface instead */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Node)
	TObjectPtr<UPCGSettingsInterface> SettingsInterface;

	UPROPERTY()
	TArray<TObjectPtr<UPCGNode>> OutboundNodes_DEPRECATED;

	UPROPERTY(TextExportTransient)
	TArray<TObjectPtr<UPCGEdge>> InboundEdges_DEPRECATED;

	UPROPERTY(TextExportTransient)
	TArray<TObjectPtr<UPCGEdge>> OutboundEdges_DEPRECATED;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Node)
	TArray<TObjectPtr<UPCGPin>> InputPins;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Node)
	TArray<TObjectPtr<UPCGPin>> OutputPins;

	// TODO: add this information:
	// - Ability to run on non-game threads (here or element)
	// - Ability to be multithreaded (here or element)
	// - Generates artifacts (here or element)
	// - Priority
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "PCGCommon.h"
#include "PCGPin.h"
#endif
