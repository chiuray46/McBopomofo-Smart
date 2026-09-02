// Copyright (c) 2026 and onwards The McBopomofo Authors.

#include "SmartCandidateReranker.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <regex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace McBopomofo {
namespace {

std::string CandidateLearningKey(const std::string& reading,
                                 const std::string& value) {
  return reading + "\x1d" + value;
}

bool ReadDouble(const std::string& json, const std::string& key,
                double* value) {
  std::regex pattern("\\\"" + key +
                     "\\\"\\s*:\\s*(-?[0-9]+(?:\\.[0-9]+)?)");
  std::smatch match;
  if (!std::regex_search(json, match, pattern)) {
    return false;
  }
  try {
    *value = std::stod(match[1].str());
    return std::isfinite(*value);
  } catch (...) {
    return false;
  }
}

bool ReadSize(const std::string& json, const std::string& key, size_t* value) {
  double parsed = 0;
  if (!ReadDouble(json, key, &parsed) || parsed < 0) {
    return false;
  }
  *value = static_cast<size_t>(parsed);
  return true;
}

}  // namespace

bool SmartScoringWeights::loadFromJsonFile(const std::string& path) {
  std::ifstream input(path);
  if (!input.is_open()) {
    return false;
  }
  std::string json((std::istreambuf_iterator<char>(input)),
                   std::istreambuf_iterator<char>());
  ReadDouble(json, "baseWeight", &base);
  ReadDouble(json, "contextWeight", &context);
  ReadDouble(json, "userWeight", &user);
  ReadDouble(json, "recencyWeight", &recency);
  ReadDouble(json, "domainWeight", &domain);
  ReadDouble(json, "phraseCompletionWeight", &phraseCompletion);
  ReadDouble(json, "localModelWeight", &localModel);
  ReadDouble(json, "learningHalfLifeSeconds", &learningHalfLifeSeconds);
  ReadSize(json, "minimumObservations", &minimumObservations);
  ReadSize(json, "completionLimit", &completionLimit);
  ReadSize(json, "minimumCorrectionRejections",
           &minimumCorrectionRejections);
  ReadDouble(json, "correctionFeedbackMaxAgeSeconds",
             &correctionFeedbackMaxAgeSeconds);
  return true;
}

SmartCandidateReranker::SmartCandidateReranker(
    std::unique_ptr<LearningDatabase> database, EngineeringLexicon lexicon,
    SmartScoringWeights weights,
    std::unique_ptr<LocalModelProvider> localModel)
    : database_(std::move(database)),
      lexicon_(std::move(lexicon)),
      weights_(weights),
      localModel_(std::move(localModel)) {
  if (database_ != nullptr) {
    database_->open();
  }
}

std::vector<SmartCandidateReranker::RankedCandidate>
SmartCandidateReranker::rank(
    const std::vector<Candidate>& candidates,
    const std::vector<std::string>& previousTokens,
    const std::string& completionPrefix, double timestamp,
    bool smartRankingEnabled, bool userLearningEnabled,
    bool phraseCompletionEnabled, bool engineeringVocabularyEnabled) const {
  std::vector<Candidate> expanded = candidates;
  std::unordered_set<std::string> values;
  for (const Candidate& candidate : expanded) {
    values.insert(candidate.value);
  }

  if (phraseCompletionEnabled && engineeringVocabularyEnabled &&
      !completionPrefix.empty()) {
    double completionBase = candidates.empty() ? -8.0 : candidates.back().baseScore;
    for (const EngineeringLexicon::Entry& entry :
         lexicon_.completionsForPrefix(completionPrefix,
                                       weights_.completionLimit)) {
      if (values.contains(entry.phrase)) {
        continue;
      }
      expanded.push_back(Candidate{completionPrefix, entry.phrase, entry.phrase,
                                   completionBase, expanded.size(), true});
      values.insert(entry.phrase);
    }
  }

  std::unordered_map<std::string, std::vector<std::string>> valuesByReading;
  for (const Candidate& candidate : expanded) {
    valuesByReading[candidate.reading].push_back(candidate.value);
  }

  std::unordered_map<std::string, LearningDatabase::CandidateFeatures>
      learned;
  if (userLearningEnabled && database_ != nullptr) {
    for (const auto& [reading, candidateValues] : valuesByReading) {
      auto features = database_->featuresForCandidates(
          reading, candidateValues, previousTokens, timestamp,
          weights_.learningHalfLifeSeconds, weights_.minimumObservations);
      for (auto& [value, valueFeatures] : features) {
        learned[CandidateLearningKey(reading, value)] = valueFeatures;
      }
    }
  }

  std::vector<RankedCandidate> ranked;
  ranked.reserve(expanded.size());
  for (const Candidate& candidate : expanded) {
    RankedCandidate output;
    static_cast<Candidate&>(output) = candidate;
    auto learnedIter =
        learned.find(CandidateLearningKey(candidate.reading, candidate.value));
    if (learnedIter != learned.end()) {
      output.contextScore = learnedIter->second.contextScore;
      output.userScore = learnedIter->second.userScore;
      output.recencyScore = learnedIter->second.recencyScore;
    }
    if (engineeringVocabularyEnabled) {
      output.domainScore =
          lexicon_.domainScore(candidate.value, previousTokens);
    }
    output.phraseCompletionScore = candidate.phraseCompletion ? 1.0 : 0.0;
    output.localModelScore =
        localModel_ == nullptr ? 0 : localModel_->score(previousTokens,
                                                        candidate.value);
    output.finalScore =
        weights_.base * output.baseScore +
        weights_.context * output.contextScore +
        weights_.user * output.userScore +
        weights_.recency * output.recencyScore +
        weights_.domain * output.domainScore +
        weights_.phraseCompletion * output.phraseCompletionScore +
        weights_.localModel * output.localModelScore;
    ranked.push_back(std::move(output));
  }

  if (smartRankingEnabled) {
    std::stable_sort(ranked.begin(), ranked.end(),
                     [](const RankedCandidate& lhs,
                        const RankedCandidate& rhs) {
                       if (lhs.finalScore == rhs.finalScore) {
                         return lhs.originalIndex < rhs.originalIndex;
                       }
                       return lhs.finalScore > rhs.finalScore;
                     });
  } else {
    std::stable_sort(ranked.begin(), ranked.end(),
                     [](const RankedCandidate& lhs,
                        const RankedCandidate& rhs) {
                       return lhs.originalIndex < rhs.originalIndex;
                     });
  }
  return ranked;
}

bool SmartCandidateReranker::observeSelection(
    const std::string& reading, const std::string& selectedCandidate,
    const std::vector<std::string>& previousTokens,
    const std::vector<std::string>& displayedCandidates, int selectedRank,
    double timestamp) {
  return database_ != nullptr &&
         database_->recordSelection(reading, selectedCandidate, previousTokens,
                                    displayedCandidates, selectedRank,
                                    timestamp);
}

bool SmartCandidateReranker::observeTypingCorrection(
    const std::string& rawKeys, const std::string& correctedKeys,
    bool accepted, double timestamp) {
  return database_ != nullptr &&
         database_->recordTypingCorrection(
             rawKeys, correctedKeys, accepted, timestamp,
             weights_.correctionFeedbackMaxAgeSeconds);
}

bool SmartCandidateReranker::shouldSuppressTypingCorrection(
    const std::string& rawKeys, const std::string& correctedKeys,
    double timestamp) const {
  return database_ != nullptr &&
         database_->shouldSuppressTypingCorrection(
             rawKeys, correctedKeys, timestamp,
             weights_.minimumCorrectionRejections,
             weights_.correctionFeedbackMaxAgeSeconds);
}

bool SmartCandidateReranker::resetLearningData() {
  return database_ != nullptr && database_->reset();
}

uint64_t SmartCandidateReranker::learningDatabaseSizeBytes() const {
  return database_ == nullptr ? 0 : database_->sizeBytes();
}

}  // namespace McBopomofo
