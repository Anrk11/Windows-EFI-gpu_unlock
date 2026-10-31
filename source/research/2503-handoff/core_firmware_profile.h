/* GPL-2.0-only. This owner's captured X99/2503 experiment. This is NOT a
 * reviewed upstream profile and does not admit other GA106 cards or firmware.
 * The standalone adapter additionally pins AMI image/code, topology and PROM. */
#pragma once
#if !defined(HT_EXECUTE_CORE) || !defined(HT_HEADLESS_WINDOWS_TEST) || !defined(HT_RESTORE_VGA_DECODE) || !defined(GU_UEFI_PORT) || !defined(GU_ENTRY_LATCHED_SS1_FULL)
#error The 2503 firmware profile requires its standalone full adapter
#endif
#if __STDC_HOSTED__ == 1 && !defined(GU_2503_ADAPTER_HOST_TEST)
#error Hosted 2503 adapter builds require an explicit simulation define
#endif
#if defined(GU_RESEARCH_2503_HOST) || defined(GU_PROFILE_LOCAL) || defined(GU_FULL_UNLOCK) || defined(GU_INTERNAL_BOOT) || defined(GU_HEADLESS_RESEARCH) || defined(GU_PROFILE_H610M_3060) || defined(GU_PROFILE_B650M_3090FE) || defined(GU_PROFILE_B660M_3070TI) || defined(GU_PROFILE_B660M_3090TI) || defined(GU_PROFILE_X870_3090TI) || defined(GU_PROFILE_B660PLUS_3060) || defined(GU_PROFILE_PROBE_H610M_3060)
#error The standalone 2503 experiment must not select another boot or board profile
#endif
#define GU_PROFILE_NAME "EXPERIMENTAL-OWNER-X99-XD3-GA106-2503"
#define GU_BOARD_ID 0x250310deU
#define GU_BOARD_SUBSYSTEM 0x250310deU
#define GU_TARGET_SEGMENT 0U
#define GU_TARGET_BUS 7U
#define GU_BAR0_BASE 0xfa000000ULL
#define GU_BAR0_END 0xfaffffffULL
#define GU_DEVICE_MEMORY_SIZE 0x3000U
#define GU_CHIP_ID 0xb76000a1U
#define GU_PROM_SHA256 "023fc329d52741164ccb1632206883b87a778af6f5f8532a1dd5426fa09a13c5"
#define GU_CONSOLE_AGGREGATE 0
#define GU_ROOT_PORT_DEVICE 3U
#define GU_ROOT_PORT_FUNCTION 0U
#define GU_RTX_PATH_BYTES 2,1,12,0,0xd0,0x41,3,10,0,0,0,0,1,1,6,0,0,3,1,1,6,0,0,0,2,3,8,0,0,1,1,128,127,255,4,0
