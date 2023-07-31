// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuR/CodeVisitor.h"

#include "HAL/PlatformCrt.h"
#include "MuR/MutableTrace.h"

namespace mu
{


	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------
	void SubtreeParametersVisitor::Run(OP::ADDRESS root, PROGRAM& program)
	{
		// Cached?
		auto it = m_resultCache.Find(root);
		if (it)
		{
			m_params = *it;
			return;
		}

		// Not cached
		{
			MUTABLE_CPUPROFILER_SCOPE(SubtreeParametersVisitor);

			m_visited.SetNum(program.m_opAddress.Num());
			if (program.m_opAddress.Num())
			{
				FMemory::Memzero(&m_visited[0], m_visited.Num());
			}

			m_currentParams.SetNum(program.m_parameters.Num());
			if (m_currentParams.Num())
			{
				FMemory::Memzero(&m_currentParams[0], sizeof(int) * m_currentParams.Num());
			}

			m_pending.Reserve(program.m_opAddress.Num() / 4);
			m_pending.Add(root);

			while (m_pending.Num())
			{
				OP::ADDRESS at = m_pending.Pop();

				if (!m_visited[at])
				{
					m_visited[at] = true;

					switch (program.GetOpType(at))
					{
					case OP_TYPE::NU_PARAMETER:
					case OP_TYPE::SC_PARAMETER:
					case OP_TYPE::BO_PARAMETER:
					case OP_TYPE::CO_PARAMETER:
					case OP_TYPE::PR_PARAMETER:
					case OP_TYPE::IM_PARAMETER:
						m_currentParams[program.GetOpArgs<OP::ParameterArgs>(at).variable]++;
						break;

					default:
						break;
					}

					ForEachReference(program, at, [&](OP::ADDRESS ref)
						{
							if (ref)
							{
								m_pending.Add(ref);
							}
						});
				}
			}


			// Build result
			m_params.Empty();
			for (size_t i = 0; i < m_currentParams.Num(); ++i)
			{
				if (m_currentParams[i])
				{
					m_params.Add(int(i));
				}
			}

			m_resultCache.Add(root, m_params);
		}
	}

}