# compress
Maximum Entropy Compressor - Using Probability Hashes w/Hash-Chaining to compress "impossible" data sets.

Copyright © 2021 by Brett Kuntz. All rights reserved.

See the /full/ directory for the latest test iteration that compresses & decompresses. Both files are stand-alone and you do not need to compile both for either to work.

Compressing takes about 35 minutes on an EC2 c5.24xlarge machine, decompression takes place in real-time.

Neither the compressor or decompressor require more than a few kilobytes of RAM to operate. They are best implemented on ASIC or GPU.

Example Usage:

    compress 2020-04-20.bin 2020-04-20.iv output.bin > output1.txt
    decompress output.bin 2020-04-20.iv final.bin > output2.txt

    (the .iv file must be 16 bytes; random bytes or just all 0's, it does not matter)

Compression should output 3 files (for now):

1. output.bin - The 1MB file with approx 12% bits remaining, with the majority of bits at the start of the file
2. tweaks.bin - The 600kb worth of tweaks used. I'm about to work on compressing these next.
3. inverts.bin - A 2kb file that was needed since I made a mistake designing the decompressor and it turns out it is not able to decompress without this information.

For now please use FREE files from [Random.org/binary](https://archive.random.org/binary) so we can download and use the same files as you for testing purposes!