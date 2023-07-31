// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Margin.h"
#include "Components/SlateWrapperTypes.h"
#include "Components/PanelSlot.h"
#include "Widgets/SBoxPanel.h"

#include "StackBoxSlot.generated.h"

class SPanel;

/** The Slot for the UStackBox, contains the widget that is flowed vertically or horizontally. */
UCLASS()
class UMG_API UStackBoxSlot : public UPanelSlot
{
	GENERATED_BODY()

private:
	/** The padding area between the slot and the content it contains. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category="Layout|Stack Box Slot", meta = (AllowPrivateAccess = "true"))
	FMargin Padding;

	/** How much space this slot should occupy in the direction of the panel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category = "Layout|Stack Box Slot", meta = (AllowPrivateAccess = "true"))
	FSlateChildSize Size = FSlateChildSize(ESlateSizeRule::Automatic);

	/** The alignment of the object horizontally. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category="Layout|Stack Box Slot", meta = (AllowPrivateAccess = "true"))
	TEnumAsByte<EHorizontalAlignment> HorizontalAlignment = EHorizontalAlignment::HAlign_Fill;

	/** The alignment of the object vertically. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category="Layout|Stack Box Slot", meta = (AllowPrivateAccess = "true"))
	TEnumAsByte<EVerticalAlignment> VerticalAlignment = EVerticalAlignment::VAlign_Fill;

private:
	/** A raw pointer to the slot to allow us to adjust the size, padding...etc at runtime. */
	SStackBox::FSlot* Slot = nullptr;

public:
	FMargin GetPadding() const;
	void SetPadding(FMargin InPadding);

	FSlateChildSize GetSize() const;
	void SetSize(FSlateChildSize InSize);

	EHorizontalAlignment GetHorizontalAlignment() const;
	void SetHorizontalAlignment(EHorizontalAlignment InHorizontalAlignment);

	EVerticalAlignment GetVerticalAlignment() const;
	void SetVerticalAlignment(EVerticalAlignment InVerticalAlignment);

public:
	//~ UPanelSlot interface
	virtual void SynchronizeProperties() override;
	//~ End of UPanelSlot interface

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

	/** Builds the underlying FSlot for the Slate layout panel. */
	void BuildSlot(TSharedRef<SStackBox> InBox);

#if WITH_EDITOR
	virtual bool NudgeByDesigner(const FVector2D& NudgeDirection, const TOptional<int32>& GridSnapSize) override;
	virtual void SynchronizeFromTemplate(const UPanelSlot* const TemplateSlot) override;
#endif //WITH_EDITOR
};
