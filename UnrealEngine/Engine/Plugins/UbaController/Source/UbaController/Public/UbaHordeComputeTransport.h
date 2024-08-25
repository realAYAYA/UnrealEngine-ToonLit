// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Compute/ComputeTransport.h"
#include "UbaHordeMetaClient.h"

class FSocket;

class FUbaHordeComputeTransport : public FComputeTransport
{
public:
	// The constructor just performs a connect.
	FUbaHordeComputeTransport(const FHordeRemoteMachineInfo &Info, bool& InHasErrors);
	~FUbaHordeComputeTransport() override;
	
	// Sends data to the remote
	size_t Send(const void* Data, size_t Size) override;

	// Receives data from the remote
	size_t Recv(void* Data, size_t Size) override;

	// Indicates to the remote that no more data will be sent.
	void MarkComplete() override;

	// Indicates that no more data will be sent or received, and that any blocking reads/writes should stop.
	void Close() override;

	bool IsValid();

private:
	FSocket* Socket;
	bool bIsClosed;
	bool& bHasErrors;
};
