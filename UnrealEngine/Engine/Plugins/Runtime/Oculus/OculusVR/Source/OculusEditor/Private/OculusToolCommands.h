// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "OculusToolStyle.h"
#include "OculusEditorModule.h"
#include "HAL/IConsoleManager.h"

	class FOculusToolCommands : public TCommands<FOculusToolCommands>
	{
	public:

		FOculusToolCommands()
			: TCommands<FOculusToolCommands>(TEXT("OculusTool"), NSLOCTEXT("Contexts", "OculusEditor", "OculusEditor Plugin"), NAME_None, FOculusToolStyle::GetStyleSetName()),
			ShowOculusToolCommand(TEXT("vr.oculus.ShowToolWindow"),
				*NSLOCTEXT("OculusRift", "CCommandText_ShowToolWindow", "Show the Oculus Editor Tool window (editor only).").ToString(),
				FConsoleCommandDelegate::CreateRaw(this, &FOculusToolCommands::ShowOculusTool)
			)
		{
		}

		// TCommands<> interface
		virtual void RegisterCommands() override;

	public:
		TSharedPtr< FUICommandInfo > OpenPluginWindow;

	private:
		void ShowOculusTool();

	private:
		FAutoConsoleCommand ShowOculusToolCommand;
	};