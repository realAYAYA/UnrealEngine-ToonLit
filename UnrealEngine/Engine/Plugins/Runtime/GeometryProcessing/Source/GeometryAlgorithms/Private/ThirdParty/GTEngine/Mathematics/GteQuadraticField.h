// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.21.0 (2019/01/21)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteMath.h>

// A real quadratic field has elements of the form z = x + y * sqrt(d), where
// x, y and d are rational numbers and where d > 0.  In abstract algebra,
// it is require that d not be the square of a rational number.  When this
// is the case, the representation of z is unique.  For GTEngine, the
// square-free constraint is not essential, because we need only the
// arithmetic properties of the field (addition, subtraction, multiplication
// and division).  We also need comparisons between elements of the quadratic
// field, but these can be reformulated to use only operations for rational
// arithmetic.

namespace gte
{
    // The type T can be rational (for arbitrary precision arithmetic) or
    // floating-point (the quadratic field operations are valid, although
    // with floating-point rounding errors).
    template <typename T>
    class QuadraticField
    {
    public:
        // The quadratic field descriptor that simply stores d.
        QuadraticField()
            :
            d(0)
        {
        }

        QuadraticField(T const& inD)
            :
            d(inD)
        {
        }

        T d;

        // Elements of the quadratic field.  The arithmetic and comparisons
        // are managed by the quadratic field itself to avoid having multiple
        // copies of d (one copy per Element).
        class Element
        {
        public:
            Element()
                :
                x(0),
                y(0)
            {
            }

            Element(T const& inX, T const& inY)
                :
                x(inX),
                y(inY)
            {
            }

            T x, y;
        };

        T Convert(Element const& e) const
        {
            return e.x + e.y * std::sqrt(d);
        }

        Element Negate(Element const& e) const
        {
            return Element(-e.x, -e.y);
        }

        Element Add(Element const& e0, Element const& e1) const
        {
            T x = e0.x + e1.x;
            T y = e0.y + e1.y;
            return Element(x, y);
        }

        Element Add(Element const& e, T const& s) const
        {
            T x = e.x + s;
            T y = e.y;
            return Element(x, y);
        }

        Element Add(T const& s, Element const& e) const
        {
            T x = s + e.x;
            T y = e.y;
            return Element(x, y);
        }

        Element Sub(Element const& e0, Element const& e1) const
        {
            T x = e0.x - e1.x;
            T y = e0.y - e1.y;
            return Element(x, y);
        }

        Element Sub(Element const& e, T const& s) const
        {
            T x = e.x - s;
            T y = e.y;
            return Element(x, y);
        }

        Element Sub(T const& s, Element const& e) const
        {
            T x = s - e.x;
            T y = -e.y;
            return Element(x, y);
        }

        Element Mul(Element const& e0, Element const& e1) const
        {
            T x = e0.x * e1.x + e0.y * e1.y * d;
            T y = e0.x * e1.y + e0.y * e1.x;
            return Element(x, y);
        }

        Element Mul(Element const& e, T const& s)
        {
            return Element(e.x * s, e.y * s);
        }

        Element Mul(T const& s, Element const& e)
        {
            return Element(s * e.x, s * e.y);
        }

        // Expose division only when T has a division operator.
        template <typename Dummy = T>
        typename std::enable_if<has_division_operator<Dummy>::value, Element>::type
        Div(Element const& e0, Element const& e1) const
        {
            T z = e1.x * e1.x - e1.y * e1.y * d;
            T x = (e0.x * e1.x - e0.y * e1.y * d) / z;
            T y = (e0.y * e1.x - e0.x * e1.y) / z;
            return Element(x, y);
        }

        template <typename Dummy = T>
        typename std::enable_if<has_division_operator<Dummy>::value, Element>::type
        Div(Element const& e, T const& s)
        {
            return Element(e.x / s, e.y / s);
        }

        // Comparison functions.
        bool EqualZero(Element const& e) const
        {
            T const zero(0);
            if (d == zero || e.y == zero)
            {
                return e.x == zero;
            }
            else if (e.y > zero)
            {
                if (e.x >= zero)
                {
                    return false;
                }
                else
                {
                    return e.x * e.x - e.y * e.y * d == zero;
                }
            }
            else // e.y < zero
            {
                if (e.x <= zero)
                {
                    return false;
                }
                else
                {
                    return e.x * e.x - e.y * e.y * d == zero;
                }
            }
        }

        bool NotEqualZero(Element const& e) const
        {
            T const zero(0);
            if (d == zero || e.y == zero)
            {
                return e.x != zero;
            }
            else if (e.y > zero)
            {
                if (e.x >= zero)
                {
                    return true;
                }
                else
                {
                    return e.x * e.x - e.y * e.y * d != zero;
                }
            }
            else // e.y < zero
            {
                if (e.x <= zero)
                {
                    return true;
                }
                else
                {
                    return e.x * e.x - e.y * e.y * d != zero;
                }
            }
        }

        bool LessThanZero(Element const& e) const
        {
            T const zero(0);
            if (d == zero || e.y == zero)
            {
                return e.x < zero;
            }
            else if (e.y > zero)
            {
                if (e.x >= zero)
                {
                    return false;
                }
                else
                {
                    return e.x * e.x - e.y * e.y * d > zero;
                }
            }
            else // e.y < zero
            {
                if (e.x <= zero)
                {
                    return true;
                }
                else
                {
                    return e.x * e.x - e.y * e.y * d < zero;
                }
            }
        }

        bool GreaterThanZero(Element const& e) const
        {
            T const zero(0);
            if (d == zero || e.y == zero)
            {
                return e.x > zero;
            }
            else if (e.y > zero)
            {
                if (e.x >= zero)
                {
                    return true;
                }
                else
                {
                    return e.x * e.x - e.y * e.y * d < zero;
                }
            }
            else // e.y < zero
            {
                if (e.x <= zero)
                {
                    return false;
                }
                else
                {
                    return e.x * e.x - e.y * e.y * d > zero;
                }
            }
        }

        bool LessThanOrEqualZero(Element const& e) const
        {
            T const zero(0);
            if (d == zero || e.y == zero)
            {
                return e.x <= zero;
            }
            else if (e.y > zero)
            {
                if (e.x >= zero)
                {
                    return false;
                }
                else
                {
                    return e.x * e.x - e.y * e.y * d >= zero;
                }
            }
            else // e.y < zero
            {
                if (e.x <= zero)
                {
                    return true;
                }
                else
                {
                    return e.x * e.x - e.y * e.y * d <= zero;
                }
            }
        }

        bool GreaterThanOrEqualZero(Element const& e) const
        {
            T const zero(0);
            if (d == zero || e.y == zero)
            {
                return e.x >= zero;
            }
            else if (e.y > zero)
            {
                if (e.x >= zero)
                {
                    return true;
                }
                else
                {
                    return e.x * e.x - e.y * e.y * d <= zero;
                }
            }
            else // e.y < zero
            {
                if (e.x <= zero)
                {
                    return false;
                }
                else
                {
                    return e.x * e.x - e.y * e.y * d >= zero;
                }
            }
        }

        bool Equal(Element const& e0, Element const& e1) const
        {
            return EqualZero(Sub(e0, e1));
        }

        bool LessThan(Element const& e0, Element const& e1) const
        {
            return LessThanZero(Sub(e0, e1));
        }

        bool GreaterThan(Element const& e0, Element const& e1) const
        {
            return GreaterThanZero(Sub(e0, e1));
        }

        bool LessThanOrEqual(Element const& e0, Element const& e1) const
        {
            return LessThanOrEqualZero(Sub(e0, e1));
        }

        bool GreaterThanOrEqual(Element const& e0, Element const& e1) const
        {
            return GreaterThanOrEqualZero(Sub(e0, e1));
        }
    };
}
