// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonBoundActionButtonInterface.h"
#include "CommonButtonBase.h"
#include "UIActionBindingHandle.h"
#include "CommonBoundActionButton.generated.h"

class UCommonTextBlock;

UCLASS(Abstract, meta = (DisableNativeTick))
class COMMONUI_API UCommonBoundActionButton : public UCommonButtonBase, public ICommonBoundActionButtonInterface
{
	GENERATED_BODY()

public:
	//~ Begin ICommonBoundActionButtonInterface
	virtual void SetRepresentedAction(FUIActionBindingHandle InBindingHandle) override;
	//~ End ICommonBoundActionButtonInterface
	
protected:
	virtual void NativeOnClicked() override;
	virtual void NativeOnCurrentTextStyleChanged() override;

	virtual void UpdateInputActionWidget() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Common Bound Action")
	void OnUpdateInputAction();

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Text Block")
	TObjectPtr<UCommonTextBlock> Text_ActionName;

private:
	FUIActionBindingHandle BindingHandle;
};