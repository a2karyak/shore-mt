/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994,5, 6  Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
/*
 * SGI machines differentiate between SysV and BSD
 * get time of day sys calls. Make sure it uses the
 * BSD sys calls by defining _BSD_TIME. Check /usr/include/sys/time.h
 */

#if defined(Irix)
#define _BSD_TIME
#endif

#include <sys/time.h>

#include <time.h>
#include <string.h>

#include <w_base.h>
#include <stime.h>

/*
   All this magic is to allow either timevals or timespecs
   to be used without code change.  It's disgusting, but it
   avoid templates.  Templates are evil.

   st_tod	== time of day part of the time
   st_hires	== "higher resolution" part of the time
   HR_SECOND	== high-resolution units in a second
 */  

#define	NS_SECOND	1000000000	/* nanoseconds in a second */
#define	US_SECOND	1000000	/* microseconds in a second */
#define	MS_SECOND	1000	/* millisecs in a second */

#ifdef USE_POSIX_TIME
typedef	struct timespec _stime_t;
#define	st_tod		tv_sec
#define	st_hires	tv_nsec
#define	HR_SECOND	NS_SECOND
#else
typedef struct timeval	_stime_t;
#define	st_tod		tv_sec
#define	st_hires	tv_usec
#define	HR_SECOND	US_SECOND
#endif


/*
   XXX problems

   Rounding policy is currently ill-defined.  I need to choose one,
   and make sure the various input and output (to/from other type)
   operators implement it uniformly.  XXX This really needs to be
   fixed.
 */

#if !defined(SOLARIS2) && !defined(AIX41)
extern "C" int gettimeofday(struct timeval *, struct timezone *);
#endif


/* Internal constructor, exposes implementation */
stime_t::stime_t(time_t tod, long hires)
{
	_time.st_tod = tod;
	_time.st_hires = hires;

	normalize();
}


stime_t::stime_t(double secs)
{
	_time.st_tod = (long) secs;
	_time.st_hires = (long) ((secs - _time.st_tod) * HR_SECOND);

	/* the conversion automagically normalizes */
}


stime_t::stime_t(const struct timeval &tv)
{
	_time.st_tod = tv.tv_sec;
	_time.st_hires = tv.tv_usec * (HR_SECOND / US_SECOND);

	normalize();
}


int	stime_t::operator==(const stime_t &r) const
{
	return _time.st_tod == r._time.st_tod &&
		_time.st_hires == r._time.st_hires;
}


int	stime_t::operator<(const stime_t &r) const
{
	if (_time.st_tod == r._time.st_tod)
		return _time.st_hires < r._time.st_hires;
	return _time.st_tod < r._time.st_tod;
}


int	stime_t::operator<=(const stime_t &r) const
{
	return *this == r  ||  *this < r;
}


static inline int sign(const int i)
{
	return i > 0 ? 1 : i < 0 ? -1 : 0;
	
}


#if !defined(HPUX8) && !defined(__xlC__)
static inline int abs(const int i)
{
	return i >= 0 ? i : -i;
}
#endif


/* Put a stime into normal form, where the HIRES part
   will contain less than a TODs worth of HIRES time.
   Also, the signs of the TOD and HIRES parts should
   agree (unless TOD==0) */

void stime_t::signs()
{
	if (_time.st_tod  &&  _time.st_hires
	    && sign(_time.st_tod) != sign(_time.st_hires)) {

		if (sign(_time.st_tod) == 1) {
			_time.st_tod--;
			_time.st_hires += HR_SECOND;
		}
		else {
			_time.st_tod++;
			_time.st_hires -= HR_SECOND;
		}
	}
}

/* off-by one */
void stime_t::_normalize()
{
	if (abs(_time.st_hires) >= HR_SECOND) {
		_time.st_tod += sign(_time.st_hires);
		_time.st_hires -= sign(_time.st_hires) * HR_SECOND;
	}
	signs();
}
   

/* something that could be completely wacked out */
void stime_t::normalize()
{
	int	factor;

	factor = _time.st_hires / HR_SECOND;
	if (factor) {
		_time.st_tod += factor;
		_time.st_hires -= HR_SECOND * factor;
	}

	signs();
}


stime_t	stime_t::operator-() const
{
	stime_t	result;

	result._time.st_tod = -_time.st_tod;
	result._time.st_hires = -_time.st_hires;

	return result;
}


stime_t	stime_t::operator+(const stime_t &r) const
{
	stime_t	result;

	result._time.st_tod  = _time.st_tod  + r._time.st_tod;
	result._time.st_hires = _time.st_hires + r._time.st_hires;

	result._normalize();

	return result;
}


stime_t	stime_t::operator-(const stime_t &r) const
{
	return *this + -r;
}


stime_t stime_t::operator*(const int factor) const
{
	stime_t	result;

	result._time.st_tod = _time.st_tod * factor;
	result._time.st_hires = _time.st_hires * factor;
	result.normalize();

	return result;
}

/* XXX
   Float scaling is stupid for the moment.  It doesn't need
   to use double arithmetic, instead it should use an
   intermediate normalization step which moves
   lost TOD units into the HIRES range.

   The double stuff at least makes it seem to work right.
 */  


stime_t stime_t::operator/(const int factor) const
{
	return *this / (double)factor;
}


stime_t	stime_t::operator*(const double factor) const
{
#if 1
	double d = *this;
	d *= factor;
	stime_t result(d); 
#else
	stime_t result;
	result._time.st_tod = (long) (_time.st_tod * factor);
	result._time.st_hires = (long) (_time.st_hires * factor);
#endif
	result.normalize();

	return result;
}


stime_t	stime_t::operator/(const double factor) const
{
	return *this * (1.0 / factor);
}



stime_t::operator double() const
{
	return _time.st_tod + _time.st_hires / (double) HR_SECOND;
}


stime_t::operator float() const
{
	return (double) *this;
//	return _time.st_tod + _time.st_hires / (float) HR_SECOND;
}


stime_t::operator struct timeval() const
{
	struct	timeval tv;
	tv.tv_sec = _time.st_tod;
	tv.tv_usec = (_time.st_hires * US_SECOND) / HR_SECOND;
	return tv;
}


void	stime_t::gettime()
{
	int	kr;
#ifdef USE_POSIX_TIME
	kr = clock_gettime(CLOCK_REALTIME, &_time);
#else
	kr = gettimeofday(&_time, 0);
#endif
	if (kr == -1)
		W_FATAL(fcOS);
}


ostream	&stime_t::print(ostream &s) const
{
	struct	tm	*local;
	char	*when;
	char	*nl;

	/* the second field of the time structs should be a time_t */
	time_t	kludge = _time.st_tod;
	local = localtime(&kludge);
	when = asctime(local);

	/* chop the newline */
	nl = strchr(when, '\n');
	if (nl)
		*nl = '\0';

	stime_t	tod(_time.st_tod, 0);

	s << when << " and " << sinterval_t(*this - tod);
	return s;
}


ostream	&sinterval_t::print(ostream &s) const
{
	/* XXX should decode interval in hours, min, sec, usec, nsec */
	if (_time.st_tod) {
		W_FORM(s)("%d sec%s",
		       _time.st_tod,
		       _time.st_hires ? " and " : "");
	}

	if (_time.st_hires) {
#ifdef USE_POSIX_TIME
		W_FORM(s)("%ld ns", _time.st_hires);
#else
		W_FORM(s)("%ld us", _time.st_hires);
#endif
	}

	return s;
}


ostream &operator<<(ostream &s, const stime_t &t)
{
	return t.print(s);
}


ostream &operator<<(ostream &s, const sinterval_t &t)
{
	return t.print(s);
}


/* Conversion operators */
/* XXX A common conversion operator should be created,
   with the conversion operators feeding it a scale factor

   It will also localize sources of error to one common point,
   and allow a choice of rounding mode(s). For example nsec(6000)
   turns into 0, not 6 usec.
   */

stime_t stime_t::sec(int sec)
{
	stime_t	r;

	r._time.st_tod = sec;
	r._time.st_hires = 0;

	return r;
}


stime_t	stime_t::usec(int us, int sec)
{
	stime_t	r;

	r._time.st_tod = sec + us / US_SECOND;
	r._time.st_hires = (us % US_SECOND) * (HR_SECOND / US_SECOND);
	/* conversion normalizes */

	return r;
}


stime_t	stime_t::msec(int ms, int sec)
{
	stime_t	r;

	r._time.st_tod = sec + ms / MS_SECOND;
	r._time.st_hires = (ms % MS_SECOND) * (HR_SECOND / MS_SECOND);
	/* conversion normalizes */

	return r;
}

	
stime_t	stime_t::nsec(int ns, int sec)
{
	stime_t	r;

	r._time.st_tod = sec + ns / NS_SECOND;
	r._time.st_hires = (ns % NS_SECOND) * (HR_SECOND / NS_SECOND);
	/* conversion normalizes */

	return r;
}


stime_t	stime_t::now()
{
	stime_t	now;
	now.gettime();

	return now;
}
