// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "IDetailCustomization.h"

class IDetailLayoutBuilder;

class FGeometryCacheAbcFileComponentCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	// IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;
	// End of IDetailCustomization interface

protected:
	FReply ReloadAbcFile();
	bool IsReloadEnabled() const;

	TWeakObjectPtr<class UGeometryCacheAbcFileComponent> GeometryCacheAbcFileComponent;
};
