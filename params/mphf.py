#!/usr/bin/env python3
# (Minimal) Perfect Hash Functions Generator (key, value) value in this code
# is the key counter during reading but can be any number

# implementing the MOS Algorithm II CACM92 , and Amjad M Daoud Thesis 1993 at VT;
# based on Steve Hanof implementation
# http://stevehanov.ca/blog/index.php?id=119.
#
# Download as http://iswsa.acm.org/mphf/mphf.py
# For minimal perfect hashing use:  size = len(dict)
#
# Modified from the original for Leela

import sys
import math

def ROR(x, n):
    mask = (2**n) - 1
    mask_bits = x & mask
    return (x >> n) | (mask_bits << (32 - n))

def ROL(x, n):
    return ROR(x, 32 - n)

# first level simple hash ... used to disperse patterns using random d values
def myhash(d, p):
    p ^= ROL(p, 11) ^ (((p + d) & 0xFFFFFFFF) >> 6)
    return p & 0xFFFFFFFF
    # Actually good distribution
    #d = (d * 747796405) & 0xFFFFFFFF
    #p = (p * 851723965) & 0xFFFFFFFF
    #d = (p + d) & 0xFFFFFFFF;
    #p = rotl(p, 11) ^ (d >> 13);
    #SipHash
    #return hash(str(d) + str(p)) & 0xFFFFFFFF

def ghash(p):
    p ^= p >> 3
    return p & 0xFFFFFFFF

def isprime(x):
    x = abs(int(x))
    if x < 2:
        return "Less 2", False
    elif x == 2:
        return True
    elif x % 2 == 0:
        return False
    else:
        for n in range(3, int(x**0.5) + 2, 2):
            if x % n == 0:
                return False
        return True

def nextprime(x):
    while True:
        if isprime(x):
            break
        x += 1
    return x

def power_log(x):
    return 2**(math.ceil(math.log(x, 2)))

# create PHF with MOS(Map,Order,Search), g is specifications array
def CreatePHF(dict):
    size = len(dict)
    #size = nextprime(int(len(dict) * 1.23))
    #size = (int(len(dict) * 1.019))
    size = int(power_log(len(dict)))
    print("Size = %d" % (size))

    # nextprime(int(size/(6*math.log(size,2))))
    # c=4 corresponds to 4 bits/key
    # for fast construction use size/5
    # for faster construction use gsize=size
    #gsize = int(size / 2)
    gsize = int(power_log(size // 32))
    print("G array size = %d" % gsize)
    sys.stdout.flush()

    # Step 1: Mapping
    patterns = [[] for i in range(gsize)]
    g = [0] * gsize  # initialize g

    values = [None] * size  # initialize values
    for key in dict.keys():
        patterns[ghash(key) % gsize].append(key)

    # Step 2: Sort patterns in descending order and process
    patterns.sort(key=len, reverse=True)
    print("Largest buckets: ")
    print([len(x) for x in patterns[0:min(len(patterns), 20)]])
    for b in range(gsize):
        pattern = patterns[b]
        if len(pattern) == 0:
            break
        d = 0
        item = 0
        slots = []

    # Step 3: rotate patterns and search for suitable displacement
        while item < len(pattern):
            slot = myhash(d, pattern[item]) % size
            if values[slot] is not None or slot in slots:
                d += 1
                if d == 1 << 16:
                    print("%d/%d is %d, hard " % (b, gsize, len(pattern)))
                if d == 1 << 24:
                    print("%d/%d is %d, very hard " % (b, gsize, len(pattern)))
                if d < 0:
                    print("Giving up on " + str(pattern))
                    break
                item = 0
                slots = []
            else:
                slots.append(slot)
                item += 1
        if d < 0:
            print("failed")
            return

        g[ghash(pattern[0]) % gsize] = d
        for i in range(len(pattern)):
            values[slots[i]] = dict[pattern[i]]
        if (b % 1000) == 0:
            print("%d/%d: pattern %d processed" % (b, gsize, len(pattern)))
        sys.stdout.flush()

    print("Empy buckets left: %d" % (gsize - b - 1))

    # Process patterns with one key and use a negative value of d
    freelist = []
    for i in range(size):
        if values[i] is None:
            freelist.append(i)
    for b in range(b, gsize):
        pattern = patterns[b]
        if len(pattern) == 0:
            break
        assert len(pattern) == 1
        slot = freelist.pop()
        # subtract one to handle slot zero
        g[ghash(pattern[0]) % gsize] = -slot - 1
        values[slot] = dict[pattern[0]]
        if (b % 1000) == 0:
            print("-%d: pattern %d processed" % (b, len(pattern)))
            sys.stdout.flush()
    print("PHF succeeded")
    return (g, values)


# Look up a value in the hash table, defined by g and V.
def lookup(g, V, key):
    d = g[ghash(key) % len(g)]
    if d < 0:
        return V[-d - 1]
    return V[myhash(d, key) % len(V)]


# main program
# reading keyset size is given by num
DICTIONARY = "sortpat.txt"

pdict = {}
line = 1

for key in open(DICTIONARY, "rt").readlines():
    pdict[int(key.strip())] = line
    line += 1

print("Read %d patterns" % (len(pdict)))

# creating phf
print("Creating perfect hash for %d patterns" % (len(pdict)))
(g, V) = CreatePHF(pdict)

# printing phf specification
print("Printing g[]")
#print(g)
with open('g_mphf.txt', 'a') as out_file:
    for i, g_v in enumerate(g):
        if i % 10 == 9:
            out_file.write("\n")
        out_file.write(" " + str(g_v) + ",")

maxg = max(g)
ming = min(g)
print("Min/Max g: %d/%d" % (ming, maxg))
gSize = 0
if ming >= 0:
    maxg = max(abs(ming), maxg)
    if maxg <= 255:
        gSize = 1
    elif maxg <= 65535:
        gSize = 2
    else:
        gSize = 4
else:
    maxg = max(abs(ming), maxg)
    if maxg <= 127:
        gSize = 1
    elif maxg <= 32767:
        gSize = 2
    else:
        gSize = 4
print("Data size %d x 4 + %d x %d = %d" %
      (len(V), len(g),  gSize, len(V)*4+len(g)*gSize))

# fast verification for few (key,value) count given by num1
num1 = 10
print("Verifying %d hash values" % (num1))
line = 1

for key in open(DICTIONARY, "rt").readlines():
    line = lookup(g, V, int(key.strip()))
    print("Pattern %d occurs on line %d" % (int(key.strip()), line))
    line += 1
    if line > num1:
        break
