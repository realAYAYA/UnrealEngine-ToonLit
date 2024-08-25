//
// This is free and unencumbered software released into the public domain.
//
// Anyone is free to copy, modify, publish, use, compile, sell, or
// distribute this software, either in source code form or as a compiled
// binary, for any purpose, commercial or non-commercial, and by any
// means.
//
// In jurisdictions that recognize copyright laws, the author or authors
// of this software dedicate any and all copyright interest in the
// software to the public domain. We make this dedication for the benefit
// of the public at large and to the detriment of our heirs and
// successors. We intend this dedication to be an overt act of
// relinquishment in perpetuity of all present and future rights to this
// software under copyright law.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.
//
//
// For more information, please refer to <https://unlicense.org>
//

#include <math.h>

#define AHEASING_PI 3.1415926535897932f
#define AHEASING_PI_2 1.57079632679f

#define AH_FLOAT_TYPE float
typedef AH_FLOAT_TYPE AHFloat;

typedef AHFloat (*AHEasingFunction)(AHFloat);

// Modeled after the line y = x
inline AHFloat LinearInterpolation(AHFloat p)
{
	return p;
}
 
// Modeled after the parabola y = x^2
inline AHFloat QuadraticEaseIn(AHFloat p)
{
	return p * p;
}
 
// Modeled after the parabola y = -x^2 + 2x
inline AHFloat QuadraticEaseOut(AHFloat p)
{
	return -(p * (p - 2.f));
}
 
// Modeled after the piecewise quadratic
// y = (1/2)((2x)^2)             ; [0, 0.5)
// y = -(1/2)((2x-1)*(2x-3) - 1) ; [0.5, 1]
inline AHFloat QuadraticEaseInOut(AHFloat p)
{
	if(p < 0.5f)
	{
		return 2.f * p * p;
	}
	else
	{
		return (-2.f * p * p) + (4.f * p) - 1.f;
	}
}
 
// Modeled after the cubic y = x^3
inline AHFloat CubicEaseIn(AHFloat p)
{
	return p * p * p;
}
 
// Modeled after the cubic y = (x - 1)^3 + 1
inline AHFloat CubicEaseOut(AHFloat p)
{
	AHFloat f = (p - 1.f);
	return f * f * f + 1.f;
}
 
// Modeled after the piecewise cubic
// y = (1/2)((2x)^3)       ; [0, 0.5)
// y = (1/2)((2x-2)^3 + 2) ; [0.5, 1]
inline AHFloat CubicEaseInOut(AHFloat p)
{
	if(p < 0.5f)
	{
		return 4.f * p * p * p;
	}
	else
	{
		AHFloat f = ((2.f * p) - 2.f);
		return 0.5f * f * f * f + 1.f;
	}
}
 
// Modeled after the quartic x^4
inline AHFloat QuarticEaseIn(AHFloat p)
{
	return p * p * p * p;
}
 
// Modeled after the quartic y = 1 - (x - 1)^4
inline AHFloat QuarticEaseOut(AHFloat p)
{
	AHFloat f = (p - 1.f);
	return f * f * f * (1.f - p) + 1.f;
}
 
// Modeled after the piecewise quartic
// y = (1/2)((2x)^4)        ; [0, 0.5)
// y = -(1/2)((2x-2)^4 - 2) ; [0.5, 1]
inline AHFloat QuarticEaseInOut(AHFloat p) 
{
	if(p < 0.5f)
	{
		return 8.f * p * p * p * p;
	}
	else
	{
		AHFloat f = (p - 1.f);
		return -8.f * f * f * f * f + 1.f;
	}
}
 
// Modeled after the quintic y = x^5
inline AHFloat QuinticEaseIn(AHFloat p) 
{
	return p * p * p * p * p;
}
 
// Modeled after the quintic y = (x - 1)^5 + 1
inline AHFloat QuinticEaseOut(AHFloat p) 
{
	AHFloat f = (p - 1.f);
	return f * f * f * f * f + 1.f;
}
 
// Modeled after the piecewise quintic
// y = (1/2)((2x)^5)       ; [0, 0.5)
// y = (1/2)((2x-2)^5 + 2) ; [0.5, 1]
inline AHFloat QuinticEaseInOut(AHFloat p) 
{
	if(p < 0.5f)
	{
		return 16.f * p * p * p * p * p;
	}
	else
	{
		AHFloat f = ((2.f * p) - 2.f);
		return  0.5f * f * f * f * f * f + 1.f;
	}
}
 
// Modeled after quarter-cycle of sine wave
inline AHFloat SineEaseIn(AHFloat p)
{
	return sinf((p - 1.f) * AHEASING_PI_2) + 1.f;
}
 
// Modeled after quarter-cycle of sine wave (different phase)
inline AHFloat SineEaseOut(AHFloat p)
{
	return sinf(p * AHEASING_PI_2);
}
 
// Modeled after half sine wave
inline AHFloat SineEaseInOut(AHFloat p)
{
	return 0.5f * (1.f - cosf(p * AHEASING_PI));
}
 
// Modeled after shifted quadrant IV of unit circle
inline AHFloat CircularEaseIn(AHFloat p)
{
	return 1.f - sqrtf(1.f - (p * p));
}
 
// Modeled after shifted quadrant II of unit circle
inline AHFloat CircularEaseOut(AHFloat p)
{
	return sqrtf((2.f - p) * p);
}
 
// Modeled after the piecewise circular function
// y = (1/2)(1 - sqrtf(1 - 4x^2))           ; [0, 0.5)
// y = (1/2)(sqrtf(-(2x - 3)*(2x - 1)) + 1) ; [0.5, 1]
inline AHFloat CircularEaseInOut(AHFloat p)
{
	if(p < 0.5f)
	{
		return 0.5f * (1.f - sqrtf(1.f - 4.f * (p * p)));
	}
	else
	{
		return 0.5f * (sqrtf(-((2.f * p) - 3.f) * ((2.f * p) - 1.f)) + 1.f);
	}
}
 
// Modeled after the exponential function y = 2^(10(x - 1))
inline AHFloat ExponentialEaseIn(AHFloat p)
{
	return (p == 0.f) ? p : powf(2.f, 10.f * (p - 1.f));
}
 
// Modeled after the exponential function y = -2^(-10x) + 1
inline AHFloat ExponentialEaseOut(AHFloat p)
{
	return (p == 1.f) ? p : 1.f - powf(2.f, -10.f * p);
}
 
// Modeled after the piecewise exponential
// y = (1/2)2^(10(2x - 1))         ; [0,0.5)
// y = -(1/2)*2^(-10(2x - 1))) + 1 ; [0.5,1]
inline AHFloat ExponentialEaseInOut(AHFloat p)
{
	if(p == 0.f || p == 1.f) return p;
	
	if(p < 0.5f)
	{
		return 0.5f * powf(2.f, (20.f * p) - 10.f);
	}
	else
	{
		return -0.5f * powf(2.f, (-20.f * p) + 10.f) + 1.f;
	}
}
 
// Modeled after the damped sine wave y = sinf(13pi/2*x)*powf(2, 10 * (x - 1))
inline AHFloat ElasticEaseIn(AHFloat p)
{
	return sinf(13.f * AHEASING_PI_2 * p) * powf(2.f, 10.f * (p - 1.f));
}
 
// Modeled after the damped sine wave y = sinf(-13pi/2*(x + 1))*powf(2, -10x) + 1
inline AHFloat ElasticEaseOut(AHFloat p)
{
	return sinf(-13.f * AHEASING_PI_2 * (p + 1.f)) * powf(2.f, -10.f * p) + 1.f;
}
 
// Modeled after the piecewise exponentially-damped sine wave:
// y = (1/2)*sinf(13pi/2*(2*x))*powf(2, 10 * ((2*x) - 1))      ; [0,0.5)
// y = (1/2)*(sinf(-13pi/2*((2x-1)+1))*powf(2,-10(2*x-1)) + 2) ; [0.5, 1]
inline AHFloat ElasticEaseInOut(AHFloat p)
{
	if(p < 0.5f)
	{
		return 0.5f * sinf(13.f * AHEASING_PI_2 * (2.f * p)) * powf(2.f, 10.f * ((2.f * p) - 1.f));
	}
	else
	{
		return 0.5f * (sinf(-13.f * AHEASING_PI_2 * ((2.f * p - 1.f) + 1.f)) * powf(2, -10 * (2.f * p - 1.f)) + 2.f);
	}
}
 
// Modeled after the overshooting cubic y = x^3-x*sinf(x*pi)
inline AHFloat BackEaseIn(AHFloat p)
{
	return p * p * p - p * sinf(p * AHEASING_PI);
}
 
// Modeled after overshooting cubic y = 1-((1-x)^3-(1-x)*sinf((1-x)*pi))
inline AHFloat BackEaseOut(AHFloat p)
{
	AHFloat f = (1.f - p);
	return 1.f - (f * f * f - f * sinf(f * AHEASING_PI));
}
 
// Modeled after the piecewise overshooting cubic function:
// y = (1/2)*((2x)^3-(2x)*sinf(2*x*pi))           ; [0, 0.5)
// y = (1/2)*(1-((1-x)^3-(1-x)*sinf((1-x)*pi))+1) ; [0.5, 1]
inline AHFloat BackEaseInOut(AHFloat p)
{
	if(p < 0.5f)
	{
		AHFloat f = 2.f * p;
		return 0.5f * (f * f * f - f * sinf(f * AHEASING_PI));
	}
	else
	{
		AHFloat f = (1.f - (2.f * p - 1.f));
		return 0.5f * (1.f - (f * f * f - f * sinf(f * AHEASING_PI))) + 0.5f;
	}
}
 
inline AHFloat BounceEaseOut(AHFloat p)
{
	if(p < 4.f / 11.f)
	{
		return (12.f * p * p) / 16.f;
	}
	else if(p < 8.f / 11.f)
	{
		return (363.f / 40.f * p * p) - (99.f / 10.f * p) + 17.f /5.f;
	}
	else if(p < 9.f / 10.f)
	{
		return (4356.f / 361.f * p * p) - (35442.f / 1805.f * p) + 16061.f / 1805.f;
	}
	else
	{
		return (54.f / 5.f * p * p) - (513.f / 25.f * p) + 268.f / 25.f;
	}
}

inline AHFloat BounceEaseIn(AHFloat p)
{
	return 1.f - BounceEaseOut(1.f - p);
}
  
inline AHFloat BounceEaseInOut(AHFloat p)
{
	if(p < 0.5f)
	{
		return 0.5f * BounceEaseIn(p * 2.f);
	}
	else
	{
		return 0.5f * BounceEaseOut(p * 2.f - 1.f) + 0.5f;
	}
}