// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeImagePrivate.h"
#include "MuT/NodeImageConstant.h"
#include "MuT/AST.h"
#include "MuT/StreamsPrivate.h"


namespace mu
{
	class NodeImageConstant::Private : public NodeImage::Private
	{
	public:

		static FNodeType s_type;

        Ptr<ResourceProxy<Image>> m_pProxy;

		//!
		void Serialise( OutputArchive& arch ) const
		{
			uint32 Ver = 0;
			arch << Ver;

			Ptr<const Image> ActualImage;
			if (m_pProxy)
			{
				ActualImage = m_pProxy->Get();
			}

			arch << ActualImage;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32 Ver;
			arch >> Ver;
			check(Ver==0);


			m_pProxy = arch.NewImageProxy();
			if (!m_pProxy)
			{
				// Normal serialisation
				Ptr<Image> ActualImage;
				arch >> ActualImage;
				m_pProxy = new ResourceProxyMemory<Image>(ActualImage.get());
			}
		}
	};

}
