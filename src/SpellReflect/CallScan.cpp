#include "SpellReflect/CallScan.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <format>
#include <mutex>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace std::literals;

namespace SpellReflect::CallScan {
    namespace {
        // PE layout, read straight from the loaded image (no windows.h).
        struct RuntimeFunction {
            std::uint32_t begin;
            std::uint32_t end;
            std::uint32_t unwind;
        };
        constexpr std::uint8_t kChainInfo = 0x4;  // UNW_FLAG_CHAININFO

        struct Image {
            std::uintptr_t         base = 0;
            const RuntimeFunction* table = nullptr;
            std::size_t            count = 0;
            std::uintptr_t         textBegin = 0;
            std::uintptr_t         textEnd = 0;
        };

        const Image& GetImage() {
            static const Image image = [] {
                Image      img;
                const auto& module = REL::Module::get();
                img.base = module.base();
                const auto text = module.segment(REL::Segment::textx);
                img.textBegin = text.address();
                img.textEnd = img.textBegin + text.size();

                const auto* bytes = reinterpret_cast<const std::uint8_t*>(img.base);
                std::uint32_t ntOffset = 0;
                std::memcpy(&ntOffset, bytes + 0x3C, sizeof(ntOffset));
                const auto* optional = bytes + ntOffset + 24;
                // PE32+: data directories start at +112; entry 3 = exception (.pdata).
                std::uint32_t rva = 0;
                std::uint32_t size = 0;
                std::memcpy(&rva, optional + 112 + 3 * 8, sizeof(rva));
                std::memcpy(&size, optional + 112 + 3 * 8 + 4, sizeof(size));
                if (rva && size) {
                    img.table = reinterpret_cast<const RuntimeFunction*>(img.base + rva);
                    img.count = size / sizeof(RuntimeFunction);
                }
                Log::INFO("SpellReflect: call scan: .text {:#x} bytes, {} unwind entries", text.size(), img.count);
                return img;
            }();
            return image;
        }

        std::uintptr_t CallTarget(std::uintptr_t a_site) {
            std::int32_t rel = 0;
            std::memcpy(&rel, reinterpret_cast<const void*>(a_site + 1), sizeof(rel));
            return a_site + 5 + static_cast<std::intptr_t>(rel);
        }
    }

    std::uintptr_t FunctionStart(std::uintptr_t a_address) {
        const auto& img = GetImage();
        if (!img.table || a_address < img.base) {
            return 0;
        }
        const auto rva = static_cast<std::uint32_t>(a_address - img.base);
        const auto* end = img.table + img.count;
        // The table is sorted by begin address: the last entry starting at or before rva.
        const auto* it = std::upper_bound(img.table, end, rva,
                                          [](std::uint32_t a_rva, const RuntimeFunction& a_rf) { return a_rva < a_rf.begin; });
        if (it == img.table || rva >= (it - 1)->end) {
            return 0;
        }
        const RuntimeFunction* rf = it - 1;
        for (int depth = 0; depth < 32; ++depth) {
            if (rf->unwind & 1) {  // points at another table entry, not at unwind info
                rf = reinterpret_cast<const RuntimeFunction*>(img.base + (rf->unwind & ~1u));
                continue;
            }
            const auto* info = reinterpret_cast<const std::uint8_t*>(img.base + rf->unwind);
            if (!((info[0] >> 3) & kChainInfo)) {
                return img.base + rf->begin;
            }
            const std::uint32_t codes = info[2];
            rf = reinterpret_cast<const RuntimeFunction*>(info + 4 + 2 * ((codes + 1) & ~1u));
        }
        return 0;
    }

    std::vector<Call> FindCalls(std::uintptr_t a_callee) {
        const auto& img = GetImage();
        std::vector<Call> calls;
        const auto* code = reinterpret_cast<const std::uint8_t*>(img.textBegin);
        const auto  size = img.textEnd - img.textBegin;
        for (std::size_t i = 0; i + 5 <= size; ++i) {
            if (code[i] == 0xE8 && CallTarget(img.textBegin + i) == a_callee) {
                const auto site = img.textBegin + i;
                calls.push_back({ site, FunctionStart(site) });
            }
        }
        return calls;
    }

    std::optional<std::uintptr_t> FindCallIn(std::string_view a_tag, std::uint64_t a_callerId, std::uintptr_t a_callee,
                                             const std::vector<Call>& a_calls, std::span<const Hint> a_hints) {
        const auto& img = GetImage();
        const auto  caller = REL::ID(a_callerId).address();

        std::vector<std::uintptr_t> found;
        for (const auto& call : a_calls) {
            if (call.function == caller) {
                found.push_back(call.site);
            }
        }
        if (found.size() == 1) {
            Log::INFO("SpellReflect: {}: call found in {} at +{:#x}", a_tag, a_callerId, found[0] - caller);
            return found[0];
        }
        if (found.size() > 1) {
            std::string offsets;
            for (const auto site : found) {
                offsets += std::format(" +{:#x}", site - caller);
            }
            Log::ERROR("SpellReflect: {}: {} calls in {} ({}), expected one", a_tag, found.size(), a_callerId, offsets);
            return std::nullopt;
        }

        // None: maybe another plugin got there first (its call leaves .text).
        const auto runtime = REL::Module::get().version();
        for (const auto& hint : a_hints) {
            if (hint.runtime != runtime) {
                continue;
            }
            const auto site = caller + hint.offset;
            const auto target = reinterpret_cast<const std::uint8_t*>(site)[0] == 0xE8 ? CallTarget(site) : 0;
            if (target && (target < img.textBegin || target >= img.textEnd)) {
                Log::INFO("SpellReflect: {}: {}+{:#x} is already hooked by another plugin; chaining", a_tag, a_callerId, hint.offset);
                return site;
            }
        }
        Log::ERROR("SpellReflect: {}: no call of {:#x} found in {}", a_tag, a_callee - img.base, a_callerId);
        return std::nullopt;
    }
}
