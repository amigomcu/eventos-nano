# HAL库与EIO库的区别
-----------------------------------------
1. HAL库的接口定义如下，其他接口到子对象里去定义。
``` C
struct hal_ops
{
    hal_err_t (* enable)(hal_obj_t *me, bool status);
    void (* poll)(hal_obj_t *me);
};
```

2. 为了节省RAM，UART删除掉config函数，程序员在driver中采用固定配置。
3. HAL库的uart接口定义为
struct io_uart_ops
{
    int32_t (* write)(eio_obj_t *me, const void *buff, uint32_t size);
    int32_t (* read)(eio_obj_t *me, void *buff, uint32_t size);
};

4. HAL库中的IO中断使能接口，用全局中断替代。
5. HAL库中的eio_set_event放进各子设备中去。