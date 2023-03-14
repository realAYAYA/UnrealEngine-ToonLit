//
// Copyright (C) Valve Corporation. All rights reserved.
//

#pragma once

#include "Editor/UnrealEd/Public/Toolkits/BaseToolkit.h"
#include "IDetailsView.h"

class FEdMode;
class SWidget;
class IToolkitHost;
class FTabManager;
class FRunnableThread;

namespace SteamAudio
{
	class FSteamAudioEdModeToolkit : public FModeToolkit
	{
	public:
		FSteamAudioEdModeToolkit();
		~FSteamAudioEdModeToolkit();

		/** Initializes the geometry mode toolkit */
		virtual void Init(const TSharedPtr<IToolkitHost>& InitToolkitHost) override;

		/** IToolkit interface */
		virtual FName GetToolkitFName() const override;
		virtual FText GetBaseToolkitName() const override;
		virtual FEdMode* GetEditorMode() const override;
		virtual TSharedPtr<SWidget> GetInlineContent() const override { return ToolkitWidget; }

		FReply OnAddAll();
		bool IsAddAllEnabled() const;

		FReply OnRemoveAll();
		bool IsRemoveAllEnabled() const;

		FReply OnExportObj();
		bool IsExportObjEnabled() const;

		FReply OnExportScene();
		bool IsExportSceneEnabled() const;

		FText GetNumSceneTrianglesText() const;
		FText GetNumDynSceneTrianglesText() const;
		FText GetSceneDataSizeText() const;
		FText GetDynSceneDataSizeText() const;

	private:
		TSharedPtr<SWidget> ToolkitWidget;
	};
}