// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/OverlaySlot.h"

#include "CommonVisibilitySwitcherSlot.generated.h"

class SBox;
enum class ESlateVisibility : uint8;

UCLASS()
class COMMONUI_API UCommonVisibilitySwitcherSlot : public UOverlaySlot
{
	GENERATED_BODY()

public:

	UCommonVisibilitySwitcherSlot(const FObjectInitializer& Initializer);

	virtual void BuildSlot(TSharedRef<SOverlay> InOverlay) override;

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

	void SetSlotVisibility(ESlateVisibility Visibility);

	const TSharedPtr<SBox>& GetVisibilityBox() const { return VisibilityBox; }

private:

	TSharedPtr<SBox> VisibilityBox;
};
