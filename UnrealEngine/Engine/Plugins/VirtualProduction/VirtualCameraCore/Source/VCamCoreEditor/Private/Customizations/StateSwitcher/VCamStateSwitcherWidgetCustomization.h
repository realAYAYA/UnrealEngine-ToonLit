// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"

namespace UE::VCamCoreEditor::Private
{
	/** Makes CurrentState a drop-down based on the available states. */
	class FVCamStateSwitcherWidgetCustomization : public IDetailCustomization
	{
	public:
		
		static TSharedRef<IDetailCustomization> MakeInstance();
		void CustomizeCurrentState(IDetailLayoutBuilder& DetailBuilder);

		//~ Begin IDetailCustomization Interface
		virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
		//~ End IDetailCustomization Interface
	};
}


