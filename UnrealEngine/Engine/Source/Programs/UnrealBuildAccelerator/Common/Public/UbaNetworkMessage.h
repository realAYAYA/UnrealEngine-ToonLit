// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaNetworkBackend.h"
#include "UbaNetworkClient.h"

namespace uba
{
	struct NetworkMessage
	{
		NetworkMessage(NetworkClient& client, u8 serviceId, u8 messageType, BinaryWriter& sendWriter);
		NetworkMessage() {}
		~NetworkMessage();

		void Init(NetworkClient& client, u8 serviceId, u8 messageType, BinaryWriter& sendWriter);

		bool Send(BinaryReader& response);
		bool Send(BinaryReader& response, Timer& outTimer);
		bool Send();

		using DoneFunc = void(bool error, void* userData);
		bool SendAsync(BinaryReader& response, DoneFunc* func, void* userData); // Try to minimize work in GetResponseFunc since it is running on receiving thread
		bool ProcessAsyncResults(BinaryReader& response); // Note, this must be called after GetResponseFunc has been called

	private:
		void Done(bool shouldLock = true);

		NetworkClient* m_client = nullptr;
		BinaryWriter* m_sendWriter = nullptr;
		DoneFunc* m_doneFunc = nullptr;
		void* m_doneUserData = nullptr;
		void* m_response = nullptr;
		NetworkBackend::SendContext m_sendContext;
		u32 m_responseSize = 0;
		u32 m_responseCapacity = 0;
		u16 m_id = 0;
		Atomic<bool> m_error;
		NetworkClient::Connection* m_connection = nullptr;
		friend NetworkClient;
		NetworkMessage(const NetworkMessage&) = delete;
		NetworkMessage(NetworkMessage&&) = delete;
	};
}
