#pragma once

#include "RawData.hpp"

#include <memory> // std::shared_ptr

class icommand {
public:
  icommand() = default;
  virtual ~icommand() = default;
  virtual void Execute() = 0;

protected:
  std::shared_ptr<RawData> m_rawData;
};

