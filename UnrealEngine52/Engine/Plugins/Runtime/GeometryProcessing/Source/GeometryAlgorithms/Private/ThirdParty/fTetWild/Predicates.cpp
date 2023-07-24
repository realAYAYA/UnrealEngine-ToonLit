#include "ThirdParty/fTetWild/Predicates.hpp"

#include "CompGeom/ExactPredicates.h"

namespace floatTetWild {
    const int Predicates::ORI_POSITIVE;
    const int Predicates::ORI_ZERO;
    const int Predicates::ORI_NEGATIVE;
    const int Predicates::ORI_UNKNOWN;

    int Predicates::orient_3d(const Vector3& p1, const Vector3& p2, const Vector3& p3, const Vector3& p4) {
		double result = UE::Geometry::ExactPredicates::Orient3D(p1.data(), p2.data(), p3.data(), p4.data());

        if (result > 0)
            return ORI_POSITIVE;
        else if (result < 0)
            return ORI_NEGATIVE;
        else
            return ORI_ZERO;
    }

    Scalar Predicates::orient_3d_volume(const Vector3& p1, const Vector3& p2, const Vector3& p3, const Vector3& p4)
	{
		Scalar ori = (Scalar)FMath::Sign(UE::Geometry::ExactPredicates::Orient3D(p1.data(), p2.data(), p3.data(), p4.data()));

        if (ori <= 0)
            return ori;
        else // Note: Clamped to a positive value to respect result of exact predicate
            return FMath::Max((Scalar)FLT_MIN, (p1 - p4).dot((p2 - p4).cross(p3 - p4)) / 6);
    }

    int Predicates::orient_2d(const Vector2& p1, const Vector2& p2, const Vector2& p3) {
		return (int)FMath::Sign(UE::Geometry::ExactPredicates::Orient2D(p1.data(), p2.data(), p3.data()));
    }

} // namespace floatTetWild
