// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"


namespace mu
{
	class NodeRange;

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
			DEPRECATED_IsNull = 2,
			Not = 3,
			And = 4,

			None
		};

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		static void Serialise( const NodeBool* pNode, OutputArchive& arch );
		static Ptr<NodeBool> StaticUnserialise( InputArchive& arch );


		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------

        const FNodeType* GetType() const override;
		static const FNodeType* GetStaticType();


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
		static Ptr<NodeBoolConstant> StaticUnserialise( InputArchive& arch );


		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------

        const FNodeType* GetType() const override;
		static const FNodeType* GetStaticType();

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
		static Ptr<NodeBoolParameter> StaticUnserialise( InputArchive& arch );


		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------

        const FNodeType* GetType() const override;
		static const FNodeType* GetStaticType();

		//-----------------------------------------------------------------------------------------
		// Own Interface
		//-----------------------------------------------------------------------------------------

		//! Set the name of the parameter. It will be exposed in the final compiled data.
		void SetName( const FString& );

		//! Get the default value of the parameter.
		void SetDefaultValue( bool v );

        //! Set the number of ranges (dimensions) for this parameter.
        //! By default a parameter has 0 ranges, meaning it only has one value.
        void SetRangeCount( int i );
        void SetRange( int i, Ptr<NodeRange> );

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
		static Ptr<NodeBoolNot> StaticUnserialise( InputArchive& arch );


		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------

        const FNodeType* GetType() const override;
		static const FNodeType* GetStaticType();

		//-----------------------------------------------------------------------------------------
		// Own Interface
		//-----------------------------------------------------------------------------------------
		
		//! Input
		Ptr<NodeBool> GetInput() const;
		void SetInput( Ptr<NodeBool> );

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
		static Ptr<NodeBoolAnd> StaticUnserialise( InputArchive& arch );


		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------

        const FNodeType* GetType() const override;
		static const FNodeType* GetStaticType();

		//-----------------------------------------------------------------------------------------
		// Own Interface
		//-----------------------------------------------------------------------------------------
		
		//! Inputs
		Ptr<NodeBool> GetA() const;
		void SetA(Ptr<NodeBool>);

		Ptr<NodeBool> GetB() const;
		void SetB(Ptr<NodeBool>);

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
