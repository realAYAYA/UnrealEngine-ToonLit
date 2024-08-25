// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaSessionClient.h"
#include "UbaApplicationRules.h"
#include "UbaNetworkClient.h"
#include "UbaNetworkMessage.h"
#include "UbaProcess.h"
#include "UbaProcessStartInfoHolder.h"
#include "UbaProtocol.h"
#include "UbaStorage.h"

namespace uba
{
	SessionClient::SessionClient(const SessionClientCreateInfo& info)
	: Session(info, TC("UbaSessionClient"), true, &info.client)
	,	m_client(info.client)
	,	m_name(info.name.data)
	,	m_terminationTime(~0ull)
	,	m_waitToSendEvent(false)
	,	m_loop(true)
	{
		m_maxProcessCount = info.maxProcessCount;
		m_dedicated = info.dedicated;
		m_useStorage = info.useStorage;
		m_defaultPriorityClass = info.defaultPriorityClass;
		m_outputStatsThresholdMs = info.outputStatsThresholdMs;
		m_maxIdleSeconds = info.maxIdleSeconds;
		m_disableCustomAllocator = info.disableCustomAllocator;
		m_useBinariesAsVersion = info.useBinariesAsVersion;
		m_memWaitLoadPercent = info.memWaitLoadPercent;
		m_memKillLoadPercent = info.memKillLoadPercent;
		m_processFinished = info.processFinished;

		if (m_name.IsEmpty())
		{
			tchar buf[256];
			if (GetComputerNameW(buf, sizeof_array(buf)))
				m_name.Appendf(TC("%s"), buf);
		}

		m_processWorkingDir.Append(m_rootDir).Append(TC("empty"));// + TC("empty\\");
		m_storage.CreateDirectory(m_processWorkingDir.data);
		m_processWorkingDir.EnsureEndsWithSlash();

		if (info.killRandom)
		{
			Guid g;
			CreateGuid(g);
			m_killRandomIndex = 10 + g.data1 % 30;
		}

		m_nameToHashTableMem.Init(NameToHashMemSize);

		Create(info);
	}

	SessionClient::~SessionClient()
	{
		Stop();
	}

	bool SessionClient::Start()
	{
		m_client.RegisterOnDisconnected([this]()
			{
				m_loop = false;
			});

		m_client.RegisterOnConnected([this]()
			{
				Connect();
			});
		return true;
	}

	void SessionClient::Stop()
	{
		CancelAllProcessesAndWait();
		m_loop = false;
		m_waitToSendEvent.Set();
		m_loopThread.Wait();
	}

	bool SessionClient::Wait(u32 milliseconds, Event* wakeupEvent)
	{
		return m_loopThread.Wait(milliseconds, wakeupEvent);
	}

	void SessionClient::SetIsTerminating(const tchar* reason, u64 delayMs)
	{
		m_terminationTime = GetTime() + MsToTime(delayMs);
		m_terminationReason = reason;

		StackBinaryWriter<1024> writer;
		NetworkMessage msg(m_client, ServiceId, SessionMessageType_Notification, writer);
		writer.WriteU32(m_sessionId);
		writer.WriteString(reason);
		msg.Send();
	}

	void SessionClient::SetMaxProcessCount(u32 count)
	{
		m_maxProcessCount = count;
	}

	u64 SessionClient::GetBestPing()
	{
		return m_bestPing;
	}

	bool SessionClient::RetrieveCasFile(CasKey& outNewKey, u64& outSize, const CasKey& casKey, const tchar* hint, bool storeUncompressed, bool allowProxy)
	{
		//StringBuffer<> workName;
		//u32 len = TStrlen(hint);
		//workName.Append(TC("RC:")).Append(len > 30 ? hint + (len - 30) : hint);
		//TrackWorkScope tws(m_client, workName.data);

		TimerScope s(m_stats.storageRetrieve);
		CasKey tempKey = casKey;

#if UBA_USE_SPARSEFILE
		storeUncompressed = false;
#endif

		if (storeUncompressed)
			tempKey = AsCompressed(casKey, false);
		//else
		//	storeUncompressed = !IsCompressed(casKey);

		Storage::RetrieveResult result;
		bool res = m_storage.RetrieveCasFile(result, tempKey, hint, nullptr, 1, allowProxy);
		outNewKey = result.casKey;
		outSize = result.size;
		return res;
	}

	bool SessionClient::GetCasKeyForFile(CasKey& out, u32 processId, const StringBufferBase& fileName, const StringKey& fileNameKey)
	{
		TimerScope waitTimer(Stats().waitGetFileMsg);
		SCOPED_WRITE_LOCK(m_nameToHashLookupLock, lock);
		auto insres = m_nameToHashLookup.try_emplace(fileNameKey);
		HashRec& rec = insres.first->second;
		lock.Leave();
		SCOPED_WRITE_LOCK(rec.lock, lock2);
		if (rec.key == CasKeyZero)//!rec.serverTime)
		{
			waitTimer.Cancel();

			// These will never succeed
			if (fileName.StartsWith(m_sessionBinDir.data) || fileName.StartsWith(TC("c:\\noenvironment")) || fileName.StartsWith(m_processWorkingDir.data))
			{
				out = CasKeyZero;
				return true;
			}

			StackBinaryWriter<1024> writer;
			NetworkMessage msg(m_client, ServiceId, SessionMessageType_GetFileFromServer, writer);
			writer.WriteU32(processId);
			writer.WriteString(fileName);
			writer.WriteStringKey(fileNameKey);

			StackBinaryReader<128> reader;
			if (!msg.Send(reader, Stats().getFileMsg))
				return false;

			rec.key = reader.ReadCasKey();
			rec.serverTime = reader.ReadU64();
		}
		out = rec.key;
		return true;
	}

	bool SessionClient::EnsureBinaryFile(StringBufferBase& out, StringBufferBase& outVirtual, u32 processId, const StringBufferBase& fileName, const StringKey& fileNameKey, const tchar* applicationDir)
	{
		CasKey casKey;
		u32 fileAttributes = DefaultAttributes(); // TODO: This is wrong.. need to retrieve from server if this is executable or not

		bool isAbsolute = IsWindows ? (fileName.count > 1 && fileName[1] == ':') : (fileName.count > 0 && fileName[0] == '/');
		if (isAbsolute)
		{
			UBA_ASSERT(fileNameKey != StringKeyZero);
			if (!GetCasKeyForFile(casKey, processId, fileName, fileNameKey))
				return false;
			// This needs to be absolute virtual path (the path on the host).. don't remember why this was written like this. Changed to just use fileName (needed for shadercompilerworker)
			//const tchar* lastSlash = TStrrchr(fileName.data, PathSeparator);
			//outVirtual.Append(lastSlash + 1);
			outVirtual.Append(fileName);
		}
		else
		{
			UBA_ASSERT(fileNameKey == StringKeyZero);
			StackBinaryWriter<1024> writer;
			NetworkMessage msg(m_client, ServiceId, SessionMessageType_EnsureBinaryFile, writer);
			writer.WriteU32(processId);
			writer.WriteString(fileName);
			writer.WriteStringKey(fileNameKey);
			writer.WriteString(applicationDir);

			StackBinaryReader<1024> reader;
			if (!msg.Send(reader, Stats().getBinaryMsg))
				return false;

			casKey = reader.ReadCasKey();
			reader.ReadString(outVirtual);
		}

		if (casKey == CasKeyZero)
		{
			out.Append(fileName);
			return true;
		}
		bool storeUncompressed = true;
		CasKey newKey;
		u64 fileSize;
		if (!RetrieveCasFile(newKey, fileSize, casKey, outVirtual.data, storeUncompressed))
			UBA_ASSERTF(false, TC("Casfile not found for %s using %s"), outVirtual.data, CasKeyString(casKey).str);
		StringBuffer<> destFile;
		if (isAbsolute || fileName.Contains(TC(".."))) // This is not beautiful, but we need to keep some dlls in the sub folder (for cl.exe etc)
			destFile.AppendFileName(fileName.data);
		else
			destFile.Append(fileName);

		StringBuffer<> applicationDirLower;
		applicationDirLower.Append(applicationDir).MakeLower();
		KeyToString keyStr(ToStringKey(applicationDirLower));

		return WriteBinFile(out, destFile.data, newKey, keyStr, fileAttributes);
	}

	bool SessionClient::PrepareProcess(const ProcessStartInfo& startInfo, bool isChild, StringBufferBase& outRealApplication, const tchar*& outRealWorkingDir)
	{
		outRealApplication.Clear();
		outRealWorkingDir = m_processWorkingDir.data;
		return EnsureApplicationEnvironment(outRealApplication, 0, startInfo.application);
	}

	struct SessionClient::ModuleInfo
	{
		ModuleInfo(const tchar* n, const CasKey& c, u32 a) : name(n), casKey(c), attributes(a), done(true) {}
		TString name;
		CasKey casKey;
		u32 attributes;
		Event done;
	};

	bool SessionClient::ReadModules(List<ModuleInfo>& outModules, u32 processId, const tchar* application)
	{
		StackBinaryReader<SendMaxSize> reader;
		{
			StackBinaryWriter<1024> writer;
			NetworkMessage msg(m_client, ServiceId, SessionMessageType_GetApplication, writer);
			writer.WriteU32(processId);
			writer.WriteString(application);
			if (!msg.Send(reader, m_stats.getApplicationMsg))
				return false;
		}

		u32 serverSystemPathLen = reader.ReadU32();
		u32 moduleCount = reader.ReadU32();
		if (moduleCount == 0)
			return m_logger.Error(TC("Application %s not found"), application);

		while (moduleCount--)
		{
			StringBuffer<> moduleFile;
			reader.ReadString(moduleFile);
			u32 fileAttributes = reader.ReadU32();
			bool isSystem = reader.ReadBool();

			CasKey casKey = reader.ReadCasKey();
			if (casKey == CasKeyZero)
				return m_logger.Error(TC("Bad CasKey for %s (%s)"), moduleFile.data, CasKeyString(casKey).str);

			if (isSystem)
			{
				StringBuffer<> localSystemModule;
				localSystemModule.Append(m_systemPath).Append(moduleFile.data + serverSystemPathLen);
				if (FileExists(m_logger, localSystemModule.data))
					continue;
				moduleFile.Clear().Append(localSystemModule);
			}
			outModules.emplace_back(moduleFile.data, casKey, fileAttributes);
		}

		return true;
	}

	bool SessionClient::EnsureApplicationEnvironment(StringBufferBase& out, u32 processId, const tchar* application)
	{
		StringBuffer<MaxPath> applicationDir;
		applicationDir.AppendDir(application);
		KeyToString keyStr(ToStringKeyLower(applicationDir));

		UBA_ASSERT(application && *application);
		SCOPED_WRITE_LOCK(m_handledApplicationEnvironmentsLock, lock);
		auto insres = m_handledApplicationEnvironments.insert(application);
		if (insres.second)
		{
			auto failGuard = MakeGuard([&]() { m_handledApplicationEnvironments.erase(insres.first); });

			List<ModuleInfo> modules;

			if (!ReadModules(modules, processId, application))
				return false;

			Atomic<bool> success = true;
			Atomic<u32> handledCount;
			for (auto& m : modules)
			{
				m_client.AddWork([&handledCount, &m, this, &success, &keyStr]()
					{
						++handledCount;
						auto g = MakeGuard([&]() { m.done.Set(); });
						CasKey newCasKey;
						bool storeUncompressed = true;
						u64 fileSize;
						const tchar* moduleName = m.name.c_str();
						if (!RetrieveCasFile(newCasKey, fileSize, m.casKey, moduleName, storeUncompressed))
						{
							m_logger.Error(TC("Casfile not found for %s (%s)"), moduleName, CasKeyString(m.casKey).str);
							success = false;
							return;
						}
						if (const tchar* lastSeparator = TStrrchr(moduleName, PathSeparator))
							moduleName = lastSeparator + 1;
						StringBuffer<MaxPath> temp;
						if (!WriteBinFile(temp, moduleName, newCasKey, keyStr, m.attributes))
							success = false;
					}, 1, TC("EnsureApp"));
			}

			while (handledCount < modules.size())
				m_client.DoWork();

			// Wait for all to be done
			for (auto& m : modules)
				if (!m.done.IsSet(10 * 60 * 1000)) // 10 minutes is a very long time
					return m_logger.Error(TC("Timed out while waiting for application cas files to be downloaded"));

			if (!success)
				return false;

			failGuard.Cancel();
		}
		out.Append(m_sessionBinDir).Append(keyStr).Append(PathSeparator).AppendFileName(application);
		return true;
	}

	void* SessionClient::GetProcessEnvironmentVariables()
	{
		UBA_ASSERT(!m_environmentVariables.empty());
		return m_environmentVariables.data();
	}

	bool SessionClient::WriteBinFile(StringBufferBase& out, const tchar* binaryName, const CasKey& casKey, const KeyToString& applicationDir, u32 fileAttributes)
	{
		UBA_ASSERT(fileAttributes);

		out.Append(m_sessionBinDir);
		out.Append(applicationDir).Append(PathSeparator);
		
		StringBuffer<> lower;
		lower.Append(applicationDir).Append(PathSeparator).Append(binaryName);
		lower.MakeLower();
		SCOPED_WRITE_LOCK(m_binFileLock, lock);

		auto insres = m_writtenBinFiles.try_emplace(lower.data, casKey);
		if (!insres.second)
		{
			out.Append(binaryName);
			if (insres.first->second != casKey)
				return m_logger.Error(TC("Writing same binary file %s multiple times but with different data! (Current: %s Previous: %s)"), out.data, CasKeyString(casKey).str, CasKeyString(insres.first->second).str);
			return true;
		}

		m_storage.CreateDirectory(out.data);
		out.Append(binaryName);

		//if (GetFileAttributesW(out.data) != INVALID_FILE_ATTRIBUTES)
		//	return true;

		if (TStrchr(binaryName, PathSeparator))
		{
			StringBuffer<> binaryDir;
			binaryDir.AppendDir(out);
			if (!m_storage.CreateDirectory(binaryDir.data))
				return false;
		}

		return m_storage.CopyOrLink(casKey, out.data, fileAttributes);
	}

	bool SessionClient::CreateFile(CreateFileResponse& out, const CreateFileMessage& msg)
	{
		const StringBufferBase& fileName = msg.fileName;
		StringKey fileNameKey = msg.fileNameKey;

		if ((msg.access & FileAccess_Write) != 0)
			return Session::CreateFile(out, msg);
	
		CasKey casKey;
		if (!GetCasKeyForFile(casKey, msg.process.m_id, fileName, fileNameKey))
			return false;

		// Not finding a file is a valid path. Some applications try with a path and if fails try another path
		if (casKey == CasKeyZero)
		{
			//m_logger.Warning(TC("No casfile found for %s"), fileName.data);
			out.directoryTableSize = GetDirectoryTableSize();
			out.mappedFileTableSize = GetFileMappingSize();
			out.fileName.Append(fileName);
			return true;
		}

		StringBuffer<> newName;
		bool isDir = casKey == CasKeyIsDirectory;
		u64 fileSize = InvalidValue;
		CasKey newCasKey;

		u32 memoryMapAlignment = 0;
		if (m_allowMemoryMaps)
		{
			memoryMapAlignment = GetMemoryMapAlignment(fileName.data, fileName.count);
			if (!memoryMapAlignment && !m_useStorage)
				memoryMapAlignment = 64 * 1024;
		}

		if (isDir)
		{
			newName.Append(TC("$d"));
		}
		else if (casKey != CasKeyZero)
		{
			if (m_useStorage || memoryMapAlignment == 0)
			{
				bool storeUncompressed = memoryMapAlignment == 0;
				bool allowProxy = GetApplicationRules()[msg.process.m_rulesIndex].rules->AllowStorageProxy(fileName);
				if (!RetrieveCasFile(newCasKey, fileSize, casKey, fileName.data, storeUncompressed, allowProxy))
					return m_logger.Error(TC("Error retrieving cas entry %s (%s)"), CasKeyString(casKey).str, fileName.data);

				#if !UBA_USE_SPARSEFILE
				if (!m_storage.GetCasFileName(newName, newCasKey))
					return false;
				#else
				if (!memoryMapAlignment)
					memoryMapAlignment = 4096;
				MemoryMap map;
				if (!CreateMemoryMapFromView(map, fileNameKey, fileName.data, newCasKey, memoryMapAlignment))
					return false;
				newName.Append(map.name);
				fileSize = map.size;
				#endif
			}
			else
			{
				StorageStats& stats = m_storage.Stats();
				TimerScope ts(stats.ensureCas);

				SCOPED_WRITE_LOCK(m_fileMappingTableLookupLock, lookupLock);
				auto insres = m_fileMappingTableLookup.try_emplace(fileNameKey);
				FileMappingEntry& entry = insres.first->second;
				lookupLock.Leave();

				SCOPED_WRITE_LOCK(entry.lock, entryCs);
				ts.Leave();

				if (entry.handled)
				{
					if (!entry.success)
						return false;
				}
				else
				{
					TimerScope s(m_stats.storageRetrieve);
					casKey = AsCompressed(casKey, false);
					entry.handled = true;
					Storage::RetrieveResult result;
					bool allowProxy = GetApplicationRules()[msg.process.m_rulesIndex].rules->AllowStorageProxy(fileName);
					if (!m_storage.RetrieveCasFile(result, casKey, fileName.data, &m_fileMappingBuffer, memoryMapAlignment, allowProxy))
						return m_logger.Error(TC("Error retrieving cas entry %s (%s)"), CasKeyString(casKey).str, fileName.data);
					entry.success = true;
					entry.size = result.size;
					entry.mapping = result.view.handle;
					entry.mappingOffset = result.view.offset;
				}

				fileSize = entry.size;
				if (entry.mapping.IsValid())
					Storage::GetMappingString(newName, entry.mapping, entry.mappingOffset);
				else
					newName.Append(entry.isDir ? TC("$d") : TC("$f"));
			}
		}

		UBA_ASSERTF(!newName.IsEmpty(), TC("No casfile available for %s using %s"), fileName.data, CasKeyString(casKey).str);

		if (newName[0] != '^')
		{
			if (!isDir && memoryMapAlignment)
			{
				MemoryMap map;
				if (!CreateMemoryMapFromFile(map, fileNameKey, newName.data, IsCompressed(newCasKey), memoryMapAlignment))
					return false;

				fileSize = map.size;
				newName.Clear().Append(map.name);
			}
			else if (!IsRarelyRead(msg.process, fileName))
			{
				AddFileMapping(fileNameKey, fileName.data, newName.data, fileSize);
			}
		}

		out.directoryTableSize = GetDirectoryTableSize();
		out.mappedFileTableSize = GetFileMappingSize();
		out.fileName.Append(newName);
		out.size = fileSize;
		return true;
	}

	bool SessionClient::SendFiles(ProcessImpl& process, Timer& sendFiles)
	{
		StorageStatsScope storageStatsScope(process.m_storageStats);
		for (auto& pair : process.m_writtenFiles)
		{
			TimerScope timer(sendFiles);
#ifdef _DEBUG
			if (!pair.second.mappingHandle.IsValid())
				m_logger.Warning(TC("%s is not using file mapping"), pair.first.c_str());
#endif
			bool keepMappingInMemory = IsWindows && !IsRarelyReadAfterWritten(process, pair.first.c_str(), pair.first.size());
			if (!SendFile(pair.second, pair.first.c_str(), process.GetId(), keepMappingInMemory))
				return false;
		}
		return true;
	}

	bool SessionClient::SendFile(WrittenFile& source, const tchar* destination, u32 processId, bool keepMappingInMemory)
	{
		CasKey casKey;
		{
			TimerScope ts(m_stats.storageSend);
			bool deferCreation = false;
			if (!m_storage.StoreCasFile(casKey, source.key, source.name.c_str(), source.mappingHandle, 0, source.mappingWritten, destination, deferCreation, keepMappingInMemory))
				return false;
		}
		UBA_ASSERTF(casKey != CasKeyZero, TC("Failed to store cas file for %s (destination %s)"), source.name.c_str(), destination);
	
		CloseFileMapping(source.mappingHandle);
		source.mappingHandle = {};

		StackBinaryReader<128> reader;
		{
			StackBinaryWriter<1024> writer;
			NetworkMessage msg(m_client, ServiceId, SessionMessageType_SendFileToServer, writer);
			writer.WriteU32(processId);
			writer.WriteString(destination);
			writer.WriteStringKey(source.key);
			writer.WriteU32(source.attributes);
			writer.WriteCasKey(casKey);
			if (!msg.Send(reader, Stats().sendFileMsg))
				return m_logger.Error(TC("Failed to send file %s to server"), source.name.c_str());
		}
		if (!reader.ReadBool())
			return m_logger.Error(TC("Server failed to receive file %s"), source.name.c_str());
		return true;
	}

	void RemoveWrittenFile(ProcessImpl& process, const TString& name);

	bool SessionClient::DeleteFile(DeleteFileResponse& out, const DeleteFileMessage& msg)
	{
		// TODO: Deleting output files should also delete them on disk (for now they will leak until process shutdown)
		RemoveWrittenFile(msg.process, msg.fileName.data);

		bool sendDelete = true;
		if (msg.closeId != 0)
		{
			UBA_ASSERTF(false, TC("This has not been tested properly"));
			SCOPED_WRITE_LOCK(m_activeFilesLock, lock);
			sendDelete = m_activeFiles.erase(msg.closeId) == 0;
		}

		{
			SCOPED_WRITE_LOCK(m_outputFilesLock, lock);
			sendDelete = m_outputFiles.erase(msg.fileName.data) == 0 && sendDelete;
		}

		bool isTemp = StartsWith(msg.fileName.data, m_tempPath.data);
		if (isTemp)
			sendDelete = false;

		if (!sendDelete)
		{
			if (!m_allowMemoryMaps && isTemp)
			{
				out.result = uba::DeleteFileW(msg.fileName.data);
				out.errorCode = GetLastError();
				return true;
			}
			out.result = true;
			out.errorCode = ERROR_SUCCESS;
			return true;
		}

		// TODO: Cache this if it becomes noisy

		StackBinaryWriter<1024> writer;
		NetworkMessage networkMsg(m_client, ServiceId, SessionMessageType_DeleteFile, writer);
		writer.WriteStringKey(msg.fileNameKey);
		writer.WriteString(msg.fileName);
		StackBinaryReader<SendMaxSize> reader;
		if (!networkMsg.Send(reader, Stats().deleteFileMsg))
			return false;
		out.result = reader.ReadBool();
		out.errorCode = reader.ReadU32();
		if (out.result)
		{
			reader.Reset();
			if (!SendUpdateDirectoryTable(reader))
				return false;
		}
		out.directoryTableSize = GetDirectoryTableSize();
		return true;
	}

	bool SessionClient::CopyFile(CopyFileResponse& out, const CopyFileMessage& msg)
	{
		SCOPED_WRITE_LOCK(m_outputFilesLock, lock);
		auto findIt = m_outputFiles.find(msg.fromName.data);
		if (findIt == m_outputFiles.end())
		{
			lock.Leave();

			StackBinaryWriter<1024> writer;
			NetworkMessage networkMsg(m_client, ServiceId, SessionMessageType_CopyFile, writer);
			writer.WriteStringKey(msg.fromKey);
			writer.WriteString(msg.fromName);
			writer.WriteStringKey(msg.toKey);
			writer.WriteString(msg.toName);
			StackBinaryReader<SendMaxSize> reader;
			if (!networkMsg.Send(reader, Stats().copyFileMsg))
				return false;
			out.fromName.Append(msg.fromName);
			out.toName.Append(msg.toName);
			out.closeId = ~0u;
			out.errorCode = reader.ReadU32();
			if (!out.errorCode)
			{
				reader.Reset();
				if (!SendUpdateDirectoryTable(reader))
					return false;
			}
			out.directoryTableSize = GetDirectoryTableSize();
			return true;
		}
		lock.Leave();

		out.fromName.Append(findIt->second);

		CreateFileMessage writeMsg { msg.process };
		writeMsg.fileName.Append(msg.toName.data);
		writeMsg.fileNameKey = msg.toKey;
		writeMsg.access = FileAccess_Write;
		CreateFileResponse writeOut;
		if (!CreateFile(writeOut, writeMsg))
			return false;

		out.toName.Append(writeOut.fileName);
		out.closeId = writeOut.closeId;
		return true;
	}

	bool SessionClient::MoveFile(MoveFileResponse& out, const MoveFileMessage& msg)
	{
		const tchar* fromName = msg.fromName.data;
		const tchar* toName = msg.toName.data;

		{
			SCOPED_WRITE_LOCK(msg.process.m_writtenFilesLock, lock);
			auto& writtenFiles = msg.process.m_writtenFiles;
			auto findIt = writtenFiles.find(fromName);
			if (findIt != writtenFiles.end())
			{
				auto insres = writtenFiles.try_emplace(toName);
				UBA_ASSERTF(insres.second, TC("Moving written file to other written file."));
				insres.first->second = findIt->second;
				insres.first->second.owner = &msg.process;
				writtenFiles.erase(findIt);
			}
		}

		bool sendMove = true;
		{
			SCOPED_WRITE_LOCK(m_outputFilesLock, lock);
			auto findIt = m_outputFiles.find(fromName);
			if (findIt != m_outputFiles.end())
			{
				auto insres = m_outputFiles.try_emplace(toName);
				UBA_ASSERTF(insres.second, TC("Failed to add move destination file %s as output file because it is already added."), toName);
				insres.first->second = findIt->second;
				m_outputFiles.erase(findIt);
				sendMove = false;
			}
		}

		if (!sendMove)
		{
			out.result = true;
			out.errorCode = ERROR_SUCCESS;
			return true;
		}

		out.result = uba::MoveFileExW(fromName, toName, 0);
		out.errorCode = GetLastError();

		return true;
	}

	bool SessionClient::Chmod(ChmodResponse& out, const ChmodMessage& msg)
	{
		const tchar* fromName = msg.fileName.data;

		{
			SCOPED_WRITE_LOCK(msg.process.m_writtenFilesLock, lock);
			auto& writtenFiles = msg.process.m_writtenFiles;
			auto findIt = writtenFiles.find(fromName);
			if (findIt != writtenFiles.end())
			{
				bool executable = false;
				#if !PLATFORM_WINDOWS
				if (msg.fileMode & S_IXUSR)
					executable = true;
				#endif
				findIt->second.attributes = DefaultAttributes(executable);
				out.errorCode = 0;
				return true;
			}
		}

		UBA_ASSERTF(false, TC("Code path not implemented.. should likely send message to server"));
		return true;
	}

	bool SessionClient::CreateDirectory(CreateDirectoryResponse& out, const CreateDirectoryMessage& msg)
	{
		// TODO: Cache this if it becomes noisy

		StackBinaryWriter<1024> writer;
		NetworkMessage networkMsg(m_client, ServiceId, SessionMessageType_CreateDirectory, writer);
		writer.WriteString(msg.name);
		StackBinaryReader<1024> reader;
		if (!networkMsg.Send(reader, Stats().createDirMsg))
			return false;
		out.result = reader.ReadBool();
		out.errorCode = reader.ReadU32();
		return true;
	}

	bool SessionClient::GetFullFileName(GetFullFileNameResponse& out, const GetFullFileNameMessage& msg)
	{
		// TODO: There is a potential risk here where two different applications asks for the full name of a file
		// and they have different bin/working dir.. and there are two versions of this file.

		SCOPED_WRITE_LOCK(m_nameToNameLookupLock, lock);
		auto insres = m_nameToNameLookup.try_emplace(msg.fileName.data);
		NameRec& rec = insres.first->second;
		lock.Leave();
		SCOPED_WRITE_LOCK(rec.lock, lock2);

		if (rec.handled)
		{
			out.fileName.Append(rec.name.c_str());
			out.virtualFileName.Append(rec.virtualName.c_str());
			return true;
		}
		rec.handled = true;

		auto& dir = msg.process.m_virtualApplicationDir;
		if (!EnsureBinaryFile(out.fileName, out.virtualFileName, msg.process.m_id, msg.fileName, msg.fileNameKey, dir.c_str()))
			return false;

		StringKey fileNameKey = msg.fileNameKey;
		if (fileNameKey == StringKeyZero)
			fileNameKey =  CaseInsensitiveFs ? ToStringKeyLower(out.virtualFileName) : ToStringKey(out.virtualFileName);

		rec.name = out.fileName.data;
		rec.virtualName = out.virtualFileName.data;
		out.mappedFileTableSize = AddFileMapping(fileNameKey, msg.fileName.data, out.fileName.data);
		return true;
	}

	bool SessionClient::GetListDirectoryInfo(ListDirectoryResponse& out, tchar* dirName, const StringKey& dirKey)
	{
		StackBinaryWriter<1024> writer;
		NetworkMessage msg(m_client, ServiceId, SessionMessageType_ListDirectory, writer);
		writer.WriteU32(m_sessionId);
		writer.WriteString(dirName);
		writer.WriteStringKey(dirKey);
		
		StackBinaryReader<SendMaxSize> reader;

		if (!msg.Send(reader, Stats().listDirMsg))
			return false;

		u32 tableOffset = reader.ReadU32();

		u32 oldTableSize = GetDirectoryTableSize();
		if (!UpdateDirectoryTableFromServer(reader))
			return false;
		u32 newTableSize = GetDirectoryTableSize();

		// Ask for a refresh of hashes straight away since they will likely be asked for by the process doing this query
		if (oldTableSize != newTableSize)
			m_waitToSendEvent.Set();

		out.tableOffset = tableOffset;
		out.tableSize = newTableSize;

		return true;
	}

	bool SessionClient::WriteFilesToDisk(ProcessImpl& process, WrittenFile** files, u32 fileCount)
	{
		// Do nothing, we will send the data to the host when process is finished
		return true;
	}


	struct SessionClient::ActiveUpdateDirectoryEntry
	{
		Event done;
		u32 readPos = 0;
		ActiveUpdateDirectoryEntry* prev = nullptr;
		ActiveUpdateDirectoryEntry* next = nullptr;

		static bool Wait(SessionClient& client, ActiveUpdateDirectoryEntry*& first, ScopedWriteLock& lock, u32 readPos)
		{
			ActiveUpdateDirectoryEntry item;
			item.next = first;
			if (item.next)
				item.next->prev = &item;
			item.readPos = readPos;
			first = &item;
			item.done.Create(true);
			lock.Leave();
			while (!item.done.IsSet(10000))
			{
				client.m_logger.Error(TC("Timed out waiting for update directory message"));
				return false;
			}
			lock.Enter();
			if (item.prev)
				item.prev->next = item.next;
			else
				first = item.next;
			if (item.next)
				item.next->prev = item.prev;
			return true;
		}

		static void UpdateReadPosMatching(ActiveUpdateDirectoryEntry*& first, u32 readPos)
		{
			for (auto i = first; i; i = i->next)
			{
				if (i->readPos != readPos)
					continue;
				i->done.Set();
				break;
			}
		}

		static void UpdateReadPosLess(ActiveUpdateDirectoryEntry*& first, u32 readPos)
		{
			for (auto i = first; i; i = i->next)
				if (i->readPos <= readPos)
					i->done.Set();
		}
	};

	bool SessionClient::UpdateDirectoryTableFromServer(StackBinaryReader<SendMaxSize>& reader)
	{
		auto& dirTable = m_directoryTable;

		bool isFirst = true;
		while (true)
		{
			if (!isFirst)
			{
				reader.Reset();

				StackBinaryWriter<1024> writer;
				NetworkMessage msg(m_client, ServiceId, SessionMessageType_GetDirectoriesFromServer, writer);
				writer.WriteU32(m_sessionId);
				if (!msg.Send(reader, Stats().getDirsMsg))
					return false;
			}

			u32 readPos = reader.ReadU32();

			u8* pos = dirTable.m_memory + readPos;
			u32 toRead = u32(reader.GetLeft());

			SCOPED_WRITE_LOCK(m_directoryTableLock, lock);

			if (toRead == 0)
			{
				// We might share this position with others
				if (readPos > dirTable.m_memorySize)
					if (!ActiveUpdateDirectoryEntry::Wait(*this, m_firstEmptyWait, lock, readPos))
						return false;
				return true;
			}

			reader.ReadBytes(pos, toRead);

			if (readPos != m_directoryTableMemPos)
				if (!ActiveUpdateDirectoryEntry::Wait(*this, m_firstReadWait, lock, readPos))
					return false;

			m_directoryTableMemPos += toRead;
			
			ActiveUpdateDirectoryEntry::UpdateReadPosMatching(m_firstReadWait, m_directoryTableMemPos);

			if (reader.GetPosition() < m_client.GetMessageMaxSize() - m_client.GetMessageReceiveHeaderSize())
			{
				//dirTable.ParseDirectoryTable(m_directoryTableMemPos); // This is not needed.. we never read from the directory table in the client session
				{
					SCOPED_WRITE_LOCK(dirTable.m_memoryLock, lock2);
					dirTable.m_memorySize = m_directoryTableMemPos;
				}
				ActiveUpdateDirectoryEntry::UpdateReadPosLess(m_firstEmptyWait, m_directoryTableMemPos);
				break;
			}
			isFirst = false;
		}

		return true;
	}

	bool SessionClient::UpdateNameToHashTableFromServer(StackBinaryReader<SendMaxSize>& reader)
	{
		u32 serverTableSize = 0;
		bool isFirst = true;
		u32 readStartPos = u32(m_nameToHashTableMem.writtenSize);
		u32 localTableSize = readStartPos;
		u64 serverTime = 0;
		while (true)
		{
			if (isFirst)
			{
				serverTableSize = reader.ReadU32();
				isFirst = false;
			}
			else
			{
				StackBinaryWriter<1024> writer;
				NetworkMessage msg(m_client, ServiceId, SessionMessageType_GetNameToHashFromServer, writer);
				writer.WriteU32(serverTableSize);
				writer.WriteU32(localTableSize);

				reader.Reset();
				if (!msg.Send(reader, Stats().getHashesMsg))
					return false;
			}
			serverTime = reader.ReadU64();

			u8* pos = m_nameToHashTableMem.memory + localTableSize;

			u32 left = u32(reader.GetLeft());
			u32 toRead = serverTableSize - localTableSize;

			bool needMore = left < toRead;
			if (needMore)
				toRead = left;

			m_nameToHashTableMem.AllocateNoLock(toRead, 1, TC("NameToHashTable"));
			reader.ReadBytes(pos, toRead);
			localTableSize += toRead;

			if (!needMore)
				break;
		}

		u32 addCount = 0;
		BinaryReader r(m_nameToHashTableMem.memory, readStartPos, NameToHashMemSize);
		SCOPED_WRITE_LOCK(m_nameToHashLookupLock, lock);
		while (r.GetPosition() < localTableSize)
		{
			StringKey name = r.ReadStringKey();
			CasKey hash = r.ReadCasKey();

			HashRec& rec = m_nameToHashLookup[name];
			SCOPED_WRITE_LOCK(rec.lock, lock2);
			if (serverTime < rec.serverTime)
				continue;
			rec.key = hash;
			rec.serverTime = serverTime;
			++addCount;
		}

		return true;
	}

	void SessionClient::Connect()
	{
		StackBinaryWriter<1024> writer;
		NetworkMessage msg(m_client, ServiceId, SessionMessageType_Connect, writer);
		writer.WriteString(m_name.data);
		writer.WriteU32(SessionNetworkVersion);

		CasKey keys[2];
		if (m_useBinariesAsVersion)
		{
			StringBuffer<> dir;
			GetDirectoryOfCurrentModule(m_logger, dir);
			u64 dirCount = dir.count;
			dir.Append(PathSeparator).Append(UBA_AGENT_EXECUTABLE);
			m_storage.CalculateCasKey(keys[0], dir.data);
			dir.Resize(dirCount).Append(PathSeparator).Append(UBA_DETOURS_LIBRARY);
			m_storage.CalculateCasKey(keys[1], dir.data);
		}

		writer.WriteCasKey(keys[0]);
		writer.WriteCasKey(keys[1]);

		writer.WriteU32(m_maxProcessCount);
		writer.WriteBool(m_dedicated);

		StringBuffer<> info;
		GetSystemInfo(info);
		writer.WriteString(info);

		StackBinaryReader<SendMaxSize> reader;

		if (!msg.Send(reader, m_stats.connectMsg))
			return;

		m_connected = reader.ReadBool();
		if (!m_connected)
		{
			StringBuffer<> str;
			reader.ReadString(str);
			m_logger.Error(str.data);

			CasKey exeKey = reader.ReadCasKey();
			CasKey dllKey = reader.ReadCasKey();
			m_client.InvokeVersionMismatch(exeKey, dllKey);
			return;
		}

		{
			CasKey detoursBinaryKey = reader.ReadCasKey();
			{
				TimerScope s(m_stats.storageRetrieve);
				Storage::RetrieveResult result;
				if (!m_storage.RetrieveCasFile(result, AsCompressed(detoursBinaryKey, false), UBA_DETOURS_LIBRARY))
					return;
			}
			KeyToString dir(StringKeyZero);
			StringBuffer<> detoursFile;
			if (!WriteBinFile(detoursFile, UBA_DETOURS_LIBRARY, detoursBinaryKey, dir, DefaultAttributes()))
				return;

			#if PLATFORM_WINDOWS
			char dll[1024];
			sprintf_s(dll, sizeof(dll), "%ls", detoursFile.data);
			m_detoursLibrary = dll;
			#else
			m_detoursLibrary = detoursFile.data;
			#endif
		}

		bool resetCas = reader.ReadBool();
		if (resetCas)
			m_storage.Reset();

		m_sessionId = reader.ReadU32();
		m_uiLanguage = reader.ReadU32();
		m_detailedTrace = reader.ReadBool();
		m_shouldSendLogToServer = reader.ReadBool();
		m_shouldSendTraceToServer = reader.ReadBool();

		if (m_shouldSendLogToServer)
			m_logToFile = true;

		if (m_shouldSendTraceToServer)
		{
			//if (m_detailedTrace)
			{
				m_client.SetWorkTracker(&m_trace);
			}
			StartTrace(nullptr);
		}

		BuildEnvironmentVariables(reader);

		m_loop = true;
		m_loopThread.Start([this]() { ThreadCreateProcessLoop(); return 0; });
	}

	void SessionClient::BuildEnvironmentVariables(BinaryReader& reader)
	{
		TString tempStr;
		while (true)
		{
			tempStr = reader.ReadString();
			if (tempStr.empty())
				break;
			m_environmentVariables.insert(m_environmentVariables.end(), tempStr.begin(), tempStr.end());
			m_environmentVariables.push_back(0);
		}

		#if PLATFORM_WINDOWS
		AddEnvironmentVariableNoLock(TC("Path"), TC("c:\\noenvironment"));
		AddEnvironmentVariableNoLock(TC("TEMP"), m_tempPath.data);
		AddEnvironmentVariableNoLock(TC("TMP"), m_tempPath.data);
		#else
		AddEnvironmentVariableNoLock(TC("TMPDIR"), m_tempPath.data);
		#endif

		StringBuffer<> v;
		for (auto& var : m_localEnvironmentVariables)
			if (GetEnvironmentVariableW(var, v.data, v.capacity))
				AddEnvironmentVariableNoLock(var, v.data);

		m_environmentVariables.push_back(0);
	}

	struct SessionClient::InternalProcessStartInfo : ProcessStartInfoHolder
	{
		u32 processId = 0;
	};


	bool SessionClient::SendProcessAvailable(Vector<InternalProcessStartInfo>& out, float availableWeight)
	{
		StackBinaryWriter<32> writer;
		NetworkMessage msg(m_client, ServiceId, SessionMessageType_ProcessAvailable, writer);
		writer.WriteU32(m_sessionId);
		writer.WriteU32(*(u32*)&availableWeight);

		StackBinaryReader<SendMaxSize> reader;
		if (!msg.Send(reader, m_stats.procAvailableMsg))
		{
			if (m_loop)
				m_logger.Error(TC("Failed to send ProcessAvailable message"));
			return false;
		}
		while (true)
		{
			u32 processId = reader.ReadU32();
			if (processId == 0)
				break;
			if (processId == SessionProcessAvailableResponse_Disconnect)
			{
				m_logger.Info(TC("Got disconnect request from host"));
				return false;
			}
			if (processId == SessionProcessAvailableResponse_RemoteExecutionDisabled)
			{
				m_remoteExecutionEnabled = false;
				break;
			}
			out.push_back({});
			InternalProcessStartInfo& info = out.back();
			info.processId = processId;
			info.Read(reader);
		}

		u32 neededDirectoryTableSize = reader.ReadU32();
		u32 neededHashTableSize = reader.ReadU32();

		if (u32 knownInputsCount = reader.ReadU32())
		{
			while (knownInputsCount--)
			{
				CasKey knownInputKey = reader.ReadCasKey();
				u32 mappingAlignment = reader.ReadU32();
				bool storeUncompressed = !m_allowMemoryMaps || mappingAlignment == 0;
				if (storeUncompressed)
					knownInputKey = AsCompressed(knownInputKey, false);

				m_client.AddWork([knownInputKey, this]()
					{
						Storage::RetrieveResult result;
						bool allowProxy = true;
						bool res = m_storage.RetrieveCasFile(result, knownInputKey, TC("KnownInput"), nullptr, 1, allowProxy);
						(void)res;
					}, 1, TC("KnownInput"));
			}
		}

		if (!out.empty())
		{
			if (neededDirectoryTableSize > GetDirectoryTableSize())
			{
				reader.Reset();
				if (!SendUpdateDirectoryTable(reader))
					return false;
			}
		}

		// Always nice to update name-to-hash table since it can reduce number of messages while building.
		u32 hashTableMemSize;
		{
			SCOPED_READ_LOCK(m_nameToHashMemLock, l);
			hashTableMemSize = u32(m_nameToHashTableMem.writtenSize);
		}
		if (neededHashTableSize > hashTableMemSize)
		{
			reader.Reset();
			if (!SendUpdateNameToHashTable(reader))
				return false;
		}

		return true;
	}

	void SessionClient::SendReturnProcess(u32 processId, const tchar* reason)
	{
		StackBinaryWriter<1024> writer;
		NetworkMessage msg(m_client, ServiceId, SessionMessageType_ProcessReturned, writer);
		writer.WriteU32(processId);
		writer.WriteString(reason);
		StackBinaryReader<32> reader;
		if (!msg.Send(reader, m_stats.procReturnedMsg))
			return;
	}

	bool SessionClient::SendUpdateDirectoryTable(StackBinaryReader<SendMaxSize>& reader)
	{
		UBA_ASSERT(reader.GetPosition() == 0);
		StackBinaryWriter<32> writer;
		NetworkMessage msg(m_client, ServiceId, SessionMessageType_GetDirectoriesFromServer, writer);
		writer.WriteU32(m_sessionId);
		writer.WriteU32(~u32(0));
		if (!msg.Send(reader, Stats().getDirsMsg))
			return false;
		return UpdateDirectoryTableFromServer(reader);
	}

	bool SessionClient::SendUpdateNameToHashTable(StackBinaryReader<SendMaxSize>& reader)
	{
		StackBinaryWriter<32> writer;
		NetworkMessage msg(m_client, ServiceId, SessionMessageType_GetNameToHashFromServer, writer);
		writer.WriteU32(~u32(0));

		SCOPED_WRITE_LOCK(m_nameToHashMemLock, lock);
		writer.WriteU32(u32(m_nameToHashTableMem.writtenSize));

		if (!msg.Send(reader, Stats().getHashesMsg))
			return false;
		return UpdateNameToHashTableFromServer(reader);
	}

	void SessionClient::SendPing(u64 memAvail, u64 memTotal)
	{
		u64 time = GetTime();
		if (TimeToMs(time - m_lastPingSendTime) < 2000) // Ping every ~2 seconds... this is so server can disconnect a client quickly if no ping is coming
			return;

		float cpuLoad = UpdateCpuLoad();

		StackBinaryWriter<128> writer;
		NetworkMessage msg(m_client, ServiceId, SessionMessageType_Ping, writer);
		writer.WriteU32(m_sessionId);
		writer.WriteU64(m_lastPing);
		writer.WriteU64(memAvail);
		writer.WriteU64(memTotal);
		writer.WriteU32(*(u32*)&cpuLoad);
		StackBinaryReader<32> reader;
		time = GetTime();
		if (!msg.Send(reader, m_stats.pingMsg))
			m_loop = false;
		u64 newTime = GetTime();
		m_lastPing = newTime - time;
		m_lastPingSendTime = newTime;

		if (m_lastPing < m_bestPing || m_bestPing == 0)
			m_bestPing = m_lastPing;

		m_storage.Ping();

		if (reader.ReadBool()) // abort
			abort();
	}

	void SessionClient::SendSummary(const Function<void(Logger&)>& extraInfo)
	{
		StackBinaryWriter<SendMaxSize> writer;
		NetworkMessage msg(m_client, ServiceId, SessionMessageType_Summary, writer);

		writer.WriteU32(m_sessionId);

		WriteSummary(writer, [&](Logger& logger)
			{
				PrintSummary(logger);
				m_storage.PrintSummary(logger);
				m_client.PrintSummary(logger);
				SystemStats::GetGlobal().Print(logger, true);
				if (extraInfo)
					extraInfo(logger);
			});

		msg.Send();
	}

	void SessionClient::SendLogFileToServer(ProcessImpl& pi)
	{
		auto logFile = pi.m_startInfo.logFile;
		if (!logFile || !*logFile)
			return;
		WrittenFile f;
		f.name = logFile;
		f.attributes = DefaultAttributes();
		StringBuffer<> dest;
		if (const tchar* lastSlash = TStrrchr(logFile, PathSeparator))
			logFile = lastSlash + 1;
		dest.Append(TC("<log>")).Append(logFile);
		f.key = ToStringKeyLower(dest);
		SendFile(f, dest.data, pi.GetId(), false);
	}

	void SessionClient::GetLogFileName(StringBufferBase& out, const tchar* logFile, const tchar* arguments)
	{
		out.Append(m_sessionLogDir.data);
		if (logFile && *logFile)
		{
			if (const tchar* lastSeparator = TStrrchr(logFile, PathSeparator))
				logFile = lastSeparator + 1;
			out.Append(logFile);
		}
		else
		{
			GetNameFromArguments(out, arguments, true);
			out.Append(TC(".log"));
		}
	}

	void SessionClient::ThreadCreateProcessLoop()
	{
		struct ProcessRec
		{
			ProcessRec(ProcessImpl* impl) : handle(impl) {}
			ProcessHandle handle;
			ReaderWriterLock lock;
			Atomic<bool> isKilled;
			Atomic<bool> isDone;
			float weight = 1.0f;
		};
		List<ProcessRec> activeProcesses;

		u64 lastWaitTime = 0;
		u64 waitForMemoryPressureStartTime = 0;

		constexpr u64 waitTimeToSpawnAfterKillMs = 5 * 1000;

		u64 memAvail;
		u64 memTotal;
		GetMemoryInfo(memAvail, memTotal);

		SendPing(memAvail, memTotal);

		u64 memRequiredToSpawn = u64(double(memTotal) * double(100 - m_memWaitLoadPercent) / 100.0);
		u64 memRequiredFree = u64(double(memTotal) * double(100 - m_memKillLoadPercent) / 100.0);

		float activeWeight = 0;
		ReaderWriterLock activeWeightLock;

		u64 idleStartTime = GetTime();
		u32 processRequestCount = 0;

		auto RemoveInactiveProcesses = [&]()
		{
			for (auto it=activeProcesses.begin();it!=activeProcesses.end();)
			{
				ProcessRec& r = *it;
				if (!r.isDone)
				{
					++it;
					continue;
				}
				r.lock.EnterWrite();
				r.lock.LeaveWrite();
				it = activeProcesses.erase(it);
			}

			if (m_remoteExecutionEnabled && m_terminationReason)
			{
				m_remoteExecutionEnabled = false;
				m_logger.Info(TC("%s. Will stop scheduling processes and send failing processes back for retry"), m_terminationReason.load());
			}

			if (!activeProcesses.empty())
			{
				idleStartTime = GetTime();
				processRequestCount = 0;
			}
			else if (m_remoteExecutionEnabled)
			{
				u32 idleTime = u32(TimeToS(GetTime() - idleStartTime));
				if (idleTime > m_maxIdleSeconds)
				{
					m_logger.Info(TC("Session has been idle longer than max idle time (%u seconds). Disconnecting (Did %u process requests during idle)"), m_maxIdleSeconds, processRequestCount);
					m_waitToSendEvent.Set();
					m_remoteExecutionEnabled = false;
				}
			}
		};


		while (m_loop)
		{
			float maxWeight = float(m_maxProcessCount);
			u32 waitTimeoutMs = 3000;

			FlushDeadProcesses();

			GetMemoryInfo(memAvail, memTotal);
			if (memAvail < memRequiredFree)
			{
				for (auto it = activeProcesses.rbegin(); it != activeProcesses.rend(); ++it)
				{
					ProcessRec& rec = *it;
					if (rec.isKilled || rec.isDone)
						continue;
					SCOPED_WRITE_LOCK(rec.lock, lock);
					rec.handle.Cancel(true);
					rec.isKilled = true;
					SendReturnProcess(rec.handle.GetId(), TC("Running out of memory"));
					++m_stats.killCount;
					m_logger.Warning(TC("Killed process due to memory pressure (Available: %s Total: %s)"), BytesToText(memAvail).str, BytesToText(memTotal).str);
					break;
				}
				lastWaitTime = GetTime();
			}

			bool canSpawn = TimeToMs(GetTime() - lastWaitTime) > waitTimeToSpawnAfterKillMs;
			if (!canSpawn)
				waitTimeoutMs = 500;

			bool firstCall = true;

			while (m_remoteExecutionEnabled && canSpawn && m_loop)
			{
				float availableWeight;
				{
					SCOPED_READ_LOCK(activeWeightLock, lock);
					if (activeWeight >= maxWeight)
						break;
					availableWeight = maxWeight - activeWeight;
				}

				if (!firstCall)
					GetMemoryInfo(memAvail, memTotal);

				if (memAvail < memRequiredToSpawn)
				{
					if (waitForMemoryPressureStartTime == 0)
					{
						m_logger.Info(TC("Delaying spawn due to memory pressure (Available: %s Total: %s)"), BytesToText(memAvail).str, BytesToText(memTotal).str);
						waitForMemoryPressureStartTime = GetTime();
					}
					break;
				}

				if (waitForMemoryPressureStartTime)
				{
					u64 waitTime = GetTime() - waitForMemoryPressureStartTime;
					m_logger.Info(TC("Waited %s for memory pressure to go down (Available: %s Total: %s)"), TimeToText(waitTime).str, BytesToText(memAvail).str, BytesToText(memTotal).str);
					m_stats.waitMemPressure += waitTime;
					waitForMemoryPressureStartTime = 0;
					lastWaitTime = GetTime();
					waitTimeoutMs = 200;
					availableWeight = Min(availableWeight, 1.0f);
				}

				Vector<InternalProcessStartInfo> startInfos;
				if (!SendProcessAvailable(startInfos, availableWeight))
				{
					m_loop = false;
					break;
				}
				++processRequestCount;

				if (!m_remoteExecutionEnabled)
				{
					m_logger.Info(TC("Got remote execution disabled response from host (will finish %llu active processes)"), startInfos.size() + activeProcesses.size());
				}

				if (startInfos.empty())
				{
					canSpawn = false;
					waitTimeoutMs = 200;
				}

				for (InternalProcessStartInfo& info : startInfos)
				{
					auto& startInfo = info.startInfo;
					startInfo.uiLanguage = int(m_uiLanguage);
					startInfo.priorityClass = m_defaultPriorityClass;
					startInfo.useCustomAllocator = !m_disableCustomAllocator;
					startInfo.outputStatsThresholdMs = m_outputStatsThresholdMs != 0 ? m_outputStatsThresholdMs : startInfo.outputStatsThresholdMs;

					StringBuffer<> logFile;
					if (m_logToFile)
					{
						GetLogFileName(logFile, startInfo.logFile, startInfo.arguments);
						startInfo.logFile = logFile.data;
					}

					StringBuffer<> realApplication;
					if (!EnsureApplicationEnvironment(realApplication, info.processId, startInfo.application))
					{
						m_logger.Error(TC("Failed to ensure application environment for %s"), startInfo.application);
						SendReturnProcess(info.processId, TC("Failed to ensure application environment"));
						m_loop = false;
						break;
					}

					void* env = GetProcessEnvironmentVariables();

					auto process = new ProcessImpl(*this, info.processId, nullptr);

					activeProcesses.emplace_back(process);
					ProcessRec* rec = &activeProcesses.back();

					rec->weight = info.weight;

					{
						SCOPED_WRITE_LOCK(activeWeightLock, lock);
						activeWeight += rec->weight;
					}

					struct ExitedRec
					{
						ExitedRec(SessionClient& s, ReaderWriterLock& l, float& w, ProcessRec* r) : session(s), activeWeightLock(l), activeWeight(w), rec(r) {}
						SessionClient& session;
						ReaderWriterLock& activeWeightLock;
						float& activeWeight;
						ProcessRec* rec;
					};

					ExitedRec* exitedRec = new ExitedRec(*this, activeWeightLock, activeWeight, rec);
					startInfo.userData = exitedRec;
					startInfo.exitedFunc = [](void* userData, const ProcessHandle& h)
					{
						auto er = (ExitedRec*)userData;
						SessionClient& session = er->session;
						ReaderWriterLock& activeWeightLock = er->activeWeightLock;
						float& activeWeight = er->activeWeight;
						ProcessRec* rec = er->rec;
						delete er;

						auto& startInfo = h.GetStartInfo();
						if (session.m_shouldSendLogToServer)
							session.SendLogFileToServer(*(ProcessImpl*)h.m_process);

						float weight = rec->weight;
						auto decreaseWeight = MakeGuard([&]()
							{
								SCOPED_WRITE_LOCK(activeWeightLock, weightLock);
								activeWeight -= weight;
								session.m_waitToSendEvent.Set();
							});

						SCOPED_WRITE_LOCK(rec->lock, lock);
						auto doneGuard = MakeGuard([&]() { rec->isDone = true; session.m_waitToSendEvent.Set(); });

						if (rec->isKilled)
							return;

						auto& process = *(ProcessImpl*)h.m_process;

						if (session.m_killRandomIndex != ~0u && session.m_killRandomCounter++ == session.m_killRandomIndex)
						{
							session.m_loop = false;
							session.m_logger.Info(TC("Killed random process (%s)"), process.m_description.c_str());
							return;
						}

						u32 exitCode = process.m_exitCode;

						if (exitCode != 0)
						{
							if (GetTime() >= session.m_terminationTime)
							{
								if (session.m_loop)
									session.SendReturnProcess(rec->handle.GetId(), session.m_terminationReason);
								return;
							}
						}

						if (exitCode == 0 || startInfo.writeOutputFilesOnFail)
						{
							// Should we decrease weight before or after sending files?
							//decreaseWeight.Execute();

							if (!session.SendFiles(process, process.m_processStats.sendFiles))
							{
								const tchar* desc = TC("Failed to send output files to host");
								session.m_logger.Error(desc);
								if (session.m_loop)
									session.SendReturnProcess(rec->handle.GetId(), desc);
								return;
							}
						}

						decreaseWeight.Execute();

						if (process.IsCancelled())
						{
							if (session.m_loop)
								session.SendReturnProcess(rec->handle.GetId(), TC("Cancelled"));
							return;
						}

						StackBinaryWriter<SendMaxSize> writer;
						NetworkMessage msg(session.m_client, ServiceId, SessionMessageType_ProcessFinished, writer);
						writer.WriteU32(process.m_id);
						writer.WriteU32(exitCode);
						writer.WriteU32(session.CountLogLines(process));
						session.WriteLogLines(writer, process);

						// Must be written last
						process.m_processStats.Write(writer);
						process.m_sessionStats.Write(writer);
						process.m_storageStats.Write(writer);
						process.m_systemStats.Write(writer);

						StackBinaryReader<16> reader;
						if (!msg.Send(reader, session.m_stats.procFinishedMsg) && session.m_loop)
							session.m_logger.Error(TC("Failed to send ProcessFinished message!"));

						// TODO: These should be removed and instead added in TraceReader (so it will update over time)
						session.m_stats.stats.Add(process.m_sessionStats);
						session.m_storage.AddStats(process.m_storageStats);

						if (session.m_processFinished)
							session.m_processFinished(&process);
					};

					process->Start(startInfo, realApplication.data, m_processWorkingDir.data, true, env, true, true);
				}

				RemoveInactiveProcesses();

				firstCall = false;
			}

			SendPing(memAvail, memTotal);

			//SendUpdateNameToHashTable(); // It is always nice to populate this at certain cadence since it might speed up running processes queries

			m_waitToSendEvent.IsSet(waitTimeoutMs);

			RemoveInactiveProcesses();

			if (activeProcesses.empty() && !m_remoteExecutionEnabled)
				break;
		}

		CancelAllProcessesAndWait(); // If we got the exit from server there is no point sending anything more back.. cancel everything

		u32 retry = 0;
		while (true)
		{
			if (retry++ == 100)
			{
				m_logger.Error(TC("This should never happen!"));
				break;
			}
			RemoveInactiveProcesses();
			if (activeProcesses.empty())
				break;
			m_waitToSendEvent.IsSet(100);
		};


		m_client.FlushWork();


		if (m_shouldSendTraceToServer)
		{
			m_client.SetWorkTracker(nullptr);

			StopTraceThread();

			StackBinaryWriter<SendMaxSize> writer;
			WriteSummary(writer, [&](Logger& logger)
				{
					PrintSummary(logger);
					m_storage.PrintSummary(logger);
					m_client.PrintSummary(logger);
					SystemStats::GetGlobal().Print(logger, true);
				});
			m_trace.SessionSummary(0, writer.GetData(), writer.GetPosition());

			StringBuffer<> ubaFile(m_sessionLogDir);
			ubaFile.Append(TC("Trace.uba"));
			if (StopTrace(ubaFile.data))
			{
				WrittenFile f;
				f.name = ubaFile.data;
				f.attributes = DefaultAttributes();
				StringBuffer<> dest(TC("<uba>"));
				f.key = ToStringKeyLower(dest);

				SendFile(f, dest.data, 0, false);
			}
		}
	}

	u32 SessionClient::CountLogLines(ProcessImpl& process)
	{
		u32 count = u32(process.m_logLines.size());
		for (auto& child : process.m_childProcesses)
			count += CountLogLines(*(ProcessImpl*)child.m_process);
		return count;
	}

	void SessionClient::WriteLogLines(BinaryWriter& writer, ProcessImpl& process)
	{
		for (auto& child : process.m_childProcesses)
			WriteLogLines(writer, *(ProcessImpl*)child.m_process);
		for (auto& line : process.m_logLines)
		{
			writer.WriteString(line.text);
			writer.WriteByte(line.type);
		}
	}

	bool SessionClient::AllocFailed(Process& process, const tchar* allocType, u32 error)
	{
		//StackBinaryWriter<32> writer;
		//NetworkMessage msg(m_client, ServiceId, SessionMessageType_VirtualAllocFailed, writer);
		//if (!msg.Send())
		//	m_logger.Error(TC("Failed to send VirtualAllocFailed message!"));
		return Session::AllocFailed(process, allocType, error);
	}

	void SessionClient::PrintSessionStats(Logger& logger)
	{
		Session::PrintSessionStats(logger);
	}

	bool SessionClient::GetNextProcess(Process& process, bool& outNewProcess, NextProcessInfo& outNextProcess, u32 prevExitCode, BinaryReader& statsReader)
	{
		outNewProcess = false;

		if (!m_remoteExecutionEnabled)
			return true;

		auto& pi = (ProcessImpl&)process;

		if (!FlushWrittenFiles(pi))
			return false;

		ProcessStats processStats;
		processStats.Read(statsReader, TraceVersion);
		processStats.sendFiles = pi.m_processStats.sendFiles;

		StackBinaryReader<SendMaxSize> reader;
		StackBinaryWriter<16 * 1024> writer;
		NetworkMessage msg(m_client, ServiceId, SessionMessageType_GetNextProcess, writer);
		writer.WriteU32(pi.m_id);
		writer.WriteU32(prevExitCode);
		processStats.Write(writer);
		writer.WriteBytes(statsReader.GetPositionData(), statsReader.GetLeft());

		if (!msg.Send(reader, m_stats.customMsg))
			return false;

		outNewProcess = reader.ReadBool();
		if (outNewProcess)
		{
			if (m_shouldSendLogToServer)
				SendLogFileToServer(pi);

			pi.m_exitCode = prevExitCode;
			if (m_processFinished)
				m_processFinished(&process);

			pi.m_exitCode = ~0u;
			pi.m_processStats = {};
			pi.m_sessionStats = {};
			pi.m_storageStats = {};
			pi.m_systemStats = {};

			outNextProcess.arguments = reader.ReadString();
			outNextProcess.workingDir = reader.ReadString();
			outNextProcess.description = reader.ReadString();
			outNextProcess.logFile = reader.ReadString();

			if (m_logToFile)
			{
				StringBuffer<512> logFile;
				GetLogFileName(logFile, outNextProcess.logFile.c_str(), outNextProcess.arguments.c_str());
				outNextProcess.logFile = logFile.data;
			}

			// TODO: Probably need to fill up with more stuff.. this is fine for current usecase
			pi.m_arguments = outNextProcess.arguments;
			pi.m_description = outNextProcess.description;
			pi.m_logFile = outNextProcess.logFile;

			pi.m_startInfo.arguments = pi.m_arguments.c_str();
			pi.m_startInfo.description = pi.m_description.c_str();
			pi.m_startInfo.logFile = pi.m_logFile.c_str();
		}

		reader.Reset();
		return SendUpdateDirectoryTable(reader);
	}

	bool SessionClient::CustomMessage(Process& process, BinaryReader& reader, BinaryWriter& writer)
	{
		StackBinaryWriter<SendMaxSize> msgWriter;
		NetworkMessage msg(m_client, ServiceId, SessionMessageType_Custom, msgWriter);

		u32 recvSize = reader.ReadU32();
		msgWriter.WriteU32(process.GetId());
		msgWriter.WriteU32(recvSize);
		msgWriter.WriteBytes(reader.GetPositionData(), recvSize);

		BinaryReader msgReader(writer.GetData(), 0);
		if (!msg.Send(msgReader, m_stats.customMsg))
			return false;

		u32 responseSize = msgReader.ReadU32();
		writer.AllocWrite(4ull + responseSize);
		return true;
	}

	bool SessionClient::FlushWrittenFiles(ProcessImpl& process)
	{
		SCOPED_WRITE_LOCK(process.m_writtenFilesLock, lock);
		if (!SendFiles(process, process.m_processStats.sendFiles))
			return false;
		{
			SCOPED_WRITE_LOCK(m_outputFilesLock, lock2);
			for (auto& kv : process.m_writtenFiles)
				m_outputFiles.erase(kv.first);
		}
		process.m_writtenFiles.clear();
		return true;
	}

	bool SessionClient::UpdateEnvironment(ProcessImpl& process, const tchar* reason, bool resetStats)
	{
		StackBinaryReader<SendMaxSize> reader;

		if (resetStats)
		{
			StackBinaryWriter<16 * 1024> writer;
			NetworkMessage msg(m_client, ServiceId, SessionMessageType_UpdateEnvironment, writer);
			writer.WriteU32(process.m_id);
			writer.WriteString(reason);
			process.m_processStats.Write(writer);
			process.m_sessionStats.Write(writer);
			process.m_storageStats.Write(writer);
			process.m_systemStats.Write(writer);

			process.m_processStats = {};
			process.m_sessionStats = {};
			process.m_storageStats = {};
			process.m_systemStats = {};

			if (!msg.Send(reader, m_stats.customMsg))
				return false;
			reader.Reset();
		}
		return SendUpdateDirectoryTable(reader);
	}

	void SessionClient::TraceSessionUpdate()
	{
		float cpuLoad = 0.0f;//UpdateCpuLoad();
		u64 send;
		u64 recv;
		if (auto backend = m_client.GetFirstConnectionBackend())
		{
			backend->GetTotalSendAndRecv(send, recv);
		}
		else
		{
			recv = m_client.GetTotalRecvBytes();
			send = m_client.GetTotalSentBytes();
		}
		u64 memAvail = 0;//m_memAvail;
		u64 memTotal = 0;//m_memTotal;

		// send and recv are swapped on purpose because that is how visualizer is visualizing 
		m_trace.SessionUpdate(0, 0, send, recv, 0, memAvail, memTotal, cpuLoad);
	}
}
