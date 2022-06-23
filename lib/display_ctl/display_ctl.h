#ifndef DISPLAY_CTL_H
#define DISPLAY_CTL_H

/* Sensor Thread Details */
#define DISP_T_STACK_SIZE   2048
#define DISP_T_PRIOR        5
#define SPLASH_DELAY        500     //ms
#define SPLASH_DELAY1       1000    //ms
#define DISP_UPDATE_DELAY   1000    //ms
#define MODE_TEMPS          0
#define MODE_AIR_QUAL       1
#define MODE_STATS          2

extern struct k_thread disp_t_data;
extern k_tid_t disp_tid;
/* ---------------------- */

/* Function Declarations */
extern void disp_ctl_thread(void *, void *, void *);
/* ---------------------- */

#endif