// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/GameInstanceSubsystem.h"
#include "Layout/Margin.h"
#include "Widgets/Layout/Anchors.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "GameViewportSubsystem.generated.h"

class ULevel;
class ULocalPlayer;
class UWidget;
class UWorld;
class SConstraintCanvas;

/**
 * The default value fills the entire screen / player region.
 */
USTRUCT(BlueprintType)
struct UMG_API FGameViewportWidgetSlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "User Interface")
	FAnchors Anchors = FAnchors(0.f, 0.f, 1.f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "User Interface")
	FMargin Offsets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "User Interface")
	FVector2D Alignment = FVector2D(0.f, 0.f);

	/** The higher the number, the more on top this widget will be. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "User Interface")
	int32 ZOrder = 0;
};


/**
 * 
 */
UCLASS()
class UMG_API UGameViewportSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin Subsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End Subsystem

	static UGameViewportSubsystem* Get(UWorld* World);
	
public:
	/* @return true if the widget was added to the viewport using AddWidget or AddWidgetForPlayer. */
	UFUNCTION(BlueprintPure, BlueprintCosmetic, Category = "User Interface")
	bool IsWidgetAdded(const UWidget* Widget) const;

	/** Adds it to the game's viewport. */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "User Interface")
	void AddWidget(UWidget* Widget, FGameViewportWidgetSlot Slot);

	/**
	 * Adds the widget to the game's viewport in the section dedicated to the player. This is valuable in a split screen
	 * game where you need to only show a widget over a player's portion of the viewport.
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "User Interface")
	void AddWidgetForPlayer(UWidget* Widget, ULocalPlayer* Player, FGameViewportWidgetSlot Slot);

	/** Removes the widget from the viewport. */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "User Interface")
	void RemoveWidget(UWidget* Widget);

	/* The slot info from previously added widget or info that is store for later. */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "User Interface")
	FGameViewportWidgetSlot GetWidgetSlot(const UWidget* Widget) const;

	/* Update the slot info of a previously added widget or Store the slot info for later use. */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "User Interface")
	void SetWidgetSlot(UWidget* Widget, FGameViewportWidgetSlot Slot);

public:
	/**
	 * Helper function to set the position in the viewport for the Slot.
	 * @param Position The 2D position to set the widget to in the viewport.
	 * @param bRemoveDPIScale If you've already calculated inverse DPI, set this to false.
	 * Otherwise inverse DPI is applied to the position so that when the location is scaled
	 * by DPI, it ends up in the expected position.
	 */
	UFUNCTION(BlueprintCallable, Category = "GameViewportWidgetSlot")
	static FGameViewportWidgetSlot SetWidgetSlotPosition(FGameViewportWidgetSlot Slot, const UWidget* Widget, FVector2D Position, bool bRemoveDPIScale);

	/**
	 * Helper function to set the desired size in the viewport for the Slot.
	 */
	UFUNCTION(BlueprintCallable, Category = "GameViewportWidgetSlot")
	static FGameViewportWidgetSlot SetWidgetSlotDesiredSize(FGameViewportWidgetSlot Slot, FVector2D Size);

private:
	struct FSlotInfo
	{
		FGameViewportWidgetSlot Slot;
		TWeakObjectPtr<ULocalPlayer> LocalPlayer;
		TWeakPtr<SConstraintCanvas> FullScreenWidget;
		SConstraintCanvas::FSlot* FullScreenWidgetSlot = nullptr;
	};

private:
	void AddToScreen(UWidget* Widget, ULocalPlayer* Player, FGameViewportWidgetSlot& Slot);
	void RemoveWidgetInternal(UWidget* Widget, const TWeakPtr<SConstraintCanvas>& FullScreenWidget, const TWeakObjectPtr<ULocalPlayer>& LocalPlayer);
	void OnLevelRemovedFromWorld(ULevel* InLevel, UWorld* InWorld);
	FMargin GetFullScreenOffsetForWidget(UWidget* Widget) const;
	TPair<FMargin, bool> GetOffsetAttribute(UWidget* Widget, const FGameViewportWidgetSlot& Slot) const;

private:
	using FViewportWidgetList = TMap<TWeakObjectPtr<UWidget>, FSlotInfo>;
	FViewportWidgetList ViewportWidgets;
};
