// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"


namespace mu
{

	// Forward definitions
	class NodeBool;
    using NodeBoolPtr = Ptr<NodeBool>;
    using NodeBoolPtrConst = Ptr<const NodeBool>;

	class NodeBoolConstant;
    using NodeBoolConstantPtr = Ptr<NodeBoolConstant>;
    using NodeBoolConstantPtrConst = Ptr<const NodeBoolConstant>;

	class NodeBoolParameter;
    using NodeBoolParameterPtr = Ptr<NodeBoolParameter>;
    using NodeBoolParameterPtrConst = Ptr<const NodeBoolParameter>;

	class NodeBoolIsNull;
    using NodeBoolIsNullPtr = Ptr<NodeBoolIsNull>;
    using NodeBoolIsNullPtrConst = Ptr<const NodeBoolIsNull>;

	class NodeBoolNot;
    using NodeBoolNotPtr = Ptr<NodeBoolNot>;
    using NodeBoolNotPtrConst = Ptr<const NodeBoolNot>;

	class NodeBoolAnd;
    using NodeBoolAndPtr = Ptr<NodeBoolAnd>;
    using NodeBoolAndPtrConst = Ptr<const NodeBoolAnd>;

    class NodeRange;
    using NodeRangePtr = Ptr<NodeRange>;
    using NodeRangePtrConst = Ptr<const NodeRange>;


	//---------------------------------------------------------------------------------------------
    //! %Base class of any node that outputs a Bool value.
	//! \ingroup model
	//---------------------------------------------------------------------------------------------
	class MUTABLETOOLS_API NodeBool : public Node
	{
	public:

		// Possible subclasses
		enum class EType : uint8
		{
			Constant = 0,
			Parameter = 1,
			IsNull = 2,
			Not = 3,
			And = 4,

			None
		};

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		static void Serialise( const NodeBool* pNode, OutputArchive& arch );
		static NodeBoolPtr StaticUnserialise( InputArchive& arch );


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
		inline ~NodeBool() {}

		//!
		EType Type = EType::None;

	};


	//---------------------------------------------------------------------------------------------
	//! Node returning a Bool constant value.
	//! \ingroup model
	//---------------------------------------------------------------------------------------------
	class MUTABLETOOLS_API NodeBoolConstant : public NodeBool
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeBoolConstant();

		static void Serialise( const NodeBoolConstant* pNode, OutputArchive& arch );
		void SerialiseWrapper(OutputArchive& arch) const override;
		static NodeBoolConstantPtr StaticUnserialise( InputArchive& arch );


		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------

        const NODE_TYPE* GetType() const override;
		static const NODE_TYPE* GetStaticType();

        virtual int GetInputCount() const override;
        virtual Node* GetInputNode( int i ) const override;
        void SetInputNode( int i, NodePtr pNode ) override;

		//-----------------------------------------------------------------------------------------
		// Own Interface
		//-----------------------------------------------------------------------------------------

		//! Get the value to be returned by the node.
		bool GetValue() const;

		//! Set the value to be returned by the node.
		void SetValue( bool v );

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden.
		~NodeBoolConstant();

	private:

		Private* m_pD;

	};


	//---------------------------------------------------------------------------------------------
	//! Node that defines a Bool model parameter.
	//! \ingroup model
	//---------------------------------------------------------------------------------------------
	class MUTABLETOOLS_API NodeBoolParameter : public NodeBool
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeBoolParameter();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeBoolParameter* pNode, OutputArchive& arch );
		static NodeBoolParameterPtr StaticUnserialise( InputArchive& arch );


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
		bool GetDefaultValue() const;
		void SetDefaultValue( bool v );

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

		//! Forbidden. 
		~NodeBoolParameter();

	private:

		Private* m_pD;

	};


	//---------------------------------------------------------------------------------------------
	//! Node that returns true if there is an input or false if there is nothing connected.
	//! This node is mostly useful when used in the model Transform's conditions.
	//! \ingroup model
	//---------------------------------------------------------------------------------------------
	class MUTABLETOOLS_API NodeBoolIsNull : public NodeBool
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeBoolIsNull();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeBoolIsNull* pNode, OutputArchive& arch );
		static NodeBoolIsNullPtr StaticUnserialise( InputArchive& arch );


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

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. 
		~NodeBoolIsNull();

	private:

		Private* m_pD;

	};


	//---------------------------------------------------------------------------------------------
	//! Node that returns the oposite of the input value.
	//! \ingroup model
	//---------------------------------------------------------------------------------------------
	class MUTABLETOOLS_API NodeBoolNot : public NodeBool
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeBoolNot();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeBoolNot* pNode, OutputArchive& arch );
		static NodeBoolNotPtr StaticUnserialise( InputArchive& arch );


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
		
		//! Input
		NodeBoolPtr GetInput() const;
		void SetInput( NodeBoolPtr );

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. 
		~NodeBoolNot();

	private:

		Private* m_pD;

	};


	//---------------------------------------------------------------------------------------------
	//! 
	//! \ingroup model
	//---------------------------------------------------------------------------------------------
	class MUTABLETOOLS_API NodeBoolAnd : public NodeBool
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeBoolAnd();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeBoolAnd* pNode, OutputArchive& arch );
		static NodeBoolAndPtr StaticUnserialise( InputArchive& arch );


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
		
		//! Inputs
		NodeBoolPtr GetA() const;
		void SetA( NodeBoolPtr );

		NodeBoolPtr GetB() const;
		void SetB( NodeBoolPtr );

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. 
		~NodeBoolAnd();

	private:

		Private* m_pD;

	};


}
