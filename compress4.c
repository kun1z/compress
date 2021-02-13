//----------------------------------------------------------------------------------------------------------------------
// Copyright © 2021 by Brett Kuntz. All rights reserved.
//----------------------------------------------------------------------------------------------------------------------
#include "compress4.h"
#define CUTS_LENGTH 5
static ui CHAIN_CUTS[CUTS_LENGTH] = { 37, 23, 17, 14, 11 };
//----------------------------------------------------------------------------------------------------------------------
#include <windows.h>
si main(si argc, s8 ** argv)
{
    if (SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS))
    {
        printf("Low priority set\n");
    }

    if (sem_init(&csoutput, 1, 1) == -1)
    {
        printf("sem_init error\n");
        return EXIT_FAILURE;
    }

    if ((global_total = mmap(0, sizeof(u64), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0)) == MAP_FAILED)
    {
        printf("mmap error\n");
        return EXIT_FAILURE;
    }

    if (argc != 3)
    {
        printf("param error\n");
        return EXIT_FAILURE;
    }

    const ui CUTOFF = atol(argv[1]);
    const ul SAMPLES = atol(argv[2]);

    // Start
    printf("Starting\n");

    const ui threads = get_nprocs();

    SAMPLES_PER_TEST = round((r64)SAMPLES / threads);

    printf("CUTOFF we're testing: %u\n", CUTOFF);
    printf("SAMPLES: %lu\n", SAMPLES);
    printf("threads: %u\n", threads);
    printf("SAMPLES per thread: %lu\n", (ul)SAMPLES_PER_TEST);
    fflush(0);

    for (ui t=0;t<threads;t++)
    {
        if (!fork())
        {
            check();

            printf("Child Exiting\n");
            fflush(0);

            exit(EXIT_SUCCESS);
        }
    }

    cwait; // Wait on all children to exit

    if (*global_total != 0) // fixes rare fork() emulation bug
    {
        s8 buf[4096];
        const r64 avg_distance = (r64)*global_total / SAMPLES;
        sprintf(buf, "Test Params: 37, 23, 17, 14, 11, (NEW: %u)\n", CUTOFF);
        sprintf(&buf[strlen(buf)], "n=%lu\n", SAMPLES);
        sprintf(&buf[strlen(buf)], "Average Distance: %.8f\n\n", avg_distance);
        FILE * fout = fopen("output_results.txt", "ab");
        fputs(buf, fout);
        fclose(fout);
    }

    printf("Parent Exiting\n");

    return EXIT_SUCCESS;
}
//----------------------------------------------------------------------------------------------------------------------
static void check(void)
{
    FILE * frand = fopen("/dev/urandom", "rb");

    if (!frand)
    {
        printf("/dev/urandom error\n");
        exit(EXIT_FAILURE);
    }

    u64 total = 0;

    const u64 start = tick();

    for (u64 i=0;i<SAMPLES_PER_TEST;i++)
    {
        if (i & 1)
        {
            const r64 m = (tick() - start) / 60000.;
            const r64 pm = i / m;
            const u64 rem = SAMPLES_PER_TEST - i;
            printf("%.1f mins\n", rem / pm);
            fflush(0);
        }

        u8 input_iv[128], input_block[128], output_block[128];

        fread(   input_iv, 1, 128, frand);
        fread(input_block, 1, 128, frand);

        {
            ui distance;
            u64 v[16], m[16];
            const u64 sub_block = i * 64;

            find_hash(0, output_block, input_block, v, m, input_iv, sub_block, 20);

            for (ui j=0;j<CUTS_LENGTH;j++)
            {
                memcpy(input_block, output_block, 128);
                find_p_hash(0, output_block, input_block, v, m, input_iv, sub_block + 1 + j, CHAIN_CUTS[j], 20);
            }

            memcpy(input_block, output_block, 128);

            find_p_hash(&distance, output_block, input_block, v, m, input_iv, sub_block + CUTS_LENGTH + 1, CUTOFF, 20);
            //find_p_hash2(&distance, output_block, input_block, v, m, input_iv, sub_block + CUTS_LENGTH + 1, CUTOFF, 20);

            total += distance;
        }
    }

    fclose(frand);

    sem_wait(&csoutput);
    {
        *global_total += total;
    }
    sem_post(&csoutput);
}
//----------------------------------------------------------------------------------------------------------------------
static u64 find_p_hash2(ui * const restrict distance, u8 * const restrict output_block, u8 const * const restrict input_block, u64 * const restrict v, u64 * const restrict m, u8 const * const restrict input_iv, const u64 block_n, const ui cutoff, const ui limit)
{
    u64 best_n = 0;
    ui best_distance = 0;
    const u64 total_n = (u64)1 << (limit - 1);

    u64 RO_IV[16];
    memcpy(RO_IV, input_iv, 128);

    for (ui i=0;i<16;i++)
    {
        RO_IV[i] += BLAKE_IV * block_n;
    }

    memcpy(m, input_iv, 128);

    for (u64 n=0;n<total_n;n++)
    {
        u8 block[128];
        p_hash2(block, RO_IV, v, m, cutoff, input_block);

        const si distance = labs(get_hash_score(block));

        if (distance > best_distance)
        {
            best_n = n;
            best_distance = distance;
        }

        for (ui i=0;i<16;i++)
        {
            m[i] += BLAKE_IV;
        }
    }

    memcpy(m, input_iv, 128);

    for (ui i=0;i<16;i++)
    {
        m[i] += BLAKE_IV * best_n;
    }

    p_hash2(output_block, RO_IV, v, m, cutoff, input_block);

    si temp_distance = get_hash_score(output_block);

    if (temp_distance < 0)
    {
        for (ui i=0;i<128;i++)
        {
            output_block[i] = ~output_block[i];
        }

        temp_distance = -temp_distance;
    }

    if (temp_distance != best_distance)
    {
        printf("ERROR: temp_distance [%d] != best_distance [%u]\nHash confirmation failed!!\n", temp_distance, best_distance);
        fflush(0);
        exit(EXIT_FAILURE);
    }

    if (distance)
    {
        *distance = temp_distance;
    }

    return best_n;
}
//----------------------------------------------------------------------------------------------------------------------
static void p_hash2(u8 * const restrict block, u64 const * const restrict RO_IV, u64 * const v, u64 const * const restrict m, const ui cutoff, u8 const * const restrict input_block)
{
    memcpy(v, RO_IV, 128);

    blake2b(v, m);

    ui p = -1;
    memset(block, 0, 128);

    for (ui i=0;i<cutoff;i++)
    {
        if (++p == 64)
        {
            p = 0;
            blake2b(v, m);
        }

        u16 word;
        {
            u8 const * const restrict vp = (u8 *)v;
            memcpy(&word, &vp[p * 2], 2);
        }

        const ui random_byte = word & 127;
        const ui random_bit = (word >> 7) & 7;

        block[random_byte] |= 1 << random_bit;
    }

    for (ui i=0;i<128;i++)
    {
        block[i] ^= input_block[i];
    }
}
//----------------------------------------------------------------------------------------------------------------------
static u64 find_p_hash(ui * const restrict distance, u8 * const restrict output_block, u8 const * const restrict input_block, u64 * const restrict v, u64 * const restrict m, u8 const * const restrict input_iv, const u64 block_n, const u8 cutoff, const ui limit)
{
    u64 best_n = 0;
    ui best_distance = 0;
    const u64 total_n = (u64)1 << (limit - 1);

    u64 RO_IV[16];
    memcpy(RO_IV, input_iv, 128);

    for (ui i=0;i<16;i++)
    {
        RO_IV[i] += BLAKE_IV * block_n;
    }

    memcpy(m, input_iv, 128);

    for (u64 n=0;n<total_n;n++)
    {
        u8 block[128];
        p_hash(block, RO_IV, v, m, cutoff, input_block);

        const si distance = labs(get_hash_score(block));

        if (distance > best_distance)
        {
            best_n = n;
            best_distance = distance;
        }

        for (ui i=0;i<16;i++)
        {
            m[i] += BLAKE_IV;
        }
    }

    memcpy(m, input_iv, 128);

    for (ui i=0;i<16;i++)
    {
        m[i] += BLAKE_IV * best_n;
    }

    p_hash(output_block, RO_IV, v, m, cutoff, input_block);

    si temp_distance = get_hash_score(output_block);

    if (temp_distance < 0)
    {
        for (ui i=0;i<128;i++)
        {
            output_block[i] = ~output_block[i];
        }

        temp_distance = -temp_distance;
    }

    if (temp_distance != best_distance)
    {
        printf("ERROR: temp_distance [%d] != best_distance [%u]\nHash confirmation failed!!\n", temp_distance, best_distance);
        fflush(0);
        exit(EXIT_FAILURE);
    }

    if (distance)
    {
        *distance = temp_distance;
    }

    return best_n;
}
//----------------------------------------------------------------------------------------------------------------------
static void p_hash(u8 * const restrict block, u64 const * const restrict RO_IV, u64 * const v, u64 const * const restrict m, const u8 cutoff, u8 const * const restrict input_block)
{
    memcpy(v, RO_IV, 128);

    blake2b(v, m);

    u8 const * vp = (u8 *)v;
    u8 const * const vl = &vp[128];

    for (ui i=0;i<128;i++)
    {
        if (vp == vl)
        {
            vp = (u8 *)v;
            blake2b(v, m);
        }

        u8 byte = 0;

        for (u8 b=1;b;b<<=1,vp++)
        {
            if (*vp < cutoff)
            {
                byte |= b;
            }
        }

        block[i] = byte ^ input_block[i];
    }
}
//----------------------------------------------------------------------------------------------------------------------
static u64 find_hash(ui * const restrict distance, u8 * const restrict output_block, u8 const * const restrict input_block, u64 * const restrict v, u64 * const restrict m, u8 const * const restrict input_iv, const u64 block_n, const ui limit)
{
    u64 best_n = 0;
    ui best_distance = 0;
    const u64 total_n = (u64)1 << (limit - 1);

    u64 RO_IV[16];
    memcpy(RO_IV, input_iv, 128);

    for (ui i=0;i<16;i++)
    {
        RO_IV[i] += BLAKE_IV * block_n;
    }

    memcpy(m, input_iv, 128);

    for (u64 n=0;n<total_n;n++)
    {
        u8 block[128];
        hash(block, RO_IV, v, m, input_block);

        const si distance = labs(get_hash_score(block));

        if (distance > best_distance)
        {
            best_n = n;
            best_distance = distance;
        }

        for (ui i=0;i<16;i++)
        {
            m[i] += BLAKE_IV;
        }
    }

    memcpy(m, input_iv, 128);

    for (ui i=0;i<16;i++)
    {
        m[i] += BLAKE_IV * best_n;
    }

    hash(output_block, RO_IV, v, m, input_block);

    si temp_distance = get_hash_score(output_block);

    if (temp_distance < 0)
    {
        for (ui i=0;i<128;i++)
        {
            output_block[i] = ~output_block[i];
        }

        temp_distance = -temp_distance;
    }

    if (temp_distance != best_distance)
    {
        printf("ERROR: temp_distance [%d] != best_distance [%u]\nHash confirmation failed!!\n", temp_distance, best_distance);
        fflush(0);
        exit(EXIT_FAILURE);
    }

    if (distance)
    {
        *distance = temp_distance;
    }

    return best_n;
}
//----------------------------------------------------------------------------------------------------------------------
static void hash(u8 * const restrict block, u64 const * const restrict RO_IV, u64 * const v, u64 const * const restrict m, u8 const * const restrict input_block)
{
    u8 const * const restrict vp = (u8 *)v;

    memcpy(v, RO_IV, 128);

    blake2b(v, m);

    for (ui i=0;i<128;i++)
    {
        block[i] = vp[i] ^ input_block[i];
    }
}
//----------------------------------------------------------------------------------------------------------------------
static si get_hash_score(void const * const block)
{
    u8 const * const restrict vp = (u8 *)block;

    si population = 0;

    for (ui i=0;i<16;i++)
    {
        u64 temp;
        memcpy(&temp, &vp[i * 8], 8);
        population += __builtin_popcountl(temp);
    }

    return 512 - population;
}
//----------------------------------------------------------------------------------------------------------------------
static void blake2b(u64 * const restrict v, u64 const * const restrict m)
{
    #define G(x, y, a, b, c, d)                 \
    do {                                        \
        a = a + b + m[x];                       \
        d = ((d ^ a) >> 32) | ((d ^ a) << 32);  \
        c = c + d;                              \
        b = ((b ^ c) >> 24) | ((b ^ c) << 40);  \
        a = a + b + m[y];                       \
        d = ((d ^ a) >> 16) | ((d ^ a) << 48);  \
        c = c + d;                              \
        b = ((b ^ c) >> 63) | ((b ^ c) << 1);   \
    } while (0)

    G(13, 11, v[ 0], v[ 4], v[ 8], v[12]);
    G( 7, 14, v[ 1], v[ 5], v[ 9], v[13]);
    G(12,  1, v[ 2], v[ 6], v[10], v[14]);
    G( 3,  9, v[ 3], v[ 7], v[11], v[15]);
    G( 5,  0, v[ 0], v[ 5], v[10], v[15]);
    G(15,  4, v[ 1], v[ 6], v[11], v[12]);
    G( 8,  6, v[ 2], v[ 7], v[ 8], v[13]);
    G( 2, 10, v[ 3], v[ 4], v[ 9], v[14]);

    #undef G
}
//----------------------------------------------------------------------------------------------------------------------
static u64 tick(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return ((u64)now.tv_sec * 1000) + ((u64)now.tv_nsec / 1000000);
}
//----------------------------------------------------------------------------------------------------------------------