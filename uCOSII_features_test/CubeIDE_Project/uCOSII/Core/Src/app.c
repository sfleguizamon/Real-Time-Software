/*
 * .NOTES
 *    Version:        0.1
 *    Author:         Santiago F. Leguizamon
 *    Email:          stgoleguizamon@gmail.com
 *    Creation Date:  Tuesday, June 22nd 2021, 9:56:39 pm
 *    File: app.c
 *    Copyright (c) 2021 Santiago F. Leguizamon
 * HISTORY:
 * Date      	          By	Comments
 * ----------	          ---	----------------------------------------------------------
 * 
 * .LINK
 *    https://github.com/sfleguizamon
 */

/*
*********************************************************************************************************
*                                              INCLUDES
*********************************************************************************************************
*/

#include "app.h"
#include "main.h"
#include <cpu_core.h>
#include <os.h>
#include "DIO.h"

/*
*********************************************************************************************************
*                                              VARIABLES
*********************************************************************************************************
*/

extern u_int16_t output_mode ;
extern u_int16_t seq_number ;
extern u_int16_t frequencymeter_en ;
extern u_int16_t sin_gen_freq ;
extern u_int32_t measured_freq;

OS_EVENT *statusMbox;
OS_EVENT *measuredfreqMbox;

TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;

/*
*********************************************************************************************************
*                                           TASK PRIORITIES
*********************************************************************************************************
*/


#define  APP_CFG_KBCHECKTASK_PRIO		4u
#define  APP_CFG_SEQ1TASK_PRIO			5u
#define  APP_CFG_SEQ2TASK_PRIO			6u
#define  APP_CFG_SEQ3TASK_PRIO			7u
#define  APP_CFG_STATSHOWTASK_PRIO		8u

#define  OS_TASK_TMR_PRIO			(OS_LOWEST_PRIO - 2u)


/*
*********************************************************************************************************
*                                          TASK STACK SIZES
*                             Size of the task stacks (# of OS_STK entries)
*********************************************************************************************************
*/

#define  APP_CFG_KBCHECKTASK_STK_SIZE		128u
#define  APP_CFG_SEQ1TASK_STK_SIZE			128u
#define  APP_CFG_SEQ2TASK_STK_SIZE			128u
#define  APP_CFG_SEQ3TASK_STK_SIZE			128u
#define  APP_CFG_STATSHOWTASK_STK_SIZE		128u

/*
*********************************************************************************************************
*                                              TASK STACKS
*********************************************************************************************************
*/

static OS_STK KbchecktaskStk[APP_CFG_KBCHECKTASK_STK_SIZE];
static OS_STK Seq1TaskStk[APP_CFG_SEQ1TASK_STK_SIZE];
static OS_STK Seq2TaskStk[APP_CFG_SEQ2TASK_STK_SIZE];
static OS_STK Seq3TaskStk[APP_CFG_SEQ3TASK_STK_SIZE];
static OS_STK StatshowtaskStk[APP_CFG_STATSHOWTASK_STK_SIZE];

/*
*********************************************************************************************************
*                                           FUNCTIONS PROTOTYPES
*********************************************************************************************************
*/

static void App_TaskCreate (void);
static void App_EventCreate (void);
static void Kbchecktask(void *p_arg);
static void Seq1Task(void *p_arg);
static void Seq2Task(void *p_arg);
static void Seq3Task(void *p_arg);
static void Statshowtask(void *p_arg);

/*
*********************************************************************************************************
*                                           	   TASKS
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                          		Kbchecktask()
*
* Description : Controls the inputs and enable differents output/input modes.
*
* Argument(s) : p_arg
*
* Return(s)   : none.
*
* Caller(s)   : This is a task.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static void Kbchecktask (void *p_arg)
{
	(void)p_arg ;
	int16_t data[4];
	BOOLEAN output_mode = 0 ;
	BOOLEAN frequency_meter_en = 0 ;
	uint8_t enabled_seq = 0 ;
	uint16_t sin_freq = 50;
	uint16_t prescaler = 562;

	while (DEF_TRUE){

		/* Checking keyboard related inputs and applying configurations selected by the user */

		if(DIGet(DIO_PA0)){										/* Enable sine generation or low freq sequences */
			output_mode = ~output_mode ;
			if(output_mode){
				HAL_TIM_Base_Start_IT(&htim4);
				DOSet(DIO_PC13, FALSE);
				DOSet(DIO_PC14, FALSE);
				DOSet(DIO_PC15, FALSE);
			}
			if(!output_mode){
				HAL_TIM_Base_Stop_IT(&htim4);
				switch(enabled_seq){
					case 0:
						OSTaskResume(APP_CFG_SEQ1TASK_PRIO) ;
						break ;
					case 1:
						OSTaskResume(APP_CFG_SEQ2TASK_PRIO) ;
						break ;
					case 2:
						OSTaskResume(APP_CFG_SEQ3TASK_PRIO) ;
						break ;
				}
			}
		}

		if(DIGet(DIO_PA1) && !output_mode){						/* Selects between 3 defined low freq sequences */
			enabled_seq++ ;
			if(enabled_seq>2) enabled_seq = 0 ;
			switch(enabled_seq){
				case 0:
					OSTaskResume(APP_CFG_SEQ1TASK_PRIO) ;
					break ;
				case 1:
					OSTaskResume(APP_CFG_SEQ2TASK_PRIO) ;
					break ;
				case 2:
					OSTaskResume(APP_CFG_SEQ3TASK_PRIO) ;
					break ;
			}
		}

		if(DIGet(DIO_PA2) && output_mode){						/* Increases sine frequency by changing timer prescaler */
			if(sin_freq < 500) sin_freq += 50;
			/* Prescaler = 72MHz/(256*10*sinfreq)
			 * 72MHz -> timer frequency
			 * 256 -> number of signal steps
			 * 10 -> Timer counter period
			 * */
			prescaler = 28125/sin_freq;
			__HAL_TIM_SET_PRESCALER(&htim4, prescaler);
		}

		if(DIGet(DIO_PA3) && output_mode){						/* Decreases sine frequency by changing timer prescaler */
			if(sin_freq>50) sin_freq -= 50;
			prescaler = 28125/sin_freq;
			__HAL_TIM_SET_PRESCALER(&htim4, prescaler);
		}

		if(DIGet(DIO_PA4)){										/* Enable/disables frequency meter */
			frequency_meter_en = ~frequency_meter_en ;
			if(frequency_meter_en){
				HAL_TIM_Base_Start_IT(&htim3);
				HAL_TIM_IC_Start_IT(&htim3, TIM_CHANNEL_1);
			}
			if(!frequency_meter_en){
				HAL_TIM_Base_Stop_IT(&htim3);
				HAL_TIM_IC_Stop_IT(&htim3, TIM_CHANNEL_1);
			}
		}

		/* Sending info to displaytask */
		data[0] = output_mode;
		data[1] = frequency_meter_en;
		data[2] = enabled_seq;
		data[3] = sin_freq;

		OSMboxPostOpt(statusMbox, (void *) &data, OS_POST_OPT_NONE);

		OSTimeDlyHMSM(0u, 0u, 0u, 150u) ;
	}
}

/*
*********************************************************************************************************
*                                          		Seq1Task()
*
* Description : Configures 1st low frequency sequence.
*
* Argument(s) : p_arg
*
* Return(s)   : none.
*
* Caller(s)   : This is a task.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static void Seq1Task (void *p_arg)
{
	(void)p_arg;
	while (DEF_TRUE){
		OSTaskSuspend(OS_PRIO_SELF) ;
		DOCfgBlink(DIO_PC13, DO_BLINK_EN_NORMAL, 1, 2);
		DOCfgBlink(DIO_PC14, DO_BLINK_EN_NORMAL, 2, 4);
		DOSet(DIO_PC13, TRUE);
		DOSet(DIO_PC14, TRUE);
		DOSet(DIO_PC15, FALSE);
	}
}

/*
*********************************************************************************************************
*                                          		Seq2Task()
*
* Description : Configures 2nd low frequency sequence.
*
* Argument(s) : p_arg
*
* Return(s)   : none.
*
* Caller(s)   : This is a task.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static void Seq2Task (void *p_arg)
{
	(void)p_arg;
	while (DEF_TRUE){
		OSTaskSuspend(OS_PRIO_SELF) ;
		DOCfgBlink(DIO_PC13, DO_BLINK_EN_NORMAL, 1, 2);
		DOCfgBlink(DIO_PC14, DO_BLINK_EN_NORMAL, 2, 4);
		DOCfgBlink(DIO_PC15, DO_BLINK_EN_NORMAL, 3, 4);
		DOSet(DIO_PC13, TRUE);
		DOSet(DIO_PC14, TRUE);
		DOSet(DIO_PC15, TRUE);

	}
}

/*
*********************************************************************************************************
*                                          		Seq3Task()
*
* Description : Configures 3rd low frequency sequence.
*
* Argument(s) : p_arg
*
* Return(s)   : none.
*
* Caller(s)   : This is a task.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static void Seq3Task (void *p_arg)
{
	(void)p_arg;
	while (DEF_TRUE){
		OSTaskSuspend(OS_PRIO_SELF) ;
		DOCfgBlink(DIO_PC13, DO_BLINK_EN_NORMAL, 2, 4);
		DOCfgBlink(DIO_PC14, DO_BLINK_EN_NORMAL, 3, 6);
		DOCfgBlink(DIO_PC15, DO_BLINK_EN_NORMAL, 4, 7);
		DOSet(DIO_PC13, TRUE);
		DOSet(DIO_PC14, TRUE);
		DOSet(DIO_PC15, TRUE);
	}
}

/*
*********************************************************************************************************
*                                          		Statshowtask()
*
* Description : Receives system status from other tasks. It writes over global variables because
* 				STM32CubeMonitor requires global variables to work. It could be implemented using an LCD.
*
* Argument(s) : p_arg
*
* Return(s)   : none.
*
* Caller(s)   : This is a task.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static void Statshowtask (void *p_arg)
{
	(void)p_arg;
	CPU_INT08U err;
	uint16_t *p;
	u_int32_t *p1;

	while (DEF_TRUE){
		p = OSMboxPend(statusMbox, 0, &err);
		output_mode = *p;
		frequencymeter_en = *(p+1);
		seq_number = *(p+2);

		if(output_mode){
			sin_gen_freq = *(p+3);
		}
		else sin_gen_freq = 0;

		if(frequencymeter_en){
			p1 = OSMboxPend(measuredfreqMbox, 50, &err);
			measured_freq = *p1;
		}
		else measured_freq = 0;

	}
}

/*
*********************************************************************************************************
*                                          App_TaskStart()
*
* Description : The startup task.  The uC/OS-II ticker should only be initialize once multitasking starts.
*
* Argument(s) : p_arg       Argument passed to 'App_TaskStart()' by 'OSTaskCreate()'.
*
* Return(s)   : none.
*
* Caller(s)   : This is a task.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void StartupTask (void *p_arg)
{
 CPU_INT32U cpu_clk;
 (void)p_arg;
 cpu_clk = HAL_RCC_GetHCLKFreq();
 /* Initialize and enable System Tick timer */
 OS_CPU_SysTickInitFreq(cpu_clk);

 #if (OS_TASK_STAT_EN > 0)
  OSStatInit();													/* Determine CPU capacity.                              */
 #endif

 DIOInit();

 App_EventCreate();												/* Create application events.                           */
 App_TaskCreate();												/* Create application tasks.                            */

 /* DIO inputs config */
 DICfgMode(DIO_PA0, DI_MODE_INV);								/* Mode inv because input switches are inverted.		*/
 DICfgMode(DIO_PA1, DI_MODE_INV);
 DICfgMode(DIO_PA2, DI_MODE_INV);
 DICfgMode(DIO_PA3, DI_MODE_INV);
 DICfgMode(DIO_PA4, DI_MODE_INV);

 /* DIO outputs config */
 DOCfgMode(DIO_PC13, DO_MODE_BLINK_ASYNC, FALSE);
 DOCfgMode(DIO_PC14, DO_MODE_BLINK_ASYNC, FALSE);
 DOCfgMode(DIO_PC15, DO_MODE_BLINK_ASYNC, FALSE);

 while (DEF_TRUE){
	 OSTaskSuspend(OS_PRIO_SELF);
  }
}

/*
*********************************************************************************************************
*                                          App_TaskCreate()
*
* Description : This function creates every task.
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Caller(s)   : StartupTask.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void App_TaskCreate (void)
{
    CPU_INT08U os_err;

    os_err = OSTaskCreateExt((void (*)(void *)) Kbchecktask,
                             (void          * ) 0,
                             (OS_STK        * )&KbchecktaskStk[APP_CFG_KBCHECKTASK_STK_SIZE - 1],
                             (INT8U           ) APP_CFG_KBCHECKTASK_PRIO,
                             (INT16U          ) APP_CFG_KBCHECKTASK_PRIO,
                             (OS_STK        * ) &KbchecktaskStk[0],
                             (INT32U          ) APP_CFG_KBCHECKTASK_STK_SIZE,
                             (void          * ) 0,
                             (INT16U          )(OS_TASK_OPT_STK_CLR | OS_TASK_OPT_STK_CHK));

    OSTaskNameSet( APP_CFG_KBCHECKTASK_PRIO, (INT8U *)"KbCheckTask", &os_err);

    os_err = OSTaskCreateExt((void (*)(void *)) Seq1Task,
                             (void          * ) 0,
                             (OS_STK        * )&Seq1TaskStk[APP_CFG_SEQ1TASK_STK_SIZE - 1],
                             (INT8U           ) APP_CFG_SEQ1TASK_PRIO,
                             (INT16U          ) APP_CFG_SEQ1TASK_PRIO,
                             (OS_STK        * ) &Seq1TaskStk[0],
                             (INT32U          ) APP_CFG_SEQ1TASK_STK_SIZE,
                             (void          * ) 0,
                             (INT16U          )(OS_TASK_OPT_STK_CLR | OS_TASK_OPT_STK_CHK));

    OSTaskNameSet( APP_CFG_SEQ1TASK_PRIO, (INT8U *)"Seq1Task", &os_err);

    os_err = OSTaskCreateExt((void (*)(void *)) Seq2Task,
                             (void          * ) 0,
                             (OS_STK        * )&Seq2TaskStk[APP_CFG_SEQ2TASK_STK_SIZE - 1],
                             (INT8U           ) APP_CFG_SEQ2TASK_PRIO,
                             (INT16U          ) APP_CFG_SEQ2TASK_PRIO,
                             (OS_STK        * ) &Seq2TaskStk[0],
                             (INT32U          ) APP_CFG_SEQ2TASK_STK_SIZE,
                             (void          * ) 0,
                             (INT16U          )(OS_TASK_OPT_STK_CLR | OS_TASK_OPT_STK_CHK));

    OSTaskNameSet( APP_CFG_SEQ2TASK_PRIO, (INT8U *)"Seq2Task", &os_err);

    os_err = OSTaskCreateExt((void (*)(void *)) Seq3Task,
                             (void          * ) 0,
                             (OS_STK        * )&Seq3TaskStk[APP_CFG_SEQ3TASK_STK_SIZE - 1],
                             (INT8U           ) APP_CFG_SEQ3TASK_PRIO,
                             (INT16U          ) APP_CFG_SEQ3TASK_PRIO,
                             (OS_STK        * ) &Seq3TaskStk[0],
                             (INT32U          ) APP_CFG_SEQ3TASK_STK_SIZE,
                             (void          * ) 0,
                             (INT16U          )(OS_TASK_OPT_STK_CLR | OS_TASK_OPT_STK_CHK));

    OSTaskNameSet( APP_CFG_SEQ3TASK_PRIO, (INT8U *)"Seq3Task", &os_err);

    os_err = OSTaskCreateExt((void (*)(void *)) Statshowtask,
                             (void          * ) 0,
                             (OS_STK        * )&StatshowtaskStk[APP_CFG_STATSHOWTASK_STK_SIZE - 1],
                             (INT8U           ) APP_CFG_STATSHOWTASK_PRIO,
                             (INT16U          ) APP_CFG_STATSHOWTASK_PRIO,
                             (OS_STK        * ) &StatshowtaskStk[0],
                             (INT32U          ) APP_CFG_STATSHOWTASK_STK_SIZE,
                             (void          * ) 0,
                             (INT16U          )(OS_TASK_OPT_STK_CLR | OS_TASK_OPT_STK_CHK));

    OSTaskNameSet( APP_CFG_STATSHOWTASK_PRIO, (INT8U *)"StatShowTask", &os_err);

}

/*
*********************************************************************************************************
*                                          App_EventCreate()
*
* Description : This function creates every event used.
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Caller(s)   : StartupTask.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void App_EventCreate (void){
	statusMbox = OSMboxCreate((void *) 0);
	measuredfreqMbox = OSMboxCreate((void *) 0);
}

/*
*********************************************************************************************************
*										INTERRUPTIONS CALLBACKS
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                       HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim4)
*
* Description : This writes pins B8 to B15. Those pins are the inputs of a DAC.
*
* Argument(s) : Pointer to TIM Time Base Handle Structure.
*
* Return(s)   : none.
*
* Caller(s)   : ISR.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim4){

	//OSIntEnter();

	static u_int8_t sin_counter = 0;
	static u_int8_t taps[] = {	0x80, 0x83, 0x86, 0x89, 0x8C, 0x8F, 0x92, 0x95, 0x98, 0x9B, 0x9E, 0xA2, 0xA5, 0xA7, 0xAA, 0xAD,
								0xB0, 0xB3, 0xB6, 0xB9, 0xBC, 0xBE, 0xC1, 0xC4, 0xC6, 0xC9, 0xCB, 0xCE, 0xD0, 0xD3, 0xD5, 0xD7,
								0xDA, 0xDC, 0xDE, 0xE0, 0xE2, 0xE4, 0xE6, 0xE8, 0xEA, 0xEB, 0xED, 0xEE, 0xF0, 0xF1, 0xF3, 0xF4,
								0xF5, 0xF6, 0xF8, 0xF9, 0xFA, 0xFA, 0xFB, 0xFC, 0xFD, 0xFD, 0xFE, 0xFE, 0xFE, 0xFF, 0xFF, 0xFF,
								0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0xFE, 0xFE, 0xFD, 0xFD, 0xFC, 0xFB, 0xFA, 0xFA, 0xF9, 0xF8, 0xF6,
								0xF5, 0xF4, 0xF3, 0xF1, 0xF0, 0xEE, 0xED, 0xEB, 0xEA, 0xE8, 0xE6, 0xE4, 0xE2, 0xE0, 0xDE, 0xDC,
								0xDA, 0xD7, 0xD5, 0xD3, 0xD0, 0xCE, 0xCB, 0xC9, 0xC6, 0xC4, 0xC1, 0xBE, 0xBC, 0xB9, 0xB6, 0xB3,
								0xB0, 0xAD, 0xAA, 0xA7, 0xA5, 0xA2, 0x9E, 0x9B, 0x98, 0x95, 0x92, 0x8F, 0x8C, 0x89, 0x86, 0x83,
								0x80, 0x7C, 0x79, 0x76, 0x73, 0x70, 0x6D, 0x6A, 0x67, 0x64, 0x61, 0x5D, 0x5A, 0x58, 0x55, 0x52,
								0x4F, 0x4C, 0x49, 0x46, 0x43, 0x41, 0x3E, 0x3B, 0x39, 0x36, 0x34, 0x31, 0x2F, 0x2C, 0x2A, 0x28,
								0x25, 0x23, 0x21, 0x1F, 0x1D, 0x1B, 0x19, 0x17, 0x15, 0x14, 0x12, 0x11, 0x0F, 0x0E, 0x0C, 0x0B,
								0x0A, 0x09, 0x07, 0x06, 0x05, 0x05, 0x04, 0x03, 0x02, 0x02, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00,
								0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x02, 0x02, 0x03, 0x04, 0x05, 0x05, 0x06, 0x07, 0x09,
								0x0A, 0x0B, 0x0C, 0x0E, 0x0F, 0x11, 0x12, 0x14, 0x15, 0x17, 0x19, 0x1B, 0x1D, 0x1F, 0x21, 0x23,
								0x25, 0x28, 0x2A, 0x2C, 0x2F, 0x31, 0x34, 0x36, 0x39, 0x3B, 0x3E, 0x41, 0x43, 0x46, 0x49, 0x4C,
								0x4F, 0x52, 0x55, 0x58, 0x5A, 0x5D, 0x61, 0x64, 0x67, 0x6A, 0x6D, 0x70, 0x73, 0x76, 0x79, 0x7C };

	GPIOB->ODR = (INT16U)( taps[sin_counter] << 8);
	sin_counter++;
	if (sin_counter > 255) sin_counter = 0;

	//OSIntExit();
}

/*
*********************************************************************************************************
*                       HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim3)
*
* Description : This function measures frequency by measuring time between rising edges of a square input
* 				signal. It sends frequency value through a message.
*
* Argument(s) : Pointer to TIM Time Base Handle Structure.
*
* Return(s)   : none.
*
* Caller(s)   : ISR.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim3){

	//OSIntEnter();

	static u_int32_t IC_Value1 ;
	static u_int32_t IC_Value2 ;
	static u_int32_t Difference;
	static u_int32_t Frequency ;
	static BOOLEAN first_cap ;	// flag para saber cuando se capturo el primer flanco

	if (htim3->Channel == HAL_TIM_ACTIVE_CHANNEL_1){
		if(first_cap==0){
			IC_Value1 = HAL_TIM_ReadCapturedValue(htim3, TIM_CHANNEL_1);
			first_cap = 1;
		}
		else if(first_cap==1){
			IC_Value2 = HAL_TIM_ReadCapturedValue(htim3, TIM_CHANNEL_1);
			if(IC_Value2>IC_Value1){
				Difference = IC_Value2-IC_Value1;
			}
			else if(IC_Value2<IC_Value1){
				Difference = ((0xffff-IC_Value1)+IC_Value2) + 1;
			}
			else{
				Error_Handler();
			}
			Frequency = HAL_RCC_GetPCLK1Freq()/Difference*2;
			OSMboxPostOpt(measuredfreqMbox, (void *) &Frequency, OS_POST_OPT_NONE);
			first_cap = 0;
		}
	}

	//OSIntExit();
}

