// Copyright Epic Games, Inc. All Rights Reserved.

#include "Pch.h"
#include "Asio.h"
#include "AsioContext.h"
#include "Recorder.h"
#include "Store.h"
#include "StoreCborServer.h"
#include "StoreService.h"

////////////////////////////////////////////////////////////////////////////////
struct FStoreServiceImpl
{
public:
							FStoreServiceImpl(const FStoreService::FDesc& Desc);
							~FStoreServiceImpl();
	FAsioContext			Context;
	FStore					Store;
	FRecorder				Recorder;
	FStoreCborServer		CborServer;
};

////////////////////////////////////////////////////////////////////////////////
FStoreServiceImpl::FStoreServiceImpl(const FStoreService::FDesc& Desc)
: Context(Desc.ThreadCount)
, Store(Context.Get(), Desc.StoreDir)
, Recorder(Context.Get(), Store)
, CborServer(Context.Get(), Desc.StorePort, Store, Recorder)
{
}

////////////////////////////////////////////////////////////////////////////////
FStoreServiceImpl::~FStoreServiceImpl()
{
	asio::post(Context.Get(), [this] () {
		CborServer.Close();
		Recorder.Close();
		Store.Close();
	});
	Context.Wait();
}



////////////////////////////////////////////////////////////////////////////////
FStoreService* FStoreService::Create(const FDesc& InDesc)
{
	FDesc Desc = InDesc;

	if (Desc.ThreadCount <= 0)
	{
		Desc.ThreadCount = std::thread::hardware_concurrency();
	}

	// TODO: not thread safe yet
	Desc.ThreadCount = 1;

	FStoreServiceImpl* Impl = new FStoreServiceImpl(Desc);
	if (Desc.RecorderPort >= 0)
	{
		FRecorder& Recorder = Impl->Recorder;
		Recorder.StartServer(Desc.RecorderPort);
	}

	Impl->Context.Start();

	return (FStoreService*)Impl;
}

////////////////////////////////////////////////////////////////////////////////
void FStoreService::operator delete (void* Addr)
{
	auto* Self = (FStoreServiceImpl*)Addr;
	delete Self;
}

////////////////////////////////////////////////////////////////////////////////
uint32 FStoreService::GetPort() const
{
	auto* Self = (FStoreServiceImpl*)this;
	return Self->CborServer.GetPort();
}

////////////////////////////////////////////////////////////////////////////////
uint32 FStoreService::GetRecorderPort() const
{
	auto* Self = (FStoreServiceImpl*)this;
	return Self->Recorder.GetPort();
}

/* vim: set noexpandtab : */
