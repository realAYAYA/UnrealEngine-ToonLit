// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/SoftObjectPath.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class IConcertClient;
enum class ECheckBoxState : uint8;

namespace UE::ConcertSharedSlate { class IObjectHierarchyModel; }

namespace UE::MultiUserClient
{
	class FReplicationClientManager;
	
	/**
	 * Checkbox in a combo button.
	 * 
	 * The checkbox can either the object it was created for or all of its child objects.
	 * If the checkbox in the combo button is pressed it defaults to toggling just the object.
	 *
	 * TODO UE-200487: Make sure this widget updates when a remote client changes their setting for being editable
	 */
	class SReplicationMultiToggleCheckbox : public SCompoundWidget
	{
	public:

		SLATE_BEGIN_ARGS(SReplicationMultiToggleCheckbox)
		{}
		    SLATE_ARGUMENT(FSoftObjectPath, Object)
			SLATE_ATTRIBUTE(ConcertSharedSlate::IObjectHierarchyModel*, ObjectHierarchyModel)
		SLATE_END_ARGS()

		void Construct(
			const FArguments& InArgs,
		    FReplicationClientManager& InClientManager,
		    TSharedRef<IConcertClient> InConcertClient
		);

	private:

		/** The object path this checkbox was created for */
		FSoftObjectPath Object;

		/** Used to access all clients for toggling authority. */
		FReplicationClientManager* ClientManager = nullptr;
		/** Used to get children of Object */
		TAttribute<ConcertSharedSlate::IObjectHierarchyModel*> ObjectHierarchyModelAttribute;
		
		/** Used to look up client display names in case of conflicts. */
		TSharedPtr<IConcertClient> ConcertClient;

		/** @return Tooltip for the combo box. Informs user what the end result is if enabled or why it's disabled. */
		FText GetRootToolTipText() const;
		
		/** @return Goes through all clients that can be edited and returns whether all of them have authority. */
		ECheckBoxState GetCheckboxStateForObject(const FSoftObjectPath& InObject) const { return GetCheckboxStateForObjects({ InObject }); }
		ECheckBoxState GetCheckboxStateForThisAndChildren() const
		{
			// This algorithm could be improved but it will be performant enough for small hierarchies.
			// The growth is linear in the number of children but every checkbox evaluates itself constantly instead of asking child boxes for cached state.
			return GetCheckboxStateForObjects(GetThisAndChildren());
		}
		ECheckBoxState GetCheckboxStateForObjects(TConstArrayView<FSoftObjectPath> InObjects) const;
		/** @return Whether there are any clients for which the authority can be changed. */
		bool IsCheckboxEnabledForObject(FSoftObjectPath InObject) const;
		/** Sets the authority state of this and all children based on NewState. */
		void OnCheckboxStateChanged(ECheckBoxState NewState) const;

		/** @return Menu widget that with the options to remove or give authority to this and other objects. */
		TSharedRef<SWidget> GetDropDownMenuContent();

		TArray<FSoftObjectPath> GetThisAndChildren() const;
		TArray<FSoftObjectPath> GetChildObjects() const;

		void ToggleThis() const { ToggleObjects({ Object }); };
		void ToggleChildren() const { ToggleObjects(GetChildObjects()); }
		void ToggleThisAndChildren() const { ToggleObjects(GetThisAndChildren()); }

		bool CanToggleThis() const { return CanToggleObjects({ Object }); }
		bool CanToggleChildren() const { return CanToggleObjects(GetChildObjects()); }
		bool CanToggleThisOrChildren() const { return CanToggleObjects(GetThisAndChildren()); }

		void ToggleObjects(TConstArrayView<FSoftObjectPath> Objects) const;
		bool CanToggleObjects(TConstArrayView<FSoftObjectPath> Objects) const;
		void SetAuthorityForObject(bool bShouldHaveAuthority, const FSoftObjectPath& InObject) const;
		
		EVisibility GetWarningVisibility() const;
		FText GetWarningToolTipText() const;
		TArray<FGuid> GetNonEditableClients() const;
	};
}

