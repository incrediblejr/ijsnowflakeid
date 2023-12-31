/* clang-format off */
/*
`ijsnowflakeid` - https://github.com/incrediblejr/ijsnowflakeid

`ijsnowflakeid` is a bit layout configurable, lock-free, and thread-safe implementation of
Twitter's Snowflake ID [1] or any UUID that adopts a similar approach but varies in
bit assignments within the UUID. Its configurable bit layout allows users to fully
utilize all 64 bits (as opposed to Twitter's Snowflake ID which employs only 63 bits)
or to customize their bit layout for timestamp, machine, and sequence identifiers.

A Snowflake ID, henceforth referred to as 'snowflake', is a 64-bit unique identifier
divided into three parts:

 * Timestamp
      The number of milliseconds since a specific epoch.
      `ijsnowflakeid` uses Unix epoch by default but any start time is configurable.
      `ijsnowflakeid` also supports custom time unit in ms to facilitate that bits in
      the snowflake's timestamp can represent a longer lifetime.

 * Machine/Instance ID
      A unique identifier for a machine or instance to prevent collisions when
      snowflakes are generated in the same millisecond/time unit in a
      distributed environment.

 * Sequence Number
      A local sequence number assigned to each snowflake generated in the same
      millisecond/time unit. This implies that a machine or instance can generate a
      maximum of (1 << number of sequence number bits) snowflakes per
      millisecond/time unit without advancing the time.

One limitation of Snowflake IDs, or similar UUIDs, is the sequence number cap,
restricting the number of IDs that can be generated per millisecond/time unit.
A common solution is to have the ID generator pause/sleep/spin a time unit before
generating new IDs, effectively acting as a rate limiter. However, `ijsnowflakeid`
employs an alternative method by generating IDs set in the future. Instead of
waiting for a time unit to elapse, it advances its internal timestamp 1 time unit
and continues ID generation. This approach assumes that such a need,
like a sudden spike in requests, is rare and that the real clock will eventually
synchronize. The duration which it takes to synchronize depends on the average
load factor (percentage of sequence numbers used during a typical time unit) and
the extent of future time borrowed.

Due to its implementation, `ijsnowflakeid` can only guarantee successful artificial
advancement of its timestamp if the machine or instance operates with fewer than
1<<(64 - number of timestamp bits - number of sequence number bits) concurrent
threads generating IDs. This limitation is mostly theoretical;
for instance, with Twitter's Snowflake ID configuration, it translates to fewer
than 2048 threads (1<<(64-41-12)).

This file provides both the interface and the implementation.

`ijsnowflakeid` is implemented as a stb-style header-file library[2]
which means that in *ONE* source file, put:

#define IJSNOWFLAKEID_IMPLEMENTATION
// if no external dependencies is wanted on assert.h (or wanting a custom handler)
#define IJSF_assert custom_assert
#include "ijsnowflakeid.h"

Other source files should just include ijsnowflakeid.h

EXAMPLE
   // Twitter Snowflake ID configuration
   uint32_t timestamp_num_bits = 41;
   uint32_t machineid_num_bits = 10;
   uint32_t sequence_num_bits = 12;
   uint32_t machineid = 3; // optional, will be used with `ijsnowflakeid_generate`
   uint64_t optional_starttime_in_ms_since_unix_epoch = 1288834974657ull; // (Thu Nov 04 2010 01:42:54 UTC)
   uint32_t timeunit_in_ms = 1;

   // allocate memory for `ijsnowflakeid` instance
   void *snowmem = malloc(IJSNOWFLAKEID_MEMORY_SIZE);

   ijsnowflakeid *snow = ijsnowflakeid_init(snowmem,
      timestamp_num_bits, machineid_num_bits, sequence_num_bits,
      machineid, optional_starttime_in_ms_since_unix_epoch, timeunit_in_ms);

   int64_t snowflakes[2];

   // generate a unique id, with provided machineid
   snowflakes[0] = ijsnowflakeid_generate_id(snow, machineid+3);
   assert(ijsnowflakeid_machineid(snow, snowflakes[0]) == machineid+3);

   // generate a unique id, with machineid of snow
   snowflakes[1] = ijsnowflakeid_generate(snow);
   assert(ijsnowflakeid_machineid(snow, snowflakes[1]) == machineid);

   // last snowflake is guaranteed to have same or later timestamp as it happened after
   assert(ijsnowflakeid_timestamp(snow, snowflakes[1]) >= ijsnowflakeid_timestamp(snow, snowflakes[0]));

   // deallocate memory when done with instance
   free(snow);

LICENSE
See end of file for license information

REFERENCES

[1] https://en.wikipedia.org/wiki/Snowflake_ID
[2] https://github.com/nothings/stb
*/
#ifndef IJSNOWFLAKEID_INCLUDED
#define IJSNOWFLAKEID_INCLUDED

#include <stdint.h>

#if defined(_MSC_VER) && !defined(__clang__)
   #ifndef IJSF_FORCEINLINE
      #define IJSF_FORCEINLINE inline __forceinline
   #endif
#else
   #ifndef IJSF_FORCEINLINE
      #define IJSF_FORCEINLINE inline __attribute__((__always_inline__))
   #endif
#endif

#ifdef __cplusplus
   extern "C" {
#endif

#define IJSNOWFLAKEID_MEMORY_SIZE (64+64)
#define IJSNOWFLAKEID_ALIGNMENT   (8)

typedef struct ijsnowflakeid {
   uint64_t starttime;

   uint8_t timestamp_num_bits;
   uint8_t machineid_num_bits;
   uint8_t sequence_num_bits;
   uint8_t unused_num_bits;

   uint32_t machineid;
   uint32_t timeunit_ms;
   uint32_t padding32;
} ijsnowflakeid;

/*
`memory` must be at least `IJSNOWFLAKEID_MEMORY_SIZE` bytes and aligned to at
least `IJSNOWFLAKEID_ALIGNMENT`.

`timestamp_num_bits`, `machineid_num_bits`, and `sequence_num_bits`
defines the bit-layout of the 64-bit snowflake. The first `timestamp_num_bits`
bits are a timestamp, representing time units since the chosen epoch. The next
`machineid_num_bits` bits represent a machine ID, preventing clashes.
`sequence_num_bits` more bits represent a per-machine sequence number,
to allow creation of multiple snowflakes in the same time unit.

Example: with 41-bit `timestamp_num_bits`, valid snowflakes can be generated
until 7 September 2039. To extend the validity period of snowflakes, use a
combination of `optional_starttime_in_ms_since_unix_epoch`, more
`timestamp_num_bits`, and/or `optional_timeunit_in_ms` > 1.

NB: If `ijsnowflakeid` runs out of sequence IDs for the current millisecond/
'time unit', it borrows sequences from future ticks. To facilitate this, we
need bits to detect exhaustion of sequence numbers for the current
millisecond/'time unit' and time to start borrowing from the future. This
requires fewer than 1<<(64 - number of timestamp bits - number of sequence
number bits) concurrent threads generating IDs.

`machineid` is optional but useful if all snowflakes generated by this instance
will have the same `machineid`. This is the `machineid` that is used in
`ijsnowflakeid_generate_id`, for using other `machineid` not set during
initialization `ijsnowflakeid_generate` should be used.

`optional_starttime_in_ms_since_unix_epoch` is an optional custom start time in
milliseconds since the Unix epoch. This optional start time sets the epoch
relative to which all snowflakes generated from these settings are measured.
Setting this to 0 means that all snowflakes' timestamps are relative to the
Unix epoch.

Examples:
- Twitter's Snowflake ID uses the custom start time/epoch of 1288834974657
  (Thu Nov 04 2010 01:42:54 UTC).
- Discord's IDs use a custom start time/epoch of 1420070400000
  (Thu Jan 01 2015 00:00:00 UTC).
- Sonyflakes uses a custom start time/epoch of 1409529600000
  (Mon Sep 01 2014 00:00:00 UTC). Sonyflakes also uses a 10ms time unit,
  more details below.

`optional_timeunit_in_ms` defines the time unit (or resolution) of the timestamp,
with the default being 1 ms. Setting this to 0 or 1 maintains the default time
unit of 1ms. Increasing this above 1ms extends the lifespan of the snowflakes
(i.e., the number of years that can be represented by the timestamp), but
reduces the number of snowflakes that can be generated per millisecond without
artificially advancing the internal clock, as only (1<<sequence_num_bits)
can be generated per time unit before the internal clock must be advanced.

Returns a non-null instance on valid input parameters.
*/
ijsnowflakeid *ijsnowflakeid_init(void *memory, uint32_t timestamp_num_bits,
   uint32_t machineid_num_bits, uint32_t sequence_num_bits, uint32_t machineid,
   uint64_t optional_starttime_in_ms_since_unix_epoch,
   uint32_t optional_timeunit_in_ms);

/* generate a snowflake with machineid */
int64_t ijsnowflakeid_generate_id(ijsnowflakeid *self, uint32_t machineid);

/* generate a snowflake with machineid assigned on `init` */
static IJSF_FORCEINLINE int64_t ijsnowflakeid_generate(ijsnowflakeid *self) {
   return ijsnowflakeid_generate_id(self, self->machineid);
}

/* helper functions for getting timestamp/machineid/sequence from a snowflake id */
static IJSF_FORCEINLINE int64_t ijsnowflakeid_timestamp(ijsnowflakeid *self, int64_t snowflakeid) {
   int64_t mask = (1ll<<self->timestamp_num_bits)-1;
   return (snowflakeid>>(self->machineid_num_bits+self->sequence_num_bits))&mask;
}

static IJSF_FORCEINLINE int64_t ijsnowflakeid_timestamp_ms_since_unix_epoch(ijsnowflakeid *self, int64_t snowflakeid) {
   int64_t timestamp = ijsnowflakeid_timestamp(self, snowflakeid);
   return (timestamp+self->starttime)*self->timeunit_ms;
}

static IJSF_FORCEINLINE uint32_t ijsnowflakeid_machineid(ijsnowflakeid *self, int64_t snowflakeid) {
   uint32_t mask = (1u<<self->machineid_num_bits)-1;
   return ((uint32_t)(snowflakeid>>self->sequence_num_bits))&mask;
}

static IJSF_FORCEINLINE uint32_t ijsnowflakeid_sequenceid(ijsnowflakeid *self, int64_t snowflakeid) {
   int64_t mask = (1ll<<self->sequence_num_bits)-1;
   return (uint32_t)(snowflakeid&mask);
}

#ifdef __cplusplus
   }
#endif

#endif /* IJSNOWFLAKEID_INCLUDED */

#if defined(IJSNOWFLAKEID_IMPLEMENTATION) && !defined(IJSNOWFLAKEID_IMPLEMENTED)

#define IJSNOWFLAKEID_IMPLEMENTED (1)

#if !defined(IJSF_assert)
   #if !defined(NDEBUG)
      #include <assert.h>
      #define IJSF_assert(x) assert((x))
   #else
      #if 0
         static IJSF_FORCEINLINE void SFID_ASSERT_impl(int32_t cond) { if (!cond) *((volatile int32_t*)(intptr_t)cond) = cond+1; }
         #define IJSF_assert SFID_ASSERT_impl
      #else
         #define IJSF_assert(...)
      #endif
   #endif
#endif

#if defined(_WIN32) || defined(__WIN32__) || defined(_WIN64)
   #define IJSF_PLATFORM_WINDOWS (1)
   #define IJSF_PLATFORM_POSIX (0)
#else
   #define IJSF_PLATFORM_WINDOWS (0)
   #define IJSF_PLATFORM_POSIX (1)
#endif

#if IJSF_PLATFORM_WINDOWS
   #pragma warning(push)
   /* C4711: function 'xyz' selected for automatic inline expansion */
   /* C5045: Compiler will insert Spectre mitigation for memory load if /Qspectre switch specified */
   #pragma warning(disable: 4711 5045)
   #if !defined(WIN32_LEAN_AND_MEAN)
      #define WIN32_LEAN_AND_MEAN
   #endif

   #include <Windows.h>

   static IJSF_FORCEINLINE uint64_t ijsnowflakeid__current_time_ms(uint64_t time_interval_ms) {
      FILETIME filetime;
      ULARGE_INTEGER largeint;

      /* filetime represents the number of 100-nanosecond intervals since January 1, 1601 (UTC) */
      #if _WIN32_WINNT >= _WIN32_WINNT_WIN8
         GetSystemTimePreciseAsFileTime(&filetime);
      #else
         GetSystemTimeAsFileTime(&filetime);
      #endif

      largeint.LowPart = filetime.dwLowDateTime;
      largeint.HighPart = filetime.dwHighDateTime;

      /* 116444736000000000 is the number of 100-nanosecond intervals
         from January 1, 1601 (UTC) until January 1, 1970 (midnight UTC/GMT) (Unix epoch)

         division by 10000 = from 100 nano seconds (10^-7) to millisecond (10^-3) interval */
      return ((largeint.QuadPart - 116444736000000000ull) / (10000*time_interval_ms));
   }
#else
   #include <sys/time.h>
   static IJSF_FORCEINLINE uint64_t ijsnowflakeid__current_time_ms(uint64_t time_interval_ms) {
      struct timeval tv = {0, 0};
      gettimeofday(&tv, NULL);

      return (((uint64_t)tv.tv_sec * 1000) + ((uint64_t)tv.tv_usec / 1000))/time_interval_ms;
   }
#endif

#if defined(_MSC_VER) && !defined(__clang__)
   #ifndef _Static_assert
      #define _Static_assert static_assert
   #endif

   #define IJSF_LIKELY(x) (x)
   #define IJSF_UNLIKELY(x) (x)

   typedef volatile long long ijsf_atomic64_t;
   static IJSF_FORCEINLINE uint64_t ijsf_atomic_load64(ijsf_atomic64_t *src) { return *src; }
   /* returns the result of the add */
   static IJSF_FORCEINLINE uint64_t ijsf_atomic_add64(ijsf_atomic64_t *val, uint64_t add) { return (uint64_t)InterlockedExchangeAdd64(val, add) + add; }
   /* returns non-zero on successful swap */
   static IJSF_FORCEINLINE int32_t ijsf_atomic_cas64_acquire(ijsf_atomic64_t *dst, uint64_t val, uint64_t ref) { return ((uint64_t)InterlockedCompareExchange64(dst, val, ref) == ref) ? 1 : 0; }
#else
   #define IJSF_LIKELY(x) __builtin_expect((x), 1)
   #define IJSF_UNLIKELY(x) __builtin_expect((x), 0)

   #include <stdatomic.h>

   typedef volatile _Atomic(uint64_t) ijsf_atomic64_t;
   static IJSF_FORCEINLINE uint64_t ijsf_atomic_load64(ijsf_atomic64_t *val) { return atomic_load_explicit(val, memory_order_relaxed); }
   /* returns the result of the add */
   static IJSF_FORCEINLINE uint64_t ijsf_atomic_add64(ijsf_atomic64_t *val, uint64_t add) { return atomic_fetch_add_explicit(val, add, memory_order_relaxed) + add; }
   /* returns non-zero on successful swap */
   static IJSF_FORCEINLINE int32_t ijsf_atomic_cas64_acquire(ijsf_atomic64_t *dst, uint64_t val, uint64_t ref) { return atomic_compare_exchange_weak_explicit(dst, &ref, val, memory_order_acquire, memory_order_relaxed); }
#endif

static IJSF_FORCEINLINE uint64_t ijsnowflakeid__overflow_mask(ijsnowflakeid *self) {
   uint64_t overflow_mask = (uint64_t)-1;
   overflow_mask >>= (self->timestamp_num_bits+self->sequence_num_bits);
   overflow_mask <<= self->sequence_num_bits;
   return overflow_mask;
}

static IJSF_FORCEINLINE uint64_t ijsnowflakeid__state_timestamp_mask(ijsnowflakeid *self) {
   uint64_t state_timestamp_mask = (uint64_t)-1;
   state_timestamp_mask >>= self->timestamp_num_bits;
   return ~state_timestamp_mask;
}

typedef struct ijsnowflakeid_private {
   ijsnowflakeid base;
   unsigned char cacheline_padding1[64-sizeof(ijsnowflakeid)];

   ijsf_atomic64_t state;
   unsigned char cacheline_padding2[64-sizeof(ijsf_atomic64_t)];
} ijsnowflakeid_private;

_Static_assert(IJSNOWFLAKEID_MEMORY_SIZE >= sizeof(ijsnowflakeid_private));

ijsnowflakeid * ijsnowflakeid_init(void *memory,
   uint32_t timestamp_num_bits, uint32_t machineid_num_bits, uint32_t sequence_num_bits,
   uint32_t machineid, uint64_t optional_starttime_in_ms_since_unix_epoch,
   uint32_t optional_timeunit_in_ms) {

   ijsnowflakeid_private *res = (ijsnowflakeid_private*)memory;

   uint32_t total_nbits = timestamp_num_bits+machineid_num_bits+sequence_num_bits;
   uint32_t unused_num_bits = 64-total_nbits;

   if (!res)
      return 0;

   if ((uintptr_t)res&0x7)
      return 0;

   if (total_nbits > 64)
      return 0;

   if (machineid >= (1u<<machineid_num_bits))
      return 0;

   if (!optional_timeunit_in_ms)
      optional_timeunit_in_ms = 1;

   /* NB: make the starttime in time units */
   optional_starttime_in_ms_since_unix_epoch /= optional_timeunit_in_ms;

   if (optional_starttime_in_ms_since_unix_epoch >= (1ull<<timestamp_num_bits))
      return 0;

   res->base.starttime = optional_starttime_in_ms_since_unix_epoch;

   res->base.timestamp_num_bits = (uint8_t)timestamp_num_bits;
   res->base.machineid_num_bits = (uint8_t)machineid_num_bits;
   res->base.sequence_num_bits = (uint8_t)sequence_num_bits;
   res->base.unused_num_bits = (uint8_t)unused_num_bits;

   res->base.machineid = machineid;
   res->base.timeunit_ms = optional_timeunit_in_ms;

   res->state = 0;

   return &res->base;
}

static IJSF_FORCEINLINE int64_t ijsnowflakeid__from_state(uint64_t state,
   uint64_t state_timestamp_mask, int32_t num_unused_bits,
   int32_t num_sequence_bits, uint32_t machineid) {

   /* Account for any unused bits and place the timestamp in the most significant bits. */
   int64_t snowflakeid = (int64_t)((state&state_timestamp_mask)>>num_unused_bits);
   /* We only create snowflakes from a state where overflow buffer is empty,
      therefor the below mask is valid  */
   snowflakeid |= (int64_t)(state&~state_timestamp_mask);
   snowflakeid |= (int64_t)machineid<<num_sequence_bits;

   return snowflakeid;
}

int64_t ijsnowflakeid_generate_id(ijsnowflakeid *self, uint32_t machineid) {
   int32_t timeshift = 64-self->timestamp_num_bits;
   int32_t num_unused_bits = self->unused_num_bits;
   int32_t num_sequence_bits = self->sequence_num_bits;

   uint64_t timeunit_ms = self->timeunit_ms;

   uint64_t snowflake_starttime = self->starttime;
   uint64_t state_timestamp_mask = ijsnowflakeid__state_timestamp_mask(self);
   uint64_t overflow_mask = ijsnowflakeid__overflow_mask(self);

   uint64_t new_state, timestamp_now, timestamp_now_state;

   ijsf_atomic64_t *snowflake_state = &((ijsnowflakeid_private*)self)->state;
   int64_t snowid;

   /* Snowflake state is used for bookkeeping and lock-free concurrent snowflake
      generation

      Timestamp(T) is stored in the MSB and LSB stores the current sequence id(I),
      the in between bits is used as a overflow buffer to detect that current
      timestamp ran out of sequences.

      below is a snowflakeid generator setup with:
         - 41-bit timestamps
         - 10-bit machineid
         - 12-bit sequence id

      for a total of 63-bits. All unused bits (from the full 64-bits) becomes
      overflow bits as we do not store machineid in the state. This means that we
      can generate (1<<num sequence bit) snowflakes per `timeunit_ms` before we
      overflow.

      MSB                                                           LSB
      [TTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTOOOOOOOOOOOIIIIIIIIIIII]

      once all sequence ids for current timestamp is used up we overflow into the
      bits assigned for the overflow buffer and will try to manually advance the
      timestamp to recover from the overflow. This means that we can have at most
      (1<<num bits for overflow buffer) concurrent writers, which is not a issue
      in reality (:tm:)
    */
   timestamp_now = ijsnowflakeid__current_time_ms(timeunit_ms);

   if (IJSF_LIKELY(timestamp_now > snowflake_starttime)) {
      timestamp_now -= snowflake_starttime;
      timestamp_now_state = timestamp_now << timeshift;
      IJSF_assert((timestamp_now>>self->timestamp_num_bits)==0);
   } else {
      /* Either clock have gone backwards or a badly configured starttime, do best
         we can by setting the timestamp to zero and use the one stored in state */
      timestamp_now_state = timestamp_now = 0;
   }

   IJSF_assert((machineid>>self->machineid_num_bits)==0);

   for (;;) {
      uint64_t current_state = ijsf_atomic_load64(snowflake_state);
      uint64_t timestamp_state = (current_state&state_timestamp_mask);
      if (timestamp_now_state > timestamp_state) {
         /* Current time is newer than stored state, try to advance time. */
         if (ijsf_atomic_cas64_acquire(snowflake_state, timestamp_now_state, current_state)) {
            /* Successfully advanced the timestamp. create snowflake from timestamp
               and machineid as this is now the first sequence of this timestamp. */
            snowid = ijsnowflakeid__from_state(timestamp_now_state, state_timestamp_mask,
               num_unused_bits, num_sequence_bits, machineid);

            IJSF_assert((timestamp_now_state>>timeshift)==(uint64_t)ijsnowflakeid_timestamp(self, snowid));
            IJSF_assert(machineid==ijsnowflakeid_machineid(self, snowid));
            IJSF_assert(0==ijsnowflakeid_sequenceid(self, snowid));

            return snowid;
         }
         /* Failed to swap, lets continue using that timestamp instead.
            NB: It is possible that the one that successfully completed the swap did so with
            an older timestamp, but the most important feature of (this) snowflake is the
            increasing timestamp and unique ids, not the absolute order. */
      }

      /* add one to the sequence id and check if it overflowed */
      new_state = ijsf_atomic_add64(snowflake_state, 1);
      if (IJSF_UNLIKELY(new_state&overflow_mask)) {
         /* All sequences of the current timestamp have been used up.

         We can now either wait for `ijsnowflakeid__current_time_ms` to advance
         `timeunit_ms` so we can try to swap the current timestamp, or we can
         bypass the wait by advancing time ourselves.

         We simply advance time by `timeunit_ms` and try to swap in the next loop.
         This approach ensures that the snowflake generation does not act as a
         rate limiter and should be a very unusual case.

         NB: This means that if we encounter many
         'overflow->"cheat timestamp"->overflow->"cheat timestamp"' loops,
         we will move forward "into the future". However, since we never allow going
         back in time, we still guarantee unique snowflakes. The system will correct
         itself once the current surge in snowflake generation slows down, and real
         time catches up with the artificially advanced time.

         Now artificially advance time by 1 `timeunit_ms` and try to swap in the next iteration... */
         timestamp_now_state = (new_state&state_timestamp_mask)+(1ull<<timeshift);
         continue;
      }

      /* Did not overflow the sequence ids, make a snowflake from this. */
      snowid = ijsnowflakeid__from_state(new_state, state_timestamp_mask,
         num_unused_bits, num_sequence_bits, machineid);

      IJSF_assert((new_state>>timeshift)==(uint64_t)ijsnowflakeid_timestamp(self, snowid));
      IJSF_assert(machineid==ijsnowflakeid_machineid(self, snowid));
      IJSF_assert((uint32_t)(new_state&~state_timestamp_mask)==ijsnowflakeid_sequenceid(self, snowid));

      return snowid;
   }
}

#if IJSF_PLATFORM_WINDOWS
   #pragma warning(pop)
#endif

#endif /* IJSNOWFLAKEID_IMPLEMENTATION */

/*
LICENSE
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - 3-Clause BSD License
Copyright (c) 2023-, Fredrik Engkvist
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
* Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.
* Neither the name of the copyright holder nor the
names of its contributors may be used to endorse or promote products
derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDER BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/
/* clang-format on */
