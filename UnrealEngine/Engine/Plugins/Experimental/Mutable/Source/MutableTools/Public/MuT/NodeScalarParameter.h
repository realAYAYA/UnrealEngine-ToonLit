// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Parameters.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeScalar.h"


namespace mu
{

	// Forward definitions
    class NodeImage;
    using NodeImagePtr = Ptr<NodeImage>;
    using NodeImagePtrConst = Ptr<const NodeImage>;

    class NodeRange;
    using NodeRangePtr = Ptr<NodeRange>;
    using NodeRangePtrConst = Ptr<const NodeRange>;

    class NodeScalarParameter;
    using NodeScalarParameterPtr = Ptr<NodeScalarParameter>;
    using NodeScalarParameterPtrConst = Ptr<const NodeScalarParameter>;


	//! Node that defines a scalar model parameter.
	//! \ingroup model
	class MUTABLETOOLS_API NodeScalarParameter : public NodeScalar
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeScalarParameter();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeScalarParameter* pNode, OutputArchive& arch );
		static NodeScalarParameterPtr StaticUnserialise( InputArchive& arch );


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
		float GetDefaultValue() const;
		void SetDefaultValue( float v );

		//! Get the additional information about the type
		PARAMETER_DETAILED_TYPE GetDetailedType() const ;
		void SetDetailedType( PARAMETER_DETAILED_TYPE type );

        //! Set the number of additional description images associated to this parameter.
        //! These images can be built from a parameter and, for example, they can be used to build
        //! user-interfaces for customisation.
        void SetDescImageCount( int i );
        void SetDescImage( int i, NodeImagePtr pImage );

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
		~NodeScalarParameter();

	private:

		Private* m_pD;

	};


}
