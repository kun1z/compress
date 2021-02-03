//----------------------------------------------------------------------------------------------------------------------
// Copyright © 2021 by Brett Kuntz. All rights reserved.
//----------------------------------------------------------------------------------------------------------------------
#include "compress.h"
#define CUTS_LENGTH 15
static ui CHAIN_CUTS[CUTS_LENGTH] = { 37, 23, 17, 14, 11, 9, 8, 7, 6, 6, 5, 5, 5, 4, -1 };
//----------------------------------------------------------------------------------------------------------------------
//#include <windows.h>
si main(si argc, s8 ** argv)
{
    /*if (SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS))
    {
        printf("Low priority set\n");
    }*/

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

    if (argc != 3) return EXIT_FAILURE;

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

    CHAIN_CUTS[CUTS_LENGTH - 1] = CUTOFF;

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
        s8 buf[2048];
        const r64 avg_distance = (r64)*global_total / SAMPLES;
        sprintf(buf, "Test Params: ");
        for (ui i=0;i<CUTS_LENGTH;i++)
        {
            sprintf(&buf[strlen(buf)], "%u, ", CHAIN_CUTS[i]);
        }
        buf[strlen(buf) - 2] = 0;
        sprintf(&buf[strlen(buf)], "\nn=%lu\n", SAMPLES);
        sprintf(&buf[strlen(buf)], "Average Distance: %.4f\n\n", avg_distance);
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
            const u64 sub_block = i * 32;

            find_hash(0, output_block, input_block, v, m, input_iv, sub_block, 20);

            for (ui j=0;j<CUTS_LENGTH;j++)
            {
                memcpy(input_block, output_block, 128);
                find_p_hash(&distance, output_block, input_block, v, m, input_iv, sub_block + 1 + j, CHAIN_CUTS[j], 20);
            }

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
static u64 find_hash2(ui * const restrict distance, u8 * const restrict output_block, u8 const * const restrict input_block, u64 * const restrict v, u64 * const restrict m, u8 const * const restrict input_iv, const u64 block_n, const r64 lbit_p, const r64 rbit_p, const ui limit)
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
        hash2(block, RO_IV, v, m, lbit_p, rbit_p, input_block);

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

    hash2(output_block, RO_IV, v, m, lbit_p, rbit_p, input_block);

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
static void hash2(u8 * const restrict block, u64 const * const restrict RO_IV, u64 * const v, u64 const * const restrict m, const r64 lbit_p, const r64 rbit_p, u8 const * const restrict input_block)
{
    u8 const * const restrict vp = (u8 *)v;

    memcpy(v, RO_IV, 128);

    blake2b(v, m);

    ui p = 0;
    const r64 unit = (lbit_p - rbit_p) / 127.;

    for (ui i=0;i<128;i++)
    {
        const r64 temp_hash_p = lbit_p - (i * unit);
        const u16 cutoff = round(65536 * temp_hash_p);

        if (p == 64)
        {
            p = 0;
            blake2b(v, m);
        }

        u8 byte = 0;

        for (ui b=0;b<8;b++)
        {
            u16 n;
            memcpy(&n, &vp[p * 2], 2);

            p++;
            byte <<= 1;

            if (n < cutoff)
            {
                byte |= 1;
            }
        }

        block[i] = byte ^ input_block[i];
    }
}
//----------------------------------------------------------------------------------------------------------------------
static void shuffle(u8 * const restrict block, u64 const * const restrict RO_IV, u64 * const restrict v, u64 const * const restrict m, u8 const * const restrict input_block)
{
    memcpy(v, RO_IV, 128);

    blake2b(v, m);

    memcpy(block, input_block, 128);

    ui p = 0;

    for (ui i=1023;i;i--)
    {
        const ui j = rng(v, m, i, &p);

        const ui bi = get_bit(block, i);
        const ui bj = get_bit(block, j);

        set_bit(block, i, bj);
        set_bit(block, j, bi);
    }
}
//----------------------------------------------------------------------------------------------------------------------
static u64 find_shuffle(u32 * const restrict score, u8 * const restrict output_block, u8 const * const restrict input_block, u64 * const restrict v, u64 * const restrict m, u8 const * const restrict input_iv, const u64 block_n, const ui limit)
{
    u64 RO_IV[16];
    memcpy(RO_IV, input_iv, 128);

    for (ui i=0;i<16;i++)
    {
        RO_IV[i] += BLAKE_IV * block_n;
    }

    u64 best_n = 0;
    u32 best_score = 0;
    const u64 total_n = (u64)1 << (limit - 1);

    memcpy(m, input_iv, 128);

    for (u64 n=0;n<total_n;n++)
    {
        u8 block[128];
        shuffle(block, RO_IV, v, m, input_block);

        const u32 score = labs(get_shuffle_score(block));

        if (score > best_score)
        {
            best_score = score;
            best_n = n;
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

    shuffle(output_block, RO_IV, v, m, input_block);

    s32 temp_score = get_shuffle_score(output_block);

    if (temp_score < 0)
    {
        for (ui i=0;i<512;i++)
        {
            const ui bi = get_bit(output_block, i);
            const ui bj = get_bit(output_block, 1023 - i);

            set_bit(output_block, i, bj);
            set_bit(output_block, 1023 - i, bi);
        }

        temp_score = -temp_score;
    }

    if (temp_score != best_score)
    {
        printf("ERROR: temp_score [%"PRIi32"] != best_score [%"PRIu32"]\nShuffle confirmation failed!!\n", temp_score, best_score);
        fflush(0);
        exit(EXIT_FAILURE);
    }

    if (score)
    {
        *score = temp_score;
    }

    return best_n;
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
static s32 get_shuffle_score(void const * const restrict block)
{
    s32 score = 0, mscore = 0;

    for (ui i=0;i<1024;i++)
    {
        if (!get_bit(block, i))
        {
            score += i;
        }

        if (!get_bit(block, 1023 - i))
        {
            mscore += i;
        }
    }

    return score > mscore ? score : -mscore ;
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
static u16 rng(u64 * const restrict v, u64 const * const restrict m, const u16 n, ui * const restrict p)
{
    u16 mask = n - 1;

    mask |= mask >> 1;
    mask |= mask >> 2;
    mask |= mask >> 4;
    mask |= mask >> 8;

    mask |= mask + 1;

    u16 r;
    do r = rng_word(v, m, p) & mask;
    while (r > n);

    return r;
}
//----------------------------------------------------------------------------------------------------------------------
static u16 rng_word(u64 * const v, u64 const * const restrict m, ui * const restrict p)
{
    u8 const * const restrict vp = (u8 *)v;

    if (*p == 64)
    {
        *p = 0;
        blake2b(v, m);
    }

    u16 temp;
    memcpy(&temp, &vp[(*p)++ * 2], 2);
    return temp;
}
//----------------------------------------------------------------------------------------------------------------------
static ui get_bit(void const * const restrict stream, const ui address)
{
    return (((u8 *)stream)[address / CHAR_BIT] >> ((CHAR_BIT - 1) - (address % CHAR_BIT))) & 1;
}
//----------------------------------------------------------------------------------------------------------------------
static void set_bit(void * const restrict stream, const ui address, const ui bit)
{
    if (bit) ((u8 *)stream)[address / CHAR_BIT] |= (1 << ((CHAR_BIT - 1) - (address % CHAR_BIT)));
    else ((u8 *)stream)[address / CHAR_BIT] &= ~(1 << ((CHAR_BIT - 1) - (address % CHAR_BIT)));
}
//----------------------------------------------------------------------------------------------------------------------
static void print_bytes(s8 const * const restrict label, void const * const p, const ui l)
{
    printf("%s: ", label);
    for (ui i=0;i<l;i++)
    {
        u8 const * const restrict s = (u8 *)p;
        printf("%02X", s[i]);
    }
    printf("\n");
}
//----------------------------------------------------------------------------------------------------------------------
static void print_population(s8 const * const restrict label, void const * const p)
{
    u8 const * const restrict s = (u8 *)p;
    ui pop = 0;
    printf("%s: [ ", label);
    for (ui i=0;i<8;i++)
    {
        u64 temp1, temp2;
        memcpy(&temp1, &s[(i * 16) + 0], 8);
        memcpy(&temp2, &s[(i * 16) + 8], 8);
        const ui temp = __builtin_popcountl(temp1) + __builtin_popcountl(temp2);
        printf("%u ", temp);
        pop += temp;

    }
    printf("] %u %.2f%% %lu\n", pop, (pop * 100.) / 1024, labs(get_shuffle_score(p)));
}
//----------------------------------------------------------------------------------------------------------------------
static void gen_test_file(s8 const * const restrict filename, const r64 p)
{
    u64 v[16], m[16];

    FILE * frand = fopen("/dev/urandom", "rb");

    fread(v, sizeof(u64), 16, frand);
    fread(m, sizeof(u64), 16, frand);

    fclose(frand);

    const u16 cutoff = round(p * 65536);

    FILE * f = fopen(filename, "wb");

    for (u64 x=0;x<131072;x++)
    {
        u64 out = 0;

        for (u64 i=1;i;i<<=1)
        {
            blake2b(v, m);
            memcpy(m, v, 64);
            blake2b(v, m);

            u16 n;
            memcpy(&n, v, sizeof(u16));

            if (n < cutoff)
            {
                out |= i;
            }
        }

        fwrite(&out, sizeof(u64), 1, f);
    }

    fclose(f);
}
//----------------------------------------------------------------------------------------------------------------------