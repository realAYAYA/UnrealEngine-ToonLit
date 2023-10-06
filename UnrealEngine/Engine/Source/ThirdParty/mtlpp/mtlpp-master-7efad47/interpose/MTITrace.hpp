// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "imp_SelectorCache.hpp"

#include <assert.h>
#include <fstream>
#include <string>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <vector>

MTLPP_BEGIN

struct MTIString : public std::string
{
	MTIString() {}
	MTIString(std::string const& other) : std::string(other) {}
	MTIString(std::string&& other) : std::string(other) {}
};

std::fstream& operator>>(std::fstream& fs, MTIString& dt);

std::fstream& operator<<(std::fstream& fs, const MTIString& dt);

struct MTITraceCommand
{
	MTIString Class;
	uintptr_t Thread;
	uintptr_t Receiver;
	MTIString Cmd;
};

struct MTITraceCommandHandler
{
	MTIString Class;
	MTIString Cmd;
	MTIString Id;
	
	MTITraceCommandHandler(std::string Class, std::string Cmd);
	
	void Trace(std::fstream& fs, uintptr_t Receiver);
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs) = 0;
};

std::fstream& operator>>(std::fstream& fs, MTITraceCommand& dt);

std::fstream& operator<<(std::fstream& fs, const MTITraceCommand& dt);

template <typename T>
struct MTITraceArray
{
	T const* Data;
	uint64_t Length;
	std::vector<T> Backing;
	
	MTITraceArray()
	: Data(nullptr)
	, Length(0)
	{
		
	}
	
	MTITraceArray(T const* InData, uint64_t InLength)
	: Data(InData)
	, Length(InLength)
	{
		
	}
};

template <typename T>
std::fstream& operator>>(std::fstream& fs, MTITraceArray<T>& dt)
{
	fs >> dt.Length;
	if (dt.Length)
	{
		dt.Backing.resize(dt.Length);
		for (uint64_t i = 0; i < dt.Length; i++)
		{
			fs >> dt.Backing[i];
		}
		dt.Data = dt.Backing.data();
	}
	return fs;
}

template <typename T>
std::fstream& operator<<(std::fstream& fs, MTITraceArray<T>& dt)
{
	fs << dt.Length;
	if (dt.Length)
	{
		assert(dt.Data);
		for (uint64_t i = 0; i < dt.Length; i++)
		{
			fs << dt.Data[i];
		}
	}
	return fs;
}

class MTITrace
{
public:
	MTITrace();
	~MTITrace();
	
	static MTITrace& Get();
	
	std::fstream& BeginWrite();
	void EndWrite();
	
	void RegisterObject(uintptr_t Original, id Actual);
	id FetchObject(uintptr_t Original);
	
	void RegisterCommandHandler(MTITraceCommandHandler* Handler);
	
	void Replay(std::string path);
	
private:
	std::recursive_mutex Mutex;
	std::string Path;
	std::fstream File;
	std::unordered_map<uintptr_t, id> Objects;
	std::unordered_map<std::string, MTITraceCommandHandler*> CommandHandlers;
};

MTLPP_END
