#pragma once
#include <cstdint>
#include <type_traits>

namespace native_hall {
struct ControlWrite224 {
    enum class Kind : uint8_t {Coefficient,AddressLow};
    Kind kind=Kind::Coefficient;
    uint8_t row=0,value=0;
};
struct IgnoreControlWrite224 {void operator()(uint64_t,ControlWrite224) const noexcept {}};
template<class Write> unsigned control_write224(Write& write,unsigned at,ControlWrite224 value) noexcept {
    if constexpr(std::is_invocable_v<Write,unsigned,ControlWrite224>)return write(at,value);
    else return write(at);
}
} // namespace native_hall
