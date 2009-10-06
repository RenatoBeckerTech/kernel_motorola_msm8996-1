/*
 * File: include/asm-blackfin/mach-bf527/anomaly.h
 * Bugs: Enter bugs at http://blackfin.uclinux.org/
 *
 * Copyright (C) 2004-2009 Analog Devices Inc.
 * Licensed under the GPL-2 or later.
 */

/* This file should be up to date with:
 *  - Revision D, 08/14/2009; ADSP-BF526 Blackfin Processor Anomaly List
 *  - Revision F, 03/03/2009; ADSP-BF527 Blackfin Processor Anomaly List
 */

#ifndef _MACH_ANOMALY_H_
#define _MACH_ANOMALY_H_

/* We do not support old silicon - sorry */
#if __SILICON_REVISION__ < 0
# error will not work on BF526/BF527 silicon version
#endif

#if defined(__ADSPBF522__) || defined(__ADSPBF524__) || defined(__ADSPBF526__)
# define ANOMALY_BF526 1
#else
# define ANOMALY_BF526 0
#endif
#if defined(__ADSPBF523__) || defined(__ADSPBF525__) || defined(__ADSPBF527__)
# define ANOMALY_BF527 1
#else
# define ANOMALY_BF527 0
#endif

#define _ANOMALY_BF526(rev526) (ANOMALY_BF526 && __SILICON_REVISION__ rev526)
#define _ANOMALY_BF527(rev527) (ANOMALY_BF527 && __SILICON_REVISION__ rev527)
#define _ANOMALY_BF526_BF527(rev526, rev527) (_ANOMALY_BF526(rev526) || _ANOMALY_BF527(rev527))

/* Multi-Issue Instruction with dsp32shiftimm in slot1 and P-reg Store in slot2 Not Supported */
#define ANOMALY_05000074 (1)
/* DMA_RUN Bit Is Not Valid after a Peripheral Receive Channel DMA Stops */
#define ANOMALY_05000119 (1)	/* note: brokenness is noted in documentation, not anomaly sheet */
/* Rx.H Cannot Be Used to Access 16-bit System MMR Registers */
#define ANOMALY_05000122 (1)
/* False Hardware Error from an Access in the Shadow of a Conditional Branch */
#define ANOMALY_05000245 (1)
/* Incorrect Timer Pulse Width in Single-Shot PWM_OUT Mode with External Clock */
#define ANOMALY_05000254 (1)
/* Sensitivity To Noise with Slow Input Edge Rates on External SPORT TX and RX Clocks */
#define ANOMALY_05000265 (1)
/* False Hardware Errors Caused by Fetches at the Boundary of Reserved Memory */
#define ANOMALY_05000310 (1)
/* PPI Is Level-Sensitive on First Transfer In Single Frame Sync Modes */
#define ANOMALY_05000313 (_ANOMALY_BF526_BF527(< 1, < 2))
/* Incorrect Access of OTP_STATUS During otp_write() Function */
#define ANOMALY_05000328 (_ANOMALY_BF527(< 2))
/* Host DMA Boot Modes Are Not Functional */
#define ANOMALY_05000330 (__SILICON_REVISION__ < 2)
/* Disallowed Configuration Prevents Subsequent Allowed Configuration on Host DMA Port */
#define ANOMALY_05000337 (_ANOMALY_BF527(< 2))
/* Ethernet MAC MDIO Reads Do Not Meet IEEE Specification */
#define ANOMALY_05000341 (_ANOMALY_BF527(< 2))
/* TWI May Not Operate Correctly Under Certain Signal Termination Conditions */
#define ANOMALY_05000342 (_ANOMALY_BF527(< 2))
/* USB Calibration Value Is Not Initialized */
#define ANOMALY_05000346 (_ANOMALY_BF526_BF527(< 1, < 2))
/* USB Calibration Value to use */
#define ANOMALY_05000346_value 0xE510
/* Preboot Routine Incorrectly Alters Reset Value of USB Register */
#define ANOMALY_05000347 (_ANOMALY_BF527(< 2))
/* Security Features Are Not Functional */
#define ANOMALY_05000348 (_ANOMALY_BF527(< 1))
/* bfrom_SysControl() Firmware Function Performs Improper System Reset */
#define ANOMALY_05000353 (_ANOMALY_BF526(< 1))
/* Regulator Programming Blocked when Hibernate Wakeup Source Remains Active */
#define ANOMALY_05000355 (_ANOMALY_BF527(< 2))
/* Serial Port (SPORT) Multichannel Transmit Failure when Channel 0 Is Disabled */
#define ANOMALY_05000357 (_ANOMALY_BF527(< 2))
/* Incorrect Revision Number in DSPID Register */
#define ANOMALY_05000364 (_ANOMALY_BF527(== 1))
/* PPI Underflow Error Goes Undetected in ITU-R 656 Mode */
#define ANOMALY_05000366 (1)
/* Incorrect Default CSEL Value in PLL_DIV */
#define ANOMALY_05000368 (_ANOMALY_BF527(< 2))
/* Possible RETS Register Corruption when Subroutine Is under 5 Cycles in Duration */
#define ANOMALY_05000371 (_ANOMALY_BF527(< 2))
/* Authentication Fails To Initiate */
#define ANOMALY_05000376 (_ANOMALY_BF527(< 2))
/* Data Read From L3 Memory by USB DMA May be Corrupted */
#define ANOMALY_05000380 (_ANOMALY_BF527(< 2))
/* 8-Bit NAND Flash Boot Mode Not Functional */
#define ANOMALY_05000382 (_ANOMALY_BF526_BF527(< 1, < 2))
/* Boot from OTP Memory Not Functional */
#define ANOMALY_05000385 (_ANOMALY_BF527(< 2))
/* bfrom_SysControl() Firmware Routine Not Functional */
#define ANOMALY_05000386 (_ANOMALY_BF527(< 2))
/* Programmable Preboot Settings Not Functional */
#define ANOMALY_05000387 (_ANOMALY_BF527(< 2))
/* CRC32 Checksum Support Not Functional */
#define ANOMALY_05000388 (_ANOMALY_BF526_BF527(< 1, < 2))
/* Reset Vector Must Not Be in SDRAM Memory Space */
#define ANOMALY_05000389 (_ANOMALY_BF527(< 2))
/* pTempCurrent Not Present in ADI_BOOT_DATA Structure */
#define ANOMALY_05000392 (_ANOMALY_BF527(< 2))
/* Deprecated Value of dTempByteCount in ADI_BOOT_DATA Structure */
#define ANOMALY_05000393 (_ANOMALY_BF527(< 2))
/* Log Buffer Not Functional */
#define ANOMALY_05000394 (_ANOMALY_BF527(< 2))
/* Hook Routine Not Functional */
#define ANOMALY_05000395 (_ANOMALY_BF527(< 2))
/* Header Indirect Bit Not Functional */
#define ANOMALY_05000396 (_ANOMALY_BF527(< 2))
/* BK_ONES, BK_ZEROS, and BK_DATECODE Constants Not Functional */
#define ANOMALY_05000397 (_ANOMALY_BF527(< 2))
/* SWRESET, DFRESET and WDRESET Bits in the SYSCR Register Not Functional */
#define ANOMALY_05000398 (_ANOMALY_BF527(< 2))
/* BCODE_NOBOOT in BCODE Field of SYSCR Register Not Functional */
#define ANOMALY_05000399 (_ANOMALY_BF527(< 2))
/* PPI Data Signals D0 and D8 do not Tristate After Disabling PPI */
#define ANOMALY_05000401 (_ANOMALY_BF526_BF527(< 1, < 2))
/* Level-Sensitive External GPIO Wakeups May Cause Indefinite Stall */
#define ANOMALY_05000403 (_ANOMALY_BF526_BF527(< 1, < 2))
/* Lockbox SESR Disallows Certain User Interrupts */
#define ANOMALY_05000404 (_ANOMALY_BF526_BF527(< 1, < 2))
/* Lockbox SESR Firmware Does Not Save/Restore Full Context */
#define ANOMALY_05000405 (1)
/* Lockbox SESR Firmware Arguments Are Not Retained After First Initialization */
#define ANOMALY_05000407 (_ANOMALY_BF526_BF527(< 1, < 2))
/* Lockbox Firmware Memory Cleanup Routine Does not Clear Registers */
#define ANOMALY_05000408 (1)
/* Lockbox firmware leaves MDMA0 channel enabled */
#define ANOMALY_05000409 (_ANOMALY_BF526_BF527(< 1, < 2))
/* Incorrect Default Internal Voltage Regulator Setting */
#define ANOMALY_05000410 (_ANOMALY_BF527(< 2))
/* bfrom_SysControl() Firmware Function Cannot be Used to Enter Power Saving Modes */
#define ANOMALY_05000411 (_ANOMALY_BF526_BF527(< 1, < 2))
/* OTP_CHECK_FOR_PREV_WRITE Bit is Not Functional in bfrom_OtpWrite() API */
#define ANOMALY_05000414 (_ANOMALY_BF526_BF527(< 1, < 2))
/* DEB2_URGENT Bit Not Functional */
#define ANOMALY_05000415 (_ANOMALY_BF526_BF527(< 1, < 2))
/* Speculative Fetches Can Cause Undesired External FIFO Operations */
#define ANOMALY_05000416 (1)
/* SPORT0 Ignores External TSCLK0 on PG14 When TMR6 is an Output */
#define ANOMALY_05000417 (_ANOMALY_BF527(< 2))
/* PPI Timing Requirements tSFSPE and tHFSPE Do Not Meet Data Sheet Specifications */
#define ANOMALY_05000418 (_ANOMALY_BF526_BF527(< 1, < 2))
/* USB PLL_STABLE Bit May Not Accurately Reflect the USB PLL's Status */
#define ANOMALY_05000420 (_ANOMALY_BF526_BF527(< 1, < 2))
/* TWI Fall Time (Tof) May Violate the Minimum I2C Specification */
#define ANOMALY_05000421 (1)
/* TWI Input Capacitance (Ci) May Violate the Maximum I2C Specification */
#define ANOMALY_05000422 (_ANOMALY_BF526_BF527(> 0, > 1))
/* Certain Ethernet Frames With Errors are Misclassified in RMII Mode */
#define ANOMALY_05000423 (_ANOMALY_BF526_BF527(< 1, < 2))
/* Internal Voltage Regulator Not Trimmed */
#define ANOMALY_05000424 (_ANOMALY_BF527(< 2))
/* Multichannel SPORT Channel Misalignment Under Specific Configuration */
#define ANOMALY_05000425 (_ANOMALY_BF526_BF527(< 1, < 2))
/* Speculative Fetches of Indirect-Pointer Instructions Can Cause False Hardware Errors */
#define ANOMALY_05000426 (1)
/* WB_EDGE Bit in NFC_IRQSTAT Incorrectly Reflects Buffer Status Instead of IRQ Status */
#define ANOMALY_05000429 (_ANOMALY_BF526_BF527(< 1, < 2))
/* Software System Reset Corrupts PLL_LOCKCNT Register */
#define ANOMALY_05000430 (_ANOMALY_BF527(> 1))
/* Incorrect Use of Stack in Lockbox Firmware During Authentication */
#define ANOMALY_05000431 (1)
/* bfrom_SysControl() Does Not Clear SIC_IWR1 Before Executing PLL Programming Sequence */
#define ANOMALY_05000432 (_ANOMALY_BF526(< 1))
/* Certain SIC Registers are not Reset After Soft or Core Double Fault Reset */
#define ANOMALY_05000435 (_ANOMALY_BF526_BF527(< 1, >= 0))
/* Preboot Cannot be Used to Alter the PLL_DIV Register */
#define ANOMALY_05000439 (_ANOMALY_BF526_BF527(< 1, >= 0))
/* bfrom_SysControl() Cannot be Used to Write the PLL_DIV Register */
#define ANOMALY_05000440 (_ANOMALY_BF526_BF527(< 1, >= 0))
/* OTP Write Accesses Not Supported */
#define ANOMALY_05000442 (_ANOMALY_BF527(< 1))
/* IFLUSH Instruction at End of Hardware Loop Causes Infinite Stall */
#define ANOMALY_05000443 (1)
/* The WURESET Bit in the SYSCR Register is not Functional */
#define ANOMALY_05000445 (1)
/* USB DMA Mode 1 Short Packet Data Corruption */
#define ANOMALY_05000450 (1)
/* BCODE_QUICKBOOT, BCODE_ALLBOOT, and BCODE_FULLBOOT Settings in SYSCR Register Not Functional */
#define ANOMALY_05000451 (1)
/* Incorrect Default Hysteresis Setting for RESET, NMI, and BMODE Signals */
#define ANOMALY_05000452 (_ANOMALY_BF526_BF527(< 1, >= 0))
/* USB Receive Interrupt Is Not Generated in DMA Mode 1 */
#define ANOMALY_05000456 (1)
/* Host DMA Port Responds to Certain Bus Activity Without HOST_CE Assertion */
#define ANOMALY_05000457 (1)
/* USB DMA Mode 1 Failure When Multiple USB DMA Channels Are Concurrently Enabled */
#define ANOMALY_05000460 (1)
/* False Hardware Error when RETI Points to Invalid Memory */
#define ANOMALY_05000461 (1)
/* Synchronization Problem at Startup May Cause SPORT Transmit Channels to Misalign */
#define ANOMALY_05000462 (1)
/* USB Rx DMA hang */
#define ANOMALY_05000465 (1)
/* TxPktRdy Bit Not Set for Transmit Endpoint When Core and DMA Access USB Endpoint FIFOs Simultaneously */
#define ANOMALY_05000466 (1)
/* Possible RX data corruption when control & data EP FIFOs are accessed via the core */
#define ANOMALY_05000467 (1)
/* PLL Latches Incorrect Settings During Reset */
#define ANOMALY_05000469 (1)

/* Anomalies that don't exist on this proc */
#define ANOMALY_05000099 (0)
#define ANOMALY_05000120 (0)
#define ANOMALY_05000125 (0)
#define ANOMALY_05000149 (0)
#define ANOMALY_05000158 (0)
#define ANOMALY_05000171 (0)
#define ANOMALY_05000179 (0)
#define ANOMALY_05000182 (0)
#define ANOMALY_05000183 (0)
#define ANOMALY_05000189 (0)
#define ANOMALY_05000198 (0)
#define ANOMALY_05000202 (0)
#define ANOMALY_05000215 (0)
#define ANOMALY_05000220 (0)
#define ANOMALY_05000227 (0)
#define ANOMALY_05000230 (0)
#define ANOMALY_05000231 (0)
#define ANOMALY_05000233 (0)
#define ANOMALY_05000234 (0)
#define ANOMALY_05000242 (0)
#define ANOMALY_05000244 (0)
#define ANOMALY_05000248 (0)
#define ANOMALY_05000250 (0)
#define ANOMALY_05000257 (0)
#define ANOMALY_05000261 (0)
#define ANOMALY_05000263 (0)
#define ANOMALY_05000266 (0)
#define ANOMALY_05000273 (0)
#define ANOMALY_05000274 (0)
#define ANOMALY_05000278 (0)
#define ANOMALY_05000281 (0)
#define ANOMALY_05000283 (0)
#define ANOMALY_05000285 (0)
#define ANOMALY_05000287 (0)
#define ANOMALY_05000301 (0)
#define ANOMALY_05000305 (0)
#define ANOMALY_05000307 (0)
#define ANOMALY_05000311 (0)
#define ANOMALY_05000312 (0)
#define ANOMALY_05000315 (0)
#define ANOMALY_05000323 (0)
#define ANOMALY_05000362 (1)
#define ANOMALY_05000363 (0)
#define ANOMALY_05000400 (0)
#define ANOMALY_05000402 (0)
#define ANOMALY_05000412 (0)
#define ANOMALY_05000447 (0)
#define ANOMALY_05000448 (0)

#endif
