#include "FreeRTOS.h"
#include "semphr.h"
#include <stdio.h>

#define MAXTSB 20

/* ---------------------------------------------------------------- */
/*                  Global Data Structures                        */
/* ---------------------------------------------------------------- */

/* Data Structure for Time Stamp Buffer */
struct TimeStampBuffer {
    unsigned char *soc;
    unsigned char *fracsec;
    int used;   // 0 for unused, -1 for used, and 1 is for ready to dispatch?
    struct pmupdc_id_list *idlist;
    struct data_frame *first_data_frame;
} TSB[MAXTSB];

struct pmupdc_id_list {
	unsigned char *idcode;
    int num_pmu;
    struct pmupdc_id_list *nextid;
};

struct waitTime {
    int index;
    int wait_time;
};

/* ---------------------------------------------------------------- */
/*                     Function Prototypes                         */
/* ---------------------------------------------------------------- */

void time_align(struct data_frame *df);

void assign_df_to_TSB(struct data_frame *df, int index);

void* dispatch();

void sort_data_inside_TSB(int index);

void clear_TSB(int index);

int create_dataframe(int index);

int create_cfgframe();

void initializeTSB();

int get_TSB_index();

void* TSBwait(void* index);
