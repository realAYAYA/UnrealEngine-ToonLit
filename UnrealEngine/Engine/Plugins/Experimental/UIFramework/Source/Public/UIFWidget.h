// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/Widget.h"
#include "MVVMViewModelBase.h"
#include "Types/UIFParentWidget.h"
#include "Types/UIFWidgetId.h"

#include "UIFWidget.generated.h"

template <typename ObjectType> class TNonNullPtr;

class FUIFrameworkModule;
class UUIFrameworkWidget;
struct FUIFrameworkWidgetTree;
class IUIFrameworkWidgetTreeOwner;

/**
 *
 */
UINTERFACE(MinimalAPI)
class UUIFrameworkWidgetWrapperInterface : public UInterface
{
	GENERATED_BODY()
};

class IUIFrameworkWidgetWrapperInterface
{
	GENERATED_BODY()

public:
	virtual void ReplaceWidget(UUIFrameworkWidget* OldWidget, UUIFrameworkWidget* NewWidget) {}
};

/**
 * 
 */
UCLASS(Abstract, BlueprintType)
class UIFRAMEWORK_API UUIFrameworkWidget : public UMVVMViewModelBase
{
	GENERATED_BODY()

	friend FUIFrameworkModule;
	friend FUIFrameworkWidgetTree;

private:
	UPROPERTY(BlueprintReadWrite, ReplicatedUsing = "OnRep_IsEnabled", Getter = "IsEnabled", Setter="SetEnabled", Category = "UI Framework", meta = (AllowPrivateAccess = "true"))
	bool bIsEnabled = true;

	UPROPERTY(BlueprintReadWrite, ReplicatedUsing = "OnRep_Visibility", Getter, Setter = "SetVisibility", Category = "UI Framework", meta = (AllowPrivateAccess = "true"))
	ESlateVisibility Visibility = ESlateVisibility::Visible;

public:
	//~ Begin UObject
	virtual bool IsSupportedForNetworking() const override
	{
		return true;
	}
	virtual int32 GetFunctionCallspace(UFunction* Function, FFrame* Stack) override;
	virtual bool CallRemoteFunction(UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack) override;
	//~ End UObject

	TScriptInterface<IUIFrameworkWidgetWrapperInterface> AuthorityGetWrapper() const
	{
		return Wrapper;
	}

	void AuthoritySetWrapper(TScriptInterface<IUIFrameworkWidgetWrapperInterface> InWrapper)
	{
		Wrapper = InWrapper;
	}

	FUIFrameworkWidgetId GetWidgetId() const
	{
		return Id;
	}

	IUIFrameworkWidgetTreeOwner* GetWidgetTreeOwner() const
	{
		return WidgetTreeOwner;
	}

	FUIFrameworkWidgetTree* GetWidgetTree() const;

	TSoftClassPtr<UWidget> GetUMGWidgetClass() const
	{
		return WidgetClass;
	}

	//~ Authority functions
	FUIFrameworkParentWidget AuthorityGetParent() const
	{
		return AuthorityParent;
	}

	virtual void AuthorityForEachChildren(const TFunctionRef<void(UUIFrameworkWidget*)>& Func)
	{
	}

	//~ Local functions
	virtual bool LocalIsReplicationReady() const
	{
		return true;
	}

	UWidget* LocalGetUMGWidget() const
	{
		return LocalUMGWidget;
	}

	void LocalCreateUMGWidget(TNonNullPtr<IUIFrameworkWidgetTreeOwner> Owner);
	virtual void LocalAddChild(FUIFrameworkWidgetId ChildId);
	void LocalDestroyUMGWidget();

	//~ Properties
	ESlateVisibility GetVisibility() const;
	void SetVisibility(ESlateVisibility InVisibility);

	bool IsEnabled() const;
	void SetEnabled(bool bEnabled);

protected:
	virtual void AuthorityRemoveChild(UUIFrameworkWidget* Widget)
	{
	}
	virtual void LocalOnUMGWidgetCreated()
	{
	}

	void ForceNetUpdate();

private:
	UFUNCTION()
	void OnRep_IsEnabled();
	UFUNCTION()
	void OnRep_Visibility();

protected:
	UPROPERTY(BlueprintReadOnly, Replicated, EditDefaultsOnly, Category = "UI Framework")
	TSoftClassPtr<UWidget> WidgetClass; // todo: make this private and use a constructor argument

private:
	//~ Authority and Local
	UPROPERTY(Replicated, Transient, DuplicateTransient)
	FUIFrameworkWidgetId Id = FUIFrameworkWidgetId::MakeNew();

	//~ Authority
	UPROPERTY(Transient)
	TScriptInterface<IUIFrameworkWidgetWrapperInterface> Wrapper;

	//~ Authority and Local
	IUIFrameworkWidgetTreeOwner* WidgetTreeOwner = nullptr;

	//~ AuthorityOnly
	UPROPERTY(Transient)
	FUIFrameworkParentWidget AuthorityParent;
	
	//~ LocalOnly
	UPROPERTY(Transient)
	TObjectPtr<UWidget> LocalUMGWidget;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Templates/NonNullPointer.h"
#endif
