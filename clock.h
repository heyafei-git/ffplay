//
// Created by heyafei on 2021/6/1.
//

#ifndef BJY_CLOCK_H
#define BJY_CLOCK_H

typedef struct Clock {
    double pts;           /* clock base */
    double pts_drift;     /* clock base minus time at which we updated the clock */
    double last_updated;
    double speed;
    int serial;           /* clock is based on a packet with this serial */
    int paused;
    int *queue_serial;    /* pointer to the current packet queue serial, used for obsolete clock detection */
} Clock;


#endif //BJY_CLOCK_H
