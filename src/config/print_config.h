#pragma once

#include <cstdint>

namespace mp {

// 打印头基础规格常量。
//
// 这些值描述的是“打印一行数据”时的基本几何关系，
// 它们暂时不涉及真实加热驱动，只是为缓存、位图和分组控制提供统一基线。
constexpr std::uint16_t PRINT_DOTS_PER_LINE = 384U;
constexpr std::uint16_t PRINT_BYTES_PER_LINE = 48U;
constexpr std::uint8_t PRINT_STB_GROUP_COUNT = 6U;
constexpr std::uint8_t PRINT_DOTS_PER_STB_GROUP = 64U;
constexpr std::uint8_t PRINT_MONO_MSB_LEFT = 1U;

// 这两个静态断言没有运行时成本，
// 但能在编译期尽早发现基础配置是否自相矛盾。
static_assert(PRINT_DOTS_PER_LINE == (PRINT_BYTES_PER_LINE * 8U),
              "PRINT_DOTS_PER_LINE must equal PRINT_BYTES_PER_LINE * 8");

static_assert(
    PRINT_DOTS_PER_LINE ==
        (static_cast<std::uint16_t>(PRINT_STB_GROUP_COUNT) *
         static_cast<std::uint16_t>(PRINT_DOTS_PER_STB_GROUP)),
    "PRINT_DOTS_PER_LINE must equal PRINT_STB_GROUP_COUNT * "
    "PRINT_DOTS_PER_STB_GROUP");

}  // namespace mp
