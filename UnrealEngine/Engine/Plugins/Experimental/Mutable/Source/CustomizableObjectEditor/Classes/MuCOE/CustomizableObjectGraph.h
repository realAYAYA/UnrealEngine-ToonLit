// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "Containers/Set.h"
#include "EdGraph/EdGraph.h"
#include "Misc/Guid.h"
#include "UObject/NameTypes.h"
#include "UObject/UObjectGlobals.h"

#include "CustomizableObjectGraph.generated.h"

class UObject;


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectGraph : public UEdGraph
{
public:
	GENERATED_BODY()

	UCustomizableObjectGraph();

	// UObject Interface
	virtual void PostLoad() override;
	void PostRename(UObject * OldOuter, const FName OldName) override;

	// Own Interface
	void NotifyNodeIdChanged(const FGuid& OldGuid, const FGuid& NewGuid);

	FGuid RequestNotificationForNodeIdChange(const FGuid& OldGuid, const FGuid& NodeToNotifyGuid);

	void PostDuplicate(bool bDuplicateForPIE) override;

private:

	// Request Node Id Update Map
	TMap<FGuid, TSet<FGuid>> NodesToNotifyMap;

	// Guid map with the key beeing the old Guid and the Value the new one, filled after duplicating COs
	TMap<FGuid, FGuid> NotifiedNodeIdsMap;
};

