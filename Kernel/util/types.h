#pragma once

using u8 = unsigned char;
using u16 = unsigned short;
using u32 = unsigned int;
using u64 = unsigned long long;
using i8 = signed char;
using i16 = signed short;
using i32 = signed int;
using i64 = signed long long;
using usz = u64;
using isz = i64;

constexpr u32 kernelPID = 0;
using EventHandler = void (*)(void *data);
using InterruptHandler = void (*)(void *, u32);

struct Color {
    u8 red;
    u8 green;
    u8 blue;
};

static constexpr Color colorWhite = { .red = 255, .green = 255, .blue = 255 };
static constexpr Color colorBlack = { .red = 0, .green = 0, .blue = 0 };