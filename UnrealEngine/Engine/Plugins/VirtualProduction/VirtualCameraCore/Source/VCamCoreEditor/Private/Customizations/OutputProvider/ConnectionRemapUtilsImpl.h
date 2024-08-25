// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ConnectionContainerDummy.h"
#include "DetailLayoutBuilder.h"
#include "Customization/IConnectionRemapUtils.h"

namespace UE::VCamCoreEditor::Private
{
	class FConnectionRemapUtilsImpl : public IConnectionRemapUtils
	{
	public:
		
		FConnectionRemapUtilsImpl(TSharedRef<IDetailLayoutBuilder> Builder);

		//~ Begin IConnectionRemapUtils Interface
		virtual void AddConnection(FAddConnectionArgs Args) override;
		virtual FSlateFontInfo GetRegularFont() const override;
		virtual void ForceRefreshProperties() const override;
		//~ End IConnectionRemapUtils Interface

	private:
		
		TWeakPtr<IDetailLayoutBuilder> Builder;
		TMap<FName, TSharedPtr<TStructOnScope<FConnectionContainerDummy>>> AddedConnections;
	};

}

