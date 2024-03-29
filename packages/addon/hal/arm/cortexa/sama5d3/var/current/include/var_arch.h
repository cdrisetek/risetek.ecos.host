#ifndef CYGONCE_HAL_VAR_ARCH_H
#define CYGONCE_HAL_VAR_ARCH_H
//=============================================================================
//
//      var_arch.h
//
//      AT91 variant architecture overrides
//
//=============================================================================
// ####ECOSGPLCOPYRIGHTBEGIN####                                            
// -------------------------------------------                              
// This file is part of eCos, the Embedded Configurable Operating System.   
// Copyright (C) 2003 Free Software Foundation, Inc.                        
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
// Author(s):    jlarmour
// Contributors: Daniel Neri
// Date:         2003-06-24
// Purpose:      AT91 variant architecture overrides
// Description: 
// Usage:        #include <cyg/hal/hal_arch.h>
//
//####DESCRIPTIONEND####
//
//=============================================================================

#include <pkgconf/hal.h>
#include <cyg/hal/hal_io.h>

//=============================================================================
// Memory access macros

#ifndef CYGARC_CACHED_ADDRESS
#  define CYGARC_CACHED_ADDRESS(x)                      (x)
#endif
#ifndef CYGARC_UNCACHED_ADDRESS
#  define CYGARC_UNCACHED_ADDRESS(x)                    (x)
#endif
#ifndef CYGARC_PHYSICAL_ADDRESS
#  define CYGARC_PHYSICAL_ADDRESS(x)                    (x)
#endif
#ifndef CYGARC_VIRTUAL_ADDRESS
#  define CYGARC_VIRTUAL_ADDRESS(x)                     (x)
#endif

#ifndef HAL_MEMORY_BARRIER
#define HAL_MEMORY_BARRIER()                                            \
CYG_MACRO_START                                                         \
    asm volatile (                                                      \
        "mcr    p15, 0, %0, c7, c10 , 4;" /* drain the write buffer */  \
        :                                                               \
        : "r" (0)                                                       \
        : "memory"                                                      \
        );                                                              \
CYG_MACRO_END
#endif


//--------------------------------------------------------------------------
// Idle thread code.
// This macro is called in the idle thread loop, and gives the HAL the
// chance to insert code. Typical idle thread behaviour might be to halt the
// processor. These implementations halt the system core clock.

#ifndef HAL_IDLE_THREAD_ACTION
#define HAL_IDLE_THREAD_ACTION(_count_)                       \
CYG_MACRO_START                                               \
    HAL_WRITE_UINT32(AT91_PMC+AT91_PMC_SCDR, 1);              \
CYG_MACRO_END
#endif

//-----------------------------------------------------------------------------
// end of var_arch.h
#endif // CYGONCE_HAL_VAR_ARCH_H
