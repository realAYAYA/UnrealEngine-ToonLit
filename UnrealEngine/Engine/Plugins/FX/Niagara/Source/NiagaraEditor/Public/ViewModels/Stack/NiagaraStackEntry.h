// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DragAndDrop/DecoratedDragDropOp.h"
#include "NiagaraEditorCommon.h"
#include "NiagaraValidationRule.h"
#include "Widgets/Views/STableRow.h"
#include "NiagaraStackEntry.generated.h"

class FNiagaraSystemViewModel;
class FNiagaraEmitterViewModel;
class FNiagaraScriptViewModel;
class UNiagaraStackEditorData;
class UNiagaraStackErrorItem;
class UNiagaraClipboardContent;
class UNiagaraStackEntry;

UENUM()
enum class EStackIssueSeverity : uint8
{
	Error = 0,
	Warning, 
	Info,
	CustomNote,
	None
};

class FNiagaraStackEntryDragDropOp : public FDecoratedDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FNiagaraStackEntryDragDropOp, FDecoratedDragDropOp)

public:
	FNiagaraStackEntryDragDropOp(TArray<UNiagaraStackEntry*> InDraggedEntries)
	{
		DraggedEntries = InDraggedEntries;
	}

	const TArray<UNiagaraStackEntry*> GetDraggedEntries() const
	{
		return DraggedEntries;
	}

private:
	TArray<UNiagaraStackEntry*> DraggedEntries;
};

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackEntry : public UObject
{
	GENERATED_BODY()

public:
	enum class EDragOptions
	{
		Copy,
		None
	};

	enum class EDropOptions
	{
		Overview,
		None
	};

	enum class EIconMode
	{
		Brush,
		Text,
		None
	};

	struct FDropRequest
	{
		FDropRequest(TSharedRef<const FDragDropOperation> InDragDropOperation, EItemDropZone InDropZone, EDragOptions InDragOptions, EDropOptions InDropOptions)
			: DragDropOperation(InDragDropOperation)
			, DropZone(InDropZone)
			, DragOptions(InDragOptions)
			, DropOptions(InDropOptions)
		{
		}

		const TSharedRef<const FDragDropOperation> DragDropOperation;
		const EItemDropZone DropZone;
		const EDragOptions DragOptions;
		const EDropOptions DropOptions;
	};

	struct FDropRequestResponse
	{
		FDropRequestResponse(TOptional<EItemDropZone> InDropZone, FText InDropMessage = FText())
			: DropZone(InDropZone)
			, DropMessage(InDropMessage)
		{
		}

		const TOptional<EItemDropZone> DropZone;
		const FText DropMessage;
	};

	DECLARE_MULTICAST_DELEGATE(FOnExpansionChanged);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnStructureChanged, ENiagaraStructureChangedFlags);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnDataObjectModified, TArray<UObject*>, ENiagaraDataObjectChange);
	DECLARE_MULTICAST_DELEGATE(FOnRequestFullRefresh);
	DECLARE_MULTICAST_DELEGATE(FOnRequestFullRefreshDeferred);
	DECLARE_DELEGATE_RetVal_TwoParams(TOptional<FDropRequestResponse>, FOnRequestDrop, const UNiagaraStackEntry& /*TargetEntry*/, const FDropRequest& /*DropRequest*/);
	DECLARE_DELEGATE_RetVal_OneParam(bool, FOnFilterChild, const UNiagaraStackEntry&);
	DECLARE_DELEGATE(FStackIssueFixDelegate);
	DECLARE_MULTICAST_DELEGATE(FOnAlternateDisplayNameChanged);

public:
	struct NIAGARAEDITOR_API FExecutionCategoryNames
	{
		static const FName System;
		static const FName Emitter;
		static const FName Particle;
		static const FName Render;
	};

	struct NIAGARAEDITOR_API FExecutionSubcategoryNames
	{
		static const FName Settings;
		static const FName Spawn;
		static const FName Update;
		static const FName Event;
		static const FName SimulationStage;
		static const FName Render;
	};

	enum class EStackRowStyle
	{
		None,
		GroupHeader,
		GroupFooter,
		ItemHeader,
		ItemContent,
		ItemContentAdvanced,
		ItemContentNote,
		ItemFooter,
		ItemCategory,
		ItemSubCategory,
		StackIssue,
		Spacer
	};

	struct FRequiredEntryData
	{
		FRequiredEntryData(TSharedRef<FNiagaraSystemViewModel> InSystemViewModel, TSharedPtr<FNiagaraEmitterViewModel> InEmitterViewModel, FName InExecutionCategoryName, FName InExecutionSubcategoryName, UNiagaraStackEditorData& InStackEditorData)
			: SystemViewModel(InSystemViewModel)
			, EmitterViewModel(InEmitterViewModel)
			, ExecutionCategoryName(InExecutionCategoryName)
			, ExecutionSubcategoryName(InExecutionSubcategoryName)
			, StackEditorData(&InStackEditorData)
		{
		}

		const TSharedRef<FNiagaraSystemViewModel> SystemViewModel;
		const TSharedPtr<FNiagaraEmitterViewModel> EmitterViewModel;
		const FName ExecutionCategoryName;
		const FName ExecutionSubcategoryName;
		UNiagaraStackEditorData* const StackEditorData;
	};

	struct FStackSearchItem
	{
		FName Key;
		FText Value;
		
		inline bool operator==(FStackSearchItem Item) {
			return (Item.Key == Key 
				&& Item.Value.ToString() == Value.ToString());
		}
	};

	// stack issue stuff

	enum class EStackIssueFixStyle
	{
		Fix,
		Link
	};

	struct NIAGARAEDITOR_API FStackIssueFix
	{
		FStackIssueFix();

		FStackIssueFix(FText InDescription, FStackIssueFixDelegate InFixDelegate, EStackIssueFixStyle FixStyle = EStackIssueFixStyle::Fix);

		bool IsValid() const;

		const FText& GetDescription() const;

		void SetFixDelegate(const FStackIssueFixDelegate& InFixDelegate);

		const FStackIssueFixDelegate& GetFixDelegate() const;

		EStackIssueFixStyle GetStyle() const;

		const FString& GetUniqueIdentifier() const;

	private:
		FText Description;
		FStackIssueFixDelegate FixDelegate;
		EStackIssueFixStyle Style;
		FString UniqueIdentifier;
	};

	struct NIAGARAEDITOR_API FStackIssue
	{
		FStackIssue();

		FStackIssue(EStackIssueSeverity InSeverity, FText InShortDescription, FText InLongDescription, FString InStackEditorDataKey, bool bInCanBeDismissed, const TArray<FStackIssueFix>& InFixes);

		FStackIssue(EStackIssueSeverity InSeverity, FText InShortDescription, FText InLongDescription, FString InStackEditorDataKey, bool bInCanBeDismissed, FStackIssueFix InFix);

		FStackIssue(EStackIssueSeverity InSeverity, FText InShortDescription, FText InLongDescription, FString InStackEditorDataKey, bool bInCanBeDismissed);

		bool IsValid();

		EStackIssueSeverity GetSeverity() const;

		const FText& GetShortDescription() const;

		const FText& GetLongDescription() const;

		const FString& GetUniqueIdentifier() const;

		bool GetCanBeDismissed() const;

		const TArray<FStackIssueFix>& GetFixes() const;

		bool GetIsExpandedByDefault() const;

		void SetIsExpandedByDefault(bool InExpanded);
		
		void InsertFix(int32 InsertionIdx, const FStackIssueFix& Fix);

	private:
		EStackIssueSeverity Severity;
		FText ShortDescription;
		FText LongDescription;
		FString UniqueIdentifier;
		bool bCanBeDismissed = false;
		bool bIsExpandedByDefault = true;
		TArray<FStackIssueFix> Fixes;
	};

public:
	UNiagaraStackEntry();

	void Initialize(FRequiredEntryData InRequiredEntryData, FString InStackEditorDataKey);

	void Finalize();

	bool IsFinalized() const;

	virtual FText GetDisplayName() const;

	TOptional<FText> GetAlternateDisplayName() const;

	virtual UObject* GetDisplayedObject() const;

	virtual FGuid GetSelectionId() const;

	UNiagaraStackEditorData& GetStackEditorData() const;

	FString GetStackEditorDataKey() const;

	virtual FText GetTooltipText() const;

	virtual bool GetCanExpand() const;

	virtual bool GetCanExpandInOverview() const;

	virtual bool IsExpandedByDefault() const;

	bool GetIsExpanded() const;

	void SetIsExpanded(bool bInExpanded);

	void SetIsExpanded_Recursive(bool bInExpanded);

	bool GetIsExpandedInOverview() const;

	void SetIsExpandedInOverview(bool bInExpanded);

	virtual bool GetIsEnabled() const;

	bool GetOwnerIsEnabled() const;

	bool GetIsEnabledAndOwnerIsEnabled() const { return GetIsEnabled() && GetOwnerIsEnabled(); }

	FName GetExecutionCategoryName() const;

	FName GetExecutionSubcategoryName() const;

	virtual EStackRowStyle GetStackRowStyle() const;

	int32 GetIndentLevel() const;

	/** Returns whether or not this entry should be treated as a child of a previous sibling for layout purposes. */
	virtual bool IsSemanticChild() const { return false; }

	virtual bool GetShouldShowInStack() const;

	virtual bool GetShouldShowInOverview() const;

	void GetFilteredChildren(TArray<UNiagaraStackEntry*>& OutFilteredChildren) const;
	
	void GetUnfilteredChildren(TArray<UNiagaraStackEntry*>& OutUnfilteredChildren) const;

	void GetCustomFilteredChildren(TArray<UNiagaraStackEntry*>& OutFilteredChildren, const TArray<FOnFilterChild>& ChildFilters) const;

	void GetFilteredChildrenOfTypes(TArray<UNiagaraStackEntry*>& OutFilteredChildren, const TSet<UClass*>& AllowedClasses) const
	{
		TArray<UNiagaraStackEntry*> FilteredChildrenTmp;
		GetFilteredChildren(FilteredChildrenTmp);
		for (UNiagaraStackEntry* FilteredChild : FilteredChildrenTmp)
		{
			for(const UClass* Class : AllowedClasses)
			{
				if(FilteredChild->IsA(Class))
				{
					OutFilteredChildren.Add(FilteredChild);
					break;
				}
			}
			UClass* ChildClass = FilteredChild->GetClass();

			if(AllowedClasses.Contains(ChildClass))
			{
				OutFilteredChildren.Add(FilteredChild);	
			}
		}
	}
	
	template<typename T>
	void GetUnfilteredChildrenOfType(TArray<T*>& OutUnfilteredChildrenOfType) const
	{
		TArray<UNiagaraStackEntry*> UnfilteredChildren;
		GetUnfilteredChildren(UnfilteredChildren);
		for (UNiagaraStackEntry* UnfilteredChild : UnfilteredChildren)
		{
			T* UnfilteredChildOfType = Cast<T>(UnfilteredChild);
			if (UnfilteredChildOfType != nullptr)
			{
				OutUnfilteredChildrenOfType.Add(UnfilteredChildOfType);
			}
		}
	}

	template<typename T>
	void GetFilteredChildrenOfType(TArray<T*>& OutFilteredChildrenOfType) const
	{
		TArray<UNiagaraStackEntry*> OutFilteredChildren;
		GetFilteredChildren(OutFilteredChildren);
		for (UNiagaraStackEntry* UnfilteredChild : OutFilteredChildren)
		{
			T* UnfilteredChildOfType = Cast<T>(UnfilteredChild);
			if (UnfilteredChildOfType != nullptr)
			{
				OutFilteredChildrenOfType.Add(UnfilteredChildOfType);
			}
		}
	}

	template<typename T>
	void GetCustomFilteredChildrenOfType(TArray<T*>& OutFilteredChildrenOfType, const TArray<FOnFilterChild>& CustomChildFilters) const
	{
		for (UNiagaraStackEntry* Child : Children)
		{
			if(T* CastChild = Cast<T>(Child))
			{
				bool bPassesFilter = true;
				for (const FOnFilterChild& ChildFilter : CustomChildFilters)
				{
					if (ChildFilter.Execute(*CastChild) == false)
					{
						bPassesFilter = false;
						break;
					}
				}

				if (bPassesFilter)
				{
					OutFilteredChildrenOfType.Add(CastChild);
				}				
			}
		}
	}

	FOnExpansionChanged& OnExpansionChanged();

	FOnExpansionChanged& OnExpansionInOverviewChanged();

	FOnStructureChanged& OnStructureChanged();

	FOnDataObjectModified& OnDataObjectModified();

	FOnRequestFullRefresh& OnRequestFullRefresh();

	const FOnRequestFullRefresh& OnRequestFullRefreshDeferred() const;

	FOnRequestFullRefresh& OnRequestFullRefreshDeferred();

	FOnAlternateDisplayNameChanged& OnAlternateDisplayNameChanged();

	/** Recursively refreshes the children for the current stack entry.  This may cause children to be added or removed and will automatically cause the filtered 
	children to be refreshed. This will also cause the structure changed delegate to be broadcast. */
	void RefreshChildren();

	/** Invalidates the cached filtered children so that the filters will be run the next time that GetFilteredChildren is called.  This should be called any time
	a change to the data is made which will affect how children are filtered.  This will also cause the structure changed delegate to be broadcast. */
	void RefreshFilteredChildren();

	FDelegateHandle AddChildFilter(FOnFilterChild ChildFilter);
	void RemoveChildFilter(FDelegateHandle FilterHandle);

	TSharedRef<FNiagaraSystemViewModel> GetSystemViewModel() const;
	TSharedPtr<FNiagaraEmitterViewModel> GetEmitterViewModel() const;

	template<typename ChildType, typename PredicateType>
	static ChildType* FindCurrentChildOfTypeByPredicate(const TArray<UNiagaraStackEntry*>& CurrentChildren, PredicateType Predicate)
	{
		for (UNiagaraStackEntry* CurrentChild : CurrentChildren)
		{
			ChildType* TypedCurrentChild = Cast<ChildType>(CurrentChild);
			if (TypedCurrentChild != nullptr && Predicate(TypedCurrentChild))
			{
				return TypedCurrentChild;
			}
		}
		return nullptr;
	}

	virtual void GetSearchItems(TArray<FStackSearchItem>& SearchItems) const;

	virtual UObject* GetExternalAsset() const;

	virtual bool CanDrag() const;

	TOptional<FDropRequestResponse> CanDrop(const FDropRequest& DropRequest);

	TOptional<FDropRequestResponse> Drop(const FDropRequest& DropRequest);

	void SetOnRequestCanDrop(FOnRequestDrop InOnRequestCanDrop);

	void SetOnRequestDrop(FOnRequestDrop InOnRequestCanDrop);

	const bool GetIsSearchResult() const;

	void SetIsSearchResult(bool bInIsSearchResult);

	bool HasBaseEmitter() const;

	bool HasIssuesOrAnyChildHasIssues() const;
	bool HasUsagesOrAnyChildHasUsages() const;
	void GetRecursiveUsages(bool& bRead, bool& bWrite) const;

	int32 GetTotalNumberOfCustomNotes() const;
	
	int32 GetTotalNumberOfInfoIssues() const;

	int32 GetTotalNumberOfWarningIssues() const;

	int32 GetTotalNumberOfErrorIssues() const;

	virtual EStackIssueSeverity GetIssueSeverity() const { return EStackIssueSeverity::None; }

	const TArray<FStackIssue>& GetIssues() const;

	const TArray<UNiagaraStackEntry*>& GetAllChildrenWithIssues() const;
	
	void AddValidationIssue(EStackIssueSeverity Severity, const FText& SummaryText, const FText& Description, bool bCanBeDismissed, const TArray<FNiagaraValidationFix>& Fixes, const TArray<FNiagaraValidationFix>& Links);
	void AddExternalIssue(EStackIssueSeverity Severity, const FText& SummaryText, const FText& Description, bool bCanBeDismissed);
	void ClearExternalIssues();

	virtual bool SupportsCut() const { return false; }

	virtual bool TestCanCutWithMessage(FText& OutMessage) const { return false; }

	virtual FText GetCutTransactionText() const { return FText(); }

	virtual void CopyForCut(UNiagaraClipboardContent* ClipboardContent) const { }
	virtual void RemoveForCut() { }

	virtual bool SupportsCopy() const { return false; }

	virtual bool TestCanCopyWithMessage(FText& OutMessage) const { return false; }

	virtual void Copy(UNiagaraClipboardContent* ClipboardContent) const { }

	virtual bool SupportsPaste() const { return false; }

	virtual bool TestCanPasteWithMessage(const UNiagaraClipboardContent* ClipboardContent, FText& OutMessage) const { return false; }

	virtual FText GetPasteTransactionText(const UNiagaraClipboardContent* ClipboardContent) const { return FText(); }

	virtual void Paste(const UNiagaraClipboardContent* ClipboardContent, FText& OutPasteWarning) { }

	virtual bool SupportsDelete() const { return false; }

	virtual bool TestCanDeleteWithMessage(FText& OutCanDeleteMessage) const { return false; }

	virtual FText GetDeleteTransactionText() const { return FText(); }

	virtual void Delete() { }

	/** Returns whether or not this entry can be renamed. */
	virtual bool SupportsRename() const { return false; }

	/** Gets whether this entry has a rename pending. */
	virtual bool GetIsRenamePending() const;

	/** Sets whether this entry has a rename pending. */
	virtual void SetIsRenamePending(bool bIsRenamePending);

	/** Handler for when a rename is committed for this stack entry. */
	virtual void OnRenamed(FText NewName);

	virtual EIconMode GetSupportedIconMode() const { return EIconMode::None; }

	virtual const FSlateBrush* GetIconBrush() const { return nullptr; }

	virtual FText GetIconText() const { return FText(); }

	virtual bool SupportsInheritance() const { return false; }

	virtual bool GetIsInherited() const { return false; }

	virtual FText GetInheritanceMessage() const { return FText(); }

	virtual void InvalidateCollectedUsage();


	struct FCollectedUsageData
	{
	public:
		bool bHasReferencedParameterRead = false;
		bool bHasReferencedParameterWrite = false;
	};
	virtual const FCollectedUsageData& GetCollectedUsageData() const;

protected:
	virtual void BeginDestroy() override;

	virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues);

	virtual void PostRefreshChildrenInternal();

	FRequiredEntryData CreateDefaultChildRequiredData() const;

	virtual int32 GetChildIndentLevel() const;

	virtual TOptional<FDropRequestResponse> CanDropInternal(const FDropRequest& DropRequest);

	virtual TOptional<FDropRequestResponse> DropInternal(const FDropRequest& DropRequest);

	virtual TOptional<FDropRequestResponse> ChildRequestCanDropInternal(const UNiagaraStackEntry& TargetChild, const FDropRequest& DropRequest);

	virtual TOptional<FDropRequestResponse> ChildRequestDropInternal(const UNiagaraStackEntry& TargetChild, const FDropRequest& DropRequest);

	virtual void ChildStructureChangedInternal();

	virtual void FinalizeInternal();


	mutable TOptional<FCollectedUsageData> CachedCollectedUsageData;

	bool IsSystemViewModelValid() const { return SystemViewModel.IsValid(); }


private:
	void ChildStructureChanged(ENiagaraStructureChangedFlags Info);

	void ChildExpansionChanged();

	void ChildExpansionInOverviewChanged();
	
	void ChildDataObjectModified(TArray<UObject*> ChangedObjects, ENiagaraDataObjectChange ChangeType);

	void ChildRequestFullRefresh();

	void ChildRequestFullRefreshDeferred();

	TOptional<FDropRequestResponse> ChildRequestCanDrop(const UNiagaraStackEntry& TargetChild, const FDropRequest& DropRequest);

	TOptional<FDropRequestResponse> ChildRequestDrop(const UNiagaraStackEntry& TargetChild, const FDropRequest& DropRequest);

	void RefreshStackErrorChildren();

	void IssueModified();

	void InvalidateFilteredChildren();

	struct FCollectedIssueData
	{
		FCollectedIssueData()
			: TotalNumberOfInfoIssues(0)
			, TotalNumberOfWarningIssues(0)
			, TotalNumberOfErrorIssues(0)
			, TotalNumberOfCustomNotes(0)
		{
		}

		bool HasAnyIssues() const { return TotalNumberOfInfoIssues > 0 || TotalNumberOfWarningIssues > 0 || TotalNumberOfErrorIssues > 0 || TotalNumberOfCustomNotes > 0; }

		int32 TotalNumberOfInfoIssues;
		int32 TotalNumberOfWarningIssues;
		int32 TotalNumberOfErrorIssues;
		int32 TotalNumberOfCustomNotes;
		TArray<UNiagaraStackEntry*> ChildrenWithIssues;
	};


	const FCollectedIssueData& GetCollectedIssueData() const;

	TWeakPtr<FNiagaraSystemViewModel> SystemViewModel;
	TWeakPtr<FNiagaraEmitterViewModel> EmitterViewModel;

	UPROPERTY()
	TObjectPtr<UNiagaraStackEditorData> StackEditorData;

	FString StackEditorDataKey;

	FOnExpansionChanged ExpansionChangedDelegate;

	FOnExpansionChanged ExpansionInOverviewChangedDelegate;
	
	FOnStructureChanged StructureChangedDelegate;

	FOnDataObjectModified DataObjectModifiedDelegate;

	FOnRequestFullRefresh RequestFullRefreshDelegate;

	FOnRequestFullRefresh RequestFullRefreshDeferredDelegate;

	FOnAlternateDisplayNameChanged AlternateDisplayNameChangedDelegate;

	TArray<FOnFilterChild> ChildFilters;

	UPROPERTY()
	TArray<TObjectPtr<UNiagaraStackEntry>> Children;

	mutable bool bFilterChildrenPending;

	mutable TArray<UNiagaraStackEntry*> FilteredChildren;

	UPROPERTY()
	TArray<TObjectPtr<UNiagaraStackErrorItem>> ErrorChildren;

	mutable TOptional<bool> bIsExpandedCache;
	mutable TOptional<bool> bIsExpandedInOverviewCache;

	int32 IndentLevel;

	FName ExecutionCategoryName;
	FName ExecutionSubcategoryName;

	FOnRequestDrop OnRequestCanDropDelegate;
	FOnRequestDrop OnRequestDropDelegate;
	
	TArray<FStackIssue> StackIssues;
	TArray<FStackIssue> ExternalStackIssues;

	bool bIsFinalized;

	bool bIsSearchResult;

	mutable TOptional<bool> bHasBaseEmitterCache;

	bool bOwnerIsEnabled;

	TOptional<FText> AlternateDisplayName;

	mutable TOptional<FCollectedIssueData> CachedCollectedIssueData;
};


UCLASS()
class NIAGARAEDITOR_API UNiagaraStackSpacer : public UNiagaraStackEntry
{
	GENERATED_BODY()
public:
	void Initialize(FRequiredEntryData InRequiredEntryData, float InSpacerHeight, TAttribute<bool> InShouldShowInStack, FString InOwningStackItemEditorDataKey);
	virtual EStackRowStyle GetStackRowStyle() const override { return UNiagaraStackEntry::EStackRowStyle::Spacer; }
	virtual bool GetCanExpand() const override { return false; }
	virtual bool GetShouldShowInStack() const override { return ShouldShowInStack.Get(); }

	float GetSpacerHeight() const { return SpacerHeight; }

private:
	float SpacerHeight;
	TAttribute<bool> ShouldShowInStack;
};