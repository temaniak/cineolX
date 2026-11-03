#include "../desktop/bank.hpp"
#include "../desktop/concert48.hpp"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>
using namespace cineol::xl;
static void require(bool ok, const char *text) {
    if (!ok) {
        std::cerr << text << '\n';
        std::exit(1);
    }
}
// Fresh physical v8.21 one-second noise observations after a settled selection.
// Columns: program, enabled modes, Chorus byte, slow/fast entry rate.
struct Oracle {
    unsigned program, mode, chorus;
    double slow, fast;
};
static constexpr Oracle oracle[] = {
    {13, 1, 0, 52.6898, 264},   {13, 1, 128, 93.7278, 469}, {13, 1, 248, 93.7347, 469}, {13, 0, 0, 52.5549, 263},
    {13, 0, 128, 52.5505, 263}, {13, 0, 248, 52.5453, 263}, {13, 7, 0, 46.2396, 231},   {13, 7, 128, 70.1262, 349},
    {13, 7, 248, 71.026, 355},  {13, 6, 0, 45.729, 229},    {13, 6, 128, 45.3691, 227}, {13, 6, 248, 44.1808, 222},
    {21, 1, 0, 52.6634, 263},   {21, 1, 128, 93.322, 466},  {21, 1, 248, 93.3294, 466}, {21, 0, 0, 52.9807, 265},
    {21, 0, 128, 53.0033, 265}, {21, 0, 248, 52.9959, 265}, {21, 7, 0, 44.4033, 222},   {21, 7, 128, 72.6851, 364},
    {21, 7, 248, 69.9659, 351}, {21, 6, 0, 44.305, 221},    {21, 6, 128, 44.9742, 224}, {21, 6, 248, 44.9783, 224}};
template <Graph g> void run(const Bank &bank) {
    const auto &d = bank.programs[unsigned(g)];
    for (unsigned mode : {1u, 0u, 7u, 6u})
        for (unsigned chorus : {0u, 128u, 248u}) {
            auto a = std::make_unique<Native48<g>>();
            auto s = d.template settings<g>();
            a->prepare(s);
            a->set_modulation(d.modulation, d.initial_modulation, true);
            a->set_dynamics(d.dynamics, d.initial_dynamics);
            a->set_chorus_reference(uint8_t(d.chorus * 8));
            auto raw = d.controls.factory;
            raw[d.pages[d.chorus_page - 1].cells[d.chorus_slot]] = chorus;
            raw[42] = uint8_t((raw[42] & ~193) | ((mode & 1) ? 64 : 0) | ((mode & 2) ? 128 : 0) | ((mode & 4) ? 1 : 0));
            if (mode & 4) {
                raw[d.dynamics.shared_stop ? 6 : 12] = 18;
                raw[d.dynamics.shared_stop ? 7 : 13] = 18;
                raw[45] = 10;
            }
            a->set_native_controls(s, d.controls, raw, bool(mode & 2));
            a->set_chorus(chorus);
            a->enable_modulation(mode & 1);
            a->set_global(0, 1, 1, 0, 2);
            float l, r;
            for (unsigned n = 0; n < 24000; ++n)
                a->process(0, 0, l, r, true);
            const auto before = a->rich_controller().counters();
            uint32_t random = 17;
            for (unsigned n = 0; n < 48000; ++n) {
                random = random * 1664525 + 1013904223;
                float x = (float(random >> 8) / 16777216.f * 2 - 1) * .03f;
                a->process(x, x * .7f, l, r, true);
            }
            const auto c = a->rich_controller().counters();
            for (const auto &expected : oracle)
                if (expected.program == unsigned(g) && expected.mode == mode && expected.chorus == chorus) {
                    require(std::abs(double(c.slow - before.slow) - expected.slow) <=
                                std::max(2.0, expected.slow * .05),
                            "Rich slow cadence differs materially from physical ROM");
                    require(std::abs(double(c.fast - before.fast) - expected.fast) <=
                                std::max(2.0, expected.fast * .05),
                            "Rich fast cadence differs materially from physical ROM");
                }
            // Parameter requests must preserve elapsed time and live level/decay state,
            // even while a gradual compiler is busy. Identical values cause no transaction.
            const auto time = a->rich_controller().cpu_state();
            const auto live = a->dynamics_state();
            auto changed = raw;
            changed[0] = changed[0] > 16 ? uint8_t(changed[0] - 8) : uint8_t(changed[0] + 8);
            a->set_native_controls(s, d.controls, changed, bool(mode & 2));
            require(a->rich_controller().cpu_state() == time && a->dynamics_state().held == live.held &&
                        a->dynamics_state().mid == live.mid,
                    "parameter request reset the live controller");
            a->set_native_controls(s, d.controls, raw, bool(mode & 2));
            for (unsigned n = 0; n < 24000; ++n)
                a->process(0, 0, l, r, true);
            require(a->rich_controller().counters().parameters == c.parameters,
                    "cancelled/identical request compiled a transaction");
            unsigned falls = 0;
            auto mid = a->dynamics_state().mid;
            // A substantial signal and stop tests a real gradual transition, without
            // assuming the independent reference's exact detector/scan startup phase.
            for (unsigned n = 0; n < 96000; ++n) {
                float x = n < 9600 ? .15f * std::sin(n * .17f) : 0;
                a->process(x, x * .7f, l, r, true);
                auto next = a->dynamics_state().mid;
                if (next < mid) {
                    require(unsigned(mid - next) <= 15, "Rich decay still jumps directly to Stop Decay");
                    ++falls;
                }
                mid = next;
                require(std::isfinite(l) && std::isfinite(r), "non-finite tail");
            }
            if (mode & 4)
                require(falls > 0, "dynamic fixture did not exercise gradual decay");
            if (mode == 7 && chorus == 128) {
                auto latest = raw;
                latest[0] = uint8_t(std::min(240u, unsigned(raw[0]) + 8));
                latest[1] = uint8_t(std::min(240u, unsigned(raw[1]) + 8));
                auto superseded = latest;
                superseded[45] = 48;
                const auto clock = a->rich_controller().cpu_state();
                a->set_native_controls(s, d.controls, superseded, true);
                a->set_native_controls(s, d.controls, latest, true);
                require(a->rich_controller().cpu_state() == clock, "coalesced update restarted the scan");
                for (unsigned n = 0; n < 24000; ++n)
                    a->process(0, 0, l, r, true);
                require(a->rich_controller().active_parameters() == latest,
                        "latest coherent parameter snapshot did not commit");
                latest[42] &= uint8_t(~1u);
                latest[43] = 2;
                latest[44] = 248;
                a->set_native_controls(s, d.controls, latest, false);
                a->enable_modulation(false);
                for (unsigned n = 0; n < 48000; ++n) {
                    a->process(n < 2400 ? .1f : 0, 0, l, r, true);
                    require(std::isfinite(l) && std::isfinite(r), "mode/Size edge produced non-finite output");
                }
                require(a->rich_controller().active_parameters() == latest && a->dynamics_state().flags == 0,
                        "mode/Size update lost the requested state");
            }
            std::cout << unsigned(g) << ',' << chorus << ',' << mode << ',' << c.slow - before.slow << ','
                      << c.fast - before.fast << ',' << c.mod - before.mod << ',' << c.decay - before.decay << ','
                      << c.feedback - before.feedback << '\n';
        }
}
int main(int argc, char **argv) {
    require(argc == 2, "usage: cineol_xl_rich_timing_check XL_BANK");
    std::ifstream f(argv[1], std::ios::binary);
    std::vector<char> b((std::istreambuf_iterator<char>(f)), {});
    auto p = std::make_unique<Bank>();
    if (!read_bank(b.data(), b.size(), *p))
        return 1;
    std::cout << "program,chorus,mode,slow,fast,mod,decay,feedback\n";
    run<Graph::rich_plate>(*p);
    run<Graph::rich_split>(*p);
}
