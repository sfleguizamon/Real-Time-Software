/*
 * swo_debug.c
 *
 *  Created on: 18 feb. 2020
 *      Author: VM_Esteban
 */

#include "swo_debug.h"

/*
**************************************************************************************************************************
*                                               SerPrintfMsg()
*
* Description :
* Argument(s) : none
* Return(s)   : none.
* Caller(s)   :
* Note(s)     : none.
**************************************************************************************************************************
*/
void SwdPrintf (CPU_CHAR  *p_fmt, ...)
{
    CPU_CHAR    str[80u + 1u];
    CPU_SIZE_T  len;
    va_list     vArgs;
    CPU_INT08U  i;

    va_start(vArgs, p_fmt);

    vsprintf((char       *)str,
             (char const *)p_fmt,
                           vArgs);

    va_end(vArgs);

    len = Str_Len(str);

    for (i = 0; i < len; i++)
     ITM_SendChar(str[i]);
}
