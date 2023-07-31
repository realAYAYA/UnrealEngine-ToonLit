// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGCommon.h"
#include "PCGPin.h"
#include "PCGNode.generated.h"

class UPCGSettings;
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
	virtual void PostEditImport() override;
	virtual void PreEditUndo() override;
	virtual void PostEditUndo() override;
	/** ~End UObject interface */

	/** Used to be able to force deprecation when things need to be deprecated at the graph level */
	void ApplyDeprecation();
#endif

	/** Returns the owning graph */
	UFUNCTION(BlueprintCallable, Category = Node)
	UPCGGraph* GetGraph() const;

	/** Adds an edge in the owning graph to the given "To" node. */
	UFUNCTION(BlueprintCallable, Category = Node)
	UPCGNode* AddEdgeTo(FName InboundName, UPCGNode* To, FName OutboundName);

	/** Returns the node title, based either on the current node label, or defaulted to its settings */
	FName GetNodeTitle() const;

	/** Returns all the input pin properties */
	TArray<FPCGPinProperties> InputPinProperties() const;

	/** Returns all the output pin properties */
	TArray<FPCGPinProperties> OutputPinProperties() const;

	/** Returns true if the input pin is connected */
	bool IsInputPinConnected(const FName& Label) const;

	/** Returns true if the output pin is connected */
	bool IsOutputPinConnected(const FName& Label) const;

	/** Changes the default settings in the node */
	void SetDefaultSettings(TObjectPtr<UPCGSettings> InSettings, bool bUpdatePins = true);

	/** Triggers some uppdates after creating a new node and changing its settings */
	void UpdateAfterSettingsChangeDuringCreation();

	UPCGPin* GetInputPin(const FName& Label);
	const UPCGPin* GetInputPin(const FName& Label) const;
	UPCGPin* GetOutputPin(const FName& Label);
	const UPCGPin* GetOutputPin(const FName& Label) const;
	bool HasInboundEdges() const;

	const TArray<TObjectPtr<UPCGPin>>& GetInputPins() const { return InputPins; }
	const TArray<TObjectPtr<UPCGPin>>& GetOutputPins() const { return OutputPins; }

#if WITH_EDITOR
	/** Transfer all editor only properties to the other node */
	void TransferEditorProperties(UPCGNode* OtherNode) const;
#endif // WITH_EDITOR

	/** Note: do not set this property directly from code, use SetDefaultSettings instead */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Node, meta=(EditInline))
	TObjectPtr<UPCGSettings> DefaultSettings;

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
	bool UpdatePins();
	bool UpdatePins(TFunctionRef<UPCGPin* (UPCGNode*)> PinAllocator);

#if WITH_EDITOR
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	void OnSettingsChanged(UPCGSettings* InSettings, EPCGChangeType ChangeType);
#endif

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
