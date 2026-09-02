# 智慧小麥 Beta MVP v0.1 開發報告

## 狀態摘要

核心智慧排序、SQLite 學習、工程詞庫、phrase completion engine、設定權重與單元測試已完成。macOS app 的獨立身分、即時候選 reranking、選字學習、工程英文 token passthrough、Secure Input 防護、Debug Logging 與設定頁原始碼已接妥。

已使用 Xcode 27.0 beta 6（27A5252f）完成 Universal Release app 與 installer 建置，並以本機 Apple Development Personal Team 完成實機測試簽署。Beta 已獨立安裝及成功註冊，TIS 可辨識 `Smart McBopomofo Beta`；正式版 McBopomofo 未被修改。這類本機測試簽章不等同可供公開散布的 Developer ID 簽章。

## 架構

```text
鍵盤事件
  -> BopomofoReadingBuffer parsing
  -> ReadingGrid 組字與 walk
  -> McBopomofoLM 原始候選及 base score
  -> SmartCandidateReranker
       |-> 前 1/2/3 個 token context
       |-> SQLite user learning + recency decay
       |-> EngineeringLexicon.tsv
       |-> phrase completion engine
       `-> 保留 LocalModelProvider 介面
  -> InputState.ChoosingCandidate
  -> 使用者選字
  -> ReadingGrid override + SQLite observe
  -> output
```

詳細原版流程與插入點見 `ARCHITECTURE_NOTES.md`。

## Database schema

- `selection_events`：reading、selected candidate、context 1-3、顯示候選、原排名、時間。
- `candidate_stats`：reading + candidate + context key 的累積次數與最近使用時間。
- `typing_corrections`：短音節原始鍵、修正鍵、接受／撤銷計數與最後更新時間；兩次以上明確撤銷才抑制，90 天後失效。
- SQLite 使用 WAL 與 `synchronous=NORMAL`。
- context key 同時保留 global、unigram、bigram、trigram suffix。

## 測試與效能

- Xcode XCTest：125/125 通過。
- Engine CTest：137 項全部無失敗（其中 4 項原專案預設 skip）；新增英文 token 詞表、大小寫無關比對、三組修正回饋學習與查詢效能測試。
- KeyHandlerBopomofoTests 回歸：117/117 通過，包含一致的字母／數字／標點緩衝、純數字防誤判、短工程英文保護、單一冗餘鍵刪除、聲調轉中文、一聲空白、Shift、Caps Lock、工程符號、包含聲調鍵的單音節按鍵順序容錯、錯誤修正立即撤銷，以及關閉功能時保留原版行為。
- Xcode Swift Testing 的多數套件已通過，但完整程序在既有的台灣點字測試開始後未結束；已停止測試 host，列為 Xcode 27/macOS 27 相容性待查，不視為智慧排序測試失敗。
- Release build：`BUILD SUCCEEDED`，包含 arm64 與 x86_64；公開散布前仍需 Developer ID 簽署與 Apple notarization。
- 實機註冊：`All input sources enabled`、退出碼 0；TIS 同時保留正式版與 Beta 的獨立 Bundle ID。
- 安裝流程會保存並恢復安裝前的目前輸入來源，不會把 Beta 留為目前或預設輸入法。
- 包含一般中文回退、指定工程詞、三組補全、重複選字升權、一次誤選不污染、Reset、關閉智慧排序維持原順序。
- 20 個候選、SQLite 學習查詢開啟、500 次 rerank 平均：`0.015088 ms`。
- 自動修正回饋 SQLite 查詢 500 次平均：`0.002864 ms`。
- 兩項目標皆為 `< 10 ms`；本次核心 microbenchmark 均遠低於門檻。

上述數字是 reranking 核心，不含 InputMethodKit 候選窗繪製；完整 app latency 仍需 Xcode build 後以 Instruments 驗證。

## 已知限制

1. Phrase completion engine 與詞庫測試已完成，但尚未把「比目前 reading 更長的補全文字」接到 ReadingGrid 的安全選取/提交流程；live app 目前只啟用既有候選的排序，避免候選可見卻選不到。
2. Mixed Chinese / English 已支援英文優先的本機歧義緩衝、聲調判斷與個人化修正抑制；第一次遇到未收錄且恰好等同合法注音的極短英文，仍可能需要立即 Backspace 撤銷並累積兩次回饋。
3. Debug Logging 已有 Secure Input gate，但仍需完整 app smoke test 驗證各 client app 的 Secure Input 切換時機。
4. BIG-IP 問題目前只有診斷與安全觀測方案，沒有加入未證實的自動 workaround。詳見 `BIGIP_INPUT_METHOD_INVESTIGATION.md`。
5. Xcode 27 的完整 Swift Testing 執行在既有台灣點字測試處未正常結束；CTest 與 XCTest 均已獨立通過。

## 手動測試就緒判定

目前已達到可實際測試的程度。Beta 已安裝到 `~/Library/Input Methods/SmartMcBopomofoBeta.app`，並驗證獨立 Bundle Identifier、input source identifiers、InputMethodKit connection、UserDefaults domain、使用者資料目錄、Universal binary、Personal Team 簽章與 TIS 註冊。正式版仍可同時使用；Beta 未被設成預設輸入法。
