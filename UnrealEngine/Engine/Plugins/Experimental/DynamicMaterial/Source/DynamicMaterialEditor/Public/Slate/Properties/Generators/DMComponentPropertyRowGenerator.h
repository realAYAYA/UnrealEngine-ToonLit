// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Set.h"
#include "Templates/SharedPointer.h"

class FName;
class FProperty;
class SDMComponentEdit;
class UDMMaterialComponent;
struct FDMPropertyHandle;

class DYNAMICMATERIALEDITOR_API FDMComponentPropertyRowGenerator
{
public:
	static const TSharedRef<FDMComponentPropertyRowGenerator>& Get();

	FDMComponentPropertyRowGenerator() = default;
	virtual ~FDMComponentPropertyRowGenerator() = default;

	virtual void AddComponentProperties(const TSharedRef<SDMComponentEdit>& InComponentEditWidget, UDMMaterialComponent* InComponent, 
		TArray<FDMPropertyHandle>& InOutPropertyRows, TSet<UDMMaterialComponent*>& InOutProcessedObjects);

	virtual void AddPropertyEditRows(const TSharedRef<SDMComponentEdit>& InComponentEditWidget, UDMMaterialComponent* InComponent, 
		const FName& InProperty, TArray<FDMPropertyHandle>& InOutPropertyRows, TSet<UDMMaterialComponent*>& InOutProcessedObjects);

	virtual void AddPropertyEditRows(const TSharedRef<SDMComponentEdit>& InComponentEditWidget, UDMMaterialComponent* InComponent, FProperty* InProperty, 
		void* MemoryPtr, TArray<FDMPropertyHandle>& InOutPropertyRows, TSet<UDMMaterialComponent*>& InOutProcessedObjects);

	virtual bool AllowKeyframeButton(UDMMaterialComponent* InComponent, FProperty* InProperty);
};
