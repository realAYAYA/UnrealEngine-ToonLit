// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/SlateWrapperTypes.h"
#include "Layout/Margin.h"
#include "Types/SlateEnums.h"
#include "Types/UIFSlotBase.h"
#include "UIFWidget.h"
#include "Widgets/Layout/Anchors.h"

#include "UIFStackBox.generated.h"

class UUIFrameworkStackBox;

/**
 *
 */
USTRUCT(BlueprintType)
struct FUIFrameworkStackBoxSlot : public FUIFrameworkSlotBase
{
	GENERATED_BODY()

	/** Horizontal alignment of the widget inside the slot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Framework")
	TEnumAsByte<EHorizontalAlignment> HorizontalAlignment = EHorizontalAlignment::HAlign_Fill;

	/** Vertical alignment of the widget inside the slot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Framework")
	TEnumAsByte<EVerticalAlignment> VerticalAlignment = EVerticalAlignment::VAlign_Fill;

	/** Distance between that surrounds the widget inside the slot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Framework")
	FMargin Padding = FMargin(0.f, 0.f, 0.f, 0.f);

	/** How much space this slot should occupy in the direction of the panel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Framework")
	FSlateChildSize Size = FSlateChildSize(ESlateSizeRule::Automatic);
};


/**
 *
 */
USTRUCT()
struct UIFRAMEWORK_API FUIFrameworkStackBoxSlotList : public FFastArraySerializer
{
	GENERATED_BODY()

	FUIFrameworkStackBoxSlotList() = default;
	FUIFrameworkStackBoxSlotList(UUIFrameworkStackBox* InOwner)
		: Owner(InOwner)
	{}

public:
	//~ Begin of FFastArraySerializer
	void PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize);
	//~ End of FFastArraySerializer

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms);

	void AddEntry(FUIFrameworkStackBoxSlot Entry);
	bool RemoveEntry(UUIFrameworkWidget* Widget);
	FUIFrameworkStackBoxSlot* FindEntry(FUIFrameworkWidgetId WidgetId);
	void ForEachChildren(const TFunctionRef<void(UUIFrameworkWidget*)>& Func);

private:
	UPROPERTY()
	TArray<FUIFrameworkStackBoxSlot> Slots;

	UPROPERTY(NotReplicated, Transient)
	TObjectPtr<UUIFrameworkStackBox> Owner;
};


template<>
struct TStructOpsTypeTraits<FUIFrameworkStackBoxSlotList> : public TStructOpsTypeTraitsBase2<FUIFrameworkStackBoxSlotList>
{
	enum { WithNetDeltaSerializer = true };
};


/**
 *
 */
UCLASS(DisplayName="StackBox UIFramework")
class UIFRAMEWORK_API UUIFrameworkStackBox : public UUIFrameworkWidget
{
	GENERATED_BODY()

	friend FUIFrameworkStackBoxSlotList;

public:
	UUIFrameworkStackBox();

private:
	/** The orientation of the stack box. */
	UPROPERTY(ReplicatedUsing="OnRep_Orientation", EditAnywhere, BlueprintReadWrite, Setter, Getter, Category = "UI Framework", meta = (AllowPrivateAccess = "true"))
	TEnumAsByte<EOrientation> Orientation = EOrientation::Orient_Horizontal;

public:
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "UI Framework")
	void AddWidget(FUIFrameworkStackBoxSlot Widget);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "UI Framework")
	void RemoveWidget(UUIFrameworkWidget* Widget);

	/** Get the orientation of the stack box. */
	EOrientation GetOrientation() const;
	/** Set the orientation of the stack box. The existing elements will be rearranged. */
	void SetOrientation(EOrientation Value);

	virtual void AuthorityForEachChildren(const TFunctionRef<void(UUIFrameworkWidget*)>& Func) override;
	virtual void AuthorityRemoveChild(UUIFrameworkWidget* Widget) override;
	virtual void LocalAddChild(FUIFrameworkWidgetId ChildId) override;
	virtual void LocalOnUMGWidgetCreated() override;

private:
	void AddEntry(FUIFrameworkStackBoxSlot Entry);
	bool RemoveEntry(UUIFrameworkWidget* Widget);
	FUIFrameworkStackBoxSlot* FindEntry(FUIFrameworkWidgetId WidgetId);

	UFUNCTION()
	void OnRep_Orientation();

private:
	UPROPERTY(Replicated)
	FUIFrameworkStackBoxSlotList ReplicatedSlotList;
};
