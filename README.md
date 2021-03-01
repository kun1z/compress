# compress - WIP TEST SOFTWARE

Maximum Entropy Compressor - Using Probability Hashes w/Hash-Chaining to compress "impossible" data sets.

Copyright © 2021 by Brett Kuntz. All rights reserved.

# Instructions

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

# Examples

Included in the /examples/ directory are compressed files and their respective outputs.

# To Do

1. Tweaks must be compressed using a similar technique as the 1MB main blocks.
2. A secondary compressor must be found (or created) to compress the 1MB output.bin files. The compressor needs to be tailored for files where there are many bits at the front, and very few in the middle and end.
3. A second method for scoring shuffling needs to be tested. It may be better than the current method.
4. More cuts are needed to further reduce the entropy in output.bin

If you would like to help out with testing, or to just provide free computations, it would be greatly appreciated.

# History

I came up with the idea for this compressor (symbol substitution via brute-force) in 2007, and begun preliminary work on it in Nov 2018. I put that work on hold once again until Dec 2020, which I can now focus on it full-time. The general idea of symbol-substitution is that a large hash is really just a giant 2^1159 byte database of random noise that is compressed into an extremely tiny decompressor. Brute-force is used to figure out which keys into the database best match the white noise of the input file, and then those symbols are subtracted (XOR'd) out from the input, leaving behind very few bits.