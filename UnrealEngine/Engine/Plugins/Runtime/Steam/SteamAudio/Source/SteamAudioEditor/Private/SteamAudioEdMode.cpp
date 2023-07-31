//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "SteamAudioEdMode.h"
#include "SteamAudioEdModeToolkit.h"
#include "SteamAudioEditorModule.h"

#include "Toolkits/ToolkitManager.h"
#include "EditorModeManager.h"

namespace SteamAudio
{
	const FEditorModeID FSteamAudioEdMode::EM_SteamAudio = TEXT("EM_SteamAudio");

	void FSteamAudioEdMode::Enter()
	{
		FEdMode::Enter();

		if (!Toolkit.IsValid() && UsesToolkits())
		{
			Toolkit = MakeShareable(new FSteamAudioEdModeToolkit);
			Toolkit->Init(Owner->GetToolkitHost());
		}
	}

	void FSteamAudioEdMode::Exit()
	{
		if (Toolkit.IsValid())
		{
			FToolkitManager::Get().CloseToolkit(Toolkit.ToSharedRef());
			Toolkit.Reset();
		}

		FEdMode::Exit();
	}

	bool FSteamAudioEdMode::UsesToolkits() const
	{
		return true;
	}
}
