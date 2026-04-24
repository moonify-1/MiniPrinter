#include "app/app_system.h"

#include "rtos/rtos_objects.h"

namespace {

constexpr TickType_t kStateMutexWaitTicks = pdMS_TO_TICKS(5U);

// 系统状态机内部上下文。
//
// 当前阶段只保存最必要的信息：
// - 当前系统状态。
// - 最近一次错误原因。
struct SystemContext {
  mp::SystemState state;
  mp::AppErrorCode lastError;
};

SystemContext g_system = {
    mp::SystemState::BOOT,
    mp::APP_OK,
};

// 尝试获取状态互斥量。
//
// 如果互斥量还没创建，当前函数会直接放行，
// 这样 app_system 仍然可以在早期骨架阶段工作。
bool LockStateMutex() {
  if (mp::g_rtos.stateMutex == nullptr) {
    return true;
  }

  return xSemaphoreTake(mp::g_rtos.stateMutex, kStateMutexWaitTicks) == pdTRUE;
}

// 释放状态互斥量。
void UnlockStateMutex() {
  if (mp::g_rtos.stateMutex == nullptr) {
    return;
  }

  xSemaphoreGive(mp::g_rtos.stateMutex);
}

// 计算当前正常运行路径下应进入的状态。
//
// 注意：
// - 当前没有真实 paper / temp / bat / driver 实现时，
//   不应强行进入 READY。
// - 因此只有条件全部满足时才进入 READY，
//   否则保持在 IDLE。
mp::SystemState DetermineOperationalState(const mp::SystemAppInput& input) {
  if (input.printActive) {
    return mp::SystemState::RUNNING;
  }

  if (input.paperPresent && input.tempOk && input.batOk && input.drvReady &&
      input.vhOff) {
    return mp::SystemState::READY;
  }

  return mp::SystemState::IDLE;
}

}  // namespace

namespace mp {

void SystemApp_Init() {
  if (!LockStateMutex()) {
    return;
  }

  g_system.state = SystemState::BOOT;
  g_system.lastError = APP_OK;
  UnlockStateMutex();
}

void SystemApp_ProcessEvent(const SystemAppInput& input) {
  if (!LockStateMutex()) {
    return;
  }

  if (input.hasError) {
    g_system.lastError = input.errorCode;
  }

  // 安全模式请求和致命错误拥有最高优先级。
  if (input.safeModeRequested ||
      (input.hasError && isFatalError(input.errorCode))) {
    g_system.state = SystemState::SAFE_MODE;
    UnlockStateMutex();
    return;
  }

  // 低功耗状态次高优先级。
  if (input.lowPowerRequested) {
    g_system.state = SystemState::LOW_POWER;
    UnlockStateMutex();
    return;
  }

  // 普通错误进入 ERROR，后续再由恢复逻辑接管。
  if (input.hasError || input.errorPending) {
    g_system.state = SystemState::ERROR;
    UnlockStateMutex();
    return;
  }

  switch (g_system.state) {
    case SystemState::BOOT:
      g_system.state = SystemState::INIT;
      break;

    case SystemState::INIT:
      g_system.state = SystemState::SELF_TEST;
      break;

    case SystemState::SELF_TEST:
      // 当前阶段自检先用 mock 通过。
      g_system.state = SystemState::IDLE;
      break;

    case SystemState::ERROR:
      g_system.state = SystemState::RECOVERY;
      break;

    case SystemState::RECOVERY:
      g_system.state = DetermineOperationalState(input);
      break;

    case SystemState::LOW_POWER:
      g_system.state = DetermineOperationalState(input);
      break;

    case SystemState::IDLE:
    case SystemState::READY:
    case SystemState::RUNNING:
      g_system.state = DetermineOperationalState(input);
      break;

    case SystemState::SAFE_MODE:
      break;

    case SystemState::SHUTDOWN:
    default:
      break;
  }

  UnlockStateMutex();
}

SystemState SystemApp_GetState() {
  if (!LockStateMutex()) {
    return SystemState::ERROR;
  }

  const SystemState state = g_system.state;
  UnlockStateMutex();
  return state;
}

void SystemApp_EnterSafeMode(AppErrorCode reason) {
  if (!LockStateMutex()) {
    return;
  }

  g_system.lastError = reason;
  g_system.state = SystemState::SAFE_MODE;
  UnlockStateMutex();
}

}  // namespace mp
