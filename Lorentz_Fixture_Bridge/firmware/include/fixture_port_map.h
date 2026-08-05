#pragma once

#include "fixture_bridge_types.h"

namespace lorentz::fixture {

constexpr ChannelConfig kDefaultChannels[] = {
    {ChannelId::S1, 2101, {9600, 8, SerialParity::None, 1}, true},
    {ChannelId::S2, 2102, {9600, 8, SerialParity::None, 1}, true},
    {ChannelId::S3, 2103, {1200, 8, SerialParity::None, 1}, true},
    {ChannelId::S4, 2104, {9600, 8, SerialParity::None, 1}, true},
    {ChannelId::Hmi, 2105, {9600, 8, SerialParity::None, 1}, true},
    {ChannelId::X2x, 2106, {9600, 8, SerialParity::None, 1}, true},
};

static_assert(
    sizeof(kDefaultChannels) / sizeof(kDefaultChannels[0]) == channel_count(),
    "fixture channel map must define all channels"
);

}  // namespace lorentz::fixture
