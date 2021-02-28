//----------------------------------------------------------------------------------------------------------------------
// Copyright © 2021 by Brett Kuntz. All rights reserved.
//----------------------------------------------------------------------------------------------------------------------
#define _FILE_OFFSET_BITS 64
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
//----------------------------------------------------------------------------------------------------------------------
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <inttypes.h>
#include <sys/sysinfo.h>
//----------------------------------------------------------------------------------------------------------------------
#define FILE_SIZE 1048576
#define BLOCKS 8192
#define BLOCK_PRIME_MUL 83
#define CUTS_LENGTH 28
#define TWEAKS (CUTS_LENGTH + 2) // 28 chains, 1 full hash, 1 shuffle
#define TWEAK_BITS 20
#define TWEAK_SIZE ((TWEAKS * TWEAK_BITS * BLOCKS) / CHAR_BIT)
#define INVERT_SIZE ((2 * BLOCKS) / CHAR_BIT)
const u8 CHAIN_CUTS[CUTS_LENGTH] = { 37, 23, 17, 14, 11, 9, 8, 7, 6, 6, 5, 5, 5, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2 };
const u64 BLAKE_IV = UINT64_C(0xA54FF53A5F1D36F1);
u8 * indata, * outdata, * tweaks, * inverts, iv[16], global_iv[128];
u64 CS_NEXT_BLOCK_NUM, start_tick;
pthread_spinlock_t csjob, csmem;
//----------------------------------------------------------------------------------------------------------------------
u64 find_hash(u8 * const restrict, u8 const * const restrict, const u64, ui * const restrict);
void hash(u8 * const restrict, u8 const * const restrict, u64 const * const restrict, u64 const * const restrict);
u64 find_p_hash(u8 * const restrict, u8 const * const restrict, const u64, const u8);
void p_hash(u8 * const restrict, u8 const * const restrict, u64 const * const restrict, u64 const * const restrict, const u8);
si get_hash_score(u8 const * const restrict);
//----------------------------------------------------------------------------------------------------------------------
u64 find_shuffle(u8 * const restrict, u8 const * const restrict, const u64, ui * const restrict);
void shuffle(u8 * const restrict, u8 const * const restrict, u64 const * const restrict, u64 const * const restrict);
void ishuffle(u8 * const restrict, u8 const * const restrict, u64 const * const restrict, u64 const * const restrict);
s32 get_shuffle_score(u8 const * const restrict);
//----------------------------------------------------------------------------------------------------------------------
void * thread(void *);
void set_tweak(const u64, const ui, u64);
u64 get_tweak(const u64, const ui);
void set_bit(u8 * const restrict, const u32, const ui);
ui get_bit(u8 const * const restrict, const u32);
void expand_iv(void);
void blake2b(u64 * const restrict, u64 const * const restrict);
u64 tick(void);
//----------------------------------------------------------------------------------------------------------------------