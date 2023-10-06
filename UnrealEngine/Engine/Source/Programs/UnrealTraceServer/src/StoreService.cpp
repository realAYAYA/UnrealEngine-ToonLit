// Copyright Epic Games, Inc. All Rights Reserved.

#include "Pch.h"
#include "Asio.h"
#include "AsioContext.h"
#include "Recorder.h"
#include "Store.h"
#include "StoreCborServer.h"
#include "StoreService.h"
#include "StoreSettings.h"

////////////////////////////////////////////////////////////////////////////////
struct FStoreServiceImpl
{
public:
							FStoreServiceImpl(FStoreSettings* Settings);
							~FStoreServiceImpl();
	FAsioContext			Context;
	FStore					Store;
	FRecorder				Recorder;
	FStoreCborServer		CborServer;
};

////////////////////////////////////////////////////////////////////////////////
FStoreServiceImpl::FStoreServiceImpl(FStoreSettings* Settings)
: Context(Settings->ThreadCount)
, Store(Context.Get(), Settings)
, Recorder(Context.Get(), Store)
, CborServer(Context.Get(), Settings, Store, Recorder)
{
	if (Settings->RecorderPort >= 0)
	{
		Recorder.StartServer(Settings->RecorderPort);
	}
	Context.Start();
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
FStoreService* FStoreService::Create(FStoreSettings* Settings)
{
	if (Settings->ThreadCount <= 0)
	{
		Settings->ThreadCount = std::thread::hardware_concurrency();
	}

	// TODO: not thread safe yet
	Settings->ThreadCount = 1;

	return (FStoreService*) new FStoreServiceImpl(Settings);
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
