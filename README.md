# ijsnowflakeid

`ijsnowflakeid` is a bit layout configurable, lock-free, and thread-safe implementation of [Twitter's Snowflake ID](https://en.wikipedia.org/wiki/Snowflake_ID) or any UUID that adopts a similar approach but varies in bit assignments within the UUID. Its configurable bit layout allows users to fully utilize all 64 bits (as opposed to Twitter's Snowflake ID which employs only 63 bits) or to customize their bit layout for timestamp, machine, and sequence identifiers.

## Generate UUIDs example

```c
void snowflake_gen(void) {
   int32_t timestamp_num_bits = 41;
   int32_t machineid_num_bits = 10;
   int32_t sequence_num_bits = 12;
   int32_t machineid = 3; /* optional, will be used with `ijsnowflakeid_generate` */
   void *snowmem = malloc(IJSNOWFLAKEID_MEMORY_SIZE);
   ijsnowflakeid *snow = ijsnowflakeid_init(snowmem, timestamp_num_bits, machineid_num_bits, sequence_num_bits, machineid);

   int64_t snowflakes[2];

   /* generate a unique id, with provided machineid */
   snowflakes[0] = ijsnowflakeid_generate_id(snow, 7);
   ASSERT(ijsnowflakeid_machineid(snow, snowflakes[0]) == 7);

   /* generate a unique id, with machineid of snow */
   snowflakes[1] = ijsnowflakeid_generate(snow);
   ASSERT(ijsnowflakeid_machineid(snow, snowflakes[1]) == 3);

   /* last snowflake is guaranteed to have same or later timestamp as it happened after */
   ASSERT(ijsnowflakeid_timestamp(snow, snowflakes[1]) >= ijsnowflakeid_timestamp(snow, snowflakes[0]));

   /* deallocate the snowflakeid when you are done... or not  */
   free(snow);
}
```

## Licence

Dual licensed under 3-Clause BSD and unlicense, choose whichever you prefer.