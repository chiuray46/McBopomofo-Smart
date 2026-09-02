// Copyright (c) 2026 and onwards The McBopomofo Authors.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.

#ifndef SRC_ENGINE_LOCALMODELPROVIDER_H_
#define SRC_ENGINE_LOCALMODELPROVIDER_H_

#include <string>
#include <vector>

namespace McBopomofo {

// Extension point for a future Foundation Models or Core ML implementation.
// The MVP uses NoOpLocalModelProvider and never performs network requests.
class LocalModelProvider {
 public:
  virtual ~LocalModelProvider() = default;
  virtual double score(const std::vector<std::string>& context,
                       const std::string& candidate) const = 0;
};

class NoOpLocalModelProvider final : public LocalModelProvider {
 public:
  double score(const std::vector<std::string>&,
               const std::string&) const override {
    return 0;
  }
};

}  // namespace McBopomofo

#endif  // SRC_ENGINE_LOCALMODELPROVIDER_H_

