// discord_sdk_impl.cpp — single translation unit that instantiates the Discord
// Social SDK C++ wrapper implementations.
//
// discordpp.h is a single-header library: the inline method bodies (which call
// into the Discord_* C ABI exported by discord_partner_sdk.lib) are compiled
// only where DISCORDPP_IMPLEMENTATION is defined. It MUST be defined in exactly
// one .cpp in the whole program to avoid duplicate-symbol / ODR errors.
//
// This file is added to the build only when OMNIPRESENCE_WITH_DISCORD is ON.

#define DISCORDPP_IMPLEMENTATION
#include <discordpp.h>
