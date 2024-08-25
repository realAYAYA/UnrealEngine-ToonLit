// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "K2Node_SwitchName.h"
#include "VCamK2Node_SwitchConnectionSystemBase.generated.h"

/**
 * Base functionality for a switch node for the connections & connection points system.
 * 
 * It updates automatically when connections or connections points are added from a target object.
 * Subclasses define how to obtain connections & connection points.
 */
UCLASS(Abstract)
class VCAMBLUEPRINTNODES_API UVCamK2Node_SwitchConnectionSystemBase : public UK2Node_Switch
{
	GENERATED_BODY()
public:

	UVCamK2Node_SwitchConnectionSystemBase();

	void RefreshPins();

	//~ Begin UObject Interface
	virtual void BeginDestroy() override;
	virtual void PostLoad() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject Interface
	
	//~ Begin UK2Node Interface
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual bool IsActionFilteredOut(FBlueprintActionFilter const& Filter) override;
	virtual void ReconstructNode() override;
	virtual bool CanEverInsertExecutionPin() const override { return false; }
	virtual bool CanEverRemoveExecutionPin() const override { return false; }
	virtual bool ShouldShowNodeProperties() const override { return true; }
	//~ End UK2Node Interface

	// UK2Node_Switch Interface
	virtual FEdGraphPinType GetPinType() const override;
	virtual bool SupportsAddPinButton() const override { return false; }
	// End of UK2Node_Switch Interface

protected:

	virtual bool SupportsBlueprintClass(UClass* Class) const { unimplemented(); return false; }
	virtual TArray<FName> GetPinsToCreate() const { unimplemented(); return {}; }

	//~ Begin UK2Node_Switch Interface
	virtual void CreateSelectionPin() override;
	virtual void CreateCasePins() override;
	//~ End UK2Node_Switch Interface

private:
	
	FDelegateHandle DelegateHandle;
	
	void RemoveOrOrphanInvalidCases(const TArray<FName>& PinData);
	void AddMissingCasePins(const TArray<FName>& PinData);
	void SortCasePinsToHaveSameOrderAsPinData(const TArray<FName>& PinData);

	void SetupBlueprintModifiedCallbacks();
	void CleanUpCallbacks();
	
	void OnObjectTransacted(UObject* Object, const class FTransactionObjectEvent&);
};
