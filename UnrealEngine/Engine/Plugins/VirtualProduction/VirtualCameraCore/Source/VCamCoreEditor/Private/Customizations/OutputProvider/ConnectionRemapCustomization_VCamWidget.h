// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Customization/IConnectionRemapCustomization.h"
#include "UI/VCamConnectionStructs.h"

namespace UE::VCamCoreEditor::Private
{
	class FConnectionRemapCustomization_VCamWidget : public IConnectionRemapCustomization
	{
	public:

		static TSharedRef<IConnectionRemapCustomization> Make();

		//~ Begin IConnectionRemappingCustomization Interface
		virtual bool CanGenerateGroup(const FShouldGenerateArgs& Args) const override;
		virtual void Customize(const FConnectionRemapCustomizationArgs& Args) override;
		//~ End IConnectionRemappingCustomization Interface

	private:

		TWeakObjectPtr<UVCamWidget> Widget;
		
		void OnTargetSettingsChanged(const FVCamConnectionTargetSettings& NewConnectionTargetSettings, FName ConnectionName) const;
	};
}

