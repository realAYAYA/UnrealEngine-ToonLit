/*
 * Copyright 1995, 1996 Perforce Software.  All rights reserved.
 *
 * This file is part of Perforce - the FAST SCM System.
 */

/*
 * DateTime - get and set the date as a string
 */

// size for Fmt, FmtDay

# define DateTimeBufSize 20 

// size for FmtTz, which can say things like '0700 - Pacific Standard Time'

# define DateTimeZoneBufSize 80 

class DateTime {

    public:
		DateTime() {}
		DateTime( const int date ) { Set( date ); }
		DateTime( const char *date, Error *e ) { Set( date, e ); }

 	void	Set( const char *date, Error *e );
 	void	Set( const int date ) { wholeDay = 0; tval = (time_t)date; }
	void	SetNow() { Set( (int)Now() ); }

	time_t 	Compare( const DateTime &t2 ) const { 
	        return (tval - t2.tval); };

	void	Fmt( char *buf ) const;
	void	FmtDay( char *buf ) const;
	void	FmtDayUTC( char *buf ) const;
	void	FmtTz( char *buf ) const;
	void	FmtUTC( char *buf ) const;
	void 	FmtElapsed( char *buf, const DateTime &t2 );
	void	FmtUnifiedDiff( char *buf ) const;
	void	FmtISO8601( char *buf ) const;
	void	FmtISO8601Min( char *buf ) const;
	void	FmtRFC5322( char *buf ) const;

	void	SetGit( const StrPtr &gitDate, Error *e );
	void	FmtGit( StrBuf &buf ) const;

	time_t	Value() const { return tval; }
	time_t	Tomorrow() const { return tval + 24*60*60; }
	int	IsWholeDay() const { return wholeDay; }

	static time_t Never() { return 0; }
	static time_t Forever() { return (time_t)2147483647; }

	// for stat() and utime() conversion

	static time_t Localize( time_t centralTime );
	static time_t Centralize( time_t localTime );	
	time_t    TzOffset( int *isdst = 0 ) const;

    protected:
	time_t	Now();

    private:
	time_t	tval;
	int	wholeDay;

	time_t	ParseOffset( const char *s, const char *odate, Error *e );
};

class DateTimeNow : public DateTime {

    public:
		DateTimeNow() { Set( Now() ); }

} ;

// Pass a buffer of at least this size to DateTimeHighPrecision::Fmt():

# define DTHighPrecisionBufSize 40 

/*
 * Uses gettimeofday/clock_gettime/etc. to find more precise system time
 */
class DateTimeHighPrecision
{
    public:

	// Orthodox Canonical Form (OCF) methods (we don't need a dtor)
	        DateTimeHighPrecision(time_t secs = 0, int nsecs = 0)
		    : seconds( secs ), nanos( nsecs ) { }

	        DateTimeHighPrecision(const DateTimeHighPrecision &rhs)
		    : seconds( rhs.seconds ), nanos( rhs.nanos ) { }

	DateTimeHighPrecision &
		operator=( const DateTimeHighPrecision &rhs );

	DateTimeHighPrecision &
		operator+=( const DateTimeHighPrecision &rhs );

	DateTimeHighPrecision &
		operator-=( const DateTimeHighPrecision &rhs );

	bool
	operator==(
		const DateTimeHighPrecision &rhs) const;

	bool
	operator!=(
		const DateTimeHighPrecision &rhs) const;

	bool
	operator<(
		const DateTimeHighPrecision &rhs) const;

	bool
	operator<=(
		const DateTimeHighPrecision &rhs) const;

	bool
	operator>(
		const DateTimeHighPrecision &rhs) const;

	bool
	operator>=(
		const DateTimeHighPrecision &rhs) const;

	void	Now();
	void	Fmt( char *buf ) const;

	time_t	Seconds() const;
	int	Nanos() const;

	bool	IsZero() const { return seconds == 0 && nanos == 0; }

	// return (t2 - *this) in nanoseconds
	P4INT64 ElapsedNanos( const DateTimeHighPrecision &t2 ) const;

	void	FmtElapsed( StrBuf &buf, const DateTimeHighPrecision t2 ) const;
	// return < 0, = 0, or > 0 if *this < rhs, *this == rhs, or *this > rhs, respectively
	int 	Compare( const DateTimeHighPrecision &rhs ) const;

	P4INT64	ToNanos() const;
	P4INT64	ToMs() const;

    private:

	time_t	seconds; // Since 1/1/1970, natch
	int	nanos;
} ;

