/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994,5, 6  Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#include <iostream.h>
#include <assert.h>
#include <stddef.h>

#define __STIME_TESTER__

#include <w.h>
#include <stime.h>

/*
   Test edge conditions in stime_t normal form conversion

   The following edge conditions exist seperately:

   1) sign of TOD matches sign of HIRES
   2) HIRES truncated and TOD fixed

   */


#ifndef HR_SECOND
#define	HR_SECOND	1000000
#define	st_tod	tv_sec
#define	st_hires	tv_usec
#endif


class stime_print_t : public stime_t {
public:
	ostream &print(ostream &) const;
	stime_print_t(const stime_t &time) : stime_t(time) { }
};

ostream &stime_print_t::print(ostream &s) const
{
	return s.form("(%d %d)", _time.st_tod, _time.st_hires);
}

ostream &operator<<(ostream &s, const stime_print_t &gr)
{
	return gr.print(s);
}


ostream &operator<<(ostream &s, const struct timeval &tv)
{
	return s.form("timeval(sec=%d  usec=%d)", tv.tv_sec, tv.tv_usec);
}

#ifdef USE_POSIX_TIME
ostream &operator<<(ostream &s, const struct timespec &ts)
{
	return s.form("timespec(sec=%d  nsec=%d)", ts.tv_sec, ts.tv_nsec);
}
#endif


int	tod;
int	hires;


void normal()
{
	int	i;
	int	n;
	stime_t	*raw;

	cout << "============ NORMAL ==================" << endl;

	raw = new stime_t[100];
	assert(raw != 0);
	n = 0;

	/* normal form w/ tod == 0 */
	new (raw + n++) stime_t(0, 8);
	new (raw + n++) stime_t(0, -8);
	new (raw + n++) stime_t(0, hires + 8);
	new (raw + n++) stime_t(0, -hires - 9);

	/* normal form w/ non-zero tod */
	new (raw + n++) stime_t(tod, 8);
	new (raw + n++) stime_t(tod, hires + 8);
	new (raw + n++) stime_t(-tod, -8);
	new (raw + n++) stime_t(-tod, -hires - 8);

	/* sign correction w/out normal */
	new (raw + n++) stime_t(tod, 9);
	new (raw + n++) stime_t(tod, -9);
	new (raw + n++) stime_t(-tod, -9);
	new (raw + n++) stime_t(-tod, 9);

	/* sign correction w/ normal */
	new (raw + n++) stime_t(tod, hires + 7);
	new (raw + n++) stime_t(tod, -hires - 7);
	new (raw + n++) stime_t(-tod, -hires - 7);
	new (raw + n++) stime_t(-tod, hires + 7);

	for (i = 0; i < n; i++) {
		stime_t	_norm, norm;

		_norm = norm = raw[i];

		_norm._normalize();
		norm.normalize();

		cout.form("[%d] == ", i);
		cout << (stime_print_t) raw[i];
		cout << " to " << (stime_print_t)_norm;
		if (norm != _norm)
			cout << " AND " << (stime_print_t)norm << "   XXX";

		cout << endl;
	}
		
	delete [] raw;
}


struct stime_pair {
	stime_t	l;
	stime_t r;
};


void arithmetic()
{
	int	i;
	int	n;
	stime_pair	*raw;

	cout << "============ ARITHMETIC ==================" << endl;

	raw = new stime_pair[100];
	assert(raw != 0);
	n = 0;

	raw[n].l = stime_t(0,  8);
	raw[n++].r = stime_t(0,  5);

	raw[n].l = stime_t(1, hires-7);
	raw[n++].r = stime_t(1, 8);

	raw[n].l = stime_t(1, hires-7);
	raw[n++].r = stime_t(0, 8);

	raw[n].l = stime_t(1, 8);
	raw[n++].r = stime_t(-2, -9);

	cout << "================== add / subtract =============" << endl;
	for (i = 0; i < n; i++) {
		cout.form("[%d] => ", i);
		cout << (stime_print_t) raw[i].l << " op ";
		cout << (stime_print_t) raw[i].r << ": ";

		cout << " +: " << (stime_print_t)(raw[i].l + raw[i].r);
		cout << ", +': " << (stime_print_t)(raw[i].r + raw[i].l);
		cout << ", -: " << (stime_print_t)(raw[i].l - raw[i].r);
		cout << ", -': " << (stime_print_t)(raw[i].r - raw[i].l);

		cout << endl;
	}

	cout << "================== scaling ==============" << endl;
	for (i = 0; i < n; i++) {
		cout.form("[%d] => ", i);
		cout << (stime_print_t) raw[i].l;

		cout << "  * " << (stime_print_t)(raw[i].l * 2);
		cout << "  *' " << (stime_print_t)(raw[i].l * 2.0);

		cout << "  / " << (stime_print_t)(raw[i].l / 2);
		cout << "  /' " << (stime_print_t)(raw[i].l / 2.0);

		cout << endl;
	}

		
	delete [] raw;
}


void conversion()
{
	stime_t a(stime_t::msec(hires));
	stime_t b(stime_t::usec(hires));
	stime_t c(stime_t::nsec(hires));

	cout << "hires == " << hires << endl;
	cout << "====== conversion statics ====" << endl;
	cout << "msec " << (stime_print_t)a << ' ' << a << endl;
	cout << "usec " << (stime_print_t)b << endl;
	cout << "nsec " << (stime_print_t)c << endl;

	cout << "======= output casts =======" << endl;
	a = stime_t(tod, hires);
	struct timeval tv;
	tv = a;
	cout << (stime_print_t)a << ": " << tv << endl;
#ifdef USE_POSIX_TIME
	struct timespec ts;
	ts = a;
	cout << (stime_print_t)a << ": " << ts << endl;
#endif

	/* XXX what is this supposed to test? */
	stime_t ten(10,0);
	stime_t tenn(ten);
	tenn.normalize();
	cout << "stime_t(10,0) == " << (stime_print_t)ten;
	cout << (stime_print_t)tenn << endl;
}


main(int argc, char **argv)
{
	tod = argc > 1 ? atoi(argv[1]) : 1;
	hires = (argc > 2 ? atoi(argv[2]) : 1) * HR_SECOND;

	normal();

	arithmetic();

	conversion();


	return 0;
}
