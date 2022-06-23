#ifndef SENS_H
#define SENS_H

/* Sensor Thread Details */
#define SENS_T_STACK_SIZE 2048
#define SENS_T_PRIOR 4
#define SAMPLE_UPDATE_RATE 1000 //ms

#define RAD_TO_DEG 57.2958

extern struct k_thread sens_t_data;
extern k_tid_t sens_tid;
extern struct k_msgq sens_q;
/* ---------------------- */

/* Sensor Packet */
struct sens_packet {
    double hts221_temp;     //celsius
    double hts221_rh;       //rh%
    double lps22hb_press;   //kPA
    double lps22hb_temp;    //celsius
    uint32_t ccs811_eco2;   //ppm
    uint32_t ccs811_etvoc;  //ppb
    double xy_angle;        //angle in degrees
};

/* Function Declarations */
extern void sens_thread(void *, void *, void *);
/* ---------------------- */

#endif