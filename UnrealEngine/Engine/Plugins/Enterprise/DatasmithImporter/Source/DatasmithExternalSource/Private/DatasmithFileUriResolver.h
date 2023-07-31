// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IUriResolver.h"

namespace UE::DatasmithImporter
{
	class FDatasmithFileUriResolver : public IUriResolver
	{
	public:
		// IUriResolver interface begin
		virtual TSharedPtr<FExternalSource> GetOrCreateExternalSource(const FSourceUri& URI) const override;
		virtual bool CanResolveUri(const FSourceUri& URI) const override;
		virtual FName GetScheme() const override;
#if WITH_EDITOR
		virtual TSharedPtr<FExternalSource> BrowseExternalSource(const FSourceUri& DefaultUri) const override;
#endif //WITH_EDITOR
		// IUriResolver interface end
	};
}