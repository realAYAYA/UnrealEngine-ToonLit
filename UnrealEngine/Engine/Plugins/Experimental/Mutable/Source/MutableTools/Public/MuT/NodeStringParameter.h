// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Parameters.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeString.h"


namespace mu
{

	// Forward definitions
    class NodeImage;
    using NodeImagePtr = Ptr<NodeImage>;
    using NodeImagePtrConst = Ptr<const NodeImage>;

    class NodeRange;
    using NodeRangePtr = Ptr<NodeRange>;
    using NodeRangePtrConst = Ptr<const NodeRange>;

    class NodeStringParameter;
    using NodeStringParameterPtr = Ptr<NodeStringParameter>;
    using NodeStringParameterPtrConst = Ptr<const NodeStringParameter>;


	//! Node that defines a string model parameter.
	//! \ingroup model
	class MUTABLETOOLS_API NodeStringParameter : public NodeString
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeStringParameter();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeStringParameter* pNode, OutputArchive& arch );
		static NodeStringParameterPtr StaticUnserialise( InputArchive& arch );


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
		void SetName( const char* );

		//! Get the uid of the parameter. It will be exposed in the final compiled data.
		const char* GetUid() const;
		void SetUid( const char* );

		//! Get the default value of the parameter.
		const char* GetDefaultValue() const;
		void SetDefaultValue( const char* v );

		//! Get the additional information about the type
		PARAMETER_DETAILED_TYPE GetDetailedType() const ;
		void SetDetailedType( PARAMETER_DETAILED_TYPE type );

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
		~NodeStringParameter();

	private:

		Private* m_pD;

	};


}
