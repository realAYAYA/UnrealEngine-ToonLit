// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "GeometryCollection/ManagedArrayCollection.h"

namespace ManageArrayAccessor
{
	enum class EPersistencePolicy : uint8
	{
		KeepExistingPersistence,
		MakePersistent
	};
};

/**
 * this class wraps a managed array
 * this provides a convenient API for optional attributes in a collection facade
 */
template <typename T>
struct TManagedArrayAccessor
{
public:
	TManagedArrayAccessor(FManagedArrayCollection& InCollection, const FName& AttributeName, const FName& AttributeGroup)
		: Collection(InCollection)
		, Name(AttributeName)
		, Group(AttributeGroup)
	{
		AttributeArray = InCollection.FindAttributeTyped<T>(AttributeName, AttributeGroup);

	}
	bool IsValid() const { return AttributeArray != nullptr; }

	bool IsPersistent() const { return Collection.IsAttributePersistent(Name, Group); }

	/** get the attribute for read only */
	const TManagedArray<T>& Get() const
	{
		check(IsValid());
		return *AttributeArray;
	}

	/** get the attribute for modification */
	TManagedArray<T>& Modify() const
	{
		check(IsValid());
		AttributeArray->MarkDirty();
		return *AttributeArray;
	}

	/** add the attribute if it does not exists yet */
	TManagedArray<T>& Add(ManageArrayAccessor::EPersistencePolicy PersistencePolicy = ManageArrayAccessor::EPersistencePolicy::MakePersistent)
	{
		if (PersistencePolicy == ManageArrayAccessor::EPersistencePolicy::MakePersistent && !IsPersistent())
		{
			Remove();
		}
		const bool bSaved = (PersistencePolicy == ManageArrayAccessor::EPersistencePolicy::MakePersistent);
		FManagedArrayCollection::FConstructionParameters Params(FName(), bSaved);
		AttributeArray = &Collection.AddAttribute<T>(Name, Group, Params);
		return *AttributeArray;
	}

	/** add and fill the attribute if it does not exist yet */
	void AddAndFill(const T& Value, ManageArrayAccessor::EPersistencePolicy PersistencePolicy = ManageArrayAccessor::EPersistencePolicy::MakePersistent)
	{
		if (!Collection.HasAttribute(Name, Group))
		{
			Add(PersistencePolicy);
			AttributeArray->Fill(Value);
		}
	}

	/** Fill the attribute with a specific value */
	void Fill(const T& Value)
	{
		if (AttributeArray)
		{
			AttributeArray->Fill(Value);
		}
	}

	/** copy from another attribute ( create if necessary ) */
	void Copy(const TManagedArrayAccessor<T>& FromAttribute )
	{
		Collection.CopyAttribute(FromAttribute.Collection, Name, Group);
	}

	void Remove()
	{
		Collection.RemoveAttribute(Name, Group);
		AttributeArray = nullptr;
	}

private:
	FManagedArrayCollection& Collection;
	FName Name;
	FName Group;
	TManagedArray<T>* AttributeArray;
};
