// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaEvent.h"
#include "UbaSynchronization.h"

namespace uba
{

	struct Bottleneck
	{
		Bottleneck(u32 mc) : m_underMax(true), m_activeCount(0), m_maxCount(mc) {}

		void Enter()
		{
			SCOPED_WRITE_LOCK(m_lock, lock);
			while (true)
			{
				if (m_activeCount < m_maxCount)
				{
					++m_activeCount;
					if (m_activeCount == m_maxCount)
						m_underMax.Reset();
					break;
				}

				lock.Leave();
				m_underMax.IsSet();
				lock.Enter();
			}
		}

		void Leave()
		{
			SCOPED_WRITE_LOCK(m_lock, lock);
			if (m_activeCount == m_maxCount)
				m_underMax.Set();
			--m_activeCount;
		}

	private:
		ReaderWriterLock m_lock;
		Event m_underMax;
		u32 m_activeCount;
		u32 m_maxCount;
	};

	struct BottleneckScope
	{
		BottleneckScope(Bottleneck& b) : bottleneck(b) { b.Enter(); }
		~BottleneckScope() { bottleneck.Leave(); }

		Bottleneck& bottleneck;
	};

}
