// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ViewModels/Stack/NiagaraStackItem.h"
#include "Misc/NotifyHook.h"
#include "PropertyEditorDelegates.h"
#include "NiagaraStackObject.generated.h"

class IPropertyRowGenerator;
class UNiagaraNode;
class IDetailTreeNode;
class INiagaraMessage;

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackObject : public UNiagaraStackItemContent, public FNotifyHook
{
	GENERATED_BODY()
		
public:
	DECLARE_DELEGATE_TwoParams(FOnSelectRootNodes, TArray<TSharedRef<IDetailTreeNode>>, TArray<TSharedRef<IDetailTreeNode>>*);

public:
	UNiagaraStackObject();

	/** Initialized an entry for displaying a UObject in the niagara stack. 
		@param InRequiredEntryData Struct with required data for all stack entries.
		@param InObject The object to be displayed in the stack.
		@param bInIsTopLevelObject Whether or not the object being displayed should be treated as a top level object shown in the root of the stack.  This can be used to make layout decisions such as category styling.
		@param InOwnerStackItemEditorDataKey The stack editor data key of the owning stack entry. 
		@param InOwningNiagaraNode An optional niagara node which owns this object. */
	void Initialize(FRequiredEntryData InRequiredEntryData, UObject* InObject, bool bInIsTopLevelObject, FString InOwnerStackItemEditorDataKey, UNiagaraNode* InOwningNiagaraNode = nullptr);

	void SetOnSelectRootNodes(FOnSelectRootNodes OnSelectRootNodes);

	void RegisterInstancedCustomPropertyLayout(UStruct* Class, FOnGetDetailCustomizationInstance DetailLayoutDelegate);
	void RegisterInstancedCustomPropertyTypeLayout(FName PropertyTypeName, FOnGetPropertyTypeCustomizationInstance PropertyTypeLayoutDelegate, TSharedPtr<IPropertyTypeIdentifier> Identifier = nullptr);

	UObject* GetObject();

	//~ FNotifyHook interface
	virtual void NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged) override;

	//~ UNiagaraStackEntry interface
	virtual bool GetIsEnabled() const override;
	virtual bool GetShouldShowInStack() const override;
	virtual UObject* GetDisplayedObject() const override;

protected:
	virtual void FinalizeInternal() override;

	virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues) override;
	virtual void PostRefreshChildrenInternal() override;

private:
	void PropertyRowsRefreshed();
	void OnMessageManagerRefresh(const TArray<TSharedRef<const INiagaraMessage>>& NewMessages);

private:
	struct FRegisteredClassCustomization
	{
		UStruct* Class;
		FOnGetDetailCustomizationInstance DetailLayoutDelegate;
	};

	struct FRegisteredPropertyCustomization
	{
		FName PropertyTypeName;
		FOnGetPropertyTypeCustomizationInstance PropertyTypeLayoutDelegate;
		TSharedPtr<IPropertyTypeIdentifier> Identifier;
	};

	TWeakObjectPtr<UObject> WeakObject;

	// Whether or not the object being displayed should be treated as a top level object shown in the root of the stack.  This can be used to make layout decisions such as category styling.
	bool bIsTopLevelObject;

	UNiagaraNode* OwningNiagaraNode;
	FOnSelectRootNodes OnSelectRootNodesDelegate;
	TArray<FRegisteredClassCustomization> RegisteredClassCustomizations;
	TArray<FRegisteredPropertyCustomization> RegisteredPropertyCustomizations;
	TSharedPtr<IPropertyRowGenerator> PropertyRowGenerator;
	bool bIsRefresingDataInterfaceErrors;

	FGuid MessageManagerRegistrationKey;
	TArray<FStackIssue> MessageManagerIssues;
	FGuid MessageLogGuid;
};
