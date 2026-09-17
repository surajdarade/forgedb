#pragma once

#include <cstddef>
#include <cstdint>

namespace forgedb {

inline constexpr std::uint64_t kBPlusTreeMetadataMagic =
    0x464F524745425054ULL; // "FORGEBPT"

inline constexpr std::uint32_t kBPlusTreeMetadataVersion = 1;

inline constexpr std::size_t kMetadataMagicOffset = 0;
inline constexpr std::size_t kMetadataVersionOffset = 8;
inline constexpr std::size_t kMetadataRootPageIdOffset = 12;
inline constexpr std::size_t kMetadataSizeOffset = 20;

inline constexpr std::size_t kBPlusTreeMetadataSize = 28;

} // namespace forgedb