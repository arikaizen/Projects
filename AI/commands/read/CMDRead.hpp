#pragma once

#include "RawData.hpp"

#include <memory> // std::shared_ptr

class CMDRead {
public:
  explicit CMDRead(std::shared_ptr<RawData> rawData);
  virtual ~CMDRead() = default;

  CMDRead(const CMDRead&) = delete;
  CMDRead& operator=(const CMDRead&) = delete;
  CMDRead(CMDRead&&) = delete;
  CMDRead& operator=(CMDRead&&) = delete;

private:
  std::shared_ptr<RawData> m_rawData;
};

