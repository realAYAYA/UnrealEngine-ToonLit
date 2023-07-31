// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"

#include "LiveLinkControllerBase.h"

class IDetailLayoutBuilder;

/**
* Customizes ULiveLinkControllerBase details
*/
class FLiveLinkControllerBaseDetailCustomization : public IDetailCustomization
{
public:

	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShared<FLiveLinkControllerBaseDetailCustomization>();
	}

	// IDetailCustomization interface
	virtual void CustomizeDetails(class IDetailLayoutBuilder& DetailBuilder) override;
	// End IDetailCustomization interface

private:
	void OnComponentChanged();

protected:
	TWeakObjectPtr<ULiveLinkControllerBase> LiveLinkControllerWeak;
};
