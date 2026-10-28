/*
*********************************************************************************************************
*                                            EXAMPLE CODE
*
*               This file is provided as an example on how to use Micrium products.
*
*               Please feel free to use any application code labeled as 'EXAMPLE CODE' in
*               your application products.  Example code may be used as is, in whole or in
*               part, or may be used as a reference only. This file can be modified as
*               required to meet the end-product requirements.
*
*               Please help us continue to provide the Embedded community with the finest
*               software available.  Your honesty is greatly appreciated.
*
*               You can find our product's user manual, API reference, release notes and
*               more information at https://doc.micrium.com.
*               You can contact us at www.micrium.com.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*
*                                              uC/OS-II
*                                            EXAMPLE CODE
*
* Filename : main.c
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <cpu.h>
#include  <lib_mem.h>
#include  <os.h>

#include  "app_cfg.h"


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/

/* HW1: keep the original OS2 project and the two task entry points. */
#define TASK_STACKSIZE 2048u
static OS_STK TaskStacks[2][TASK_STACKSIZE];
static void task1(void *p_arg);
static void task2(void *p_arg);
static void TaskBody(void *p_arg);

int main(void)
{
    INT8U err;
    int n;

    CPU_IntInit();
    Mem_Init();
    CPU_IntDis();
    CPU_Init();
    OSInit();
    InputFile();
    OutFileInit();

    /* HW1: priorities are 1 and 2; job numbers start at zero. */
    for (n = 0; n < TASK_NUMBER; ++n) {
        err = OSTaskCreateExt(n == 0 ? task1 : task2,
                             &TaskParameter[n],
                             &TaskStacks[n][TASK_STACKSIZE - 1u],
                             (INT8U)TaskParameter[n].TaskPriority,
                             TaskParameter[n].TaskID,
                             &TaskStacks[n][0], TASK_STACKSIZE,
                             &TaskParameter[n],
                             OS_TASK_OPT_STK_CHK | OS_TASK_OPT_STK_CLR);
        if (err != OS_ERR_NONE) {
            fprintf(stderr, "OSTaskCreateExt failed: %u\n", (unsigned)err);
            fclose(Output_fp);
            return EXIT_FAILURE;
        }
    }
    /* HW1: the initial dispatch is at tick zero. */
    OSTimeSet(0u);
    OSStart();
    return EXIT_FAILURE;  /* OSStart should not return. */
}

/* HW1: execution time in TaskSet is metadata, not a busy-wait for HW1. */
static void TaskBody(void *p_arg)
{
    task_para_set *task_data = (task_para_set *)p_arg;
    OS_CPU_SR cpu_sr = 0u;

    while (1) {
        OS_ENTER_CRITICAL();
        HW1_Log("%lu\tTask(%u) is running\n",
                (unsigned long)OSTimeGet(), (unsigned)task_data->TaskID);
        OS_EXIT_CRITICAL();
        OSTimeDly(task_data->TaskPeriodic);
    }
}

static void task1(void *p_arg)
{
    TaskBody(p_arg);
}

static void task2(void *p_arg)
{
    TaskBody(p_arg);
}
