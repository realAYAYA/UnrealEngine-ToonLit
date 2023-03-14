// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonUITypes.h"
#include "Input/UIActionBindingHandle.h"

#include "CommonUserWidget.generated.h"

class UCommonInputSubsystem;
class UCommonUISubsystemBase;
class FSlateUser;

struct FUIActionTag;
struct FBindUIActionArgs;
enum class ECommonInputMode : uint8;

UCLASS(ClassGroup = UI, meta = (Category = "Common UI", DisableNativeTick))
class COMMONUI_API UCommonUserWidget : public UUserWidget
{
	GENERATED_UCLASS_BODY()

public:
	/** Sets whether or not this widget will consume ALL pointer input that reaches it */
	UFUNCTION(BlueprintCallable, Category = CommonUserWidget)
	void SetConsumePointerInput(bool bInConsumePointerInput);

	const TArray<FUIActionBindingHandle>& GetActionBindings() const { return ActionBindings; }
	const TArray<TWeakObjectPtr<const UWidget>> GetScrollRecipients() const { return ScrollRecipients; }

protected:
	virtual void OnWidgetRebuilt() override;
	virtual void NativeDestruct() override;
	
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnTouchGesture(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	
	UCommonInputSubsystem* GetInputSubsystem() const;
	UCommonUISubsystemBase* GetUISubsystem() const;
	TSharedPtr<FSlateUser> GetOwnerSlateUser() const;

	template <typename GameInstanceT = UGameInstance>
	GameInstanceT& GetGameInstanceChecked() const
	{
		GameInstanceT* GameInstance = GetGameInstance<GameInstanceT>();
		check(GameInstance);
		return *GameInstance;
	}

	template <typename PlayerControllerT = APlayerController>
	PlayerControllerT& GetOwningPlayerChecked() const
	{
		PlayerControllerT* PC = GetOwningPlayer<PlayerControllerT>();
		check(PC);
		return *PC;
	}

	void RegisterScrollRecipient(const UWidget& AnalogScrollRecipient);
	void UnregisterScrollRecipient(const UWidget& AnalogScrollRecipient);

	/** 
	 * Convenience methods for menu action registrations (any UWidget can register via FCommonUIActionRouter directly, though generally that shouldn't be needed).
	 * Persistent bindings are *always* listening for input while registered, while normal bindings are only listening when all of this widget's activatable parents are activated.
	 */
	FUIActionBindingHandle RegisterUIActionBinding(const FBindUIActionArgs& BindActionArgs);

	void RemoveActionBinding(FUIActionBindingHandle ActionBinding);
	void AddActionBinding(FUIActionBindingHandle ActionBinding);

	/** True to generally display this widget's actions in the action bar, assuming it has actions. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = true))
	bool bDisplayInActionBar = false;

private:

	/** Set this to true if you don't want any pointer (mouse and touch) input to bubble past this widget */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = true))
	bool bConsumePointerInput = false;

private:

	TArray<FUIActionBindingHandle> ActionBindings;
	TArray<TWeakObjectPtr<const UWidget>> ScrollRecipients;
};