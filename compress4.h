//----------------------------------------------------------------------------------------------------------------------
// Copyright © 2021 by Brett Kuntz. All rights reserved.
//--NOV-26-2018---------------------------------------------------------------------------------------------------------
#include <assert.h>
#include <limits.h>
static_assert(CHAR_BIT == 8, "This code requires [char] to be exactly 8 bits.");
static_assert(sizeof(long) == 8, "This code requires [long] to be exactly 8 bytes."); // __builtin_popcountl
//----------------------------------------------------------------------------------------------------------------------
#include <stdint.h>
typedef   unsigned char        u8     ;   typedef   char         s8     ;
typedef   uint16_t             u16    ;   typedef   int16_t      s16    ;
typedef   uint32_t             u32    ;   typedef   int32_t      s32    ;
typedef   uint64_t             u64    ;   typedef   int64_t      s64    ;
typedef   __uint128_t          u128   ;   typedef   __int128_t   s128   ;
typedef   unsigned int         ui     ;   typedef   int          si     ;
typedef   unsigned long        ul     ;   typedef   long         sl     ;
typedef   unsigned long long   ull    ;   typedef   long long    sll    ;
typedef   float                r32    ;   typedef   double       r64    ;
//----------------------------------------------------------------------------------------------------------------------
#define halt do { fflush(0); while (1) sleep(-1); } while (0)
#define cwait do { fflush(0); sleep(1); do errno = 0, wait(0); while (errno != ECHILD); sleep(1); } while(0)
//----------------------------------------------------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <semaphore.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
//----------------------------------------------------------------------------------------------------------------------
static u64 * global_total, SAMPLES_PER_TEST;
static sem_t csoutput;
static const u64 BLAKE_IV = UINT64_C(0xA54FF53A5F1D36F1);
//----------------------------------------------------------------------------------------------------------------------
static void check(void);
static u64 find_hash(ui * const restrict, u8 * const restrict, u8 const * const restrict, u64 * const restrict, u64 * const restrict, u8 const * const restrict, const u64, const ui);
static u64 find_p_hash(ui * const restrict, u8 * const restrict, u8 const * const restrict, u64 * const restrict, u64 * const restrict, u8 const * const restrict, const u64, const u8, const ui);
static void hash(u8 * const restrict, u64 const * const restrict, u64 * const, u64 const * const restrict, u8 const * const restrict);
static void p_hash(u8 * const restrict, u64 const * const restrict, u64 * const, u64 const * const restrict, const u8, u8 const * const restrict);
static si get_hash_score(void const * const);
static void blake2b(u64 * const restrict, u64 const * const restrict);
static u64 tick(void);
//----------------------------------------------------------------------------------------------------------------------