// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Widgets/SWidget.h"
#include "Components/ContentWidget.h"
#include "InvalidationBox.generated.h"

class SInvalidationPanel;

/**
 * Invalidate
 * * Single Child
 * * Caching / Performance
 */
UCLASS()
class UMG_API UInvalidationBox : public UContentWidget
{
	GENERATED_UCLASS_BODY()

public:

	/**
	 * 
	 */
	UE_DEPRECATED(4.27, "InvalidationCache is not used.")
	UFUNCTION(BlueprintCallable, Category="Invalidation Box", meta=(DeprecatedFunction))
	void InvalidateCache();

	/**
	 * @returns true when the invalidation box cache the widgets.
	 * The widgets will be updated only if they get invalidated.
	 */
	UFUNCTION(BlueprintCallable, Category="Invalidation Box")
	bool GetCanCache() const;

	/**
	 * Tell the InvalidationBox to use the invalidation process.
	 * @note the other internal flags can disable the option.
	 */
	UFUNCTION(BlueprintCallable, Category="Invalidation Box")
	void SetCanCache(bool CanCache);

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

protected:

	//~ Begin UPanelWidget interface
	virtual void OnSlotAdded(UPanelSlot* Slot) override;
	virtual void OnSlotRemoved(UPanelSlot* Slot) override;
	//~ End UPanelWidget interface

	//~ Begin UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	//~ End UWidget interface

protected:
	/**
	 * Should the invalidation panel cache the widgets?  Making this false makes it so the invalidation
	 * panel stops acting like an invalidation panel, just becomes a simple container widget.
	 */
	UPROPERTY(EditAnywhere, Category="Caching")
	bool bCanCache;

	/**
	 * Caches the locations for child draw elements relative to the invalidation box,
	 * this adds extra overhead to drawing them every frame.  However, in cases where
	 * the position of the invalidation boxes changes every frame this can be a big savings.
	 */
	UPROPERTY()
	bool CacheRelativeTransforms_DEPRECATED;

protected:
	TSharedPtr<class SInvalidationPanel> MyInvalidationPanel;
};
