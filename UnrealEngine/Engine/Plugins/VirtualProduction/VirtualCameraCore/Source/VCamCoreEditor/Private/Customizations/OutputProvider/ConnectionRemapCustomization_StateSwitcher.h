// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Customizations/OutputProvider/ConnectionRemapCustomization_VCamWidget.h"

namespace UE::VCamCoreEditor::Private
{
	class FConnectionRemapCustomization_StateSwitcher : public FConnectionRemapCustomization_VCamWidget
	{
		using Super = FConnectionRemapCustomization_VCamWidget;
	public:

		static TSharedRef<IConnectionRemapCustomization> Make();
		
		//~ Begin IConnectionRemappingCustomization Interface
		virtual bool CanGenerateGroup(const FShouldGenerateArgs& Args) const override;
		virtual void Customize(const FConnectionRemapCustomizationArgs& Args) override;
		//~ End IConnectionRemappingCustomization Interface

	private:
		
		void AddCurrentStateProperty(const FConnectionRemapCustomizationArgs& Args);
	};
}