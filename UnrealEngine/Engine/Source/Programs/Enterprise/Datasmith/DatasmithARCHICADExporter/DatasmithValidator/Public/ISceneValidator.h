// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Logging/LogMacros.h"
#include "Templates/SharedPointer.h"

class IDatasmithScene;
class IDatasmithActorElement;

namespace Validator {

class DATASMITHVALIDATOR_API ISceneValidator
{
  public:
	enum TInfoLevel
	{
		kBug = ELogVerbosity::Fatal,
		kError = ELogVerbosity::Error,
		kWarning = ELogVerbosity::Warning,
		kVerbose = ELogVerbosity::Verbose,
		kInfoLevelMax
	};

    static TSharedRef< ISceneValidator > CreateForScene(const TSharedRef< IDatasmithScene >& InScene);

	virtual ~ISceneValidator() = default;

    virtual void CheckTexturesFiles() = 0;

    virtual void CheckMeshFiles() = 0;
    
    virtual void CheckElementsName() = 0;

    virtual void CheckActorsName(const IDatasmithActorElement& InActor) = 0;

    virtual void CheckDependances() = 0;

    virtual void CheckActorsDependances(const IDatasmithActorElement& InActor) = 0;
    
	virtual FString GetReports(TInfoLevel Level) = 0;
};

} // namespace Validator
