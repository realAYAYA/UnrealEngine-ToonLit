// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/ISlateStyle.h"
#include "Toolkits/IToolkitHost.h"
#include "AssetTypeActions_Base.h"
#include "AssetTypeCategories.h"

struct FTemplateSequenceToolkitParams;

/**
 * Implements actions for UTemplateSequence assets.
 */
class FTemplateSequenceActions : public FAssetTypeActions_Base
{
public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InStyle The style set to use for asset editor toolkits.
	 */
	FTemplateSequenceActions(const TSharedRef<ISlateStyle>& InStyle, const EAssetTypeCategories::Type InAssetCategory);

public:

	// IAssetTypeActions interface
	virtual uint32 GetCategories() override;
	virtual FText GetName() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual FColor GetTypeColor() const override;
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;
	virtual bool ShouldForceWorldCentric() override;
	virtual bool CanLocalize() const override { return false; }

protected:

	virtual void InitializeToolkitParams(FTemplateSequenceToolkitParams& ToolkitParams) const;

private:

	/** Pointer to the style set to use for toolkits. */
	TSharedRef<ISlateStyle> Style;

	const EAssetTypeCategories::Type AssetCategory;
};
