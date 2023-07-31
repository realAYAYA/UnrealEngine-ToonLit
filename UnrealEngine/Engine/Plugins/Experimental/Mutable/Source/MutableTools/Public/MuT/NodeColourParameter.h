// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeColour.h"


namespace mu
{

	class NodeColourParameter;
    using NodeColourParameterPtr = Ptr<NodeColourParameter>;
    using NodeColourParameterPtrConst = Ptr<const NodeColourParameter>;

    class NodeRange;
    using NodeRangePtr = Ptr<NodeRange>;
    using NodeRangePtrConst = Ptr<const NodeRange>;


	//! Node that defines a colour model parameter.
	//! \ingroup model
	class MUTABLETOOLS_API NodeColourParameter : public NodeColour
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeColourParameter();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeColourParameter* pNode, OutputArchive& arch );
		static NodeColourParameterPtr StaticUnserialise( InputArchive& arch );


		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------

		

		const NODE_TYPE* GetType() const override;
		static const NODE_TYPE* GetStaticType();

		int GetInputCount() const override;
		Node* GetInputNode( int i ) const override;
		void SetInputNode( int i, NodePtr pNode ) override;

		//-----------------------------------------------------------------------------------------
		// Own Interface
		//-----------------------------------------------------------------------------------------

		//! Get the name of the parameter. It will be exposed in the final compiled data.
		const char* GetName() const;

		//! Set the name of the parameter.
		void SetName( const char* );

		//! Get the uid of the parameter. It will be exposed in the final compiled data.
		const char* GetUid() const;

		//! Set the uid of the parameter.
		void SetUid( const char* );

		//! Get the default value of the parameter.
		void GetDefaultValue( float* pR, float* pG, float* pB ) const;

		//! Set the default value of the parameter.
		void SetDefaultValue( float r, float g, float b );

        //! Set the number of ranges (dimensions) for this parameter.
        //! By default a parameter has 0 ranges, meaning it only has one value.
        void SetRangeCount( int i );
        void SetRange( int i, NodeRangePtr pRange );

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeColourParameter();

	private:

		Private* m_pD;

	};


}
