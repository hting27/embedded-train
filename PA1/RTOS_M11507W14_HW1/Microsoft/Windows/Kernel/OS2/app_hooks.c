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
#include  <stdarg.h>
#include  <errno.h>
#include  <stdlib.h>


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

/* PA1: keep console/file I/O outside the tick path. */
static char PA1_Output[256 * (SYSTEM_END_TIME + 1) * (OS_MAX_TASKS + 3)];
static size_t PA1_OutputUsed;

void PA1_Log(const char *format, ...)
{
    va_list args;
    int length;

    va_start(args, format);
    length = vsnprintf(PA1_Output + PA1_OutputUsed,
                       sizeof(PA1_Output) - PA1_OutputUsed, format, args);
    va_end(args);
    if (length < 0 || (size_t)length >= sizeof(PA1_Output) - PA1_OutputUsed) {
        fprintf(stderr, "PA1 output buffer exhausted\n");
        exit(EXIT_FAILURE);
    }
    PA1_OutputUsed += (size_t)length;
}

void PA1_Finish(void)
{
    if (fwrite(PA1_Output, 1, PA1_OutputUsed, Output_fp) != PA1_OutputUsed ||
        fclose(Output_fp) != 0) {
        fprintf(stderr, "Cannot write Output.txt\n");
        exit(EXIT_FAILURE);
    }
    fwrite(PA1_Output, 1, PA1_OutputUsed, stdout);
    exit(EXIT_SUCCESS);
}

void OutFileInit(void) {
    /*Clear the file*/
    Output_err = fopen_s(&Output_fp, OUTPUT_FILE_NAME, "w");
    if (Output_err != 0) {
        fprintf(stderr, "Error to clear output file\n");
        exit(EXIT_FAILURE);
    }
    PA1_OutputUsed = 0;
}

void InputFile(void) {
    /*
     * Read File
     * Task Information:
     * Task_ID ArriveTime ExecutionTime Periodic
     */
    errno_t err;
    char str[256];
    char *ptr, *end;
    long TaskInfo[INFO];
    int i, j, line = 0;

    if ((err = fopen_s(&fp, INPUT_FILE_NAME, "r")) == 0)     /*task set 1-4*/
    {
        printf("The file 'TaskSet.txt' was opened\n");
    }
    else
    {
        fprintf(stderr, "The file 'TaskSet.txt' was not opened\n");
        exit(EXIT_FAILURE);
    }

    TASK_NUMBER = 0;
    while (fgets(str, sizeof(str), fp) != NULL)
    {
        line++;
        ptr = str;
        while (*ptr == ' ' || *ptr == '\t' || *ptr == '\r' || *ptr == '\n') ptr++;
        if (*ptr == '\0') continue;
        if (TASK_NUMBER >= OS_MAX_TASKS) goto invalid;
        for (i = 0; i < INFO; i++) {
            errno = 0;
            TaskInfo[i] = strtol(ptr, &end, 10);
            if (end == ptr || errno == ERANGE || TaskInfo[i] < 0 || TaskInfo[i] > 65535)
                goto invalid;
            if (*end != '\0' && *end != ' ' && *end != '\t' && *end != '\r' && *end != '\n')
                goto invalid;
            ptr = end;
        }
        while (*ptr == ' ' || *ptr == '\t' || *ptr == '\r' || *ptr == '\n') ptr++;
        if (*ptr != '\0' || TaskInfo[0] >= 65533 || TaskInfo[2] == 0 || TaskInfo[3] == 0)
            goto invalid;
        for (j = 0; j < TASK_NUMBER; j++) {
            if (TaskParameter[j].TaskID == TaskInfo[0]) goto invalid;
        }
        j = TASK_NUMBER++;
        TaskParameter[j].TaskID = (INT16U)TaskInfo[0];
        TaskParameter[j].TaskArriveTime = (INT16U)TaskInfo[1];
        TaskParameter[j].TaskExecutionTime = (INT16U)TaskInfo[2];
        TaskParameter[j].TaskPeriodic = (INT16U)TaskInfo[3];
        TaskParameter[j].TaskNumber = 0;
    }
    if (ferror(fp) || TASK_NUMBER == 0) goto invalid;
    fclose(fp);

    /*Initial Priority*/
    /* PA1: shorter periods have higher RM priority; break ties by TaskID. */
    for (i = 0; i < TASK_NUMBER; i++) {
        TaskParameter[i].TaskPriority = 1;
        for (j = 0; j < TASK_NUMBER; j++) {
            if ((ALGORITHM == RM && TaskParameter[j].TaskPeriodic < TaskParameter[i].TaskPeriodic) ||
                ((ALGORITHM == FIFO || TaskParameter[j].TaskPeriodic == TaskParameter[i].TaskPeriodic) &&
                 TaskParameter[j].TaskID < TaskParameter[i].TaskID)) {
                TaskParameter[i].TaskPriority++;
            }
        }
    }
    /*read file*/
    return;

invalid:
    fprintf(stderr, "Invalid TaskSet.txt at line %d (four integers, unique IDs, positive execution/period, at most %u tasks)\n",
            line, (unsigned)OS_MAX_TASKS);
    fclose(fp);
    exit(EXIT_FAILURE);
}

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

void  App_TaskCreateHook (OS_TCB *ptcb)
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

void  App_TaskDelHook (OS_TCB *ptcb)
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
void  App_TaskIdleHook (void)
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

void  App_TaskStatHook (void)
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
void  App_TaskReturnHook (OS_TCB  *ptcb)
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
void  App_TaskSwHook (void)
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
void  App_TCBInitHook (OS_TCB *ptcb)
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
void  App_TimeTickHook (void)
{
#if (APP_CFG_PROBE_OS_PLUGIN_EN == DEF_ENABLED) && (OS_PROBE_HOOKS_EN > 0)
    OSProbe_TickHook();
#endif
}
#endif
#endif
