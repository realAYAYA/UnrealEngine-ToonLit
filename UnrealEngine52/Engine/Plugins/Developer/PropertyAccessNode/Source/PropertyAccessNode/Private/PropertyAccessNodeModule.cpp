// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "PropertyAccessNodeFactory.h"

class FPropertyAccessNodeModule : public IModuleInterface
{
public:
	// IModuleInterface interface
	virtual void StartupModule()
	{
		PropertyAccessNodeFactory = MakeShared<FPropertyAccessNodeFactory>();
		FEdGraphUtilities::RegisterVisualNodeFactory(PropertyAccessNodeFactory);
	}

	virtual void ShutdownModule()
	{
		FEdGraphUtilities::UnregisterVisualNodeFactory(PropertyAccessNodeFactory);
	}

	TSharedPtr<FPropertyAccessNodeFactory> PropertyAccessNodeFactory;
};

IMPLEMENT_MODULE(FPropertyAccessNodeModule, PropertyAccessNode)
