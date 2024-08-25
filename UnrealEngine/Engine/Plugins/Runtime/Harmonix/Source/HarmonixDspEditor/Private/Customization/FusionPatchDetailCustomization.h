// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "DetailCategoryBuilder.h"
#include "UObject/NameTypes.h"
#include "Templates/SharedPointer.h"
#include "Layout/Visibility.h"

class SWidget;

//Fusion Patch detail customizatiaon class
class FFusionPatchDetailCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShareable(new FFusionPatchDetailCustomization);
	}

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;
};


