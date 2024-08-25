// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AsyncDetailViewDiff.h"
#include "UObject/Object.h"
#include "Templates/SharedPointer.h"
#include "InstanceDataObjectFixupPanel.generated.h"

class SLinkableScrollBar;

struct FRedirectedPropertyNode : public TSharedFromThis<FRedirectedPropertyNode>
{
	FRedirectedPropertyNode() = default;
	FRedirectedPropertyNode(const FRedirectedPropertyNode& Other);
	FRedirectedPropertyNode(const FPropertyInfo& Info, const TWeakPtr<FRedirectedPropertyNode>& Parent);
	FRedirectedPropertyNode(FName InPropertyName, FName InType, int32 InArrayIndex, const TWeakPtr<FRedirectedPropertyNode>& InParent);
	TSharedPtr<FRedirectedPropertyNode> FindOrAdd(const FPropertyPath& Path, int32 PathIndex = 0);
	TSharedPtr<FRedirectedPropertyNode> FindOrAdd(const FPropertyInfo& ChildInfo);
	TSharedPtr<FRedirectedPropertyNode> FindOrAdd(FName ChildPropertyName, FName ChildType, int32 ChildArrayIndex = 0);
	bool Remove(const FPropertyPath& Path, int32 PathIndex = 0);
	bool Remove(const FPropertyInfo& ChildInfo);
	bool Remove(FName ChildPropertyName, FName ChildType, int32 ChildArrayIndex = 0);
	TSharedPtr<FRedirectedPropertyNode> Find(const FPropertyPath& Path, int32 PathIndex = 0) const;
	TSharedPtr<FRedirectedPropertyNode> Find(const FPropertyInfo& ChildInfo) const;
	TSharedPtr<FRedirectedPropertyNode> Find(FName ChildPropertyName, FName ChildType, int32 ChildArrayIndex = 0) const;
	bool Move(const FPropertyPath& FromPath, const FPropertyPath& ToPath);
	int32 FindIndex(const FPropertyInfo& ChildInfo) const;
	int32 FindIndex(FName ChildPropertyName, FName ChildType, int32 ChildArrayIndex = 0) const;

	FName PropertyName;
	FName Type;
	int32 ArrayIndex;
	TWeakPtr<FRedirectedPropertyNode> Parent;
	TArray<TSharedPtr<FRedirectedPropertyNode>> Children;
};

class FInstanceDataObjectFixupPanel : public TSharedFromThis<FInstanceDataObjectFixupPanel>
{
public:
	enum class EViewFlags : uint8
	{
		None = 0,
		HideLooseProperties = (1 << 0), // hide properties with isLoose metadata set to true
		IncludeOnlySetBySerialization = (1 << 1), // hide properties that weren't set by serialization
		AllowRemapLooseProperties = (1 << 3),
		ReadonlyValues = (1 << 2),

		// displays only properties found in the property bag, allow remapping, and disallow value edits
		DefaultLeftPanel = IncludeOnlySetBySerialization | AllowRemapLooseProperties | ReadonlyValues,
		// display only properties found in latest version of the class but allow value edits
		DefaultRightPanel = HideLooseProperties,
	};
	
	FInstanceDataObjectFixupPanel(TConstArrayView<TObjectPtr<UObject>> InstanceDataObjects, EViewFlags ViewFlags);
	
	int32 Find(UObject* Value) const;
	TSharedPtr<IDetailsView>& GenerateDetailsView(bool bScrollbarOnLeft = false);
	
	void SetDiffAgainstLeft(const TSharedPtr<FAsyncDetailViewDiff>& DiffAgainstLeft);
	void SetDiffAgainstRight(const TSharedPtr<FAsyncDetailViewDiff>& DiffAgainstRight);
	TSharedPtr<FAsyncDetailViewDiff> GetDiffAgainstLeft() const;
	TSharedPtr<FAsyncDetailViewDiff> GetDiffAgainstRight() const;

	bool ShouldSplitterIgnoreRow(const TWeakPtr<FDetailTreeNode>& DetailTreeNode) const;
	bool AreAllConflictsRedirected() const;
	const FPropertyPath& GetOriginalPath(const FPropertyPath& Path) const;
	
	void MarkForDelete(const FPropertyPath& Path);
	// pass-by-copy version for delegates. Use MarkForDelete when possible
	void OnMarkForDelete(FPropertyPath Path);
	// mark all conflicted properties for delete (FixupMode only)
	void AutoApplyMarkDeletedActions();

	// the redirected property tree keeps track of which properties in the InstanceDataObject were either set by a property bag during serialization or
	// redirected to from a floating property. The members of the tree are visible in the left panel.
	bool IsInRedirectedPropertyTree(const FPropertyPath& Path) const;

	bool HasViewFlag(EViewFlags Flag);

	// Initialized by SInstanceDataObjectFixupTool
	TSharedPtr<IDetailsView> DetailsView;
	TSharedPtr<SLinkableScrollBar> LinkableScrollBar;

	// functor used to generate warnings about a type conversion (or set of conversions) and invoke that conversion on demand
	struct FTypeConverter
	{
		FTypeConverter() = default;
		
		// add a conversion that should be performed call operator is invoked.
		void Push(FProperty* SourceProperty, const void* SourceData, FProperty* DestinationProperty, void* DestinationData);

		// return whether this is a valid conversion. (only true if all conversions pushed are valid)
		operator bool() const;

		// run conversion on all pushed data
		void operator()() const;

		// return the most severe text warning for the conversions pushed.
		FText GetWarning() const;
	private:
		enum class EWarning
		{
			// sorted by least severe to most severe.
			SafeConversion,
			NarrowingConversion,
			NonInvertibleConversion,
			InvalidConversion,
		};
		struct FInstanceInfo
		{
			FProperty* SourceProperty;
			const void* SourceData;
			FProperty* DestinationProperty;
			void* DestinationData;
		};
		
		static bool TryConvert(FProperty* SourceProperty, const void* SourceData, FProperty* DestinationProperty, void* DestinationData);
		static EWarning GenerateWarning(FProperty* SourceProperty, const void* SourceData, FProperty* DestinationProperty);

		// most severe conversion warning found in all the pushed data
		EWarning Warning = EWarning::SafeConversion;

		TArray<FInstanceInfo> InstanceInfo;
	};
	
	FTypeConverter CreateTypeConverter(const FPropertyPath& From, const FPropertyPath& To);

private:
	friend class FInstanceDataObjectFixupSpecification; // for access to Redirects
	friend class FInstanceDataObjectNameWidgetOverride;
	friend class UInstanceDataObjectFixupUndoHandler;
	struct FRevertInfo
	{
		TArray<uint8> OriginalValue;
		FPropertyPath OriginalPath;
		bool bWasTransient;
		bool bWasHidden;
	};

	void RedirectPropertyHelper(const FPropertyPath& From, const FPropertyPath& To, TOptional<FRevertInfo>& FromRevertInfo, FRevertInfo*& ToRevertInfo);
	void RedirectProperty(const FPropertyPath& From, const FPropertyPath& To);
	void RedirectProperty(const FPropertyPath& From, const FPropertyPath& To, const FTypeConverter& TypeConversion);
	// pass-by-copy version for delegates. Use RedirectProperty when possible
	void OnRedirectProperty(FPropertyPath From, FPropertyPath To);
	void OnRedirectProperty(FPropertyPath From, FPropertyPath To, FTypeConverter TypeConversion);
	
	void InitRedirectedPropertyTree();
	
	TArray<TObjectPtr<UObject>> Instances; // stores either InstanceDataObject property bag or just regular objects

	// the redirected property tree keeps track of which properties in the InstanceDataObject were either set by a property bag during serialization or
	// redirected to from a floating property. The members of the tree are visible in the left panel.
	TSharedPtr<FRedirectedPropertyNode> RedirectedPropertyTree;

	
	TMap<FPropertyPath, FRevertInfo> RevertInfo;
	TSet<FPropertyPath> MarkedForDelete;
	
	TWeakPtr<FAsyncDetailViewDiff> DiffAgainstLeft;
	TWeakPtr<FAsyncDetailViewDiff> DiffAgainstRight;
	EViewFlags ViewFlags = EViewFlags::None;
};

UCLASS()
class UInstanceDataObjectFixupUndoHandler : public UObject
{
public:
	GENERATED_BODY()
	void Init(const TSharedRef<FInstanceDataObjectFixupPanel>& Panel);
	void OnRedirect(const FPropertyPath& From, const FPropertyPath& To);
	virtual void PostEditUndo() override;
	
	TWeakPtr<FInstanceDataObjectFixupPanel> InstanceDataObjectPanel;
	
	TMap<FPropertyPath, FInstanceDataObjectFixupPanel::FRevertInfo> RevertInfo;
	TSet<FPropertyPath> MarkedForDelete;
	FPropertyPath RedirectFrom;
	FPropertyPath RedirectTo;
	
	UPROPERTY()
	int32 ChangeNum = 0;
};
