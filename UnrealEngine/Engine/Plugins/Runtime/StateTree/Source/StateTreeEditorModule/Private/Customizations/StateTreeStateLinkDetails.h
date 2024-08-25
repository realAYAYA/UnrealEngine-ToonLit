// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"

enum class EStateTreeTransitionType : uint8;
template <typename OptionalType> struct TOptional;

class IPropertyHandle;
class SWidget;
class UStateTree;
class UStateTreeState;

/**
 * Type customization for FStateTreeStateLink.
 */

class FStateTreeStateLinkDetails : public IPropertyTypeCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

private:

	// Special indices for the combo selection.
	inline static constexpr int ComboNotSet = -1;
	inline static constexpr int ComboSucceeded = -2;
	inline static constexpr int ComboFailed = -3;
	inline static constexpr int ComboNextState = -4;
	inline static constexpr int ComboNextSelectableState = -5;

	void CacheStates();
	void CacheStates(const UStateTreeState* State);
	void OnIdentifierChanged(const UStateTree& StateTree);

	void OnStateComboChange(int Idx);
	TSharedRef<SWidget> OnGetStateContent() const;
	FText GetCurrentStateDesc() const;
	bool IsValidLink() const;
	TOptional<EStateTreeTransitionType> GetTransitionType() const;

	TArray<FName> CachedNames;
	TArray<FGuid> CachedIDs;

	TSharedPtr<IPropertyHandle> NameProperty;
	TSharedPtr<IPropertyHandle> IDProperty;
	TSharedPtr<IPropertyHandle> LinkTypeProperty;

	// If set, hide selecting meta states like Next or (tree) Succeeded.
	bool bDirectStatesOnly = false;
	// If set, allow to select only states marked as subtrees.
	bool bSubtreesOnly = false;
	
	class IPropertyUtilities* PropUtils;
	TSharedPtr<IPropertyHandle> StructProperty;
};
