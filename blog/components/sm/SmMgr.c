#include "SmMgr.h"
#include <string.h>

/* ============================================================================
 * 内部辅助函数
 * ============================================================================ */

/**
 * @brief 查找状态定义
 */
static SmState *SmFindState(SmMachine *machine, SmStateId state_id)
{
    if (machine == NULL || machine->sm_class == NULL)
    {
        return NULL;
    }

    for (uint16_t i = 0; i < machine->sm_class->state_count; i++)
    {
        if (machine->sm_class->states[i].state_id == state_id)
        {
            return &machine->sm_class->states[i];
        }
    }

    return NULL;
}

/**
 * @brief 查找状态转换规则
 */
static SmTransition *SmFindTransition(SmState *state, SmEventId event)
{
    if (state == NULL || state->transitions == NULL)
    {
        return NULL;
    }

    for (uint16_t i = 0; i < state->trans_count; i++)
    {
        if (state->transitions[i].event_id == event)
        {
            return &state->transitions[i];
        }
    }

    return NULL;
}

/**
 * @brief 执行状态转换
 */
static SmRetCode SmPerformTransition(SmMachine *machine, SmState *current_state, SmTransition *trans)
{
    SmRetCode ret = SM_RET_OK;

    /* 执行转换前动作(如果有) */
    if (trans->action != NULL)
    {
        ret = trans->action((SmHandle)machine, trans->action_data);
        if (ret != SM_RET_OK)
        {
            return ret;
        }
    }

    /* 查找目标状态 */
    SmState *next_state = SmFindState(machine, trans->next_state);
    if (next_state == NULL)
    {
        return SM_RET_ERROR;
    }

    /* 退出当前状态 */
    if (current_state->on_exit != NULL)
    {
        ret = current_state->on_exit((SmHandle)machine);
        if (ret != SM_RET_OK)
        {
            return ret;
        }
    }

    /* 更新状态 */
    machine->previous_state = machine->current_state;
    machine->current_state = trans->next_state;

    /* 输出转换日志 */
    if (machine->trans_log_fn != NULL)
    {
        const char *event_name = NULL;
        if (machine->get_event_name_fn != NULL)
        {
            event_name = machine->get_event_name_fn(trans->event_id);
        }
        machine->trans_log_fn(
            machine->sm_class->class_name,
            current_state->state_name,
            next_state->state_name,
            trans->event_id,
            event_name);
    }

    /* 进入新状态 */
    if (next_state->on_enter != NULL)
    {
        ret = next_state->on_enter((SmHandle)machine);
        if (ret != SM_RET_OK)
        {
            return ret;
        }
    }

    return SM_RET_OK;
}

/* ============================================================================
 * API 实现
 * ============================================================================ */

SmRetCode SmCreate(SmMachine *machine, const SmClass *sm_class, void *user_data)
{
    if (machine == NULL || sm_class == NULL)
    {
        return SM_RET_ERROR;
    }

    /* 清零状态机实例 */
    memset(machine, 0, sizeof(SmMachine));

    /* 设置类指针和用户数据 */
    machine->sm_class = sm_class;
    machine->user_data = user_data;
    machine->current_state = SM_STATE_INVALID;
    machine->previous_state = SM_STATE_INVALID;
    machine->is_initialized = false;
    machine->trans_log_fn = NULL;
    machine->get_event_name_fn = NULL;

    /* 调用类初始化函数 */
    if (sm_class->on_init != NULL)
    {
        SmRetCode ret = sm_class->on_init((SmHandle)machine);
        if (ret != SM_RET_OK)
        {
            return ret;
        }
    }

    machine->is_initialized = true;
    return SM_RET_OK;
}

SmRetCode SmDestroy(SmMachine *machine)
{
    if (machine == NULL || !machine->is_initialized)
    {
        return SM_RET_ERROR;
    }

    /* 调用类反初始化函数 */
    if (machine->sm_class != NULL && machine->sm_class->on_deinit != NULL)
    {
        machine->sm_class->on_deinit((SmHandle)machine);
    }

    /* 停止状态机(如果正在运行) */
    if (machine->current_state != SM_STATE_INVALID)
    {
        SmState *state = SmFindState(machine, machine->current_state);
        if (state != NULL && state->on_exit != NULL)
        {
            state->on_exit((SmHandle)machine);
        }
    }

    /* 清零 */
    memset(machine, 0, sizeof(SmMachine));
    return SM_RET_OK;
}

SmRetCode SmStart(SmMachine *machine, SmStateId initial_state)
{
    if (machine == NULL || !machine->is_initialized)
    {
        return SM_RET_ERROR;
    }

    /* 查找初始状态 */
    SmState *state = SmFindState(machine, initial_state);
    if (state == NULL)
    {
        return SM_RET_ERROR;
    }

    /* 设置当前状态 */
    machine->current_state = initial_state;
    machine->previous_state = SM_STATE_INVALID;

    /* 调用进入函数 */
    if (state->on_enter != NULL)
    {
        SmRetCode ret = state->on_enter((SmHandle)machine);
        if (ret != SM_RET_OK)
        {
            return ret;
        }
    }

    return SM_RET_OK;
}

SmRetCode SmStop(SmMachine *machine)
{
    if (machine == NULL || !machine->is_initialized)
    {
        return SM_RET_ERROR;
    }

    if (machine->current_state == SM_STATE_INVALID)
    {
        return SM_RET_OK; /* 已停止 */
    }

    /* 退出当前状态 */
    SmState *state = SmFindState(machine, machine->current_state);
    if (state != NULL && state->on_exit != NULL)
    {
        state->on_exit((SmHandle)machine);
    }

    machine->current_state = SM_STATE_INVALID;
    return SM_RET_OK;
}

SmRetCode SmSendEvent(SmMachine *machine, SmEventId event)
{
    if (machine == NULL || !machine->is_initialized)
    {
        return SM_RET_ERROR;
    }

    if (machine->current_state == SM_STATE_INVALID)
    {
        return SM_RET_ERROR;
    }

    /* 查找当前状态 */
    SmState *state = SmFindState(machine, machine->current_state);
    if (state == NULL)
    {
        return SM_RET_ERROR;
    }

    /* 1. 先调用状态处理函数 */
    if (state->on_handle != NULL)
    {
        SmRetCode ret = state->on_handle((SmHandle)machine, event);
        if (ret == SM_RET_TRANSITION)
        {
            /* 状态处理函数返回TRANSITION,需要执行转换 */
            /* 这里简化处理,实际可能需要从状态上下文获取目标状态 */
            return SM_RET_OK;
        }
        else if (ret != SM_RET_OK && ret != SM_RET_IGNORE)
        {
            return ret;
        }
    }

    /* 2. 查找转换规则 */
    SmTransition *trans = SmFindTransition(state, event);
    if (trans == NULL)
    {
        return SM_RET_IGNORE; /* 无转换规则,忽略事件 */
    }

    /* 3. 检查转换条件 */
    if (trans->condition != NULL)
    {
        bool can_trans = trans->condition((SmHandle)machine, trans->action_data);
        if (!can_trans)
        {
            return SM_RET_IGNORE; /* 条件不满足,忽略事件 */
        }
    }

    /* 4. 执行转换 */
    return SmPerformTransition(machine, state, trans);
}

SmStateId SmGetCurrentState(SmMachine *machine)
{
    if (machine == NULL || !machine->is_initialized)
    {
        return SM_STATE_INVALID;
    }

    return machine->current_state;
}

SmRetCode SmForceTransition(SmMachine *machine, SmStateId new_state)
{
    if (machine == NULL || !machine->is_initialized)
    {
        return SM_RET_ERROR;
    }

    if (machine->current_state == new_state)
    {
        return SM_RET_OK; /* 已是目标状态 */
    }

    SmState *current_state = SmFindState(machine, machine->current_state);
    SmState *next_state = SmFindState(machine, new_state);

    if (next_state == NULL)
    {
        return SM_RET_ERROR;
    }

    /* 退出当前状态 */
    if (current_state != NULL && current_state->on_exit != NULL)
    {
        SmRetCode ret = current_state->on_exit((SmHandle)machine);
        if (ret != SM_RET_OK)
        {
            return ret;
        }
    }

    /* 更新状态 */
    machine->previous_state = machine->current_state;
    machine->current_state = new_state;

    /* 输出转换日志(强制切换) */
    if (machine->trans_log_fn != NULL)
    {
        SmState *current_state = SmFindState(machine, machine->previous_state);
        if (current_state != NULL)
        {
            machine->trans_log_fn(
                machine->sm_class->class_name,
                current_state->state_name,
                next_state->state_name,
                SM_EVENT_INVALID,
                "FORCE_TRANSITION");
        }
    }

    /* 进入新状态 */
    if (next_state->on_enter != NULL)
    {
        SmRetCode ret = next_state->on_enter((SmHandle)machine);
        if (ret != SM_RET_OK)
        {
            return ret;
        }
    }

    return SM_RET_OK;
}

void *SmGetUserData(SmMachine *machine)
{
    if (machine == NULL)
    {
        return NULL;
    }

    return machine->user_data;
}

void SmSetTransLogFn(SmMachine *machine, SmTransLogFn trans_log_fn)
{
    if (machine != NULL)
    {
        machine->trans_log_fn = trans_log_fn;
    }
}

void SmSetGetEventNameFn(SmMachine *machine, SmGetEventNameFn get_event_name_fn)
{
    if (machine != NULL)
    {
        machine->get_event_name_fn = get_event_name_fn;
    }
}

const char *SmGetCurrentStateName(SmMachine *machine)
{
    if (machine == NULL)
    {
        return NULL;
    }

    if (!machine->is_initialized)
    {
        return NULL;
    }

    const SmClass *sm_class = machine->sm_class;
    if (sm_class == NULL || machine->current_state < 0 || machine->current_state >= sm_class->state_count)
    {
        return NULL;
    }

    return sm_class->states[machine->current_state].state_name;
}
