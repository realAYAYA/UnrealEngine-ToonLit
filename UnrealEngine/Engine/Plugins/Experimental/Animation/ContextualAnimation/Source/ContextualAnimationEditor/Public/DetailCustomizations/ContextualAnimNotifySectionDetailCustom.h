// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "IDetailCustomization.h"

class IDetailLayoutBuilder;
class IDetailsView;
class UContextualAnimMovieSceneNotifySection;

/** Detail Customization for ContextualAnimMovieSceneNotifySection */
class FContextualAnimNotifySectionDetailCustom : public IDetailCustomization
{
public:

	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

protected:

	TWeakObjectPtr<UContextualAnimMovieSceneNotifySection> Section;
};