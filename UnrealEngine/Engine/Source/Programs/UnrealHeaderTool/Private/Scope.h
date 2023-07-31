// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"
#include "UnrealTypeDefinitionInfo.h"

class UEnum;
class UScriptStruct;
class UDelegateFunction;

// Forward declarations.
class UStruct;
class FUnrealSourceFile;
class FFileScope;
class FStructScope;

// Traits to achieve conditional types for const/non-const iterators.
template <bool Condition, class TypeIfTrue, class TypeIfFalse>
class TConditionalType
{
public:
	typedef TypeIfFalse Type;
};

template <class TypeIfTrue, class TypeIfFalse>
class TConditionalType<true, TypeIfTrue, TypeIfFalse>
{
public:
	typedef TypeIfTrue Type;
};

// Base class representing type scope.
class FScope : public TSharedFromThis<FScope>
{
public:
	// Default constructor i.e. Parent == nullptr
	FScope();

	// Constructor that's providing parent scope.
	FScope(FScope* Parent);

	// Virtual destructor.
	virtual ~FScope()
	{ };

	virtual FFileScope* AsFileScope() { return nullptr; }
	virtual FStructScope* AsStructScope() { return nullptr; }

	/**
	 * Adds type to the scope.
	 *
	 * @param Type Type to add.
	 */
	void AddType(FUnrealFieldDefinitionInfo& Type);

	/**
	 * Finds type by its name.
	 *
	 * @param Name Name to look for.
	 *
	 * @returns Found type or nullptr on failure.
	 */
	FUnrealFieldDefinitionInfo* FindTypeByName(FName Name);

	/**
	 * Finds type by its name.
	 *
	 * @param Name Name to look for.
	 *
	 * @returns Found type or nullptr on failure.
	 */
	template <typename CharType>
	FUnrealFieldDefinitionInfo* FindTypeByName(const CharType* Name)
	{
		return FindTypeByName(FName(Name, FNAME_Find));
	}

	/**
	 * Finds type by its name.
	 *
	 * Const version.
	 *
	 * @param Name Name to look for.
	 *
	 * @returns Found type or nullptr on failure.
	 */
	const FUnrealFieldDefinitionInfo* FindTypeByName(FName Name) const;

	/**
	 * Finds type by its name.
	 *
	 * Const version.
	 *
	 * @param Name Name to look for.
	 *
	 * @returns Found type or nullptr on failure.
	 */
	template <typename CharType>
	const FUnrealFieldDefinitionInfo* FindTypeByName(const CharType* Name) const
	{
		return FindTypeByName(FName(Name, FNAME_Find));
	}

	/**
	 * Checks if scope contains type that satisfies a predicate.
	 *
	 * @param Predicate Predicate to satisfy.
	 */
	template <typename TPredicateType>
	bool Contains(TPredicateType Predicate)
	{
		for (const auto& NameTypePair : TypeMap)
		{
			if (Predicate.operator()(NameTypePair.Value))
			{
				return true;
			}
		}

		return false;
	}

	/**
	 * Collect a list of all types declared in this scope.  This includes any types defined in other types which 
	 * at this time is not currently supported.  Functions that aren't delegates are not collected.
	 * The types are returned in the order they are declared.
	 */
	void GatherTypes(TArray<FUnrealFieldDefinitionInfo*>& Types);

	/**
	 * Gets scope name.
	 */
	virtual FName GetName() const = 0;

	/**
	 * Gets scope's parent.
	 */
	const FScope* GetParent() const
	{
		return Parent;
	}

	/**
	 * Scope type iterator.
	 */
	template <class TType, bool TIsConst>
	class TScopeTypeIterator
	{
	public:
		// Conditional typedefs.
		typedef typename TConditionalType<TIsConst, TMap<FName, FUnrealFieldDefinitionInfo*>::TConstIterator, TMap<FName, FUnrealFieldDefinitionInfo*>::TIterator>::Type MapIteratorType;
		typedef typename TConditionalType<TIsConst, const FScope, FScope>::Type ScopeType;

		// Constructor
		TScopeTypeIterator(ScopeType* Scope)
			: TypeIterator(Scope->TypeMap)
		{
			bBeforeStart = true;
		}

		/**
		 * Indirection operator.
		 *
		 * @returns Type this iterator currently point to.
		 */
		TType* operator*()
		{
			return bBeforeStart ? nullptr : static_cast<TType*>((*TypeIterator).Value);
		}

		/**
		 * Moves this iterator to next type.
		 *
		 * @returns True on success. False otherwise.
		 */
		bool MoveNext()
		{
			if (!bBeforeStart)
			{
				++TypeIterator;
			}

			bBeforeStart = false;
			return TypeIterator.operator bool();
		}

	private:

		// Current type.
		MapIteratorType TypeIterator;

		// Tells if this iterator is in initial state.
		bool bBeforeStart;
	};

	/**
	 * Gets type iterator.
	 *
	 * @return Type iterator.
	 */
	template <class TType = FUnrealFieldDefinitionInfo>
	TScopeTypeIterator<TType, false> GetTypeIterator()
	{
		return TScopeTypeIterator<TType, false>(this);
	}

	/**
	 * Gets const type iterator.
	 *
	 * @return Const type iterator.
	 */
	template <class TType = FUnrealFieldDefinitionInfo>
	TScopeTypeIterator<TType, true> GetTypeIterator() const
	{
		return TScopeTypeIterator<TType, true>(this);
	}

	/**
	 * Tells if this scope is a file scope.
	 *
	 * @returns True if this is a file scope. False otherwise.
	 */
	bool IsFileScope() const;

	/**
	 * Tells if this scope contains any type.
	 *
	 * @returns True if this scope contains type. False otherwise.
	 */
	bool ContainsTypes() const;

	FFileScope* GetFileScope();
private:
	// This scopes parent.
	const FScope* Parent;

	// Map of types in this scope.
	TMap<FName, FUnrealFieldDefinitionInfo*> TypeMap;
};

using FScopeSet = TSet<FScope*, DefaultKeyFuncs<FScope*>, TInlineSetAllocator<1024>>;

/**
 * Represents a scope associated with source file.
 */
class FFileScope : public FScope
{
public:
	FFileScope()
		: SourceFile(nullptr)
		, Name(NAME_None)
	{ }
	// Constructor.
	FFileScope(FName Name, FUnrealSourceFile* SourceFile);

	/**
	 * Includes other file scopes into this scope (#include).
	 *
	 * @param IncludedScope File scope to include.
	 */
	void IncludeScope(FFileScope* IncludedScope);

	virtual FFileScope* AsFileScope() override { return this; }

	/**
	 * Gets scope name.
	 */
	FName GetName() const override;

	/**
	 * Gets source file associated with this scope.
	 */
	FUnrealSourceFile* GetSourceFile() const;

	/**
	 * Appends included file scopes into given array.
	 *
	 * @param Out (Output parameter) Array to append scopes.
	 */
	void AppendIncludedFileScopes(FScopeSet& Out)
	{
		bool bAlreadyAdded = false;
		Out.Add(this, &bAlreadyAdded);

		if (!bAlreadyAdded)
		{
			for (FFileScope* IncludedScope : IncludedScopes)
			{
				IncludedScope->AppendIncludedFileScopes(Out);
			}
		}
	}

	const TArray<FFileScope*>& GetIncludedScopes() const
	{
		return IncludedScopes;
	}

	void SetSourceFile(FUnrealSourceFile* InSourceFile)
	{
		SourceFile = InSourceFile;
	}

private:
	// Source file.
	FUnrealSourceFile* SourceFile;

	// Scope name.
	FName Name;

	// Included scopes list.
	TArray<FFileScope*> IncludedScopes;
};


/**
 * Data structure representing scope of a struct or class.
 */
class FStructScope : public FScope
{
public:
	// Constructor.
	FStructScope(FUnrealStructDefinitionInfo& InStructDef, FScope* InParent)
		: FScope(InParent)
		, StructDef(InStructDef)
	{
	}

	virtual FStructScope* AsStructScope() override { return this; }

	/**
	 * Get the structure definition associated with this scope
	 */
	FUnrealStructDefinitionInfo& GetStructDef() const
	{
		return StructDef;
	}

	/**
	 * Gets scope name.
	 */
	FName GetName() const override;

private:
	// Struct associated with this scope.
	FUnrealStructDefinitionInfo& StructDef;
};

/**
 * Deep scope type iterator. It looks for type in the whole scope hierarchy.
 * First going up inheritance from inner structs to outer. Then through all
 * included scopes.
 */
template <class TType, bool TIsConst>
class TDeepScopeTypeIterator
{
public:
	// Conditional typedefs.
	typedef typename TConditionalType<TIsConst, TMap<FName, UField*>::TConstIterator, TMap<FName, UField*>::TIterator>::Type MapIteratorType;
	typedef typename TConditionalType<TIsConst, const FScope, FScope>::Type ScopeType;
	typedef typename TConditionalType<TIsConst, const FFileScope, FFileScope>::Type FileScopeType;
	typedef typename TConditionalType<TIsConst, FScopeSet::TConstIterator, FScopeSet::TIterator>::Type ScopeArrayIteratorType;

	// Constructor.
	TDeepScopeTypeIterator(ScopeType* Scope)
	{
		const ScopeType* CurrentScope = Scope;

		while (!CurrentScope->IsFileScope())
		{
			ScopesToTraverse.Add(Scope);

			FUnrealStructDefinitionInfo& StructDef = ((FStructScope*)CurrentScope)->GetStructDef();

			if (FUnrealClassDefinitionInfo* ClassDef = UHTCast<FUnrealClassDefinitionInfo>(StructDef))
			{
				// Skip myself when starting this loop, we only care about the parents
				for (ClassDef = ClassDef->GetSuperClass(); ClassDef && !ClassDef->HasAnyClassFlags(EClassFlags::CLASS_Intrinsic); ClassDef = ClassDef->GetSuperClass())
				{
					ScopesToTraverse.Add(&ClassDef->GetScope().Get());
				}
			}

			CurrentScope = CurrentScope->GetParent();
		}

		((FileScopeType*)CurrentScope)->AppendIncludedFileScopes(ScopesToTraverse);
	}

	/**
	 * Iterator increment.
	 *
	 * @returns True if moved iterator to another position. False otherwise.
	 */
	bool MoveNext()
	{
		if (!ScopeIterator.IsSet() && !MoveToNextScope())
		{
			return false;
		}

		if (ScopeIterator->MoveNext())
		{
			return true;
		}
		else
		{
			do
			{
				ScopeIterator.Reset();
				if (!MoveToNextScope())
				{
					return false;
				}
			}
			while(!ScopeIterator->MoveNext());

			return true;
		}
	}

	/**
	 * Current type getter.
	 *
	 * @returns Current type.
	 */
	TType* operator*()
	{
		return ScopeIterator->operator*();
	}

private:
	/**
	 * Moves the iterator to the next scope.
	 *
	 * @returns True if succeeded. False otherwise.
	 */
	bool MoveToNextScope()
	{
		if (!ScopesIterator.IsSet())
		{
			ScopesIterator.Emplace(ScopesToTraverse);
		}
		else
		{
			ScopesIterator->operator++();
		}

		if (!ScopesIterator->operator bool())
		{
			return false;
		}

		ScopeIterator.Emplace(ScopesIterator->operator*());
		return true;
	}

	// Current scope iterator.
	TOptional<FScope::TScopeTypeIterator<TType, TIsConst> > ScopeIterator;

	// Scopes list iterator.
	TOptional<ScopeArrayIteratorType> ScopesIterator;

	// List of scopes to traverse.
	FScopeSet  ScopesToTraverse;
};

