//
// Copyright (C) Valve Corporation. All rights reserved.
//

#pragma once

#include "EdMode.h"
#include "EditorModes.h"
#include "EditorModeTools.h"

namespace SteamAudio
{
	class FSteamAudioEdMode : public FEdMode
	{
	public:
		static const FEditorModeID EM_SteamAudio;

		virtual void Enter() override;
		virtual void Exit() override;
		bool UsesToolkits() const override;
	};
}
