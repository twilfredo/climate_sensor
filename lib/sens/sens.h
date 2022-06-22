#ifndef SENS_H
#define SENS_H

/* Sensor Thread Details */
#define SENS_T_STACK_SIZE 2048
#define SENS_T_PRIOR 4

K_THREAD_STACK_DEFINE(sens_t_stack_area, SENS_T_STACK_SIZE);
struct k_thread sens_t_data;
k_tid_t sens_tid;
/* ---------------------- */

/* Function Declarations */
extern void sens_thread(void *, void *, void *);
/* ---------------------- */

#endif