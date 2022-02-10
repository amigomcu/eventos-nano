
// include ---------------------------------------------------------------------
#include "meow.h"
#include "stdio.h"
#include "stdlib.h"
#include <stdarg.h>
#include <string.h>
#include "mdebug/m_debug.h"
#include "mdebug/m_assert.h"

M_MODULE_NAME("Meow")

#ifdef __cplusplus
extern "C" {
#endif

// todo ------------------------------------------------------------------------
// TODO 【已经隐式实现】事件的分级
// 事件分为系统事件、全局事件、局部事件。如何对这三种事件进行实现，应该仔细考虑。
// TODO 【暂不实现，未看到明显优势】事件带参的一般化实现
// TODO 【暂不实现，未看到明显优势】可以考虑使用链表实现时间定时器，可以实现无限多个，而且遍历会更加快速。
// TODO 【暂不实现，未看到明显优势】状态机也考虑用链表管理，可以突破数量的限制。最高优先级为0。
// TODO 【放弃】增加平面状态机模式。
// TODO 【完成】使优先级最高为0。
// TODO 【放弃】是否考虑使时间定时器产生的事件直接执行，而不通过事件池，减少开销？
// TODO 【完成】增加LOG功能在内核中的无缝嵌入。
// TODO 【完成】加快定时器的执行效率。
// TODO 【完成】在m_obj_t中增加list，绑定事件和回调函数，加入事件的回调函数机制。
// TODO 【暂不修改】如果事件队列满，进入断言。这样的设计很不合理，需要进行修改。
// TODO 增加延时或周期执行回调函数的功能。
// TODO 【完成】增加了周期清空log缓冲区，或者80%满清空缓冲区的功能。
// TODO 增加对Shell在内核中的无缝集成。
// TODO 【完成】在打印的LOG上加上时间。
// TODO 行为树并行节点子节点缓冲区，可以使用vector容器实现，可以实现每个控制节点无限个子节点。
// TODO 行为树的栈也可以使用vector实现，可以实现行为树无限深度。
// TODO 【完成】删去此类型BtfNode_Root
// TODO 尚未对行为树Fail的情况进行单元测试。
// TODO 添加修饰节点的实现，修饰节点可以用一个回调函数进行实现。
// TODO 可以考虑将设备框架的Poll机制，一并合并在为行为节点，以并行控制树的形式进行实现。
// TODO 可以考虑对malloc和free进行实现，使事件的参数改为可变长参数机制。
// TODO 添加返回历史状态的功能
// TODO 增加进入低功耗状态的功能
// TODO 考虑以Json形式对事件进行封装，从而实现分布式事件框架。
// TODO 优化事件的存储机制，以便减少系统开销，增强健壮性。
//      01  由于携带参数的事件较少，使事件参数额外存储，不跟随事件本身。
//      02  将事件直接存在事件队列里，而非存储事件池ID。开销较大，而且不便于保持稳定。
//      03  增加evt_send_self，用于发送给自身，直接存入事件队列，不再经过事件池。
//      04  事件的订阅机制。
// TODO 当系统ms数字发生变化时，才进行定时器的扫描。
// TODO 定时事件，发送时返回其ID，以便可以取消此定时事件。
// TODO 【完成】将Assert剥离出去。
// TODO 【完成】完成对Magic数字的校验，以防止对obj的内存覆盖风险。
// TODO 【完成】对Meow框架中每一块RAM添加头尾Magic，以便防止内存被覆盖。
// TODO 【完成】添加编译开关，关闭行为树功能。

// bug list --------------------------------------------------------------------
// TODO Bug01 【解决】MCU下运行获取到当前时间是不对的。
// TODO Bug02 【解决】对事件池中的事件没有很好的销毁掉。
// TODO Bug03 【解决】Evt_Enter执行的内容总是先于Evt_Enter本身打印出来。
// TODO Bug04 宏HSM_ENTER存在bug，在添加了sm_state_log之后，会导致，第一个状态不能执
//            行Evt_Enter。
// TODO Bug05 【解决】随机数发生函数，并不能在任何情况下，都能有效产生随机数，删去此函数。

// framework -------------------------------------------------------------------
typedef struct m_evt_timer_tag {
    int sig;
    int time_ms_delay;
    uint64_t timeout_ms;
    bool is_one_shoot;
} m_evt_timer_t;

typedef struct frame_tag {
    uint32_t magic_head;
    uint32_t evt_sub_tab[Evt_Max];                          // 事件订阅表
#if (MEOW_SYSTEMLOG_EN != 0)
    const char * evt_name[Evt_Max];                         // 事件名称表
#endif

    // 状态机池
    int flag_obj_exist;
    int flag_obj_enable;
    m_obj_t * p_obj[M_SM_NUM_MAX];

    // 关于事件池
    m_evt_t e_pool[M_EPOOL_SIZE];                           // 事件池
    uint32_t flag_epool[M_EPOOL_SIZE / 32 + 1];             // 事件池标志位

    // 定时器池
    m_evt_timer_t e_timer_pool[M_ETIMERPOOL_SIZE];
    uint32_t flag_etimerpool[M_ETIMERPOOL_SIZE / 32 + 1];   // 事件池标志位
    bool is_etimerpool_empty;
    uint64_t timeout_ms_min;

    uint32_t flag_apply[EVT_APPLY_SIZE / 32 + 1];

    bool is_log_enabled;
    bool is_enabled;
    bool is_running;
    bool is_idle;
    
    uint64_t time_crt_ms;
    uint32_t magic_tail;
} frame_t;

static frame_t frame;

#define MEOW_MAGIC_HEAD                         0x4F2EA035
#define MEOW_MAGIC_TAIL                         0x397C67AB

// data ------------------------------------------------------------------------
static const m_evt_t meow_system_evt_table[Evt_DefaultMax] = {
    {Evt_Null, 0, EvtMode_Ps},
    {Evt_Enter, 0, EvtMode_Ps},
    {Evt_Exit, 0, EvtMode_Ps},
    {Evt_Init, 0, EvtMode_Ps},
};

static const uint32_t table_left_shift[32] = {
    0x00000001, 0x00000002, 0x00000004, 0x00000008, 0x00000010, 0x00000020, 0x00000040, 0x00000080,
    0x00000100, 0x00000200, 0x00000400, 0x00000800, 0x00001000, 0x00002000, 0x00004000, 0x00008000,
    0x00010000, 0x00020000, 0x00040000, 0x00080000, 0x00100000, 0x00200000, 0x00400000, 0x00800000,
    0x01000000, 0x02000000, 0x04000000, 0x08000000, 0x10000000, 0x20000000, 0x40000000, 0x80000000
};

static const uint32_t table_left_shift_rev[32] = {
    0xfffffffe, 0xfffffffd, 0xfffffffb, 0xfffffff7, 0xffffffef, 0xffffffdf, 0xffffffbf, 0xffffff7f,
    0xfffffeff, 0xfffffdff, 0xfffffbff, 0xfffff7ff, 0xffffefff, 0xffffdfff, 0xffffbfff, 0xffff7fff,
    0xfffeffff, 0xfffdffff, 0xfffbffff, 0xfff7ffff, 0xffefffff, 0xffdfffff, 0xffbfffff, 0xff7fffff,
    0xfeffffff, 0xfdffffff, 0xfbffffff, 0xf7ffffff, 0xefffffff, 0xdfffffff, 0xbfffffff, 0x7fffffff
};

// macro -----------------------------------------------------------------------
#define HSM_TRIG_(state_, sig_)                                                \
    ((*(state_))(me, &meow_system_evt_table[sig_]))

#define HSM_EXIT_(state_) do {                                                 \
        sm_state_log(me, state_, Evt_Exit);                                    \
        HSM_TRIG_(state_, Evt_Exit);                                           \
    } while (false)

// #define HSM_ENTER_(state_) do {                                                \
//         sm_state_log(me, state_, Evt_Enter);                                   \
//         HSM_TRIG_(state_, Evt_Enter);                                          \
//     } while (false)

#define HSM_ENTER_(state_) do {                                                \
        HSM_TRIG_(state_, Evt_Enter);                                          \
    } while (false)

// static function -------------------------------------------------------------
static void sm_dispath(m_obj_t * const me, m_evt_t const * const e);
static void hook_execute(m_obj_t * const me, m_evt_t const * const e);
static int sm_tran(m_obj_t * const me, m_state_handler path[MAX_NEST_DEPTH]);
static int sm_state_log(m_obj_t * const me, m_state_handler state, int evt_id);
#if (MEOW_BETREE_EN != 0)
static bool meow_condition_null(m_obj_t * const me);
static m_obj_t * bt_get_obj(const char * obj_name);
static betree_t * bt_get_tree(m_obj_t * const me, const char* betree_name);
static betree_t * bt_get_tree_id(m_obj_t * const me, int id);
static int betree_execute(m_obj_t * const me, m_evt_t const * const e);
#endif

// meow ------------------------------------------------------------------------
static void meow_clear(void)
{
    // 清空事件池
    for (int i = 0; i < M_EPOOL_SIZE / 32 + 1; i ++)
        frame.flag_epool[i] = 0xffffffff;
    for (int i = 0; i < M_EPOOL_SIZE; i ++)
        frame.flag_epool[i / 32] &= table_left_shift_rev[i % 32];
    // 清空事件定时器池
    for (int i = 0; i < M_ETIMERPOOL_SIZE / 32 + 1; i ++)
        frame.flag_etimerpool[i] = 0xffffffff;
    for (int i = 0; i < M_ETIMERPOOL_SIZE; i ++)
        frame.flag_etimerpool[i / 32] &= table_left_shift_rev[i % 32];
    frame.is_etimerpool_empty = true;
    // 清空事件申请区
    for (int i = 0; i < EVT_APPLY_SIZE / 32 + 1; i ++)
        frame.flag_apply[i] = 0xffffffff;
    for (int i = 0; i < EVT_APPLY_SIZE; i ++)
        frame.flag_apply[i / 32] &= table_left_shift_rev[i % 32];
    // 清空事件名字区
    for (int i = 0; i < Evt_Max; i ++)
        frame.evt_name[i] = 0;
}

void meow_init(void)
{
    meow_clear();
    frame.is_enabled = true;
    frame.is_running = false;
    frame.is_log_enabled = false;
    frame.is_idle = true;
    frame.magic_head = MEOW_MAGIC_HEAD;
    frame.magic_tail = MEOW_MAGIC_TAIL;

    evt_name(Evt_Enter, "Enter");
    evt_name(Evt_Exit, "Exit");
    evt_name(Evt_Init, "Init");
}

int meow_evttimer(void)
{
    // 获取当前时间，检查延时事件队列
    frame.time_crt_ms = port_get_time_ms();
    
    if (frame.is_etimerpool_empty == true)
        return 210;

    // 时间未到达
    if (frame.time_crt_ms < frame.timeout_ms_min)
        return 211;
    
    // 若时间到达，将此事件推入事件队列，同时在e_timer_pool里删除。
    bool is_etimerpool_empty = true;
    for (int i = 0; i < M_ETIMERPOOL_SIZE; i ++) {
        if ((frame.flag_etimerpool[i / 32] & table_left_shift[i % 32]) == 0)
            continue;
        if (frame.e_timer_pool[i].timeout_ms > frame.time_crt_ms) {
            is_etimerpool_empty = false;
            continue;
        }

        evt_publish(frame.e_timer_pool[i].sig);
        // 清零标志位
        if (frame.e_timer_pool[i].is_one_shoot == true)
            frame.flag_etimerpool[i / 32] &= table_left_shift_rev[i % 32];
        else {
            frame.e_timer_pool[i].timeout_ms += frame.e_timer_pool[i].time_ms_delay;
            is_etimerpool_empty = false;
        }
    }
    frame.is_etimerpool_empty = is_etimerpool_empty;
    if (frame.is_etimerpool_empty == true)
        return 212;

    // 寻找到最小的时间定时器
    uint64_t min_time_out_ms = 0xffffffffffffffff;
    for (int i = 0; i < M_ETIMERPOOL_SIZE; i ++) {
        if ((frame.flag_etimerpool[i / 32] & table_left_shift[i % 32]) == 0)
            continue;
        if (min_time_out_ms <= frame.e_timer_pool[i].timeout_ms)
            continue;
        min_time_out_ms = frame.e_timer_pool[i].timeout_ms;
    }
    frame.timeout_ms_min = min_time_out_ms;

    return 0;
}

int meow_once(void)
{
    int ret = 0;

    if (frame.is_enabled == false) {
        meow_clear();
        return 1;
    }

    // 检查是否有状态机的注册
    if (frame.flag_obj_exist == 0 || frame.flag_obj_enable == 0) {
        ret = 201;
        return ret;
    }

    meow_evttimer();

    if (frame.is_idle == true) {
        ret = 202;
        return ret;
    }
    // 寻找到优先级最高且事件队列不为空的状态机
    m_obj_t * p_obj = (m_obj_t *)0;
    for (int i = 0; i < M_SM_NUM_MAX; i ++) {
        if ((frame.flag_obj_exist & table_left_shift[i]) == 0)
            continue;
        M_ASSERT(frame.p_obj[i]->magic_head == MEOW_MAGIC_HEAD);
        M_ASSERT(frame.p_obj[i]->magic_tail == MEOW_MAGIC_TAIL);
        if (frame.p_obj[i]->is_equeue_empty == true)
            continue;
        p_obj = frame.p_obj[i];
        break;
    }
    if (p_obj == (m_obj_t *)0) {
        frame.is_idle = true;
        
        ret = 203;
        return ret;
    }

    // 寻找到最老的事件
    uint32_t epool_index = *(p_obj->e_queue + p_obj->tail);
    port_critical_enter();
    p_obj->tail ++;
    p_obj->tail %= p_obj->depth;
    if (p_obj->tail == p_obj->head) {
        p_obj->is_equeue_empty = true;
        p_obj->tail = 0;
        p_obj->head = 0;
    }
    port_critical_exit();
    // 对事件进行执行
    m_evt_t * _e = (m_evt_t *)&frame.e_pool[epool_index];
    if ((frame.evt_sub_tab[_e->sig] & table_left_shift[p_obj->priv]) != 0) {
        if (p_obj->obj_type == ObjType_Sm) {
            // 执行状态的转换
            sm_dispath(p_obj, _e);
            // 执行回调函数
            hook_execute(p_obj, _e);
        }
#if (MEOW_BETREE_EN != 0)
        else if (p_obj->obj_type == ObjType_BeTree)
            betree_execute(p_obj, _e);
#endif
        else
            M_ASSERT(false);
    }
    else
        ret = 204;
    if (_e->mode == EvtMode_Ps)
        _e->flag_sub &= table_left_shift_rev[p_obj->priv];

    // 销毁过期事件与其携带的参数
    if ((_e->mode == EvtMode_Ps && _e->flag_sub == 0) ||
        _e->mode == EvtMode_Send) {
        port_critical_enter();
        frame.flag_epool[epool_index / 32] &= table_left_shift_rev[epool_index % 32];
        port_critical_exit();
    }

    return ret;
}

int meow_run(void)
{
    M_ASSERT(frame.is_enabled == true);
    hook_start();
    frame.is_running = true;

    while (true) {
        // 检查Magic
        M_ASSERT(frame.magic_head == MEOW_MAGIC_HEAD);
        M_ASSERT(frame.magic_tail == MEOW_MAGIC_TAIL);
        int ret = meow_once();
        if ((int)(ret / 100) == 2) {
            hook_idle();
        }
        else if (ret == 1)
            break;
    }

    while (true) {
        hook_idle();
    }
}

void meow_stop(void)
{
    frame.is_enabled = false;
    hook_stop();
}

// state machine ---------------------------------------------------------------
void obj_init(m_obj_t * const me, const char* name, m_obj_type_t obj_type, 
              int priv, int e_queue_depth)
{
    M_ASSERT(strlen(name) < OBJ_NAME_MAX);

    // 框架需要先启动起来
    M_ASSERT(frame.is_enabled == true);
    M_ASSERT(frame.is_running == false);
    // 参数检查
    M_ASSERT(me != (m_obj_t *)0);
    M_ASSERT(priv >= 0 && priv < M_SM_NUM_MAX);

    me->name = name;
    me->obj_type = obj_type;
    me->magic_head = MEOW_MAGIC_HEAD;
    me->magic_tail = MEOW_MAGIC_TAIL;
    // 状态模式相关
    if (me->obj_type == ObjType_Sm) {
        me->sm = port_malloc(sizeof(m_mode_sm_t), name);
        M_ASSERT(me->sm != (m_mode_sm_t *)0);
        
        me->sm->state_crt = (void *)m_state_top;
        me->sm->state_tgt = (void *)m_state_top;
        me->sm->hook_list = (m_hook_list *)0;
        me->sm->state_log_list = (m_state_log_t *)0;
    }
#if (MEOW_BETREE_EN != 0)
    // 行为树模式相关
    else if (me->obj_type == ObjType_BeTree) {
        me->bt = port_malloc(sizeof(m_mode_betree_t), name);
        M_ASSERT(me->bt != (m_mode_betree_t *)0);

        me->bt->betree_list = (betree_t *)0;
        me->bt->betree_crt = (betree_t *)0;
        me->bt->is_idle = true;
        for (int i = 0; i < MAX_BETREE_DEPTH; i ++) {
            me->bt->stack[i].node = (btf_node_t *)0;
            me->bt->stack[i].index = 0;
        }
        me->bt->count_stack = 0;
        for (int i = 0; i < BETREE_NODE_CHILD_MAX; i ++)
            me->bt->node_act_success[i] = false;
    }
#endif
    else {
        M_ASSERT(false);
    }

    // 防止二次启动
    if (me->is_enabled == true)
        return;

    // 检查优先级的重复注册
    M_ASSERT((frame.flag_obj_exist & table_left_shift[priv]) == 0);

    // 注册到框架里
    frame.flag_obj_exist |= table_left_shift[priv];
    frame.p_obj[priv] = me;
    // 状态机   
    me->priv = priv;

    // 事件队列
    port_critical_enter();
    me->e_queue = (int *)port_malloc(e_queue_depth * sizeof(int), name);
    M_ASSERT_ID(105, me->e_queue != 0);
    me->head = 0;
    me->tail = 0;
    me->depth = e_queue_depth;
    me->is_equeue_empty = true;
    port_critical_exit();
}

void sm_start(m_obj_t * const me, m_state_handler state_init)
{
    M_ASSERT(me->obj_type == ObjType_Sm);

    me->sm->state_crt = (void *)state_init;
    me->is_enabled = true;
    frame.flag_obj_enable |= table_left_shift[me->priv];

    // 记录原来的最初状态
    m_state_handler t = (m_state_handler)me->sm->state_tgt;
    M_ASSERT(t == m_state_top && t != 0);
    // 进入初始状态，执行TRAN动作。这也意味着，进入初始状态，必须无条件执行Tran动作。
    m_ret_t _ret = ((m_state_handler)me->sm->state_crt)(me, &meow_system_evt_table[Evt_Null]);
    M_ASSERT(_ret == M_Ret_Tran);

    // 由初始状态转移，引发的各层状态的进入
    // 每一个循环，都代表着一个Evt_Init的执行
    _ret = M_Ret_Null;
    do {
        m_state_handler path[MAX_NEST_DEPTH];

        int8_t ip = 0;

        // 由当前层，探测需要进入的各层父状态
        path[0] = (m_state_handler)me->sm->state_crt;
        // 一层一层的探测，一直探测到原状态
        HSM_TRIG_((m_state_handler)me->sm->state_crt, Evt_Null);
        while (me->sm->state_crt != t) {
            ++ ip;
            M_ASSERT(ip < MAX_NEST_DEPTH);
            path[ip] = (m_state_handler)me->sm->state_crt;
            HSM_TRIG_((m_state_handler)me->sm->state_crt, Evt_Null);
        }
        me->sm->state_crt = (void *)path[0];

        // 各层状态的进入
        do {
            HSM_ENTER_(path[ip --]);
        } while (ip >= 0);

        t = path[0];

        _ret = HSM_TRIG_(t, Evt_Init);
        if (_ret == M_Ret_Tran)
            sm_state_log(me, path[ip], Evt_Init);
    } while (_ret == M_Ret_Tran);

    me->sm->state_crt = (void *)t;
    me->sm->state_tgt = (void *)t;
}

int sm_reg_hook(m_obj_t * const me, int evt_id, m_state_handler hook)
{
    // 检查已注册
    m_hook_list * p_list = me->sm->hook_list;
    if (p_list != (m_hook_list *)0)
        do {
            if (p_list->hook == hook)
                return 1;
            p_list = p_list->next;
        } while (p_list != (m_hook_list *)0);

    // 建立新节点
    m_hook_list * p_new = (m_hook_list *)port_malloc(sizeof(m_hook_list), me->name);
    M_ASSERT(p_new != 0);
    p_new->next = me->sm->hook_list;
    p_new->evt_id = evt_id;
    p_new->hook = hook;
    me->sm->hook_list = p_new;

    return 0;
}

int sm_state_name(m_obj_t * const me, m_state_handler state, const char * name)
{
    // 检查已注册名称
    m_state_log_t * p_list = me->sm->state_log_list;
    if (p_list != (m_state_log_t *)0)
        do {
            if (p_list->state == state)
                return 1;
            p_list = p_list->next;
        } while (p_list != (m_state_log_t *)0);

    // 建立新节点
    m_state_log_t * p_new = (m_state_log_t *)port_malloc(sizeof(m_state_log_t), name);
    M_ASSERT(p_new != 0);
    p_new->next = me->sm->state_log_list;
    p_new->state = (void *)state;
    p_new->name = name;
    me->sm->state_log_list = p_new;

    return 0;
}

void sm_log_en(m_obj_t * const me, bool log_en)
{
    me->is_log_enabled = log_en;
}

// Behavior Tree ---------------------------------------------------------------
#if (MEOW_BETREE_EN != 0)
btf_node_t * bt_node_act_create(m_act_handler func, int para[])
{
    btf_node_t * node = (btf_node_t *)port_malloc(sizeof(btf_node_t), "bt_act_node");

    node->node_type = BtfNode_Action;
    node->func = (void *)func;
    node->self_condition = meow_condition_null;
    node->ctrl_type = BtfCtrl_Null;
    for (int i = 0; i < BETREE_NODE_CHILD_MAX; i ++)
        node->child[i] = (void *)0;
    node->count_child = 0;
    for (int i = 0; i < MEOW_ACT_PARA_SIZE; i ++)
        node->para[i] = para[i];

    return node;
}

btf_node_t * bt_node_ctrl_create(int ctrl_type)
{
    btf_node_t * node = (btf_node_t *)port_malloc(sizeof(btf_node_t), "bt_ctrl_node");

    node->node_type = BtfNode_Control;
    node->func = (void *)0;
    node->self_condition = meow_condition_null;
    node->ctrl_type = ctrl_type;
    for (int i = 0; i < BETREE_NODE_CHILD_MAX; i ++) {
        node->child[i] = (void *)0;
        node->child_condition[i] = meow_condition_null;
    }
    node->count_child = 0;
    node->depth = 1;

    return node;
}

void bt_node_set_condition(btf_node_t * const node, m_condition_t condition)
{
    node->self_condition = condition;
}

void bt_ctrl_add_node(btf_node_t * const node_ctrl, btf_node_t * const node)
{
    // 不允许并行节点，添加控制节点。也就是说，并行节点的子节点，必须全部都是
    // 行为节点。
    M_ASSERT(!(node_ctrl->ctrl_type == BtfCtrl_Parallel && node->node_type == BtfNode_Control));
    M_ASSERT(node_ctrl->ctrl_type != BtfCtrl_Select);
    M_ASSERT(node_ctrl->count_child < BETREE_NODE_CHILD_MAX);
    M_ASSERT(node_ctrl->depth < MAX_BETREE_DEPTH);

    node_ctrl->child[node_ctrl->count_child ++] = node;
    if (node->depth >= node_ctrl->depth)
        node_ctrl->depth = node->depth + 1;
}

void bt_ctrl_add_node_condition(btf_node_t * const node_ctrl,
                                btf_node_t * const node,
                                m_condition_t condition)
{
    M_ASSERT(node_ctrl->ctrl_type == BtfCtrl_Select);
    M_ASSERT(node_ctrl->count_child < BETREE_NODE_CHILD_MAX);
    M_ASSERT(node_ctrl->depth < MAX_BETREE_DEPTH);

    node_ctrl->child[node_ctrl->count_child] = node;
    node_ctrl->child_condition[node_ctrl->count_child] = condition;
    node_ctrl->count_child ++;
    if (node->depth >= node_ctrl->depth)
        node_ctrl->depth = node->depth + 1;
}

betree_t * bt_tree_create(const char* name, int id, btf_node_t * const root)
{
    M_ASSERT(strlen(name) < BETREE_NAME_MAX);
    M_ASSERT(root->depth <= MAX_BETREE_DEPTH);

    betree_t * betree = (betree_t *)port_malloc(sizeof(betree_t), name);

    betree->name = name;
    betree->id = id;
    betree->root = root;
    betree->root->node_type = BtfNode_Control;
    betree->next = (void *)0;
    betree->depth = root->depth;

    return betree;
}

void bt_add_betree(m_obj_t * const me, betree_t * const betree)
{
    if (me->bt->betree_list == (betree_t *)0)
        me->bt->betree_list = betree;
    else {
        betree_t * p_betree = me->bt->betree_list;
        while (p_betree->next != (betree_t *)0) {
            // 检查行为树的名称是否重复
            if (strncmp(p_betree->name, betree->name, BETREE_NAME_MAX) == 0)
                M_ASSERT_ID(110, false);
            // 检查行为树的ID是否重复
            if (p_betree->id == betree->id)
                M_ASSERT_ID(111, false);
            p_betree = p_betree->next;
        }
        if (strncmp(p_betree->name, betree->name, BETREE_NAME_MAX) == 0)
            M_ASSERT_ID(110, false);
        // 检查行为树的ID是否重复
        if (p_betree->id == betree->id)
            M_ASSERT_ID(111, false);
        p_betree->next = betree;
    }
}

void bt_start(m_obj_t * const me)
{
    M_ASSERT(me->obj_type == ObjType_BeTree);

    me->is_enabled = true;
    frame.flag_obj_enable |= table_left_shift[me->priv];
    // 自动订阅Evt_Tick
    evt_subscribe(me, Evt_Tick);
}

static void bt_tree_start(m_obj_t * const me, betree_t * const tree)
{
    me->bt->count_stack = 0;

    btf_node_t * node = tree->root;
    while (node->node_type == BtfNode_Control) {
        me->bt->stack[me->bt->count_stack].node = node;
        me->bt->stack[me->bt->count_stack].index = 0;
        me->bt->count_stack ++;

        node = node->child[0];
    }

    // 栈顶控制节点的类型
    int ret;
    int index = me->bt->count_stack - 1;
    int ctrl_type_crt = me->bt->stack[index].node->ctrl_type;
    btf_node_t * ctrl_node_crt = me->bt->stack[me->bt->count_stack - 1].node;
    if (ctrl_type_crt == BtfCtrl_Sequence) {
        me->bt->node_id_crt_se = 0;
        m_act_handler func = (m_act_handler)ctrl_node_crt->child[me->bt->node_id_crt_se]->func;
        int * para = ctrl_node_crt->child[me->bt->node_id_crt_se]->para;
        ret = func(me, &meow_system_evt_table[Evt_Enter], para);
        M_ASSERT(ret == M_Ret_Running || ret == M_Ret_Null);
    }
    else if (ctrl_type_crt == BtfCtrl_Parallel) {
        for (int i = 0; i < ctrl_node_crt->count_child; i ++) {
            m_act_handler func = (m_act_handler)ctrl_node_crt->child[i]->func;
            int * para = ctrl_node_crt->child[i]->para;
            ret = func(me, &meow_system_evt_table[Evt_Enter], para);
            M_ASSERT(ret == M_Ret_Running || ret == M_Ret_Null);
        }
    }
    else if (ctrl_type_crt == BtfCtrl_Select) {
        for (int i = 0; i < me->bt->stack[index].node->count_child; i ++) {
            m_condition_t condition = (m_condition_t)me->bt->stack[index].node->child[i]->self_condition;
            if (condition(me) == true)
                me->bt->node_id_crt_se = i;
        }
        m_act_handler func = (m_act_handler)ctrl_node_crt->child[me->bt->node_id_crt_se]->func;
        int * para = ctrl_node_crt->child[me->bt->node_id_crt_se]->para;
        ret = func(me, &meow_system_evt_table[Evt_Enter], para);
        M_ASSERT(ret == M_Ret_Running || ret == M_Ret_Null);
    }
    else {
        M_ASSERT(false);
    }
}

void bt_tree_run(const char * obj_name, const char* betree_name)
{
    m_obj_t * me = bt_get_obj(obj_name);
    M_ASSERT(me != (m_obj_t *)0);
    betree_t * tree = bt_get_tree(me, betree_name);
    M_ASSERT(tree != (betree_t *)0);

    if (me->bt->is_idle == true) {
        me->bt->is_idle = false;
        me->bt->betree_crt = tree;
        // 将当前的行为树设为Running状态，其他的行为树设为Sleep状态
        betree_t * betree = me->bt->betree_list;
        while (betree != (betree_t *)0) {
            if (betree == tree)
                betree->status = BtStatus_Running;
            else
                betree->status = BtStatus_Sleep;
            betree = betree->next;
        }

        bt_tree_start(me, tree);
    }
}

void bt_tree_run_id(const char * obj_name, int id)
{
    m_obj_t * me = bt_get_obj(obj_name);
    M_ASSERT(me != (m_obj_t *)0);
    betree_t * tree = bt_get_tree_id(me, id);
    M_ASSERT(tree != (betree_t *)0);

    if (me->bt->is_idle == true) {
        me->bt->is_idle = false;
        me->bt->betree_crt = tree;
        // 将当前的行为树设为Running状态，其他的行为树设为Sleep状态
        betree_t * betree = me->bt->betree_list;
        while (betree != (betree_t *)0) {
            if (betree == tree)
                betree->status = BtStatus_Running;
            else
                betree->status = BtStatus_Sleep;
            betree = betree->next;
        }

        bt_tree_start(me, tree);
    }
}

void bt_tree_stop(const char * obj_name, const char* betree_name)
{
    m_obj_t * me = bt_get_obj(obj_name);
    M_ASSERT(me != (m_obj_t *)0);
    betree_t * tree = bt_get_tree(me, betree_name);
    M_ASSERT(tree != (betree_t *)0);

    if (tree == me->bt->betree_crt && me->bt->is_idle == false) {
        me->bt->is_idle = true;
        me->bt->betree_crt = (betree_t *)0;
    }
}

bt_status_t bt_tree_run_status(const char * obj_name, const char* betree_name)
{
    m_obj_t * me = bt_get_obj(obj_name);
    M_ASSERT(me != (m_obj_t *)0);
    betree_t * tree = bt_get_tree(me, betree_name);
    M_ASSERT(tree != (betree_t *)0);

    return tree->status;
}
#endif

// event -----------------------------------------------------------------------
void evt_name(int evt_id, const char * name)
{
    frame.evt_name[evt_id] = name;
}

void evt_publish(int evt_id)
{
    int para;
    evt_publish_para(evt_id, &para);
}

void evt_publish_para(int evt_id, int paras[])
{
    // 保证框架已经运行
    M_ASSERT(frame.is_enabled == true);
    // 没有状态机使能，返回
    if (frame.flag_obj_enable == 0)
        return;
    // 没有状态机订阅，返回
    if (frame.evt_sub_tab[evt_id] == 0)
        return;
    // 新建事件
    static m_evt_t e;
    e.sig = evt_id;
    e.flag_sub = frame.evt_sub_tab[evt_id];
    e.mode = EvtMode_Ps;
    for (int i = 0; i < MEOW_EVT_PARAS_NUM; i ++)
        e.para[i].s32 = paras[i];
    // 如果事件池满，进入断言；如果未满，获取到空位置，并将此位置1
    port_critical_enter();
    int index_empty = 0xffffffff;
    for (int i = 0; i < (M_EPOOL_SIZE / 32 + 1); i ++) {
        if (frame.flag_epool[i] == 0xffffffff)
            continue;
        for (int j = 0; j < 32; j ++) {
            if ((frame.flag_epool[i] & table_left_shift[j]) == 0) {
                frame.flag_epool[i] |= table_left_shift[j];
                index_empty = i * 32 + j;
                break;
            }
        }
        break;
    }
    M_ASSERT_ID(101, index_empty != 0xffffffff);
    frame.e_pool[index_empty] = *(m_evt_t *)&e;

    // 根据flagSub的信息，将事件推入各对象
    for (int i = 0; i < M_SM_NUM_MAX; i ++) {
        if ((frame.flag_obj_exist & table_left_shift[i]) == 0)
            continue;
        if (frame.p_obj[i]->is_enabled == false)
            continue;
        if ((frame.evt_sub_tab[e.sig] & table_left_shift[i]) == 0)
            continue;
        // 如果事件队列满，进入断言
        if (((frame.p_obj[i]->head + 1) % frame.p_obj[i]->depth) == frame.p_obj[i]->tail)
            M_ASSERT_ID(104, false);
        // 事件队列未满，将事件池序号放入事件队列
        *(frame.p_obj[i]->e_queue + frame.p_obj[i]->head) = index_empty;

        frame.p_obj[i]->head ++;
        frame.p_obj[i]->head %= frame.p_obj[i]->depth;
        if (frame.p_obj[i]->is_equeue_empty == true)
            frame.p_obj[i]->is_equeue_empty = false;
        frame.is_idle = false;
    }
    port_critical_exit();
}

void evt_send(const char * obj_name, int evt_id)
{
    int para;
    evt_send_para(obj_name, evt_id, &para);
}

void evt_send_para(const char * obj_name, int evt_id, int para[])
{
    // 保证框架已经运行
    M_ASSERT(frame.is_enabled == true);
    // 没有状态机使能，返回
    if (frame.flag_obj_enable == 0)
        return;
    // 如果事件池满，进入断言；如果未满，获取到空位置，并将此位置1
    port_critical_enter();
    int index_empty = 0xffffffff;
    for (int i = 0; i < (M_EPOOL_SIZE / 32 + 1); i ++) {
        if (frame.flag_epool[i] == 0xffffffff)
            continue;
        for (int j = 0; j < 32; j ++) {
            if ((frame.flag_epool[i] & table_left_shift[j]) == 0) {
                frame.flag_epool[i] |= table_left_shift[j];
                index_empty = i * 32 + j;
                break;
            }
        }
        break;
    }
    M_ASSERT_ID(101, index_empty != 0xffffffff);
    // 新建事件
    static m_evt_t e;
    e.sig = evt_id;
    e.mode = EvtMode_Send;
    for (int i = 0; i < MEOW_EVT_PARAS_NUM; i ++)
        e.para[i].s32 = para[i];
    // 放入事件池
    frame.e_pool[index_empty] = *(m_evt_t *)&e;
    port_critical_exit();

    // 根据名字寻找相应的对象
    int priv = M_SM_NUM_MAX;
    for (int i = 0; i < M_SM_NUM_MAX; i ++) {
        if ((frame.flag_obj_exist & table_left_shift[i]) == 0)
            continue;
        if (frame.p_obj[i]->is_enabled == false)
            continue;
        if (strncmp(frame.p_obj[i]->name, obj_name, OBJ_NAME_MAX) == 0) {
            priv = i;
            break;
        }
    }
    if (priv == M_SM_NUM_MAX)
        M_ASSERT_ID(109, false);

    port_critical_enter();
    // 如果事件队列满，进入断言
    if (((frame.p_obj[priv]->head + 1) % frame.p_obj[priv]->depth) == frame.p_obj[priv]->tail)
        M_ASSERT_ID(104, false);
    // 事件队列未满，将事件池序号放入事件队列
    *(frame.p_obj[priv]->e_queue + frame.p_obj[priv]->head) = index_empty;

    frame.p_obj[priv]->head ++;
    frame.p_obj[priv]->head %= frame.p_obj[priv]->depth;
    if (frame.p_obj[priv]->is_equeue_empty == true)
        frame.p_obj[priv]->is_equeue_empty = false;
    frame.is_idle = false;
    port_critical_exit();
}

void evt_subscribe(m_obj_t * const me, int e_id)
{
    frame.evt_sub_tab[e_id] |= table_left_shift[me->priv];
}

void evt_unsubscribe(m_obj_t * const me, int e_id)
{
    frame.evt_sub_tab[e_id] &= table_left_shift_rev[me->priv];
}

void evt_unsub_all(m_obj_t * const me)
{
    for (int i = 0; i < Evt_Max; i ++)
        frame.evt_sub_tab[i] = 0;
}

static void evt_publish_time(int evt_id, int time_ms, bool is_oneshoot)
{
    M_ASSERT(time_ms >= 0);
    M_ASSERT(!(time_ms == 0 && is_oneshoot == false));

    if (time_ms == 0) {
        EVT_PUB(evt_id);
        return;
    }

    // 如果是周期性的，检查是否已经对某个事件进行过周期设定。
    if (is_oneshoot == false && frame.is_etimerpool_empty == false) {
        bool is_evt_id_set = false;
        for (int i = 0; i < M_ETIMERPOOL_SIZE; i ++) {
            if ((frame.flag_etimerpool[i / 32] & table_left_shift[i % 32]) == 0)
                continue;
            if (frame.e_timer_pool[i].sig != evt_id)
                continue;
            if (frame.e_timer_pool[i].time_ms_delay == time_ms)
                return;
            is_evt_id_set = true;
            break;
        }
        M_ASSERT_ID(106, is_evt_id_set == false);
    }

    // 寻找到空的时间定时器
    int index_empty = 0xffffffff;
    for (int i = 0; i < (M_ETIMERPOOL_SIZE / 32 + 1); i ++) {
        if (frame.flag_etimerpool[i] == 0xffffffff)
            continue;
        for (int j = 0; j < 32; j ++) {
            if ((frame.flag_etimerpool[i] & table_left_shift[j]) == 0) {
                frame.flag_etimerpool[i] |= table_left_shift[j];
                index_empty = i * 32 + j;
                break;
            }
        }
        break;
    }
    M_ASSERT_ID(102, index_empty != 0xffffffff);

    uint64_t time_crt_ms = port_get_time_ms();
    frame.e_timer_pool[index_empty] = (m_evt_timer_t) {
        evt_id, time_ms, time_crt_ms + time_ms, is_oneshoot
    };

    frame.is_etimerpool_empty = false;
    
    // 寻找到最小的时间定时器
    uint64_t min_time_out_ms = 0xffffffffffffffff;
    for (int i = 0; i < M_ETIMERPOOL_SIZE; i ++) {
        if ((frame.flag_etimerpool[i / 32] & table_left_shift[i % 32]) == 0)
            continue;
        if (min_time_out_ms <= frame.e_timer_pool[i].timeout_ms)
            continue;
        min_time_out_ms = frame.e_timer_pool[i].timeout_ms;
    }
    frame.timeout_ms_min = min_time_out_ms;
}

void evt_publish_delay(int evt_id, int time_ms)
{
    evt_publish_time(evt_id, time_ms, true);
}

void evt_publish_period(int evt_id, int time_ms_period)
{
    evt_publish_time(evt_id, time_ms_period, false);
}

int evt_apply(void)
{
    // 寻找尚未被申请的最小的事件
    int index = (int)0xffffffff;
    for (int i = 0; i < (EVT_APPLY_SIZE / 32 + 1); i ++) {
        if (frame.flag_apply[i] == 0xffffffff)
            continue;
        
        for (int j = 0; j < 32; j ++)
            if ((frame.flag_apply[i] & table_left_shift[j]) == 0) {
                frame.flag_apply[i] |= table_left_shift[j];
                index = (Evt_Time_Max + (i * 32) + j);
                break;
            }
        break;
    }
    M_ASSERT_ID(103, index != (int)0xffffffff);

    return index;
}

void evt_unapply(int evt_id)
{
    M_ASSERT(evt_id >= Evt_Apply && evt_id < Evt_User);

    int index = evt_id - Evt_Apply;
    frame.flag_apply[index / 32] &= table_left_shift_rev[index % 32];
}

// state tran ------------------------------------------------------------------
m_ret_t m_tran(m_obj_t * const me, m_state_handler state)
{
    me->sm->state_crt = (void *)state;
    me->sm->state_tgt = (void *)state;

    return M_Ret_Tran;
}

m_ret_t m_super(m_obj_t * const me, m_state_handler state)
{
    me->sm->state_crt = (void *)state;
    me->sm->state_tgt = (void *)state;

    return M_Ret_Super;
}

m_ret_t m_state_top(m_obj_t * const me, m_evt_t const * const e)
{
    (void)me;
    (void)e;

    return M_Ret_Null;
}

/* log ---------------------------------------------------------------------- */
void meow_log_enable(bool log_enable)
{
    frame.is_log_enabled = log_enable;
}

// static function -------------------------------------------------------------
static int sm_state_log(m_obj_t * const me, m_state_handler state, int evt_id)
{
    if (me->is_log_enabled == false)
        return 1;
    
    const char * name_state = 0;
    // 获取状态名称
    m_state_log_t * p_state_list = me->sm->state_log_list;
    while (p_state_list != (m_state_log_t *)0) {
        if (state == p_state_list->state)
            name_state= p_state_list->name;
        p_state_list = p_state_list->next;
    }
    if (name_state == 0)
        return 2;

    if (frame.evt_name[evt_id] == 0)
        return 3;

    m_dbg_evt(me->name, name_state, frame.evt_name[evt_id]);

    return 0;
}

static void sm_dispath(m_obj_t * const me, m_evt_t const * const e)
{
    M_ASSERT(e != (m_evt_t *)0);

    m_state_handler t = (m_state_handler)me->sm->state_crt;
    m_state_handler s;
    m_ret_t r;

    // 层次化的处理事件
    // 注：分为两种情况：
    // (1) 当该状态存在数据时，处理此事件。
    // (2) 当该状态不存在该事件时，到StateTop状态下处理此事件。
    do {
        s = (m_state_handler)me->sm->state_crt;
        r = (*s)(me, e);                              // 执行状态S下的事件处理
        sm_state_log(me, s, e->sig);
    } while (r == M_Ret_Super);

    // 如果不存在状态转移
    if (r != M_Ret_Tran) {
        me->sm->state_crt = (void *)t;                                  // 更新当前状态
        me->sm->state_crt = (void *)t;                                  // 防止覆盖
        return;
    }

    // 如果存在状态转移
    m_state_handler path[MAX_NEST_DEPTH];

    path[0] = (m_state_handler)me->sm->state_crt;    // 保存目标状态
    path[1] = t;
    path[2] = s;

    // exit current state to transition source s...
    while (t != s) {
        // exit handled?
        if (HSM_TRIG_(t, Evt_Exit) == M_Ret_Handled) {
            (void)HSM_TRIG_(t, Evt_Null); // find superstate of t
        }
        sm_state_log(me, t, Evt_Exit);
        t = (m_state_handler)me->sm->state_crt; // stateTgt_ holds the superstate
    }

    int ip = sm_tran(me, path); // take the HSM transition

    // retrace the entry path in reverse (desired) order...
    for (; ip >= 0; --ip) {
        HSM_ENTER_(path[ip]); // enter path[ip]
    }
    t = path[0];    // stick the target into register
    me->sm->state_crt = (void *)t; // update the next state

    // 一级一级的钻入各层
    while (HSM_TRIG_(t, Evt_Init) == M_Ret_Tran) {
        sm_state_log(me, t, Evt_Init);
        ip = 0;
        path[0] = (m_state_handler)me->sm->state_crt;
        (void)HSM_TRIG_((m_state_handler)me->sm->state_crt, Evt_Null);       // 获取其父状态
        while (me->sm->state_crt != t) {
            ip ++;
            path[ip] = (m_state_handler)me->sm->state_crt;
            (void)HSM_TRIG_((m_state_handler)me->sm->state_crt, Evt_Null);   // 获取其父状态
        }
        me->sm->state_crt = (void *)path[0];

        // 层数不能大于MAX_NEST_DEPTH_
        M_ASSERT(ip < MAX_NEST_DEPTH);

        // retrace the entry path in reverse (correct) order...
        do {
            HSM_ENTER_(path[ip --]);                   // 进入path[ip]
        } while (ip >= 0);

        t = path[0];
    }

    me->sm->state_crt = (void *)t;                                  // 更新当前状态
    me->sm->state_tgt = (void *)t;                                  // 防止覆盖
}

static void hook_execute(m_obj_t * const me, m_evt_t const * const e)
{
    m_hook_list * p_next = me->sm->hook_list;
    while (p_next != (m_hook_list *)0) {
        if (p_next->evt_id == e->sig) {
            ((m_state_handler)(p_next->hook))(me, e);
        }
        p_next = p_next->next;
    }
}

#if (MEOW_BETREE_EN != 0)
static int betree_execute(m_obj_t * const me, m_evt_t const * const e)
{
    if (me->bt->is_idle == true) {
        return 5000;
    }

    m_ret_t ret;
    int ctrl_type_top = me->bt->stack[me->bt->count_stack - 1].node->ctrl_type;
    btf_node_t * ctrl_node_crt = me->bt->stack[me->bt->count_stack - 1].node;
    if (ctrl_type_top == BtfCtrl_Sequence) {
        // 执行事件
        m_act_handler func = (m_act_handler)ctrl_node_crt->child[me->bt->node_id_crt_se]->func;
        int * para = ctrl_node_crt->child[me->bt->node_id_crt_se]->para;
        ret = func(me, e, para);
        M_ASSERT(ret == M_Ret_Running || ret == M_Ret_Failed || ret == M_Ret_Success);
        if (ret == M_Ret_Running) {
            me->bt->betree_crt->status = BtStatus_Running;
            return 5111;
        }
        else if (ret == M_Ret_Failed) {
            me->bt->betree_crt->status = BtStatus_Failed;
            return 5112;
        }
        // 执行退出动作
        ret = func(me, &meow_system_evt_table[Evt_Exit], para);
        M_ASSERT(ret == M_Ret_Success || ret == M_Ret_Null);
    }
    else if (ctrl_type_top == BtfCtrl_Parallel) {
        for (int i = 0; i < ctrl_node_crt->count_child; i ++) {
            if (me->bt->node_act_success[i] == true)
                continue;
            // 执行事件
            m_act_handler func = (m_act_handler)ctrl_node_crt->child[i]->func;
            int * para = ctrl_node_crt->child[i]->para;
            ret = func(me, e, para);
            M_ASSERT(ret == M_Ret_Running || ret == M_Ret_Failed || ret == M_Ret_Success);
            if (ret == M_Ret_Failed) {
                // 一个节点执行失败，整个行为树就执行失败。
                me->bt->is_idle = true;
                me->bt->betree_crt->status = BtStatus_Failed;
                return 5121;
            }
            else if (ret == M_Ret_Success) {
                me->bt->node_act_success[i] = true;
                // 执行退出动作
                ret = func(me, &meow_system_evt_table[Evt_Exit], para);
                M_ASSERT(ret == M_Ret_Success || ret == M_Ret_Null);
            }
        }

        int count = 0;
        for (int i = 0; i < ctrl_node_crt->count_child; i ++)
            if (me->bt->node_act_success[i] == true)
                count ++;

        if (count < ctrl_node_crt->count_child) {
            me->bt->betree_crt->status = BtStatus_Running;
            return 5122;
        }
    }
    else if (ctrl_type_top == BtfCtrl_Select) {
        // 执行事件
        m_act_handler func = (m_act_handler)ctrl_node_crt->child[me->bt->node_id_crt_se]->func;
        int * para = ctrl_node_crt->child[me->bt->node_id_crt_se]->para;
        ret = func(me, e, para);
        M_ASSERT(ret == M_Ret_Failed || ret == M_Ret_Running || ret == M_Ret_Success);
        if (ret == M_Ret_Failed) {
            // 一个节点执行失败，整个行为树就执行失败。
            me->bt->is_idle = true;
            me->bt->betree_crt->status = BtStatus_Failed;
            return 5131;
        }
        else if (ret == M_Ret_Running) {
            me->bt->betree_crt->status = BtStatus_Running;
            return 5132;
        }
        // 执行退出动作
        ret = func(me, &meow_system_evt_table[Evt_Exit], para);
        M_ASSERT(ret == M_Ret_Success || ret == M_Ret_Null);
    }
    else {
        M_ASSERT(false);
    }

    int node_id_crt = 0;
    // 当前控制节点未完成，仅存在于顺序控制节点
    if (ctrl_type_top == BtfCtrl_Sequence &&
        (me->bt->node_id_crt_se + 1) < ctrl_node_crt->count_child) {
        me->bt->node_id_crt_se ++;
        node_id_crt = me->bt->node_id_crt_se;
    }
    else {
        // 当前控制节点完成
        int index_ctrl_node_end = 0;
        bool stack_top_end = false;
        do {
            stack_top_end = false;
            // 弹出当前控制节点
            me->bt->count_stack --;
            index_ctrl_node_end = me->bt->stack[me->bt->count_stack].index;
            // 整个行为树完成
            if (me->bt->count_stack == 0) {
                me->bt->is_idle = true;
                me->bt->betree_crt->status = BtStatus_Success;
                return 5200;
            }
            btf_node_t * node_stack_top = me->bt->stack[me->bt->count_stack - 1].node;
            if (((index_ctrl_node_end + 1) == node_stack_top->count_child) &&
                node_stack_top->ctrl_type == BtfCtrl_Sequence) {
                stack_top_end = true;
            }
            else if (node_stack_top->ctrl_type == BtfCtrl_Select) {
                stack_top_end = true;
            }
        } while (stack_top_end == true);
        node_id_crt = index_ctrl_node_end + 1;
    }

    // 寻找当前控制节点的下一个子节点
    // 寻找下一个子节点上的行为节点
    btf_node_t * node = me->bt->stack[me->bt->count_stack - 1].node->child[node_id_crt];
    // 深入这个树的分支
    while (node->node_type == BtfNode_Control) {
        if (node->ctrl_type == BtfCtrl_Sequence) {
            me->bt->stack[me->bt->count_stack].node = node;
            me->bt->stack[me->bt->count_stack].index = node_id_crt;
            ctrl_type_top = node->ctrl_type;
            me->bt->count_stack ++;
            node = node->child[0];
            node_id_crt = 0;
        }
        else if (node->ctrl_type == BtfCtrl_Parallel) {
            me->bt->stack[me->bt->count_stack].node = node;
            me->bt->stack[me->bt->count_stack].index = node_id_crt;
            me->bt->count_stack ++;
            break;
        }
        else if (node->ctrl_type == BtfCtrl_Select) {
            bool all_condition_false = true;
            for (int i = 0; i < node->count_child; i ++) {
                if (((m_condition_t)(node->child_condition[i]))(me) == true) {
                    all_condition_false = false;

                    me->bt->stack[me->bt->count_stack].node = node;
                    me->bt->stack[me->bt->count_stack].index = node_id_crt;
                    // ctrl_type_top = node->ctrl_type;
                    me->bt->count_stack ++;
                    node = node->child[i];
                    node_id_crt = i;

                    break;
                }
            }

            // 不允许某个选择节点的所有字节点，执行条件都是false。
            M_ASSERT(all_condition_false == false);
        }
        else {
            M_ASSERT(false);
        }
    }

    // 根据栈顶控制节点的类型，决定哪些行为节点的执行Evt_Enter
    ctrl_type_top = me->bt->stack[me->bt->count_stack - 1].node->ctrl_type;
    ctrl_node_crt = me->bt->stack[me->bt->count_stack - 1].node;
    // 顺序控制节点
    if (ctrl_type_top == BtfCtrl_Sequence) {
        me->bt->node_id_crt_se = node_id_crt;
        m_act_handler func = (m_act_handler)ctrl_node_crt->child[me->bt->node_id_crt_se]->func;
        int * para = ctrl_node_crt->child[me->bt->node_id_crt_se]->para;
        ret = func(me, &meow_system_evt_table[Evt_Enter], para);
        M_ASSERT(ret == M_Ret_Running || ret == M_Ret_Null);

        me->bt->betree_crt->status = BtStatus_Running;
        return 5301;
    }
    // 并行控制节点
    else if (ctrl_type_top == BtfCtrl_Parallel) {
        // 拷贝并行控制节点下的所有子节点到缓冲区
        m_act_handler func;
        for (int i = 0; i < ctrl_node_crt->count_child; i ++) {
            // 不允许并行控制节点下出现行为节点之外的节点
            M_ASSERT(ctrl_node_crt->child[i]->node_type == BtfNode_Action);
            me->bt->node_act_success[i] = false;
            func = (m_act_handler)ctrl_node_crt->child[i]->func;
            int * para = ctrl_node_crt->child[i]->para;
            ret = func(me, &meow_system_evt_table[Evt_Enter], para);
            M_ASSERT(ret == M_Ret_Running || ret == M_Ret_Null);
        }

        me->bt->betree_crt->status = BtStatus_Running;
        return 5302;
    }
    // 选择控制节点
    else if (ctrl_type_top == BtfCtrl_Select) {
        // 遍历所有的子节点，找到第一个条件满足的子节点
        for (int i = 0; i < me->bt->stack[me->bt->count_stack - 1].node->count_child; i ++) {
            m_condition_t condition = (m_condition_t)me->bt->stack[me->bt->count_stack - 1].node->child_condition[i];
            if (condition(me) == true) {
                m_act_handler func = (m_act_handler)ctrl_node_crt->child[i]->func;
                int * para = ctrl_node_crt->child[i]->para;
                ret = func(me, &meow_system_evt_table[Evt_Enter], para);
                M_ASSERT(ret == M_Ret_Running || ret == M_Ret_Null);

                me->bt->betree_crt->status = BtStatus_Running;
                me->bt->node_id_crt_se = i;
                return 5303;
            }
        }

        M_ASSERT(false);
    }
    else {
        M_ASSERT(false);
    }
    
    return 5400;
}
#endif

static int sm_tran(m_obj_t * const me, m_state_handler path[MAX_NEST_DEPTH])
{
    // transition entry path index
    int ip = -1;
    int iq; // helper transition entry path index
    m_state_handler t = path[0];
    m_state_handler s = path[2];
    m_ret_t r;

    // (a) 跳转到自身 s == t
    if (s == t) {
        HSM_EXIT_(s);  // exit the source
        return 0; // cause entering the target
    }

    (void)HSM_TRIG_(t, Evt_Null); // superstate of target
    t = (m_state_handler)me->sm->state_crt;

    // (b) check source == target->super
    if (s == t)
        return 0; // cause entering the target

    (void)HSM_TRIG_(s, Evt_Null); // superstate of src

    // (c) check source->super == target->super
    if ((m_state_handler)me->sm->state_crt == t) {
        HSM_EXIT_(s);  // exit the source
        return 0; // cause entering the target
    }

    // (d) check source->super == target
    if ((m_state_handler)me->sm->state_crt == path[0]) {
        HSM_EXIT_(s); // exit the source
        return -1;
    }

    // (e) check rest of source == target->super->super..
    // and store the entry path along the way

    // indicate that the LCA was not found
    iq = 0;

    // enter target and its superstate
    ip = 1;
    path[1] = t; // save the superstate of target
    t = (m_state_handler)me->sm->state_crt; // save source->super

    // find target->super->super
    r = HSM_TRIG_(path[1], Evt_Null);
    while (r == M_Ret_Super) {
        ++ ip;
        path[ip] = (m_state_handler)me->sm->state_crt; // store the entry path
        if ((m_state_handler)me->sm->state_crt == s) { // is it the source?
            // indicate that the LCA was found
            iq = 1;

            // entry path must not overflow
            M_ASSERT(ip < MAX_NEST_DEPTH);
            --ip;  // do not enter the source
            r = M_Ret_Handled; // terminate the loop
        }
        // it is not the source, keep going up
        else
            r = HSM_TRIG_((m_state_handler)me->sm->state_crt, Evt_Null);
    }

    // LCA found yet?
    if (iq == 0) {
        // entry path must not overflow
        M_ASSERT(ip < MAX_NEST_DEPTH);

        HSM_EXIT_(s); // exit the source

        // (f) check the rest of source->super
        //                  == target->super->super...
        iq = ip;
        r = M_Ret_Null; // indicate LCA NOT found
        do {
            // is this the LCA?
            if (t == path[iq]) {
                r = M_Ret_Handled; // indicate LCA found
                // do not enter LCA
                ip = iq - 1;
                // cause termination of the loop
                iq = -1;
            }
            else
                -- iq; // try lower superstate of target
        } while (iq >= 0);

        // LCA not found yet?
        if (r != M_Ret_Handled) {
            // (g) check each source->super->...
            // for each target->super...
            r = M_Ret_Null; // keep looping
            do {
                // exit t unhandled?
                if (HSM_TRIG_(t, Evt_Exit) == M_Ret_Handled) {
                    (void)HSM_TRIG_(t, Evt_Null);
                }
                sm_state_log(me, t, Evt_Exit);
                t = (m_state_handler)me->sm->state_crt; //  set to super of t
                iq = ip;
                do {
                    // is this LCA?
                    if (t == path[iq]) {
                        // do not enter LCA
                        ip = iq - 1;
                        // break out of inner loop
                        iq = -1;
                        r = M_Ret_Handled; // break outer loop
                    }
                    else
                        --iq;
                } while (iq >= 0);
            } while (r != M_Ret_Handled);
        }
    }

    return ip;
}



static bool meow_condition_null(m_obj_t * const me)
{
    (void)me;

    return true;
}

#if (MEOW_BETREE_EN != 0)
static m_obj_t * bt_get_obj(const char * obj_name)
{
    // 寻找对象
    m_obj_t * me = (m_obj_t *)0;
    for (int i = 0; i < M_SM_NUM_MAX; i ++) {
        if ((frame.flag_obj_exist & table_left_shift[i]) == 0)
            continue;
        if (frame.p_obj[i]->obj_type == ObjType_Sm)
            continue;
        if (strncmp(frame.p_obj[i]->name, obj_name, OBJ_NAME_MAX) == 0) {
            me = frame.p_obj[i];
            break;
        }
    }
    
    return me;
}

static betree_t * bt_get_tree(m_obj_t * const me, const char* betree_name)
{
    // 寻找行为树
    betree_t * tree = (betree_t *)0;
    betree_t * betree = me->bt->betree_list;
    while (betree != (betree_t *)0) {
        if (strncmp(betree->name, betree_name, BETREE_NAME_MAX) == 0) {
            tree = betree;
            break;
        }
        betree = betree->next;
    }

    return tree;
}

static betree_t * bt_get_tree_id(m_obj_t * const me, int id)
{
    // 寻找行为树
    betree_t * tree = (betree_t *)0;
    betree_t * betree = me->bt->betree_list;
    while (betree != (betree_t *)0) {
        if (betree->id == id) {
            tree = betree;
            break;
        }
        betree = betree->next;
    }

    return tree;
}
#endif

/* for unittest ------------------------------------------------------------- */
void * meow_get_frame(void)
{
    return (void *)&frame;
}

#if (MEOW_BETREE_EN != 0)
int ut_betree_execute(m_obj_t * const me, m_evt_t const * const e)
{
    return betree_execute(me, e);
}
#endif

#ifdef __cplusplus
}
#endif