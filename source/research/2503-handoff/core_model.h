/* GPL-2.0-only. Captured 2503 inputs for RAM-only real-core simulation.
 * This is deliberately NOT an admitted hardware or firmware profile. */
#pragma once
#if !defined(GU_RESEARCH_2503_HOST) || __STDC_HOSTED__ != 1
#error The 2503 core model is forbidden in firmware builds
#endif
#if defined(GU_PROFILE_LOCAL) || defined(GU_FULL_UNLOCK) || defined(GU_INTERNAL_BOOT) || defined(GU_HEADLESS_RESEARCH)
#error The host model must not select a production boot/release policy
#endif
#define GU_PROFILE_NAME "HOST-ONLY-X99-XD3-GA106-2503"
#define GU_BOARD_ID 0x250310deU
#define GU_BOARD_SUBSYSTEM 0x250310deU
#define GU_TARGET_SEGMENT 0U
#define GU_TARGET_BUS 7U
#define GU_BAR0_BASE 0xfa000000ULL
#define GU_BAR0_END 0xfaffffffULL
#define GU_DEVICE_MEMORY_SIZE 0x3000U
#define GU_CHIP_ID 0xb76000a1U
#define GU_CONSOLE_AGGREGATE 0
#define GU_ROOT_PORT_DEVICE 3U
#define GU_ROOT_PORT_FUNCTION 0U
#define GU_RTX_PATH_BYTES 2,1,12,0,0xd0,0x41,3,10,0,0,0,0,1,1,6,0,0,3,1,1,6,0,0,0,2,3,8,0,0,1,1,128,127,255,4,0
#define GU_MODEL_FRTS 0x2ffee0000ULL
