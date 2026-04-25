#pragma once

#include <cstddef>  // std::byte, std::size_t
#include <memory>   // std::shared_ptr
#include <vector>   // std::vector

struct RawData {
  std::vector<std::byte> Bytes;
};

