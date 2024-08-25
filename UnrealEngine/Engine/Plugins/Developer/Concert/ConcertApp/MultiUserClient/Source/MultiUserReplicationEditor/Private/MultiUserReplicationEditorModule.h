// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMultiUserReplicationEditorModule.h"

namespace UE::MultiUserReplicationEditor
{
	class FMultiUserReplicationEditorModule : public IMultiUserReplicationEditorModule
	{
	public:
	
		//~ Begin IModuleInterface Interface
		virtual void StartupModule() override;
		virtual void ShutdownModule() override;
		//~ End IModuleInterface Interface
	};
}
