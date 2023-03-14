// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreFwd.h"
#include "HAL/Platform.h"
#include "Templates/UniquePtr.h"

class IFileHandle;

namespace UE {
namespace Trace {

class IInDataStream
{
public:
	virtual			~IInDataStream() = default;
	virtual int32	Read(void* Data, uint32 Size) = 0;
	virtual void	Close() {}
};

/*
* An implementation of IInDataStream that reads from a file on disk.
*/
class TRACEANALYSIS_API FFileDataStream : public IInDataStream
{
public:
	FFileDataStream();
	~FFileDataStream();

	/*
	* Open the file.
	* 
	* @param Path	The path to the file.
	* 
	* @return True if the file was opened successfully.
	*/
	bool Open(const TCHAR* Path);

	/*
	* Read from the file.
	* 
	* @param Data	The address in memory to write file data to.
	* @param Size	The size of available memory at Data.
	* 
	* @return The number of bytes read from the file to Data, or zero in the case of a read failure.
	*/
	virtual int32 Read(void* Data, uint32 Size) override;

	/*
	* Close the file.
	*/
	virtual void Close() override;

private:
	TUniquePtr<IFileHandle> Handle;
	uint64 Remaining;
};

} // namespace Trace
} // namespace UE
