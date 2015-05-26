# Benchmarks

The time I use for these benchmarks is the one printed out when the program exits. These results were obtained with an Intel i5-2430M SandyBridge second generation processor.

## Brute Force
This benchmark checks all lower alphanumeric strings of length 6 or less. It is run with "Hackintosh-cxx brute benchmark".

Initially, it was done in 0-3 range.

Various changes applied before each test, most of which were optimizations:

    55.427s
    57.36s
    56.237s
    55.455s
    53.535s
    53.51s

After moving MD5::m_finalized to end of class to optimize cache lines:

    52.964s
    52.293s
    52.833s
    52.535s

If branching is replaced with while loop when doing password digit carry:

    53.714s
    53.641s
    53.843s

After PGO:

    51.902s
    51.814s

After using OpenSSL's MD5 hash routine (this was reverted after this set of tests):

    50.788s
    51.397s

    PGO didn't improve it

With GCC 5.1:

    49.905s
    50.44s
    49.439s

Checkpoint save triggered by a counter instead of system clock:

    43.347s
    43.304s
    43.442s

MD5 class methods' inline keyword has been removed so compiler can decide it:

    43.369s
    43.99s
    43.375s

Overflow loop now checks "pos", an unsigned integer, instead of bool and removed some branching:

    11.027s
    10.968s

Changed to 0-1 range for benchmarking after this.

With if statement around overflow while loop:

    11.016s
    11.025s
    11.027s
    11.041s
    11.102s
    11.042s

Without if statement around overflow while loop:

    10.969s
    10.976s
    10.974s
    10.988s
    10.97s
    10.971s

After fixing corner case for passwords of length 1:

    10.951s
    10.949s
    10.973s

After fixing overflow triggering prematurely:

    21.882s
    21.933s
    21.912s

Changed back to 0-3 range for consistency with previous data:

    43.606s
    43.559s
    43.517s
    43.539s
    43.569s
    43.644s
    43.688s

Moved startTime up to outer while loop to avoid including file I/O in timing:

    43.572s
    43.662s

Before adding [[gnu::packed]] attribute to MD5 class:

    44.214s
    44.248s

After adding [[gnu::packed]] attribute to MD5 class:

    43.886s
    43.885s

Initial dual-thread benchmark:

    63.413s
    63.432s

    63.345s
    63.357s

Made thread-local copy of target hash:

    62.138s
    62.192s

    62.153s
    62.206s

# Dictionary
This benchmark checks 1000 integers appended on 349900 words. The dictionary is included in the source tree. The benchmark is run with "Hackintosh-cxx dict benchmark".

Initial:

    10.820s

# Combo
This benchmark combines two words from a dictionary and appends an integer. The benchmark is run with "Hackintosh-cxx combo benchmark".

Initial:

    58.306s

