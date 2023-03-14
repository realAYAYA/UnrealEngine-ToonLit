// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TemplateSequenceActions.h"
#include "AssetTypeCategories.h"

class FCameraAnimationSequenceActions : public FTemplateSequenceActions
{
public:
    
    FCameraAnimationSequenceActions(const TSharedRef<ISlateStyle>& InStyle, const EAssetTypeCategories::Type InAssetCategory);

    virtual FText GetName() const override;
    virtual UClass* GetSupportedClass() const override;

protected:

    virtual void InitializeToolkitParams(FTemplateSequenceToolkitParams& ToolkitParams) const override;
};

