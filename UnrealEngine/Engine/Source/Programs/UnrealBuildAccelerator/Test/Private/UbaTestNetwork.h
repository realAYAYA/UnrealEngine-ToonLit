// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaBinaryReaderWriter.h"
#include "UbaPlatform.h"
#include "UbaNetworkClient.h"
#include "UbaNetworkServer.h"
#include "UbaNetworkMessage.h"
#include "UbaNetworkBackendTcp.h"
#include "UbaNetworkBackendMemory.h"

namespace uba
{
	bool TestSockets(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
		static constexpr u16 port = 1346;
		Atomic<u8> result;
		NetworkBackendTcp tcp(logger.m_writer);
		Thread t([&]()
			{
				logger.Info(TC("Starting to Listen"));
				tcp.StartListen(logger, port, TC("127.0.0.1"), [&](void* connection, const sockaddr& remoteSocketAddr)
					{
						logger.Info(TC("Listen got connection"));
						tcp.SetDisconnectCallback(connection, nullptr, [](void*, const Guid&, void*) {});
						tcp.SetRecvCallbacks(connection, &result, 1,
							[](void* context, const Guid& connectionUid, u8* headerData, void*& outBodyContext, u8*& outBodyData, u32& outBodySize)
							{
								*(Atomic<u8>*)context = *headerData;
								//wprintf(TC("Listen got data from peer: %u\n"), *headerData);
								return true;
							},
							nullptr, TC(""));
						return true;
					});
				return 0;
			});

		Sleep(100);
		logger.Info(TC("Starting to Connect"));
		tcp.Connect(logger, TC("127.0.0.1"), [&](void* connection, const sockaddr& remoteSocketAddr, bool* timedOut)
			{
				tcp.SetDisconnectCallback(connection, nullptr, [](void*, const Guid&, void*) {});
				tcp.SetRecvCallbacks(connection, &tcp, 1, [](void* context, const Guid& connectionUid, u8* headerData, void*& outBodyContext, u8*& outBodyData, u32& outBodySize) { return true; }, nullptr, TC(""));
				u8 b = 42;
				NetworkBackend::SendContext sc;
				tcp.Send(logger, connection, &b, 1, sc);
				return true;
			}, port);

		Sleep(200);
		t.Wait();

		if (result != 42)
			return logger.Error(TC("Failed to receive data"));

		return true;
	}

	bool TestClientServer(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
		auto& logWriter = logger.m_writer;
		NetworkBackendTcp networkBackend(logWriter);

		bool ctorSuccess = true;
		NetworkServer server(ctorSuccess, logWriter);
		NetworkClient client(ctorSuccess, logWriter);

		server.RegisterService(1, [&](const ConnectionInfo& connectionInfo, MessageInfo& messageInfo, BinaryReader& reader, BinaryWriter& writer)
			{
				logger.Info(TC("Got ping!"));
				UBA_ASSERT(messageInfo.type == SessionMessageType_Ping);
				writer.WriteByte(42);
				return true;
			});

		auto ds = MakeGuard([&]() { server.DisconnectClients(); });
		if (!server.StartListen(networkBackend, 1234))
			return logger.Error(TC("Failed to listen"));
		Sleep(100);
		auto dg = MakeGuard([&]() { client.Disconnect(); });
		if (!client.Connect(networkBackend, TC("127.0.0.1"), 1234))
			return logger.Error(TC("Failed to connect"));


		StackBinaryWriter<128> writer;
		NetworkMessage msg(client, 1, SessionMessageType_Ping, writer);
		writer.WriteU32(32);
		StackBinaryReader<32> reader;
		if (!msg.Send(reader))
			return logger.Error(TC("Failed to get message"));

		u8 value = reader.ReadByte();
		logger.Info(TC("Got value %u"), value);
		return true;
	}

	bool TestClientServer2(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
		auto& logWriter = logger.m_writer;
		NetworkBackendTcp networkBackend(logWriter);

		bool ctorSuccess = true;
		NetworkServer server(ctorSuccess, logWriter);
		NetworkClient client(ctorSuccess, logWriter);

		server.RegisterService(1, [&](const ConnectionInfo& connectionInfo, MessageInfo& messageInfo, BinaryReader& reader, BinaryWriter& writer)
			{
				logger.Info(TC("Got ping!"));
				UBA_ASSERT(messageInfo.type == SessionMessageType_Ping);
				writer.WriteByte(42);
				return true;
			});

		if (!client.StartListen(networkBackend, 1239))
			return logger.Error(TC("Client failed to listen"));
		Sleep(100);
		auto ds = MakeGuard([&]() { server.DisconnectClients(); });
		auto dg = MakeGuard([&]() { client.Disconnect(); });
		if (!server.AddClient(networkBackend, TC("127.0.0.1"), 1239))
			return logger.Error(TC("Server failed to connect"));
		if (!server.AddClient(networkBackend, TC("127.0.0.1"), 1239))
			return logger.Error(TC("Server failed to connect second"));

		u64 time = GetTime();
		while (!client.GetConnectionCount())
		{
			if (TimeToMs(GetTime() - time) > 4000)
				return logger.Error(TC("Client failed to establish connection"));
			Sleep(100);
		}

		StackBinaryWriter<128> writer;
		NetworkMessage msg(client, 1, SessionMessageType_Ping, writer);
		writer.WriteU32(32);
		StackBinaryReader<32> reader;
		if (!msg.Send(reader))
			return logger.Error(TC("Failed to get message"));

		u8 value = reader.ReadByte();
		logger.Info(TC("Got value %u"), value);
		return true;
	}

	bool TestClientServerMem(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
		auto& logWriter = logger.m_writer;
		NetworkBackendMemory networkBackend(logWriter);

		bool ctorSuccess = true;
		NetworkServer server(ctorSuccess, logWriter);
		NetworkClient client(ctorSuccess, logWriter);

		server.RegisterService(1, [&](const ConnectionInfo& connectionInfo, MessageInfo& messageInfo, BinaryReader& reader, BinaryWriter& writer)
			{
				logger.Info(TC("Got ping!"));
				UBA_ASSERT(messageInfo.type == SessionMessageType_Ping);
				writer.WriteByte(42);
				return true;
			});

		auto ds = MakeGuard([&]() { server.DisconnectClients(); });
		if (!server.StartListen(networkBackend, 1234))
			return logger.Error(TC("Failed to listen"));
		Sleep(100);
		auto dg = MakeGuard([&]() { client.Disconnect(); });
		if (!client.Connect(networkBackend, TC("127.0.0.1"), 1234))
			return logger.Error(TC("Failed to connect"));


		StackBinaryWriter<128> writer;
		NetworkMessage msg(client, 1, SessionMessageType_Ping, writer);
		writer.WriteU32(32);
		StackBinaryReader<32> reader;
		if (!msg.Send(reader))
			return logger.Error(TC("Failed to get message"));

		u8 value = reader.ReadByte();
		logger.Info(TC("Got value %u"), value);
		return true;
	}
}
