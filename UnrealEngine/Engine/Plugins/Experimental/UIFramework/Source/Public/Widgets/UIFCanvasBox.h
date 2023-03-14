// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Layout/Margin.h"
#include "Types/UIFSlotBase.h"
#include "UIFWidget.h"
#include "Widgets/Layout/Anchors.h"

#include "UIFCanvasBox.generated.h"

class UUIFrameworkCanvasBox;

/**
 *
 */
USTRUCT(BlueprintType)
struct FUIFrameworkCanvasBoxSlot : public FUIFrameworkSlotBase
{
	GENERATED_BODY()

	/** Anchors. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Canvas)
	FAnchors Anchors = FAnchors(0.0f, 0.0f);

	/** Offset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Canvas)
	FMargin Offsets = FMargin(0.f, 0.f, 0.f, 0.f);

	/**
	 * Alignment is the pivot point of the widget.  Starting in the upper left at (0,0),
	 * ending in the lower right at (1,1).  Moving the alignment point allows you to move
	 * the origin of the widget.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Canvas)
	FVector2D Alignment = FVector2D::ZeroVector;

	/** The order priority this widget is rendered inside the layer. Higher values are rendered last (and so they will appear to be on top). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Canvas)
	int32 ZOrder = 0;

	/** When true we use the widget's desired size */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Canvas)
	bool bSizeToContent = false;
};


/**
 *
 */
USTRUCT()
struct UIFRAMEWORK_API FUIFrameworkCanvasBoxSlotList : public FFastArraySerializer
{
	GENERATED_BODY()

	FUIFrameworkCanvasBoxSlotList() = default;
	FUIFrameworkCanvasBoxSlotList(UUIFrameworkCanvasBox* InOwner)
		: Owner(InOwner)
	{}

public:
	//~ Begin of FFastArraySerializer
	void PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize);
	//~ End of FFastArraySerializer

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms);

	void AddEntry(FUIFrameworkCanvasBoxSlot Entry);
	bool RemoveEntry(UUIFrameworkWidget* Widget);
	FUIFrameworkCanvasBoxSlot* FindEntry(FUIFrameworkWidgetId WidgetId);
	void ForEachChildren(const TFunctionRef<void(UUIFrameworkWidget*)>& Func);

private:
	UPROPERTY()
	TArray<FUIFrameworkCanvasBoxSlot> Slots;

	UPROPERTY(NotReplicated, Transient)
	TObjectPtr<UUIFrameworkCanvasBox> Owner;
};


template<>
struct TStructOpsTypeTraits<FUIFrameworkCanvasBoxSlotList> : public TStructOpsTypeTraitsBase2<FUIFrameworkCanvasBoxSlotList>
{
	enum { WithNetDeltaSerializer = true };
};


/**
 *
 */
UCLASS(DisplayName="Canvas UIFramework")
class UIFRAMEWORK_API UUIFrameworkCanvasBox : public UUIFrameworkWidget
{
	GENERATED_BODY()

	friend FUIFrameworkCanvasBoxSlotList;

public:
	UUIFrameworkCanvasBox();

public:
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "UI Framework")
	void AddWidget(FUIFrameworkCanvasBoxSlot Widget);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "UI Framework")
	void RemoveWidget(UUIFrameworkWidget* Widget);

	virtual void AuthorityForEachChildren(const TFunctionRef<void(UUIFrameworkWidget*)>& Func) override;
	virtual void AuthorityRemoveChild(UUIFrameworkWidget* Widget) override;
	virtual void LocalAddChild(FUIFrameworkWidgetId ChildId) override;

private:
	void AddEntry(FUIFrameworkCanvasBoxSlot Entry);
	bool RemoveEntry(UUIFrameworkWidget* Widget);
	FUIFrameworkCanvasBoxSlot* FindEntry(FUIFrameworkWidgetId WidgetId);

private:
	UPROPERTY(Replicated)
	FUIFrameworkCanvasBoxSlotList ReplicatedSlotList;
};
