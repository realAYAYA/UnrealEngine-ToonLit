// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "IPropertyTypeCustomization.h"
#include "PropertyEditorModule.h"

namespace UE::AnimNext::Editor
{

class FParameterBlockParameterCustomization : public IDetailCustomization
{
private:

	/** Called when details should be customized */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

	/**
	 * Called when details should be customized, this version allows the customization to store a weak ptr to the layout builder.
	 * This allows property changes to trigger a force refresh of the detail panel.
	 */
	virtual void CustomizeDetails(const TSharedPtr<IDetailLayoutBuilder>& DetailBuilder) override;

	FText GetName() const;
	void SetName(const FText& InNewText, ETextCommit::Type InCommitType);
	bool OnVerifyNameChanged(const FText& InText, FText& OutErrorMessage);
};

}
