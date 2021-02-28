//----------------------------------------------------------------------------------------------------------------------
// Copyright © 2021 by Brett Kuntz. All rights reserved.
//----------------------------------------------------------------------------------------------------------------------
#include "shared.h"
//----------------------------------------------------------------------------------------------------------------------
si main(si argc, s8 ** argv)
{
    // Command line

    if (argc != 4)
    {
        puts("param error");
        return EXIT_FAILURE;
    }

    indata = calloc(FILE_SIZE, 1);
    outdata = calloc(FILE_SIZE, 1);
    tweaks = calloc(TWEAK_SIZE, 1);
    inverts = calloc(INVERT_SIZE, 1);

    FILE * finput = fopen(argv[1], "rb");
    if (finput)
    {
        fread(indata, 1, FILE_SIZE, finput);
        fclose(finput);
        printf("Opened [%s] for input\n", argv[1]);
    }
    else return EXIT_FAILURE;

    FILE * fiv = fopen(argv[2], "rb");
    if (fiv)
    {
        fread(iv, 1, 16, fiv);
        fclose(fiv);
        printf("Opened [%s] for iv\n", argv[2]);
    }
    else return EXIT_FAILURE;

    FILE * foutput = fopen(argv[3], "wb");
    if (foutput)
    {
        printf("Opened [%s] for output\n", argv[3]);
    }
    else return EXIT_FAILURE;

    // Start

    puts("Starting compression...");

    pthread_spin_init(&csjob, PTHREAD_PROCESS_PRIVATE);
    pthread_spin_init(&csmem, PTHREAD_PROCESS_PRIVATE);

    expand_iv();

    const ui threads = get_nprocs();
    pthread_t ht[threads];

    start_tick = tick();

    for (ui i=0;i<threads;i++)
    {
        pthread_create(&ht[i], 0, thread, 0);
    }

    for (ui i=0;i<threads;i++)
    {
        pthread_join(ht[i], 0);
    }

    // Transpose

    puts("Transposing");
    memcpy(indata, outdata, FILE_SIZE);

    for (u64 i=0;i<128;i++)
    {
        for (u64 b=0;b<BLOCKS;b++)
        {
            outdata[(i * BLOCKS) + b] = indata[(b * 128) + i];
        }
    }

    printf("Saving [%s]\n", argv[3]);
    fwrite(outdata, 1, FILE_SIZE, foutput);
    fclose(foutput);

    // Temp save tweaks to file

    FILE * ftweaks = fopen("tweaks.bin", "wb");
    if (ftweaks)
    {
        fwrite(tweaks, 1, TWEAK_SIZE, ftweaks);
        fclose(ftweaks);
        puts("Saving [tweaks.bin]");
    }
    else return EXIT_FAILURE;

    // Temp save inverts to file

    FILE * finverts = fopen("inverts.bin", "wb");
    if (finverts)
    {
        fwrite(inverts, 1, INVERT_SIZE, finverts);
        fclose(finverts);
        puts("Saving [inverts.bin]");
    }
    else return EXIT_FAILURE;

    puts("Done :)\n");

    return EXIT_SUCCESS;
}
//----------------------------------------------------------------------------------------------------------------------
void * thread(void * UNUSED)
{
    while (1)
    {
        // Check for and possibly grab a new job

        u64 block_num = -1;
        pthread_spin_lock(&csjob);
        if (CS_NEXT_BLOCK_NUM < BLOCKS)
        {
            block_num = CS_NEXT_BLOCK_NUM++;
        }
        pthread_spin_unlock(&csjob);
        if (block_num == -1) break;

        // Do some work

        printf("compressing block %04"PRIu64"...\n", block_num);
        fflush(0);

        u64 tweak;
        ui invert;
        u8 input_block[128], output_block[128];

        const u64 sub_block = block_num * BLOCK_PRIME_MUL;

        memcpy(input_block, &indata[block_num * 128], 128);

        // Find the first collision

        tweak = find_hash(output_block, input_block, sub_block, &invert);

        pthread_spin_lock(&csmem);
        {
            set_tweak(block_num, 0, tweak);
            set_bit(inverts, block_num * 2, invert);
        }
        pthread_spin_unlock(&csmem);

        memcpy(input_block, output_block, 128);

        // Find all subsequent collisions

        for (ui i=0;i<CUTS_LENGTH;i++)
        {
            tweak = find_p_hash(output_block, input_block, sub_block + i + 1, CHAIN_CUTS[i]);

            pthread_spin_lock(&csmem);
            {
                set_tweak(block_num, i + 1, tweak);
            }
            pthread_spin_unlock(&csmem);

            memcpy(input_block, output_block, 128);
        }

        // Shuffle the resulting block

        tweak = find_shuffle(output_block, input_block, sub_block + (TWEAKS - 1), &invert);

        pthread_spin_lock(&csmem);
        {
            set_tweak(block_num, (TWEAKS - 1), tweak);
            set_bit(inverts, (block_num * 2) + 1, invert);
        }
        pthread_spin_unlock(&csmem);

        memcpy(&outdata[block_num * 128], output_block, 128);

        // Progress report

        const r64 ms = (tick() - start_tick) / 60000.;
        const r64 pm = (block_num + 1) / ms;
        const u64 rem = BLOCKS - (block_num + 1);
        printf("compressed block %04"PRIu64" - %.1f mins remain\n", block_num, rem / pm);
        fflush(0);
    }

    return 0;
}
//----------------------------------------------------------------------------------------------------------------------
u64 find_hash(u8 * const restrict output_block, u8 const * const restrict input_block, const u64 block_n, ui * const restrict invert)
{
    u64 tweak = 0;
    *invert = 0;
    ui best_distance = 0;
    const u64 total_n = (u64)1 << (TWEAK_BITS - 1);

    u64 RO_IV[16];
    memcpy(RO_IV, global_iv, 128);

    for (ui i=0;i<16;i++)
    {
        RO_IV[i] += BLAKE_IV * block_n;
    }

    u64 m[16];
    memcpy(m, global_iv, 128);

    // Find the best hash collision

    for (u64 n=0;n<total_n;n++)
    {
        hash(output_block, input_block, RO_IV, m);

        const si dist = labs(get_hash_score(output_block));

        if (dist > best_distance)
        {
            tweak = n;
            best_distance = dist;
        }

        for (ui i=0;i<16;i++)
        {
            m[i] += BLAKE_IV;
        }
    }

    // Confirm the hash collision

    memcpy(m, global_iv, 128);

    for (ui i=0;i<16;i++)
    {
        m[i] += BLAKE_IV * tweak;
    }

    hash(output_block, input_block, RO_IV, m);

    si temp_distance = get_hash_score(output_block);

    // Check if this hash needs to be inverted during decompression

    if (temp_distance < 0)
    {
        for (ui i=0;i<128;i++)
        {
            output_block[i] = ~output_block[i];
        }

        temp_distance = -temp_distance;
        *invert = 1;
    }

    // Temporary sanity check

    if (temp_distance != best_distance)
    {
        printf("ERROR: temp_distance [%d] != best_distance [%u]\nHash confirmation failed!!\n", temp_distance, best_distance);
        exit(EXIT_FAILURE);
    }

    return tweak;
}
//----------------------------------------------------------------------------------------------------------------------
void hash(u8 * const restrict output_block, u8 const * const restrict input_block, u64 const * const restrict RO_IV, u64 const * const restrict m)
{
    u64 v[16];
    memcpy(v, RO_IV, 128); // A copy is needed because the IV is Read-Only

    blake2b(v, m);

    for (ui i=0;i<128;i++)
    {
        u8 const * const restrict vp = (u8 *)v;
        output_block[i] = vp[i] ^ input_block[i];
    }
}
//----------------------------------------------------------------------------------------------------------------------
u64 find_p_hash(u8 * const restrict output_block, u8 const * const restrict input_block, const u64 block_n, const u8 cutoff)
{
    u64 tweak = 0;
    ui best_distance = 0;
    const u64 total_n = (u64)1 << (TWEAK_BITS - 1);

    u64 RO_IV[16];
    memcpy(RO_IV, global_iv, 128);

    for (ui i=0;i<16;i++)
    {
        RO_IV[i] += BLAKE_IV * block_n;
    }

    u64 m[16];
    memcpy(m, global_iv, 128);

    // Find the best hash collision

    for (u64 n=0;n<total_n;n++)
    {
        p_hash(output_block, input_block, RO_IV, m, cutoff);

        const si dist = labs(get_hash_score(output_block));

        if (dist > best_distance)
        {
            tweak = n;
            best_distance = dist;
        }

        for (ui i=0;i<16;i++)
        {
            m[i] += BLAKE_IV;
        }
    }

    // Confirm the hash collision

    memcpy(m, global_iv, 128);

    for (ui i=0;i<16;i++)
    {
        m[i] += BLAKE_IV * tweak;
    }

    p_hash(output_block, input_block, RO_IV, m, cutoff);

    const si temp_distance = get_hash_score(output_block);

    // Temporary sanity check

    if (temp_distance != best_distance)
    {
        printf("ERROR: temp_distance [%d] != best_distance [%u]\nHash confirmation failed!!\n", temp_distance, best_distance);
        exit(EXIT_FAILURE);
    }

    return tweak;
}
//----------------------------------------------------------------------------------------------------------------------
void p_hash(u8 * const restrict output_block, u8 const * const restrict input_block, u64 const * const restrict RO_IV, u64 const * const restrict m, const u8 cutoff)
{
    u64 v[16];
    memcpy(v, RO_IV, 128); // A copy is needed because the IV is Read-Only

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

        output_block[i] = byte ^ input_block[i];
    }
}
//----------------------------------------------------------------------------------------------------------------------
si get_hash_score(u8 const * const restrict block)
{
    si population = 0;

    for (ui i=0;i<16;i++)
    {
        u64 temp;
        memcpy(&temp, &block[i * 8], 8);
        population += __builtin_popcountl(temp);
    }

    return 512 - population;
}
//----------------------------------------------------------------------------------------------------------------------
u64 find_shuffle(u8 * const restrict output_block, u8 const * const restrict input_block, const u64 block_n, ui * const restrict invert)
{
    u64 tweak = 0;
    *invert = 0;
    u32 best_score = 0;
    const u64 total_n = (u64)1 << (TWEAK_BITS - 1);

    u64 RO_IV[16];
    memcpy(RO_IV, global_iv, 128);

    for (ui i=0;i<16;i++)
    {
        RO_IV[i] += BLAKE_IV * block_n;
    }

    u64 m[16];
    memcpy(m, global_iv, 128);

    // Find the best bit shuffle

    for (u64 n=0;n<total_n;n++)
    {
        shuffle(output_block, input_block, RO_IV, m);

        const u32 scr = labs(get_shuffle_score(output_block));

        if (scr > best_score)
        {
            best_score = scr;
            tweak = n;
        }

        for (ui i=0;i<16;i++)
        {
            m[i] += BLAKE_IV;
        }
    }

    // Confirm the bit shuffle

    memcpy(m, global_iv, 128);

    for (ui i=0;i<16;i++)
    {
        m[i] += BLAKE_IV * tweak;
    }

    shuffle(output_block, input_block, RO_IV, m);

    s32 temp_score = get_shuffle_score(output_block);

    // Check if this shuffle needs to be mirrored during decompression

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
        *invert = 1;
    }

    // Temporary sanity check

    if (temp_score != best_score)
    {
        printf("ERROR: temp_score [%"PRIi32"] != best_score [%"PRIu32"]\nShuffle confirmation failed!!\n", temp_score, best_score);
        exit(EXIT_FAILURE);
    }

    return tweak;
}
//----------------------------------------------------------------------------------------------------------------------
void shuffle(u8 * const restrict output_block, u8 const * const restrict input_block, u64 const * const restrict RO_IV, u64 const * const restrict m)
{
    u64 v[16];
    memcpy(v, RO_IV, 128); // A copy is needed because the IV is Read-Only

    blake2b(v, m);

    memcpy(output_block, input_block, 128);

    ui i = 1023;

    while (1)
    {
        u64 * const restrict p = &v[i & 15];

        const ui j = *p % (i + 1);

        const ui bi = get_bit(output_block, i);
        const ui bj = get_bit(output_block, j);

        set_bit(output_block, i, bj);
        set_bit(output_block, j, bi);

        if (i == 1) return;

        i--;

        *p ^= *p << 13;
        *p ^= *p >> 7;
        *p ^= *p << 17;
    }
}
//----------------------------------------------------------------------------------------------------------------------
s32 get_shuffle_score(u8 const * const restrict block)
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
void expand_iv(void)
{
    const u64 IV[8] =
    {
        0x6A09E667F3BCC908, 0xBB67AE8584CAA73B, 0x3C6EF372FE94F82B, 0xA54FF53A5F1D36F1,
        0x510E527FADE682D1, 0x9B05688C2B3E6C1F, 0x1F83D9ABFB41BD6B, 0x5BE0CD19137E2179,
    };

    u64 v[16], m[16];

    memcpy(&v[0], iv, 16); // 16-byte 'iv' is loaded from file in main()
    memcpy(&v[2], iv, 16);
    memcpy(&v[4], iv, 16);
    memcpy(&v[6], iv, 16);

    memcpy(&v[8], IV, 64);
    memcpy(&m[0], IV, 64);
    memcpy(&m[8], IV, 64);

    // Cheeseball way of expanding an IV from 16 to 128 bytes

    for (ui i=0;i<128;i++)
    {
        v[i & 15] ^= m[i & 15];
        blake2b(v, m);
    }

    for (ui i=0;i<128;i++)
    {
        u8 * const restrict v8 = (u8 *)v;
        u8 const * const restrict m8 = (u8 *)m;

        blake2b(v, m);
        global_iv[i] = v8[i] ^ m8[i];
    }
}
//----------------------------------------------------------------------------------------------------------------------
void blake2b(u64 * const restrict v, u64 const * const restrict m)
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
void set_tweak(const u64 block_num, const ui tweak_num, u64 tweak)
{
    const u64 base_address = (block_num * TWEAKS * TWEAK_BITS) + (tweak_num * TWEAK_BITS);

    for (ui i=TWEAK_BITS-1;tweak;i--,tweak>>=1)
    {
        set_bit(tweaks, base_address + i, tweak & 1);
    }
}
//----------------------------------------------------------------------------------------------------------------------
ui get_bit(u8 const * const restrict stream, const u32 address)
{
    return (stream[address / CHAR_BIT] >> ((CHAR_BIT - 1) - (address % CHAR_BIT))) & 1;
}
//----------------------------------------------------------------------------------------------------------------------
void set_bit(u8 * const restrict stream, const u32 address, const ui bit)
{
    const u8 byte = 1 << ((CHAR_BIT - 1) - (address % CHAR_BIT));

    if (bit) stream[address / CHAR_BIT] |= byte;
    else stream[address / CHAR_BIT] &= ~byte;
}
//----------------------------------------------------------------------------------------------------------------------
u64 tick(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return ((u64)now.tv_sec * 1000) + ((u64)now.tv_nsec / 1000000);
}
//----------------------------------------------------------------------------------------------------------------------