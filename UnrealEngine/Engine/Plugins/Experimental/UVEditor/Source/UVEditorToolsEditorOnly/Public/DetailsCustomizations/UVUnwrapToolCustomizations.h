// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "Templates/SharedPointer.h"

class IPropertyHandle;
class FPropertyRestriction;

// Customization for UUVEditorRecomputeUVsToolProperties
class FUVEditorRecomputeUVsToolDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

protected:
	void EvaluateLayoutTypeRestrictions(TSharedRef<IPropertyHandle> IslandGenerationModeHandle, TSharedRef<IPropertyHandle> UnwrapTypeHandle, TSharedRef<IPropertyHandle> LayoutTypeHandle);

	/** Property restriction applied to blend paint enum dropdown box */
	TSharedPtr<FPropertyRestriction> LayoutTypeEnumRestriction;
};


