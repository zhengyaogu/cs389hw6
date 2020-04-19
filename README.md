# CS389 HW5
author: Zhengyao Gu, Albert Ji

## Workload Generation
The workload generator has three functions in its public interface. `std::string request_type_dist()` returns 
a type of request randomly. In our workload, there are only three types of requests: `get`, `set` and `del`.
We strictly set the probability of the function returning `get` at 67%. The probability ratio between `set` and `del`
requests is left for users to specify with the constructor parameter `set_del_ratio`.

`key_type random_key()` returns users a key randomly chosen from the key pool `key_pool`. `key_pool` is generated
at the construction of the workload generator with a size `key_num`, specified by users through constructor.
To generate the key pool, the constructor ask for a randomly generated size following the generalized extreme
value distribution with parameters from the memcache paper. The workload generator then generates a string of
the size based on a set pool of around 80 characters. To simulate the temporal locality identified in the
memcache paper, we break the key pool into `n` segments, each given a probablistic weight. Each segment is two times
as likely to be chosen than the next one. After choosing a segment, a key is chosen uniformly randomly
from the segment and returned through `key_type random_key()`.

`const Cache::val_type random_val()` returns a randomly generated array of characters. It is prompted a random size
following the general Pareto distribution with parameters in the memcache table. Then a string of that size
is uniformly generated based on a set pool of characters as in `random_key`. The string is returned as in `char*` type
through the function.

### Calibration
Here is a list of parameters that affects the hit rate during our calibration, and how they affects the hit rate (positively or negatively)
To attain a stable hit rate, we warm up the server with 100 thousand requests during which the hit rate is not recorded.
Then, we compute the hit rate for the next 1 million requests as our metric.
We fixed our maxmem to be 4MB.
| Parameter | Definition | Effect |
| set_del_ratio | How many times a set request is likely to be prompted | positive |
| key_num | Number of keys in the key pool | negative |
| n_seg | number of segments (see last section. We did not make this as a constructor parameter since we decided on the number 100 pretty quickly) | positive| 
| 
A bigger `set_del_ratio` means less `del` requests, which means less invalidation miss. Thus a bigger ratio increases hit rate.
More keys in the key pool means more keys can be requested by `get`'s, thus more compulsory misses.
Recall that the a segment is always twice as likely to be chosen than the next one. More segments then concentrates a greater
probability in the smaller first segment, thus increasing the temporality overall.

We found that when `set/del = 10`, `key_num = 10,000`, and `n_seg = 100`, we achieve a hit rate around 82% consistently.
