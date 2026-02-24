#ifndef __SMMGR_H__
#define __SMMGR_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 数据类型定义
 * ============================================================================ */

typedef void *SmHandle;    /* 状态机句柄 */
typedef int32_t SmStateId; /* 状态ID类型 */
typedef int32_t SmEventId; /* 事件ID类型 */
typedef int32_t SmRetCode; /* 返回码类型 */

/* ============================================================================
 * 日志接口
 * ============================================================================ */

/**
 * @brief 状态转换日志回调函数
 * @param class_name 状态机类名
 * @param from_state 源状态名称
 * @param to_state 目标状态名称
 * @param event_id 触发事件ID
 * @param event_name 事件名称(可选,通过回调获取)
 */
typedef void (*SmTransLogFn)(const char *class_name,
                             const char *from_state,
                             const char *to_state,
                             SmEventId event_id,
                             const char *event_name);

/**
 * @brief 获取事件名称回调函数(可选)
 * @param event_id 事件ID
 * @return 事件名称字符串
 */
typedef const char *(*SmGetEventNameFn)(SmEventId event_id);

/* 返回码定义 */
#define SM_RET_OK         0  /* 成功 */
#define SM_RET_ERROR      -1 /* 错误 */
#define SM_RET_IGNORE     -2 /* 忽略事件 */
#define SM_RET_TRANSITION -3 /* 触发状态转换 */

/* 状态/事件ID无效值 */
#define SM_STATE_INVALID -1 /* 无效状态ID */
#define SM_EVENT_INVALID -1 /* 无效事件ID */

/* ============================================================================
 * 前向声明
 * ============================================================================ */

typedef struct SmStateTag SmState;
typedef struct SmTransitionTag SmTransition;
typedef struct SmMachineTag SmMachine;
typedef struct SmClassTag SmClass;

/* ============================================================================
 * 状态转换条件
 * ============================================================================ */

/**
 * @brief 状态转换条件判断函数
 * @param handle 状态机实例句柄
 * @param user_data 用户数据
 * @return true 允许转换, false 禁止转换
 */
typedef bool (*SmConditionFn)(SmHandle handle, void *user_data);

/**
 * @brief 状态转换动作函数
 * @param handle 状态机实例句柄
 * @param user_data 用户数据
 * @return 执行结果
 */
typedef SmRetCode (*SmActionFn)(SmHandle handle, void *user_data);

/**
 * @brief 状态转换结构体
 */
struct SmTransitionTag
{
    SmEventId event_id;      /* 触发事件ID */
    SmStateId next_state;    /* 目标状态ID */
    SmConditionFn condition; /* 转换条件判断(可选) */
    SmActionFn action;       /* 转换前动作(可选) */
    void *action_data;       /* 动作数据 */
};

/* ============================================================================
 * 状态定义
 * ============================================================================ */

/**
 * @brief 状态进入函数
 * @param handle 状态机实例句柄
 * @return 执行结果
 */
typedef SmRetCode (*SmStateEnterFn)(SmHandle handle);

/**
 * @brief 状态退出函数
 * @param handle 状态机实例句柄
 * @return 执行结果
 */
typedef SmRetCode (*SmStateExitFn)(SmHandle handle);

/**
 * @brief 状态处理函数
 * @param handle 状态机实例句柄
 * @param event 事件ID
 * @return 执行结果或SM_RET_TRANSITION表示触发转换
 */
typedef SmRetCode (*SmStateHandleFn)(SmHandle handle, SmEventId event);

/**
 * @brief 状态结构体
 */
struct SmStateTag
{
    SmStateId state_id;        /* 状态ID */
    const char *state_name;    /* 状态名称(调试用) */
    SmStateEnterFn on_enter;   /* 进入状态时的回调 */
    SmStateExitFn on_exit;     /* 退出状态时的回调 */
    SmStateHandleFn on_handle; /* 状态内事件处理回调 */
    SmTransition *transitions; /* 转换规则数组 */
    uint16_t trans_count;      /* 转换规则数量 */
};

/* ============================================================================
 * 状态机类定义
 * ============================================================================ */

/**
 * @brief 状态机初始化函数
 * @param handle 状态机实例句柄
 * @return 执行结果
 */
typedef SmRetCode (*SmInitFn)(SmHandle handle);

/**
 * @brief 状态机反初始化函数
 * @param handle 状态机实例句柄
 * @return 执行结果
 */
typedef SmRetCode (*SmDeinitFn)(SmHandle handle);

/**
 * @brief 状态机类定义(模板)
 */
struct SmClassTag
{
    const char *class_name; /* 类名 */
    SmState *states;        /* 状态数组 */
    uint16_t state_count;   /* 状态数量 */
    SmInitFn on_init;       /* 初始化回调 */
    SmDeinitFn on_deinit;   /* 反初始化回调 */
};

/* ============================================================================
 * 状态机实例定义
 * ============================================================================ */

/**
 * @brief 状态机实例结构体
 */
struct SmMachineTag
{
    const SmClass *sm_class;            /* 状态机类指针 */
    SmStateId current_state;            /* 当前状态ID */
    SmStateId previous_state;           /* 上一个状态ID */
    bool is_initialized;                /* 是否已初始化 */
    void *user_data;                    /* 用户数据指针 */
    SmTransLogFn trans_log_fn;          /* 状态转换日志回调 */
    SmGetEventNameFn get_event_name_fn; /* 获取事件名称回调 */
};

/* ============================================================================
 * 宏定义 - 辅助创建状态和转换
 * ============================================================================ */

/* 定义空转换(数组结束标记) */
#define SM_TRANS_END() { .event_id = SM_EVENT_INVALID }

/* 定义简单转换(无条件) */
#define SM_TRANS(evt, next) \
    { .event_id = (evt), .next_state = (next), .condition = NULL, .action = NULL, .action_data = NULL }

/* 定义带条件的转换 */
#define SM_TRANS_COND(evt, next, cond) \
    { .event_id = (evt), .next_state = (next), .condition = (cond), .action = NULL, .action_data = NULL }

/* 定义带动作的转换 */
#define SM_TRANS_ACTION(evt, next, act, act_data) \
    { .event_id = (evt), .next_state = (next), .condition = NULL, .action = (act), .action_data = (act_data) }

/* 定义完整转换 */
#define SM_TRANS_FULL(evt, next, cond, act, act_data) \
    { .event_id = (evt), .next_state = (next), .condition = (cond), .action = (act), .action_data = (act_data) }

/* 定义状态 */
#define SM_STATE(id, name, enter, exit, handle, trans_array) \
    { .state_id = (id), .state_name = (name), .on_enter = (enter), .on_exit = (exit), .on_handle = (handle), .transitions = (trans_array), .trans_count = sizeof(trans_array) / sizeof(SmTransition) }

/* 定义状态机类 */
#define SM_CLASS_DEF(name, states_array, init_fn, deinit_fn) \
    { .class_name = (name), .states = (states_array), .state_count = sizeof(states_array) / sizeof(SmState), .on_init = (init_fn), .on_deinit = (deinit_fn) }

/* ============================================================================
 * API 接口
 * ============================================================================ */

/**
 * @brief 创建状态机实例
 * @param machine 状态机实例指针
 * @param sm_class 状态机类定义
 * @param user_data 用户数据
 * @return SM_RET_OK 成功, 其他 失败
 */
SmRetCode SmCreate(SmMachine *machine, const SmClass *sm_class, void *user_data);

/**
 * @brief 销毁状态机实例
 * @param machine 状态机实例指针
 * @return SM_RET_OK 成功, 其他 失败
 */
SmRetCode SmDestroy(SmMachine *machine);

/**
 * @brief 启动状态机
 * @param machine 状态机实例指针
 * @param initial_state 初始状态ID
 * @return SM_RET_OK 成功, 其他 失败
 */
SmRetCode SmStart(SmMachine *machine, SmStateId initial_state);

/**
 * @brief 停止状态机
 * @param machine 状态机实例指针
 * @return SM_RET_OK 成功, 其他 失败
 */
SmRetCode SmStop(SmMachine *machine);

/**
 * @brief 发送事件到状态机
 * @param machine 状态机实例指针
 * @param event 事件ID
 * @return SM_RET_OK 成功, 其他 失败
 */
SmRetCode SmSendEvent(SmMachine *machine, SmEventId event);

/**
 * @brief 获取当前状态ID
 * @param machine 状态机实例指针
 * @return 当前状态ID, -1表示未初始化
 */
SmStateId SmGetCurrentState(SmMachine *machine);

/**
 * @brief 获取当前状态名称
 * @param machine 状态机实例指针
 * @return 状态名称字符串, NULL表示未初始化
 */
const char *SmGetCurrentStateName(SmMachine *machine);

/**
 * @brief 强制切换状态
 * @param machine 状态机实例指针
 * @param new_state 新状态ID
 * @return SM_RET_OK 成功, 其他 失败
 * @note 此函数会跳过条件检查,谨慎使用
 */
SmRetCode SmForceTransition(SmMachine *machine, SmStateId new_state);

/**
 * @brief 获取状态机用户数据
 * @param machine 状态机实例指针
 * @return 用户数据指针
 */
void *SmGetUserData(SmMachine *machine);

/**
 * @brief 设置状态转换日志回调
 * @param machine 状态机实例指针
 * @param trans_log_fn 日志回调函数
 */
void SmSetTransLogFn(SmMachine *machine, SmTransLogFn trans_log_fn);

/**
 * @brief 设置获取事件名称回调
 * @param machine 状态机实例指针
 * @param get_event_name_fn 事件名称获取回调
 */
void SmSetGetEventNameFn(SmMachine *machine, SmGetEventNameFn get_event_name_fn);

#ifdef __cplusplus
}
#endif

#endif /* __SMMGR_H__ */
