# 智慧小麥 Beta

智慧小麥 Beta 是 McBopomofo 3.1 的獨立實驗分支。MVP v0.1 保留原有候選產生流程，僅在候選顯示前加入本機排序層。

## 安全隔離

- App：`SmartMcBopomofoBeta.app`
- Bundle Identifier：`org.openvanilla.inputmethod.SmartMcBopomofoBeta`
- Input Source：`org.openvanilla.inputmethod.SmartMcBopomofoBeta.*`
- InputMethodKit connection：`SmartMcBopomofoBeta_1_Connection`
- Preferences：獨立 Bundle Identifier 所屬的 UserDefaults domain
- 使用者資料：`~/Library/Application Support/SmartMcBopomofoBeta/`
- 學習資料庫：上述目錄內的 `smart-learning.sqlite3`
- 除錯紀錄：上述目錄內的 `Logs/smart-ranking.jsonl`
- 安裝目的地：`~/Library/Input Methods/SmartMcBopomofoBeta.app`

正式版 `McBopomofo.app`、正式版偏好與正式版使用者詞庫不在 Beta 的讀寫路徑內。Beta 不含正式版自動更新端點，也不會自行註冊或切換成預設輸入法。

## Privacy architecture

所有候選排序與學習都在本機完成；MVP 沒有網路模型、ChatGPT API 或遙測。SQLite 僅記錄注音、顯示候選、最終選擇、最多三個前文 token、短音節修正的接受／撤銷計數、次數與時間。

偵測到 macOS Secure Event Input 時：

- 不寫入選字學習資料；
- 不寫入輸入、前文、候選或選字除錯紀錄；
- 不嘗試停用或繞過 Secure Input。

Debug Logging 預設關閉。開啟後才會在 Beta 專用資料夾寫入逐行 JSON；`Reset Learning Data` 會清除學習表，不會碰使用者詞庫。

## 智慧排序

```text
FinalScore =
    baseWeight              * BaseLanguageModelScore
  + contextWeight           * ContextScore
  + userWeight              * UserLearningScore
  + recencyWeight           * RecencyScore
  + domainWeight            * DomainScore
  + phraseCompletionWeight  * PhraseCompletionScore
  + localModelWeight        * LocalModelScore
```

權重位於 `Source/SmartResources/SmartRankingConfig.json`。工程詞彙位於 `Source/SmartResources/EngineeringLexicon.tsv`，可獨立更新，不寫死在核心。

一次選錯不會產生加權：同一候選至少累積兩次觀察後才啟用學習分數；近期性使用 30 天半衰期衰減。關閉 Smart Candidate Ranking 時保留原候選順序。

## 中英混合輸入

開啟 `Mixed Chinese / English Input` 後，標準注音配置內的字母、數字與標點按鍵都會先顯示成尚未送出的原始按鍵，同時在本機平行解析注音。如果三個鍵以內形成完全相符的合法注音音節，並以聲調鍵或一聲空白結束，才會轉成中文；較長、重複覆寫音節組件或無法形成注音的按鍵序列維持英文。

同一音節內若聲母、介音、韻母或聲調鍵的按鍵順序顛倒，Beta 會嘗試排列。正常順序永遠優先；只有原順序不合法、且重新排列後恰好只有一個合法讀音時才修正。例如標準配置的 `8a7` 與聲調提早按下的 `7a8`，都會得到與 `a87` 相同的結果。這項容錯不會重新排列跨音節文字、長英文或純數字。

`Smart Typing Correction` 會額外處理快速輸入時多按一個注音鍵的情況。明確聲調鍵視為中文意圖；一聲空白則只有在已有中文前文或出現相鄰重複鍵時才刪除冗餘鍵。短工程英文與常見技術 token（例如 `fea`、`cad`、`amr`、`api`、`vpn`）會由獨立詞表保護，不因恰好能形成注音而轉成中文。若自動調整猜錯，緊接著按一次 Backspace 會還原完整原始按鍵；繼續輸入其他鍵才視為接受修正。同一修正被撤銷至少兩次且撤銷次數明顯高於接受次數時，User Learning 會暫停該修正；回饋 90 天後失效，也可用 `Reset Learning Data` 清除。所有修正仍在本機完成，Secure Input 下不學習也不寫入 Debug Log。

為符合目前測試偏好，`Phrase Completion` 預設關閉；需要時仍可在設定頁手動開啟。

支援 Shift 與 Caps Lock，例如：

- `SOLIDWORKS 板金`
- `PPA-CF 材料`
- `M8 螺絲`
- `RealSense 相機`
- `TPU 95A`

例如 `solidworks` 會先維持英文，`su3` 則會在三聲鍵輸入後解析成「你」。英文只在空白或其他邊界確認時送出，因此不需要先把文字插入文件再回頭刪除。大寫、數字及工程符號仍支援直接 passthrough。關閉本功能後會保留原版 McBopomofo 的按鍵行為。

## Build

已使用 Xcode 27.0 beta 6 完成 Universal Release 建置。核心可獨立驗證：

```text
cmake -S Source/Engine -B work/engine -DENABLE_TEST=ON -DCMAKE_PREFIX_PATH=/opt/homebrew
cmake --build work/engine -j4
ctest --test-dir work/engine --output-on-failure
```

Release app 可透過 `McBopomofo` scheme 建置。macOS 27 實機註冊需要 Apple Development 或 Developer ID 簽章；ad-hoc `Sign to Run Locally` 可供建置驗證，但不保證能加入 TIS 清單。安裝器只以 `SmartMcBopomofoBeta.app` 為目的地，並會保存及恢復安裝前的目前輸入來源；除非使用者明確確認，不應執行安裝或註冊。
