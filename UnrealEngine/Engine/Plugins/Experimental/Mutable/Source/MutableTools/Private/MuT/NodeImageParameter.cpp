// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeImageParameter.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeImageParameterPrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
    FNodeType NodeImageParameter::Private::s_type =
            FNodeType( "ImageParameter", NodeImage::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

    MUTABLE_IMPLEMENT_NODE( NodeImageParameter, EType::Parameter, Node, Node::EType::Image)


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
    void NodeImageParameter::SetName( const FString& Name )
	{
		m_pD->m_name = Name;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageParameter::SetUid(const FString& Uid)
	{
		m_pD->m_uid = Uid;
	}

	
	//---------------------------------------------------------------------------------------------
	void NodeImageParameter::SetDefaultValue(FName Value)
	{
    	m_pD->m_defaultValue = Value;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageParameter::SetRangeCount(int i)
	{
		check(i >= 0);
		m_pD->m_ranges.SetNum(i);
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageParameter::SetRange(int i, NodeRangePtr pRange)
	{
		check(i >= 0 && i<int(m_pD->m_ranges.Num()));
		if (i >= 0 && i<int(m_pD->m_ranges.Num()))
		{
			m_pD->m_ranges[i] = pRange;
		}
	}

}


