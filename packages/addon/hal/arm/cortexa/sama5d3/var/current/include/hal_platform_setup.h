#ifndef CYGONCE_HAL_PLATFORM_SETUP_H
#define CYGONCE_HAL_PLATFORM_SETUP_H
//=============================================================================
//
//      hal_platform_setup.h
//
//      Riser主板启动代码
//
//=============================================================================
// Author(s):    ychun.w@gamil.com
// Date:         2012-12-05
// Usage:        #include <cyg/hal/hal_platform_setup.h>
//     Only used by "vectors.S"
//
//
//=============================================================================

#include <pkgconf/system.h>
#include CYGBLD_HAL_VARIANT_H		// Variant specific configuration
#include CYGBLD_HAL_PLATFORM_H		// Platform specific configuration
#include CYGBLD_HAL_BOARD_H			// Board resources define.
#include CYGHWR_MEMORY_LAYOUT_H
#include "plf_io_sama5d3.h"
#include "var_io.h"
#include <cyg/hal/hal_mmu.h>		// MMU definitions
#include <cyg/hal/hal_macro.h>
#include <cyg/hal/var_io.h>			// variant registers

// Macro to initialise the EBI interface
#ifdef CYG_HAL_STARTUP_ROMRAM

#define PLATFORM_SETUP1 _platform_setup1

#define CYGHWR_HAL_ARM_HAS_MMU
#define CYGSEM_HAL_ROM_RESET_USES_JUMP
// This macro represents the initial startup code for the platform
        .macro  _platform_setup1
		mrs r1,cpsr
		bic r1,r1,#0x1F  /* Put processor in SVC mode */
		orr r1,r1,#0x13
		msr cpsr,r1
// Eanble PIT, It is useful for a random number general
		ldr		r1,=AT91_PITC
		ldr		r0,=(AT91_PITC_PIMR_PITEN | 0xfffff)
		str		r0,[r1, #AT91_PITC_PIMR]

   		 _disable_cache

		// 允许外部复位信号
		ldr	r1, =AT91_RST
		ldr	r2, =( AT91_RST_RMR_KEY | AT91_RST_RMR_URSTEN )
		str	r2,[r1, #AT91_RST_RMR]

		// 禁止中断
		ldr		r1,=AT91_AIC
		ldr		r2,=0xffffffff
		str		r2,[r1, #AT91_AIC_IDCR]

		// 清除WATCHDOG
		// DISABLE WATCHDOG
#ifdef CYGNUM_HAL_DISABLE_WATCHDOG
		ldr		r1,=AT91_WDTC
		ldr		r2,=AT91_WDTC_WDMR_DIS
		str		r2,[r1, #AT91_WDTC_WDMR]
#endif
// define raw_led_support!
	 	ldr	r1,	=AT91_PIOE
	 	ldr r2, =( (1 << (AT91_GPIO_PE23 & 0xFF)) | ( 1 << (AT91_GPIO_PE24 & 0xFF)))
	   	str	r2,	[r1, #AT91_PIO_OWER]
	 	str	r2,	[r1, #AT91_PIO_PER]
	 	str	r2,	[r1, #AT91_PIO_OER]
	 	str	r2,	[r1, #AT91_PIO_CODR];

		_AT91SAM9_PLL_DBGU_INIT

		// 现在运行在高时钟下了。
//		_EBI_init

//		_SDRAMC_init

        // Set up a stack [for calling C code]
        ldr     r1,=__startup_stack
        ldr     r2,=SDRAM_PHYS_BASE
        orr     sp,r1,r2

#ifdef	CYGPKG_HAL_BOOT_SPI
//		LOAD_DATAFLASH
#else	// CYGPKG_HAL_BOOT_SPI
//		LOAD_NAND
#endif // CYGPKG_HAL_BOOT_SPI

        ldr     r2,=10f
        mov     pc,r2    /* Change address spaces */

10:
        nop
        nop
        nop
		BOOT_SHOW_DBGU_CHAR ':'
#if 0
#ifndef	NO_MMU
  		 _disable_cache

        // Create MMU tables
        bl      hal_mmu_init
        _turnon_mmu
#endif
#endif
		BOOT_SHOW_DBGU_CHAR ')'
		BOOT_SHOW_DBGU_CHAR '\r'
		BOOT_SHOW_DBGU_CHAR '\n'
		.endm

#else // defined(CYG_HAL_STARTUP_ROM) || defined(CYG_HAL_STARTUP_ROMRAM)
#define PLATFORM_SETUP1 _platform_setup2
#endif


//-----------------------------------------------------------------------------
// end of hal_platform_setup.h
#endif // CYGONCE_HAL_PLATFORM_SETUP_H
