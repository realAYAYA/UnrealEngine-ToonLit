// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/PlatformMath.h"
#include "MuR/ExtensionData.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"


namespace mu
{
	class Instance;

	typedef Ptr<Instance> InstancePtr;
	typedef Ptr<const Instance> InstancePtrConst;


    //! \brief A customised object created from a model and a set of parameter values.
    //!
    //! It corresponds to an "engine object" but the contents of its data depends on the Model, and
    //! it may contain any number of LODs, components, surfaces, meshes and images, even none.
	//! \ingroup runtime
	class MUTABLERUNTIME_API Instance : public Resource
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		Instance();

        //! Clone this instance
        Ptr<Instance> Clone() const;

		// Resource interface
		int32 GetDataSize() const override;

		//-----------------------------------------------------------------------------------------
		// Own interface
		//-----------------------------------------------------------------------------------------

        //! Type for the instance unique identifiers.
        typedef uint32 ID;

        //! Get a unique identifier for this instance. It doesn't change during the entire
        //! lifecycle of each instance. This identifier can be used in the System methods to update
        //! or release the instance.
        Instance::ID GetId() const;

		//! Get the number of levels-of-detail of this instance.
		int32 GetLODCount() const;

        //! Get the number of components in a level-of-detail
        //! \param lod Index of the level of detail, from 0 to GetLODCount()-1
        int32 GetComponentCount( int32 lod ) const;
		
		//! Get the Id of a component
		//! \param lod Index of the level of detail, from 0 to GetLODCount()-1
		//! \param comp Index of the component, from 0 to GetComponentCount(lod)-1
		uint16 GetComponentId( int32 lod, int32 comp ) const;

        //! Get the number of surfaces in a component
        //! \param lod Index of the level of detail, from 0 to GetLODCount()-1
        //! \param comp Index of the component, from 0 to GetComponentCount(lod)-1
        int32 GetSurfaceCount( int32 lod, int32 comp ) const;

        //! Get an id that can be used to match the surface data with the mesh surface data.
        //! \param lod Index of the level of detail, from 0 to GetLODCount()-1
        //! \param comp Index of the component, from 0 to GetComponentCount(lod)-1
        //! \param surf Index of the surface, from 0 to GetSurfaceCount(lod,comp)-1
        uint32 GetSurfaceId( int32 lod, int32 comp, int32 surf ) const;

        //! Find a surface index from the internal id (as returned by GetSurfaceId).
        //! \param lod Index of the level of detail, from 0 to GetLODCount()-1
        //! \param comp Index of the component, from 0 to GetComponentCount(lod)-1
        //! \param id ID of the surface to look for.
        int32 FindSurfaceById( int32 lod, int32 comp, uint32 id ) const;

		//! Find the base surface index and Lod index when reusing surfaces between LODs. Return the surface index
		//! and the LOD it belongs to.
		//! \param Comp - Index of the component, from 0 to GetComponentCount(lod)-1
		//! \param SharedSurfaceId - Id of the surface to look for (as returned by GetSharedSurfaceId).
		//! \param OutSurfaceIndex - Index of the surface in the OutLODIndex lod. 
		//! \param OutLODIndex - Index of the first LOD where the surface can be found. 
		void FindBaseSurfaceBySharedId(int32 CompIndex, int32 SharedId, int32& OutSurfaceIndex, int32& OutLODIndex) const;

		//! Get an id that can be used to find the same surface on other LODs
		//! \param lod Index of the level of detail, from 0 to GetLODCount()-1
		//! \param comp Index of the component, from 0 to GetComponentCount(lod)-1
		//! \param surf Index of the surface, from 0 to GetSurfaceCount(lod,comp)-1
		int32 GetSharedSurfaceId(int32 lod, int32 comp, int32 surf) const;

        //! Get an optional, opaque application-defined identifier for this surface. The meaning of
        //! this ID depends on each application, and it is specified when creating the source data
        //! that generates this surface.
        //! See NodeSurfaceNew::SetCustomID.
		//! \param lod Index of the level of detail, from 0 to GetLODCount()-1
		//! \param comp Index of the component, from 0 to GetComponentCount(lod)-1
        //! \param surf Index of the surface, from 0 to GetSurfaceCount(lod,comp)-1
        uint32 GetSurfaceCustomId( int32 lod, int32 comp, int32 surf ) const;

        //! Get the number of meshes in a surface
		//! \param lod Index of the level of detail, from 0 to GetLODCount()-1
		//! \param comp Index of the component, from 0 to GetComponentCount(lod)-1
        int32 GetMeshCount( int32 lod, int32 comp ) const;

        //! Get a mesh resource id from a component
        //! \param lod Index of the level of detail, from 0 to GetLODCount()-1
        //! \param comp Index of the component, from 0 to GetComponentCount(lod)-1
        //! \param mesh Index of the mesh, from 0 to GetMeshCount(lod,comp,surf)-1
        //! \return an ummodifiable pointer to the requested mesh. The returned object is guaranteed
        //! to be alive only while this instance is alive. The ownership of the returned object
        //! remains in the instance, so it should not be deleted.
		FResourceID GetMeshId( int32 lod, int32 comp, int32 mesh ) const;

		//! Get the number of images in a component
		//! \param lod Index of the level of detail, from 0 to GetLODCount()
		//! \param comp Index of the component, from 0 to GetComponentCount(lod)
        //! \param surf Index of the surface, from 0 to GetSurfaceCount(lod,comp)-1
        int32 GetImageCount( int32 lod, int32 comp, int32 surf ) const;

        //! Get an image resource id from a component
		//! \param lod Index of the level of detail, from 0 to GetLODCount()-1
		//! \param comp Index of the component, from 0 to GetComponentCount(lod)-1
        //! \param surf Index of the surface, from 0 to GetSurfaceCount(lod,comp)-1
        //! \param img Index of the image, from 0 to GetImageCount(lod,comp,surf)-1
        //! \return an ummodifiable pointer to the requested image. The returned object is
        //! guaranteed to be alive only while this instance is alive. The ownership of the returned
        //! object remains in the instance, so it should not be deleted.
		FResourceID GetImageId( int32 lod, int32 comp, int32 surf, int32 img ) const;

		//! Get the name of an image in a component
		//! \param lod Index of the level of detail, from 0 to GetLODCount()-1
		//! \param comp Index of the component, from 0 to GetComponentCount(lod)-1
        //! \param surf Index of the surface, from 0 to GetSurfaceCount(lod,comp)-1
        //! \param img Index of the image, from 0 to GetImageCount(lod,comp,surf)-1
		FName GetImageName( int32 lod, int32 comp, int32 surf, int32 img ) const;

		//! Get the number of vectors in a component
		//! \param lod Index of the level of detail, from 0 to GetLODCount()
		//! \param comp Index of the component, from 0 to GetComponentCount(lod)
        //! \param surf Index of the surface, from 0 to GetSurfaceCount(lod,comp)-1
        int32 GetVectorCount( int32 lod, int32 comp, int32 surf ) const;

		//! Get a vector from a component
		//! \param lod Index of the level of detail, from 0 to GetLODCount()-1
		//! \param comp Index of the component, from 0 to GetComponentCount(lod)-1
        //! \param surf Index of the surface, from 0 to GetSurfaceCount(lod,comp)-1
        //! \param vec Index of the vector, from 0 to GetVectorCount(lod,comp,surf)-1
		//! \param pX, pY, pZ, pW are optional to floats to store the components of the vector
		//!		   value. They can be null.
        FVector4f GetVector( int32 lod, int32 comp, int32 surf, int32 vec ) const;

		//! Get the name of a vector in a component
		//! \param lod Index of the level of detail, from 0 to GetLODCount()-1
		//! \param comp Index of the component, from 0 to GetComponentCount(lod)-1
        //! \param surf Index of the surface, from 0 to GetSurfaceCount(lod,comp)-1
        //! \param vec Index of the vector, from 0 to GetVectorCount(lod,comp)-1
		FName GetVectorName( int32 lod, int32 comp, int32 surf, int32 vec ) const;

        //! Get the number of scalar values in a component
        //! \param lod Index of the level of detail, from 0 to GetLODCount()
        //! \param comp Index of the component, from 0 to GetComponentCount(lod)
        //! \param surf Index of the surface, from 0 to GetSurfaceCount(lod,comp)-1
        int32 GetScalarCount( int32 lod, int32 comp, int32 surf ) const;

        //! Get a scalar value from a component
        //! \param lod Index of the level of detail, from 0 to GetLODCount()-1
        //! \param comp Index of the component, from 0 to GetComponentCount(lod)-1
        //! \param surf Index of the surface, from 0 to GetSurfaceCount(lod,comp)-1
        //! \param sca Index of the scalar, from 0 to GetScalarCount(lod,comp)-1
        float GetScalar( int32 lod, int32 comp, int32 surf, int32 sca ) const;

        //! Get the name of a scalar from a component
        //! \param lod Index of the level of detail, from 0 to GetLODCount()-1
        //! \param comp Index of the component, from 0 to GetComponentCount(lod)-1
        //! \param surf Index of the surface, from 0 to GetSurfaceCount(lod,comp)-1
        //! \param sca Index of the scalar, from 0 to GetScalarCount(lod,comp)-1
		FName GetScalarName( int32 lod, int32 comp, int32 surf, int32 sca ) const;

        //! Get the number of string values in a component
        //! \param lod Index of the level of detail, from 0 to GetLODCount()
        //! \param comp Index of the component, from 0 to GetComponentCount(lod)
        //! \param surf Index of the surface, from 0 to GetSurfaceCount(lod,comp)-1
        int32 GetStringCount( int32 lod, int32 comp, int32 surf ) const;

        //! Get a string value from a component
        //! \param lod Index of the level of detail, from 0 to GetLODCount()-1
        //! \param comp Index of the component, from 0 to GetComponentCount(lod)-1
        //! \param surf Index of the surface, from 0 to GetSurfaceCount(lod,comp)-1
        //! \param sca Index of the string, from 0 to GetStringCount(lod,comp)-1
        FString GetString( int32 lod, int32 comp, int32 surf, int32 str ) const;

        //! Get the name of a string from a component
        //! \param lod Index of the level of detail, from 0 to GetLODCount()-1
        //! \param comp Index of the component, from 0 to GetComponentCount(lod)-1
        //! \param surf Index of the surface, from 0 to GetSurfaceCount(lod,comp)-1
        //! \param sca Index of the string, from 0 to GetStringCount(lod,comp)-1
        FName GetStringName( int32 lod, int32 comp, int32 surf, int32 str ) const;

		//! Get the number of ExtensionData values in a component
		int32 GetExtensionDataCount() const;

		//! Get an ExtensionData value from a component
		//! \param Index Index of the ExtensionData to fetch
		//! \param OutExtensionData Receives the ExtensionData
		//! \param OutName Receives the name associated with the ExtensionData. Guaranteed to be a valid string of non-zero length.
		void GetExtensionData(int32 Index, Ptr<const class ExtensionData>& OutExtensionData, FName& OutName) const;

        //-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;

		Private* GetPrivate() const;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~Instance();

	private:

		Private* m_pD;

	};


}

