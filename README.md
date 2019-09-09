# redislib

《Redis大全》
《Redis——数据结构与算法》
《Redis——（应用）从入门到精通》
《Redis——（改造）从入门到精通》

Redis 是一个开源（BSD许可）的，内存中的数据结构存储系统，它可以用作数据库、缓存和消息中间件。 它支持多种类型的数据结构，如 字符串（strings）， 散列（hashes）， 列表（lists）， 集合（sets）， 有序集合（sorted sets） 与范围查询， 位映射（bitmaps）， hyperloglogs 和 地理空间（geospatial） 索引半径查询。 Redis 内置了 复制（replication），LUA脚本（Lua scripting）， LRU驱动事件（LRU eviction），事务（transactions） 和不同级别的 磁盘持久化（persistence）， 并通过 Redis哨兵（Sentinel）和自动 分区（Cluster）提供高可用性（high availability）。

Redis并不是简单的key-value存储，实际上他是一个数据结构服务器，支持不同类型的值。也就是说，你不必仅仅把字符串当作键所指向的值。
下列这些数据类型都可作为值类型：
String: 二进制安全的字符串
Lists: 按插入顺序排序的字符串元素的集合。他们基本上就是链表（linked lists）。
Sets: 不重复且无序的字符串元素的集合。
Sorted sets: 类似Sets,但是每个字符串元素都关联到一个叫score浮动数值（floating number value）。里面的元素总是通过score进行着排序，所以不同的是，它是可以检索的一系列元素。（例如你可能会问：给我前面10个或者后面10个元素）。
Hash: 由field和关联的value组成的map。field和value都是字符串的。这和Ruby、Python的hashes很像。
Bit arrays (或者说 simply bitmaps): 通过特殊的命令，你可以将 String 值当作一系列 bits 处理：可以设置和清除单独的 bits，数出所有设为 1 的 bits 的数量，找到最前的被设为 1 或 0 的 bit，等等。
HyperLogLogs: 这是被用于估计一个 set 中元素数量的概率性的数据结构。别害怕，它比看起来的样子要简单…参见本教程的 HyperLogLog 部分。D
学习这些数据类型的原理，以及如何使用它们解决 command reference 中的特定问题，并不总是不关紧要的。所以，本文档是一个关于 Redis 数据类型和它们最常见特性的导论。 在所有的例子中，我们将使用 redis-cli 工具。它是一个简单而有用的命令行工具，用于向 Redis 服务器发出命令。
地理空间（geospatial）: 将指定的地理空间位置（纬度、经度、名称）添加到指定的key中。这些数据将会存储到sorted set这样的目的是为了方便使用GEORADIUS或者GEORADIUSBYMEMBER命令对数据进行半径查询等操作。
Redis keys
Redis key值是二进制安全的，这意味着可以用任何二进制序列作为key值，从形如”foo”的简单字符串到一个JPEG文件的内容都可以。空字符串也是有效key值。
关于key的几条规则：
太长的键值不是个好主意，例如1024字节的键值就不是个好主意，不仅因为消耗内存，而且在数据中查找这类键值的计算成本很高。
太短的键值通常也不是好主意，如果你要用”u:1000:pwd”来代替”user:1000:password”，这没有什么问题，但后者更易阅读，并且由此增加的空间消耗相对于key object和value object本身来说很小。当然，没人阻止您一定要用更短的键值节省一丁点儿空间。
最好坚持一种模式。例如：”object-type:id:field”就是个不错的注意，像这样”user:1000:password”。我喜欢对多单词的字段名中加上一个点，就像这样：”comment:1234:reply.to”。


所有redis命令，都会转成redis内部redisCommand:
struct redisCommand {
    char *name; /*命令名字，如：get set*/
    redisCommandProc *proc; /*命令处理函数*/
    int arity; /*是命令的参数数量限制（命令接收的参数个数），-2表示至少2位的可变参数*/
    char *sflags; /* 字符串表示的标志，一个字符表示一个标志. */
    int flags;    /* 实际标志，通过字符串标志转换过来的*/
    /* 该函数被用来确定一行命令键参数.
     * 被用来做Redis重定向. */
    redisGetKeysProc *getkeys_proc;
    /* 当这个命令被调用时，哪些键需要后台载入? */
    int firstkey; /* 第一个关键参数 (0 = 无关键参数) */
    int lastkey;  /* 最后一个关键参数 */
    int keystep;  /* 第一个和最后一个关键参数之间的步长 */
    long long microseconds, calls; /*调用时间和次数累计*/
};

/* Our command table.
 *
 * Every entry is composed of the following fields:
 *
 * name: a string representing the command name.
 * function: pointer to the C function implementing the command.
 * arity: number of arguments, it is possible to use -N to say >= N
 * sflags: command flags as string. See below for a table of flags.
 * flags: flags as bitmask. Computed by Redis using the 'sflags' field.
 * get_keys_proc: an optional function to get key arguments from a command.
 *                This is only used when the following three fields are not
 *                enough to specify what arguments are keys.
 * first_key_index: first argument that is a key
 * last_key_index: last argument that is a key
 * key_step: step to get all the keys from first to last argument. For instance
 *           in MSET the step is two since arguments are key,val,key,val,...
 * microseconds: microseconds of total execution time for this command.
 * calls: total number of calls of this command.
 *
 * The flags, microseconds and calls fields are computed by Redis and should
 * always be set to zero.
 *
 * Command flags are expressed using strings where every character represents
 * a flag. Later the populateCommandTable() function will take care of
 * populating the real 'flags' field using this characters.
 *
 * This is the meaning of the flags:
 *
 * w: write command (may modify the key space).
 * r: read command  (will never modify the key space).
 * m: may increase memory usage once called. Don't allow if out of memory.
 * a: admin command, like SAVE or SHUTDOWN.
 * p: Pub/Sub related command.
 * f: force replication of this command, regardless of server.dirty.
 * s: command not allowed in scripts.
 * R: random command. Command is not deterministic, that is, the same command
 *    with the same arguments, with the same key space, may have different
 *    results. For instance SPOP and RANDOMKEY are two random commands.
 * S: Sort command output array if called from script, so that the output
 *    is deterministic.
 * l: Allow command while loading the database.
 * t: Allow command while a slave has stale data but is not allowed to
 *    server this data. Normally no command is accepted in this condition
 *    but just a few.
 * M: Do not automatically propagate the command on MONITOR.
 * k: Perform an implicit ASKING for this command, so the command will be
 *    accepted in cluster mode if the slot is marked as 'importing'.
 * F: Fast command: O(1) or O(log(N)) command that should never delay
 *    its execution as long as the kernel scheduler is giving us time.
 *    Note that commands that may trigger a DEL as a side effect (like SET)
 *    are not fast commands.
 */
struct redisCommand redisCommandTable[] = {
    {"module",moduleCommand,-2,"as",0,NULL,0,0,0,0,0},
    {"get",getCommand,2,"rF",0,NULL,1,1,1,0,0},
    {"set",setCommand,-3,"wm",0,NULL,1,1,1,0,0},
    {"setnx",setnxCommand,3,"wmF",0,NULL,1,1,1,0,0},
    {"setex",setexCommand,4,"wm",0,NULL,1,1,1,0,0},
    {"psetex",psetexCommand,4,"wm",0,NULL,1,1,1,0,0},
    {"append",appendCommand,3,"wm",0,NULL,1,1,1,0,0},
    {"strlen",strlenCommand,2,"rF",0,NULL,1,1,1,0,0},
    {"del",delCommand,-2,"w",0,NULL,1,-1,1,0,0},
    {"unlink",unlinkCommand,-2,"wF",0,NULL,1,-1,1,0,0},
    {"exists",existsCommand,-2,"rF",0,NULL,1,-1,1,0,0},
    {"setbit",setbitCommand,4,"wm",0,NULL,1,1,1,0,0},
    {"getbit",getbitCommand,3,"rF",0,NULL,1,1,1,0,0},
    {"bitfield",bitfieldCommand,-2,"wm",0,NULL,1,1,1,0,0},
    {"setrange",setrangeCommand,4,"wm",0,NULL,1,1,1,0,0},
    {"getrange",getrangeCommand,4,"r",0,NULL,1,1,1,0,0},
    {"substr",getrangeCommand,4,"r",0,NULL,1,1,1,0,0},
    {"incr",incrCommand,2,"wmF",0,NULL,1,1,1,0,0},
    {"decr",decrCommand,2,"wmF",0,NULL,1,1,1,0,0},
    {"mget",mgetCommand,-2,"rF",0,NULL,1,-1,1,0,0},
    {"rpush",rpushCommand,-3,"wmF",0,NULL,1,1,1,0,0},
    {"lpush",lpushCommand,-3,"wmF",0,NULL,1,1,1,0,0},
    {"rpushx",rpushxCommand,-3,"wmF",0,NULL,1,1,1,0,0},
    {"lpushx",lpushxCommand,-3,"wmF",0,NULL,1,1,1,0,0},
    {"linsert",linsertCommand,5,"wm",0,NULL,1,1,1,0,0},
    {"rpop",rpopCommand,2,"wF",0,NULL,1,1,1,0,0},
    {"lpop",lpopCommand,2,"wF",0,NULL,1,1,1,0,0},
    {"brpop",brpopCommand,-3,"ws",0,NULL,1,-2,1,0,0},
    {"brpoplpush",brpoplpushCommand,4,"wms",0,NULL,1,2,1,0,0},
    {"blpop",blpopCommand,-3,"ws",0,NULL,1,-2,1,0,0},
    {"llen",llenCommand,2,"rF",0,NULL,1,1,1,0,0},
    {"lindex",lindexCommand,3,"r",0,NULL,1,1,1,0,0},
    {"lset",lsetCommand,4,"wm",0,NULL,1,1,1,0,0},
    {"lrange",lrangeCommand,4,"r",0,NULL,1,1,1,0,0},
    {"ltrim",ltrimCommand,4,"w",0,NULL,1,1,1,0,0},
    {"lrem",lremCommand,4,"w",0,NULL,1,1,1,0,0},
    {"rpoplpush",rpoplpushCommand,3,"wm",0,NULL,1,2,1,0,0},
    {"sadd",saddCommand,-3,"wmF",0,NULL,1,1,1,0,0},
    {"srem",sremCommand,-3,"wF",0,NULL,1,1,1,0,0},
    {"smove",smoveCommand,4,"wF",0,NULL,1,2,1,0,0},
    {"sismember",sismemberCommand,3,"rF",0,NULL,1,1,1,0,0},
    {"scard",scardCommand,2,"rF",0,NULL,1,1,1,0,0},
    {"spop",spopCommand,-2,"wRF",0,NULL,1,1,1,0,0},
    {"srandmember",srandmemberCommand,-2,"rR",0,NULL,1,1,1,0,0},
    {"sinter",sinterCommand,-2,"rS",0,NULL,1,-1,1,0,0},
    {"sinterstore",sinterstoreCommand,-3,"wm",0,NULL,1,-1,1,0,0},
    {"sunion",sunionCommand,-2,"rS",0,NULL,1,-1,1,0,0},
    {"sunionstore",sunionstoreCommand,-3,"wm",0,NULL,1,-1,1,0,0},
    {"sdiff",sdiffCommand,-2,"rS",0,NULL,1,-1,1,0,0},
    {"sdiffstore",sdiffstoreCommand,-3,"wm",0,NULL,1,-1,1,0,0},
    {"smembers",sinterCommand,2,"rS",0,NULL,1,1,1,0,0},
    {"sscan",sscanCommand,-3,"rR",0,NULL,1,1,1,0,0},
    {"zadd",zaddCommand,-4,"wmF",0,NULL,1,1,1,0,0},
    {"zincrby",zincrbyCommand,4,"wmF",0,NULL,1,1,1,0,0},
    {"zrem",zremCommand,-3,"wF",0,NULL,1,1,1,0,0},
    {"zremrangebyscore",zremrangebyscoreCommand,4,"w",0,NULL,1,1,1,0,0},
    {"zremrangebyrank",zremrangebyrankCommand,4,"w",0,NULL,1,1,1,0,0},
    {"zremrangebylex",zremrangebylexCommand,4,"w",0,NULL,1,1,1,0,0},
    {"zunionstore",zunionstoreCommand,-4,"wm",0,zunionInterGetKeys,0,0,0,0,0},
    {"zinterstore",zinterstoreCommand,-4,"wm",0,zunionInterGetKeys,0,0,0,0,0},
    {"zrange",zrangeCommand,-4,"r",0,NULL,1,1,1,0,0},
    {"zrangebyscore",zrangebyscoreCommand,-4,"r",0,NULL,1,1,1,0,0},
    {"zrevrangebyscore",zrevrangebyscoreCommand,-4,"r",0,NULL,1,1,1,0,0},
    {"zrangebylex",zrangebylexCommand,-4,"r",0,NULL,1,1,1,0,0},
    {"zrevrangebylex",zrevrangebylexCommand,-4,"r",0,NULL,1,1,1,0,0},
    {"zcount",zcountCommand,4,"rF",0,NULL,1,1,1,0,0},
    {"zlexcount",zlexcountCommand,4,"rF",0,NULL,1,1,1,0,0},
    {"zrevrange",zrevrangeCommand,-4,"r",0,NULL,1,1,1,0,0},
    {"zcard",zcardCommand,2,"rF",0,NULL,1,1,1,0,0},
    {"zscore",zscoreCommand,3,"rF",0,NULL,1,1,1,0,0},
    {"zrank",zrankCommand,3,"rF",0,NULL,1,1,1,0,0},
    {"zrevrank",zrevrankCommand,3,"rF",0,NULL,1,1,1,0,0},
    {"zscan",zscanCommand,-3,"rR",0,NULL,1,1,1,0,0},
    {"zpopmin",zpopminCommand,-2,"wF",0,NULL,1,1,1,0,0},
    {"zpopmax",zpopmaxCommand,-2,"wF",0,NULL,1,1,1,0,0},
    {"bzpopmin",bzpopminCommand,-3,"wsF",0,NULL,1,-2,1,0,0},
    {"bzpopmax",bzpopmaxCommand,-3,"wsF",0,NULL,1,-2,1,0,0},
    {"hset",hsetCommand,-4,"wmF",0,NULL,1,1,1,0,0},
    {"hsetnx",hsetnxCommand,4,"wmF",0,NULL,1,1,1,0,0},
    {"hget",hgetCommand,3,"rF",0,NULL,1,1,1,0,0},
    {"hmset",hsetCommand,-4,"wmF",0,NULL,1,1,1,0,0},
    {"hmget",hmgetCommand,-3,"rF",0,NULL,1,1,1,0,0},
    {"hincrby",hincrbyCommand,4,"wmF",0,NULL,1,1,1,0,0},
    {"hincrbyfloat",hincrbyfloatCommand,4,"wmF",0,NULL,1,1,1,0,0},
    {"hdel",hdelCommand,-3,"wF",0,NULL,1,1,1,0,0},
    {"hlen",hlenCommand,2,"rF",0,NULL,1,1,1,0,0},
    {"hstrlen",hstrlenCommand,3,"rF",0,NULL,1,1,1,0,0},
    {"hkeys",hkeysCommand,2,"rS",0,NULL,1,1,1,0,0},
    {"hvals",hvalsCommand,2,"rS",0,NULL,1,1,1,0,0},
    {"hgetall",hgetallCommand,2,"rR",0,NULL,1,1,1,0,0},
    {"hexists",hexistsCommand,3,"rF",0,NULL,1,1,1,0,0},
    {"hscan",hscanCommand,-3,"rR",0,NULL,1,1,1,0,0},
    {"incrby",incrbyCommand,3,"wmF",0,NULL,1,1,1,0,0},
    {"decrby",decrbyCommand,3,"wmF",0,NULL,1,1,1,0,0},
    {"incrbyfloat",incrbyfloatCommand,3,"wmF",0,NULL,1,1,1,0,0},
    {"getset",getsetCommand,3,"wm",0,NULL,1,1,1,0,0},
    {"mset",msetCommand,-3,"wm",0,NULL,1,-1,2,0,0},
    {"msetnx",msetnxCommand,-3,"wm",0,NULL,1,-1,2,0,0},
    {"randomkey",randomkeyCommand,1,"rR",0,NULL,0,0,0,0,0},
    {"select",selectCommand,2,"lF",0,NULL,0,0,0,0,0},
    {"swapdb",swapdbCommand,3,"wF",0,NULL,0,0,0,0,0},
    {"move",moveCommand,3,"wF",0,NULL,1,1,1,0,0},
    {"rename",renameCommand,3,"w",0,NULL,1,2,1,0,0},
    {"renamenx",renamenxCommand,3,"wF",0,NULL,1,2,1,0,0},
    {"expire",expireCommand,3,"wF",0,NULL,1,1,1,0,0},
    {"expireat",expireatCommand,3,"wF",0,NULL,1,1,1,0,0},
    {"pexpire",pexpireCommand,3,"wF",0,NULL,1,1,1,0,0},
    {"pexpireat",pexpireatCommand,3,"wF",0,NULL,1,1,1,0,0},
    {"keys",keysCommand,2,"rS",0,NULL,0,0,0,0,0},
    {"scan",scanCommand,-2,"rR",0,NULL,0,0,0,0,0},
    {"dbsize",dbsizeCommand,1,"rF",0,NULL,0,0,0,0,0},
    {"auth",authCommand,2,"sltF",0,NULL,0,0,0,0,0},
    {"ping",pingCommand,-1,"tF",0,NULL,0,0,0,0,0},
    {"echo",echoCommand,2,"F",0,NULL,0,0,0,0,0},
    {"save",saveCommand,1,"as",0,NULL,0,0,0,0,0},
    {"bgsave",bgsaveCommand,-1,"as",0,NULL,0,0,0,0,0},
    {"bgrewriteaof",bgrewriteaofCommand,1,"as",0,NULL,0,0,0,0,0},
    {"shutdown",shutdownCommand,-1,"aslt",0,NULL,0,0,0,0,0},
    {"lastsave",lastsaveCommand,1,"RF",0,NULL,0,0,0,0,0},
    {"type",typeCommand,2,"rF",0,NULL,1,1,1,0,0},
    {"multi",multiCommand,1,"sF",0,NULL,0,0,0,0,0},
    {"exec",execCommand,1,"sM",0,NULL,0,0,0,0,0},
    {"discard",discardCommand,1,"sF",0,NULL,0,0,0,0,0},
    {"sync",syncCommand,1,"ars",0,NULL,0,0,0,0,0},
    {"psync",syncCommand,3,"ars",0,NULL,0,0,0,0,0},
    {"replconf",replconfCommand,-1,"aslt",0,NULL,0,0,0,0,0},
    {"flushdb",flushdbCommand,-1,"w",0,NULL,0,0,0,0,0},
    {"flushall",flushallCommand,-1,"w",0,NULL,0,0,0,0,0},
    {"sort",sortCommand,-2,"wm",0,sortGetKeys,1,1,1,0,0},
    {"info",infoCommand,-1,"ltR",0,NULL,0,0,0,0,0},
    {"monitor",monitorCommand,1,"as",0,NULL,0,0,0,0,0},
    {"ttl",ttlCommand,2,"rFR",0,NULL,1,1,1,0,0},
    {"touch",touchCommand,-2,"rF",0,NULL,1,1,1,0,0},
    {"pttl",pttlCommand,2,"rFR",0,NULL,1,1,1,0,0},
    {"persist",persistCommand,2,"wF",0,NULL,1,1,1,0,0},
    {"slaveof",replicaofCommand,3,"ast",0,NULL,0,0,0,0,0},
    {"replicaof",replicaofCommand,3,"ast",0,NULL,0,0,0,0,0},
    {"role",roleCommand,1,"lst",0,NULL,0,0,0,0,0},
    {"debug",debugCommand,-2,"as",0,NULL,0,0,0,0,0},
    {"config",configCommand,-2,"last",0,NULL,0,0,0,0,0},
    {"subscribe",subscribeCommand,-2,"pslt",0,NULL,0,0,0,0,0},
    {"unsubscribe",unsubscribeCommand,-1,"pslt",0,NULL,0,0,0,0,0},
    {"psubscribe",psubscribeCommand,-2,"pslt",0,NULL,0,0,0,0,0},
    {"punsubscribe",punsubscribeCommand,-1,"pslt",0,NULL,0,0,0,0,0},
    {"publish",publishCommand,3,"pltF",0,NULL,0,0,0,0,0},
    {"pubsub",pubsubCommand,-2,"pltR",0,NULL,0,0,0,0,0},
    {"watch",watchCommand,-2,"sF",0,NULL,1,-1,1,0,0},
    {"unwatch",unwatchCommand,1,"sF",0,NULL,0,0,0,0,0},
    {"cluster",clusterCommand,-2,"a",0,NULL,0,0,0,0,0},
    {"restore",restoreCommand,-4,"wm",0,NULL,1,1,1,0,0},
    {"restore-asking",restoreCommand,-4,"wmk",0,NULL,1,1,1,0,0},
    {"migrate",migrateCommand,-6,"wR",0,migrateGetKeys,0,0,0,0,0},
    {"asking",askingCommand,1,"F",0,NULL,0,0,0,0,0},
    {"readonly",readonlyCommand,1,"F",0,NULL,0,0,0,0,0},
    {"readwrite",readwriteCommand,1,"F",0,NULL,0,0,0,0,0},
    {"dump",dumpCommand,2,"rR",0,NULL,1,1,1,0,0},
    {"object",objectCommand,-2,"rR",0,NULL,2,2,1,0,0},
    {"memory",memoryCommand,-2,"rR",0,NULL,0,0,0,0,0},
    {"client",clientCommand,-2,"as",0,NULL,0,0,0,0,0},
    {"eval",evalCommand,-3,"s",0,evalGetKeys,0,0,0,0,0},
    {"evalsha",evalShaCommand,-3,"s",0,evalGetKeys,0,0,0,0,0},
    {"slowlog",slowlogCommand,-2,"aR",0,NULL,0,0,0,0,0},
    {"script",scriptCommand,-2,"s",0,NULL,0,0,0,0,0},
    {"time",timeCommand,1,"RF",0,NULL,0,0,0,0,0},
    {"bitop",bitopCommand,-4,"wm",0,NULL,2,-1,1,0,0},
    {"bitcount",bitcountCommand,-2,"r",0,NULL,1,1,1,0,0},
    {"bitpos",bitposCommand,-3,"r",0,NULL,1,1,1,0,0},
    {"wait",waitCommand,3,"s",0,NULL,0,0,0,0,0},
    {"command",commandCommand,0,"ltR",0,NULL,0,0,0,0,0},
    {"geoadd",geoaddCommand,-5,"wm",0,NULL,1,1,1,0,0},
    {"georadius",georadiusCommand,-6,"w",0,georadiusGetKeys,1,1,1,0,0},
    {"georadius_ro",georadiusroCommand,-6,"r",0,georadiusGetKeys,1,1,1,0,0},
    {"georadiusbymember",georadiusbymemberCommand,-5,"w",0,georadiusGetKeys,1,1,1,0,0},
    {"georadiusbymember_ro",georadiusbymemberroCommand,-5,"r",0,georadiusGetKeys,1,1,1,0,0},
    {"geohash",geohashCommand,-2,"r",0,NULL,1,1,1,0,0},
    {"geopos",geoposCommand,-2,"r",0,NULL,1,1,1,0,0},
    {"geodist",geodistCommand,-4,"r",0,NULL,1,1,1,0,0},
    {"pfselftest",pfselftestCommand,1,"a",0,NULL,0,0,0,0,0},
    {"pfadd",pfaddCommand,-2,"wmF",0,NULL,1,1,1,0,0},
    {"pfcount",pfcountCommand,-2,"r",0,NULL,1,-1,1,0,0},
    {"pfmerge",pfmergeCommand,-2,"wm",0,NULL,1,-1,1,0,0},
    {"pfdebug",pfdebugCommand,-3,"w",0,NULL,0,0,0,0,0},
    {"xadd",xaddCommand,-5,"wmFR",0,NULL,1,1,1,0,0},
    {"xrange",xrangeCommand,-4,"r",0,NULL,1,1,1,0,0},
    {"xrevrange",xrevrangeCommand,-4,"r",0,NULL,1,1,1,0,0},
    {"xlen",xlenCommand,2,"rF",0,NULL,1,1,1,0,0},
    {"xread",xreadCommand,-4,"rs",0,xreadGetKeys,1,1,1,0,0},
    {"xreadgroup",xreadCommand,-7,"ws",0,xreadGetKeys,1,1,1,0,0},
    {"xgroup",xgroupCommand,-2,"wm",0,NULL,2,2,1,0,0},
    {"xsetid",xsetidCommand,3,"wmF",0,NULL,1,1,1,0,0},
    {"xack",xackCommand,-4,"wF",0,NULL,1,1,1,0,0},
    {"xpending",xpendingCommand,-3,"rR",0,NULL,1,1,1,0,0},
    {"xclaim",xclaimCommand,-6,"wRF",0,NULL,1,1,1,0,0},
    {"xinfo",xinfoCommand,-2,"rR",0,NULL,2,2,1,0,0},
    {"xdel",xdelCommand,-3,"wF",0,NULL,1,1,1,0,0},
    {"xtrim",xtrimCommand,-2,"wFR",0,NULL,1,1,1,0,0},
    {"post",securityWarningCommand,-1,"lt",0,NULL,0,0,0,0,0},
    {"host:",securityWarningCommand,-1,"lt",0,NULL,0,0,0,0,0},
    {"latency",latencyCommand,-2,"aslt",0,NULL,0,0,0,0,0},
    {"lolwut",lolwutCommand,-1,"r",0,NULL,0,0,0,0,0}
};

在具体描述这几种数据类型之前，我们先通过一张图了解下 Redis 内部内存管理中是如何描述这些不同数据类型的：





首先 Redis 内部使用一个 redisObject 对象来表示所有的 key 和 value，redisObject 最主要的信息如上图所示。type 代表一个 value 对象具体是何种数据类型，encoding是不同数据类型在Redis内部的存储方式，比如：type=string 代表 value 存储的是一个普通字符串，那么对应的 encoding 可以是 raw 或者是 int，如果是 int 则代表实际 Redis 内部是按数值型类存储和表示这个字符串的，当然前提是这个字符串本身可以用数值表示，比如："123" "456"这样的字符串。



这里需要特殊说明一下 vm 字段，只有打开了 Redis 的虚拟内存功能，此字段才会真正的分配内存，该功能默认是关闭状态的，该功能会在后面具体描述。



通过上图我们可以发现 ，Redis 使用 redisObject 来表示所有的 key/value 数据是比较浪费内存的，当然这些内存管理成本的付出主要也是为了给 Redis 不同数据类型提供一个统一的管理接口，实际作者也提供了多种方法帮助我们尽量节省内存使用，随后会具体讨论。


下面我们先逐一分析Redis数据类型的使用和内部实现方式：
String
常用命令：
Set、get、decr、incr、mget等。

应用场景：
String 是最常用的一种数据类型，普通的 key/value 存储都可以归为此类，这里就不所做解释了。

实现方式：
String 在 Redis 内部存储默认就是一个字符串，被 redisObject 所引用，当遇到 incr、decr 等操作时会转成数值型进行计算，此时 redisObject 的 encoding 字段为int。
关于String的更详细的讲解请阅读《Redis——String详解》


Hash
常用命令：
Hget、hset、hgetall 等。

应用场景：
我们简单举个实例来描述下 Hash 的应用场景，比如我们要存储一个用户信息对象数据，包含以下信息：

用户 ID 为查找的 key，存储的 value 用户对象包含姓名，年龄，生日等信息，如果用普通的 key/value 结构来存储，主要有以下2种存储方式：







第一种方式将用户 ID 作为查找 key，把其它信息封装成一个对象以序列化的方式存储，这种方式的缺点是，增加了序列化/反序列化的开销，并且在需要修改其中一项信息时，需要把整个对象取回，并且修改操作需要对并发进行保护，引入CAS等复杂问题。







第二种方法是这个用户信息对象有多少成员就存成多少个 key-value 对儿，用用户 ID +对应属性的名称作为唯一标识来取得对应属性的值，虽然省去了序列化开销和并发问题，但是用户 ID 为重复存储，如果存在大量这样的数据，内存浪费还是非常可观的。



那么 Redis 提供的 Hash 很好地解决了这个问题，Redis 的 Hash 实际是内部存储的 Value 为一个 HashMap，并提供了直接存取这个 Map 成员的接口，如下图：







也就是说，Key 仍然是用户 ID，value 是一个 Map，这个 Map 的 key 是成员的属性名，value 是属性值，这样对数据的修改和存取都可以直接通过其内部 Map 的 Key（Redis 里称内部 Map 的 key 为 field），也就是通过 key（用户 ID） + field（属性标签）就可以操作对应属性数据了，既不需要重复存储数据，也不会带来序列化和并发修改控制的问题，很好地解决了问题。



这里同时需要注意，Redis 提供了接口（hgetall）可以直接取到全部的属性数据，但是如果内部 Map 的成员很多，那么涉及到遍历整个内部 Map 的操作，由于 Redis 单线程模型的缘故，这个遍历操作可能会比较耗时，而另其它客户端的请求完全不响应，这点需要格外注意。



实现方式：
上面已经说到 Redis Hash 对应 Value 内部实际就是一个 HashMap，实际这里会有2种不同实现，这个 Hash 的成员比较少时 Redis 为了节省内存会采用类似一维数组的方式来紧凑存储，而不会采用真正的 HashMap 结构，对应的 value redisObject 的 encoding 为 zipmap，当成员数量增大时会自动转成真正的 HashMap，此时 encoding 为 ht。
关于Hash的更详细的讲解请阅读《Redis——Hash详解》

List
常用命令：
Lpush、rpush、lpop、rpop、lrange等。

应用场景：
Redis list 的应用场景非常多，也是 Redis 最重要的数据结构之一，比如 twitter 的关注列表，粉丝列表等都可以用 Redis 的 list 结构来实现，比较好理解，这里不再重复。

实现方式：
Redis list 的实现为一个双向链表，即可以支持反向查找和遍历，更方便操作，不过带来了部分额外的内存开销，Redis 内部的很多实现，包括发送缓冲队列等也都是用的这个数据结构。
关于List的更详细的讲解请阅读《Redis——List详解》


Set
常用命令：
Sadd、spop、smembers、sunion 等。

应用场景：
Redis set 对外提供的功能与 list 类似是一个列表的功能，特殊之处在于 set 是可以自动排重的，当你需要存储一个列表数据，又不希望出现重复数据时，set 是一个很好的选择，并且 set 提供了判断某个成员是否在一个 set 集合内的重要接口，这个也是 list 所不能提供的。

实现方式：
set 的内部实现是一个 value 永远为 null 的 HashMap，实际就是通过计算 hash 的方式来快速排重的，这也是 set 能提供判断一个成员是否在集合内的原因。
关于Set的更详细的讲解请阅读《Redis——Set详解》


Sorted set
常用命令：
zadd、zrange、zrem、zcard等。



使用场景：

Redis sorted set的使用场景与 set 类似，区别是set不是自动有序的，而 sorted set 可以通过用户额外提供一个优先级（score）的参数来为成员排序，并且是插入有序的，即自动排序。当你需要一个有序的并且不重复的集合列表，那么可以选择 sorted set 数据结构，比如 twitter 的 public timeline 可以以发表时间作为 score 来存储，这样获取时就是自动按时间排好序的。



实现方式：

Redis sorted set 的内部使用 HashMap 和跳跃表（SkipList）来保证数据的存储和有序，HashMap 里放的是成员到 score 的映射，而跳跃表里存放的是所有的成员，排序依据是 HashMap 里存的 score，使用跳跃表的结构可以获得比较高的查找效率，并且在实现上比较简单。
关于Sorted set的更详细的讲解请阅读《Redis——Sorted set详解》


Bitmaps
常用命令：
setbit getbit、bitop、bitcount等。

使用场景：
Bitmaps

实现方式：
Bitmaps。
关于Bitmaps的更详细的讲解请阅读《Redis——Bitmaps详解》


HyperLogLogs
常用命令：
pfadd pfcount等。

使用场景：
HyperLogLogs

实现方式：
HyperLogLogs
关于HyperLogLogs的更详细的讲解请阅读《Redis——HyperLogLogs详解》

geospatial
常用命令：
geoadd geodist georadius等。

使用场景：
geospatial
该命令以采用标准格式的参数x,y,所以经度必须在纬度之前。这些坐标的限制是可以被编入索引的，区域面积可以很接近极点但是不能索引。具体的限制，由EPSG:900913 / EPSG:3785 / OSGEO:41001 规定如下：
有效的经度从-180度到180度。
有效的纬度从-85.05112878度到85.05112878度。
当坐标位置超出上述指定范围时，该命令将会返回一个错误。

实现方式：
sorted set使用一种称为Geohash的技术进行填充。经度和纬度的位是交错的，以形成一个独特的52位整数. 我们知道，一个sorted set 的double score可以代表一个52位的整数，而不会失去精度。
这种格式允许半径查询检查的1 + 8个领域需要覆盖整个半径，并丢弃元素以外的半径。通过计算该区域的范围，通过计算所涵盖的范围，从不太重要的部分的排序集的得分，并计算得分范围为每个区域的sorted set中的查询。
使用什么样的地球模型（Earth model）？
这只是假设地球是一个球体，因为使用的距离公式是Haversine公式。这个公式仅适用于地球，而不是一个完美的球体。当在社交网站和其他大多数需要查询半径的应用中使用时，这些偏差都不算问题。但是，在最坏的情况下的偏差可能是0.5%，所以一些地理位置很关键的应用还是需要谨慎考虑。
关于geospatial的更详细的讲解请阅读《Redis——geospatial详解》

常用内存优化手段与参数


通过我们上面的一些实现上的分析可以看出 Redis 实际上的内存管理成本非常高，即占用了过多的内存，作者对这点也非常清楚，所以提供了一系列的参数和手段来控制和节省内存，我们分别来讨论下。



首先最重要的一点是不要开启 Redis 的 VM 选项，即虚拟内存功能，这个本来是作为 Redis 存储超出物理内存数据的一种数据在内存与磁盘换入换出的一个持久化策略，但是其内存管理成本也非常的高，并且我们后续会分析此种持久化策略并不成熟，所以要关闭 VM 功能，请检查你的 redis.conf 文件中 vm-enabled 为 no。



其次最好设置下 redis.conf 中的 maxmemory 选项，该选项是告诉 Redis 当使用了多少物理内存后就开始拒绝后续的写入请求，该参数能很好地保护好你的 Redis 不会因为使用了过多的物理内存而导致 swap，最终严重影响性能甚至崩溃。



另外 Redis 为不同数据类型分别提供了一组参数来控制内存使用，我们在前面详细分析过 Redis Hash 是 value 内部为一个 HashMap，如果该 Map 的成员数比较少，则会采用类似一维线性的紧凑格式来存储该 Map，即省去了大量指针的内存开销，这个参数控制对应在 redis.conf 配置文件中下面2项：

hash-max-zipmap-entries 64

hash-max-zipmap-value 512

hash-max-zipmap-entries 含义是当 value 这个 Map 内部不超过多少个成员时会采用线性紧凑格式存储，默认是64，即 value 内部有64个以下的成员就是使用线性紧凑存储，超过该值自动转成真正的 HashMap。



hash-max-zipmap-value 含义是当 value 这个 Map 内部的每个成员值长度不超过多少字节就会采用线性紧凑存储来节省空间。



以上2个条件任意一个条件超过设置值都会转换成真正的 HashMap，也就不会再节省内存了，那么这个值是不是设置的越大越好呢，答案当然是否定的，HashMap 的优势就是查找和操作的时间复杂度都是 O(1) 的，而放弃 Hash 采用一维存储则是 O(n) 的时间复杂度，如果成员数量很少，则影响不大，否则会严重影响性能，所以要权衡好这个值的设置，总体上还是最根本的时间成本和空间成本上的权衡。



同样类似的参数还有：

list-max-ziplist-entries 512 说明：list 数据类型多少节点以下会采用去指针的紧凑存储格式。
list-max-ziplist-value 64 说明：list 数据类型节点值大小小于多少字节会采用紧凑存储格式。set-max-intset-entries 512


注：set 数据类型内部数据如果全部是数值型，且包含多少节点以下会采用紧凑格式存储。



最后想说的是Redis内部实现没有对内存分配方面做过多的优化，在一定程度上会存在内存碎片，不过大多数情况下这个不会成为Redis的性能瓶 颈，不过如果在Redis内部存储的大部分数据是数值型的话，Redis内部采用了一个 shared integer 的方式来省去分配内存的开销，即在系统启动时先分配一个从 1~n，那么多个数值对象放在一个池子中，如果存储的数据恰好是这个数值范围内的数据，则直接从池子里取出该对象，并且通过引用计数的方式来共享，这样在系统存储了大量数值下，也能一定程度上节省内存并且提高性能，这个参数值 n 的设置需要修改源代码中的一行宏定义 REDIS_SHARED_INTEGERS，该值 默认是 10000，可以根据自己的需要进行修改，修改后重新编译就可以了。



Redis 的持久化机制



Redis 由于支持非常丰富的内存数据结构类型，如何把这些复杂的内存组织方式持久化到磁盘上是一个难题，所以 Redis 的持久化方式与传统数据库的方式有比较多的差别，Redis 一共支持四种持久化方式，分别是：



定时快照方式（snapshot）

基于语句追加文件的方式（aof）

虚拟内存（vm）

Diskstore 方式



在设计思路上，前两种是基于全部数据都在内存中，即小数据量下提供磁盘落地功能，而后两种方式则是作者在尝试存储数据超过物理内存时，即大数据量的数据存储，截止到本文，后两种持久化方式仍然是在实验阶段，并且 vm 方式基本已经被作者放弃，所以实际能在生产环境用的只有前两种，换句话说 Redis 目前还只能作为小数据量存储（全部数据能够加载在内存中），海量数据存储方面并不是 Redis 所擅长的领域。下面分别介绍下这几种持久化方式：



定时快照方式（snapshot）：



该持久化方式实际是在 Redis 内部一个定时器事件，每隔固定时间去检查当前数据发生的改变次数与时间是否满足配置的持久化触发的条件，如果满足则通过操作系统 fork 调用来创建出一个子进程，这个子进程默认会与父进程共享相同的地址空间，这时就可以通过子进程来遍历整个内存来进行存储操作，而主进程则仍然可以提供服务，当有写入时由操作系统按照内存页（page）为单位来进行 copy-on-write 保证父子进程之间不会互相影响。



该持久化的主要缺点是定时快照只是代表一段时间内的内存映像，所以系统重启会丢失上次快照与重启之间所有的数据。



基于语句追加方式（aof）：



aof 方式实际类似MySQL基于语句的 binlog 方式，即每条会使 Redis 内存数据发生改变的命令都会追加到一个 log 文件中，也就是说这个 log 文件就是 Redis 的持久化数据。



aof 的方式的主要缺点是追加 log 文件可能导致体积过大，当系统重启恢复数据时如果是 aof 的方式则加载数据会非常慢，几十G的数据可能需要几小时才能加载完，当然这个耗时并不是因为磁盘文件读取速度慢，而是由于读取的所有命令都要在内存中执行一遍。另外由于每条命令都要写 log，所以使用 aof 的方式，Redis 的读写性能也会有所下降。



虚拟内存方式：



虚拟内存方式是 Redis 来进行用户空间的数据换入换出的一个策略，此种方式在实现的效果上比较差，主要问题是代码复杂、重启慢、复制慢等等，目前已经被作者放弃。



diskstore 方式：



diskstore 方式是作者放弃了虚拟内存方式后选择的一种新的实现方式，也就是传统的 B-tree 的方式，目前仍在实验阶段，后续是否可用我们可以拭目以待。



Redis持久化磁盘IO方式及其带来的问题



有 Redis 线上运维经验的人会发现 Redis 在物理内存使用比较多，但还没有超过实际物理内存总容量时就会发生不稳定甚至崩溃的问题，有人认为是基于快照方式持久化的 fork 系统调用造成内存占用加倍而导致的，这种观点是不准确的，因为 fork 调用的 copy-on-write 机制是基于操作系统页这个单位的，也就是只有有写入的脏页会被复制，但是一般你的系统不会在短时间内所有的页都发生了写入而导致复制，那是什么原因导致 Redis 崩溃呢？



答案是 Redis 的持久化使用了 Buffer IO 造成的，所谓 Buffer IO 是指 Redis 对持久化文件的写入和读取操作都会使用物理内存的 Page Cache，而大多数数据库系统会使用 Direct IO 来绕过这层 Page Cache 并自行维护一个数据的 Cache，而当 Redis 的持久化文件过大（尤其是快照文件），并对其进行读写时，磁盘文件中的数据都会被加载到物理内 存中作为操作系统对该文件的一层 Cache，而这层 Cache 的数据与 Redis 内存中管理的数据实际是重复存储的，虽然内核在物理内存紧张时会做 Page Cache 的剔除工作，但内核很可能认为某块 Page Cache 更重要，而让你的进程开始 Swap，这时你的系统就会开始出现不稳定或者崩溃了。我们的经验是当你的 Redis 物理内存使用超过内存总容量的3/5时就会开始比较危险了。



下图是 Redis 在读取或者写入快照文件 dump.rdb 后的内存数据图：







总结


根据业务需要选择合适的数据类型，并为不同的应用场景设置相应的紧凑存储参数。

当业务场景不需要数据持久化时，关闭所有的持久化方式可以获得最佳的性能以及最大的内存使用量。

如果需要使用持久化，根据是否可以容忍重启丢失部分数据在快照方式与语句追加方式之间选择其一，不要使用虚拟内存以及 diskstore 方式。

不要让你的 Redis 所在机器物理内存使用超过实际内存总量的3/5。