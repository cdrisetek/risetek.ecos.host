/*=============================================================================
//
//      hal_diag.c
//
//      HAL diagnostic output code
//
//=============================================================================
// ####ECOSGPLCOPYRIGHTBEGIN####                                            
// -------------------------------------------                              
// This file is part of eCos, the Embedded Configurable Operating System.   
// Copyright (C) 1998, 1999, 2000, 2001, 2002 Free Software Foundation, Inc.
//
// eCos is free software; you can redistribute it and/or modify it under    
// the terms of the GNU General Public License as published by the Free     
// Software Foundation; either version 2 or (at your option) any later      
// version.                                                                 
//
// eCos is distributed in the hope that it will be useful, but WITHOUT      
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or    
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License    
// for more details.                                                        
//
// You should have received a copy of the GNU General Public License        
// along with eCos; if not, write to the Free Software Foundation, Inc.,    
// 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.            
//
// As a special exception, if other files instantiate templates or use      
// macros or inline functions from this file, or you compile this file      
// and link it with other works to produce a work based on this file,       
// this file does not by itself cause the resulting work to be covered by   
// the GNU General Public License. However the source code for this file    
// must still be made available in accordance with section (3) of the GNU   
// General Public License v2.                                               
//
// This exception does not invalidate any other reasons why a work based    
// on this file might be covered by the GNU General Public License.         
// -------------------------------------------                              
// ####ECOSGPLCOPYRIGHTEND####                                              
//=============================================================================
//#####DESCRIPTIONBEGIN####
//
// Author(s):   jskov
// Contributors:jskov, gthomas
// Date:        2001-07-12
// Purpose:     HAL diagnostic output
// Description: Implementations of HAL diagnostic output support.
//
//####DESCRIPTIONEND####
//
//===========================================================================*/

#include <pkgconf/hal.h>
#include CYGBLD_HAL_PLATFORM_H

#include <cyg/infra/cyg_type.h>         // base types

#include <cyg/hal/hal_arch.h>           // SAVE/RESTORE GP macros
#include <cyg/hal/hal_io.h>             // IO macros
#include <cyg/hal/hal_if.h>             // interface API
#include <cyg/hal/hal_intr.h>           // HAL_ENABLE/MASK/UNMASK_INTERRUPTS
#include <cyg/hal/hal_misc.h>           // Helper functions
#include <cyg/hal/drv_api.h>            // CYG_ISR_HANDLED
#include <cyg/hal/hal_diag.h>

#include <cyg/hal/var_io.h>             // USART registers

#include "hal_diag_dcc.h"               // DCC initialization file
//-----------------------------------------------------------------------------
typedef struct {
    cyg_uint8* base;
    cyg_int32 msec_timeout;
    int isr_vector;
    int baud_rate;
} channel_data_t;

//-----------------------------------------------------------------------------

void
cyg_hal_plf_serial_putc(void *__ch_data, char c);

static void
cyg_hal_plf_serial_init_channel(void* __ch_data)
{
    channel_data_t* chan = (channel_data_t*)__ch_data;
    cyg_uint8* base = chan->base;

    // Reset device
    HAL_WRITE_UINT32(base+AT91_US_CR, AT91_US_CR_RxRESET | AT91_US_CR_TxRESET);

    // 8-1-no parity.
    HAL_WRITE_UINT32(base+AT91_US_MR,
                     AT91_US_MR_CLOCK_MCK | AT91_US_MR_LENGTH_8 |
                     AT91_US_MR_PARITY_NONE | AT91_US_MR_STOP_1);

    HAL_WRITE_UINT32(base+AT91_US_BRG, AT91_US_BAUD(chan->baud_rate));

    // Enable RX and TX
    HAL_WRITE_UINT32(base+AT91_US_CR, AT91_US_CR_RxENAB | AT91_US_CR_TxENAB);

}

void
cyg_hal_plf_serial_putc(void *__ch_data, char c)
{
    cyg_uint8* base = ((channel_data_t*)__ch_data)->base;
    cyg_uint32 status, ch;
    CYGARC_HAL_SAVE_GP();

    do {
        HAL_READ_UINT32(base+AT91_US_CSR, status);
    } while ((status & AT91_US_CSR_TxRDY) == 0);

    ch = (cyg_uint32)c;
    HAL_WRITE_UINT32(base+AT91_US_THR, ch);

    CYGARC_HAL_RESTORE_GP();
}

static cyg_bool
cyg_hal_plf_serial_getc_nonblock(void* __ch_data, cyg_uint8* ch)
{
    channel_data_t* chan = (channel_data_t*)__ch_data;
    cyg_uint8* base = chan->base;
    cyg_uint32 stat;
    cyg_uint32 c;

    HAL_READ_UINT32(base+AT91_US_CSR, stat);
    if ((stat & AT91_US_CSR_RxRDY) == 0)
        return false;

    HAL_READ_UINT32(base+AT91_US_RHR, c);
    *ch = (cyg_uint8)(c & 0xff);

    return true;
}

cyg_uint8
cyg_hal_plf_serial_getc(void* __ch_data)
{
    cyg_uint8 ch;
    CYGARC_HAL_SAVE_GP();

    while(!cyg_hal_plf_serial_getc_nonblock(__ch_data, &ch));

    CYGARC_HAL_RESTORE_GP();
    return ch;
}

static void
cyg_hal_plf_serial_write(void* __ch_data, const cyg_uint8* __buf, 
                         cyg_uint32 __len)
{
    CYGARC_HAL_SAVE_GP();

    while(__len-- > 0)
        cyg_hal_plf_serial_putc(__ch_data, *__buf++);

    CYGARC_HAL_RESTORE_GP();
}

static void
cyg_hal_plf_serial_read(void* __ch_data, cyg_uint8* __buf, cyg_uint32 __len)
{
    CYGARC_HAL_SAVE_GP();

    while(__len-- > 0)
        *__buf++ = cyg_hal_plf_serial_getc(__ch_data);

    CYGARC_HAL_RESTORE_GP();
}

cyg_bool
cyg_hal_plf_serial_getc_timeout(void* __ch_data, cyg_uint8* ch)
{
    int delay_count;
    channel_data_t* chan = (channel_data_t*)__ch_data;
    cyg_bool res;
    CYGARC_HAL_SAVE_GP();

    delay_count = chan->msec_timeout * 50; // delay in .02 ms steps

    for(;;) {
        res = cyg_hal_plf_serial_getc_nonblock(__ch_data, ch);
        if (res || 0 == delay_count--)
            break;
        
        CYGACC_CALL_IF_DELAY_US(20);
    }

    CYGARC_HAL_RESTORE_GP();
    return res;
}

static int
cyg_hal_plf_serial_control(void *__ch_data, __comm_control_cmd_t __func, ...)
{
    static int irq_state = 0;
    channel_data_t* chan = (channel_data_t*)__ch_data;
    cyg_uint8* base = ((channel_data_t*)__ch_data)->base;
    int ret = 0;
    va_list ap;

    CYGARC_HAL_SAVE_GP();
    va_start(ap, __func);

    switch (__func) {
    case __COMMCTL_GETBAUD:
        ret = chan->baud_rate;
        break;
    case __COMMCTL_SETBAUD:
        chan->baud_rate = va_arg(ap, cyg_int32);
        // Should we verify this value here?
        cyg_hal_plf_serial_init_channel(chan);
        ret = 0;
        break;
    case __COMMCTL_IRQ_ENABLE:
        irq_state = 1;
        HAL_INTERRUPT_ACKNOWLEDGE(chan->isr_vector);
        HAL_INTERRUPT_UNMASK(chan->isr_vector);
        HAL_WRITE_UINT32(base+AT91_US_IER, AT91_US_IER_RxRDY);
        break;
    case __COMMCTL_IRQ_DISABLE:
        ret = irq_state;
        irq_state = 0;
        HAL_INTERRUPT_MASK(chan->isr_vector);
        HAL_WRITE_UINT32(base+AT91_US_IDR, AT91_US_IER_RxRDY);
        break;
    case __COMMCTL_DBG_ISR_VECTOR:
        ret = chan->isr_vector;
        break;
    case __COMMCTL_SET_TIMEOUT:
        ret = chan->msec_timeout;
        chan->msec_timeout = va_arg(ap, cyg_uint32);
    default:
        break;
    }

    va_end(ap);
    CYGARC_HAL_RESTORE_GP();
    return ret;
}

static int
cyg_hal_plf_serial_isr(void *__ch_data, int* __ctrlc, 
                       CYG_ADDRWORD __vector, CYG_ADDRWORD __data)
{
    int res = 0;
    channel_data_t* chan = (channel_data_t*)__ch_data;
    cyg_uint32 c;
    cyg_uint8 ch;
    cyg_uint32 stat;
    CYGARC_HAL_SAVE_GP();

    *__ctrlc = 0;
    HAL_READ_UINT32(chan->base+AT91_US_CSR, stat);
    if ( (stat & AT91_US_CSR_RxRDY) != 0 ) {

        HAL_READ_UINT32(chan->base+AT91_US_RHR, c);
        ch = (cyg_uint8)(c & 0xff);
        if( cyg_hal_is_break( (char*)&ch , 1 ) )
            *__ctrlc = 1;

        res = CYG_ISR_HANDLED;
    }

    HAL_INTERRUPT_ACKNOWLEDGE(chan->isr_vector);

    CYGARC_HAL_RESTORE_GP();
    return res;
}

static channel_data_t at91_ser_channels[CYGNUM_HAL_VIRTUAL_VECTOR_COMM_CHANNELS] = {
#if CYGNUM_HAL_VIRTUAL_VECTOR_COMM_CHANNELS > 0
    { (cyg_uint8*)AT91_USART0, 1000, CYGNUM_HAL_INTERRUPT_USART0, CYGNUM_HAL_VIRTUAL_VECTOR_CONSOLE_CHANNEL_BAUD},
#if CYGNUM_HAL_VIRTUAL_VECTOR_COMM_CHANNELS > 1
    { (cyg_uint8*)AT91_USART1, 1000, CYGNUM_HAL_INTERRUPT_USART1, CYGNUM_HAL_VIRTUAL_VECTOR_CONSOLE_CHANNEL_BAUD},
#if CYGNUM_HAL_VIRTUAL_VECTOR_COMM_CHANNELS > 2
    { (cyg_uint8*)AT91_USART2, 1000, CYGNUM_HAL_INTERRUPT_USART2, CYGNUM_HAL_VIRTUAL_VECTOR_CONSOLE_CHANNEL_BAUD},
#if CYGNUM_HAL_VIRTUAL_VECTOR_COMM_CHANNELS > 3
    { (cyg_uint8*)AT91_USART3, 1000, CYGNUM_HAL_INTERRUPT_USART3, CYGNUM_HAL_VIRTUAL_VECTOR_CONSOLE_CHANNEL_BAUD},
#if CYGNUM_HAL_VIRTUAL_VECTOR_COMM_CHANNELS > 4
    { (cyg_uint8*)AT91_USART4, 1000, CYGNUM_HAL_INTERRUPT_USART4, CYGNUM_HAL_VIRTUAL_VECTOR_CONSOLE_CHANNEL_BAUD},
#if CYGNUM_HAL_VIRTUAL_VECTOR_COMM_CHANNELS > 5
    { (cyg_uint8*)AT91_USART5, 1000, CYGNUM_HAL_INTERRUPT_USART5, CYGNUM_HAL_VIRTUAL_VECTOR_CONSOLE_CHANNEL_BAUD},
#if CYGNUM_HAL_VIRTUAL_VECTOR_COMM_CHANNELS > 6
    { (cyg_uint8*)AT91_USART6, 1000, CYGNUM_HAL_INTERRUPT_USART6, CYGNUM_HAL_VIRTUAL_VECTOR_CONSOLE_CHANNEL_BAUD}
#endif
#endif
#endif
#endif
#endif
#endif
#endif
};

static void
cyg_hal_plf_serial_init(void)
{
    hal_virtual_comm_table_t* comm;
    int cur, con;

    cur = CYGACC_CALL_IF_SET_CONSOLE_COMM(CYGNUM_CALL_IF_SET_COMM_ID_QUERY_CURRENT);

    // Init channels
#if CYGNUM_HAL_VIRTUAL_VECTOR_COMM_CHANNELS > 0
    HAL_ARM_AT91_PIO_CFG(AT91_USART_RXD0);
    HAL_ARM_AT91_PIO_CFG(AT91_USART_TXD0);
#ifdef AT91_PMC_PCER_US0
    HAL_WRITE_UINT32(AT91_PMC+AT91_PMC_PCER, AT91_PMC_PCER_US0);
#endif
#endif
#if CYGNUM_HAL_VIRTUAL_VECTOR_COMM_CHANNELS > 1
    HAL_ARM_AT91_PIO_CFG(AT91_USART_RXD1);
    HAL_ARM_AT91_PIO_CFG(AT91_USART_TXD1);
#ifdef AT91_PMC_PCER_US1
    HAL_WRITE_UINT32(AT91_PMC+AT91_PMC_PCER, AT91_PMC_PCER_US1);
#endif
#endif
#if CYGNUM_HAL_VIRTUAL_VECTOR_COMM_CHANNELS > 2
    HAL_ARM_AT91_PIO_CFG(AT91_USART_RXD2);
    HAL_ARM_AT91_PIO_CFG(AT91_USART_TXD2);
#ifdef AT91_PMC_PCER_US2
    HAL_WRITE_UINT32(AT91_PMC+AT91_PMC_PCER, AT91_PMC_PCER_US2);
#endif
#endif
#if CYGNUM_HAL_VIRTUAL_VECTOR_COMM_CHANNELS > 3
    HAL_ARM_AT91_PIO_CFG(AT91_USART_RXD3);
    HAL_ARM_AT91_PIO_CFG(AT91_USART_TXD3);
#ifdef AT91_PMC_PCER_US3
    HAL_WRITE_UINT32(AT91_PMC+AT91_PMC_PCER, AT91_PMC_PCER_US3);
#endif
#endif
#if CYGNUM_HAL_VIRTUAL_VECTOR_COMM_CHANNELS > 4
    HAL_ARM_AT91_PIO_CFG(AT91_USART_RXD4);
    HAL_ARM_AT91_PIO_CFG(AT91_USART_TXD4);
#ifdef AT91_PMC_PCER_US4
    HAL_WRITE_UINT32(AT91_PMC+AT91_PMC_PCER, AT91_PMC_PCER_US4);
#endif
#endif
#if CYGNUM_HAL_VIRTUAL_VECTOR_COMM_CHANNELS > 5
    HAL_ARM_AT91_PIO_CFG(AT91_USART_RXD5);
    HAL_ARM_AT91_PIO_CFG(AT91_USART_TXD5);
#ifdef AT91_PMC_PCER_US5
    HAL_WRITE_UINT32(AT91_PMC+AT91_PMC_PCER, AT91_PMC_PCER_US5);
#endif
#endif
#if CYGNUM_HAL_VIRTUAL_VECTOR_COMM_CHANNELS > 6
    HAL_ARM_AT91_PIO_CFG(AT91_USART_RXD6);
    HAL_ARM_AT91_PIO_CFG(AT91_USART_TXD6);
#ifdef AT91_PMC_PCER_US6
    HAL_WRITE_UINT32(AT91_PMC+AT91_PMC_PCER, AT91_PMC_PCER_US6);
#endif
#endif

    // Init channels
    for (con = 0; con < CYGNUM_HAL_VIRTUAL_VECTOR_COMM_CHANNELS; con++) {
        cyg_hal_plf_serial_init_channel(&at91_ser_channels[con]);
    }

    // Set channels
    for (con = 0; con < CYGNUM_HAL_VIRTUAL_VECTOR_COMM_CHANNELS; con++) {
        CYGACC_CALL_IF_SET_CONSOLE_COMM(con);
        comm = CYGACC_CALL_IF_CONSOLE_PROCS();
        CYGACC_COMM_IF_CH_DATA_SET(*comm, &at91_ser_channels[con]);
        CYGACC_COMM_IF_WRITE_SET(*comm, cyg_hal_plf_serial_write);
        CYGACC_COMM_IF_READ_SET(*comm, cyg_hal_plf_serial_read);
        CYGACC_COMM_IF_PUTC_SET(*comm, cyg_hal_plf_serial_putc);
        CYGACC_COMM_IF_GETC_SET(*comm, cyg_hal_plf_serial_getc);
        CYGACC_COMM_IF_CONTROL_SET(*comm, cyg_hal_plf_serial_control);
        CYGACC_COMM_IF_DBG_ISR_SET(*comm, cyg_hal_plf_serial_isr);
        CYGACC_COMM_IF_GETC_TIMEOUT_SET(*comm, cyg_hal_plf_serial_getc_timeout);
    }

#if 0
    // Init channels
#if CYGNUM_HAL_VIRTUAL_VECTOR_COMM_CHANNELS > 0
    cyg_hal_plf_serial_init_channel(&at91_ser_channels[0]);
#endif
#if CYGNUM_HAL_VIRTUAL_VECTOR_COMM_CHANNELS > 1
    cyg_hal_plf_serial_init_channel(&at91_ser_channels[1]);
#endif
#if CYGNUM_HAL_VIRTUAL_VECTOR_COMM_CHANNELS > 2
    cyg_hal_plf_serial_init_channel(&at91_ser_channels[2]);
#endif
#if CYGNUM_HAL_VIRTUAL_VECTOR_COMM_CHANNELS > 3
    cyg_hal_plf_serial_init_channel(&at91_ser_channels[3]);
#endif
#if CYGNUM_HAL_VIRTUAL_VECTOR_COMM_CHANNELS > 4
    cyg_hal_plf_serial_init_channel(&at91_ser_channels[4]);
#endif
#if CYGNUM_HAL_VIRTUAL_VECTOR_COMM_CHANNELS > 5
    cyg_hal_plf_serial_init_channel(&at91_ser_channels[5]);
#endif
#if CYGNUM_HAL_VIRTUAL_VECTOR_COMM_CHANNELS > 6
    cyg_hal_plf_serial_init_channel(&at91_ser_channels[6]);
#endif
    // Setup procs in the vector table

    // Set channel 0
#if CYGNUM_HAL_VIRTUAL_VECTOR_COMM_CHANNELS > 0
    CYGACC_CALL_IF_SET_CONSOLE_COMM(0);
    comm = CYGACC_CALL_IF_CONSOLE_PROCS();
    CYGACC_COMM_IF_CH_DATA_SET(*comm, &at91_ser_channels[0]);
    CYGACC_COMM_IF_WRITE_SET(*comm, cyg_hal_plf_serial_write);
    CYGACC_COMM_IF_READ_SET(*comm, cyg_hal_plf_serial_read);
    CYGACC_COMM_IF_PUTC_SET(*comm, cyg_hal_plf_serial_putc);
    CYGACC_COMM_IF_GETC_SET(*comm, cyg_hal_plf_serial_getc);
    CYGACC_COMM_IF_CONTROL_SET(*comm, cyg_hal_plf_serial_control);
    CYGACC_COMM_IF_DBG_ISR_SET(*comm, cyg_hal_plf_serial_isr);
    CYGACC_COMM_IF_GETC_TIMEOUT_SET(*comm, cyg_hal_plf_serial_getc_timeout);
#endif

#if CYGNUM_HAL_VIRTUAL_VECTOR_COMM_CHANNELS > 1
    // Set channel 1
    CYGACC_CALL_IF_SET_CONSOLE_COMM(1);
    comm = CYGACC_CALL_IF_CONSOLE_PROCS();
    CYGACC_COMM_IF_CH_DATA_SET(*comm, &at91_ser_channels[1]);
    CYGACC_COMM_IF_WRITE_SET(*comm, cyg_hal_plf_serial_write);
    CYGACC_COMM_IF_READ_SET(*comm, cyg_hal_plf_serial_read);
    CYGACC_COMM_IF_PUTC_SET(*comm, cyg_hal_plf_serial_putc);
    CYGACC_COMM_IF_GETC_SET(*comm, cyg_hal_plf_serial_getc);
    CYGACC_COMM_IF_CONTROL_SET(*comm, cyg_hal_plf_serial_control);
    CYGACC_COMM_IF_DBG_ISR_SET(*comm, cyg_hal_plf_serial_isr);
    CYGACC_COMM_IF_GETC_TIMEOUT_SET(*comm, cyg_hal_plf_serial_getc_timeout);
#endif

#if CYGNUM_HAL_VIRTUAL_VECTOR_COMM_CHANNELS > 2
    // Set channel 2
    CYGACC_CALL_IF_SET_CONSOLE_COMM(2);
    comm = CYGACC_CALL_IF_CONSOLE_PROCS();
    CYGACC_COMM_IF_CH_DATA_SET(*comm, &at91_ser_channels[2]);
    CYGACC_COMM_IF_WRITE_SET(*comm, cyg_hal_plf_serial_write);
    CYGACC_COMM_IF_READ_SET(*comm, cyg_hal_plf_serial_read);
    CYGACC_COMM_IF_PUTC_SET(*comm, cyg_hal_plf_serial_putc);
    CYGACC_COMM_IF_GETC_SET(*comm, cyg_hal_plf_serial_getc);
    CYGACC_COMM_IF_CONTROL_SET(*comm, cyg_hal_plf_serial_control);
    CYGACC_COMM_IF_DBG_ISR_SET(*comm, cyg_hal_plf_serial_isr);
    CYGACC_COMM_IF_GETC_TIMEOUT_SET(*comm, cyg_hal_plf_serial_getc_timeout);
#endif

#if CYGNUM_HAL_VIRTUAL_VECTOR_COMM_CHANNELS > 3
    // Set channel 3
    CYGACC_CALL_IF_SET_CONSOLE_COMM(3);
    comm = CYGACC_CALL_IF_CONSOLE_PROCS();
    CYGACC_COMM_IF_CH_DATA_SET(*comm, &at91_ser_channels[3]);
    CYGACC_COMM_IF_WRITE_SET(*comm, cyg_hal_plf_serial_write);
    CYGACC_COMM_IF_READ_SET(*comm, cyg_hal_plf_serial_read);
    CYGACC_COMM_IF_PUTC_SET(*comm, cyg_hal_plf_serial_putc);
    CYGACC_COMM_IF_GETC_SET(*comm, cyg_hal_plf_serial_getc);
    CYGACC_COMM_IF_CONTROL_SET(*comm, cyg_hal_plf_serial_control);
    CYGACC_COMM_IF_DBG_ISR_SET(*comm, cyg_hal_plf_serial_isr);
    CYGACC_COMM_IF_GETC_TIMEOUT_SET(*comm, cyg_hal_plf_serial_getc_timeout);
#endif

#if CYGNUM_HAL_VIRTUAL_VECTOR_COMM_CHANNELS > 4
    // Set channel 4
    CYGACC_CALL_IF_SET_CONSOLE_COMM(4);
    comm = CYGACC_CALL_IF_CONSOLE_PROCS();
    CYGACC_COMM_IF_CH_DATA_SET(*comm, &at91_ser_channels[4]);
    CYGACC_COMM_IF_WRITE_SET(*comm, cyg_hal_plf_serial_write);
    CYGACC_COMM_IF_READ_SET(*comm, cyg_hal_plf_serial_read);
    CYGACC_COMM_IF_PUTC_SET(*comm, cyg_hal_plf_serial_putc);
    CYGACC_COMM_IF_GETC_SET(*comm, cyg_hal_plf_serial_getc);
    CYGACC_COMM_IF_CONTROL_SET(*comm, cyg_hal_plf_serial_control);
    CYGACC_COMM_IF_DBG_ISR_SET(*comm, cyg_hal_plf_serial_isr);
    CYGACC_COMM_IF_GETC_TIMEOUT_SET(*comm, cyg_hal_plf_serial_getc_timeout);
#endif

#if CYGNUM_HAL_VIRTUAL_VECTOR_COMM_CHANNELS > 5
    // Set channel 5
    CYGACC_CALL_IF_SET_CONSOLE_COMM(5);
    comm = CYGACC_CALL_IF_CONSOLE_PROCS();
    CYGACC_COMM_IF_CH_DATA_SET(*comm, &at91_ser_channels[5]);
    CYGACC_COMM_IF_WRITE_SET(*comm, cyg_hal_plf_serial_write);
    CYGACC_COMM_IF_READ_SET(*comm, cyg_hal_plf_serial_read);
    CYGACC_COMM_IF_PUTC_SET(*comm, cyg_hal_plf_serial_putc);
    CYGACC_COMM_IF_GETC_SET(*comm, cyg_hal_plf_serial_getc);
    CYGACC_COMM_IF_CONTROL_SET(*comm, cyg_hal_plf_serial_control);
    CYGACC_COMM_IF_DBG_ISR_SET(*comm, cyg_hal_plf_serial_isr);
    CYGACC_COMM_IF_GETC_TIMEOUT_SET(*comm, cyg_hal_plf_serial_getc_timeout);
#endif

#if CYGNUM_HAL_VIRTUAL_VECTOR_COMM_CHANNELS > 6
    // Set channel 6
    CYGACC_CALL_IF_SET_CONSOLE_COMM(6);
    comm = CYGACC_CALL_IF_CONSOLE_PROCS();
    CYGACC_COMM_IF_CH_DATA_SET(*comm, &at91_ser_channels[6]);
    CYGACC_COMM_IF_WRITE_SET(*comm, cyg_hal_plf_serial_write);
    CYGACC_COMM_IF_READ_SET(*comm, cyg_hal_plf_serial_read);
    CYGACC_COMM_IF_PUTC_SET(*comm, cyg_hal_plf_serial_putc);
    CYGACC_COMM_IF_GETC_SET(*comm, cyg_hal_plf_serial_getc);
    CYGACC_COMM_IF_CONTROL_SET(*comm, cyg_hal_plf_serial_control);
    CYGACC_COMM_IF_DBG_ISR_SET(*comm, cyg_hal_plf_serial_isr);
    CYGACC_COMM_IF_GETC_TIMEOUT_SET(*comm, cyg_hal_plf_serial_getc_timeout);
#endif
#endif

    // Restore original console
    CYGACC_CALL_IF_SET_CONSOLE_COMM(cur);
}

void
cyg_hal_plf_comms_init(void)
{
    static int initialized = 0;

    if (initialized)
        return;

    initialized = 1;

    cyg_hal_plf_serial_init();

#ifdef CYGBLD_HAL_ARM_AT91_DCC
    cyg_hal_plf_dcc_init(CYGBLD_HAL_ARM_AT91_DCC_CHANNEL);
#endif
}

void
hal_diag_led(int mask)
{
    hal_at91_set_leds(mask);
}

//-----------------------------------------------------------------------------
// End of hal_diag.c
