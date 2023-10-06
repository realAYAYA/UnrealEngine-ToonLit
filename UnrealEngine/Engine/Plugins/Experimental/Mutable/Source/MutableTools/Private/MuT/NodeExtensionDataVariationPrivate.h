// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeExtensionDataVariation.h"
#include "MuT/NodePrivate.h"

namespace mu
{
	class NodeExtensionDataVariation::Private : public Node::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE()

	public:

		static NODE_TYPE s_type;

		NodeExtensionDataPtr DefaultValue;

		struct VARIATION
		{
			NodeExtensionDataPtr Value;
			string Tag;

			//!
			void Serialise(OutputArchive& InArch) const
			{
				uint32_t Ver = 1;
				InArch << Ver;

				InArch << Tag;
				InArch << Value;
			}

			void Unserialise(InputArchive& InArch)
			{
				uint32_t Ver = 0;
				InArch >> Ver;
				check(Ver == 1);

				InArch >> Tag;
				InArch >> Value;
			}
		};

		TArray<VARIATION> Variations;

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