// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "renderdoc_app.h"

class FRenderDocPluginLoader
{
public:
	void Initialize();
	void Release();

	typedef RENDERDOC_API_1_0_0 RENDERDOC_API_CONTEXT;

public:
	static void* GetRenderDocLibrary();

private:
	friend class FRenderDocPluginModule;
	friend class SRenderDocPluginSettingsEditorWindow;

	void* RenderDocDLL;
	RENDERDOC_API_CONTEXT* RenderDocAPI;
};

