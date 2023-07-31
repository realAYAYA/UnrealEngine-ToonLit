// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/PlatformMath.h"
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
	class MUTABLERUNTIME_API Instance : public RefCounted
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		Instance();

        //! Clone this instance
        InstancePtr Clone() const;


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
		int GetLODCount() const;

        //! Get the number of components in a level-of-detail
        //! \param lod Index of the level of detail, from 0 to GetLODCount()-1
        int GetComponentCount( int lod ) const;

        //! Get the name of a component
        //! \param lod Index of the level of detail, from 0 to GetLODCount()-1
        //! \param comp Index of the component, from 0 to GetComponentCount(lod)-1
        const char* GetComponentName( int lod, int comp ) const;
		
		//! Get the Id of a component
		//! \param lod Index of the level of detail, from 0 to GetLODCount()-1
		//! \param comp Index of the component, from 0 to GetComponentCount(lod)-1
		uint16 GetComponentId( int lod, int comp ) const;

        //! Get the number of surfaces in a component
        //! \param lod Index of the level of detail, from 0 to GetLODCount()-1
        //! \param comp Index of the component, from 0 to GetComponentCount(lod)-1
        int GetSurfaceCount( int lod, int comp ) const;

        //! Get the name of a surface
        //! \param lod Index of the level of detail, from 0 to GetLODCount()-1
        //! \param comp Index of the component, from 0 to GetComponentCount(lod)-1
        //! \param surf Index of the surface, from 0 to GetSurfaceCount(lod,comp)-1
        const char* GetSurfaceName( int lod, int comp, int surf ) const;

        //! Get an id that can be used to match the surface data with the mesh surface data.
        //! \param lod Index of the level of detail, from 0 to GetLODCount()-1
        //! \param comp Index of the component, from 0 to GetComponentCount(lod)-1
        //! \param surf Index of the surface, from 0 to GetSurfaceCount(lod,comp)-1
        uint32_t GetSurfaceId( int lod, int comp, int surf ) const;

        //! Find a surface index from the internal id (as returned by GetSurfaceId).
        //! \param lod Index of the level of detail, from 0 to GetLODCount()-1
        //! \param comp Index of the component, from 0 to GetComponentCount(lod)-1
        //! \param id ID of the surface to look for.
        int FindSurfaceById( int lod, int comp, uint32_t id ) const;

        //! Get an optional, opaque application-defined identifier for this surface. The meaning of
        //! this ID depends on each application, and it is specified when creating the source data
        //! that generates this surface.
        //! See NodeSurfaceNew::SetCustomID.
		//! \param lod Index of the level of detail, from 0 to GetLODCount()-1
		//! \param comp Index of the component, from 0 to GetComponentCount(lod)-1
        //! \param surf Index of the surface, from 0 to GetSurfaceCount(lod,comp)-1
        uint32_t GetSurfaceCustomId( int lod, int comp, int surf ) const;

        //! Get the number of meshes in a surface
		//! \param lod Index of the level of detail, from 0 to GetLODCount()-1
		//! \param comp Index of the component, from 0 to GetComponentCount(lod)-1
        int GetMeshCount( int lod, int comp ) const;

        //! Get a mesh resource id from a component
        //! \param lod Index of the level of detail, from 0 to GetLODCount()-1
        //! \param comp Index of the component, from 0 to GetComponentCount(lod)-1
        //! \param mesh Index of the mesh, from 0 to GetMeshCount(lod,comp,surf)-1
        //! \return an ummodifiable pointer to the requested mesh. The returned object is guaranteed
        //! to be alive only while this instance is alive. The ownership of the returned object
        //! remains in the instance, so it should not be deleted.
        RESOURCE_ID GetMeshId( int lod, int comp, int mesh ) const;

		//! Get the name of a mesh in a component
		//! \param lod Index of the level of detail, from 0 to GetLODCount()-1
		//! \param comp Index of the component, from 0 to GetComponentCount(lod)-1
        //! \param mesh Index of the mesh, from 0 to GetMeshCount(lod,comp,surf)-1
        const char* GetMeshName( int lod, int comp, int mesh ) const;

		//! Get the number of images in a component
		//! \param lod Index of the level of detail, from 0 to GetLODCount()
		//! \param comp Index of the component, from 0 to GetComponentCount(lod)
        //! \param surf Index of the surface, from 0 to GetSurfaceCount(lod,comp)-1
        int GetImageCount( int lod, int comp, int surf ) const;

        //! Get an image resource id from a component
		//! \param lod Index of the level of detail, from 0 to GetLODCount()-1
		//! \param comp Index of the component, from 0 to GetComponentCount(lod)-1
        //! \param surf Index of the surface, from 0 to GetSurfaceCount(lod,comp)-1
        //! \param img Index of the image, from 0 to GetImageCount(lod,comp,surf)-1
        //! \return an ummodifiable pointer to the requested image. The returned object is
        //! guaranteed to be alive only while this instance is alive. The ownership of the returned
        //! object remains in the instance, so it should not be deleted.
        RESOURCE_ID GetImageId( int lod, int comp, int surf, int img ) const;

		//! Get the name of an image in a component
		//! \param lod Index of the level of detail, from 0 to GetLODCount()-1
		//! \param comp Index of the component, from 0 to GetComponentCount(lod)-1
        //! \param surf Index of the surface, from 0 to GetSurfaceCount(lod,comp)-1
        //! \param img Index of the image, from 0 to GetImageCount(lod,comp,surf)-1
        const char* GetImageName( int lod, int comp, int surf, int img ) const;

		//! Get the number of vectors in a component
		//! \param lod Index of the level of detail, from 0 to GetLODCount()
		//! \param comp Index of the component, from 0 to GetComponentCount(lod)
        //! \param surf Index of the surface, from 0 to GetSurfaceCount(lod,comp)-1
        int GetVectorCount( int lod, int comp, int surf ) const;

		//! Get a vector from a component
		//! \param lod Index of the level of detail, from 0 to GetLODCount()-1
		//! \param comp Index of the component, from 0 to GetComponentCount(lod)-1
        //! \param surf Index of the surface, from 0 to GetSurfaceCount(lod,comp)-1
        //! \param vec Index of the vector, from 0 to GetVectorCount(lod,comp,surf)-1
		//! \param pX, pY, pZ, pW are optional to floats to store the components of the vector
		//!		   value. They can be null.
        void GetVector( int lod, int comp, int surf, int vec,
						float* pX, float* pY, float* pZ, float* pW ) const;

		//! Get the name of a vector in a component
		//! \param lod Index of the level of detail, from 0 to GetLODCount()-1
		//! \param comp Index of the component, from 0 to GetComponentCount(lod)-1
        //! \param surf Index of the surface, from 0 to GetSurfaceCount(lod,comp)-1
        //! \param vec Index of the vector, from 0 to GetVectorCount(lod,comp)-1
        const char* GetVectorName( int lod, int comp, int surf, int vec ) const;

        //! Get the number of scalar values in a component
        //! \param lod Index of the level of detail, from 0 to GetLODCount()
        //! \param comp Index of the component, from 0 to GetComponentCount(lod)
        //! \param surf Index of the surface, from 0 to GetSurfaceCount(lod,comp)-1
        int GetScalarCount( int lod, int comp, int surf ) const;

        //! Get a scalar value from a component
        //! \param lod Index of the level of detail, from 0 to GetLODCount()-1
        //! \param comp Index of the component, from 0 to GetComponentCount(lod)-1
        //! \param surf Index of the surface, from 0 to GetSurfaceCount(lod,comp)-1
        //! \param sca Index of the scalar, from 0 to GetScalarCount(lod,comp)-1
        float GetScalar( int lod, int comp, int surf, int sca ) const;

        //! Get the name of a scalar from a component
        //! \param lod Index of the level of detail, from 0 to GetLODCount()-1
        //! \param comp Index of the component, from 0 to GetComponentCount(lod)-1
        //! \param surf Index of the surface, from 0 to GetSurfaceCount(lod,comp)-1
        //! \param sca Index of the scalar, from 0 to GetScalarCount(lod,comp)-1
        const char* GetScalarName( int lod, int comp, int surf, int sca ) const;

        //! Get the number of string values in a component
        //! \param lod Index of the level of detail, from 0 to GetLODCount()
        //! \param comp Index of the component, from 0 to GetComponentCount(lod)
        //! \param surf Index of the surface, from 0 to GetSurfaceCount(lod,comp)-1
        int GetStringCount( int lod, int comp, int surf ) const;

        //! Get a string value from a component
        //! \param lod Index of the level of detail, from 0 to GetLODCount()-1
        //! \param comp Index of the component, from 0 to GetComponentCount(lod)-1
        //! \param surf Index of the surface, from 0 to GetSurfaceCount(lod,comp)-1
        //! \param sca Index of the string, from 0 to GetStringCount(lod,comp)-1
        const char* GetString( int lod, int comp, int surf, int str ) const;

        //! Get the name of a string from a component
        //! \param lod Index of the level of detail, from 0 to GetLODCount()-1
        //! \param comp Index of the component, from 0 to GetComponentCount(lod)-1
        //! \param surf Index of the surface, from 0 to GetSurfaceCount(lod,comp)-1
        //! \param sca Index of the string, from 0 to GetStringCount(lod,comp)-1
        const char* GetStringName( int lod, int comp, int surf, int str ) const;


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

