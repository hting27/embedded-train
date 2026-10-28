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
*                                          Application Hooks
*
* Filename : app_hooks.c
* Version  : V2.92.13
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <os.h>
#include  <stdlib.h>
#include  <stdarg.h>


/*
*********************************************************************************************************
*                                      EXTERN  GLOBAL VARIABLES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                           LOCAL CONSTANTS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            LOCAL TABLES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/



/*
*********************************************************************************************************
*********************************************************************************************************
**                                         GLOBAL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/* HW1 BEGIN: input validation and a single output stream. */
FILE *fp = NULL;
FILE *Output_fp = NULL;
errno_t Output_err = 0;
int TASK_NUMBER = 0;
OS_STK **Task_STK = NULL; /* Retained for compatibility with the original header. */
task_para_set TaskParameter[OS_MAX_TASKS];

void OutFileInit(void)
{
    Output_err = fopen_s(&Output_fp, OUTPUT_FILE_NAME, "w");
    if (Output_err != 0 || Output_fp == NULL) {
        fprintf(stderr, "Cannot open %s\n", OUTPUT_FILE_NAME);
        exit(EXIT_FAILURE);
    }
}

void InputFile(void)
{
    char line[256];
    char extra;
    unsigned id, arrival, execution, period;
    int n = 0;

    if (fopen_s(&fp, INPUT_FILE_NAME, "r") != 0 || fp == NULL) {
        fprintf(stderr, "Cannot open %s\n", INPUT_FILE_NAME);
        exit(EXIT_FAILURE);
    }
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strspn(line, " \t\r\n") == strlen(line)) {
            continue;
        }
        if (n >= 2 || sscanf_s(line, "%u %u %u %u %c", &id, &arrival,
                              &execution, &period, &extra, 1u) != 4 ||
            id != (unsigned)(n + 1) || arrival != 0u ||
            execution > 65535u || period != (n == 0 ? 4u : 7u)) {
            fprintf(stderr, "HW1 requires rows: 1 0 C 4 and 2 0 C 7.\n");
            fclose(fp);
            exit(EXIT_FAILURE);
        }
        TaskParameter[n].TaskID = (INT16U)id;
        TaskParameter[n].TaskArriveTime = (INT16U)arrival;
        TaskParameter[n].TaskExecutionTime = (INT16U)execution;
        TaskParameter[n].TaskPeriodic = (INT16U)period;
        TaskParameter[n].TaskPriority = (INT16U)(n + 1);
        TaskParameter[n].TaskNumber = 0u;
        ++n;
    }
    if (ferror(fp) || n != 2) {
        fprintf(stderr, "TaskSet must contain exactly two valid tasks.\n");
        fclose(fp);
        exit(EXIT_FAILURE);
    }
    fclose(fp);
    fp = NULL;
    TASK_NUMBER = n;
}

void HW1_Log(const char *format, ...)
{
    char line[256];
    va_list args;
    va_start(args, format);
    vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    printf("%s", line);
    if (Output_fp != NULL) {
        fputs(line, Output_fp);
        fflush(Output_fp);
    }
}

static void HW1_TaskLabel(OS_TCB *ptcb, char *label, size_t size)
{
    task_para_set *data;
    if (ptcb->OSTCBPrio == OS_TASK_IDLE_PRIO) {
        snprintf(label, size, "task(%u)", (unsigned)ptcb->OSTCBPrio);
    } else {
        data = (task_para_set *)ptcb->OSTCBExtPtr;
        snprintf(label, size, "task(%u)(%u)",
                 (unsigned)data->TaskID, (unsigned)data->TaskNumber);
    }
}

void HW1_TraceSwitch(INT8U initial)
{
    char current[48], next[48];
    /* HW1 sample: initial dispatch = 0; actual switches are numbered from 0.
     * The kernel has already incremented OSCtxSwCtr, so offset only the display. */
    INT32U displayed_count = (initial != 0u || OSCtxSwCtr == 0u)
                          ? 0u : OSCtxSwCtr - 1u;
    if (initial != 0u) {
        strcpy_s(current, sizeof(current), "***********");
    } else {
        HW1_TaskLabel(OSTCBCur, current, sizeof(current));
    }
    HW1_TaskLabel(OSTCBHighRdy, next, sizeof(next));
    HW1_Log("%lu\t%s\t%s\t%lu\n", (unsigned long)OSTimeGet(),
            current, next, (unsigned long)displayed_count);
}

void HW1_TraceReady(const char *state, OS_TCB *ptcb,
                    OS_PRIO group_before, OS_PRIO table_before)
{
    HW1_Log("%lu\t%s\tprio(%u)\tosrdygrp(0x%02X)(0x%02X)"
            "\tosrdytbl(0x%02X)(0x%02X)\n", (unsigned long)OSTimeGet(),
            state, (unsigned)ptcb->OSTCBPrio, (unsigned)group_before,
            (unsigned)OSRdyGrp, (unsigned)table_before,
            (unsigned)OSRdyTbl[ptcb->OSTCBY]);
}
/* HW1 END */

/*
*********************************************************************************************************
*********************************************************************************************************
**                                        uC/OS-II APP HOOKS
*********************************************************************************************************
*********************************************************************************************************
*/

#if (OS_APP_HOOKS_EN > 0)

/*
*********************************************************************************************************
*                                  TASK CREATION HOOK (APPLICATION)
*
* Description : This function is called when a task is created.
*
* Argument(s) : ptcb   is a pointer to the task control block of the task being created.
*
* Note(s)     : (1) Interrupts are disabled during this call.
*********************************************************************************************************
*/

void  App_TaskCreateHook(OS_TCB * ptcb)
{
#if (APP_CFG_PROBE_OS_PLUGIN_EN == DEF_ENABLED) && (OS_PROBE_HOOKS_EN > 0)
    OSProbe_TaskCreateHook(ptcb);
#endif
}


/*
*********************************************************************************************************
*                                  TASK DELETION HOOK (APPLICATION)
*
* Description : This function is called when a task is deleted.
*
* Argument(s) : ptcb   is a pointer to the task control block of the task being deleted.
*
* Note(s)     : (1) Interrupts are disabled during this call.
*********************************************************************************************************
*/

void  App_TaskDelHook(OS_TCB * ptcb)
{
    (void)ptcb;
}


/*
*********************************************************************************************************
*                                    IDLE TASK HOOK (APPLICATION)
*
* Description : This function is called by OSTaskIdleHook(), which is called by the idle task.  This hook
*               has been added to allow you to do such things as STOP the CPU to conserve power.
*
* Argument(s) : none.
*
* Note(s)     : (1) Interrupts are enabled during this call.
*********************************************************************************************************
*/

#if OS_VERSION >= 251
void  App_TaskIdleHook(void)
{
}
#endif


/*
*********************************************************************************************************
*                                  STATISTIC TASK HOOK (APPLICATION)
*
* Description : This function is called by OSTaskStatHook(), which is called every second by uC/OS-II's
*               statistics task.  This allows your application to add functionality to the statistics task.
*
* Argument(s) : none.
*********************************************************************************************************
*/

void  App_TaskStatHook(void)
{
}


/*
*********************************************************************************************************
*                                   TASK RETURN HOOK (APPLICATION)
*
* Description: This function is called if a task accidentally returns.  In other words, a task should
*              either be an infinite loop or delete itself when done.
*
* Arguments  : ptcb      is a pointer to the task control block of the task that is returning.
*
* Note(s)    : none
*********************************************************************************************************
*/


#if OS_VERSION >= 289
void  App_TaskReturnHook(OS_TCB* ptcb)
{
    (void)ptcb;
}
#endif


/*
*********************************************************************************************************
*                                   TASK SWITCH HOOK (APPLICATION)
*
* Description : This function is called when a task switch is performed.  This allows you to perform other
*               operations during a context switch.
*
* Argument(s) : none.
*
* Note(s)     : (1) Interrupts are disabled during this call.
*
*               (2) It is assumed that the global pointer 'OSTCBHighRdy' points to the TCB of the task that
*                   will be 'switched in' (i.e. the highest priority task) and, 'OSTCBCur' points to the
*                  task being switched out (i.e. the preempted task).
*********************************************************************************************************
*/

#if OS_TASK_SW_HOOK_EN > 0
void  App_TaskSwHook(void)
{
#if (APP_CFG_PROBE_OS_PLUGIN_EN > 0) && (OS_PROBE_HOOKS_EN > 0)
    OSProbe_TaskSwHook();
#endif
}
#endif


/*
*********************************************************************************************************
*                                   OS_TCBInit() HOOK (APPLICATION)
*
* Description : This function is called by OSTCBInitHook(), which is called by OS_TCBInit() after setting
*               up most of the TCB.
*
* Argument(s) : ptcb    is a pointer to the TCB of the task being created.
*
* Note(s)     : (1) Interrupts may or may not be ENABLED during this call.
*********************************************************************************************************
*/

#if OS_VERSION >= 204
void  App_TCBInitHook(OS_TCB* ptcb)
{
    (void)ptcb;
}
#endif


/*
*********************************************************************************************************
*                                       TICK HOOK (APPLICATION)
*
* Description : This function is called every tick.
*
* Argument(s) : none.
*
* Note(s)     : (1) Interrupts may or may not be ENABLED during this call.
*********************************************************************************************************
*/

#if OS_TIME_TICK_HOOK_EN > 0
void  App_TimeTickHook(void)
{
#if (APP_CFG_PROBE_OS_PLUGIN_EN == DEF_ENABLED) && (OS_PROBE_HOOKS_EN > 0)
    OSProbe_TickHook();
#endif
}
#endif
#endif
