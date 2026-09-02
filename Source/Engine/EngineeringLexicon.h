// Copyright (c) 2026 and onwards The McBopomofo Authors.

#ifndef SRC_ENGINE_ENGINEERINGLEXICON_H_
#define SRC_ENGINE_ENGINEERINGLEXICON_H_

#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace McBopomofo {

class EngineeringLexicon {
 public:
  struct Entry {
    std::string phrase;
    std::string category;
    double weight = 0;
    std::vector<std::string> prefixes;
    std::vector<std::string> contexts;
  };

  bool loadFromFile(const std::string& path);

  [[nodiscard]] double domainScore(
      const std::string& candidate,
      const std::vector<std::string>& previousTokens) const;

  [[nodiscard]] std::vector<Entry> completionsForPrefix(
      const std::string& prefix, size_t limit) const;

  // ASCII terms and aliases are also used by mixed input to avoid interpreting
  // short domain tokens such as FEA, CAD, or AMR as Bopomofo.
  [[nodiscard]] bool isKnownEnglishToken(const std::string& token) const;

  [[nodiscard]] size_t entryCount() const { return entries_.size(); }

 private:
  std::vector<Entry> entries_;
  std::unordered_map<std::string, size_t> phraseIndex_;
  std::unordered_set<std::string> englishTokens_;
};

}  // namespace McBopomofo

#endif  // SRC_ENGINE_ENGINEERINGLEXICON_H_
