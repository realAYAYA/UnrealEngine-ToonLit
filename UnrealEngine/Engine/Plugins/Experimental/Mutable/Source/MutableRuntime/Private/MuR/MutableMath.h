// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Types.h"
#include "MuR/SerialisationPrivate.h"
#include "MuR/Platform.h"
#include "Math/Matrix.h"
#include "Math/IntVector.h"


namespace mu
{

	// Unreal POD Serializables
	MUTABLE_DEFINE_POD_SERIALISABLE(FUintVector2)
	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(FUintVector2)

    //---------------------------------------------------------------------------------------------
    //! Find the first larger-or-equal power of 2 of the given number.
    //---------------------------------------------------------------------------------------------
    inline int ceilPow2( int v )
    {
        int r = 0;

        if (v>0)
        {
            r = 1;
            while( r < v  )
            {
                r <<= 1;
            }
        }

        return r;
    }


    //---------------------------------------------------------------------------------------------
    //!
    //---------------------------------------------------------------------------------------------
    template<class T>
    T clamp( const T mi, const T ma, const T v )
    {
        return FMath::Max( mi, FMath::Min( ma, v ) );
    }


	//---------------------------------------------------------------------------------------------
	// 16-bit floating point number support.
	//---------------------------------------------------------------------------------------------
    typedef uint16 float16;

	//---------------------------------------------------------------------------------------------
    inline uint32 halfToFloatI( float16 y )
	{
		int s = (y >> 15) & 0x00000001;
		int e = (y >> 10) & 0x0000001f;
		int f =  y        & 0x000003ff;

		if (e == 0)
		{
			if (f == 0)
			{
				return s << 31;
			}
			else
			{
				while (!(f & 0x00000400))
				{
					f <<= 1;
					e -=  1;
				}
				e += 1;
				f &= ~0x00000400;
			}
		}
		else if (e == 31)
		{
			if (f == 0)
			{
				return (s << 31) | 0x7f800000;
			}
			else
			{
				return (s << 31) | 0x7f800000 | (f << 13);
			}
		}

		e = e + (127 - 15);
		f = f << 13;

		return ((s << 31) | (e << 23) | f);
	}


	//---------------------------------------------------------------------------------------------
	inline float halfToFloat( float16 y)
	{
		union
		{
			float f;
            uint32 i;
		} v;

		v.i = halfToFloatI(y);
		return v.f;
	}


	//---------------------------------------------------------------------------------------------
    inline float16 floatToHalfI( uint32 i )
	{
		int s =  (i >> 16) & 0x00008000;
		int e = ((i >> 23) & 0x000000ff) - (127 - 15);
		int f =   i        & 0x007fffff;

		if (e <= 0)
		{
			if (e < -10)
			{
				if (s)
				{
					return 0x8000;
				}
				else
				{
				   return 0;
				}
			}
			f = (f | 0x00800000) >> (1 - e);
			return float16( s | (f >> 13) );
		}
		else if (e == 0xff - (127 - 15))
		{
			if (f == 0)
			{
				return float16(s | 0x7c00 );
			}
			else
			{
				f >>= 13;
				return float16(s | 0x7c00 | f | (f == 0));
			}
		}
		else
		{
			if (e > 30)
			{
				return float16( s | 0x7c00 );
			}
			return float16(s | (e << 10) | (f >> 13) );
		}
	}


	//---------------------------------------------------------------------------------------------
	inline float16 floatToHalf(float i)
	{
		union
		{
			float f;
            uint32 i;
		} v;

		v.f = i;
		return floatToHalfI(v.i);
	}



	//---------------------------------------------------------------------------------------------
	//! Generic vector template.
	//---------------------------------------------------------------------------------------------
	template <class SCALAR, int DIM>
    class vec
	{
	protected:

		SCALAR m[ DIM ];

	public:

		//-----------------------------------------------------------------------------------------
		typedef vec<SCALAR,DIM> V;

        //-----------------------------------------------------------------------------------------
        inline vec()
        {
            for( int i=0; i<DIM; ++i )
            {
                m[i]=0;
            }
        }


        inline vec( const V& o )
        {
            for( int i=0; i<DIM; ++i )
            {
                m[i]=o.m[i];
            }
        }

		//-----------------------------------------------------------------------------------------
		static inline int GetDim()
		{
			return DIM;
		}

		//-----------------------------------------------------------------------------------------
		inline SCALAR& operator[] (int i)
		{
			check( i>=0 && i <DIM );
			return m[i];
		}

		//-----------------------------------------------------------------------------------------
		inline const SCALAR& operator[] (int i) const
		{
			check( i>=0 && i <DIM );
			return m[i];
		}

		//-----------------------------------------------------------------------------------------
		inline bool operator==( const V& other ) const
		{
			bool res = true;
			for( int i=0; i<DIM; ++i )
			{
				res &= m[i] == other.m[i];
			}
			return res;
		}

		//-----------------------------------------------------------------------------------------
		inline bool operator!=( const V& other ) const
		{
			bool res = false;
			for( int i=0; i<DIM; ++i )
			{
				res |= m[i] != other.m[i];
			}
			return res;
		}

		//-----------------------------------------------------------------------------------------
		inline V operator+( const V& other ) const
		{
			V res;
			for( int i=0; i<DIM; ++i )
			{
				res.m[i] = m[i] + other.m[i];
			}
			return res;
		}

		//-----------------------------------------------------------------------------------------
		inline void operator+=( const V& other )
		{
			for( int i=0; i<DIM; ++i )
			{
				m[i] += other.m[i];
			}
		}

        //-----------------------------------------------------------------------------------------
        inline V operator-( const V& other ) const
        {
            V res;
            for( int i=0; i<DIM; ++i )
            {
                res.m[i] = m[i] - other.m[i];
            }
            return res;
        }

        //-----------------------------------------------------------------------------------------
        inline V operator-( SCALAR v ) const
        {
            V res;
            for( int i=0; i<DIM; ++i )
            {
                res.m[i] = m[i] - v;
            }
            return res;
        }

		//-----------------------------------------------------------------------------------------
		inline void operator-=( const V& other )
		{
			for( int i=0; i<DIM; ++i )
			{
                m[i] -= other.m[i];
			}
		}

		//-----------------------------------------------------------------------------------------
		inline V operator*( const V& other ) const
		{
			V res;
			for( int i=0; i<DIM; ++i )
			{
				res.m[i] = m[i] * other.m[i];
			}
			return res;
		}

		//-----------------------------------------------------------------------------------------
		inline V operator*( SCALAR s ) const
		{
			V res;
			for( int i=0; i<DIM; ++i )
			{
				res.m[i] = m[i] * s;
			}
			return res;
		}

		//-----------------------------------------------------------------------------------------
		inline V operator/( const V& other ) const
		{
			V res;
			for( int i=0; i<DIM; ++i )
			{
				res.m[i] = m[i] / other.m[i];
			}
			return res;
		}

		//-----------------------------------------------------------------------------------------
		inline V operator/( SCALAR s ) const
		{
			V res;
			for( int i=0; i<DIM; ++i )
			{
				res.m[i] = m[i] / s;
			}
			return res;
		}		

        //-----------------------------------------------------------------------------------------
        inline void operator*=( SCALAR s )
        {
            for( int i=0; i<DIM; ++i )
            {
                m[i] *= s;
            }
        }

        //-----------------------------------------------------------------------------------------
        inline void operator/=( SCALAR s )
        {
            for( int i=0; i<DIM; ++i )
            {
                m[i] /= s;
            }
        }

        //-----------------------------------------------------------------------------------------
		//! Per component min
        //-----------------------------------------------------------------------------------------
		inline static V min(const V& a, const V& b)
		{
			V r;
			for (size_t i = 0; i < DIM; ++i)
			{
				r.m[i] = FMath::Min(a.m[i], b.m[i]);
			}
			return r;
		}

		inline static V max(const V& a, const V& b)
		{
			V r;
			for (size_t i = 0; i < DIM; ++i)
			{
				r.m[i] = FMath::Max(a.m[i], b.m[i]);
			}
			return r;
		}

		//-----------------------------------------------------------------------------------------
		//! Strict ordering of vectors.
		//-----------------------------------------------------------------------------------------
		inline bool
		operator<(const V& other) const
        {
			bool equal = true;
			bool res = false;
			for( int i=0; equal && i<DIM; ++i )
			{
				if( m[i] < other.m[i] )
				{
					equal = false;
					res = true;
				}
				else if( m[i] > other.m[i] )
				{
					equal = false;
					res = false;
				}
			}
			return res;
		}


        //-----------------------------------------------------------------------------------------
        inline bool AllSmallerThan( const V& other ) const
        {
            bool res = true;
            for( int i=0; i<DIM; ++i )
            {
                res &= m[i] < other.m[i];
            }
            return res;
        }


        //-----------------------------------------------------------------------------------------
        inline bool AllSmallerOrEqualThan( const V& other ) const
        {
            bool res = true;
            for( int i=0; i<DIM; ++i )
            {
                res &= m[i] <= other.m[i];
            }
            return res;
        }

		//-----------------------------------------------------------------------------------------
		inline bool AllGreaterOrEqualThan( const V& other ) const
		{
			bool res = true;
			for( int i=0; i<DIM; ++i )
			{
				res &= m[i] >= other.m[i];
			}
			return res;
		}

		//-----------------------------------------------------------------------------------------
		inline bool AlmostEqual
			(
				const V& other,
				SCALAR tol = std::numeric_limits<SCALAR>::min()
			) const
		{
			bool res = true;
			for( int i=0; i<DIM; ++i )
			{
				res &= fabs(m[i]-other.m[i])<tol;
			}
			return res;
		}

		//-----------------------------------------------------------------------------------------
		inline bool AlmostNull( SCALAR tol = std::numeric_limits<SCALAR>::min() ) const
		{
			bool res = true;
			for( int i=0; i<DIM; ++i )
			{
				res &= fabs(m[i])<tol;
			}
			return res;
		}

		//-----------------------------------------------------------------------------------------
		inline void Serialise( OutputArchive& arch ) const
		{
			for( int i=0; i<DIM; ++i )
			{
				arch << m[i];
			}
		}

		//-----------------------------------------------------------------------------------------
		inline void Unserialise( InputArchive& arch )
		{
			for( int i=0; i<DIM; ++i )
			{
				arch >> m[i];
			}
		}

	};

    //!
    template <class SCALAR, int DIM>
    inline vec<SCALAR,DIM> clamp( const vec<SCALAR,DIM>& v, SCALAR mi, SCALAR  ma )
    {
        vec<SCALAR,DIM> res;
        for( int i=0; i<DIM; ++i )
        {
            res[i] = clamp( mi, ma, v[i] );
        }
        return res;
    }

    //!
    template <class SCALAR, int DIM>
    inline vec<SCALAR,DIM> lerp( const vec<SCALAR,DIM>& f0, const vec<SCALAR,DIM>& f1, float t )
    {
        const float s = 1.0f - t;
        return f0 * s + f1 * t;
    }

	//!
	template <class _SCALAR>
	class vec2 : public vec<_SCALAR,2>
	{
	public:
        typedef _SCALAR SCALAR;

        //-----------------------------------------------------------------------------------------
		inline vec2()
		{
			// Elements are cleared in the parent constructor
		}

        //-----------------------------------------------------------------------------------------
        inline vec2( SCALAR x, SCALAR y )
        {
            this->m[0] = x;
            this->m[1] = y;
        }

        //-----------------------------------------------------------------------------------------
        inline vec2( const vec2& ) = default;

		//-----------------------------------------------------------------------------------------
		template<class SCALAR2>
        inline vec2( const vec<SCALAR2,2>& v )
        {
			this->m[0] = v[0];
			this->m[1] = v[1];
		}

        //-----------------------------------------------------------------------------------------
        inline SCALAR& x()
        {
            return this->m[0];
        }

        //-----------------------------------------------------------------------------------------
        inline SCALAR& y()
        {
            return this->m[1];
        }

        //-----------------------------------------------------------------------------------------
        inline const SCALAR& x() const
        {
            return this->m[0];
        }

        //-----------------------------------------------------------------------------------------
        inline const SCALAR& y() const
        {
            return this->m[1];
        }
	};

    //!
    typedef vec2<float> vec2f;
    MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE( vec2f );

	//!
	template <class SCALAR>
	class vec3 : public vec<SCALAR,3>
	{
	public:

		//-----------------------------------------------------------------------------------------
		inline vec3()
		{
			// Elements are cleared in the parent constructor
		}

		//-----------------------------------------------------------------------------------------
		inline vec3( SCALAR x, SCALAR y, SCALAR z )
		{
			this->m[0] = x;
			this->m[1] = y;
			this->m[2] = z;
		}

		//-----------------------------------------------------------------------------------------
		inline vec3( const vec<SCALAR,3>& other )
		{
			this->m[0] = other[0];
			this->m[1] = other[1];
			this->m[2] = other[2];
		}

        //-----------------------------------------------------------------------------------------
        inline void Set( SCALAR x, SCALAR y, SCALAR z )
        {
            this->m[0] = x;
            this->m[1] = y;
            this->m[2] = z;
        }


        //-----------------------------------------------------------------------------------------
        inline SCALAR& x()
        {
            return this->m[0];
        }

        //-----------------------------------------------------------------------------------------
        inline SCALAR& y()
        {
            return this->m[1];
        }

        //-----------------------------------------------------------------------------------------
        inline SCALAR& z()
        {
            return this->m[2];
        }

        //-----------------------------------------------------------------------------------------
        inline const SCALAR& x() const
        {
            return this->m[0];
        }

        //-----------------------------------------------------------------------------------------
        inline const SCALAR& y() const
        {
            return this->m[1];
        }

        //-----------------------------------------------------------------------------------------
        inline const SCALAR& z() const
        {
            return this->m[2];
        }

        //-----------------------------------------------------------------------------------------
        inline vec2<SCALAR> xy() const
        {
            return vec2<SCALAR>( this->m[0], this->m[1] );
        }

	};

	//!
	typedef vec3<float> vec3f;
    MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE( vec3f );


	//---------------------------------------------------------------------------------------------
	//! Cross product of 3D vectors
	//---------------------------------------------------------------------------------------------
	template <class SCALAR>
	inline vec3<SCALAR> cross( const vec3<SCALAR>& a, const vec3<SCALAR>& b )
	{
		return vec3<SCALAR>
				(
					a[1] * b[2] - a[2] * b[1],
					a[2] * b[0] - a[0] * b[2],
					a[0] * b[1] - a[1] * b[0]
				);
	}


	//---------------------------------------------------------------------------------------------
	//! Dot product of 3D vectors
	//---------------------------------------------------------------------------------------------
	template <class SCALAR, int DIM >
	inline SCALAR dot( const vec<SCALAR, DIM>& a, const vec<SCALAR,DIM>& b )
	{
		SCALAR res = 0;

		for ( int i=0; i<DIM; ++i )
		{
			res += a[i]*b[i];
		}

		return res;
	}


	//-----------------------------------------------------------------------------------------
	//! Length of a vector
	//-----------------------------------------------------------------------------------------
	template <class SCALAR, int DIM>
	inline SCALAR length( const vec<SCALAR,DIM>& v )
	{
		SCALAR l = dot( v, v );
		l = sqrtf(l);
		return l;
	}


	//---------------------------------------------------------------------------------------------
	//! Normalise a vector
	//---------------------------------------------------------------------------------------------
	template <class SCALAR, int DIM>
	inline vec<SCALAR,DIM> normalise( const vec<SCALAR,DIM>& a )
	{
		float l = length( a );
		if ( l > std::numeric_limits<SCALAR>::epsilon() )
		{
			return a/l;
		}
		return a;
	}


	//---------------------------------------------------------------------------------------------
	//! Normalise a vector, assuming that its length won't be null
	//---------------------------------------------------------------------------------------------
	template <class SCALAR, int DIM>
	inline vec<SCALAR,DIM> normalise_unsafe( const vec<SCALAR,DIM>& a )
	{
		float l = length( a );
		return a/l;
	}

	//---------------------------------------------------------------------------------------------
	//! Point to plane distance
	//---------------------------------------------------------------------------------------------
	template <class SCALAR, int DIM>
	inline float point_to_plane_dist(const vec<SCALAR, DIM> &point, const vec<SCALAR, DIM> &plane_origin, const vec<SCALAR, DIM> &plane_normal)
	{
		return dot((point - plane_origin), plane_normal);
	}

	//---------------------------------------------------------------------------------------------
	//! Ray plane intersection
	//---------------------------------------------------------------------------------------------
	inline vec3f ray_plane_intersection(const vec3f& ray_start, const vec3f& ray_end, const vec3f& plane_origin, const vec3f &plane_normal)
	{
		return ray_start + (ray_end - ray_start) *	(dot((plane_origin - ray_start), plane_normal) / dot((ray_end - ray_start), plane_normal));
	}

    //constexpr float point_in_plane_eps = 0.0001f;

    inline bool point_in_face(const vec3f& InVtx, const vec3f& normal, vec3f vertices[3], int& out_intersected_vert, int& out_intersected_edge_v0, int& out_intersected_edge_v1, float point_in_plane_eps = 0.0001f)
    {
        vec3f SidePlaneNormal;
        vec3f Side;
        bool intersected_edge[3] = { false, false, false };
        int intersected_edge_count = 0;

        for (int x = 0; x < 3; ++x)
        {
            // Create plane perpendicular to both this side and the polygon's normal.
            Side = vertices[x] - vertices[(x - 1 < 0) ? 3 - 1 : x - 1];
            SidePlaneNormal = cross<float>(Side, normalise(normal));
            normalise(SidePlaneNormal);

            float dist = point_to_plane_dist(InVtx, vertices[x], SidePlaneNormal);

            if (dist > point_in_plane_eps)
            {
                return false; // If point is not behind all the planes created by this polys edges, it's outside the poly.
            }
            else if (dist > -point_in_plane_eps)
            {
                intersected_edge[x] = true;
                intersected_edge_count++;
            }
        }

        out_intersected_vert = -1;
        out_intersected_edge_v0 = -1;
        out_intersected_edge_v1 = -1;

        if (intersected_edge_count > 0)
        {
            if (intersected_edge_count == 1) // Only one edge intersected
            {
                if (intersected_edge[0])
                {
                    out_intersected_edge_v0 = 0;
                    out_intersected_edge_v1 = 2;
                }
                else if (intersected_edge[1])
                {
                    out_intersected_edge_v0 = 0;
                    out_intersected_edge_v1 = 1;
                }
                else if (intersected_edge[2])
                {
                    out_intersected_edge_v0 = 1;
                    out_intersected_edge_v1 = 2;
                }
            }
            else if (intersected_edge_count == 2) // Two edges intersected means it actually hit a vertex
            {
                if (intersected_edge[0] && intersected_edge[1])
                {
                    out_intersected_vert = 0;
                }
                else if (intersected_edge[1] && intersected_edge[2])
                {
                    out_intersected_vert = 1;
                }
                else if (intersected_edge[2] && intersected_edge[0])
                {
                    out_intersected_vert = 2;
                }
            }
        }

        return true;
    }

    //---------------------------------------------------------------------------------------------
    inline bool rayIntersectsFace( const vec3f& ray_start, const vec3f& ray_end,
                                   const vec3f& v0, const vec3f& v1, const vec3f& v2,
                                   vec3f& out_intersection,
                                   int& out_intersected_vert, int& out_intersected_edge_v0,
                                   int& out_intersected_edge_v1, float point_in_face_epsilon = 0.0001f)
    {
        vec3f normal = cross<float>(v1 - v0, v2 - v0);

        // Does the ray cross the plane?
        const float DistStart = point_to_plane_dist(ray_start, v0, normal);
        const float DistEnd = point_to_plane_dist(ray_end, v0, normal);

        if ((DistStart < 0 && DistEnd < 0) || (DistStart > 0 && DistEnd > 0))
        {
            return false;
        }

        // Get the intersection of the line and the plane.
        out_intersection = ray_plane_intersection(ray_start, ray_end, v0, normal);

        // \todo: review the equality comparison. Is it doing anything without a tolerance?
        if (out_intersection == ray_start || out_intersection == ray_end)
        {
            return false;
        }

        // Check if the intersection point is actually on the poly.
        vec3f vertices[3] = { v0, v1, v2 };

        return point_in_face( out_intersection, normal, vertices,
                              out_intersected_vert,
                              out_intersected_edge_v0, out_intersected_edge_v1, point_in_face_epsilon);
    }


    //---------------------------------------------------------------------------------------------
    inline bool ray_plane_intersection( const vec3f& ray_start, const vec3f& ray_end,
                                        const vec3f& plane_origin, const vec3f &plane_normal,
                                        vec3f& out_intersection, float& out_t )
    {
        float den = mu::dot<float, 3>(ray_end - ray_start, plane_normal);

        if (den == 0.f)
        {
            return false;
        }

        out_t = mu::dot<float, 3>((plane_origin - ray_start), plane_normal) / den;
        out_intersection = ray_start + (ray_end - ray_start) * out_t;

        return true;
    }


    //---------------------------------------------------------------------------------------------
    inline bool rayIntersectsFace2(const vec3f& ray_start, const vec3f& ray_end,
                                   const vec3f& v0, const vec3f& v1, const vec3f& v2,
                                   vec3f& out_intersection,
                                   int& out_intersected_vert,
                                   int& out_intersected_edge_v0, int& out_intersected_edge_v1,
                                   float& out_t )
    {
        vec3f normal = cross<float>(v1 - v0, v2 - v0);

        // Does the ray cross the plane?
        const float DistStart = point_to_plane_dist(ray_start, v0, normal);
        const float DistEnd = point_to_plane_dist(ray_end, v0, normal);

        if ((DistStart < 0 && DistEnd < 0) || (DistStart > 0 && DistEnd > 0))
        {
            return false;
        }

        // Get the intersection of the line and the plane.
        if (!ray_plane_intersection(ray_start, ray_end, v0, normal, out_intersection, out_t))
        {
            return false;
        }

        // \todo: review the equality comparison. Is it doing anything without a tolerance?
        if (out_intersection == ray_start || out_intersection == ray_end)
        {
            return false;
        }

        // Check if the intersection point is actually on the poly.
        vec3f vertices[3] = { v0, v1, v2 };

        return point_in_face( out_intersection, normal, vertices, out_intersected_vert,
                              out_intersected_edge_v0, out_intersected_edge_v1 );
    }

	//---------------------------------------------------------------------------------------------
	//! Approximated inverse square root
	//---------------------------------------------------------------------------------------------
	inline float rsqrt_approx( float number )
	{
		union MUTABLE_FLOAT_INT
		{
            int32 i;
			float y;
		} fi;
		float x2;
		const float threehalfs = 1.5F;

		x2 = number * 0.5F;
		fi.y  = number;
		fi.i  = 0x5f3759df - ( fi.i >> 1 );
		fi.y  = fi.y * ( threehalfs - ( x2 * fi.y * fi.y ) );
		return fi.y;
	}


	//---------------------------------------------------------------------------------------------
	//! Normalise a vector, approximately
	//---------------------------------------------------------------------------------------------
	template <class SCALAR, int DIM>
	inline vec<SCALAR,DIM> normalise_approx( const vec<SCALAR,DIM>& a )
	{
		return a * rsqrt_approx( dot(a,a) );
	}


	//---------------------------------------------------------------------------------------------
	//! Angle between two vectors, in radians
	//---------------------------------------------------------------------------------------------
	inline float angle( const vec3f& a, const vec3f& b )
	{
		float s = sqrt( dot(a,a) * dot(b,b) );

		if ( s > std::numeric_limits<float>::min() )
		{
			return acos( dot(a,b) / s );
		}
		else
		{
			return 0.0f;
		}
	}


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------
	template <class SCALAR>
	class vec4 : public vec<SCALAR,4>
	{
	public:

		//-----------------------------------------------------------------------------------------
		inline vec4()
		{
			// Elements are cleared in the parent constructor
		}

		//-----------------------------------------------------------------------------------------
		inline vec4( SCALAR x, SCALAR y, SCALAR z, SCALAR w )
		{
			this->m[0] = x;
			this->m[1] = y;
			this->m[2] = z;
			this->m[3] = w;
		}

        //-----------------------------------------------------------------------------------------
        inline vec4( const vec<SCALAR,4>& other )
        {
            this->m[0] = other[0];
            this->m[1] = other[1];
            this->m[2] = other[2];
            this->m[3] = other[3];
        }

        //-----------------------------------------------------------------------------------------
        inline vec4( const vec<SCALAR,3>& other, float w )
        {
            this->m[0] = other[0];
            this->m[1] = other[1];
            this->m[2] = other[2];
            this->m[3] = w;
        }

        //-----------------------------------------------------------------------------------------
        inline vec3<SCALAR> xyz() const
        {
            return vec3<SCALAR>( this->m[0], this->m[1], this->m[2] );
        }

        //-----------------------------------------------------------------------------------------
        inline vec2<SCALAR> xy() const
        {
            return vec2<SCALAR>( this->m[0], this->m[1] );
        }

	};

	//!
	typedef vec4<float> vec4f;
    MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE( vec4f );


	//!
    template <class VECTOR>
    class box
	{
	public:

		VECTOR min;
		VECTOR size;

	public:

        //-----------------------------------------------------------------------------------------
        static box<VECTOR> FromMinSize(VECTOR m, VECTOR s)
        {
            box<VECTOR> result;
            result.min = m;
            result.size = s;
            return result;
        }

        //-----------------------------------------------------------------------------------------
        inline bool Contains( const VECTOR& v ) const
        {
            return v.AllGreaterOrEqualThan(min) && v.AllSmallerThan(min+size);
        }

        //-----------------------------------------------------------------------------------------
        inline bool ContainsInclusive( const VECTOR& v ) const
        {
            return v.AllGreaterOrEqualThan(min) && v.AllSmallerOrEqualThan(min+size);
        }

        //-----------------------------------------------------------------------------------------
        //! It is inclusive: If the borders touch it is considered an intersection.
        //-----------------------------------------------------------------------------------------
        inline bool Intersects(const box<VECTOR>& other) const
        {
            bool result = true;
            for (int d = 0; d < VECTOR::GetDim(); ++d)
            {
                result = result &&
                         FMath::Abs((min[d] * 2 + size[d]) - (other.min[d] * 2 + other.size[d])) <=
                             (size[d] + other.size[d]);
            }

            return result;
        }

        //-----------------------------------------------------------------------------------------
        //! It is exclusive: If the borders touch it is not considered an intersection.
        //-----------------------------------------------------------------------------------------
        inline bool IntersectsExclusive(const box<VECTOR>& other) const
        {
            bool result = true;
            for (int d = 0; d < VECTOR::GetDim(); ++d)
            {
                result = result &&
                         FMath::Abs((min[d] * 2 + size[d]) - (other.min[d] * 2 + other.size[d])) <
                             (size[d] + other.size[d]);
            }

            return result;
        }

        //-----------------------------------------------------------------------------------------
        //! If the boxes don't intersect, at least one size of the returned box will be negative.
        //-----------------------------------------------------------------------------------------
        inline box<VECTOR> Intersect(const box<VECTOR>& other) const
        {
            box<VECTOR> result;
            for (int d = 0; d < VECTOR::GetDim(); ++d)
            {
                result.min[d] = FMath::Max(min[d], other.min[d]);
                result.size[d] =
                    FMath::Min(typename VECTOR::SCALAR(min[d] + size[d] - result.min[d]),
                             typename VECTOR::SCALAR(other.min[d] + other.size[d] - result.min[d]));
            }
            return result;
        }

        //-----------------------------------------------------------------------------------------
		VECTOR Homogenize( const VECTOR& v ) const
		{
			return (v-min)/size;
		}

        //-----------------------------------------------------------------------------------------
        inline void Bound( const VECTOR& v )
        {
            for ( int d=0; d<VECTOR::GetDim(); ++d )
            {
                if (v[d]<min[d])
                {
                    size[d] += min[d]-v[d];
                    min[d] = v[d];
                }
                else if (v[d]-min[d]>size[d])
                {
                    size[d] = v[d]-min[d];
                }
            }
        }

		//-----------------------------------------------------------------------------------------
		inline void Bound(const box<VECTOR>& other)
		{
			Bound(other.min);
			Bound(other.min + other.size);
		}

		//-----------------------------------------------------------------------------------------
		inline void ShrinkToHalf()
		{
			min /= 2;
			size /= 2;
		}

		//-----------------------------------------------------------------------------------------
		inline bool IsEmpty() const
		{
			return size[0] <= 0 || size[1] <= 0;
		}

		//-----------------------------------------------------------------------------------------
		//! Strict ordering of boxes.
		//-----------------------------------------------------------------------------------------
        inline bool operator<( const box<VECTOR>& other ) const
        {
            if( size < other.size )
            {
                return true;
            }
            else if( size == other.size )
            {
                return min < other.min;
            }

            return false;
        }

		//-----------------------------------------------------------------------------------------
		inline bool operator==(const box<VECTOR>& other) const
		{
			return (size == other.size)
				&&
				(min == other.min);
		}

        //-----------------------------------------------------------------------------------------
		inline void Serialise( OutputArchive& arch ) const
		{
			arch << min;
			arch << size;
		}

		//-----------------------------------------------------------------------------------------
		inline void Unserialise( InputArchive& arch )
		{
			arch >> min;
			arch >> size;
		}

	};




	//---------------------------------------------------------------------------------------------
	//! Generic square matrix template.
	//---------------------------------------------------------------------------------------------
	template <class SCALAR, int DIM>
    class mat
	{
	protected:

		//! Rows
		vec<SCALAR,DIM> m[ DIM ];

	public:

		//-----------------------------------------------------------------------------------------
		typedef vec<SCALAR,DIM> V;
		typedef mat<SCALAR,DIM> M;

		//-----------------------------------------------------------------------------------------
		static inline int GetDim()
		{
			return DIM;
		}

		//-----------------------------------------------------------------------------------------
		//! 0 matrix
		//-----------------------------------------------------------------------------------------
		mat()
		{
		}

		//-----------------------------------------------------------------------------------------
		//! Build from a float array
		//-----------------------------------------------------------------------------------------
		explicit mat( const float* p )
		{
			for (int j=0; j<DIM; ++j)
			{
				for (int i=0; i<DIM; ++i)
				{
					m[j][i] = *p;
					++p;
				}
			}
		}

		//-----------------------------------------------------------------------------------------
		//! Initialise from another matrix with different size, filling the unknown with identites.
		//-----------------------------------------------------------------------------------------
		template<int DIM2>
		explicit mat( const mat<SCALAR,DIM2>& other )
		{
			for (int i=0; i<FMath::Min(DIM,DIM2); ++i)
			{
				for (int j=0; j<FMath::Min(DIM,DIM2) ;++j)
				{
					m[i][j] = other[i][j];
				}
			}

			for (int i=DIM2; i<DIM; ++i)
			{
				m[i][i] = 1;
			}
		}

		//-----------------------------------------------------------------------------------------
		//! Make a scale matrix.
		//-----------------------------------------------------------------------------------------
		template<int DIM2>
		inline static M Scale( const vec<SCALAR,DIM2>& s )
		{
			M r;
			for (int i=0; i<FMath::Min(DIM,DIM2); ++i)
			{
				r.m[i][i] = s[i];
			}

			for (int i=DIM2; i<DIM; ++i)
			{
				r.m[i][i] = 1;
			}

			return r;
		}

		//-----------------------------------------------------------------------------------------
		//! Make a translation matrix.
		//-----------------------------------------------------------------------------------------
		template<int DIM2>
		static inline M Translate( const vec<SCALAR,DIM2>& pos )
		{
			M r = Identity();

			for (int i=0; i<FMath::Min(DIM,DIM2); ++i)
			{
				r.m[i][DIM-1] = pos[i];
			}

			return r;
		}

        //-----------------------------------------------------------------------------------------
        //! Return an identity matrix.
        //-----------------------------------------------------------------------------------------
        static inline M Identity()
        {
            M res;
            for (int i=0; i<DIM;++i)
            {
                res[i][i] = 1;
            }
            return res;
        }

        //-----------------------------------------------------------------------------------------
        //! Return an identity matrix.
        //-----------------------------------------------------------------------------------------
        inline void SetIdentity()
        {
            *this = M::Identity();
        }

		//-----------------------------------------------------------------------------------------
		//! Return a row
		//-----------------------------------------------------------------------------------------
        inline V& operator[] (int i)
		{
			check( i>=0 && i <DIM );
			return m[i];
		}

		//-----------------------------------------------------------------------------------------
		//! Return a row
		//-----------------------------------------------------------------------------------------
		inline const V& GetRow(int i) const
		{
			check( i>=0 && i <DIM );
			return m[i];
		}

		//-----------------------------------------------------------------------------------------
		//! Return a column
		//-----------------------------------------------------------------------------------------
		inline V GetColumn(int i) const
		{
			check( i>=0 && i <DIM );
			V r;
			for ( int j=0; j<DIM; ++j )
			{
				r[j] = m[j][i];
			}
			return r;
		}

		//-----------------------------------------------------------------------------------------
        inline const V& operator[] (int i) const
		{
			check( i>=0 && i <DIM );
			return m[i];
		}

		//-----------------------------------------------------------------------------------------
		inline bool operator==( const M& other ) const
		{
			bool res = true;
			for( int i=0; i<DIM; ++i )
			{
				res &= m[i] == other.m[i];
			}
			return res;
		}

		//-----------------------------------------------------------------------------------------
		inline bool operator!=( const M& other ) const
		{
			bool res = false;
			for( int i=0; i<DIM; ++i )
			{
				res |= m[i] != other.m[i];
			}
			return res;
		}

		//-----------------------------------------------------------------------------------------
		inline V operator+( const M& other ) const
		{
			V res;
			for( int i=0; i<DIM; ++i )
			{
				res.m[i] = m[i] + other.m[i];
			}
			return res;
		}

		//-----------------------------------------------------------------------------------------
		inline void operator+=( const M& other )
		{
			for( int i=0; i<DIM; ++i )
			{
				m[i] += other.m[i];
			}
		}

		//-----------------------------------------------------------------------------------------
		inline V operator-( const M& other ) const
		{
			V res;
			for( int i=0; i<DIM; ++i )
			{
				res.m[i] = m[i] - other.m[i];
			}
			return res;
		}

		//-----------------------------------------------------------------------------------------
		inline void operator-=( const M& other )
		{
			for( int i=0; i<DIM; ++i )
			{
				m[i] -= other.m[i];
			}
		}

        //-----------------------------------------------------------------------------------------
        M operator*( const mat<SCALAR,DIM>& m2 ) const
        {
            M r;

            for ( int i=0; i<DIM; ++i )
            {
                for ( int j=0; j<DIM; ++j )
                {
                    r.m[i][j] = dot( m[i], m2.GetColumn(j) );
                }
            }

            return r;
        }

        //-----------------------------------------------------------------------------------------
        V operator*( const V& v ) const
        {
            V r;

            for ( int i=0; i<DIM; ++i )
            {
                r[i] = dot( m[i], v );
            }

            return r;
        }

        //-----------------------------------------------------------------------------------------
        SCALAR GetDeterminant() const
        {
            SCALAR result=0;

            for ( int i=0; i<DIM; ++i )
            {
                SCALAR accum=1;
                for ( int j=0; j<DIM; ++j )
                {
                    accum *= m[(i+j)%DIM][j];
                }
                result+=accum;

                accum=1;
                for ( int j=0; j<DIM; ++j )
                {
                    accum *= m[(DIM+i-j)%DIM][j];
                }
                result-=accum;
            }

            return result;
        }


		//-----------------------------------------------------------------------------------------
		//! Strict ordering of matrices.
		//-----------------------------------------------------------------------------------------
		inline bool operator<( const M& other ) const
		{
			bool equal = true;
			bool res = false;
			for( int i=0; equal && i<DIM; ++i )
			{
				if( m[i] < other.m[i] )
				{
					equal = false;
					res = true;
				}
				else if( m[i] > other.m[i] )
				{
					equal = false;
					res = false;
				}
			}
			return res;
		}


		//-----------------------------------------------------------------------------------------
		inline bool AlmostEqual
			(
				const V& other,
				SCALAR tol = std::numeric_limits<SCALAR>::min()
			) const
		{
			bool res = true;
			for( int i=0; i<DIM; ++i )
			{
				res &= m[i].AlmostEqual(other.m[i],tol);
			}
			return res;
		}

		//-----------------------------------------------------------------------------------------
		inline bool AlmostNull( SCALAR tol = std::numeric_limits<SCALAR>::min() ) const
		{
			bool res = true;
			for( int i=0; i<DIM; ++i )
			{
				res &= m[i].AlmostNull( tol );
			}
			return res;
		}

		//-----------------------------------------------------------------------------------------
		inline void Serialise( OutputArchive& arch ) const
		{
			for( int i=0; i<DIM; ++i )
			{
				arch << m[i];
			}
		}

		//-----------------------------------------------------------------------------------------
		inline void Unserialise( InputArchive& arch )
		{
			for( int i=0; i<DIM; ++i )
			{
				arch >> m[i];
			}
		}

	};


	//!
	template <class SCALAR>
	class mat3 : public mat<SCALAR,3>
	{
	public:

		mat3()
		{
		}

        mat3( SCALAR a0, SCALAR a1, SCALAR a2,
              SCALAR a3, SCALAR a4, SCALAR a5,
              SCALAR a6, SCALAR a7, SCALAR a8 )
        {
            this->m[0][0] = a0;
            this->m[0][1] = a1;
            this->m[0][2] = a2;
            this->m[1][0] = a3;
            this->m[1][1] = a4;
            this->m[1][2] = a5;
            this->m[2][0] = a6;
            this->m[2][1] = a7;
            this->m[2][2] = a8;
        }

        mat3( vec3<SCALAR> r0, vec3<SCALAR> r1, vec3<SCALAR> r2 )
        {
            this->m[0] = r0;
            this->m[1] = r1;
            this->m[2] = r2;
        }


	};


	//!
	typedef mat3<float> mat3f;
    typedef mat<float,4> mat4f;
    MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE( mat3f );
    MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE( mat4f );

	//---------------------------------------------------------------------------------------------
	// Temp while transitioning to UE math.
	//---------------------------------------------------------------------------------------------
	inline FMatrix44f ToUnreal(const mat4f& m)
	{
		FMatrix44f ueMat;
		for (int i = 0; i < 4; ++i)
			for (int j = 0; j < 4; ++j)
				ueMat.M[i][j] = m[i][j];
		return ueMat;
	}


	inline FVector3f ToUnreal(const vec3f& v)
	{
		return FVector3f(v.x(), v.y(), v.z());
	}


	inline vec3f FromUnreal(const FVector3f& v)
	{
		return vec3f(v.X, v.Y, v.Z);
	}


}

