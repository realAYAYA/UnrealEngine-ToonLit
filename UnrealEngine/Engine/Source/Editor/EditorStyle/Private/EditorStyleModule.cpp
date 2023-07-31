// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IEditorStyleModule.h"
#include "SlateEditorStyle.h"
#include "StarshipStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleColors.h"


/**
 * Implements the Editor style module, loaded by SlateApplication dynamically at startup.
 */
class FEditorStyleModule
	: public IEditorStyleModule
{
public:

	// IEditorStyleModule interface

	virtual void StartupModule( ) override
	{
		if (FCoreStyle::IsStarshipStyle())
		{
#if ALLOW_THEMES
			USlateThemeManager::Get().ValidateActiveTheme();
#endif

			bUsingStarshipStyle = true;
			FStarshipEditorStyle::Initialize();

			// set the application style to be the editor style
			FAppStyle::SetAppStyleSetName(FStarshipEditorStyle::GetStyleSetName());

		}
		else
		{
			FSlateEditorStyle::Initialize();

			// set the application style to be the editor style
			FAppStyle::SetAppStyleSetName(FAppStyle::GetAppStyleSetName());
		}
	}

	virtual void ShutdownModule( ) override
	{
		if (bUsingStarshipStyle)
		{
			FStarshipEditorStyle::Shutdown();
		}
		else
		{
			FSlateEditorStyle::Shutdown();
		}
	}

	// End IModuleInterface interface
private:
	bool bUsingStarshipStyle = false;
};


IMPLEMENT_MODULE(FEditorStyleModule, EditorStyle)
