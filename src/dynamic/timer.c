/************************************************************

This file implements linux timer function with gettimeofday.

*************************************************************/
#include "timer.h"
#include <sys/time.h>
#include <stdlib.h>

double current_time(void)
{
	double timestamp;
	struct timeval tv;
	gettimeofday(&tv, 0);
	
	timestamp = (double)((double)(tv.tv_sec*1e6) +(double)tv.tv_usec);
	return timestamp;
}

/**************************************************************
						void WTIME(double);
**************************************************************/

void _wtime(double *t)
{
  static int sec = -1;

  struct timeval tv;
  gettimeofday(&tv, 0);
  if (sec < 0)
        sec = tv.tv_sec;
  *t = (tv.tv_sec - sec) + 1.0e-6*tv.tv_usec;
}

/*****************************************************************/
/******         E  L  A  P  S  E  D  _  T  I  M  E          ******/
/*****************************************************************/
double _elapsed_time( void )
{
    double t;

    _wtime( &t );
    return( t );
}

double start[128], elapsed[128];

/*****************************************************************/
/******            T  I  M  E  R  _  C  L  E  A  R          ******/
/*****************************************************************/
void _timer_clear( int n )
{
    elapsed[n] = 0.0;
}


/*****************************************************************/
/******            T  I  M  E  R  _  S  T  A  R  T          ******/
/*****************************************************************/
void _timer_start( int n )
{
    start[n] = _elapsed_time();
}


/*****************************************************************/
/******            T  I  M  E  R  _  S  T  O  P             ******/
/*****************************************************************/
void _timer_stop( int n )
{
    double t, now;

    now = _elapsed_time();
    t = now - start[n];
    elapsed[n] += t;

}

/*****************************************************************/
/******            T  I  M  E  R  _  R  E  A  D             ******/
/*****************************************************************/
double _timer_read( int n )
{
    return( elapsed[n] );
}
