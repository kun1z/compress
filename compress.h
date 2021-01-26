//----------------------------------------------------------------------------------------------------------------------
// Copyright © 2021 by Brett Kuntz. All rights reserved.
//----------------------------------------------------------------------------------------------------------------------
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
// 1st cut is 128   (p0.50)
// 2nd cut is 37    (p0.14453125)
// 3rd cut is 23    (p0.08984375)
// 4th cut is 17    (p0.06640625)
// 5th cut is 14    (p0.05468750)
// 6th cut is 11    (p0.04296875)
// 7th cut is 9     (p0.03515625)
// 8th cut is ?? 8 ??
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
void check(const u8, const u8, const u8, const u8, const u8, const u8, const u8);
u64 find_hash(ui * const restrict, u8 * const restrict, u8 const * const restrict, u64 * const restrict, u64 * const restrict, u8 const * const restrict, const u64, const ui);
u64 find_hash2(ui * const restrict, u8 * const restrict, u8 const * const restrict, u64 * const restrict, u64 * const restrict, u8 const * const restrict, const u64, const r64, const r64, const ui);
u64 find_p_hash(ui * const restrict, u8 * const restrict, u8 const * const restrict, u64 * const restrict, u64 * const restrict, u8 const * const restrict, const u64, const u8, const ui);
u64 find_shuffle(u32 * const restrict, u8 * const restrict, u8 const * const restrict, u64 * const restrict, u64 * const restrict, u8 const * const restrict, const u64, const ui);
void hash(u8 * const restrict, u64 const * const restrict, u64 * const, u64 const * const restrict, u8 const * const restrict);
void hash2(u8 * const restrict, u64 const * const restrict, u64 * const, u64 const * const restrict, const r64, const r64, u8 const * const restrict);
void p_hash(u8 * const restrict, u64 const * const restrict, u64 * const, u64 const * const restrict, const u8, u8 const * const restrict);
void shuffle(u8 * const restrict, u64 const * const restrict, u64 * const restrict, u64 const * const restrict, u8 const * const restrict);
si get_hash_score(void const * const);
void blake2b(u64 * const restrict, u64 const * const restrict);
u64 tick(void);
void set_bit(void * const restrict, const ui, const ui);
ui get_bit(void const * const restrict, const ui);
u16 rng_word(u64 * const, u64 const * const restrict, ui * const restrict);
u16 rng(u64 * const restrict, u64 const * const restrict, const u16, ui * const restrict);
s32 get_shuffle_score(void const * const restrict);
void print_bytes(s8 const * const restrict, void const * const, const ui);
void print_population(s8 const * const restrict, void const * const);
//----------------------------------------------------------------------------------------------------------------------