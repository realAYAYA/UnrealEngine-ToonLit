// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/Widget.h"
#include "Blueprint/UserWidgetPool.h"
#include "RadialBoxSettings.h"
#include "DynamicEntryBoxBase.generated.h"

class UUserWidget;

UENUM(BlueprintType)
enum class EDynamicBoxType : uint8
{
	Horizontal,
	Vertical,
	Wrap,
	VerticalWrap,
	Radial,
	Overlay
};

/**
 * Base for widgets that support a dynamic number of auto-generated entries at both design- and run-time.
 * Contains all functionality needed to create, construct, and cache an arbitrary number of entry widgets, but exposes no means of entry creation or removal
 * It's up to child classes to decide how they want to perform the population (some may do so entirely on their own without exposing a thing)
 *
 * @see UDynamicEntryBox for a ready-to-use version
 */
UCLASS(Abstract)
class UMG_API UDynamicEntryBoxBase : public UWidget
{
	GENERATED_BODY()

public:
	UDynamicEntryBoxBase(const FObjectInitializer& Initializer);
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

	EDynamicBoxType GetBoxType() const { return EntryBoxType; }
	const FVector2D& GetEntrySpacing() const { return EntrySpacing; }

	UFUNCTION(BlueprintCallable, Category = DynamicEntryBox)
	const TArray<UUserWidget*>& GetAllEntries() const;

	template <typename EntryWidgetT = UUserWidget>
	TArray<EntryWidgetT*> GetTypedEntries() const
	{
		TArray<EntryWidgetT*> TypedEntries;
		for (UUserWidget* Entry : GetAllEntries())
		{
			if (EntryWidgetT* TypedEntry = Cast<EntryWidgetT>(Entry))
			{
				TypedEntries.Add(TypedEntry);
			}
		}
		return TypedEntries;
	}

	UFUNCTION(BlueprintCallable, Category = DynamicEntryBox)
	int32 GetNumEntries() const;

	UFUNCTION(BlueprintCallable, Category = DynamicEntryBox)
	void SetEntrySpacing(const FVector2D& InEntrySpacing);

	UFUNCTION(BlueprintCallable, Category = DynamicEntryBox)
	void SetRadialSettings(const FRadialBoxSettings& InSettings);
	
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void SynchronizeProperties() override;
	virtual void AddEntryChild(UUserWidget& ChildWidget);

	bool IsEntryClassValid(TSubclassOf<UUserWidget> InEntryClass) const;
	UUserWidget* CreateEntryInternal(TSubclassOf<UUserWidget> InEntryClass);
	void RemoveEntryInternal(UUserWidget* EntryWidget);
	FMargin BuildEntryPadding(const FVector2D& DesiredSpacing);

	/** Clear out the box entries, optionally deleting the underlying Slate widgets entirely as well. */
	void ResetInternal(bool bDeleteWidgets = false);

	/** Clear out the box entries, executing the provided reset function for each and optionally deleting the underlying Slate widgets entirely as well. */
	template <typename WidgetT = UUserWidget>
	void ResetInternal(TFunctionRef<void(WidgetT&)> ResetEntryFunc, bool bDeleteWidgets = false)
	{
		for (UUserWidget* EntryWidget : EntryWidgetPool.GetActiveWidgets())
		{
			if (WidgetT* TypedEntry = Cast<WidgetT>(EntryWidget))
			{
				ResetEntryFunc(*TypedEntry);
			}
		}

		ResetInternal(bDeleteWidgets);
	}

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	/** The type of box panel into which created entries are added. Some differences in functionality exist between each type. */
	UPROPERTY(EditAnywhere, Category = DynamicEntryBox, meta = (DesignerRebuild))
	EDynamicBoxType EntryBoxType;

	/** 
	 * The padding to apply between entries in the box.
	 * Note horizontal boxes only use the X and vertical boxes only use Y. Value is also ignored for the first entry in the box.
	 * Wrap and Overlay types use both X and Y for spacing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = EntryLayout)
	FVector2D EntrySpacing;

	//@todo DanH EntryBox: Consider giving a callback option as well/instead. Then this thing could actually create circular or pinwheel layouts...
	/** The looping sequence of entry paddings to apply as entries are created. Overlay boxes only. Ignores EntrySpacing if not empty. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = EntryLayout)
	TArray<FVector2D> SpacingPattern;

	/** Sizing rule to apply to generated entries. Horizontal/Vertical boxes only. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = EntryLayout)
	FSlateChildSize EntrySizeRule;

	/** Horizontal alignment of generated entries. Horizontal/Vertical/Wrap boxes only. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = EntryLayout)
	TEnumAsByte<EHorizontalAlignment> EntryHorizontalAlignment;

	/** Vertical alignment of generated entries. Horizontal/Vertical/Wrap boxes only. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = EntryLayout)
	TEnumAsByte<EVerticalAlignment> EntryVerticalAlignment;

	/** The maximum size of each entry in the dominant axis of the box. Vertical/Horizontal boxes only. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = EntryLayout)
	int32 MaxElementSize = 0;

	/** Settings only relevant to RadialBox */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = EntryLayout)
	FRadialBoxSettings RadialBoxSettings;

	// Can be a horizontal, vertical, wrap box, or overlay
	TSharedPtr<SPanel> MyPanelWidget;

private:
	UPROPERTY(Transient)
	FUserWidgetPool EntryWidgetPool;

	// Let the details customization manipulate us directly
	friend class FDynamicEntryBoxBaseDetails;
};