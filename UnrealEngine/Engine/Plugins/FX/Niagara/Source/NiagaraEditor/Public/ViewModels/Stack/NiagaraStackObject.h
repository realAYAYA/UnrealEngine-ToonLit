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

UCLASS(MinimalAPI)
class UNiagaraStackObject : public UNiagaraStackItemContent, public FNotifyHook
{
	GENERATED_BODY()
		
public:
	DECLARE_DELEGATE_TwoParams(FOnSelectRootNodes, TArray<TSharedRef<IDetailTreeNode>>, TArray<TSharedRef<IDetailTreeNode>>*);

public:
	NIAGARAEDITOR_API UNiagaraStackObject();

	/** Initialized an entry for displaying a UObject in the niagara stack. 
		@param InRequiredEntryData Struct with required data for all stack entries.
		@param InObject The object to be displayed in the stack.
		@param bInIsTopLevelObject Whether or not the object being displayed should be treated as a top level object shown in the root of the stack.  This can be used to make layout decisions such as category styling.
		@param InOwnerStackItemEditorDataKey The stack editor data key of the owning stack entry. 
		@param InOwningNiagaraNode An optional niagara node which owns this object. */
	NIAGARAEDITOR_API void Initialize(FRequiredEntryData InRequiredEntryData, UObject* InObject, bool bInIsTopLevelObject, FString InOwnerStackItemEditorDataKey, UNiagaraNode* InOwningNiagaraNode = nullptr);

	NIAGARAEDITOR_API void SetOnSelectRootNodes(FOnSelectRootNodes OnSelectRootNodes);

	NIAGARAEDITOR_API void RegisterInstancedCustomPropertyLayout(UStruct* Class, FOnGetDetailCustomizationInstance DetailLayoutDelegate);
	NIAGARAEDITOR_API void RegisterInstancedCustomPropertyTypeLayout(FName PropertyTypeName, FOnGetPropertyTypeCustomizationInstance PropertyTypeLayoutDelegate, TSharedPtr<IPropertyTypeIdentifier> Identifier = nullptr);

	NIAGARAEDITOR_API UObject* GetObject();

	//~ FNotifyHook interface
	NIAGARAEDITOR_API virtual void NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged) override;

	//~ UNiagaraStackEntry interface
	NIAGARAEDITOR_API virtual bool GetIsEnabled() const override;
	NIAGARAEDITOR_API virtual bool GetShouldShowInStack() const override;
	NIAGARAEDITOR_API virtual UObject* GetDisplayedObject() const override;

	void SetObjectGuid(FGuid InGuid) { ObjectGuid = InGuid; }
	virtual bool SupportsSummaryView() const override { return ObjectGuid.IsSet() && ObjectGuid->IsValid(); }
	NIAGARAEDITOR_API virtual FNiagaraHierarchyIdentity DetermineSummaryIdentity() const override;
protected:
	NIAGARAEDITOR_API virtual void FinalizeInternal() override;

	NIAGARAEDITOR_API virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues) override;
	NIAGARAEDITOR_API virtual void PostRefreshChildrenInternal() override;

private:
	NIAGARAEDITOR_API void PropertyRowsRefreshed();
	NIAGARAEDITOR_API void OnMessageManagerRefresh(const TArray<TSharedRef<const INiagaraMessage>>& NewMessages);

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

	/** An optional object guid that can be provided to identify the object this stack entry represents. This can be used for summary view purposes and is also given to the child property rows. */
	TOptional<FGuid> ObjectGuid;
	
	FGuid MessageManagerRegistrationKey;
	TArray<FStackIssue> MessageManagerIssues;
	FGuid MessageLogGuid;
};
