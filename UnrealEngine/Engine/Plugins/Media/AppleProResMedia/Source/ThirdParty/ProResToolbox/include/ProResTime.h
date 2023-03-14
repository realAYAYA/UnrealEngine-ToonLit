//
//  ProResTime.h
//  Copyright © 2017 Apple. All rights reserved.
//

#ifndef PRORESTIME_H
#define PRORESTIME_H	1

/*!
	@header
	@abstract	API for creating and manipulating PRTime structs.
	@discussion	PRTime structs are non-opaque mutable structs representing times (either timestamps or durations).
	
				A PRTime is represented as a rational number, with a numerator (int64_t value), and a denominator (int32_t timescale).
				A flags field allows various non-numeric values to be stored (+infinity, -infinity, indefinite, invalid).  There is also
				a flag to mark whether or not the time is completely precise, or had to be rounded at some point in its past.
				
				PRTimes contain an epoch number, which is usually set to 0, but can be used to distinguish unrelated
				timelines: for example, it could be incremented each time through a presentation loop,
				to differentiate between time N in loop 0 from time N in loop 1.
*/

#include "ProResTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(push, 4)

/*!
	@typedef	PRTimeValue
	@abstract	Numerator of rational PRTime.
 */
typedef int64_t PRTimeValue;

/*!
	@typedef	PRTimeScale
	@abstract	Denominator of rational PRTime.
	@discussion	Timescales must be positive.
				Note: kCMTimeMaxTimescale is NOT a good choice of timescale for movie files.  
				(Recommended timescales for movie files range from 600 to 90000.)
*/
typedef int32_t PRTimeScale;
#define kPRTimeMaxTimescale 0x7fffffffL

/*!
	@typedef	PRTimeEpoch
	@abstract	Epoch (eg, loop number) to which a PRTime refers.
*/
typedef int64_t PRTimeEpoch;

/*!
 	@enum		PRTimeFlags
 	@abstract	Flag bits for a PRTime.
 	@constant	kPRTimeFlags_Valid Must be set, or the PRTime is considered invalid.
 				Allows simple clearing (eg. with calloc or memset) for initialization
 				of arrays of PRTime structs to "invalid". This flag must be set, even
 				if other flags are set as well.
	@constant	kPRTimeFlags_HasBeenRounded Set whenever a PRTime value is rounded, or is derived from another rounded PRTime.
 	@constant	kPRTimeFlags_PositiveInfinity Set if the PRTime is +inf.	"Implied value" flag (other struct fields are ignored).
 	@constant	kPRTimeFlags_NegativeInfinity Set if the PRTime is -inf.	"Implied value" flag (other struct fields are ignored).
 	@constant	kPRTimeFlags_Indefinite Set if the PRTime is indefinite/unknown. Example of usage: duration of a live broadcast.
 										"Implied value" flag (other struct fields are ignored).
 */
enum {
	kPRTimeFlags_Valid = 1UL<<0,
	kPRTimeFlags_HasBeenRounded = 1UL<<1,
	kPRTimeFlags_PositiveInfinity = 1UL<<2,
	kPRTimeFlags_NegativeInfinity = 1UL<<3,
	kPRTimeFlags_Indefinite = 1UL<<4,
	kPRTimeFlags_ImpliedValueFlagsMask = kPRTimeFlags_PositiveInfinity | kPRTimeFlags_NegativeInfinity | kPRTimeFlags_Indefinite
};
typedef uint32_t PRTimeFlags;

/*!
	@typedef	PRTime
	@abstract	Rational time value represented as int64/int32.
 */
typedef struct {
	PRTimeValue value;		/*! @field value The value of the PRTime. value/timescale = seconds. */
	PRTimeScale timescale;	/*! @field timescale The timescale of the PRTime. value/timescale = seconds. */
	PRTimeFlags flags;      /*! @field flags The flags, eg. kPRTimeFlags_Valid, kPRTimeFlags_PositiveInfinity, etc. */
	PRTimeEpoch	epoch;		/*! @field epoch Differentiates between equal timestamps that are actually different because
							 					of looping, multi-item sequencing, etc.
							 					Will be used during comparison: greater epochs happen after lesser ones.
							 					Additions/subtraction is only possible within a single epoch,
							 					however, since epoch length may be unknown/variable. */
} PRTime;
	
/*!
	@function	PRTIME_IS_VALID
	@abstract   Returns whether a PRTime is valid.
	@discussion This is a macro that evaluates a Boolean result.
	@result     Returns true if the PRTime is valid, false if it is invalid.
 */
#define PRTIME_IS_VALID(time) ((bool)(((time).flags & kPRTimeFlags_Valid) != 0))

/*!
	@function	PRTIME_IS_INVALID
	@abstract   Returns whether a PRTime is invalid.
	@discussion This is a macro that evaluates a Boolean result.
	@result     Returns true if the PRTime is invalid, false if it is valid.
 */
#define PRTIME_IS_INVALID(time) (!PRTIME_IS_VALID(time))
	
/*!
	@function	PRTIME_IS_POSITIVEINFINITY
	@abstract   Returns whether a PRTime is positive infinity.  Use this instead of (myTime == kPRTimePositiveInfinity),
	since there are many PRTime structs that represent positive infinity.  This is because the non-flags fields are ignored,
	so they can contain anything.
	@discussion This is a macro that evaluates a Boolean result.
	@result     Returns true if the PRTime is positive infinity, false if it is not.
 */
#define PRTIME_IS_POSITIVE_INFINITY(time) ((bool)(PRTIME_IS_VALID(time) && (((time).flags & kPRTimeFlags_PositiveInfinity) != 0)))
	
/*!
	@function	PRTIME_IS_NEGATIVEINFINITY
	@abstract   Returns whether a PRTime is negative infinity.
	@discussion This is a macro that evaluates a Boolean result.
	@result     Returns true if the PRTime is negative infinity, false if it is not.
 */
#define PRTIME_IS_NEGATIVE_INFINITY(time) ((bool)(PRTIME_IS_VALID(time) && (((time).flags & kPRTimeFlags_NegativeInfinity) != 0)))

/*!
	@function	PRTIME_IS_INDEFINITE
    @abstract   Returns whether a PRTime is indefinite.
    @discussion This is a macro that evaluates to a Boolean result.
    @result     Returns true if the PRTime is indefinite, false if it is not.
*/
#define PRTIME_IS_INDEFINITE(time) ((Boolean)(PRTIME_IS_VALID(time) && (((time).flags & kPRTimeFlags_Indefinite) != 0)))
	
/*!
	@function	PRTIME_IS_NUMERIC
	@abstract   Returns whether a PRTime is numeric (ie. contains a usable value/timescale/epoch).
	@discussion This is a macro that evaluates to a Boolean result.
	@result     Returns false if the PRTime is invalid, indefinite or +/- infinity.
				Returns true otherwise.
 */
#define PRTIME_IS_NUMERIC(time) ((bool)(((time).flags & (kPRTimeFlags_Valid | kPRTimeFlags_ImpliedValueFlagsMask)) == kPRTimeFlags_Valid))
	
/*!
	@function	PRTIME_HAS_BEEN_ROUNDED
	@abstract   Returns whether a PRTime has been rounded.
	@discussion This is a macro that evaluates a Boolean result.
	@result     Returns true if the PRTime has been rounded, false if it is completely accurate.
 */
#define PRTIME_HAS_BEEN_ROUNDED(time) ((bool)(PRTIME_IS_NUMERIC(time) && (((time).flags & kPRTimeFlags_HasBeenRounded) != 0)))

PR_EXPORT const PRTime kPRTimeInvalid;				/*! @constant kPRTimeInvalid
														Use this constant to initialize an invalid PRTime.
														All fields are 0, so you can calloc or fill with 0's to make lots of them.
														Do not test against this using (time == kPRTimeInvalid), there are many
														PRTimes other than this that are also invalid.
														Use !PRTIME_IS_VALID(time) instead. */
PR_EXPORT const PRTime kPRTimeIndefinite;			/*! @constant kPRTimeIndefinite
														Use this constant to initialize an indefinite PRTime (eg. duration of a live
														broadcast).  Do not test against this using (time == kPRTimeIndefinite),
														there are many PRTimes other than this that are also indefinite.
														Use PRTIME_IS_INDEFINITE(time) instead. */
PR_EXPORT const PRTime kPRTimePositiveInfinity;		/*! @constant kPRTimePositiveInfinity
														Use this constant to initialize a PRTime to +infinity.
														Do not test against this using (time == kPRTimePositiveInfinity),
														there are many PRTimes other than this that are also +infinity.
														Use PRTIME_IS_POSITIVEINFINITY(time) instead. */
PR_EXPORT const PRTime kPRTimeNegativeInfinity;		/*! @constant kPRTimeNegativeInfinity
														Use this constant to initialize a PRTime to -infinity.
														Do not test against this using (time == kPRTimeNegativeInfinity),
														there are many PRTimes other than this that are also -infinity.
														Use PRTIME_IS_NEGATIVEINFINITY(time) instead. */
PR_EXPORT const PRTime kPRTimeZero;					/*! @constant kPRTimeZero
														Use this constant to initialize a PRTime to 0.
														Do not test against this using (time == kPRTimeZero),
														there are many PRTimes other than this that are also 0.
														Use PRTimeCompare(time, kPRTimeZero) instead. */

/*!
	@function	PRTimeMake
	@abstract	Make a valid PRTime with value and timescale.
	@result		The resulting PRTime.
 */
PR_EXPORT 
PRTime PRTimeMake(
				int64_t value,		/*! @param value		Initializes the value field of the resulting CMTime. */
				int32_t timescale);	/*! @param timescale	Initializes the timescale field of the resulting CMTime. */
	
/*!
	@enum		PRTimeRoundingMethod
	@abstract   Rounding method to use when computing time.value during timescale conversions.
	@constant	kPRTimeRoundingMethod_Round Round towards zero if abs(fraction) is less than 0.5, away from 0 if abs(fraction) is >= 0.5.
	@constant	kPRTimeRoundingMethod_Default Synonym for kPRTimeRoundingMethod_Round.
	@constant	kPRTimeRoundingMethod_Truncate Round towards zero if fraction is != 0.
	@constant	kPRTimeRoundingMethod_RoundUp Round away from zero if abs(fraction) is > 0.
	@constant	kPRTimeRoundingMethod_QuickTime Use kPRTimeRoundingMethod_Truncate if converting from larger to smaller scale
				(ie. from more precision to less precision), but use kPRTimeRoundingMethod_RoundUp
				if converting from smaller to larger scale (ie. from less precision to more precision).
				Also, never round a negative number down to 0; always return the smallest magnitude
				negative PRTime in this case (-1/newTimescale).
 */
enum {
	kPRTimeRoundingMethod_RoundHalfAwayFromZero = 1,
	kPRTimeRoundingMethod_RoundTowardZero = 2,
	kPRTimeRoundingMethod_RoundAwayFromZero = 3,
	kPRTimeRoundingMethod_QuickTime = 4,
	kPRTimeRoundingMethod_RoundTowardPositiveInfinity = 5,
	kPRTimeRoundingMethod_RoundTowardNegativeInfinity = 6,
	
	kPRTimeRoundingMethod_Default = kPRTimeRoundingMethod_RoundHalfAwayFromZero
};
typedef uint32_t PRTimeRoundingMethod;
	
/*!
	@function	PRTimeConvertScale
	@abstract	Returns a new PRTime containing the source PRTime converted to a new timescale (rounding as requested).
	@discussion If the value needs to be rounded, the kPRTimeFlags_HasBeenRounded flag will be set.
				See definition of PRTimeRoundingMethod for a discussion of the various rounding methods available. If
				the source time is non-numeric (ie. infinite, indefinite, invalid), the result will be similarly non-numeric.
	@result		The converted result PRTime.
 */
PR_EXPORT 
PRTime PRTimeConvertScale(
				PRTime time,					/*! @param time		Source PRTime. */
				int32_t newTimescale,			/*! @param newTimescale	The requested timescale for the converted result PRTime. */
				PRTimeRoundingMethod method);	/*! @param method	The requested rounding method. */

/*!
	 @function	 PRTimeAdd
	 @abstract   Returns the sum of two PRTimes.
	 @discussion If the operands both have the same timescale, the timescale of the result will be the same as
				 the operands' timescale.  If the operands have different timescales, the timescale of the result
				 will be the least common multiple of the operands' timescales.  If that LCM timescale is
				 greater than kPRTimeMaxTimescale, the result timescale will be kPRTimeMaxTimescale,
				 and default rounding will be applied when converting the result to this timescale.
				 
				 If the result value overflows, the result timescale will be repeatedly halved until the result
				 value no longer overflows.  Again, default rounding will be applied when converting the
				 result to this timescale.  If the result value still overflows when timescale == 1, then the
				 result will be either positive or negative infinity, depending on the direction of the
				 overflow.
				 
				 If any rounding occurs for any reason, the result's kPRTimeFlags_HasBeenRounded flag will be
				 set.  This flag will also be set if either of the operands has kPRTimeFlags_HasBeenRounded set.
				 
				 If either of the operands is invalid, the result will be invalid.
				 
				 If the operands are valid, but just one operand is infinite, the result will be similarly
				 infinite. If the operands are valid, and both are infinite, the results will be as follows:
				 <ul>			+infinity + +infinity == +infinity
				 <br>			-infinity + -infinity == -infinity
				 <br>			+infinity + -infinity == invalid
				 <br>			-infinity + +infinity == invalid
				 </ul>
				 If the operands are valid, not infinite, and either or both is indefinite, the result
				 will be indefinite.
				 
				 If the two operands are numeric (ie. valid, not infinite, not indefinite), but have
				 different epochs, the result will be invalid. Times in different epochs cannot be
				 added or subtracted, because epoch length is unknown. Times in different epochs can
				 be compared, however, because numerically greater epochs always occur after numerically
				 lesser epochs.
	 @result     The sum of the two PRTimes (addend1 + addend2).
 */
PR_EXPORT 
PRTime PRTimeAdd(
				PRTime addend1,			/*! @param addend1			A PRTime to be added. */
				PRTime addend2			/*! @param addend2			Another PRTime to be added. */
				 );
/*!
	 @function	 PRTimeSubtract
	 @abstract   Returns the difference of two PRTimes.
	 @discussion If the operands both have the same timescale, the timescale of the result will be the same as
				 the operands' timescale.  If the operands have different timescales, the timescale of the result
				 will be the least common multiple of the operands' timescales.  If that LCM timescale is
				 greater than kPRTimeMaxTimescale, the result timescale will be kPRTimeMaxTimescale,
				 and default rounding will be applied when converting the result to this timescale.
				 
				 If the result value overflows, the result timescale will be repeatedly halved until the result
				 value no longer overflows.  Again, default rounding will be applied when converting the
				 result to this timescale.  If the result value still overflows when timescale == 1, then the
				 result will be either positive or negative infinity, depending on the direction of the
				 overflow.
				 
				 If any rounding occurs for any reason, the result's kPRTimeFlags_HasBeenRounded flag will be
				 set.  This flag will also be set if either of the operands has kPRTimeFlags_HasBeenRounded set.
				 
				 If either of the operands is invalid, the result will be invalid.
				 
				 If the operands are valid, but just one operand is infinite, the result will be similarly
				 infinite. If the operands are valid, and both are infinite, the results will be as follows:
				 <ul>			+infinity - +infinity == invalid
				 <li>			-infinity - -infinity == invalid
				 <li>			+infinity - -infinity == +infinity
				 <li>			-infinity - +infinity == -infinity
				 </ul>
				 If the operands are valid, not infinite, and either or both is indefinite, the result
				 will be indefinite.
				 
				 If the two operands are numeric (ie. valid, not infinite, not indefinite), but have
				 different nonzero epochs, the result will be invalid.  If they have the same nonzero
				 epoch, the result will have epoch zero (a duration).  Times in different epochs
				 cannot be added or subtracted, because epoch length is unknown.  Times in epoch zero
				 are considered to be durations and can be subtracted from times in other epochs.
				 Times in different epochs can be compared, however, because numerically greater
				 epochs always occur after numerically lesser epochs.
	 @result     The difference of the two PRTimes (minuend - subtrahend).
 */
PR_EXPORT 
PRTime PRTimeSubtract(
				PRTime minuend,
				PRTime subtrahend);

/*!
	 @function	 PRTimeCompare
	 @abstract   Returns the numerical relationship (-1 = less than, 1 = greater than, 0 = equal) of two PRTimes.
	 @discussion If the two PRTimes are numeric (ie. not invalid, infinite, or indefinite), and have
				 different epochs, it is considered that times in numerically larger epochs are always
				 greater than times in numerically smaller epochs.
				 
				 Since this routine will be used to sort lists by time, it needs to give all values
				 (even invalid and indefinite ones) a strict ordering to guarantee that sort algorithms
				 terminate safely. The order chosen is somewhat arbitrary:
				 
				 -infinity < all finite values < indefinite < +infinity < invalid
				 
				 Invalid PRTimes are considered to be equal to other invalid PRTimes, and larger than
				 any other PRTime. Positive infinity is considered to be smaller than any invalid PRTime,
				 equal to itself, and larger than any other PRTime. An indefinite PRTime is considered
				 to be smaller than any invalid PRTime, smaller than positive infinity, equal to itself,
				 and larger than any other PRTime.  Negative infinity is considered to be equal to itself,
				 and smaller than any other PRTime.
				 
				 -1 is returned if time1 is less than time2. 0 is returned if they
				 are equal. 1 is returned if time1 is greater than time2.
	 @result     The numerical relationship of the two PRTimes (-1 = less than, 1 = greater than, 0 = equal).
 */
PR_EXPORT 
int32_t PRTimeCompare(
				PRTime time1,		/*! @param time1 First PRTime in comparison. */
				PRTime time2);		/*! @param time2 Second PRTime in comparison. */

/*!
	 @function	 PRTIME_COMPARE_INLINE
	 @abstract   Returns whether the specified comparison of two PRTimes is true.
	 @discussion This is a macro that evaluates a Boolean result.
				 Example of usage:
				 PRTIME_COMPARE_INLINE(time1, <=, time2) will return true if time1 <= time2.
	 @param		 time1 First time to compare
	 @param		 comparator Comparison operation to perform (eg. <=).
	 @param		 time2 Second time to compare
	 @result     Returns the result of the specified PRTime comparison.
 */
#define PRTIME_COMPARE_INLINE(time1, comparator, time2) ((bool)(PRTimeCompare(time1, time2) comparator 0))

/*!
	 @function	 PRTimeGetSeconds
	 @abstract	 Converts a PRTime to seconds.
	 @discussion If the PRTime is invalid or indefinite, NAN is returned.  If the PRTime is infinite, +/- __inf()
				 is returned. If the PRTime is numeric, epoch is ignored, and time.value / time.timescale is
				 returned. The division is done in double, so the fraction is not lost in the returned result.
	 @result	 The resulting double number of seconds.
 */
PR_EXPORT 
double PRTimeGetSeconds(
			PRTime time);
	
/*!
	 @typedef	 PRTimeRange
	 @discussion PRTimeRange structs are non-opaque mutable structs that represent time ranges.
	 
				 A PRTimeRange is represented as two PRTime structs, one that specifies the start time of the
				 range and another that specifies the duration of the range. A time range does not include the time
				 that is the start time plus the duration.
 */
typedef struct
{
	PRTime start;		/*! @field start The start time of the time range. */
	PRTime duration;	/*! @field duration The duration of the time range. */
} PRTimeRange;
	
/*!
	 @typedef	 PRTimeMapping
	 @abstract	 A PRTimeMapping specifies the mapping of a segment of one time line (called "source") into another time line (called "target").
	 @discussion
				 When used for movie edit lists, the source time line is the media and the target time line is the track/movie.
	 @field	source
				 The time range on the source time line.
				 For an empty edit, source.start is an invalid PRTime, in which case source.duration shall be ignored.
				 Otherwise, source.start is the starting time within the source, and source.duration is the duration
				 of the source timeline to be mapped to the target time range.
	 @field	target
				 The time range on the target time line.
				 If target.duration and source.duration are different, then the source segment should
				 be played at rate source.duration/target.duration to fit.
 */
typedef struct {
	PRTimeRange source;
	PRTimeRange target;
} PRTimeMapping;

#pragma pack(pop)

#ifdef __cplusplus
}
#endif
	
#endif // PRORESTIME_H
