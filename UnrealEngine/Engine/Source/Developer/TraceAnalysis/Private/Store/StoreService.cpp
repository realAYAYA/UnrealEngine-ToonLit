// Copyright Epic Games, Inc. All Rights Reserved.

#include "Trace/StoreService.h"
#include "Asio/Asio.h"
#include "AsioContext.h"
#include "AsioRecorder.h"
#include "AsioStore.h"
#include "AsioStoreCborServer.h"
#include "HAL/PlatformFile.h"

namespace UE {
namespace Trace {

////////////////////////////////////////////////////////////////////////////////
struct FStoreServiceImpl
{
public:
							FStoreServiceImpl(const FStoreService::FDesc& Desc);
							~FStoreServiceImpl();
	FAsioContext			Context;
	FAsioStore				Store;
	FAsioRecorder			Recorder;
	FAsioStoreCborServer	CborServer;
};

////////////////////////////////////////////////////////////////////////////////
FStoreServiceImpl::FStoreServiceImpl(const FStoreService::FDesc& Desc)
: Context(Desc.ThreadCount)
, Store(Context.Get(), Desc.StoreDir)
, Recorder(Context.Get(), Store)
, CborServer(Context.Get(), Store, Recorder)
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

	IPlatformFile& PlatformFile = IPlatformFile::GetPlatformPhysical();
	PlatformFile.CreateDirectory(Desc.StoreDir);

	if (Desc.ThreadCount <= 0)
	{
		Desc.ThreadCount = FPlatformMisc::NumberOfCoresIncludingHyperthreads();
	}

	// TODO: not thread safe yet
	Desc.ThreadCount = 1;

	FStoreServiceImpl* Impl = new FStoreServiceImpl(Desc);
	if (Desc.RecorderPort >= 0)
	{
		FAsioRecorder& Recorder = Impl->Recorder;
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

} // namespace Trace
} // namespace UE
