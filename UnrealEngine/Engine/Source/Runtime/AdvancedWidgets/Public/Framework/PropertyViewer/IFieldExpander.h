// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/UnrealType.h"

class FObjectPropertyBase;
class FProperty;
class FStructProperty;
class UFunction;
class UStruct;

namespace UE::PropertyViewer
{

/** */
class ADVANCEDWIDGETS_API IFieldExpander
{
public:
	/** @return the class that should be expended for this instance. The instance can be null. */
	virtual TOptional<const UClass*> CanExpandObject(const FObjectPropertyBase* Property, const UObject* Instance) const = 0;
	/** @return true if the struct property can be expended. */
	virtual bool CanExpandScriptStruct(const FStructProperty* StructProperty) const = 0;
	virtual bool CanExpandFunction(const UFunction*) const = 0;
	virtual ~IFieldExpander() = default;
};

class ADVANCEDWIDGETS_API FFieldExpander_Default : public IFieldExpander
{
public:
	FFieldExpander_Default() = default;

	enum class EObjectExpandFlag : uint8
	{
		None = 0,
		UseInstanceClass = 1 << 1,
		RequireValidInstance = 1 << 2,
	};

	virtual TOptional<const UClass*> CanExpandObject(const FObjectPropertyBase* Property, const UObject* Instance) const override
	{
		const bool bUseInstanceClass = EnumHasAllFlags(ExpandObject, EObjectExpandFlag::UseInstanceClass);
		const bool bOnlyIfInstanceValid = EnumHasAllFlags(ExpandObject, EObjectExpandFlag::RequireValidInstance);
		if (bOnlyIfInstanceValid && !Instance)
		{
			return TOptional<const UClass*>();
		}
		if (bUseInstanceClass && Instance)
		{
			return Instance->GetClass();
		}
		return Property->PropertyClass;
	}
	virtual bool CanExpandScriptStruct(const FStructProperty*) const override { return bExpandScriptStruct; }
	virtual bool CanExpandFunction(const UFunction*) const override { return bExpandFunction; }

	void SetExpandObject(EObjectExpandFlag InExpandObject)
	{
		ExpandObject = InExpandObject;
	}

	void SetExpandScriptStruct(bool bInExpandScriptStruct)
	{
		bExpandScriptStruct = bInExpandScriptStruct;
	}
	
	void SetExpandFunction(bool bInExpandFunction)
	{
		bExpandFunction = bInExpandFunction;
	}

private:
	EObjectExpandFlag ExpandObject = EObjectExpandFlag::None;
	bool bExpandScriptStruct = false;
	bool bExpandFunction = false;
};
ENUM_CLASS_FLAGS(FFieldExpander_Default::EObjectExpandFlag);

} //namespace
