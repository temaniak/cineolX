#pragma once
#include <array>
#include <cstdint>

namespace cineol::xl {
// Latest coherent logical snapshot. Requests do not modify the active values
// used by an in-flight controller/compiler. The enclosing reconciliation clock
// owns the commit boundary and all state/coefficient work.
class ParameterQueueXL {
public:
    using Values=std::array<uint8_t,48>;
    void reset(const Values& values) noexcept {active_=requested_=values;pending_=false;}
    bool request(const Values& values) noexcept {
        requested_=values;pending_=requested_!=active_;return pending_;
    }
    bool pending()const noexcept{return pending_;}
    bool commit() noexcept {
        if(!pending_)return false;
        active_=requested_;pending_=false;return true;
    }
    const Values& active()const noexcept{return active_;}
    const Values& requested()const noexcept{return requested_;}
private:
    Values active_{},requested_{};
    bool pending_=false;
};
} // namespace cineol::xl
