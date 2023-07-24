// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "Rigs/RigHierarchyDefines.h"
#include "ControlRigBlueprint.h"
#include "Styling/SlateTypes.h"
#include "IPropertyUtilities.h"

class IPropertyHandle;

class FRigInfluenceMapPerEventDetails : public IDetailCustomization
{
public:

	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShareable(new FRigInfluenceMapPerEventDetails);
	}

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

	static UControlRigBlueprint* GetBlueprintFromDetailBuilder(IDetailLayoutBuilder& DetailBuilder);

protected:

	UControlRigBlueprint* BlueprintBeingCustomized;
};
