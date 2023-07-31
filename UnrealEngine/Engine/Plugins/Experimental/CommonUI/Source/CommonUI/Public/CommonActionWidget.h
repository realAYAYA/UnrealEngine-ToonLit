// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonUITypes.h"
#include "Input/UIActionBindingHandle.h"

#include "CommonActionWidget.generated.h"

class SBox;
class SImage;
class UMaterialInstanceDynamic;

/**
 * A widget that shows a platform-specific icon for the given input action.
 */
UCLASS(BlueprintType, Blueprintable)
class COMMONUI_API UCommonActionWidget: public UWidget
{
	GENERATED_UCLASS_BODY()

public:

	//UObject interface
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface
	
	/** Begin UWidget */
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void SynchronizeProperties() override;
	/** End UWidet */
	
	UFUNCTION(BlueprintCallable, Category = CommonActionWidget)
	virtual FSlateBrush GetIcon() const;

	UFUNCTION(BlueprintCallable, Category = CommonActionWidget)
	FText GetDisplayText() const;

	UFUNCTION(BlueprintCallable, Category = CommonActionWidget)
	void SetInputAction(FDataTableRowHandle InputActionRow);

	void SetInputActionBinding(FUIActionBindingHandle BindingHandle);

	UFUNCTION(BlueprintCallable, Category = CommonActionWidget)
	void SetInputActions(TArray<FDataTableRowHandle> NewInputActions);

	UFUNCTION(BlueprintCallable, Category = CommonActionWidget)
	void SetIconRimBrush(FSlateBrush InIconRimBrush);

	UFUNCTION(BlueprintCallable, Category = CommonActionWidget)
	bool IsHeldAction() const;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInputMethodChanged, bool, bUsingGamepad);
	UPROPERTY(BlueprintAssignable, Category = CommonActionWidget)
	FOnInputMethodChanged OnInputMethodChanged;

	/**
	 * The material to use when showing held progress, the progress will be sent using the material parameter
	 * defined by ProgressMaterialParam and the value will range from 0..1.
	 **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CommonActionWidget)
	FSlateBrush ProgressMaterialBrush;

	/** The material parameter on ProgressMaterialBrush to update the held percentage.  This value will be 0..1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = CommonActionWidget)
	FName ProgressMaterialParam;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = CommonActionWidget)
	FSlateBrush IconRimBrush;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

	void OnActionProgress(float HeldPercent);
	void OnActionComplete();
	void SetProgressMaterial(const FSlateBrush& InProgressMaterialBrush, const FName& InProgressMaterialParam);
	void SetHidden(bool bAlwaysHidden);

protected:
	/**
	 * List all the input actions that this common action widget is intended to represent.  In some cases you might have multiple actions
	 * that you need to represent as a single entry in the UI.  For example - zoom, might be mouse wheel up or down, but you just need to
	 * show a single icon for Up & Down on the mouse, this solves that problem.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = CommonActionWidget, meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase", TitleProperty = "RowName"))
	TArray<FDataTableRowHandle> InputActions;

	//@todo DanH: Create clearer split between support for the new & legacy system in here
	FUIActionBindingHandle DisplayedBindingHandle;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FDataTableRowHandle InputActionDataRow_DEPRECATED;
#endif

protected:
	UCommonInputSubsystem* GetInputSubsystem() const;
	const FCommonInputActionDataBase* GetInputActionData() const;

	void UpdateActionWidget();

	virtual void OnWidgetRebuilt() override;
	
	void UpdateBindingHandleInternal(FUIActionBindingHandle BindingHandle);

	void ListenToInputMethodChanged(bool bListen = true);
	void HandleInputMethodChanged(ECommonInputType InInputType);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ProgressDynamicMaterial;
	
	TSharedPtr<SBox> MyKeyBox;

	TSharedPtr<SImage> MyIcon;

	TSharedPtr<SImage> MyProgressImage;

	TSharedPtr<SImage> MyIconRim;

	FSlateBrush Icon;

	bool bAlwaysHideOverride = false;
};