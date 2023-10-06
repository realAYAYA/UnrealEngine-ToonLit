// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Asio/Asio.h"
#include "AsioFile.h"
#include "Containers/Array.h"
#include "Containers/StringView.h"
#include "Containers/UnrealString.h"
#include "Utils.h"

namespace UE {
namespace Trace {

class FAsioReadable;
class FAsioWriteable;

////////////////////////////////////////////////////////////////////////////////
enum class EStoreVersion
{
	Value = 0x0100, // 0xMMmm MM=major, mm=minor
};

////////////////////////////////////////////////////////////////////////////////
class FAsioStore
{
public:
	class FTrace
	{
	public:
							FTrace(const TCHAR* InPath);
		const FStringView&	GetName() const;
		uint32				GetId() const;
		uint64				GetSize() const;
		uint64				GetTimestamp() const;

	private:
		friend				FAsioStore;
		FString				Path;
		FStringView			Name;
		uint64				Timestamp;
		uint32				Id = 0;
	};

	struct FNewTrace
	{
		uint32			Id;
		FAsioWriteable* Writeable;
	};

						FAsioStore(asio::io_context& IoContext, const TCHAR* InStoreDir);
						~FAsioStore();
	void				Close();
	const TCHAR*		GetStoreDir() const;
	uint32				GetChangeSerial() const;
	uint32				GetTraceCount() const;
	const FTrace*		GetTraceInfo(uint32 Index) const;
	bool				HasTrace(uint32 Id) const;
	FNewTrace			CreateTrace();
	FAsioReadable*		OpenTrace(uint32 Id);
	class				FDirWatcher;

private:
	FTrace*				GetTrace(uint32 Id) const;
	FTrace*				AddTrace(const TCHAR* Path);
	void				ClearTraces();
	void				WatchDir();
	void				Refresh();
	asio::io_context&	IoContext;
	FString				StoreDir;
	TArray<FTrace*>		Traces;
	uint32				ChangeSerial;
	FDirWatcher*		DirWatcher = nullptr;
#if 0
	int32				LastTraceId = -1;
#endif // 0
};

} // namespace Trace
} // namespace UE
