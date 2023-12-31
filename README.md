# ijsnowflakeid

`ijsnowflakeid` is a bit layout configurable, lock-free, and thread-safe implementation of [Twitter's Snowflake ID](https://en.wikipedia.org/wiki/Snowflake_ID) or any UUID that adopts a similar approach but varies in bit assignments within the UUID. Its configurable bit layout allows users to fully utilize all 64 bits (as opposed to Twitter's Snowflake ID which employs only 63 bits) or to customize their bit layout for timestamp, machine, and sequence identifiers.

Average less than 22ns/snowflake (>45500000 snowflakes per second) with Twitter Snowflake ID configuration when concurrently generating snowflakes on 24 threads on a 16 core, 24 threads Intel i9-12900k, easily outperforming the sequence number bits allotted per time unit.

[Try it online here.](https://godbolt.org/z/8rqbh38Kh)

## Generate UUIDs example

```c
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
```

## Licence

Dual licensed under 3-Clause BSD and unlicense, choose whichever you prefer.