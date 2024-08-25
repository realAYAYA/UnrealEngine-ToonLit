// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Param/IParameterSource.h"
#include "Features/IModularFeature.h"

namespace UE::AnimNext
{
	class IParameterSource;
}

namespace UE::AnimNext
{

// Context passed to object accessor functions registered to RegisterObjectAccessor
struct FExternalParameterContext
{
	// The object that the AnimNext entry is bound to (e.g. a UAnimNextComponent)
	UObject* Object = nullptr;
};

// Modular feature allowing other modules to extend and add to external parameter system
class IParameterSourceFactory : public IModularFeature
{
public:
	inline static const FName FeatureName = TEXT("AnimNextParameterSourceFactory");

	virtual ~IParameterSourceFactory() = default;

	// Iterate over all the source names that this factory provides
	virtual void ForEachSource(TFunctionRef<void(FName)> InFunction) const = 0;

	// Factory method used to create a parameter source of the specified name, with a set of parameters that are initially required
	// @param   InContext              Context used to set up the parameter source
	// @param   InSourceName           The name of the parameter source. This is conventionally the 'root' parameter of a set of parameters (e.g.
	//                                 Game_MyComponent, which contains Game_MyComponent_Param etc.)
	// @param   InRequiredParameters   Any required parameters that the source should initially supply, can be empty
	// @return a new parameter source, or nullptr if the source was not found
	virtual TUniquePtr<IParameterSource> CreateParameterSource(const FExternalParameterContext& InContext, FName InSourceName, TConstArrayView<FName> InRequiredParameters) const = 0;

#if WITH_EDITOR
	// Info about parameters gleaned from FindParameterInfo and ForEachParameter
	struct FParameterInfo
	{
		FAnimNextParamType Type;
		FText Tooltip;
		bool bThreadSafe = false;
	};

	// Given a parameter name, find associated info for that parameter
	// @param   InParameterName        The parameter name to find
	// @param   OutType                The parameter type
	// @param   OutTooltip             Optional: The parameter's tooltip
	// @return true if the parameter info was found
	virtual bool FindParameterInfo(FName InParameterName, FParameterInfo& OutInfo) const = 0;

	// Iterate over all the known parameters for a particular source
	// @param   InSourceName           The name of the parameter source to iterate
	// @param   InFunction             Function called for each known parameter
	// @return an array of parameter names
	virtual void ForEachParameter(FName InSourceName, TFunctionRef<void(FName, const FParameterInfo&)> InFunction) const = 0;
#endif
};

}