Memory pool allocator that help to improve memory allocation speed and memory usage by improving memory locality during data manipulation. To do so it require a little bit more help from the developper to tell him what kind of usage pattern the allocator must expect.

It also include a static buffer allocator, thus avoiding any further syscall, but in that case the maximum available memory will only be 64MBytes.

To help the developper in optimizing and providing this information, it provide some pretty good statistic so that developers can learn a bit more how the application consume and use in real world the allocated data.

It also include, in debug mode, a lot of assertion and memory verification which stop as soon as detected the program. This will also help the developper by reducing the time between a memory corruption and it's detection.

The current SVN version is considered stable and optimized. The TODO is almost complete, but if you want another feature don't hesitate to ask it !