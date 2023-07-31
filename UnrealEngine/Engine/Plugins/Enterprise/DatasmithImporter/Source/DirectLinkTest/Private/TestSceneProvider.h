// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DirectLinkCommon.h"
#include "DirectLinkConnectionRequestHandler.h"


class FDatasmithSceneReceiver;

class FTestSceneProvider : public DirectLink::IConnectionRequestHandler
{
public:
	virtual bool CanOpenNewConnection(const FSourceInformation& Source) override;
	virtual TSharedPtr<DirectLink::ISceneReceiver> GetSceneReceiver(const FSourceInformation& Source) override;

	TMap<FGuid, TSharedPtr<FDatasmithSceneReceiver>> SceneReceivers;
};
