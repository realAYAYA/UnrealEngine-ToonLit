// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeImageSwizzle.h"

#include "Misc/AssertionMacros.h"
#include "MuR/ImagePrivate.h"
#include "MuT/NodeImageSwizzlePrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	FNodeType NodeImageSwizzle::Private::s_type =
			FNodeType( "ImageSwizzle", NodeImage::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeImageSwizzle, EType::Swizzle, Node, Node::EType::Image)


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	EImageFormat NodeImageSwizzle::GetFormat() const
	{
		return m_pD->m_format;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageSwizzle::SetFormat(EImageFormat format )
	{
		m_pD->m_format = format;

		int channelCount = GetImageFormatData( format ).Channels;
		m_pD->m_sources.SetNum( channelCount );
		m_pD->m_sourceChannels.SetNum( channelCount );
	}


	//---------------------------------------------------------------------------------------------
	NodeImagePtr NodeImageSwizzle::GetSource( int t ) const
	{
		check( t>=0 && t<(int)m_pD->m_sources.Num() );
		return m_pD->m_sources[t].get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageSwizzle::SetSource( int t, NodeImagePtr pNode )
	{
		check( t>=0 && t<(int)m_pD->m_sources.Num() );
		m_pD->m_sources[t] = pNode;
	}


	//---------------------------------------------------------------------------------------------
	int NodeImageSwizzle::GetSourceChannel( int t ) const
	{
		check( t>=0 && t<(int)m_pD->m_sourceChannels.Num() );
		return m_pD->m_sourceChannels[t];
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageSwizzle::SetSourceChannel( int t, int channel )
	{
		check( t>=0 && t<(int)m_pD->m_sourceChannels.Num() );
		m_pD->m_sourceChannels[t] = channel;
	}



}
