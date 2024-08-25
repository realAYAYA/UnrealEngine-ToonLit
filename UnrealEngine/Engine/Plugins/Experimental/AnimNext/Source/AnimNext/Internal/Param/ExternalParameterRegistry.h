// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Param/IParameterSource.h"
#include "Features/IModularFeature.h"
#include "Param/IParameterSourceFactory.h"

class UAnimNextSchedule;
class UAnimNextParameterBlockParameter;
class AActor;
class UActorComponent;

namespace UE::AnimNext
{
	struct FObjectAccessor;
	struct FExternalParameterContext;
}

namespace UE::AnimNext::Editor
{
	class SParameterPicker;
}

namespace UE::AnimNext::UncookedOnly
{
	struct FUtils;
}

namespace UE::AnimNext
{

// Registry of all object accessors 
struct FExternalParameterRegistry
{
	// Register all external parameter sources
	static void Init();

	// Unregister all external parameter sources
	static void Destroy();

	// Factory method used to create a parameter source of the specified name, with a set of parameters that are initially required
	// @param   InContext              Context used to set up the parameter source
	// @param   InSourceName           The name of the parameter source. This is conventionally the 'root' parameter of a set of parameters (e.g.
	//                                 Game_MyComponent, which contains Game_MyComponent_Param etc.)
	// @param   InRequiredParameters   Any required parameters that the source should initially supply, can be empty
	// @return a new parameter source, or nullptr if the source was not found
	static TUniquePtr<IParameterSource> CreateParameterSource(const FExternalParameterContext& InContext, FName InSourceName, TConstArrayView<FName> InRequiredParameters);

private:
	friend class ::UAnimNextSchedule;
	friend class ::UAnimNextParameterBlockParameter;
	friend class UE::AnimNext::Editor::SParameterPicker;
	friend struct UE::AnimNext::UncookedOnly::FUtils;

#if WITH_EDITOR
	// Given a parameter name, find associated info for that parameter
	// @param   InParameterName        The parameter name to find
	// @param   OutInfo                The parameter's info
	// @return true if the parameter info was found
	static ANIMNEXT_API bool FindParameterInfo(FName InParameterName, IParameterSourceFactory::FParameterInfo& OutInfo);

	// Iterate over all the known parameters
	// @param   InFunction             Function called for each known parameter
	static ANIMNEXT_API void ForEachParameter(TFunctionRef<void(FName, const IParameterSourceFactory::FParameterInfo&)> InFunction);

	// Given a parameter name, find the parameter source name for any registered source factories
	// @param   InParameterName        The parameter name to find
	// @return the name of the parameter source for the parameter name, or NAME_None if none could be found
	static ANIMNEXT_API FName FindSourceForParameter(FName InParameterName);
#endif
};

}
