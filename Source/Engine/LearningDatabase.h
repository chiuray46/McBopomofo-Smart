// Copyright (c) 2026 and onwards The McBopomofo Authors.

#ifndef SRC_ENGINE_LEARNINGDATABASE_H_
#define SRC_ENGINE_LEARNINGDATABASE_H_

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct sqlite3;

namespace McBopomofo {

class LearningDatabase {
 public:
  struct CandidateFeatures {
    double contextScore = 0;
    double userScore = 0;
    double recencyScore = 0;
    double observations = 0;
    double lastUsed = 0;
  };

  explicit LearningDatabase(std::string path);
  ~LearningDatabase();

  LearningDatabase(const LearningDatabase&) = delete;
  LearningDatabase& operator=(const LearningDatabase&) = delete;

  bool open();
  [[nodiscard]] bool isOpen() const { return db_ != nullptr; }

  bool recordSelection(const std::string& reading,
                       const std::string& selectedCandidate,
                       const std::vector<std::string>& context,
                       const std::vector<std::string>& displayedCandidates,
                       int selectedRank, double timestamp);

  std::unordered_map<std::string, CandidateFeatures> featuresForCandidates(
      const std::string& reading, const std::vector<std::string>& candidates,
      const std::vector<std::string>& context, double timestamp,
      double halfLifeSeconds, size_t minimumObservations) const;

  bool recordTypingCorrection(const std::string& rawKeys,
                              const std::string& correctedKeys, bool accepted,
                              double timestamp,
                              double feedbackMaxAgeSeconds);
  [[nodiscard]] bool shouldSuppressTypingCorrection(
      const std::string& rawKeys, const std::string& correctedKeys,
      double timestamp, size_t minimumRejections,
      double feedbackMaxAgeSeconds) const;

  bool reset();
  [[nodiscard]] uint64_t sizeBytes() const;
  [[nodiscard]] const std::string& path() const { return path_; }

 private:
  static std::vector<std::string> ContextKeys(
      const std::vector<std::string>& context);
  static std::string EncodeCandidateList(
      const std::vector<std::string>& candidates);

  bool execute(const char* sql) const;
  bool createSchema();

  std::string path_;
  sqlite3* db_ = nullptr;
};

}  // namespace McBopomofo

#endif  // SRC_ENGINE_LEARNINGDATABASE_H_
