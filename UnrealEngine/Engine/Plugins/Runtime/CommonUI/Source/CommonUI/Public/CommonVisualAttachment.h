// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/SizeBox.h"

#include "CommonVisualAttachment.generated.h"

/**
 * Adds a widget as a zero-size attachment to another. Think icons to the left of labels, without changing the computed size of the label.
 */
UCLASS(ClassGroup = UI, meta = (Category = "Common UI"))
class COMMONUI_API UCommonVisualAttachment : public USizeBox
{
	GENERATED_BODY()

public:
	UCommonVisualAttachment(const FObjectInitializer& ObjectInitializer);

	UE_DEPRECATED(5.4, "Direct access to ContentAnchor is deprecated. Please use the getter or setter.")
	/** Content Anchor Point as a ratio of the content size. Use (1.0, 1.0) to anchor the content on the bottom right, (0.0) to anchor top left, (0.5, 0.5) to anchor centered.*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category="Child Layout")
	FVector2D ContentAnchor;

	/** Get Content Anchor Point*/
	FVector2D GetContentAnchor() const;

	/** Set Content Anchor Point*/
	void SetContentAnchor(FVector2D InContentAnchor);

protected:
	// UVisual interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void SynchronizeProperties() override;
	// End of UWidget interface

private:
	TSharedPtr<class SVisualAttachmentBox> MyAttachmentBox;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Widgets/SWidget.h"
#endif
