// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "Templates/SharedPointer.h"

class IPropertyHandle;
class FPropertyRestriction;

// Customization for UBakeTransformToolProperties
class FBakeTransformToolDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

protected:
	void EvaluateBakeScaleMethodRestrictions(TSharedRef<IPropertyHandle> AllowNoScale, TSharedRef<IPropertyHandle> BakeScaleHandle);

	/** Property restriction applied to the EBakeScaleMethod enum dropdown box */
	TSharedPtr<FPropertyRestriction> BakeScaleMethodEnumRestriction;
};
