/*
 * getrusage() implementation for OS/2 kLIBC
 *
 * Copyright (C) 2016 KO Myung-Hun <komh@chollian.net>
 *
 * This program is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://www.wtfpl.net/ for more details.
 */

/* OS/2 kLIBC has declarations for getrusage() */
#include <sys/types.h> /* id_t */
#include <sys/resource.h>

#include <errno.h>
#include <string.h>

#include <time.h>
#include <sys/times.h>
#include <sys/param.h>

/**
 * getrusage()
 *
 * @remark Support only user time of a current process(RUSAGE_SELF)
 */
int getrusage (int who, struct rusage *r_usage)
{
    struct tms time;

    if( who != RUSAGE_SELF && who != RUSAGE_CHILDREN )
    {
        errno = EINVAL;

        return -1;
    }

    /* Intialize members of struct rusage */
    memset( r_usage, 0, sizeof( *r_usage ));

    if( times( &time ) != ( clock_t )-1 )
    {
        clock_t u;
        clock_t s;

        if( who == RUSAGE_CHILDREN )
        {
            u = time.tms_cutime;
            s = time.tms_cstime;
        }
        else
        {
            u = time.tms_utime;
            s = time.tms_stime;
        }

        r_usage->ru_utime.tv_sec = u / CLK_TCK;
        r_usage->ru_utime.tv_usec = ( u % CLK_TCK ) * 1000000U / CLK_TCK;
        r_usage->ru_stime.tv_sec = s / CLK_TCK;
        r_usage->ru_stime.tv_usec = (s % CLK_TCK ) * 1000000U / CLK_TCK;
    }

    return 0;
}