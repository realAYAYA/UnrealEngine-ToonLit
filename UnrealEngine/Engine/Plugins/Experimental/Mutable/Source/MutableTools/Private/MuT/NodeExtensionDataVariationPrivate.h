// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeExtensionDataVariation.h"
#include "MuT/NodePrivate.h"

namespace mu
{
	class NodeExtensionDataVariation::Private : public Node::Private
	{
	public:

		static FNodeType s_type;

		NodeExtensionDataPtr DefaultValue;

		struct FVariation
		{
			NodeExtensionDataPtr Value;
			FString Tag;

			//!
			void Serialise(OutputArchive& InArch) const
			{
				uint32 Ver = 2;
				InArch << Ver;

				InArch << Tag;
				InArch << Value;
			}

			void Unserialise(InputArchive& InArch)
			{
				uint32 Ver = 0;
				InArch >> Ver;
				check(Ver >= 1 && Ver <= 2);

				if (Ver <= 1)
				{
					std::string Temp;
					InArch >> Temp;
					Tag = Temp.c_str();
				}
				else
				{
					InArch >> Tag;
				}
				InArch >> Value;
			}
		};

		TArray<FVariation> Variations;

		//!
		void Serialise(OutputArchive& InArch) const
		{
			uint32_t Ver = 1;
			InArch << Ver;

			InArch << DefaultValue;
			InArch << Variations;
		}

		//!
		void Unserialise(InputArchive& InArch)
		{
			uint32_t Ver;
			InArch >> Ver;
			check(Ver == 1);

			InArch >> DefaultValue;
			InArch >> Variations;
		}
	};
}