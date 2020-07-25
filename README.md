# Effect of cache evictions on Intel Extended Page Table

## Overview of AnC 

ASLR^Cache by VUSec researchers [1] is a side channel attack to break Address Space Layout Randomization using virtual address translation performed by the MMU. Some notes on AnC attack:

-	Recent page table translations by MMU are cached in Translation Lookaside Buffer (TLB). Since TLB misses are costly, page table pages are cached in last level cache (LLC)
-	During page table walk, skipping the 12-bit page offset, each 9 bits from the virtual address (VA) is used as index at each level of page table walk 
-	In case of TLB miss, out of 9 bits from VA, 6 bits are used as cache line index and 3 bits are used as cache line offset to fetch the PTE from cache
-	Attacker can access a target memory page to fetch the related PTEs into the cache, evict the TLB entries, evict cache lines one by one from 0-63 and time the access to target memory page for each eviction 
-	If the time to access the target memory page increases on eviction of a cache line, attacker can infer that this cache line is used by MMU 

## Extended Page Table

Extended Page Table (EPT) is a hardware feature for MMU virtualization by Intel. The physical address as seen by the guest is not the actual physical address of a page in memory. During VA translation in guest, all the PTEs in 4 level page walk - gPML4E, gPDPTE, gPDE, gPTE and gCR3 register are further translated using an intermediate page walk to locate the host physical address (hPA) of guest page table pages.

The physical address translations in EPT can result in a maximum of 20 memory loads i.e. gPML4E, gPDPTE, gPDE, gPTE and gCR3 going through 4
levels of translation (5 x 4 = 20). Moreover, the guest virtual address (gVA) is looked up in each translated page table pages, adding 4 more memory loads per translation. 

## Evicting EPT translations

The learning from AnC attack is that MMU translation can be slowed down by cache line evictions. This raises some questions:

-	Does EPT use part of guest physical address (gPML4E, gPDPTE, gPDE, gPTE and gCR3) to lookup cached entries just like how VA was used for lookup?
-	Does evicting intermediate translations from cache cause detectable signals during EPT walk?
-	Can unprivileged guest user learn about guest physical address mappings?

## Test environment and experiments

The experiment was carried out on two processors with Ubuntu Xenial running as guest in VMware Workstation:
```
Intel Core i7-5557U (Broadwell microarchitecture)
Intel Core i7-2670QM (Sandy Bridge microarchitecture)
```
The PoC includes a kernel driver to read gCR3 value for a given process ID and gain unrestricted access to Linux mem device from user space. The user space process based on revanc [2][3] maps the gCR3 value, logs all the PTEs
for a VA and measures the access time using EVICT+TIME attack during VA translation by MMU.

For each cache line, measure the filtered access time and sort the cache line indexes based on higher timings. Cache line indexes used as part of PTEs and VA scored higher timings compared to other cache lines.

```
guest@ubuntu:~/slatmmu/cr3$ make
guest@ubuntu:~/slatmmu/cr3$ sudo insmod cr3.ko
guest@ubuntu:~/slatmmu/revanc$ make
guest@ubuntu:~/slatmmu/revanc$ sudo ./obj/slatleak -r10000
4-way set associative L1 d-TLB (4 entries, 1G page)
4-way set associative L1 d-TLB (32 entries, 2M page 4M page)
4-way set associative L1 d-TLB (64 entries, 4K page)
L1 i-TLB (8 entries, 2M page 4M page)
8-way set associative L1 i-TLB (64 entries, 4K page)
64B prefetch
4-way set associative L2 TLB (16 entries, 1G page)
6-way set associative L2 TLB (1536 entries, 4K page 2M page)
8-way set associative L1 d-cache (32K, 64B line size)
8-way set associative L1 i-cache (32K, 64B line size)
8-way set associative L2 cache (256K, 64B line size)
16-way set associative L3 cache (4M, 64B line size. inclusive)
Settings:
  runs: 1
  rounds: 10000
  page format: default
  cache size: 4M
  cache line size: 64B

Detected CPU name: Intel(R) Core(TM) i7-5557U CPU @ 3.10GHz

Dumping page table information...
Profiling cache...
Check slat-timings.csv for results!
```
Install dependencies:
```
sudo apt-get install python-numpy python-matplotlib python-scipy python-pandas
```
```
guest@ubuntu:~/slatmmu/revanc$ python scripts/slatfilter.py results/slat-timings.csv 
gVA	0x3ffff6fef000:	15, 63, 54, 61
gCR3	0x6b5b6000:	0, 0, 43, 54
gPML4E	0x6a06e000:	0, 0, 42, 13
gPDPTE	0x7354b000:	0, 0, 51, 41
gPDE	0x8b9be000:	0, 0, 11, 55
gPTE	0x110f0000:	0, 0, 17, 30

SLAT cacheline candidates:
set([0, 11, 13, 15, 17, 30, 41, 42, 43, 51, 54, 55, 61, 63])
Cacheline: 55,	Score: 682	[OK]
Cacheline: 54,	Score: 536	[OK]
Cacheline: 30,	Score: 418	[OK]
Cacheline: 63,	Score: 401	[OK]
Cacheline: 13,	Score: 385	[OK]
Cacheline: 41,	Score: 358	[OK]
Cacheline: 61,	Score: 348	[OK]
Cacheline: 14,	Score: 290
Cacheline: 40,	Score: 283
Cacheline: 42,	Score: 268	[OK]
Cacheline: 43,	Score: 265	[OK]
Cacheline: 51,	Score: 259	[OK]
Cacheline: 15,	Score: 256	[OK]
Cacheline: 12,	Score: 254
Cacheline: 31,	Score: 250
Cacheline: 17,	Score: 244	[OK]
Cacheline: 62,	Score: 236
Cacheline: 11,	Score: 228	[OK]
Cacheline: 8,	Score: 224
Cacheline: 60,	Score: 223
Cacheline: 5,	Score: 218
Cacheline: 37,	Score: 216
Cacheline: 0,	Score: 213	[OK]
Cacheline: 22,	Score: 212
Cacheline: 9,	Score: 211
Cacheline: 44,	Score: 211
Cacheline: 16,	Score: 211
Cacheline: 50,	Score: 210
Cacheline: 3,	Score: 210
Cacheline: 57,	Score: 209
Cacheline: 7,	Score: 208
Cacheline: 39,	Score: 208
Cacheline: 25,	Score: 207
Cacheline: 49,	Score: 207
Cacheline: 2,	Score: 207
Cacheline: 6,	Score: 207
Cacheline: 18,	Score: 206
Cacheline: 35,	Score: 206
Cacheline: 52,	Score: 205
Cacheline: 53,	Score: 204
Cacheline: 34,	Score: 203
Cacheline: 38,	Score: 203
Cacheline: 10,	Score: 203
Cacheline: 4,	Score: 201
Cacheline: 26,	Score: 201
Cacheline: 45,	Score: 200
Cacheline: 58,	Score: 199
Cacheline: 48,	Score: 198
Cacheline: 19,	Score: 198
Cacheline: 20,	Score: 197
Cacheline: 32,	Score: 197
Cacheline: 27,	Score: 196
Cacheline: 59,	Score: 195
Cacheline: 33,	Score: 195
Cacheline: 36,	Score: 194
Cacheline: 23,	Score: 192
Cacheline: 47,	Score: 190
Cacheline: 56,	Score: 189
Cacheline: 29,	Score: 189
Cacheline: 24,	Score: 189
Cacheline: 21,	Score: 186
Cacheline: 1,	Score: 186
Cacheline: 28,	Score: 183
Cacheline: 46,	Score: 183
```
![alt text](https://github.com/renorobert/revanc/blob/master/results/slatfilter-broadwell.png "slatfilter")

Results show that its highly likely guest physical addresses from guest page tables are used as cache line index during EPT translation. Evicting those cache lines causes measurable slowdowns during memory access in a guest.

## References

[1] ASLR on the Line: Practical Cache Attacks on the MMU    
https://www.cs.vu.nl/~giuffrida/papers/anc-ndss-2017.pdf

[2] RevAnC: A Framework for Reverse Engineering Hardware Page Table Caches     
https://download.vusec.net/papers/revanc_eurosec17.pdf

[3] RevAnc source code    
https://github.com/vusec/revanc
