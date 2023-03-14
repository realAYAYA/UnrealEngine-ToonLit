// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/Widget.h"
#include "Types/UIFParentWidget.h"
#include "Types/UIFWidgetId.h"
#include "UObject/SoftObjectPtr.h"

#include "UIFWidget.generated.h"


/**
 * 
 */
UCLASS(Abstract, BlueprintType)
class UIFRAMEWORK_API UUIFrameworkWidget : public UObject
{
	GENERATED_BODY()

public:
	//~ Begin UObject
	virtual bool IsSupportedForNetworking() const override
	{
		return true;
	}
	virtual int32 GetFunctionCallspace(UFunction* Function, FFrame* Stack) override;
	virtual bool CallRemoteFunction(UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack) override;
	//~ End UObject

	FUIFrameworkWidgetId GetWidgetId() const
	{
		return Id;
	}

	UUIFrameworkPlayerComponent* GetPlayerComponent() const
	{
		return OwnerPlayerComponent;
	}

	TSoftClassPtr<UWidget> GetUMGWidgetClass() const
	{
		return WidgetClass;
	}

	//~ Authority functions
	void AuthoritySetParent(UUIFrameworkPlayerComponent* Owner, FUIFrameworkParentWidget NewParent);

	FUIFrameworkParentWidget AuthorityGetParent() const
	{
		return AuthorityParent;
	}

	virtual void AuthorityForEachChildren(const TFunctionRef<void(UUIFrameworkWidget*)>& Func) {}

	//~ Local functions
	UWidget* LocalGetUMGWidget() const
	{
		return LocalUMGWidget;
	}

	void LocalCreateUMGWidget(UUIFrameworkPlayerComponent* Owner);
	virtual void LocalAddChild(FUIFrameworkWidgetId ChildId);
	void LocalDestroyUMGWidget();

protected:
	virtual void AuthorityRemoveChild(UUIFrameworkWidget* Widget) {}
	virtual void LocalOnUMGWidgetCreated() { }

private:
	void SetParentPlayerOwnerRecursive();

protected:
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "UI Framework")
	TSoftClassPtr<UWidget> WidgetClass; // todo: make this private and use a constructor argument

private:
	//~ Authority and Local
	UPROPERTY(Replicated, Transient, DuplicateTransient)
	FUIFrameworkWidgetId Id = FUIFrameworkWidgetId::MakeNew();

	//~ Authority and Local
	UPROPERTY(Transient)
	TObjectPtr<UUIFrameworkPlayerComponent> OwnerPlayerComponent = nullptr;

	//~ AuthorityOnly
	UPROPERTY(Transient)
	FUIFrameworkParentWidget AuthorityParent;
	
	//~ LocalOnly
	UPROPERTY(Transient)
	TObjectPtr<UWidget> LocalUMGWidget;

	//UPROPERTY(BlueprintReadWrite)
	//EUIEnability Enablility;
	//
	//UPROPERTY(BlueprintReadWrite)
	//EUIVisibility Visibility;
};
