// Copyright Epic Games, Inc. All Rights Reserved.

#include "CommonVisualAttachment.h"
#include "SVisualAttachmentBox.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Framework/Application/SlateApplication.h"
#include "Components/SizeBoxSlot.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CommonVisualAttachment)

UCommonVisualAttachment::UCommonVisualAttachment(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UCommonVisualAttachment::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyAttachmentBox.Reset();
}

TSharedRef<SWidget> UCommonVisualAttachment::RebuildWidget()
{
	MyAttachmentBox = SNew(SVisualAttachmentBox);
	MySizeBox = MyAttachmentBox;

	if (GetChildrenCount() > 0)
	{
		Cast<USizeBoxSlot>(GetContentSlot())->BuildSlot(MySizeBox.ToSharedRef());
	}

	return MySizeBox.ToSharedRef();
}

void UCommonVisualAttachment::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	MyAttachmentBox->SetContentAnchor(ContentAnchor);
}
