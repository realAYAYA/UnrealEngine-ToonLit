// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Param/ParamId.h"
#include "Param/ParamType.h"
#include "Param/ParamTypeHandle.h"
#include "AnimNextComponentParameter.generated.h"

class UAnimNextGraph;

// Boilerplate macro that all derived types of UAnimNextComponentParameter should use.
// Usage is: IMPLEMENT_ANIMNEXT_COMPONENT_PARAMETER(UAnimNextComponentParameter_DerivedType)
#define IMPLEMENT_ANIMNEXT_COMPONENT_PARAMETER(Type) \
	virtual void CacheParamInfo() const override \
	{ \
		using namespace UE::AnimNext; \
		if(Name == NAME_None || ValueProperty == nullptr) \
		{ \
			Name = Parameter; \
			ValueProperty = StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(Type, Value)); \
		} \
	} \

// Base class for static parameters that can be inserted into a schedule via a component
// Each parameter type needs its own derived type of this object
UCLASS(DefaultToInstanced, abstract, editinlinenew)
class UAnimNextComponentParameter : public UObject
{
	GENERATED_BODY()

public:
	// The scope to apply the parameter to. If this is "None", it is applied at the root scope of the schedule.
	UPROPERTY(EditAnywhere, Category = "Parameter", AdvancedDisplay, meta = (CustomWidget = "ParamName", AllowedParamType = "FAnimNextScope", AllowNone))
	FName Scope;

	// Check validity
	bool IsValid() const
	{
		CacheParamInfo();

		return ValueProperty != nullptr && Name != NAME_None;
	}
	
	// Get the name, type and value for this parameter
	void GetParamInfo(FName& OutName, const FProperty*& OutProperty) const
	{
		CacheParamInfo();

		OutName = Name;
		OutProperty = ValueProperty;
	}

private:
	virtual void CacheParamInfo() const PURE_VIRTUAL(UAnimNextComponentParameter::CacheParamInfo, )

protected:
	mutable FName Name;
	mutable FProperty* ValueProperty = nullptr;
};

// An object parameter
UCLASS(MinimalAPI, meta = (DisplayName = "Anim Next Graph"))
class UAnimNextComponentParameter_AnimNextGraph : public UAnimNextComponentParameter
{
	GENERATED_BODY()

	IMPLEMENT_ANIMNEXT_COMPONENT_PARAMETER(UAnimNextComponentParameter_AnimNextGraph);

	// The parameter to set the value to
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (CustomWidget = "ParamName", AllowedParamType = "TObjectPtr<UAnimNextGraph>"))
	FName Parameter;

	// The value to set
	UPROPERTY(EditAnywhere, Category = "Parameter")
	TObjectPtr<UAnimNextGraph> Value;
};
