# 智慧小麥 Beta 架構筆記

最後更新：2026-08-30T13:13:10+08:00

基線：McBopomofo 3.1，tag commit `e965b78296b1322d11ce672aaf626c5e65411881`。

## 安全與隔離邊界

智慧小麥 Beta 必須與正式版同時存在，且不能讀寫正式版狀態。

| 項目 | 正式版 | 智慧 Beta |
| --- | --- | --- |
| App | `McBopomofo.app` | `SmartMcBopomofoBeta.app` |
| Bundle Identifier | `org.openvanilla.inputmethod.McBopomofo` | `org.openvanilla.inputmethod.SmartMcBopomofoBeta` |
| Input Source ID | `org.openvanilla.inputmethod.McBopomofo.*` | `org.openvanilla.inputmethod.SmartMcBopomofoBeta.*` |
| IMK connection | `McBopomofo_1_Connection` | `SmartMcBopomofoBeta_1_Connection` |
| UserDefaults domain | 正式版 bundle domain | Beta bundle domain |
| 使用者資料 | `Application Support/McBopomofo` | `Application Support/SmartMcBopomofoBeta` |
| 學習資料庫 | 無持久化 SQLite | Beta 資料夾內 `learning.sqlite3` |
| Debug log | 正式版行為 | Beta 專用 log，預設關閉 |

建置階段只產生 DerivedData 內的 Beta bundle，不執行 installer、不註冊 TIS、不啟用輸入來源、不改預設輸入法。

## 現有三層架構

### Swift / InputMethodKit 層

- `Source/main.swift`
  - 建立 `IMKServer`。
  - 以 bundle identifier 向 macOS 註冊 input method server connection。
- `Source/InputMethodController.swift`
  - `IMKInputController` 入口。
  - 收取 client key event、持有目前 `InputState`、把 composing text 與 candidate window 回寫給 client。
- `Source/InputState.swift`
  - 不可變狀態物件。
  - `Empty`、`Inputting`、`ChoosingCandidate`、`AssociatedPhrases`、`Committing` 等狀態形成單向資料流。
- `Source/InputMethodController+CandidateControllerDelegate.swift`
  - 候選視窗選取事件的終點。
  - 呼叫 `KeyHandler.fixNode(...)` 寫回使用者選擇。
- `Source/Preferences.swift` 與 Preferences UI
  - 使用 `UserDefaults.standard`，因此 bundle identifier 必須隔離。

### Objective-C++ 橋接層

- `Source/KeyHandler.mm`
  - 主要鍵盤與狀態轉換邏輯。
  - 使用 `BopomofoReadingBuffer` 解析注音。
  - 把完整讀音插入 `ReadingGrid`，呼叫 `_walk()`。
  - `_buildCandidateStateFromInputtingState(...)` 從 grid 取得候選並建立 Swift `InputState.Candidate`。
  - `fixNode(...)` 套用候選 override，並把選擇交給 `UserOverrideModel.observe(...)`。
- `Source/LanguageModelManager.mm`
  - 管理主詞庫、傳統注音詞庫、使用者詞彙、排除詞、替換表及 `UserOverrideModel`。
  - 現有 `UserOverrideModel` 僅在記憶體中，重啟後消失。

### C++ 引擎層

- `Source/Engine/Mandarin`
  - 鍵盤配置、聲母韻母聲調與注音 syllable parsing。
- `Source/Engine/McBopomofoLM.*`
  - 合併主詞庫與 user phrase，套用 exclusion、replacement、converter。
- `Source/Engine/gramambular2/reading_grid.*`
  - 依連續讀音建立 1 至 8 音節 node。
  - 每個 node 含同讀音的 unigram candidates。
  - DAG/Viterbi walk 以 unigram score 總和找最佳路徑。
- `Source/Engine/UserOverrideModel.*`
  - 觀察前兩個 node + 目前候選的局部 context。
  - 使用 count、recency decay 與 LRU，但容量只有 500 且不持久化。
- `Source/Engine/UserPhrasesLM.*`
  - 使用者自訂與排除詞的文字檔模型。
- `Source/Data`
  - `BPMFBase.txt`、`BPMFMappings.txt`、`phrase.occ` 生成內建語言模型。

## 現有輸入流程

```text
macOS client key event
  -> InputMethodController.handle(event:client:)
  -> KeyHandler.handleInput(...)
  -> BopomofoReadingBuffer / Mandarin parsing
  -> completed syllable reading
  -> ReadingGrid.insertReading(...)
  -> McBopomofoLM.getUnigrams(...)
  -> grid node/candidate generation
  -> ReadingGrid.walk() unigram Viterbi ranking
  -> UserOverrideModel.suggest(...) soft override
  -> InputState.Inputting / ChoosingCandidate
  -> CandidateUI
  -> user selection
  -> KeyHandler.fixNode(...)
  -> UserOverrideModel.observe(...)
  -> InputMethodController commits text to IMK client
```

對照需求的簡化流程：

```text
鍵盤輸入
  -> 注音 parsing
  -> 組字 ReadingGrid
  -> language model
  -> candidate generation
  -> base candidate ranking
  -> context-aware reranking
  -> user override / explicit selection
  -> output
```

## 候選產生與排序的現況

`ReadingGrid::candidatesAt()` 會收集游標位置重疊的 node，先依 spanning length 由長到短排列，再依 node 內已由 base score 排序的 unigram 順序輸出。候選物件目前只有 reading、value、rawValue，沒有攜帶 score。

自動組字路徑由 `ReadingGrid::walk()` 計算；手動候選清單由 `candidatesAt()` 產生。這是兩個不同層面：

- 自動組字：決定組字區預設顯示什麼。
- 候選清單：決定打開選字窗時的順序。

MVP 先只 rerank 候選清單並做可控的 soft override，不改寫主詞庫與 Viterbi 演算法，降低回歸風險。

## 智慧排序層插入點

最佳插入點是：

1. `ReadingGrid::candidatesAt()` 已保留所有原始候選後；
2. `KeyHandler::_buildCandidateStateFromInputtingState(...)` 建立 UI state 前；
3. 使用者選取後，在 `KeyHandler::fixNode(...)` 寫入學習事件。

理由：

- 不改候選生成，Smart Ranking 關閉時可直接回到原順序。
- 能取得目前 walk 的前 1 至 3 個詞作 context。
- 能記錄 displayed candidates、原始 rank 與 final choice。
- reranker 可獨立 benchmark，不讓 SQLite 查詢散落在 key handling。

```text
ReadingGrid.candidatesAt()
  -> [candidate + base score]
  -> SmartCandidateReranker.rank(context, candidates)
      -> ContextModel
      -> LearningDatabase
      -> EngineeringLexicon
      -> PhraseCompletion
  -> InputState.ChoosingCandidate
```

## MVP 模組邊界

### `SmartCandidateReranker`

- 純本機、決定性排序。
- 輸入 candidate、base score、最多三個前文 token。
- 輸出每個候選的分數明細與排序。
- Smart Ranking 關閉時保留原始 order。

### `LearningDatabase`

除了候選選擇事件與 context 統計，Beta 也用 `typing_corrections` 保存短音節自動修正的接受／撤銷計數。單次撤銷不會改變行為；重複明確撤銷才暫停完全相同的 raw/corrected pair，且回饋有 90 天時效。Secure Input 下不讀寫這項個人回饋，Reset Learning Data 會與候選學習一起清除。

- SQLite WAL database。
- 學習資料只存在 Beta application support 目錄。
- 一次選錯不立即強推：需要 minimum observations、confidence 與 exponential decay。
- 提供 reset 與 database size。

### `EngineeringLexicon`

- 獨立 TSV resource，不 hard-code 在 C++。
- 提供 domain boost、mixed English token 與 completion prefix。
- 後續可單獨替換/更新檔案。

### `PhraseCompletion`

- 只增加候選，不自動 commit。
- MVP 使用 prefix index；未來可換成本機模型。

### `LocalModelProvider`

- 保留介面，MVP 的 implementation 為 no-op。
- 未來可接 Foundation Models/Core ML，但 rerank path 不進行網路呼叫。

## Scoring

所有權重由 `SmartRankingConfig.json` 與 Preferences 載入。

```text
FinalScore =
    BaseWeight * BaseLanguageModelScore
  + ContextWeight * ContextScore
  + UserWeight * UserLearningScore
  + RecencyWeight * RecencyScore
  + DomainWeight * DomainScore
  + PhraseCompletionWeight * PhraseCompletionScore
```

Base score 保留原 McBopomofo log score。其他項目為有上限的正向 boost，避免智慧層完全壓過語言模型。

建議 MVP 預設：

| 權重 | 預設值 |
| --- | ---: |
| Base | 1.00 |
| Context | 0.75 |
| User | 1.10 |
| Recency | 0.30 |
| Domain | 0.65 |
| Completion | 0.55 |

同分時使用原始 index 作穩定排序，確保 deterministic fallback。

## 學習與抗污染策略

- 第一次選擇只記錄，不足以產生強 boost。
- 同 context + reading + candidate 至少兩次才開始顯著提升。
- confidence 使用平滑比例，避免 1/1 成為 100%。
- count 與 recency 依半衰期衰減。
- 明確選擇其他候選會提高 competing candidate 的證據，不永久鎖定舊選擇。
- Reset Learning Data 以 transaction 清空學習表，不動工程詞庫或 Preferences。

## Mixed Chinese / English

MVP 不把英文字串送到語言模型。策略是：

- 在 key handling 保留連續 ASCII/digit/hyphen token。
- lexicon 辨識 `SOLIDWORKS`、`M8`、`PPA-CF`、`TPU 95A` 等 token。
- token 作為 context 或 completion candidate。
- 不在 Secure Text Entry 中記錄或 log。

完整 passthrough 需要另加明確的 input state，避免把現有 `LetterBehavior` 變成大量條件分支；MVP 先保留介面與 lexicon 測試。

## macOS Text Input Source integration

- `Info.plist` 的 bundle ID、`TISInputSourceID`、input mode IDs 與 IMK connection name 都必須唯一。
- Beta build 只被動產出 app；註冊、enable、選為 input source 都是後續手動測試步驟。
- installer 必須以 Beta bundle name 為 target，不能向 `McBopomofo.app` 路徑寫入，也不能 kill 正式版 process。

## 隱私與 Secure Input

- 無網路 inference。
- Debug logging 預設關閉。
- log 不寫 client app 全文，只寫有限 context token 與 score；Secure Input 開啟時完全不記錄 input/candidate/selection。
- SQLite 不記錄密碼欄位；InputMethodKit 正常情況下不應收到 Secure Text Entry，但仍設第二層 guard。
- 所有 learning/debug 資料均可在 Settings reset。

## 不在 MVP 大改的部分

- 不取代 `McBopomofoLM`。
- 不把主模型改成 neural model。
- 不重寫 `ReadingGrid` 的 Viterbi。
- 不自動修復 BIG-IP 視窗或繞過 Secure Input。
- 不安裝、不註冊、不啟用 Beta，直到使用者明確確認。
