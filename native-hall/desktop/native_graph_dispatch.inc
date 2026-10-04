// Shared by the native arithmetic network and offline shape verification.
if constexpr(graph==Graph::concert) {
#include "concert_network.inc"
}
else if constexpr(graph==Graph::bright_hall) {
#include "bright_hall_network.inc"
}
else if constexpr(graph==Graph::dark_hall) {
#include "dark_hall_network.inc"
}
else if constexpr(graph==Graph::plate) {
#include "plate_network.inc"
}
else if constexpr(graph==Graph::room) {
#include "concert_network.inc"
}
else if constexpr(graph==Graph::rich_chamber) {
#include "rich_chamber_network.inc"
}
else if constexpr(graph==Graph::small_room) {
#include "small_room_network.inc"
}
else if constexpr(graph==Graph::chamber) {
#include "chamber_network.inc"
}
else if constexpr(graph==Graph::dark_chamber) {
#include "dark_chamber_network.inc"
}
else if constexpr(graph==Graph::inverse_room) {
#include "inverse_room_network.inc"
}
else if constexpr(graph==Graph::small_plate) {
#include "plate_network.inc"
}
else if constexpr(graph==Graph::cd_plate_a) {
#include "cd_plate_a_network.inc"
}
else if constexpr(graph==Graph::cd_plate_b) {
#include "cd_plate_b_network.inc"
}
else if constexpr(graph==Graph::rich_plate) {
#include "rich_plate_network.inc"
}
else if constexpr(graph==Graph::chorus_echo) {
#include "chorus_echo_network.inc"
}
else if constexpr(graph==Graph::resonant_chords) {
#include "resonant_chords_network.inc"
}
else if constexpr(graph==Graph::multiband_delay) {
#include "multiband_delay_network.inc"
}
else if constexpr(graph==Graph::hall_hall) {
#include "hall_hall_network.inc"
}
else if constexpr(graph==Graph::plate_plate) {
#include "plate_plate_network.inc"
}
else if constexpr(graph==Graph::plate_hall) {
#include "plate_hall_network.inc"
}
else if constexpr(graph==Graph::plate_chorus) {
#include "plate_chorus_network.inc"
}
else if constexpr(graph==Graph::rich_split) {
#include "rich_split_network.inc"
}
