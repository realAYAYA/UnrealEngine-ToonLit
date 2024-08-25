// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuR/CodeVisitor.h"

#include "HAL/PlatformCrt.h"
#include "MuR/MutableTrace.h"

namespace mu
{

	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------
	void SubtreeParametersVisitor::Run(OP::ADDRESS Root, const FProgram& Program)
	{
		// Cached?
		TArray<int32>* it = ResultCache.Find(Root);
		if (it)
		{
			RelevantParams = *it;
			return;
		}

		// Not cached
		MUTABLE_CPUPROFILER_SCOPE(SubtreeParametersVisitor);

		Visited.SetNum(Program.m_opAddress.Num());
		if (Program.m_opAddress.Num())
		{
			FMemory::Memzero(&Visited[0], Visited.Num());
		}

		CurrentParams.SetNum(Program.m_parameters.Num());
		if (CurrentParams.Num())
		{
			FMemory::Memzero(CurrentParams.GetData(), CurrentParams.GetAllocatedSize());
		}

		Pending.Reserve(Program.m_opAddress.Num() / 4);
		Pending.Add(Root);

		while (Pending.Num())
		{
			OP::ADDRESS at = Pending.Pop();

			if (!Visited[at])
			{
				Visited[at] = true;

				switch (Program.GetOpType(at))
				{
				case OP_TYPE::NU_PARAMETER:
				case OP_TYPE::SC_PARAMETER:
				case OP_TYPE::BO_PARAMETER:
				case OP_TYPE::CO_PARAMETER:
				case OP_TYPE::PR_PARAMETER:
				case OP_TYPE::IM_PARAMETER:
				case OP_TYPE::ST_PARAMETER:
					CurrentParams[Program.GetOpArgs<OP::ParameterArgs>(at).variable]++;
					break;

				default:
					break;
				}

				ForEachReference(Program, at, [&](OP::ADDRESS ref)
					{
						if (ref)
						{
							Pending.Add(ref);
						}
					});
			}
		}


		// Build result
		RelevantParams.Empty();
		for (int32 i = 0; i < CurrentParams.Num(); ++i)
		{
			if (CurrentParams[i])
			{
				RelevantParams.Add(i);
			}
		}

		ResultCache.Add(Root, RelevantParams);
	}

}