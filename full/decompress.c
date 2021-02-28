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

    // Temp load tweaks from file

    FILE * ftweaks = fopen("tweaks.bin", "rb");
    if (ftweaks)
    {
        fread(tweaks, 1, TWEAK_SIZE, ftweaks);
        fclose(ftweaks);
        puts("Opened [tweaks.bin] for input");
    }
    else return EXIT_FAILURE;

    // Temp load inverts from file

    FILE * finverts = fopen("inverts.bin", "rb");
    if (finverts)
    {
        fread(inverts, 1, INVERT_SIZE, finverts);
        fclose(finverts);
        puts("Opened [inverts.bin] for input");
    }
    else return EXIT_FAILURE;

    // Transpose

    puts("Transposing");
    memcpy(outdata, indata, FILE_SIZE);

    for (u64 i=0;i<128;i++)
    {
        for (u64 b=0;b<BLOCKS;b++)
        {
            indata[(b * 128) + i] = outdata[(i * BLOCKS) + b];
        }
    }

    memset(outdata, 0, FILE_SIZE);

    // Start

    puts("Starting decompression...");

    pthread_spin_init(&csjob, PTHREAD_PROCESS_PRIVATE);

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

    printf("Saving [%s]\n", argv[3]);
    fwrite(outdata, 1, FILE_SIZE, foutput);
    fclose(foutput);

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

        printf("decompressing block %04"PRIu64"...\n", block_num);
        fflush(0);

        u64 tweak;
        ui invert;
        u64 v[16], m[16];
        u8 input_block[128], output_block[128];

        const u64 sub_block = block_num * BLOCK_PRIME_MUL;

        memcpy(input_block, &indata[block_num * 128], 128);

        // Invert the shuffle

        tweak = get_tweak(block_num, TWEAKS - 1);
        invert = get_bit(inverts, (block_num * 2) + 1);

        memcpy(v, global_iv, 128);
        memcpy(m, global_iv, 128);

        for (ui i=0;i<16;i++)
        {
            v[i] += BLAKE_IV * (sub_block + (TWEAKS - 1));
            m[i] += BLAKE_IV * tweak;
        }

        if (invert) // mirror all bits
        {
            for (ui i=0;i<512;i++)
            {
                const ui ii = get_bit(input_block, i);
                const ui ij = get_bit(input_block, 1023 - i);

                set_bit(input_block, i, ij);
                set_bit(input_block, 1023 - i, ii);
            }
        }

        ishuffle(output_block, input_block, v, m);

        // Invert the p-hashes

        for (si i=CUTS_LENGTH-1;i>=0;i--)
        {
            memcpy(input_block, output_block, 128);

            tweak = get_tweak(block_num, i + 1);

            memcpy(v, global_iv, 128);
            memcpy(m, global_iv, 128);

            for (ui j=0;j<16;j++)
            {
                v[j] += BLAKE_IV * (sub_block + i + 1);
                m[j] += BLAKE_IV * tweak;
            }

            p_hash(output_block, input_block, v, m, CHAIN_CUTS[i]);
        }

        memcpy(input_block, output_block, 128);

        // Invert the final hash

        tweak = get_tweak(block_num, 0);
        invert = get_bit(inverts, block_num * 2);

        memcpy(v, global_iv, 128);
        memcpy(m, global_iv, 128);

        for (ui i=0;i<16;i++)
        {
            v[i] += BLAKE_IV * sub_block;
            m[i] += BLAKE_IV * tweak;
        }

        if (invert) // flip all bits
        {
            for (ui i=0;i<128;i++)
            {
                input_block[i] = ~input_block[i];
            }
        }

        hash(output_block, input_block, v, m);

        memcpy(&outdata[block_num * 128], output_block, 128);

        // Progress report

        const r64 ms = (tick() - start_tick) / 60000.;
        const r64 pm = (block_num + 1) / ms;
        const u64 rem = BLOCKS - (block_num + 1);
        printf("decompressed block %04"PRIu64" - %.1f mins remain\n", block_num, rem / pm);
        fflush(0);
    }

    return 0;
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
void ishuffle(u8 * const restrict output_block, u8 const * const restrict input_block, u64 const * const restrict RO_IV, u64 const * const restrict m)
{
    u64 v[16];
    memcpy(v, RO_IV, 128); // A copy is needed because the IV is Read-Only

    blake2b(v, m);

    u16 indices[1024];

    for (u16 i=0;i<1024;i++)
    {
        indices[i] = i;
    }

    ui i = 1023;

    while (1)
    {
        u64 * const restrict p = &v[i & 15];

        const ui j = *p % (i + 1);

        const ui ii = indices[i];
        const ui ij = indices[j];

        indices[i] = ij;
        indices[j] = ii;

        if (i == 1) break;

        i--;

        *p ^= *p << 13;
        *p ^= *p >> 7;
        *p ^= *p << 17;
    }

    for (ui i=0;i<1024;i++)
    {
        set_bit(output_block, indices[i], get_bit(input_block, i));
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
u64 get_tweak(const u64 block_num, const ui tweak_num)
{
    u64 tweak = 0;
    const u64 base_address = (block_num * TWEAKS * TWEAK_BITS) + (tweak_num * TWEAK_BITS);

    for (ui i=0;i<TWEAK_BITS;i++)
    {
        tweak <<= 1;
        tweak |= get_bit(tweaks, base_address + i);
    }

    return tweak;
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