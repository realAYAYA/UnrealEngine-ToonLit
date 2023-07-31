// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeImagePrivate.h"
#include "MuT/NodeImageConstant.h"
#include "MuT/AST.h"
#include "MuT/StreamsPrivate.h"

#include "MuR/MemoryPrivate.h"


namespace mu
{

	class NodeImageConstant::Private : public NodeImage::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE();

	public:

		static NODE_TYPE s_type;

        Ptr<ResourceProxy<Image>> m_pProxy;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 0;
			arch << ver;

            Ptr<const Image> image;
            if (m_pProxy)
            {
                image = m_pProxy->Get();
            }

            arch << image;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
			check(ver==0);

            // Are we using proxies?
            m_pProxy = arch.NewImageProxy();
            if (!m_pProxy)
            {
                // Normal serialisation
                ImagePtr image;
                arch >> image;
                m_pProxy = new ResourceProxyMemory<Image>( image.get() );
            }
		}
	};

}
