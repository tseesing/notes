Nginx Linux线程池实现分析
=================
作者：tseesing   
日期：2017-11    
Nginx版本：1.13.6    
>注：如果在Nginx中使用线程目前并不是为增加性能，而是非用线程不可，那可能你的程序或架构还有优化的空间。    
>如果觉得有用，转载请自便

内容目录
```
  1. 简介
  2. 配置指令
  3. 在Nginx模块中使用
  3.1. 相关的基本结构
  3.2. 生成任务结构   
  3.3. 获取线程池    
  3.4. 提交任务到线程池   
  3.5. 处理任务的线程执行过程  
  3.6. 主线程回归接管执行过程      	
  4. 可能出现的副作用	    
  5. 参考资料	    
  6. 代码示例	    
```
## 1.	简介
线程池从1.7.11版本开始加入。   
线程context切换会消耗点CPU及内存，在大量并发时，累计的消耗可能也比较可观，有些追求极致性能的人舍不得浪费掉，故线程不会成为他们的选择。    
我曾遇到一个场景：某些请求的处理，要循环读取海量数据，很久才完成，当这样的请求多发几次，nginx的workers悉数要“粘”在这些读取操作上，短期无法再回到epoll_wait()，在客户端看来，服务器已经“不响应”了。   
当然服务器架构或是数据库的设计固然是有问题，但遇到这种情形“阻塞”的情形，不得已需要异步处理，避免一些操作霸占着worker，线程池可以是合适的选择。   
## 2.	配置指令
要使用线程池，编译时期configure时明确地加参数--with-threads来编译：   
  `./configure --with-threads && make && make install`

配置指令在参考资料[2]中有介绍，原文介绍已经很明确完整了，本处只是搬运下：
指令语法：   
`thread_pool name threads=number [max_queue=number];`   
默认：   
`thread_pool default threads=32 max_queue=65536;`   
配置位置：*main*  
*name* 为线程池之名；    
*number*  指明线程池内线程的数目（nginx启动时在init worker阶段，就会创建number个线程）；  
*max_queue*：如果当前线程池内无空闲线程，任务就挂在此线程池队列中；此参数指定该等待队列中的任务的最大数量，超出此限制时，再添加任务会报错。  

原文开头有句话：
> Defines named thread pools used for multi-threaded reading and sending of files without blocking worker processes.

目前官方Nginx中似乎没什么模块在使用线程池。
```
[root@localhost nginx_dev]# grep -nr ngx_thread_pool_get ./nginx-1.13.6
./nginx-1.13.6/src/core/ngx_thread_pool.h:30:ngx_thread_pool_t *ngx_thread_pool_get(ngx_cycle_t *cycle, ngx_str_t *name);
./nginx-1.13.6/src/core/ngx_thread_pool.c:530:    tp = ngx_thread_pool_get(cf->cycle, name);
./nginx-1.13.6/src/core/ngx_thread_pool.c:562:ngx_thread_pool_get(ngx_cycle_t *cycle, ngx_str_t *name)
./nginx-1.13.6/src/http/ngx_http_file_cache.c:771:        tp = ngx_thread_pool_get((ngx_cycle_t *) ngx_cycle, &name);
./nginx-1.13.6/src/http/ngx_http_upstream.c:3703:        tp = ngx_thread_pool_get((ngx_cycle_t *) ngx_cycle, &name);
./nginx-1.13.6/src/http/ngx_http_copy_filter_module.c:283:        tp = ngx_thread_pool_get((ngx_cycle_t *) ngx_cycle, &name);
```
在文件读、写时可进行配置来使用。
在需要使用aio的location，配置:
aio threads=<thread pool name>;
配置aio使用线程后，nginx的文件I/O能使用线程进行。
如果在自开发模块中使用线程池，继续往下。

## 3.	在Nginx模块中使用
### 3.1.	相关的基本结构
```
  struct ngx_thread_task_s {
    ngx_thread_task_t   *next;
    ngx_uint_t           id;
    void                *ctx;  
    void               (*handler)(void *data, ngx_log_t *log);  // 线程任务
    ngx_event_t          event;
  };
  typedef struct ngx_thread_pool_s  ngx_thread_pool_t;
```

此是线程任务结构，定义在src/core/ngx_thread_pool.h文件中，用户需要设置的字段有：  
*ctx*：此字段用户可自由定义，用于将数据从主线程传递给任务线程。  
*hander*： 这是个回调函数的指针，线程通过执行该函数来完成该任务！需要线程处理的工作，定义到此函数指针指向的回调函数即可。  
线程执行任务是这样进行的：  
`task->handler(task->ctx, tp->log);`  
其中第二个参数中的*tp*是线程池结构的指针。这个在使用线程池时不需关注。
*event*: 这个结构定义在`src/event/ngx_event.h` 此结构字段比较多，只复制那些一定会需要（跟踪调试时发现会改动的）的几个：
```
typedef struct ngx_event_s    ngx_event_t;
struct ngx_event_s {
    ...
    unsigned         active:1;
    unsigned         complete:1;
    ngx_event_handler_pt  handler;
    ....
};
```  
其中 *ngx_event_handler_pt* 也是声明 *handler* 为函数指针, 其定义在`src/core/ngx_core.h`文件，定义如下：  
`typedef void (*ngx_event_handler_pt)(ngx_event_t *ev);`  
*active* 及 *complete* 标志任务当前状态，这是thread pool模块自行设置的，用户在需要时，读即可，不要去写。  
*active == 1* 表示任务已经提交并在等候或正在处理。*active == 0* 表示任务没在处理中了；  
*complete == 1* 表示任务已经完成。  
此处的 *handler* 是任务被线程执行完成后，主线程重新接管时，执行的回调函数指针，主线程是这样回调的：  
```
...
event = &task->event;
event->handler(event);
...
```
### 3.2.	生成任务结构
使用：  
`ngx_thread_task_t * ngx_thread_task_alloc(ngx_pool_t *pool, size_t size)；`  
分配线程任务结构。size是可以自定义线程context使用的一些结构，此函数为任务结构及ctx分配内存  
成功返回后：  
*taskngx_thread_task_t.ctx* 会指向该内存区域。  
按 *3.1.节* 的介绍填充需要的字段。  
### 3.3.	获取线程池   
*thread_pool* 指令的第一个参数是线程池的名字，就是使用名称来获取参数的。  
```
ngx_thread_pool_t *  ngx_thread_pool_get(ngx_cycle_t *cycle, ngx_str_t *name)
```
该函数就是从Nginx中找出以name为名的线程池。  
### 3.4.	提交任务到线程池
```
ngx_int_t ngx_thread_task_post(ngx_thread_pool_t *tp, ngx_thread_task_t *task)
```  
*tp* 为 *3.3.节* 获取到的线程池。  
*task* 即任务, 即 *3.2.* 节分配到的结构。  
该函数主要的操作：  
1. task->active = 1；  
2. 将task挂到tp的等待的任务队列中；  
3. 使用条件变量唤醒线程池内候命的线程；  

任务线程通过：
```
ngx_epoll_notify(ngx_event_handler_pt handler)  
```   
通知主线程任务已经完成。  
主线程从epoll_wait()中出来，执行。  
提交任务后，坐等线程处理完成即可。  
### 3.5.	处理任务的线程执行过程
1. 从等待队列头取下一个任务task；  
2. 执行任务：`task->handler(task->ctx, tp->log);  `  
3. 完成任务后，向worker进程（主线程）的某个已经位于epoll中的专用于线程池的fd写数据；  
4. 继续下一任务  

### 3.6.	主线程回归接管执行过程
1. 主线程从 *epoll_wait()* 中被唤醒；  
2. 之后，从完成的任务队列中一次取下所有的完成的任务：    
依次如下执行：
```
event = &task->event;
event->complete = 1;
event->active = 0;
event->handler(event);
```  

## 4.	可能出现的副作用
除了已经提到的CPU切换线程需要消耗一定时间外，可能还有个副作用，如果像本文档第6节的示例那般，一整个请求都使用线程去处理，将会限制并发数量！
在介绍thread_pool指令时，max_queue的参数限制了等待处理的任务数量上限，上限到达后，在任何任务完成前，不能再加入新的任务。间接说明：如果所有请求全程处理都使用线程，当等待处理的请求数量达到max_queue设定的数值时，后续的请求会因为不能被处理而报错。
还是开篇那句话，如果使用线程不是为了提高性能，而是非用不可，则程序的设计或服务器的架构可能还有优化的空间。


## 5.	参考资料
[1] "https://www.nginx.com/blog/thread-pools-boost-performance-9x/"  
[2] "http://nginx.org/en/docs/ngx_core_module.html#thread_pool"  

## 6.	代码示例
示例使用代码，见本目录下的 *ngx_http_block_request_module* 目录。对应的线程池配置如下：  

// file: cong/nginx.conf 的相关配置（片段）
```
...
thread_pool blk_thread_pool threads=1;
...
        location / {
            root   html;
            block_request ;
            index  index.html index.htm;
        }
...
```
