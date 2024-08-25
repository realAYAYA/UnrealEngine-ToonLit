// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaProtocol.h"
#include "UbaProcessStats.h"
#include "UbaDetoursPayload.h"
#include "UbaDetoursShared.h"
#include "UbaDetoursFunctionsWin.h"
#include <detours/detours.h>
#include <stdio.h>

namespace uba
{
	HANDLE g_hostProcess;
	HANDLE g_writeEvent;
	HANDLE g_readEvent;
	HANDLE g_cancelEvent;
	HANDLE g_messageMappingHandle;
	u8* g_readMessageMappingMem;
	u8* g_writeMessageMappingMem;

	bool WasCancelled()
	{
		return WaitForSingleObject(g_cancelEvent, 0) != WAIT_TIMEOUT;
	}

	int Connect(DetoursPayload& payload)
	{
		g_hostProcess = payload.hostProcess;

		if (!DuplicateHandle(g_hostProcess, payload.cancelEvent, GetCurrentProcess(), &g_cancelEvent, SYNCHRONIZE, false, 0))
		{
			UBA_ASSERT(!WasCancelled());
			return -4;
		}

		// Sync primitive for writing message to host process
		if (!DuplicateHandle(g_hostProcess, payload.readEvent, GetCurrentProcess(), &g_writeEvent, EVENT_MODIFY_STATE, false, 0))
			return -2;

		// Sync primitive for reading messages from host process
		if (!DuplicateHandle(g_hostProcess, payload.writeEvent, GetCurrentProcess(), &g_readEvent, SYNCHRONIZE, false, 0))
			return -3;

		if (!DuplicateHandle(g_hostProcess, payload.communicationHandle, GetCurrentProcess(), &g_messageMappingHandle, FILE_MAP_READ | FILE_MAP_WRITE, false, 0))
		{
			UBA_ASSERT(!WasCancelled());
			return -4;
		}

		u8* mem = (u8*)::MapViewOfFile(g_messageMappingHandle, FILE_MAP_READ | FILE_MAP_WRITE, ToHigh(payload.communicationOffset), ToLow(payload.communicationOffset), CommunicationMemSize);
		g_readMessageMappingMem = mem;
		g_writeMessageMappingMem = mem;// +CommunicationMemSize / 2;

		return 0;
	}

	int Disconnect()
	{
		UnmapViewOfFile(g_writeMessageMappingMem);
		CloseHandle(g_messageMappingHandle);
		CloseHandle(g_cancelEvent);
		CloseHandle(g_readEvent);
		CloseHandle(g_writeEvent);

		CloseHandle(g_hostProcess);
		return 0;
	}

	ANALYSIS_NORETURN void TerminateCurrentProcess(u32 exitCode)
	{
		if (True_TerminateProcess)
			True_TerminateProcess(GetCurrentProcess(), exitCode);
		else
			TerminateProcess(GetCurrentProcess(), exitCode);
	}

	BinaryWriter::BinaryWriter()
	{
		m_begin = g_writeMessageMappingMem;
		m_pos = m_begin;
		m_end = m_begin + CommunicationMemSize;// / 2;
	}

	void BinaryWriter::Flush(bool waitOnResponse)
	{
		if (!waitOnResponse)
		{
			SetEvent(g_writeEvent);
			return;
		}

		TimerScope ts(g_stats.waitOnResponse);
		DWORD res = SignalObjectAndWait(g_writeEvent, g_readEvent, 1000, FALSE);

		do
		{
			if (res == WAIT_OBJECT_0)
				break;

			if (res != WAIT_TIMEOUT)
				TerminateCurrentProcess(1337);

			DWORD exitCode;
			if (!GetExitCodeProcess(g_hostProcess, &exitCode))
				TerminateCurrentProcess(1338);

			if (exitCode != STILL_ACTIVE)
				TerminateCurrentProcess(1339);

			DWORD cancelRes = WaitForSingleObject(g_cancelEvent, 0);
			if (cancelRes == WAIT_OBJECT_0)
				ExitProcess(1339);
			else if (cancelRes != WAIT_TIMEOUT)
				TerminateCurrentProcess(1353);

			res = WaitForSingleObject(g_readEvent, 500);

		} while (true);
	}

	BinaryReader::BinaryReader()
	{
		m_begin = g_readMessageMappingMem;
		m_pos = m_begin;
		m_end = m_begin + CommunicationMemSize;// / 2;
	}
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved)
{
	using namespace uba;

	static GROUP_AFFINITY GroupAffinity;

	u64 startTime = GetTime();

	if (DetourIsHelperProcess())
		return TRUE;

	if (dwReason == DLL_PROCESS_ATTACH)
	{
		GetThreadGroupAffinity(GetCurrentThread(), &GroupAffinity);

		if (!DetourRestoreAfterWith())
			TerminateCurrentProcess(1344);

		DetoursPayload* payload = (DetoursPayload*)DetourFindPayloadEx(DetoursPayloadGuid, nullptr);
		if (!payload)
			TerminateCurrentProcess(1342);
		if (payload->version != ProcessMessageVersion)
			TerminateCurrentProcess(1354);

		PreInit(*payload);

		int result = Connect(*payload);
		if (result != 0)
		{
			Disconnect();
			TerminateCurrentProcess(result);
		}

		Init(*payload, startTime);
	}
	else if (dwReason == DLL_THREAD_ATTACH)
	{
		SetThreadGroupAffinity(GetCurrentThread(), &GroupAffinity, NULL);
	}
	else if (dwReason == DLL_PROCESS_DETACH)
	{
		Deinit(startTime);
		Disconnect();
		PostDeinit();
	}

	return TRUE;
}
