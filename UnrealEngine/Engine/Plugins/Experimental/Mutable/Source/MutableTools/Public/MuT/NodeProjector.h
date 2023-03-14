// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/Parameters.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"


namespace mu
{

	class NodeProjector;
    using NodeProjectorPtr = Ptr<NodeProjector>;
    using NodeProjectorPtrConst = Ptr<const NodeProjector>;

	class NodeProjectorConstant;
    using NodeProjectorConstantPtr = Ptr<NodeProjectorConstant>;
    using NodeProjectorConstantPtrConst = Ptr<const NodeProjectorConstant>;

	class NodeProjectorParameter;
    using NodeProjectorParameterPtr = Ptr<NodeProjectorParameter>;
    using NodeProjectorParameterPtrConst = Ptr<const NodeProjectorParameter>;

    class NodeRange;
    using NodeRangePtr = Ptr<NodeRange>;
    using NodeRangePtrConst = Ptr<const NodeRange>;


    //! Base class of any node that outputs a Projector.
	//! \ingroup model
	class MUTABLETOOLS_API NodeProjector : public Node
	{
	public:

		// Possible subclasses
		enum class EType : uint8
		{
			Constant = 0,
			Parameter = 1,

			None
		};

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		static void Serialise( const NodeProjector* pNode, OutputArchive& arch );
		static NodeProjectorPtr StaticUnserialise( InputArchive& arch );


		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------

        const NODE_TYPE* GetType() const override;
		static const NODE_TYPE* GetStaticType();


		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		inline ~NodeProjector() {}
		
		//!
		EType Type = EType::None;

	};


	//! This node outputs a predefined Projector value.
	//! \ingroup model
	class MUTABLETOOLS_API NodeProjectorConstant : public NodeProjector
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeProjectorConstant();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeProjectorConstant* pNode, OutputArchive& arch );
		static NodeProjectorConstantPtr StaticUnserialise( InputArchive& arch );

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

		//! Get the value that this node returns
        void GetValue( PROJECTOR_TYPE* pType,
                       float* pPosX, float* pPosY, float* pPosZ,
					   float* pDirX, float* pDirY, float* pDirZ,
					   float* pUpX, float* pUpY, float *pUpZ,
                       float* pScaleU, float* pScaleV, float* pScaleW,
                       float* pProjectionAngle ) const;

		//! Set the value to be returned by this node
        void SetValue( PROJECTOR_TYPE type,
                       float posX, float posY, float posZ,
					   float dirX, float dirY, float dirZ,
					   float upX, float upY, float upZ,
                       float scaleU, float scaleV, float scaleW,
                       float projectionAngle );

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeProjectorConstant();

	private:

		Private* m_pD;

	};


	//! Node that defines a Projector model parameter.
	//! \ingroup model
	class MUTABLETOOLS_API NodeProjectorParameter : public NodeProjector
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeProjectorParameter();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeProjectorParameter* pNode, OutputArchive& arch );
		static NodeProjectorParameterPtr StaticUnserialise( InputArchive& arch );


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
        void GetDefaultValue( PROJECTOR_TYPE* pType,
                              float* posX, float* posY, float* posZ,
							  float* pDirX, float* pDirY, float* pDirZ,
							  float* pUpX, float* pUpY, float *pUpZ,
                              float *pScaleU, float *pScaleV, float* pScaleW,
                              float* pProjectionAngle ) const;

		//! Set the default value of the parameter.
        void SetDefaultValue( PROJECTOR_TYPE type,
                              float posX, float posY, float posZ,
							  float dirX, float dirY, float dirZ,
							  float upX, float upY, float upZ,
                              float scaleU, float scaleV, float scaleW,
                              float projectionAngle );

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
		~NodeProjectorParameter();

	private:

		Private* m_pD;

	};


}
