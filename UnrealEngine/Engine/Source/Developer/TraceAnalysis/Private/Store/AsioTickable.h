// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Asio/Asio.h"

namespace UE {
namespace Trace {

////////////////////////////////////////////////////////////////////////////////
class FAsioTickable
{
public:
						FAsioTickable(asio::io_context& IoContext);
	virtual				~FAsioTickable();
	asio::io_context&	GetIoContext();
	bool				IsActive() const;
	bool				StartTick(uint32 MillisecondRate);
	bool				StopTick();
	bool				TickOnce(uint32 MillisecondRate);
	virtual void		OnTick() = 0;

private:
	void				AsyncTick();
	asio::steady_timer	Timer;
	uint32				MillisecondRate = 0;
};

} // namespace Trace
} // namespace UE
