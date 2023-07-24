// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NetBlobTestFixture.h"
#include "MockNetObjectAttachment.h"

namespace UE::Net
{

class FPartialNetBlobTestFixture : public FNetBlobTestFixture
{
	typedef FNetBlobTestFixture Super;

protected:
	virtual void SetUp() override;

	virtual void TearDown() override;

	void RegisterNetBlobHandlers(FReplicationSystemTestNode* Node);

private:
	void AddNetBlobHandlerDefinitions();

protected:
	TStrongObjectPtr<UMockSequentialPartialNetBlobHandler> MockSequentialPartialNetBlobHandler;
	TStrongObjectPtr<UMockNetObjectAttachmentHandler> MockNetObjectAttachmentHandler;
	TStrongObjectPtr<UMockNetObjectAttachmentHandler> ClientMockNetObjectAttachmentHandler;
};

}
