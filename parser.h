#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <stdio.h>

#define MAXBUFLEN 2000

/* ---------------------------------------------------------------- */
/*                 	Data Structure Prototypes	            */
/* ---------------------------------------------------------------- */

/* ---------------------------------------------------------------- */
/*                   Configuration Frame Data Structure	            */
/* ---------------------------------------------------------------- */

struct cfg_frame {
    char *framesize;
    char *idcode;
    char *soc;
    char *fracsec;
    char *time_base;
    char *num_pmu;
    struct for_each_pmu **pmu;
    char *data_rate;
    struct cfg_frame *cfgnext;
} *cfgfirst;

struct for_each_pmu {
    char *stn;
    char *idcode;
    char *data_format;
    struct format *fmt;
    char *phnmr;
    char *annmr;
    char *dgnmr;
    struct channel_names *cnext;
    char **phunit;
    char **anunit;
    char **dgunit;
    char *fnom;
    char *cfg_cnt;
};

struct channel_names {
    char **phnames;
    char **angnames;
    struct dgnames *first;
};

struct dgnames {
    char **dgn; // Stores 16 digital names for each word
    struct dgnames *dg_next;
};

struct format {
    char freq;
    char analog;
    char phasor;
    char polar;
};

/* ---------------------------------------------------------------- */
/*                 	Data Frame Data Structure	            */
/* ---------------------------------------------------------------- */

struct data_frame {
    char *framesize;
    char *idcode;
    char *soc;
    char *fracsec;
    int num_pmu;
    struct data_for_each_pmu **dpmu;
    struct data_frame *dnext;
};

struct data_for_each_pmu {
    char *stat;
    int phnmr;
    int annmr;
    int dgnmr;
    struct format *fmt;
    char **phasors;
    char **analog;
    char *freq;
    char *dfreq;
    char **digital;
};

/* ---------------------------------------------------------------- */
/*                 	Status Change Data Structure	            */
/* ---------------------------------------------------------------- */

struct status_change_pmupdcid {
    char idcode[3];
    struct status_change_pmupdcid *pmuid_next;
} *root_pmuid;

/* ---------------------------------------------------------------- */
/*                 	Function Prototypes   		            */
/* ---------------------------------------------------------------- */

void cfgparser(unsigned char st[]);

void write_cfg_to_file();

int dataparser(unsigned char data[]);

int check_statword(unsigned char stat[]);

void add_id_to_status_change_list(unsigned char idcode[]);

void remove_id_from_status_change_list(char idcode[]);

unsigned int to_intconvertor(unsigned char array[]);

void long_int_to_ascii_convertor(unsigned long int n, unsigned char hex[]);

void int_to_ascii_convertor(unsigned int n, unsigned char hex[]);

void copy_cbyc(unsigned char dst[], unsigned char *s, int size);

int ncmp_cbyc(unsigned char dst[], unsigned char src[], int size);

void byte_by_byte_copy(unsigned char dst[], unsigned char src[], int index, int n);

unsigned long int to_long_int_convertor(unsigned char array[]);

uint16_t compute_CRC(unsigned char *message, int length);
