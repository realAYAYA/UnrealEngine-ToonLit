// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"

namespace UE::VCamCoreEditor::Private
{
	/** Makes the "Virtual Camera" category appear first on the actor properties. */
	class FVCamBaseActorCustomization : public IDetailCustomization
	{
	public:

		static TSharedRef<IDetailCustomization> MakeInstance();

		//~ Begin IDetailCustomization Interface
		virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
		//~ End IDetailCustomization Interface
	};
}

