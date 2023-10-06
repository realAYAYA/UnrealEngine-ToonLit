// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MTLTTrace.cpp: Metal device RHI implementation.
 =============================================================================*/

#include "MTITrace.hpp"
#import <Foundation/Foundation.h>

MTLPP_BEGIN

std::fstream& operator>>(std::fstream& fs, MTIString& dt)
{
	size_t size;
	fs >> size;
	char c;
	for(unsigned i = 0; i < size; i++)
	{
		fs >> c;
		dt.push_back(c);
	}
	return fs;
}

std::fstream& operator<<(std::fstream& fs, const MTIString& dt)
{
	size_t size = dt.length();
	fs << size;
	for(unsigned i = 0; i < dt.length(); i++)
	{
		fs << dt[i];
	}
	return fs;
}

std::fstream& operator>>(std::fstream& fs, MTITraceCommand& dt)
{
	fs >> dt.Class;
	fs >> dt.Thread;
	fs >> dt.Receiver;
	fs >> dt.Cmd;
	return fs;
}

std::fstream& operator<<(std::fstream& fs, const MTITraceCommand& dt)
{
	fs << dt.Class;
	fs << dt.Thread;
	fs << dt.Receiver;
	fs << dt.Cmd;
	return fs;
}

MTITraceCommandHandler::MTITraceCommandHandler(std::string InClass, std::string InCmd)
: Class(InClass)
, Cmd(InCmd)
{
	Id = Class + Cmd;
	MTITrace::Get().RegisterCommandHandler(this);
}

void MTITraceCommandHandler::Trace(std::fstream& fs, uintptr_t Receiver)
{
	fs << Class;
	fs << (uint32)pthread_mach_thread_np(pthread_self());
	fs << Receiver;
	fs << Cmd;
}

MTITrace::MTITrace()
{
}

MTITrace::~MTITrace()
{
	if(File.is_open())
	{
		File.flush();
		File.close();
	}
}

MTITrace& MTITrace::Get()
{
	static MTITrace sSelf;
	return sSelf;
}

std::fstream& MTITrace::BeginWrite()
{
	Mutex.lock();
	if (!File.is_open())
	{
		Path = [[NSTemporaryDirectory() stringByAppendingPathComponent:[[NSUUID UUID] UUIDString]] UTF8String];
		File.open(Path, std::ios_base::out|std::ios_base::binary);
		assert(File.is_open());
	}
	return File;
}

void MTITrace::EndWrite()
{
	Mutex.unlock();
}

void MTITrace::RegisterObject(uintptr_t Original, id Actual)
{
	Mutex.lock();
	Objects[Original] = Actual;
	Mutex.unlock();
}

id MTITrace::FetchObject(uintptr_t Original)
{
	id Object = nullptr;
	Mutex.lock();
	auto It = Objects.find(Original);
	if (It != Objects.end())
	{
		Object = It->second;
	}
	Mutex.unlock();
	return Object;
}

void MTITrace::RegisterCommandHandler(MTITraceCommandHandler* Handler)
{
	Mutex.lock();
	CommandHandlers[Handler->Id] = Handler;
	Mutex.unlock();
}

void MTITrace::Replay(std::string InPath)
{
	assert(!File.is_open());
	Path = InPath;
	File.open(Path, std::ios_base::in|std::ios_base::binary);
	assert(File.is_open());
	
	MTITraceCommand Command;
	do {
		File >> Command;
		auto it = CommandHandlers.find(Command.Class + Command.Cmd);
		if (it != CommandHandlers.end())
		{
			it->second->Handle(Command, File);
		}
	} while (File.good());
}

MTLPP_END
