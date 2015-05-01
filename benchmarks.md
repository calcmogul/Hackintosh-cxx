# Benchmarks

## Brute Force
This benchmark checks all lower alphanumeric strings of length 6 or less. It is run with "Hackintosh-cxx brute 0".

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

# Dictionary
This benchmark checks 1000 integers appended on 349900 words. The dictionary is included in the source tree. The benchmark is run with "Hackintosh-cxx dict 0".

Initial:

    10.820s

