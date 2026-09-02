// Copyright (c) 2026 and onwards The McBopomofo Authors.

#include "SmartCandidateReranker.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace McBopomofo {
namespace {

#ifndef SMART_RESOURCE_DIR
#define SMART_RESOURCE_DIR "../SmartResources"
#endif

class SmartCandidateRerankerTest : public testing::Test {
 protected:
  void SetUp() override {
    path_ = (std::filesystem::temp_directory_path() /
             ("smart-mcbopomofo-test-" +
              std::to_string(std::chrono::steady_clock::now()
                                 .time_since_epoch()
                                 .count()) +
              ".sqlite3"))
                .string();
  }

  void TearDown() override {
    std::error_code error;
    std::filesystem::remove(path_, error);
    std::filesystem::remove(path_ + "-wal", error);
    std::filesystem::remove(path_ + "-shm", error);
  }

  SmartCandidateReranker makeReranker(EngineeringLexicon lexicon = {}) {
    SmartScoringWeights weights;
    weights.minimumObservations = 2;
    return SmartCandidateReranker(
        std::make_unique<LearningDatabase>(path_), std::move(lexicon),
        weights);
  }

  std::string path_;
};

TEST_F(SmartCandidateRerankerTest, DisabledRankingPreservesOriginalOrder) {
  auto reranker = makeReranker();
  std::vector<SmartCandidateReranker::Candidate> candidates = {
      {"a", "甲", "甲", -5, 0, false},
      {"a", "乙", "乙", -1, 1, false},
  };
  auto result = reranker.rank(candidates, {}, "", 1000, false, true, false,
                              false);
  ASSERT_EQ(result.size(), 2);
  EXPECT_EQ(result[0].value, "甲");
  EXPECT_EQ(result[1].value, "乙");
}

TEST_F(SmartCandidateRerankerTest, RepeatedContextSelectionRaisesCandidate) {
  auto reranker = makeReranker();
  std::vector<std::string> context = {"我要", "設計", "法蘭"};
  std::vector<std::string> displayed = {"接頭", "螺帽", "軸承"};
  EXPECT_TRUE(reranker.observeSelection("ㄓㄡˊ-ㄔㄥˊ", "軸承", context,
                                        displayed, 2, 1000));
  EXPECT_TRUE(reranker.observeSelection("ㄓㄡˊ-ㄔㄥˊ", "軸承", context,
                                        displayed, 2, 1001));
  EXPECT_TRUE(reranker.observeSelection("ㄓㄡˊ-ㄔㄥˊ", "軸承", context,
                                        displayed, 2, 1002));

  std::vector<SmartCandidateReranker::Candidate> candidates = {
      {"ㄓㄡˊ-ㄔㄥˊ", "接頭", "接頭", -2.0, 0, false},
      {"ㄓㄡˊ-ㄔㄥˊ", "軸承", "軸承", -2.3, 1, false},
  };
  auto result = reranker.rank(candidates, context, "", 1003, true, true,
                              false, false);
  ASSERT_EQ(result.size(), 2);
  EXPECT_EQ(result[0].value, "軸承");
  EXPECT_GT(result[0].userScore, 0);
  EXPECT_GT(result[0].contextScore, 0);
}

TEST_F(SmartCandidateRerankerTest, SingleSelectionDoesNotPolluteRanking) {
  auto reranker = makeReranker();
  std::vector<std::string> context = {"法蘭"};
  EXPECT_TRUE(reranker.observeSelection("r", "誤選", context,
                                        {"正確", "誤選"}, 1, 1000));
  std::vector<SmartCandidateReranker::Candidate> candidates = {
      {"r", "正確", "正確", -1, 0, false},
      {"r", "誤選", "誤選", -1.1, 1, false},
  };
  auto result = reranker.rank(candidates, context, "", 1001, true, true,
                              false, false);
  EXPECT_EQ(result[0].value, "正確");
  EXPECT_EQ(result[1].userScore, 0);
}

TEST_F(SmartCandidateRerankerTest,
       RepeatedCorrectionRejectionSuppressesSameCorrection) {
  auto reranker = makeReranker();
  EXPECT_FALSE(reranker.shouldSuppressTypingCorrection("8a7", "a87", 1000));
  EXPECT_TRUE(
      reranker.observeTypingCorrection("8a7", "a87", false, 1000));
  EXPECT_FALSE(reranker.shouldSuppressTypingCorrection("8a7", "a87", 1001));
  EXPECT_TRUE(
      reranker.observeTypingCorrection("8a7", "a87", false, 1001));
  EXPECT_TRUE(reranker.shouldSuppressTypingCorrection("8a7", "a87", 1002));
  EXPECT_FALSE(
      reranker.shouldSuppressTypingCorrection("8a7", "a97", 1002));
}

TEST_F(SmartCandidateRerankerTest,
       AcceptedCorrectionsRequireAdditionalRejectionsToSuppress) {
  auto reranker = makeReranker();
  EXPECT_TRUE(reranker.observeTypingCorrection("8a7", "a87", true, 1000));
  EXPECT_TRUE(
      reranker.observeTypingCorrection("8a7", "a87", false, 1001));
  EXPECT_TRUE(
      reranker.observeTypingCorrection("8a7", "a87", false, 1002));
  EXPECT_FALSE(reranker.shouldSuppressTypingCorrection("8a7", "a87", 1003));
  EXPECT_TRUE(
      reranker.observeTypingCorrection("8a7", "a87", false, 1003));
  EXPECT_TRUE(reranker.shouldSuppressTypingCorrection("8a7", "a87", 1004));
}

TEST_F(SmartCandidateRerankerTest, CorrectionRejectionExpiresAndCanBeReset) {
  auto reranker = makeReranker();
  EXPECT_TRUE(
      reranker.observeTypingCorrection("8a7", "a87", false, 1000));
  EXPECT_TRUE(
      reranker.observeTypingCorrection("8a7", "a87", false, 1001));
  EXPECT_TRUE(reranker.shouldSuppressTypingCorrection("8a7", "a87", 1002));

  constexpr double kMoreThanNinetyDays = 7776001;
  EXPECT_FALSE(reranker.shouldSuppressTypingCorrection(
      "8a7", "a87", 1001 + kMoreThanNinetyDays));

  EXPECT_TRUE(reranker.resetLearningData());
  EXPECT_FALSE(reranker.shouldSuppressTypingCorrection("8a7", "a87", 1003));
}

TEST_F(SmartCandidateRerankerTest, ResetRemovesLearnedBoost) {
  auto reranker = makeReranker();
  EXPECT_TRUE(reranker.observeSelection("r", "乙", {"甲"}, {"甲", "乙"},
                                        1, 1000));
  EXPECT_TRUE(reranker.observeSelection("r", "乙", {"甲"}, {"甲", "乙"},
                                        1, 1001));
  EXPECT_GT(reranker.learningDatabaseSizeBytes(), 0u);
  EXPECT_TRUE(reranker.resetLearningData());
  std::vector<SmartCandidateReranker::Candidate> candidates = {
      {"r", "甲", "甲", -1, 0, false},
      {"r", "乙", "乙", -1.1, 1, false},
  };
  auto result = reranker.rank(candidates, {"甲"}, "", 1002, true, true,
                              false, false);
  EXPECT_EQ(result[0].value, "甲");
  EXPECT_EQ(result[1].userScore, 0);
}

TEST(EngineeringLexiconTest, LoadsDomainAndCompletionData) {
  std::filesystem::path path = std::filesystem::temp_directory_path() /
                               "smart-mcbopomofo-lexicon.tsv";
  {
    std::ofstream output(path);
    output << "有限元素分析\tsimulation\t1.0\t有限元素\t應力|模擬\n";
    output << "法蘭軸承\tmechanical\t1.0\t法蘭軸\t法蘭\n";
  }
  EngineeringLexicon lexicon;
  ASSERT_TRUE(lexicon.loadFromFile(path.string()));
  EXPECT_EQ(lexicon.entryCount(), 2u);
  EXPECT_FALSE(lexicon.isKnownEnglishToken("有限元素分析"));
  EXPECT_GT(lexicon.domainScore("法蘭軸承", {"法蘭"}), 1.0);
  auto completions = lexicon.completionsForPrefix("有限元素", 5);
  ASSERT_EQ(completions.size(), 1u);
  EXPECT_EQ(completions[0].phrase, "有限元素分析");
  std::error_code error;
  std::filesystem::remove(path, error);
}

TEST(EngineeringLexiconTest, RecognizesEnglishTokensCaseInsensitively) {
  EngineeringLexicon lexicon;
  ASSERT_TRUE(lexicon.loadFromFile(
      std::string(SMART_RESOURCE_DIR) + "/EngineeringLexicon.tsv"));
  EXPECT_TRUE(lexicon.isKnownEnglishToken("fea"));
  EXPECT_TRUE(lexicon.isKnownEnglishToken("PPA-CF"));
  EXPECT_TRUE(lexicon.isKnownEnglishToken("realsense"));
  EXPECT_TRUE(lexicon.isKnownEnglishToken("test"));
  EXPECT_FALSE(lexicon.isKnownEnglishToken("unknownword"));
}

TEST(EngineeringLexiconTest, CoversRequiredEngineeringScenarios) {
  EngineeringLexicon lexicon;
  ASSERT_TRUE(lexicon.loadFromFile(
      std::string(SMART_RESOURCE_DIR) + "/EngineeringLexicon.tsv"));

  const std::vector<std::pair<std::string, std::string>> completions = {
      {"有限元素", "有限元素分析"}, {"法蘭軸", "法蘭軸承"},
      {"SOLID", "SOLIDWORKS"}};
  for (const auto& [prefix, expected] : completions) {
    auto results = lexicon.completionsForPrefix(prefix, 8);
    EXPECT_NE(std::find_if(results.begin(), results.end(),
                           [&](const auto& entry) {
                             return entry.phrase == expected;
                           }),
              results.end())
        << prefix;
  }

  const std::vector<std::pair<std::string, std::vector<std::string>>> cases = {
      {"法蘭軸承", {"我要", "設計", "法蘭"}},
      {"有限元素分析", {"進行", "模擬"}},
      {"SOLIDWORKS", {"板金"}},
      {"M8 螺絲", {"機械設計"}},
      {"PPA-CF", {"材料"}},
      {"RealSense", {"相機"}},
  };
  for (const auto& [candidate, context] : cases) {
    EXPECT_GT(lexicon.domainScore(candidate, context), 0) << candidate;
  }
}

TEST_F(SmartCandidateRerankerTest, GeneralChineseFallsBackToBaseModel) {
  auto reranker = makeReranker();
  std::vector<SmartCandidateReranker::Candidate> candidates = {
      {"ㄐㄧㄣ-ㄊㄧㄢ", "今天", "今天", -1, 0, false},
      {"ㄐㄧㄣ-ㄊㄧㄢ", "金天", "金天", -3, 1, false},
  };
  auto result = reranker.rank(candidates, {"我們"}, "", 1000, true, true,
                              true, true);
  ASSERT_EQ(result.size(), 2);
  EXPECT_EQ(result[0].value, "今天");
}

TEST_F(SmartCandidateRerankerTest, RerankingLatencyStaysBelowTarget) {
  auto reranker = makeReranker();
  std::vector<SmartCandidateReranker::Candidate> candidates;
  for (size_t index = 0; index < 20; ++index) {
    candidates.push_back({"reading", "candidate" + std::to_string(index),
                          "", -static_cast<double>(index), index, false});
  }
  auto start = std::chrono::steady_clock::now();
  for (size_t iteration = 0; iteration < 500; ++iteration) {
    auto result = reranker.rank(candidates, {"我要", "設計", "法蘭"}, "",
                                1000 + iteration, true, true, false, false);
    ASSERT_EQ(result.size(), candidates.size());
  }
  auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - start);
  double averageMilliseconds =
      static_cast<double>(elapsed.count()) / 500.0 / 1000.0;
  std::cout << "smart_reranking_average_ms=" << averageMilliseconds
            << std::endl;
  EXPECT_LT(averageMilliseconds, 10.0);
}

TEST_F(SmartCandidateRerankerTest, CorrectionFeedbackLookupStaysBelowTarget) {
  auto reranker = makeReranker();
  ASSERT_TRUE(
      reranker.observeTypingCorrection("8a7", "a87", false, 1000));
  constexpr size_t kIterations = 500;
  auto start = std::chrono::steady_clock::now();
  for (size_t iteration = 0; iteration < kIterations; ++iteration) {
    EXPECT_FALSE(reranker.shouldSuppressTypingCorrection(
        "8a7", "a87", 1001 + static_cast<double>(iteration)));
  }
  auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - start);
  double averageMilliseconds =
      static_cast<double>(elapsed.count()) /
      static_cast<double>(kIterations) / 1000.0;
  std::cout << "correction_feedback_lookup_average_ms="
            << averageMilliseconds << std::endl;
  EXPECT_LT(averageMilliseconds, 10.0);
}

}  // namespace
}  // namespace McBopomofo
