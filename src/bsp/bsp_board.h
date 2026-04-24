#pragma once

// 当前真实有效电平尚未最终确认，
// 所以这里用显式宏把“安全状态”的假设集中定义出来。
//
// 这样后续如果原理图或实测证明有效电平不同，
// 只需要改这里，而不需要去全项目搜索魔法数字。
#define LEVEL_VH_OFF 0
#define LEVEL_STB_INACTIVE 0
#define LEVEL_DRV_SLEEP 0

namespace mp {

// 在系统最早阶段把所有高风险输出拉到安全态。
//
// 这个函数设计给 setup() 最前面调用，
// 目的是在串口、日志、协议、任务系统都还没启动前，
// 先把与打印头、电机、功率路径相关的输出收敛到“最保守”状态。
void Bsp_PreInitSafeOutputs();

// 板级初始化入口。
//
// 当前阶段只做两件事：
// 1. 再次确认输出保持安全态。
// 2. 把已知输入脚配置成普通输入模式。
//
// 暂时不做真实驱动初始化，也不做任何会使能功率路径的动作。
void Bsp_Init();

// 把所有与功率或外设相关的输出脚统一切换到安全态。
//
// 必须满足的安全策略：
// - VH 保持关闭。
// - STB1~STB6 保持无效。
// - DI / CLK / LAT 保持低电平。
// - DRV8833 nSLEEP 保持低电平，也就是保持休眠。
// - AIN1 / AIN2 / BIN1 / BIN2 保持低电平。
void Bsp_SetAllOutputsSafe();

}  // namespace mp
