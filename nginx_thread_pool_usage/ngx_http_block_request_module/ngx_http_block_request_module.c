/*
 * 尝试使用线程池处理阻塞的请求
 */

#include <ngx_config.h>
#include <ngx_http.h>
#include <ngx_core.h>

static ngx_str_t blk_tp_name = ngx_string("blk_thread_pool"); // 线程池之名

typedef struct {
    ngx_http_request_t *r;
    ngx_fd_t fd;
    ngx_buf_t * buf;
    ngx_int_t err;
} ngx_http_block_request_ctx_t;


static ngx_int_t ngx_http_block_request_response(ngx_http_request_t *r, ngx_buf_t *buf);
static void ngx_http_block_request_thread_event_handler(ngx_event_t *ev);
static void ngx_http_block_request_thread_handler(void *data, ngx_log_t *log);
static ngx_int_t ngx_http_block_request_handler(ngx_http_request_t *r);
static char * ngx_http_block_request(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

ngx_command_t ngx_http_block_request_commands [] = {
    {
        .name = ngx_string("block_request"),
        .type = NGX_HTTP_LOC_CONF | NGX_CONF_NOARGS, 
        .set  = ngx_http_block_request,
        .conf = 0,
        .offset = 0,
        .post = NULL
    },
    ngx_null_command
};

static ngx_http_module_t ngx_http_block_request_module_ctx = {
    .preconfiguration = NULL,
    .postconfiguration = NULL,

    .create_main_conf = NULL,
    .init_main_conf = NULL,

    .create_srv_conf = NULL,
    .merge_srv_conf = NULL,

    .create_loc_conf = NULL,
    .merge_loc_conf  = NULL
};

ngx_module_t ngx_http_block_request_module = {
    NGX_MODULE_V1,
    &ngx_http_block_request_module_ctx,
    ngx_http_block_request_commands,
    NGX_HTTP_MODULE,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NGX_MODULE_V1_PADDING
};


static char * ngx_http_block_request(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t * clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    clcf->handler = ngx_http_block_request_handler;

    return NGX_CONF_OK;
}

static ngx_int_t ngx_http_block_request_handler(ngx_http_request_t *r)
{

    ngx_thread_pool_t * tp;
    ngx_thread_task_t  * task;
    ngx_http_block_request_ctx_t *ctx;
    ngx_buf_t *b;

    /* ngx_cycle 是全局变量 */
    tp = ngx_thread_pool_get((ngx_cycle_t *)ngx_cycle, &blk_tp_name);
    if (!tp)
    {
        return NGX_ERROR;
    }

    task = ngx_thread_task_alloc(r->pool, sizeof(*ctx));
    if (!task)
    {
        return NGX_ERROR;
    }
    ctx = task->ctx;

    task->handler = ngx_http_block_request_thread_handler;
    task->event.data = ctx;
    task->event.handler = ngx_http_block_request_thread_event_handler; 

    ctx->r = r;
    ctx->fd = -1; // test
    b = ngx_pcalloc(r->pool, sizeof(ngx_buf_t));
    if (!b)
    {
        return NGX_ERROR;
    }
    ctx->buf = b;

    if (ngx_thread_task_post(tp, task) != NGX_OK) 
    {
        return NGX_ERROR;
    }
    // 让nginx 暂时不要关闭连接
    // r->main->blocked++; // 应该加不加都可以
    r->connection->buffered |= NGX_HTTP_WRITE_BUFFERED; 
    r->connection->write->delayed = 1;

    return NGX_AGAIN;
 }

/* 线程调度时,执行的handler */
static void ngx_http_block_request_thread_handler(void *data, ngx_log_t *log)
{
    ngx_http_block_request_ctx_t *ctx = data;

    ngx_buf_t  *b;

    b = ctx->buf;
    ngx_int_t i = 7;
    b->start = ngx_pcalloc(ctx->r->pool, 120);
    if (b->start == NULL)
    {
        ctx->err = NGX_ERROR;
        return ;
    }
    b->end  = (u_char *)(b->pos) + 120;

    b->pos  = b->start;
    b->last = b->pos;

    while (i > 0)
    {
        ngx_log_debug0(NGX_LOG_DEBUG_CORE, log, 0, "ngx_http_block_request_thread_handler");
        ngx_sleep (1);
        i--;
        *(b->last) = 'a';
        b->last = (u_char *)(b->last) + 1;
    }
    b->memory = 1;
    b->last_buf = 1;
    ctx->err = NGX_OK;
}

// 处理完成之后, 回归主线程接管时, 会回调的函数
static void ngx_http_block_request_thread_event_handler(ngx_event_t *ev)
{
    ngx_http_block_request_ctx_t *ctx = ev->data;
    ngx_http_request_t *r = ctx->r;

    // r->main->blocked--;
    r->connection->buffered &= ~NGX_HTTP_WRITE_BUFFERED;
    r->connection->write->delayed = 0;
    // 由于线程的加入，破坏了原有的执行流程，这里需要
    //      ngx_http_finalize_request()
    // 来让服务器主动关闭连接
    ngx_http_finalize_request(r, ngx_http_block_request_response(r, ctx->buf));
}

static ngx_int_t ngx_http_block_request_response(ngx_http_request_t *r, ngx_buf_t *buf)
{
    ngx_chain_t out;


    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_type.len = sizeof("text/html") - 1;
    r->headers_out.content_type.data = (u_char *) "text/html";
    r->headers_out.content_length_n = (u_char *)(buf->last) - (u_char *)(buf->pos);

    ngx_http_send_header(r);

    out.buf = buf;
    out.next = NULL;
    buf->memory = 1;
    buf->last_buf = 1; 
    return ngx_http_output_filter(r, &out);
}

