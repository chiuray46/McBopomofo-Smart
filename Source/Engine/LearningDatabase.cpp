// Copyright (c) 2026 and onwards The McBopomofo Authors.

#include "LearningDatabase.h"

#include <sqlite3.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace McBopomofo {
namespace {

constexpr char kSchema[] = R"SQL(
CREATE TABLE IF NOT EXISTS selection_events (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  reading TEXT NOT NULL,
  selected_candidate TEXT NOT NULL,
  context_1 TEXT NOT NULL DEFAULT '',
  context_2 TEXT NOT NULL DEFAULT '',
  context_3 TEXT NOT NULL DEFAULT '',
  displayed_candidates TEXT NOT NULL,
  selected_rank INTEGER NOT NULL,
  selected_at REAL NOT NULL
);
CREATE TABLE IF NOT EXISTS candidate_stats (
  reading TEXT NOT NULL,
  candidate TEXT NOT NULL,
  context_key TEXT NOT NULL,
  selection_count REAL NOT NULL,
  last_used REAL NOT NULL,
  PRIMARY KEY (reading, candidate, context_key)
);
CREATE INDEX IF NOT EXISTS candidate_stats_lookup
  ON candidate_stats(reading, context_key);
CREATE TABLE IF NOT EXISTS typing_corrections (
  raw_keys TEXT NOT NULL,
  corrected_keys TEXT NOT NULL,
  accepted_count REAL NOT NULL DEFAULT 0,
  rejected_count REAL NOT NULL DEFAULT 0,
  last_updated REAL NOT NULL,
  PRIMARY KEY (raw_keys, corrected_keys)
);
)SQL";

std::string Join(const std::vector<std::string>& values, size_t begin,
                 size_t end, char separator) {
  std::string result;
  for (size_t index = begin; index < end; ++index) {
    if (!result.empty()) {
      result.push_back(separator);
    }
    result += values[index];
  }
  return result;
}

bool BindText(sqlite3_stmt* statement, int index, const std::string& value) {
  return sqlite3_bind_text(statement, index, value.c_str(),
                           static_cast<int>(value.size()), SQLITE_TRANSIENT) ==
         SQLITE_OK;
}

}  // namespace

LearningDatabase::LearningDatabase(std::string path) : path_(std::move(path)) {}

LearningDatabase::~LearningDatabase() {
  if (db_ != nullptr) {
    sqlite3_close(db_);
  }
}

bool LearningDatabase::open() {
  if (db_ != nullptr) {
    return true;
  }
  std::error_code error;
  std::filesystem::path databasePath(path_);
  if (databasePath.has_parent_path()) {
    std::filesystem::create_directories(databasePath.parent_path(), error);
    if (error) {
      return false;
    }
  }
  if (sqlite3_open_v2(path_.c_str(), &db_,
                      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
                          SQLITE_OPEN_FULLMUTEX,
                      nullptr) != SQLITE_OK) {
    if (db_ != nullptr) {
      sqlite3_close(db_);
      db_ = nullptr;
    }
    return false;
  }
  sqlite3_busy_timeout(db_, 25);
  if (!execute("PRAGMA journal_mode=WAL;") ||
      !execute("PRAGMA synchronous=NORMAL;") || !createSchema()) {
    sqlite3_close(db_);
    db_ = nullptr;
    return false;
  }
  return true;
}

bool LearningDatabase::execute(const char* sql) const {
  if (db_ == nullptr) {
    return false;
  }
  return sqlite3_exec(db_, sql, nullptr, nullptr, nullptr) == SQLITE_OK;
}

bool LearningDatabase::createSchema() { return execute(kSchema); }

std::vector<std::string> LearningDatabase::ContextKeys(
    const std::vector<std::string>& context) {
  std::vector<std::string> keys;
  keys.emplace_back();
  size_t count = std::min<size_t>(context.size(), 3);
  for (size_t length = 1; length <= count; ++length) {
    keys.push_back(
        Join(context, context.size() - length, context.size(), '\x1f'));
  }
  return keys;
}

std::string LearningDatabase::EncodeCandidateList(
    const std::vector<std::string>& candidates) {
  std::string encoded;
  for (const std::string& candidate : candidates) {
    if (!encoded.empty()) {
      encoded.push_back('\x1e');
    }
    encoded += candidate;
  }
  return encoded;
}

bool LearningDatabase::recordSelection(
    const std::string& reading, const std::string& selectedCandidate,
    const std::vector<std::string>& context,
    const std::vector<std::string>& displayedCandidates, int selectedRank,
    double timestamp) {
  if (db_ == nullptr || reading.empty() || selectedCandidate.empty()) {
    return false;
  }
  if (!execute("BEGIN IMMEDIATE;")) {
    return false;
  }

  bool success = true;
  sqlite3_stmt* event = nullptr;
  const char* eventSql =
      "INSERT INTO selection_events(reading, selected_candidate, context_1, "
      "context_2, context_3, displayed_candidates, selected_rank, selected_at) "
      "VALUES(?, ?, ?, ?, ?, ?, ?, ?);";
  if (sqlite3_prepare_v2(db_, eventSql, -1, &event, nullptr) != SQLITE_OK) {
    success = false;
  } else {
    std::string c1 = context.empty() ? "" : context.back();
    std::string c2 = context.size() < 2 ? "" : context[context.size() - 2];
    std::string c3 = context.size() < 3 ? "" : context[context.size() - 3];
    success = BindText(event, 1, reading) &&
              BindText(event, 2, selectedCandidate) && BindText(event, 3, c1) &&
              BindText(event, 4, c2) && BindText(event, 5, c3) &&
              BindText(event, 6, EncodeCandidateList(displayedCandidates)) &&
              sqlite3_bind_int(event, 7, selectedRank) == SQLITE_OK &&
              sqlite3_bind_double(event, 8, timestamp) == SQLITE_OK &&
              sqlite3_step(event) == SQLITE_DONE;
  }
  sqlite3_finalize(event);

  const char* statsSql =
      "INSERT INTO candidate_stats(reading, candidate, context_key, "
      "selection_count, last_used) VALUES(?, ?, ?, 1, ?) "
      "ON CONFLICT(reading, candidate, context_key) DO UPDATE SET "
      "selection_count = selection_count + 1, last_used = excluded.last_used;";
  for (const std::string& contextKey : ContextKeys(context)) {
    if (!success) {
      break;
    }
    sqlite3_stmt* stats = nullptr;
    if (sqlite3_prepare_v2(db_, statsSql, -1, &stats, nullptr) != SQLITE_OK) {
      success = false;
    } else {
      success = BindText(stats, 1, reading) &&
                BindText(stats, 2, selectedCandidate) &&
                BindText(stats, 3, contextKey) &&
                sqlite3_bind_double(stats, 4, timestamp) == SQLITE_OK &&
                sqlite3_step(stats) == SQLITE_DONE;
    }
    sqlite3_finalize(stats);
  }

  if (success) {
    success = execute("COMMIT;");
  } else {
    execute("ROLLBACK;");
  }
  return success;
}

std::unordered_map<std::string, LearningDatabase::CandidateFeatures>
LearningDatabase::featuresForCandidates(
    const std::string& reading, const std::vector<std::string>& candidates,
    const std::vector<std::string>& context, double timestamp,
    double halfLifeSeconds, size_t minimumObservations) const {
  std::unordered_map<std::string, CandidateFeatures> result;
  if (db_ == nullptr || reading.empty() || candidates.empty()) {
    return result;
  }
  std::unordered_set<std::string> requested(candidates.begin(),
                                             candidates.end());
  std::vector<std::string> contextKeys = ContextKeys(context);

  const char* sql =
      "SELECT candidate, selection_count, last_used FROM candidate_stats "
      "WHERE reading = ? AND context_key = ?;";
  for (size_t keyIndex = 0; keyIndex < contextKeys.size(); ++keyIndex) {
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &statement, nullptr) != SQLITE_OK) {
      continue;
    }
    BindText(statement, 1, reading);
    BindText(statement, 2, contextKeys[keyIndex]);

    struct Row {
      double count = 0;
      double lastUsed = 0;
    };
    std::unordered_map<std::string, Row> rows;
    double total = 0;
    size_t types = 0;
    while (sqlite3_step(statement) == SQLITE_ROW) {
      const unsigned char* text = sqlite3_column_text(statement, 0);
      if (text == nullptr) {
        continue;
      }
      std::string candidate(reinterpret_cast<const char*>(text));
      double count = sqlite3_column_double(statement, 1);
      double lastUsed = sqlite3_column_double(statement, 2);
      double elapsed = std::max(0.0, timestamp - lastUsed);
      double decay = halfLifeSeconds > 0
                         ? std::exp2(-elapsed / halfLifeSeconds)
                         : 1.0;
      double decayedCount = count * decay;
      rows[candidate] = Row{decayedCount, lastUsed};
      total += decayedCount;
      ++types;
    }
    sqlite3_finalize(statement);

    if (total <= 0) {
      continue;
    }
    double orderWeight = keyIndex == 0
                             ? 0.25
                             : (keyIndex == 1 ? 0.50
                                              : (keyIndex == 2 ? 0.75 : 1.0));
    for (const auto& [candidate, row] : rows) {
      if (!requested.contains(candidate)) {
        continue;
      }
      CandidateFeatures& features = result[candidate];
      double confidence =
          (row.count + 1.0) / (total + static_cast<double>(types) + 1.0);
      bool trusted = row.count >= static_cast<double>(minimumObservations);
      if (keyIndex == 0) {
        features.observations = row.count;
        features.lastUsed = row.lastUsed;
        if (trusted) {
          features.userScore = confidence * std::log1p(row.count);
          features.recencyScore = halfLifeSeconds > 0
                                      ? std::exp2(-std::max(0.0, timestamp -
                                                                    row.lastUsed) /
                                                  halfLifeSeconds)
                                      : 1.0;
        }
      } else if (trusted) {
        features.contextScore =
            std::max(features.contextScore,
                     orderWeight * confidence * std::log1p(row.count));
      }
    }
  }
  return result;
}

bool LearningDatabase::recordTypingCorrection(
    const std::string& rawKeys, const std::string& correctedKeys,
    bool accepted, double timestamp, double feedbackMaxAgeSeconds) {
  if (db_ == nullptr || rawKeys.empty() || correctedKeys.empty()) {
    return false;
  }
  const char* sql =
      "INSERT INTO typing_corrections(raw_keys, corrected_keys, "
      "accepted_count, rejected_count, last_updated) VALUES(?, ?, ?, ?, ?) "
      "ON CONFLICT(raw_keys, corrected_keys) DO UPDATE SET "
      "accepted_count = CASE WHEN excluded.last_updated - last_updated > ? "
      "THEN excluded.accepted_count ELSE accepted_count + "
      "excluded.accepted_count END, "
      "rejected_count = CASE WHEN excluded.last_updated - last_updated > ? "
      "THEN excluded.rejected_count ELSE rejected_count + "
      "excluded.rejected_count END, last_updated = excluded.last_updated;";
  sqlite3_stmt* statement = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &statement, nullptr) != SQLITE_OK) {
    return false;
  }
  bool success =
      BindText(statement, 1, rawKeys) && BindText(statement, 2, correctedKeys) &&
      sqlite3_bind_double(statement, 3, accepted ? 1.0 : 0.0) == SQLITE_OK &&
      sqlite3_bind_double(statement, 4, accepted ? 0.0 : 1.0) == SQLITE_OK &&
      sqlite3_bind_double(statement, 5, timestamp) == SQLITE_OK &&
      sqlite3_bind_double(statement, 6, feedbackMaxAgeSeconds) == SQLITE_OK &&
      sqlite3_bind_double(statement, 7, feedbackMaxAgeSeconds) == SQLITE_OK &&
      sqlite3_step(statement) == SQLITE_DONE;
  sqlite3_finalize(statement);
  return success;
}

bool LearningDatabase::shouldSuppressTypingCorrection(
    const std::string& rawKeys, const std::string& correctedKeys,
    double timestamp, size_t minimumRejections,
    double feedbackMaxAgeSeconds) const {
  if (db_ == nullptr || rawKeys.empty() || correctedKeys.empty()) {
    return false;
  }
  const char* sql =
      "SELECT accepted_count, rejected_count, last_updated FROM "
      "typing_corrections WHERE raw_keys = ? AND corrected_keys = ?;";
  sqlite3_stmt* statement = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &statement, nullptr) != SQLITE_OK) {
    return false;
  }
  BindText(statement, 1, rawKeys);
  BindText(statement, 2, correctedKeys);
  bool suppress = false;
  if (sqlite3_step(statement) == SQLITE_ROW) {
    double acceptedCount = sqlite3_column_double(statement, 0);
    double rejectedCount = sqlite3_column_double(statement, 1);
    double lastUpdated = sqlite3_column_double(statement, 2);
    double age = std::max(0.0, timestamp - lastUpdated);
    double threshold = static_cast<double>(minimumRejections);
    suppress = age <= feedbackMaxAgeSeconds && rejectedCount >= threshold &&
               rejectedCount >= acceptedCount + threshold;
  }
  sqlite3_finalize(statement);
  return suppress;
}

bool LearningDatabase::reset() {
  if (db_ == nullptr) {
    return false;
  }
  return execute("BEGIN IMMEDIATE; DELETE FROM selection_events; DELETE FROM "
                 "candidate_stats; DELETE FROM typing_corrections; COMMIT;") &&
         execute("PRAGMA wal_checkpoint(TRUNCATE);");
}

uint64_t LearningDatabase::sizeBytes() const {
  std::error_code error;
  uint64_t total = 0;
  for (const std::string& suffix : {std::string(), std::string("-wal"),
                                    std::string("-shm")}) {
    std::filesystem::path file(path_ + suffix);
    if (std::filesystem::exists(file, error)) {
      total += std::filesystem::file_size(file, error);
    }
    error.clear();
  }
  return total;
}

}  // namespace McBopomofo
