#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/time.h>
#include "global.h"
#include "parser.h"
#include "connections.h"
#include "dallocate.h"
#include "xil_printf.h"
#include "FreeRTOS.h"
#include "task.h"
#include "align_sort.h"


/* ------------------------------------------------------------------------------------ */
/*                        Functions in align_sort.c                                     */
/* ------------------------------------------------------------------------------------ */
/* 1. void time_align(struct data_frame *df)                                           */
/* 2. void assign_df_to_TSB(struct data_frame *df, int index)                           */
/* 3. void dispatch(void *pvParameters)                                                 */
/* 4. void sort_data_inside_TSB(int index)                                              */
/* 5. void clear_TSB(int index)                                                         */
/* 6. int create_dataframe(int index)                                                    */
/* 7. void create_cfgframe()                                                             */
/* ------------------------------------------------------------------------------------ */
extern SemaphoreHandle_t mutex_on_TSB;
int i, ab;

/* ---------------------------------------------------------------------------- */
/* FUNCTION  initializeTSB():                                                    */
/* ---------------------------------------------------------------------------- */
void initializeTSB() {
	xil_printf("mphke TSB initialize\n");
    int j;
    //TaskHandle_t Deteache_thread;

    /* Initially all the TSBs are unused */
    for (j = 0; j < MAXTSB; j++)
        TSB[j].used = 0;

    /* Create dispatch task */
    //xTaskCreate(dispatch, "DispatchTask", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, &Deteache_thread);
}

/* ---------------------------------------------------------------------------- */
/* FUNCTION  get_TSB_index():                                                    */
/* ---------------------------------------------------------------------------- */
int get_TSB_index() {

	xil_printf("mphke TSB index\n");
    int j;
    struct timeval timer_start;

    if (xSemaphoreTake(mutex_on_TSB, portMAX_DELAY) == pdTRUE) {
        for (j = 0; j < MAXTSB; j++) {
            if (TSB[j].used == 0) {
                TSB[j].used = -1;
               // gettimeofday(&timer_start, NULL);
                printf("\nTSB[%d] occupied.\n", j);
                xSemaphoreGive(mutex_on_TSB);
                return j;
            }
        }
        xSemaphoreGive(mutex_on_TSB);
    }
    if(j == MAXTSB) return -1;
}

//* ---------------------------------------------------------------------------- */
/* FUNCTION  TSBwait(void* WT):                                                  */
/* ---------------------------------------------------------------------------- */
void* TSBwait(void* arg) {
	int ind =  arg;
	xil_printf("mphke TSB wait me index %d \n", ind);

   // struct waitTime *wt = (struct waitTime*) WT;
    //int ind = wt->index;


    TickType_t ticks = pdMS_TO_TICKS(2000);

    printf("Wait time %lu ms, for TSB[%d]\n", ticks, ind);

    vTaskDelay(ticks);

    if (xSemaphoreTake(mutex_on_TSB, portMAX_DELAY) == pdTRUE) {
        printf("\nWait time over for %d. ", ind);
        TSB[ind].used = 1;
        printf("Now TSB[%d].used = %d\n", ind, TSB[ind].used);
        xSemaphoreGive(mutex_on_TSB);
    }

    vTaskDelete(NULL);
}

/* ---------------------------------------------------------------------------- */
/* FUNCTION  time_align():                                                       */
/* It searches for the correct TSB[index] where data frame df is to be           */
/* assigned. If the df has soc and fracsec which is older then soc and fracsec   */
/* of TSB[first] then we discard the data frame                                  */
/* ---------------------------------------------------------------------------- */
void time_align(struct data_frame *df) {
	xil_printf("mphke time align\n");
    int flag = 0, j;

    /* Take the mutex to protect access to TSB array */
       if (xSemaphoreTake(mutex_on_TSB, portMAX_DELAY) == pdTRUE) {
           for (j = 0; j < MAXTSB; j++) {
               if (TSB[j].used == -1) {
                   if (!ncmp_cbyc((unsigned char *)TSB[j].soc, df->soc, 4)) {
                       if (!ncmp_cbyc((unsigned char *)TSB[j].fracsec, df->fracsec, 3)) {
                           flag = 1;
                           break;
                       }
                   } else {
                       continue;
                   }
               }
           }
           xSemaphoreGive(mutex_on_TSB); // Release the mutex
       }

       if (flag) {
           /* Print message and assign data frame to TSB */
           printf("TSB[%d] is already available for sec = %ld and fsec = %ld.\n", j,
                  to_long_int_convertor(df->soc), to_long_int_convertor(df->fracsec));
           assign_df_to_TSB(df, j);
       } else {
           int i = get_TSB_index();
           if (i == -1)
               printf("No TSB is free right now?\n");
           else
               assign_df_to_TSB(df, i);
       }
}

/* ---------------------------------------------------------------------------- */
/* FUNCTION  assign_df_to_TSB():                               	     		*/
/* It assigns the arrived data frame df to TSB[index]							*/
/* ---------------------------------------------------------------------------- */
void assign_df_to_TSB(struct data_frame *df, int index) {
    TaskHandle_t waitTask;

    xil_printf("mphke TSB assign df to tsb index is %d", index);

    /* Check if the TSB is used for the first time. If so we need to
       allocate memory to its member variables */
    if (TSB[index].soc == NULL) { // 1 if
        struct cfg_frame *temp_cfg = cfgfirst;

        TSB[index].soc = pvPortMalloc(5);
        TSB[index].fracsec = pvPortMalloc(5);

        memset(TSB[index].soc, '\0', 5);
        memset(TSB[index].fracsec, '\0', 5);

        copy_cbyc((unsigned char *) TSB[index].soc, df->soc, 4);
        copy_cbyc((unsigned char *) TSB[index].fracsec, df->fracsec, 4);

        TSB[index].first_data_frame = df;

        struct pmupdc_id_list *temp_pmuid;
        while (temp_cfg != NULL) {
            struct pmupdc_id_list *pmuid = pvPortMalloc(sizeof(struct pmupdc_id_list));
            pmuid->idcode = pvPortMalloc(3);
            memset(pmuid->idcode, '\0', 3);
            copy_cbyc((unsigned char *) pmuid->idcode, temp_cfg->idcode, 2);
            pmuid->num_pmu = to_intconvertor(temp_cfg->num_pmu);
            pmuid->nextid = NULL;

            if (TSB[index].idlist == NULL) {
                TSB[index].idlist = temp_pmuid;
            } else {
                temp_pmuid->nextid = pmuid;
                temp_pmuid = pmuid;
            }
            temp_cfg = temp_cfg->cfgnext;
        }

        temp_cfg = cfgfirst;
        if (temp_cfg != NULL) {
            //struct waitTime wt;
            //wt.index = index;
            //wt.wait_time = pdMS_TO_TICKS(2000);
        	int ind = index;

            xTaskCreate(TSBwait, "TSBWaitTask", configMINIMAL_STACK_SIZE*2, (void*)ind, tskIDLE_PRIORITY, &waitTask);
        }
    } else { // 1 if else
        struct cfg_frame *temp_cfg = cfgfirst;
        if (TSB[index].first_data_frame == NULL) { // 2 if
            copy_cbyc((unsigned char *) TSB[index].soc, df->soc, 4);
            copy_cbyc((unsigned char *) TSB[index].fracsec, df->fracsec, 4);

            TSB[index].first_data_frame = df;

            struct pmupdc_id_list *temp_pmuid;
            while (temp_cfg != NULL) {
                struct pmupdc_id_list *pmuid = pvPortMalloc(sizeof(struct pmupdc_id_list));
                pmuid->idcode = pvPortMalloc(3);
                memset(pmuid->idcode, '\0', 3);
                copy_cbyc((unsigned char *) pmuid->idcode, temp_cfg->idcode, 2);
                pmuid->num_pmu = to_intconvertor(temp_cfg->num_pmu);
                pmuid->nextid = NULL;

                if (TSB[index].idlist == NULL) {
                    TSB[index].idlist = temp_pmuid;
                } else {
                    temp_pmuid->nextid = pmuid;
                    temp_pmuid = pmuid;
                }
                temp_cfg = temp_cfg->cfgnext;
            }

            temp_cfg = cfgfirst;
            if (temp_cfg != NULL) {
               // struct waitTime wt;
               // wt.index = index;
                //wt.wait_time = pdMS_TO_TICKS(2000);

            	int ind = index;

                xTaskCreate(TSBwait, "TSBWaitTask", configMINIMAL_STACK_SIZE*2, (void*)ind, tskIDLE_PRIORITY, &waitTask);
            }
        } else { // 2 if else
            xil_printf("mphke TSB assign sthn else me to idio fracsec soc\n kai index %d", index);
            struct data_frame *temp_df, *check_df;

            check_df = TSB[index].first_data_frame;
            while (check_df != NULL) {
                if (!ncmp_cbyc(check_df->idcode, df->idcode, 2)) {
                    free_dataframe_object(df);
                    return;
                } else {
                    check_df = check_df->dnext;
                }
            }

            temp_df = TSB[index].first_data_frame;
            while (temp_df->dnext != NULL) {
                temp_df = temp_df->dnext;
            }

            temp_df->dnext = df;
            xil_printf("telos else tsb assign\n");
        } // 2 if ends
    } // 1 if ends
}

/* ---------------------------------------------------------------------------- */
/* FUNCTION  sort_data_inside_TSB():                                          */
/* This function sorts the data frames in the TSB[index] in the order of the   */
/* Idcodes present in the 'struct pmupdc_id_list list' of the TSB[index]       */
/* ---------------------------------------------------------------------------- */
void sort_data_inside_TSB(int index) {

	xil_printf("mphke TSB sort\n");
	struct pmupdc_id_list *temp_list;
	struct data_frame *prev_df,*curr_df,*sorted_df,*r_df,*s_df,*last_df,*p_df;
	int match = 0;
	unsigned int id_check;

	/* Pointer track_df will hold the address of the last sorted data_frame object.
	Thus we assign to the 'track_df->dnext ' the next sorted data_frame  object and so on */

	temp_list = TSB[index].idlist; /* Starting ID required for sorting */
	last_df = TSB[index].first_data_frame;
	p_df = TSB[index].first_data_frame;

	curr_df = last_df;
	sorted_df = prev_df = NULL;

	while(temp_list != NULL) { // 1 while

		match = 0;
		while(curr_df != NULL) { // 2. Traverse the pmu id in TSB and sort

			if(!ncmp_cbyc(curr_df->idcode,(unsigned char *)temp_list->idcode,2)){

				match = 1;
				break;

			} else {

				prev_df = curr_df;
				curr_df = curr_df->dnext;

			}

		} // 2 while ends

		if (match == 1) {

			if(prev_df == NULL) {

				r_df = curr_df;
				s_df = curr_df->dnext;
				if(sorted_df == NULL) {

					sorted_df = r_df;
					TSB[index].first_data_frame = sorted_df;
				} else {

					sorted_df->dnext = r_df;
					sorted_df = r_df;
				}
				sorted_df->dnext = s_df ;
				curr_df = last_df = s_df;

			} else {

				if(sorted_df == NULL) {

					r_df = curr_df;
					s_df = r_df->dnext;
					prev_df->dnext = s_df;
					sorted_df = r_df;
					TSB[index].first_data_frame = sorted_df;
					sorted_df->dnext = last_df ;
					curr_df = last_df;
					prev_df = NULL;

				} else {//if(sorted_df != NULL) {

					r_df = curr_df;
					s_df = r_df->dnext;
					prev_df->dnext = s_df;
					sorted_df->dnext = r_df;
					sorted_df = r_df;
					sorted_df->dnext = last_df ;
					curr_df = last_df;
					prev_df = NULL;
				}
			}

		}  else {  // id whose data frame did not arrive No match

			char *idcode;
			idcode = pvPortMalloc(3);

			struct data_frame *df = pvPortMalloc(sizeof(struct data_frame));
			if(!df) {

				printf("Not enough memory data_frame.\n");
			}
			df->dnext = NULL;

			// Allocate memory for df->framesize
			df->framesize = pvPortMalloc(3);
			if(!df->framesize) {

				printf("Not enough memory df->idcode\n");
				exit(1);
			}

			// Allocate memory for df->idcode
			df->idcode = pvPortMalloc(3);
			if(!df->idcode) {

				printf("Not enough memory df->idcode\n");
				exit(1);
			}

			// Allocate memory for df->soc
			df->soc = pvPortMalloc(5);
			if(!df->soc) {

				printf("Not enough memory df->soc\n");
				exit(1);
			}

			// Allocate memory for df->fracsec
			df->fracsec = pvPortMalloc(5);
			if(!df->fracsec) {

				printf("Not enough memory df->fracsec\n");
				exit(1);
			}

			/* 16 for sync,fsize,idcode,soc,fracsec,checksum */
			unsigned int size = (16 + (temp_list->num_pmu)*2)*sizeof(unsigned char);

			df->num_pmu = temp_list->num_pmu ;

			//Copy FRAMESIZE
			int_to_ascii_convertor(size,df->framesize);
			df->framesize[2] = '\0';

			//Copy IDCODE
			copy_cbyc (df->idcode,(unsigned char *)temp_list->idcode,2);
			df->idcode[2] = '\0';

			//Copy SOC
			copy_cbyc (df->soc,(unsigned char *)TSB[index].soc,4);
			df->soc[4] = '\0';

			//Copy FRACSEC
			copy_cbyc (df->fracsec,(unsigned char *)TSB[index].fracsec,4);
			df->fracsec[4] = '\0';

			df->dpmu = pvPortMalloc(temp_list->num_pmu * sizeof(struct data_for_each_pmu *));
			if(!df->dpmu) {

				printf("Not enough memory df->dpmu[][]\n");
				exit(1);
			}

			for (i = 0; i < temp_list->num_pmu; i++) {

				df->dpmu[i] = pvPortMalloc(sizeof(struct data_for_each_pmu));
			}

			int j = 0;

			// PMU data has not come
			while(j < temp_list->num_pmu) {

				df->dpmu[j]->stat = pvPortMalloc(3);
				if(!df->dpmu[j]->stat) {

					printf("Not enough memory for df->dpmu[j]->stat\n");
				}

				df->dpmu[j]->stat[0] = 0x00;
				df->dpmu[j]->stat[1] = 0x0F;
				df->dpmu[j]->stat[2] = '\0';
				j++;
			}

			if(sorted_df == NULL) {

				r_df = df;
				sorted_df = r_df;
				TSB[index].first_data_frame = sorted_df;
				sorted_df->dnext = last_df ;
				curr_df = last_df;
				prev_df = NULL;

			} else {

				r_df = df;
				sorted_df->dnext = r_df;
				sorted_df = r_df;
				sorted_df->dnext = last_df ;
				curr_df = last_df;
				prev_df = NULL;
			}
		}

		temp_list = temp_list->nextid;  //go for next ID

	} // 1. while ends

	p_df = TSB[index].first_data_frame;
	while(p_df != NULL){

		id_check = to_intconvertor(p_df->idcode);
		p_df = p_df->dnext;
	}
}

/* ---------------------------------------------------------------------------- */
/* FUNCTION  clear_TSB():                                                        */
/* It clears TSB[index] and frees all data frame objects after the data frames  */
/* in TSB[index] have been dispatched to destination device                      */
/* ---------------------------------------------------------------------------- */
void clear_TSB(int index) { //

	unsigned long int tsb_soc,tsb_fracsec;
	tsb_soc = to_long_int_convertor((unsigned char *)TSB[index].soc);
	tsb_fracsec = to_long_int_convertor((unsigned char *)TSB[index].fracsec);

	memset(TSB[index].soc,'\0',5);
	memset(TSB[index].fracsec,'\0',5);

	struct pmupdc_id_list *t_list,*r_list;
	t_list = TSB[index].idlist;

	while(t_list != NULL) {

		r_list = t_list->nextid;
		vPortFree(t_list->idcode);
		vPortFree(t_list);
		t_list = r_list;
	}

	struct data_frame *t,*r;
	t = TSB[index].first_data_frame;

	while(t != NULL) {

		r = t->dnext;
		free_dataframe_object(t);
		t = r;
	}

	TSB[index].first_data_frame = NULL;
	TSB[index].idlist = NULL;

	xSemaphoreTake(mutex_on_TSB, portMAX_DELAY);
	TSB[index].used = 0;
    printf("ClearTSB for [%d] & used = %d.\n", index, TSB[index].used);
	xSemaphoreGive(mutex_on_TSB);
}
