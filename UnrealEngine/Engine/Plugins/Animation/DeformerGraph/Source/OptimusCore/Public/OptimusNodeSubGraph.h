// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "IOptimusParameterBindingProvider.h"
#include "OptimusNodeGraph.h"
#include "OptimusBindingTypes.h"
#include "OptimusNodeSubGraph.generated.h"

class UOptimusNode_GraphTerminal;
enum class EOptimusTerminalType;

UCLASS()
class OPTIMUSCORE_API UOptimusNodeSubGraph :
	public UOptimusNodeGraph,
	public IOptimusParameterBindingProvider
{
	
	GENERATED_BODY()

public:

	static FName GraphDefaultComponentPinName;
	static FName InputBindingsPropertyName;
	static FName OutputBindingsPropertyName;

	UOptimusNodeSubGraph();
	
	// IOptimusParameterBindingProvider 
	FString GetBindingDeclaration(FName BindingName) const override;
	bool GetBindingSupportAtomicCheckBoxVisibility(FName BindingName) const override;
	bool GetBindingSupportReadCheckBoxVisibility(FName BindingName) const override;
	EOptimusDataTypeUsageFlags GetTypeUsageFlags(const FOptimusDataDomain& InDataDomain) const override;

	UOptimusComponentSourceBinding* GetDefaultComponentBinding(const FOptimusPinTraversalContext& InTraversalContext) const;

	UOptimusNode_GraphTerminal* GetTerminalNode(EOptimusTerminalType InTerminalType) const;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnBindingArrayPasted, FName);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnBindingValueChanged, FName);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnBindingArrayItemAdded, FName);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnBindingArrayItemRemoved, FName);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnBindingArrayCleared, FName);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnBindingArrayItemMoved, FName);
	
	FOnBindingArrayPasted& GetOnBindingArrayPasted() { return OnBindingArrayPastedDelegate; }
	FOnBindingValueChanged& GetOnBindingValueChanged() { return OnBindingValueChangedDelegate; }
	FOnBindingArrayItemAdded& GetOnBindingArrayItemAdded() { return OnBindingArrayItemAddedDelegate; }
	FOnBindingArrayItemRemoved& GetOnBindingArrayItemRemoved() { return OnBindingArrayItemRemovedDelegate; }
	FOnBindingArrayCleared& GetOnBindingArrayCleared() { return OnBindingArrayClearedDelegate; }
	FOnBindingArrayItemMoved& GetOnBindingArrayItemMoved() { return OnBindingArrayItemMovedDelegate; }
	
#if WITH_EDITOR
	// UObject overrides
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UPROPERTY(EditAnywhere, Category=Bindings, meta=(AllowParameters))
	FOptimusParameterBindingArray InputBindings;

	UPROPERTY(EditAnywhere, Category=Bindings)
	FOptimusParameterBindingArray OutputBindings;



private:

	void SanitizeBinding(FOptimusParameterBinding& InOutBinding, FName InOldName, bool bInAllowParameter);
	FName GetSanitizedBindingName(FName InNewName, FName InOldName);
	

#if WITH_EDITOR
	void PropertyArrayPasted(const FPropertyChangedEvent& InPropertyChangedEvent);
	void PropertyValueChanged(const FPropertyChangedEvent& InPropertyChangedEvent);
	void PropertyArrayItemAdded(const FPropertyChangedEvent& InPropertyChangedEvent);
	void PropertyArrayItemRemoved(const FPropertyChangedEvent& InPropertyChangedEvent);
	void PropertyArrayCleared(const FPropertyChangedEvent& InPropertyChangedEvent);
	void PropertyArrayItemMoved(const FPropertyChangedEvent& InPropertyChangedEvent);
#endif
	
	FOnBindingArrayPasted OnBindingArrayPastedDelegate;
	FOnBindingValueChanged OnBindingValueChangedDelegate;
	FOnBindingArrayItemAdded OnBindingArrayItemAddedDelegate;
	FOnBindingArrayItemRemoved OnBindingArrayItemRemovedDelegate;
	FOnBindingArrayCleared OnBindingArrayClearedDelegate;
	FOnBindingArrayItemMoved OnBindingArrayItemMovedDelegate;

};
