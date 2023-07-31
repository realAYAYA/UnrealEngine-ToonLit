//
// Copyright (C) Google Inc. 2017. All rights reserved.
//

#pragma once

#include "IResonanceAudioEditorModule.h"
#include "Framework/MultiBox/MultiBoxExtender.h"

DECLARE_LOG_CATEGORY_EXTERN(LogResonanceAudioEditor, Warning, All);

class FSlateStyleSet;

namespace ResonanceAudio
{
	class FResonanceAudioEditorModule : public IResonanceAudioEditorModule
	{
	public:
		virtual void StartupModule() override;
		virtual void ShutdownModule() override;

	private:
		TSharedPtr<FSlateStyleSet> ResonanceAudioStyleSet;
	};

} // namespace ResonanceAudio
