// Copyright (c) 2026 and onwards The McBopomofo Authors.

#ifndef SRC_ENGINE_SMARTCANDIDATERERANKER_H_
#define SRC_ENGINE_SMARTCANDIDATERERANKER_H_

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "EngineeringLexicon.h"
#include "LearningDatabase.h"
#include "LocalModelProvider.h"

namespace McBopomofo {

struct SmartScoringWeights {
  double base = 1.0;
  double context = 0.75;
  double user = 1.10;
  double recency = 0.30;
  double domain = 0.65;
  double phraseCompletion = 0.55;
  double localModel = 0;
  double learningHalfLifeSeconds = 2592000;
  size_t minimumObservations = 2;
  size_t completionLimit = 5;
  size_t minimumCorrectionRejections = 2;
  double correctionFeedbackMaxAgeSeconds = 7776000;

  bool loadFromJsonFile(const std::string& path);
};

class SmartCandidateReranker {
 public:
  struct Candidate {
    std::string reading;
    std::string value;
    std::string rawValue;
    double baseScore = 0;
    size_t originalIndex = 0;
    bool phraseCompletion = false;
  };

  struct RankedCandidate : Candidate {
    double contextScore = 0;
    double userScore = 0;
    double recencyScore = 0;
    double domainScore = 0;
    double phraseCompletionScore = 0;
    double localModelScore = 0;
    double finalScore = 0;
  };

  SmartCandidateReranker(
      std::unique_ptr<LearningDatabase> database,
      EngineeringLexicon lexicon, SmartScoringWeights weights,
      std::unique_ptr<LocalModelProvider> localModel =
          std::make_unique<NoOpLocalModelProvider>());

  std::vector<RankedCandidate> rank(
      const std::vector<Candidate>& candidates,
      const std::vector<std::string>& previousTokens,
      const std::string& completionPrefix, double timestamp,
      bool smartRankingEnabled, bool userLearningEnabled,
      bool phraseCompletionEnabled, bool engineeringVocabularyEnabled) const;

  bool observeSelection(const std::string& reading,
                        const std::string& selectedCandidate,
                        const std::vector<std::string>& previousTokens,
                        const std::vector<std::string>& displayedCandidates,
                        int selectedRank, double timestamp);

  bool observeTypingCorrection(const std::string& rawKeys,
                               const std::string& correctedKeys,
                               bool accepted, double timestamp);
  [[nodiscard]] bool shouldSuppressTypingCorrection(
      const std::string& rawKeys, const std::string& correctedKeys,
      double timestamp) const;

  bool resetLearningData();
  [[nodiscard]] uint64_t learningDatabaseSizeBytes() const;
  [[nodiscard]] size_t engineeringLexiconEntryCount() const {
    return lexicon_.entryCount();
  }
  [[nodiscard]] bool isKnownEnglishToken(const std::string& token) const {
    return lexicon_.isKnownEnglishToken(token);
  }

 private:
  std::unique_ptr<LearningDatabase> database_;
  EngineeringLexicon lexicon_;
  SmartScoringWeights weights_;
  std::unique_ptr<LocalModelProvider> localModel_;
};

}  // namespace McBopomofo

#endif  // SRC_ENGINE_SMARTCANDIDATERERANKER_H_
