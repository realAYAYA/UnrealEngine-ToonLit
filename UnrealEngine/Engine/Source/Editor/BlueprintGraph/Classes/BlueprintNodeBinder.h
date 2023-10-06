// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/Casts.h"
#include "UObject/Object.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakFieldPtr.h"

class UEdGraphNode;

class FBindingObject
{
	TWeakObjectPtr<UObject> Object;
	TWeakFieldPtr<FField> Property;
	bool bIsUObject;
public:
	FBindingObject()
		: bIsUObject(false)
	{}
	template <typename T, decltype(ImplicitConv<UObject*>(DeclVal<T>()))* = nullptr>
	FBindingObject(T InObject)
		: Object(InObject)
		, bIsUObject(true)
	{}
	FBindingObject(FField* InField)
		: Property(InField)
		, bIsUObject(false)
	{}
	FBindingObject(FFieldVariant InFieldOrObject)
	{
		bIsUObject = InFieldOrObject.IsUObject();
		if (bIsUObject)
		{
			Object = InFieldOrObject.ToUObject();
		}
		else
		{
			Property = InFieldOrObject.ToField();
		}
	}
	template <typename T, decltype(ImplicitConv<UObject*>(DeclVal<T>()))* = nullptr>
	FBindingObject& operator=(T InObject)
	{
		Object = ImplicitConv<UObject*>(InObject);
		Property = nullptr;
		bIsUObject = true;
		return *this;
	}
	FBindingObject& operator=(FField* InField)
	{
		Object = nullptr;
		Property = InField;
		bIsUObject = false;
		return *this;
	}
	FBindingObject& operator=(TYPE_OF_NULLPTR)
	{
		Object = nullptr;
		Property = nullptr;
		return *this;
	}
	bool IsUObject() const
	{
		return bIsUObject;
	}
	bool IsValid() const
	{
		return bIsUObject ? Object.IsValid() : Property.IsValid();
	}
	FName GetFName() const
	{
		return bIsUObject ? Object->GetFName() : Property->GetFName();
	}
	FString GetName() const
	{
		return bIsUObject ? Object->GetName() : Property->GetName();
	}
	FString GetPathName() const
	{
		return bIsUObject ? Object->GetPathName() : Property->GetPathName();
	}
	FString GetFullName() const
	{
		return bIsUObject ? Object->GetFullName() : Property->GetFullName();
	}
	bool IsA(const UClass* InClass) const
	{
		return bIsUObject && Object.IsValid() && Object->IsA(InClass);
	}
	bool IsA(const FFieldClass* InClass) const
	{
		return !bIsUObject && Property.IsValid() && Property->IsA(InClass);
	}
	template <typename T>
	bool IsA() const
	{
		if constexpr (std::is_base_of_v<UObject, T>)
		{
			if (bIsUObject && Object.IsValid())
			{
				return Object->IsA(T::StaticClass());
			}
		}
		else
		{
			if (!bIsUObject && Property.IsValid())
			{
				return Property->IsA(T::StaticClass());
			}
		}
		return false;
	}
	template <typename T>
	T* Get() const
	{
		if constexpr (std::is_base_of_v<UObject, T>)
		{
			if (bIsUObject && Object.IsValid())
			{
				return Cast<T>(Object.Get());
			}
		}
		else
		{
			if (!bIsUObject && Property.IsValid())
			{
				return CastField<T>(Property.Get());
			}
		}
		return nullptr;
	}

	friend uint32 GetTypeHash(const FBindingObject& BindingObject)
	{
		return BindingObject.bIsUObject ? GetTypeHash(BindingObject.Object.Get()) : GetTypeHash(BindingObject.Property.Get());
	}

	bool operator==(const FBindingObject &Other) const
	{
		return bIsUObject == Other.bIsUObject && Object == Other.Object && Property == Other.Property;
	}
	bool operator!=(const FBindingObject &Other) const
	{
		return bIsUObject != Other.bIsUObject || Object != Other.Object || Property != Other.Property;
	}

	friend bool operator==(const FBindingObject &Lhs, const UObject* Rhs)
	{
		return Lhs.IsUObject() && Lhs.Object == Rhs;
	}
	friend bool operator!=(const FBindingObject &Lhs, const UObject* Rhs)
	{
		return !Lhs.IsUObject() || Lhs.Object != Rhs;
	}
	friend bool operator==(const UObject* Lhs, const FBindingObject &Rhs)
	{
		return Rhs.IsUObject() && Rhs.Object == Lhs;
	}
	friend bool operator!=(const UObject* Lhs, const FBindingObject &Rhs)
	{
		return !Rhs.IsUObject() || Rhs.Object != Lhs;
	}

	friend bool operator==(const FBindingObject &Lhs, const FField* Rhs)
	{
		return !Lhs.IsUObject() && Lhs.Property == Rhs;
	}
	friend bool operator!=(const FBindingObject &Lhs, const FField* Rhs)
	{
		return Lhs.IsUObject() || Lhs.Property != Rhs;
	}
	friend bool operator==(const FField* Lhs, const FBindingObject &Rhs)
	{
		return !Rhs.IsUObject() && Rhs.Property == Lhs;
	}
	friend bool operator!=(const FField* Lhs, const FBindingObject &Rhs)
	{
		return Rhs.IsUObject() || Rhs.Property != Lhs;
	}

	friend bool operator==(const FBindingObject &Lhs, TYPE_OF_NULLPTR)
	{
		return Lhs.IsUObject() ? !Lhs.Object.IsValid() : !Lhs.Property.IsValid();
	}
	friend bool operator!=(const FBindingObject &Lhs, TYPE_OF_NULLPTR)
	{
		return Lhs.IsUObject() ? Lhs.Object.IsValid() : Lhs.Property.IsValid();
	}
	friend bool operator==(TYPE_OF_NULLPTR, const FBindingObject &Rhs)
	{
		return Rhs.IsUObject() ? !Rhs.Object.IsValid() : !Rhs.Property.IsValid();
	}
	friend bool operator!=(TYPE_OF_NULLPTR, const FBindingObject &Rhs)
	{
		return Rhs.IsUObject() ? Rhs.Object.IsValid() : Rhs.Property.IsValid();
	}
	
};


class IBlueprintNodeBinder
{
public:
	/** */
	typedef TSet< FBindingObject > FBindingSet;

public:
	/**
	 * Checks to see if the specified object can be bound by this.
	 * 
	 * @param  BindingCandidate	The object you want to check for.
	 * @return True if BindingCandidate can be bound by this controller, false if not.
	 */
	virtual bool IsBindingCompatible(FBindingObject BindingCandidate) const = 0;

	/**
	 * Determines if this will accept more than one binding (used to block multiple 
	 * bindings from being applied to nodes that can only have one).
	 * 
	 * @return True if this will accept multiple bindings, otherwise false.
	 */
	virtual bool CanBindMultipleObjects() const = 0;

	/**
	 * Attempts to bind all bindings to the supplied node.
	 * 
	 * @param  Node	 The node you want bound to.
	 * @return True if all bindings were successfully applied, false if any failed.
	 */
	bool ApplyBindings(UEdGraphNode* Node, FBindingSet const& Bindings) const
	{
		uint32 BindingCount = 0;
		for (const FBindingObject& Binding : Bindings)
		{
			if (Binding.IsValid() && BindToNode(Node, Binding))
			{
				++BindingCount;
				if (!CanBindMultipleObjects())
				{
					break;
				}
			}
		}
		return (BindingCount == Bindings.Num());
	}

protected:
	/**
	 * Attempts to apply the specified binding to the supplied node.
	 * 
	 * @param  Node		The node you want bound.
	 * @param  Binding	The binding you want applied to Node.
	 * @return True if the binding was successful, false if not.
	 */
	virtual bool BindToNode(UEdGraphNode* Node, FBindingObject Binding) const = 0;
};
