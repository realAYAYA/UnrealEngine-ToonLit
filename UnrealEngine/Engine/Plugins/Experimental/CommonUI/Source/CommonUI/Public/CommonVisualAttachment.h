// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Widgets/SWidget.h"
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

	/** */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Child Layout")
	FVector2D ContentAnchor;

protected:
	// UVisual interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void SynchronizeProperties() override;
	// End of UWidget interface

private:
	TSharedPtr<class SVisualAttachmentBox> MyAttachmentBox;
};
