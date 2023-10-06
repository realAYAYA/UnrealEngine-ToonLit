// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Templates/SharedPointer.h"
#include "GeometryCollection/ManagedArray.h"

struct FManagedArrayCollection;

namespace UE::Chaos::ClothAsset
{
	/**
	 * Cloth Asset collection Selection facade class. Each Selection consists of a set of integer indices, a name, and a string representing the type of element being indexed.
	 * Const access (read only) version.
	 */
	class CHAOSCLOTHASSET_API FCollectionClothSelectionConstFacade
	{
	public:
		explicit FCollectionClothSelectionConstFacade(const TSharedRef<const FManagedArrayCollection>& ManagedArrayCollection);

		FCollectionClothSelectionConstFacade() = delete;
		FCollectionClothSelectionConstFacade(const FCollectionClothSelectionConstFacade&) = delete;
		FCollectionClothSelectionConstFacade& operator=(const FCollectionClothSelectionConstFacade&) = delete;
		FCollectionClothSelectionConstFacade(FCollectionClothSelectionConstFacade&&) = default;
		FCollectionClothSelectionConstFacade& operator=(FCollectionClothSelectionConstFacade&&) = default;
		virtual ~FCollectionClothSelectionConstFacade() = default;

		/** Return whether the facade is defined on the collection. */
		bool IsValid() const;

		/** Return the total number of selections in the collection */
		int32 GetNumSelections() const;

		const TArrayView<const FString> GetName() const;
		const TArrayView<const FString> GetType() const;
		const TArrayView<const TSet<int32>> GetIndices() const;

		/** Find a selection with the given name. Return INDEX_NONE if no such selection exists. */
		int32 FindSelection(const FString& Name) const;

	protected:

		TSharedRef<FManagedArrayCollection> ManagedArrayCollection;

		// Schema (non-const since these will be used by the subclass as well)
		TManagedArray<FString>* Name;
		TManagedArray<FString>* Type;
		TManagedArray<TSet<int32>>* Indices;
	};


	/**
	 * Cloth Asset collection Selection facade class. Each Selection consists of a set of integer indices, a name, and a string representing the type of element being indexed.
	 * Non-const access (read/write) version.
	 */
	class CHAOSCLOTHASSET_API FCollectionClothSelectionFacade final : public FCollectionClothSelectionConstFacade
	{
	public:

		explicit FCollectionClothSelectionFacade(const TSharedRef<const FManagedArrayCollection>& ManagedArrayCollection);

		FCollectionClothSelectionFacade() = delete;
		FCollectionClothSelectionFacade(const FCollectionClothSelectionFacade&) = delete;
		FCollectionClothSelectionFacade& operator=(const FCollectionClothSelectionFacade&) = delete;
		FCollectionClothSelectionFacade(FCollectionClothSelectionFacade&&) = default;
		FCollectionClothSelectionFacade& operator=(FCollectionClothSelectionFacade&&) = default;
		virtual ~FCollectionClothSelectionFacade() override = default;

		/** Add the Selection attributes to the underlying collection */
		void DefineSchema();

		TArrayView<FString> GetName();
		TArrayView<FString> GetType();
		TArrayView<TSet<int32>> GetIndices();

		/** 
		* Create a new Selection in the collection with the given name and return its index. If a Selection with the name already exists, return the index of the existing selection. 
		* 
		* @param Name The name of the new selection to create if it doesn't already exist
		* @return The previous number of Selections if we added a new Selection, or the index of the Selection with the given Name if it already existed, or INDEX_NONE if the schema for this facade object is not valid.
		*/
		int32 FindOrAddSelection(const FString& Name);

		/** Set the Type and Indices for the given Selection. Returns false if the Facade is invalid or if the SelectionIndex is out of range */
		bool SetSelection(int32 SelectionIndex, const FString& InType, const TSet<int32>& InSelectedIndices);
	};

}
