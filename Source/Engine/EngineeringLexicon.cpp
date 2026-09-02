// Copyright (c) 2026 and onwards The McBopomofo Authors.

#include "EngineeringLexicon.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>

namespace McBopomofo {
namespace {

std::vector<std::string> Split(const std::string& value, char delimiter) {
  std::vector<std::string> result;
  std::stringstream stream(value);
  std::string field;
  while (std::getline(stream, field, delimiter)) {
    if (!field.empty()) {
      result.push_back(field);
    }
  }
  return result;
}

bool StartsWith(const std::string& value, const std::string& prefix) {
  return value.size() >= prefix.size() &&
         value.compare(0, prefix.size(), prefix) == 0;
}

std::string LowerASCII(const std::string& value) {
  std::string result = value;
  std::transform(result.begin(), result.end(), result.begin(), [](char ch) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  });
  return result;
}

bool IsEnglishToken(const std::string& value) {
  if (value.size() < 2) {
    return false;
  }
  bool hasLetter = false;
  for (char ch : value) {
    unsigned char byte = static_cast<unsigned char>(ch);
    if (std::isalpha(byte)) {
      hasLetter = true;
    } else if (!std::isdigit(byte) && ch != '-' && ch != '_' && ch != '.' &&
               ch != '+' && ch != '#') {
      return false;
    }
  }
  return hasLetter;
}

void AddEnglishTokens(const std::string& value,
                      std::unordered_set<std::string>* tokens) {
  std::stringstream stream(value);
  std::string word;
  while (stream >> word) {
    if (IsEnglishToken(word)) {
      tokens->insert(LowerASCII(word));
    }
  }
}

}  // namespace

bool EngineeringLexicon::loadFromFile(const std::string& path) {
  std::ifstream input(path);
  if (!input.is_open()) {
    return false;
  }

  std::vector<Entry> loaded;
  std::unordered_map<std::string, size_t> index;
  std::unordered_set<std::string> englishTokens;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }
    std::vector<std::string> fields = Split(line, '\t');
    if (fields.size() != 5 || fields[0].empty()) {
      continue;
    }
    try {
      size_t consumed = 0;
      double weight = std::stod(fields[2], &consumed);
      if (consumed != fields[2].size() || !std::isfinite(weight)) {
        continue;
      }
      Entry entry;
      entry.phrase = fields[0];
      entry.category = fields[1];
      entry.weight = weight;
      entry.prefixes = Split(fields[3], '|');
      entry.contexts = Split(fields[4], '|');
      AddEnglishTokens(entry.phrase, &englishTokens);
      for (const std::string& alias : entry.prefixes) {
        AddEnglishTokens(alias, &englishTokens);
      }
      index[entry.phrase] = loaded.size();
      loaded.push_back(std::move(entry));
    } catch (...) {
      continue;
    }
  }

  entries_ = std::move(loaded);
  phraseIndex_ = std::move(index);
  englishTokens_ = std::move(englishTokens);
  return true;
}

double EngineeringLexicon::domainScore(
    const std::string& candidate,
    const std::vector<std::string>& previousTokens) const {
  auto iter = phraseIndex_.find(candidate);
  if (iter == phraseIndex_.end()) {
    return 0;
  }
  const Entry& entry = entries_[iter->second];
  double score = entry.weight;
  for (const std::string& context : entry.contexts) {
    for (const std::string& previous : previousTokens) {
      if (previous == context || StartsWith(previous, context)) {
        score += entry.weight * 0.5;
        return score;
      }
    }
  }
  return score;
}

std::vector<EngineeringLexicon::Entry>
EngineeringLexicon::completionsForPrefix(const std::string& prefix,
                                         size_t limit) const {
  std::vector<Entry> result;
  if (prefix.empty() || limit == 0) {
    return result;
  }
  for (const Entry& entry : entries_) {
    bool matched = StartsWith(entry.phrase, prefix) && entry.phrase != prefix;
    if (!matched) {
      for (const std::string& alias : entry.prefixes) {
        if (StartsWith(alias, prefix) || StartsWith(prefix, alias)) {
          matched = true;
          break;
        }
      }
    }
    if (matched) {
      result.push_back(entry);
    }
  }
  std::stable_sort(result.begin(), result.end(), [](const Entry& lhs,
                                                     const Entry& rhs) {
    if (lhs.weight == rhs.weight) {
      return lhs.phrase.size() < rhs.phrase.size();
    }
    return lhs.weight > rhs.weight;
  });
  if (result.size() > limit) {
    result.resize(limit);
  }
  return result;
}

bool EngineeringLexicon::isKnownEnglishToken(const std::string& token) const {
  return englishTokens_.contains(LowerASCII(token));
}

}  // namespace McBopomofo
