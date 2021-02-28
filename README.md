# compress
Maximum Entropy Compressor - Using Probability Hashes w/Hash-Chaining to compress "impossible" data sets.

Copyright © 2021 by Brett Kuntz. All rights reserved.

See the /full/ directory for the latest test iteration that compresses & decompresses. Both files are stand-alone and you do not need to compile both for either to work.

Compressing takes about 35 minutes on an EC2 c5.24xlarge machine, decompression takes place in real-time.

Neither the compressor or decompressor require more than a few kilobytes of RAM to operate. They are best implemented on ASIC or GPU.