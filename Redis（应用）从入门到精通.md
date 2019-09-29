目录
目录
类型
Keys
键值不要太长
键值要正确的表达意思
坚持使用一种模式
值
字符串 (Strings)
说明
实例
基础设置
原子递增
查询和删除
超时时间
散列 (Hashes)
实例
完整命令
列表 (Lists)
数据的插入和查询
推出和删除数据
常用案例
集合 (Sets)
添加和查看
元素是否存在
适用场景
有序集合 (Sorted sets)
比较规则
实际例子
bitmaps
实例
说明
实例
推荐使用位图的场景
hyperloglogs
地理空间（geospatial）
它是如何工作的？
使用什么样的地球模型（Earth model）？
返回值
实际例子
目录索引
类型
数据类型介绍

你也许已经知道Redis并不是简单的 key-value 存储，实际上他是一个数据结构服务器，支持不同类型的值。 
也就是说，你不必仅仅把字符串当作键所指向的值。下列这些数据类型都可作为值类型：

二进制安全的字符串

Lists: 按插入顺序排序的字符串元素的集合。他们基本上就是链表（linked lists）。

Sets: 不重复且无序的字符串元素的集合。

Sorted sets,类似Sets,但是每个字符串元素都关联到一个叫score浮动数值（floating number value）。里面的元素总是通过score进行着排序，所以不同的是，它是可以检索的一系列元素。（例如你可能会问：给我前面10个或者后面10个元素）。

Hashes,由field和关联的value组成的map。field和value都是字符串的。这和Ruby、Python的hashes很像。

Bit arrays (或者说 simply bitmaps): 通过特殊的命令，你可以将 String 值当作一系列 bits 处理：可以设置和清除单独的 bits，数出所有设为 1 的 bits 的数量，找到最前的被设为 1 或 0 的 bit，等等。

HyperLogLogs: 这是被用于估计一个 set 中元素数量的概率性的数据结构。别害怕，它比看起来的样子要简单…参见本教程的 HyperLogLog 部分。D 
学习这些数据类型的原理，以及如何使用它们解决 command reference 中的特定问题，并不总是不关紧要的。所以，本文档是一个关于 Redis 数据类型和它们最常见特性的导论。 在所有的例子中，我们将使用 redis-cli 工具。它是一个简单而有用的命令行工具，用于向 Redis 服务器发出命令。

Keys
Redis key值是二进制安全的，这意味着可以用任何二进制序列作为key值，从形如”foo”的简单字符串到一个JPEG文件的内容都可以。空字符串也是有效key值。

关于key的几条规则：

键值不要太长
太长的键值不是个好主意，例如1024字节的键值就不是个好主意，不仅因为消耗内存，而且在数据中查找这类键值的计算成本很高。

键值要正确的表达意思
太短的键值通常也不是好主意，如果你要用”u:1000:pwd”来代替”user:1000:password”，这没有什么问题，但后者更易阅读，并且由此增加的空间消耗相对于key object和value object本身来说很小。当然，没人阻止您一定要用更短的键值节省一丁点儿空间。

坚持使用一种模式
最好坚持一种模式。例如：”object-type:id:field”就是个不错的注意，像这样”user:1000:password”。我喜欢对多单词的字段名中加上一个点，就像这样：”comment:1234:reply.to”。

注意：坚持使用 : 做分割，因为 redis 会自动分割成为树形结构。

值
值的长度不能超过 512 MB
字符串 (Strings)
Strings

说明
这是最简单Redis类型。如果你只用这种类型，Redis就像一个可以持久化的memcached服务器（注：memcache的数据仅保存在内存中，服务器重启后，数据将丢失）。

实例
基础设置
我们用redis-cli来玩一下字符串类型：

redis:6379> set name hello
OK
redis:6379> get name
"hello"
1
2
3
4
原子递增
redis:6379> set counter 100
OK
redis:6379> INCR counter
(integer) 101
1
2
3
4
查询和删除
EXISTS 命令返回1或0标识给定key的值是否存在。

使用 DEL 命令可以删除key对应的值，DEL命令返回1或0标识值是被删除(值存在)或者没被删除(key对应的值不存在)。

redis:6379> exists name
(integer) 1
redis:6379> del name
(integer) 1
redis:6379> exists name
(integer) 0
1
2
3
4
5
6
TYPE 判断类型
redis:6379> set name 100
OK
redis:6379> type name
string
redis:6379> del name
(integer) 1
redis:6379> type name
none
1
2
3
4
5
6
7
8
超时时间
在介绍复杂类型前我们先介绍一个与值类型无关的Redis特性:超时。

你可以对key设置一个超时时间，当这个时间到达后会被删除。精度可以使用毫秒或秒。

> set key some-value
OK
> expire key 5
(integer) 1
> get key (immediately)
"some-value"
> get key (after some time)
(nil)
1
2
3
4
5
6
7
8
查看存活时间
TTL命令用来查看key对应的值剩余存活时间

> set key 100 ex 10
OK
> ttl key
(integer) 9
1
2
3
4
散列 (Hashes)
Redis hash 看起来就像一个 “hash” 的样子，由键值对组成。

Redis 中每个 hash 可以存储 2^32-1 键值对。

实例
redis:6379> HMSET hash_key name "houbinbin" description "redis blog author" github "https://github/houbb"
OK
redis:6379> HGET hash_key
(error) ERR wrong number of arguments for 'hget' command
redis:6379> HGET hash_key name
"houbinbin"
redis:6379> HGETALL hash_key
1) "name"
2) "houbinbin"
3) "description"
4) "redis blog author"
5) "github"
6) "https://github/houbb"
1
2
3
4
5
6
7
8
9
10
11
12
13
完整命令
完整命令

列表 (Lists)
Redis lists基于Linked Lists实现。

插入/删除 算法复杂度为常数级别，但是查询相对较慢。

如果快速访问集合元素很重要，建议使用可排序集合(sorted sets)。可排序集合我们会随后介绍。

数据的插入和查询
LPUSH 命令可向list的左边（头部）添加一个新元素，而RPUSH命令可向list的右边（尾部）添加一个新元素。 
最后LRANGE 命令可从list中取出一定范围的元素:

redis:6379> rpush myList A
(integer) 1
redis:6379> rpush myList B
(integer) 2
redis:6379> rpush myList C
(integer) 3
redis:6379> lrange myList 0 -1
1) "A"
2) "B"
3) "C"
1
2
3
4
5
6
7
8
9
10
连续插入
redis:6379> rpush mylist 1 2 3 4 5 "foo bar"
(integer) 6
redis:6379> lrange mylist 0 -1
1) "1"
2) "2"
3) "3"
4) "4"
5) "5"
6) "foo bar"
1
2
3
4
5
6
7
8
9
推出和删除数据
还有一个重要的命令是pop,它从 list 中删除元素并同时返回删除的值。可以在左边或右边操作。

redis:6379> rpush mylist a b c
(integer) 3
redis:6379> rpop mylist
"c"
redis:6379> rpop mylist
"b"
redis:6379> rpop mylist
"a"
redis:6379> rpop mylist
(nil)
1
2
3
4
5
6
7
8
9
10
常用案例
List上的阻塞操作
可以使用Redis来实现生产者和消费者模型，如使用LPUSH和RPOP来实现该功能。 
但会遇到这种情景：list是空，这时候消费者就需要轮询来获取数据，这样就会增加redis的访问压力、增加消费端的cpu时间，而很多访问都是无用的。 
为此redis提供了阻塞式访问 BRPOP 和 BLPOP 命令。 消费者可以在获取数据时指定如果数据不存在阻塞的时间， 
如果在时限内获得数据则立即返回，如果超时还没有数据则返回null, 0表示一直阻塞。

同时redis还会为所有阻塞的消费者以先后顺序排队。

如需了解详细信息请查看 RPOPLPUSH 和 BRPOPLPUSH。

集合 (Sets)
Redis Set 是 String 的无序排列。

SADD 指令把新的元素添加到 set 中。

对 set 也可做一些其他的操作，比如测试一个给定的元素是否存在，对不同 set 取交集，并集或差，等等。

添加和查看
redis:6379> sadd myset 1 2 3
(integer) 3
redis:6379> smembers mysql
(empty list or set)
redis:6379> smembers myset
1) "1"
2) "2"
3) "3"
1
2
3
4
5
6
7
8
元素是否存在
redis:6379> sismember myset 3
(integer) 1
redis:6379> sismember myset 30
(integer) 0
1
2
3
4
适用场景
用于表示对象间的关系
一个简单的建模方式是，对每一个希望标记的对象使用 set。这个 set 包含和对象相关联的标签的 ID。

想要给新闻打上标签
假设新闻 ID 1000 被打上了 1,2,5 和 77 四个标签，我们可以使用一个 set 把 tag ID 和新闻条目关联起来：

redis:6379> sadd news:1000:tags 1 2 5 77
(integer) 4
1
2
有时候我可能也会需要相反的关系：所有被打上相同标签的新闻列表：

> sadd tag:1:news 1000
(integer) 1
> sadd tag:2:news 1000
(integer) 1
> sadd tag:5:news 1000
(integer) 1
> sadd tag:77:news 1000
(integer) 1
1
2
3
4
5
6
7
8
获取一个对象的所有 tag 是很方便的：

> smembers news:1000:tags
1. 5
2. 1
3. 77
4. 2
1
2
3
4
5
所有命令行

有序集合 (Sorted sets)
Redis 有序集合和集合一样也是string类型元素的集合,且不允许重复的成员。

不同的是每个元素都会关联一个double类型的分数。redis正是通过分数来为集合中的成员进行从小到大的排序。

有序集合的成员是唯一的,但分数(score)却可以重复。

集合是通过哈希表实现的，所以添加，删除，查找的复杂度都是O(1)。 集合中最大的成员数为 2^32 - 1。

比较规则
If A and B are two elements with a different score, then A > B if A.score is > B.score.

If A and B have exactly the same score, then A > B if the A string is 
lexicographically greater than the B string. A and B strings can’t be equal since sorted sets only have unique elements.

实际例子
插入
> zadd hackers 1940 "Alan Kay"
(integer) 1
> zadd hackers 1957 "Sophie Wilson"
(integer 1)
> zadd hackers 1953 "Richard Stallman"
(integer) 1
> zadd hackers 1949 "Anita Borg"
(integer) 1
> zadd hackers 1965 "Yukihiro Matsumoto"
(integer) 1
> zadd hackers 1914 "Hedy Lamarr"
(integer) 1
> zadd hackers 1916 "Claude Shannon"
(integer) 1
> zadd hackers 1969 "Linus Torvalds"
(integer) 1
> zadd hackers 1912 "Alan Turing"
(integer) 1
1
2
3
4
5
6
7
8
9
10
11
12
13
14
15
16
17
18
查看所有
> zrange hackers 0 -1
1) "Alan Turing"
2) "Hedy Lamarr"
3) "Claude Shannon"
4) "Alan Kay"
5) "Anita Borg"
6) "Richard Stallman"
7) "Sophie Wilson"
8) "Yukihiro Matsumoto"
9) "Linus Torvalds"
1
2
3
4
5
6
7
8
9
10
逆序
> zrevrange hackers 0 -1
1) "Linus Torvalds"
2) "Yukihiro Matsumoto"
3) "Sophie Wilson"
4) "Richard Stallman"
5) "Anita Borg"
6) "Alan Kay"
7) "Claude Shannon"
8) "Hedy Lamarr"
9) "Alan Turing"
1
2
3
4
5
6
7
8
9
10
返回分数
> zrange hackers 0 -1 withscores
1) "Alan Turing"
2) "1912"
3) "Hedy Lamarr"
4) "1914"
5) "Claude Shannon"
6) "1916"
7) "Alan Kay"
8) "1940"
9) "Anita Borg"
10) "1949"
11) "Richard Stallman"
12) "1953"
13) "Sophie Wilson"
14) "1957"
15) "Yukihiro Matsumoto"
16) "1965"
17) "Linus Torvalds"
18) "1969"
1
2
3
4
5
6
7
8
9
10
11
12
13
14
15
16
17
18
19
bitmaps
位图不是实际的数据类型，而是在字符串类型上定义的一组面向位的操作。 
因为字符串是二进制安全blob和他们的最大长度是512 MB,他们适合设置 2^32 个不同的部分。

位操作被划分为两个组: 常量时间的单个位操作，比如将一个位设置为1或0，或获取它的值，以及对一组位的运算，例如计算给定范围内的集合位的数目(例如，计数)。

位图的最大优点之一是，它们在存储信息时通常会节省空间。 
例如，在一个系统中，不同的用户由增量的用户id表示，可以记住一点信息(例如，知道用户是否想要接收一份通讯)，而40亿用户只使用512 MB的内存。

实例
SETBIT and GETBIT

> setbit key 10 1
(integer) 1
> getbit key 10
(integer) 1
> getbit key 11
(integer) 0
1
2
3
4
5
6
说明
SETBIT命令作为它的第一个参数，它的第一个参数是位号，它的第二个参数是将比特设置为1或0的值。如果处理的位在当前字符串长度之外，则命令会自动放大字符串。

GETBIT只返回指定索引处的比特值。超出范围位(在目标键中存储的字符串长度以外的位址)总是被认为是零。

在一组位上有三个命令:

BITOP在不同的字符串之间执行位操作。所提供的操作是AND, OR, XOR and NOT

BITCOUNT执行人口计数，报告的比特数设置为1。

BITPOS发现第一个比特的指定值为0或1。

实例
BITPOS和BITCOUNT都能够使用字符串的字节范围来操作，而不是运行整个字符串的长度。下面是一个微不足道的BITCOUNT调用示例:

> setbit key 0 1
(integer) 0
> setbit key 100 1
(integer) 0
> bitcount key
(integer) 2
1
2
3
4
5
6
推荐使用位图的场景
Real time analytics of all kinds.

Storing space efficient but high performance boolean information associated with object IDs.

例如，假设您想知道您的web站点用户每天访问的最长时间。 
你开始计算从零开始的天数，这是你让你的网站公开的一天，并且每次用户访问网站时都设置一些SETBIT。 
作为一个位索引，您只需使用当前的unix时间，减去初始偏移量，并除以3600*24。

对于每个用户，您有一个包含每天访问信息的小字符串。 
使用BITCOUNT可以很容易地获得给定用户访问web站点的天数，而使用一些BITPOS调用，或者简单地获取和分析位图客户端，就可以轻松计算最长的streak。

位图可以分割成多个键，例如为了分片数据集，因为一般来说，最好避免使用大键。 
为了将位图分割成不同的键，而不是把所有的位都设置为一个键，一个简单的策略就是存储每个键的M位， 
然后用位号/M来获取密钥名，然后在密钥中使用位号对M进行处理。

hyperloglogs
HyperLogLog是一种概率数据结构，用于计算独特的东西(技术上，这指的是估算集合的基数)。 
通常计算唯一的条目需要使用与你想要计数的条目数量成比例的内存，因为你需要记住你在过去已经看到的元素，以避免多次计算它们。 
然而，有一组算法可以对内存进行精确的交易:在Redis实现的情况下，您以一个标准错误来结束估计的度量值，而这个值小于1%。 
这个算法的神奇之处在于，您不再需要使用与计算条目数量成比例的内存，而是可以使用一个常量内存!在最坏情况下的12k字节， 
或者如果你的HyperLogLog(我们现在就叫它HLL)就少了很多元素。

Redis中的HLL在技术上是一个不同的数据结构，它被编码为Redis字符串，因此您可以调用GET来序列化HLL，并将其反序列化回服务器。

从概念上讲，HLL API就像使用集合来执行相同的任务一样。您可以将每个已观察到的元素添加到一个集合中，并使用SCARD来检查集合中元素的数量， 
这是唯一的，因为SADD不会重新添加现有元素。

Every time you see a new element, you add it to the count with PFADD.

Every time you want to retrieve the current approximation of the unique elements added with 
PFADD so far, you use the PFCOUNT.

全部文档

地理空间（geospatial）
时间复杂度：每一个元素添加是O(log(N)) ，N是sorted set的元素数量。

将指定的地理空间位置（纬度、经度、名称）添加到指定的key中。

这些数据将会存储到sorted set这样的目的是为了方便使用GEORADIUS或者GEORADIUSBYMEMBER命令对数据进行半径查询等操作。

该命令以采用标准格式的参数x,y,所以经度必须在纬度之前。这些坐标的限制是可以被编入索引的，区域面积可以很接近极点但是不能索引。具体的限制，由EPSG:900913 / EPSG:3785 / OSGEO:41001 规定如下：

有效的经度从-180度到180度。 
有效的纬度从-85.05112878度到85.05112878度。 
当坐标位置超出上述指定范围时，该命令将会返回一个错误。

它是如何工作的？
sorted set使用一种称为Geohash的技术进行填充。经度和纬度的位是交错的，以形成一个独特的52位整数. 
我们知道，一个sorted set 的double score可以代表一个52位的整数，而不会失去精度。

这种格式允许半径查询检查的1 + 8个领域需要覆盖整个半径，并丢弃元素以外的半径。通过计算该区域的范围， 
通过计算所涵盖的范围，从不太重要的部分的排序集的得分，并计算得分范围为每个区域的sorted set中的查询。

使用什么样的地球模型（Earth model）？
这只是假设地球是一个球体，因为使用的距离公式是Haversine公式。这个公式仅适用于地球，而不是一个完美的球体。 
当在社交网站和其他大多数需要查询半径的应用中使用时，这些偏差都不算问题。但是，在最坏的情况下的偏差可能是0.5%， 
所以一些地理位置很关键的应用还是需要谨慎考虑。

返回值
integer-reply, 具体的:

添加到sorted set元素的数目，但不包括已更新score的元素。

实际例子
返回值
integer-reply, 具体的:

添加到sorted set元素的数目，但不包括已更新score的元素。
1
2
3
4
目录索引
目录索引
————————————————
版权声明：本文为CSDN博主「叶止水」的原创文章，遵循 CC 4.0 BY-SA 版权协议，转载请附上原文出处链接及本声明。
原文链接：https://blog.csdn.net/ryo1060732496/article/details/80297419