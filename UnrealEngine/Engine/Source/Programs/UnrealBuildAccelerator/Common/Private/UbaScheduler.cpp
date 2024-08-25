// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaScheduler.h"
#include "UbaProcess.h"
#include "UbaProcessStartInfoHolder.h"
#include "UbaSessionServer.h"
#include "UbaStringBuffer.h"

namespace uba
{
	struct Scheduler::ProcessStartInfo2 : ProcessStartInfoHolder
	{
		ProcessStartInfo2(const ProcessStartInfo& si, const u8* ki, u32 kic)
		:	ProcessStartInfoHolder(si)
		, knownInputs(ki)
		, knownInputsCount(kic)
		{
		}

		~ProcessStartInfo2()
		{
			delete[] knownInputs;
		}

		const u8* knownInputs;
		u32 knownInputsCount;
		float weight = 1.0f;
	};


	struct Scheduler::ExitProcessInfo
	{
		Scheduler* scheduler = nullptr;
		ProcessStartInfo2* startInfo;
		u32 processIndex = ~0u;
		bool wasReturned = false;
		bool isLocal = true;
	};


	class SkippedProcess : public Process
	{
	public:
		SkippedProcess(const ProcessStartInfo& i) : holder(i) {}
		virtual u32 GetExitCode() override { return ProcessCancelExitCode; }
		virtual bool HasExited() override { return true; }
		virtual bool WaitForExit(u32 millisecondsTimeout) override{ return true; }
		virtual const ProcessStartInfo& GetStartInfo() override { return holder.startInfo; }
		virtual const Vector<ProcessLogLine>& GetLogLines() override { static Vector<ProcessLogLine> v{ProcessLogLine{TC("Skipped"), LogEntryType_Warning}}; return v; }
		virtual const Vector<u8>& GetTrackedInputs() override { static Vector<u8> v; return v;}
		virtual bool IsRemote() const override { return false; }
		virtual bool IsDetoured() const { return false; }
		ProcessStartInfoHolder holder;
	};

	
	Scheduler::Scheduler(const SchedulerCreateInfo& info)
	:	m_session(info.session)
	,	m_maxLocalProcessors(info.maxLocalProcessors != ~0u ? info.maxLocalProcessors : GetLogicalProcessorCount())
	,	m_updateThreadLoop(false)
	,	m_enableProcessReuse(info.enableProcessReuse)
	,	m_forceRemote(info.forceRemote)
	,	m_forceNative(info.forceNative)
	{
		m_session.RegisterGetNextProcess([this](Process& process, NextProcessInfo& outNextProcess, u32 prevExitCode)
			{
				return HandleReuseMessage(process, outNextProcess, prevExitCode);
			});
	}

	Scheduler::~Scheduler()
	{
		Stop();
	}

	void Scheduler::Start()
	{
		m_session.SetRemoteProcessReturnedEvent([this](Process& process) { RemoteProcessReturned(process); });
		m_session.SetRemoteProcessSlotAvailableEvent([this]() { RemoteSlotAvailable(); });

		m_loop = true;
		m_thread.Start([this]() { ThreadLoop(); return 0; });
	}

	void Scheduler::Stop()
	{
		m_loop = false;
		m_updateThreadLoop.Set();
		m_thread.Wait();
		m_session.WaitOnAllTasks();

		SCOPED_WRITE_LOCK(m_processEntriesLock, lock);
		for (auto& entry : m_processEntries)
		{
			UBA_ASSERTF(entry.status !=ProcessStatus_Running, TC("Found processes in running state when stopping scheduler."));
			delete[] entry.dependencies;
			delete entry.info;
		}
		m_processEntries.clear();
		m_processEntriesStart = 0;
	}

	void Scheduler::SetMaxLocalProcessors(u32 maxLocalProcessors)
	{
		m_maxLocalProcessors = maxLocalProcessors;
		m_updateThreadLoop.Set();
	}

	u32 Scheduler::EnqueueProcess(const EnqueueProcessInfo& info)
	{
		u8* ki = nullptr;
		if (info.knownInputsCount)
		{
			ki = new u8[info.knownInputsBytes];
			memcpy(ki, info.knownInputs, info.knownInputsBytes);
		}

		u32* dep = nullptr;
		if (info.dependencyCount)
		{
			dep = new u32[info.dependencyCount];
			memcpy(dep, info.dependencies, info.dependencyCount*sizeof(u32));
		}

		auto info2 = new ProcessStartInfo2(info.info, ki, info.knownInputsCount);
		info2->weight = info.weight;

		SCOPED_WRITE_LOCK(m_processEntriesLock, lock);
		u32 index = u32(m_processEntries.size());
		auto& entry = m_processEntries.emplace_back();
		entry.info = info2;
		entry.dependencies = dep;
		entry.dependencyCount = info.dependencyCount;
		entry.status = ProcessStatus_Queued;
		entry.canDetour = info.canDetour;
		entry.canExecuteRemotely = info.canExecuteRemotely && info.canDetour;
		lock.Leave();

		UpdateQueueCounter(1);

		m_updateThreadLoop.Set();
		return index;
	}

	void Scheduler::GetStats(u32& outQueued, u32& outActiveLocal, u32& outActiveRemote, u32& outFinished)
	{
		outActiveLocal = m_activeLocalProcesses;
		outActiveRemote = m_activeRemoteProcesses;
		outFinished = m_finishedProcesses;
		outQueued = m_queuedProcesses;
	}

	void Scheduler::SetProcessFinishedCallback(const Function<void(const ProcessHandle&)>& processFinished)
	{
		m_processFinished = processFinished;
	}

	void Scheduler::ThreadLoop()
	{
		while (m_loop)
		{
			if (!m_updateThreadLoop.IsSet())
				break;

			while (RunQueuedProcess(true))
				;
		}
	}

	void Scheduler::RemoteProcessReturned(Process& process)
	{
		auto& ei = *(ExitProcessInfo*)process.GetStartInfo().userData;

		ei.wasReturned = true;
		u32 processIndex = ei.processIndex;

		process.Cancel(true); // Cancel will call ProcessExited
		
		if (processIndex == ~0u)
			return;

		SCOPED_WRITE_LOCK(m_processEntriesLock, lock);
		if (m_processEntries[processIndex].status != ProcessStatus_Running)
			return;
		m_processEntries[processIndex].status = ProcessStatus_Queued;
		m_processEntriesStart = Min(m_processEntriesStart, processIndex);
		lock.Leave();

		UpdateQueueCounter(1);
		UpdateActiveProcessCounter(false, -1);
		m_updateThreadLoop.Set();
	}

	void Scheduler::RemoteSlotAvailable()
	{
		RunQueuedProcess(false);
	}

	void Scheduler::ProcessExited(ExitProcessInfo* info, const ProcessHandle& handle)
	{
		auto ig = MakeGuard([info]() { delete info; });

		if (info->wasReturned)
			return;

		auto si = info->startInfo;
		if (!si) // Can be a process that was reused but didn't get a new process
		{
			UBA_ASSERT(info->processIndex == ~0u);
			return;
		}

		ExitProcess(*info, *handle.m_process, handle.m_process->GetExitCode());
	}

	u32 Scheduler::PopProcess(bool isLocal)
	{
		if (isLocal)
			if (m_activeLocalProcessWeight >= float(m_maxLocalProcessors))
				return ~0u;

		auto processEntries = m_processEntries.data();
		bool allFinished = true;

		for (u32 i=m_processEntriesStart, e=u32(m_processEntries.size()); i!=e; ++i)
		{
			auto& entry = processEntries[i];
			auto status = entry.status;
			if (status != ProcessStatus_Queued)
			{
				if (allFinished)
				{
					if (status != ProcessStatus_Running)
						m_processEntriesStart = i;
					else
						allFinished = false;
				}
				continue;
			}
			allFinished = false;

			if (!isLocal && !entry.canExecuteRemotely)
				continue;

			if (isLocal && m_forceRemote && entry.canExecuteRemotely)
				continue;

			bool canRun = true;
			for (u32 j=0, je=entry.dependencyCount; j!=je; ++j)
			{
				auto depIndex = entry.dependencies[j];
				UBA_ASSERTF(depIndex < m_processEntries.size(), TC("Found dependency on index %u but there are only %u processes registered"), depIndex, u32(m_processEntries.size()));
				auto depStatus = processEntries[depIndex].status;
				if (depStatus == ProcessStatus_Failed || depStatus == ProcessStatus_Skipped)
				{
					entry.status = ProcessStatus_Skipped;
					return i;
				}
				if (depStatus != ProcessStatus_Success)
				{
					canRun = false;
					break;
				}
			}

			if (!canRun)
				continue;

			if (isLocal)
				m_activeLocalProcessWeight += entry.info->weight;

			entry.status = ProcessStatus_Running;
			return i;
		}
		return ~0u;
	}


	bool Scheduler::RunQueuedProcess(bool isLocal)
	{
		while (true)
		{
			SCOPED_WRITE_LOCK(m_processEntriesLock, lock);
			u32 indexToRun = PopProcess(isLocal);
			if (indexToRun == ~0u)
				return false;

			auto& processEntry = m_processEntries[indexToRun];
			auto info = processEntry.info;
			bool canDetour = processEntry.canDetour && !m_forceNative;
			bool wasSkipped = processEntry.status == ProcessStatus_Skipped;
			lock.Leave();

			UpdateQueueCounter(-1);

			if (wasSkipped)
			{
				SkipProcess(*info);
				continue;
			}

			UpdateActiveProcessCounter(isLocal, 1);
	
			auto exitInfo = new ExitProcessInfo();
			exitInfo->scheduler = this;
			exitInfo->startInfo = info;
			exitInfo->isLocal = isLocal;
			exitInfo->processIndex = indexToRun;

			ProcessStartInfo si = info->startInfo;
			si.userData = exitInfo;
			si.exitedFunc = [](void* userData, const ProcessHandle& handle)
				{
					auto ei = (ExitProcessInfo*)userData;
					ei->scheduler->ProcessExited(ei, handle);
				};

			if (isLocal)
				m_session.RunProcess(si, true, canDetour);
			else
				m_session.RunProcessRemote(si, 1.0f, info->knownInputs, info->knownInputsCount);
			return true;
		}
	}

	bool Scheduler::HandleReuseMessage(Process& process, NextProcessInfo& outNextProcess, u32 prevExitCode)
	{
		if (!m_enableProcessReuse)
			return false;

		auto& currentStartInfo = process.GetStartInfo();
		auto ei = (ExitProcessInfo*)currentStartInfo.userData;
		if (!ei) // If null, process has already exited from some other thread
			return false;

		ExitProcess(*ei, process, prevExitCode);

		ei->startInfo = nullptr;
		ei->processIndex = ~0u;
		if (ei->wasReturned)
			return false;

		bool isLocal = !process.IsRemote();

		while (true)
		{
			SCOPED_WRITE_LOCK(m_processEntriesLock, lock);
			u32 indexToRun = PopProcess(isLocal);
			if (indexToRun == ~0u)
				return false;
			auto& processEntry = m_processEntries[indexToRun];
			auto newInfo = processEntry.info;
			bool wasSkipped = processEntry.status == ProcessStatus_Skipped;
			lock.Leave();

			UpdateQueueCounter(-1);

			if (wasSkipped)
			{
				SkipProcess(*newInfo);
				continue;
			}

			UpdateActiveProcessCounter(isLocal, 1);

			ei->startInfo = newInfo;
			ei->processIndex = indexToRun;

			auto& si = newInfo->startInfo;
			UBA_ASSERT(Equals(currentStartInfo.application, si.application));
			outNextProcess.arguments = si.arguments;
			outNextProcess.workingDir = si.workingDir;
			outNextProcess.description = si.description;
			outNextProcess.logFile = si.logFile;
			return true;
		}
	}

	void Scheduler::ExitProcess(ExitProcessInfo& info, Process& process, u32 exitCode)
	{
		ProcessHandle ph;
		ph.m_process = &process;

		auto si = info.startInfo;
		if (auto func = si->startInfo.exitedFunc)
			func(si->startInfo.userData, ph);

		SCOPED_WRITE_LOCK(m_processEntriesLock, lock);
		auto& entry = m_processEntries[info.processIndex];
		u32* dependencies = entry.dependencies;
		entry.status = exitCode == 0 ? ProcessStatus_Success : ProcessStatus_Failed;
		entry.info = nullptr;
		entry.dependencies = nullptr;
		if (info.isLocal)
			m_activeLocalProcessWeight -= si->weight;
		lock.Leave();

		UpdateActiveProcessCounter(info.isLocal, -1);
		FinishProcess(ph);
		m_updateThreadLoop.Set();
		delete[] dependencies;
		delete si;

		ph.m_process = nullptr;
	}

	void Scheduler::SkipProcess(ProcessStartInfo2& info)
	{
		ProcessHandle ph(new SkippedProcess(info.startInfo));
		if (auto func = info.startInfo.exitedFunc)
			func(info.startInfo.userData, ph);
		FinishProcess(ph);
	}

	void Scheduler::UpdateQueueCounter(int offset)
	{
		m_queuedProcesses += u32(offset);
		m_session.UpdateStatus(1, 1, TC("Queue"), 4, StringBuffer<32>().Appendf(TC("%u"), m_queuedProcesses.load()).data, LogEntryType_Info);
	}

	void Scheduler::UpdateActiveProcessCounter(bool isLocal, int offset)
	{
		if (isLocal)
			m_activeLocalProcesses += u32(offset);
		else
			m_activeRemoteProcesses += u32(offset);
		m_session.UpdateStatus(2, 1, TC("Active"), 4, StringBuffer<32>().Appendf(TC("Local %u Remote %u"), m_activeLocalProcesses.load(), m_activeRemoteProcesses.load()).data, LogEntryType_Info);
	}

	void Scheduler::FinishProcess(const ProcessHandle& handle)
	{
		++m_finishedProcesses;
		if (m_processFinished)
			m_processFinished(handle);
		m_session.UpdateStatus(3, 1, TC("Finished"), 4, StringBuffer<32>().Appendf(TC("%u"), m_finishedProcesses.load()).data, LogEntryType_Info);
	}

	template<typename LineFunc>
	bool ReadLines(Logger& logger, const tchar* file, const LineFunc& lineFunc)
	{
		FileHandle handle;
		if (!OpenFileSequentialRead(logger, file, handle))
			return logger.Error(TC("Failed to open file %s"), file);
		auto fg = MakeGuard([&]() { CloseFile(file, handle); });
		u64 fileSize = 0;
		if (!GetFileSizeEx(fileSize, handle))
			return logger.Error(TC("Failed to get size of file %s"), file);
		char buffer[512];
		u64 left = fileSize;
		std::string line;
		while (left)
		{
			u64 toRead = Min(left, u64(sizeof(buffer)));
			left -= toRead;
			if (!ReadFile(logger, file, handle, buffer, toRead))
				return false;

			u64 start = 0;
			for (u64 i=0;i!=toRead;++i)
			{
				if (buffer[i] != '\n')
					continue;
				u64 end = i;
				if (i > 0 && buffer[i-1] == '\r')
					--end;
				line.append(buffer + start, end - start);
				if (!line.empty())
					if (!lineFunc(TString(line.begin(), line.end())))
						return false;
				line.clear();
				start = i + 1;
			}
			if (toRead && buffer[toRead - 1] == '\r')
				--toRead;
			line.append(buffer + start, toRead - start);
		}
		if (!line.empty())
			if (!lineFunc(TString(line.begin(), line.end())))
				return false;
		return true;
	}

	bool Scheduler::EnqueueFromFile(const tchar* yamlFilename)
	{
		auto& logger = m_session.GetLogger();

		TString app;
		TString arg;
		TString dir;
		TString desc;
		bool allowDetour = true;
		bool allowRemote = true;
		float weight = 1.0f;
		Vector<u32> deps;

		ProcessStartInfo si;

		auto enqueueProcess = [&]()
			{
				si.application = app.c_str();
				si.arguments = arg.c_str();
				si.workingDir = dir.c_str();
				si.description = desc.c_str();

				#if UBA_DEBUG
				StringBuffer<> logFile;
				if (true)
				{
					GetNameFromArguments(logFile, si.arguments, true);
					logFile.Append(TC(".log"));
					si.logFile = logFile.data;
				};
				#endif

				EnqueueProcessInfo info { si };
				info.dependencies = deps.data();
				info.dependencyCount = u32(deps.size());
				info.canDetour = allowDetour;
				info.canExecuteRemotely = allowRemote;
				info.weight = weight;
				EnqueueProcess(info);
				app.clear();
				arg.clear();
				dir.clear();
				desc.clear();
				deps.clear();
				allowDetour = true;
				allowRemote = true;
				weight = 1.0f;
			};

		auto readLine = [&](const TString& line)
			{
				const tchar* keyStart = line.c_str();
				while (*keyStart && *keyStart == ' ')
					++keyStart;
				if (!*keyStart)
					return true;
				const tchar* colon = TStrchr(keyStart, ':');
				if (!colon)
					return false;
				if (*keyStart == '-')
				{
					keyStart += 2;
					if (!app.empty())
						enqueueProcess();
				}

				StringBuffer<32> key;
				key.Append(keyStart, colon - keyStart);
				const tchar* valueStart = colon + 1;
				while (*valueStart && *valueStart == ' ')
					++valueStart;

				if (key.Equals(TC("environment")))
				{
					#if PLATFORM_WINDOWS
					SetEnvironmentVariable(TC("PATH"), valueStart);
					#endif
				}
				else if (key.Equals(TC("processes")))
					return true;
				else if (key.Equals(TC("app")))
					app = valueStart;
				else if (key.Equals(TC("arg")))
					arg = valueStart;
				else if (key.Equals(TC("dir")))
					dir = valueStart;
				else if (key.Equals(TC("desc")))
					desc = valueStart;
				else if (key.Equals(TC("detour")))
					allowDetour = !Equals(valueStart, TC("false"));
				else if (key.Equals(TC("remote")))
					allowRemote = !Equals(valueStart, TC("false"));
				else if (key.Equals(TC("weight")))
					StringBuffer<32>(valueStart).Parse(weight);
				else if (key.Equals(TC("dep")))
				{
					const tchar* depStart = TStrchr(valueStart, '[');
					if (!depStart)
						return false;
					++depStart;
					StringBuffer<32> depStr;
					for (const tchar* it = depStart; *it; ++it)
					{
						if (*it != ']' && *it != ',')
						{
							if (*it != ' ')
								depStr.Append(*it);
							continue;
						}
						u32 depIndex;
						if (!depStr.Parse(depIndex))
							return false;
						depStr.Clear();
						deps.push_back(depIndex);

						if (!*it)
							break;
						depStart = it + 1;
					}
				}
				return true;
			};

		if (!ReadLines(logger, yamlFilename, readLine))
			return false;

		if (!app.empty())
			enqueueProcess();

		return true;
	}

}
