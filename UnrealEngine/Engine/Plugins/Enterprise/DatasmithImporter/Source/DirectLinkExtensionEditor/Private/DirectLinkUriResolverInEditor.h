// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DirectLinkUriResolver.h"

#include "DirectLinkExtensionEditorModule.h"
#include "DirectLinkExternalSource.h"

namespace UE::DatasmithImporter
{
	class FDirectLinkUriResolverInEditor : public FDirectLinkUriResolver
	{
	public:
		// IUriResolver interface begin
		virtual TSharedPtr<FExternalSource> BrowseExternalSource(const FSourceUri& DefaultSourceUri) const override
		{
			return IDirectLinkExtensionEditorModule::Get().DisplayDirectLinkSourcesDialog();
		}
		// IUriResolver interface end
	};
}