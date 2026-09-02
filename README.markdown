# Smart McBopomofo Beta／智慧小麥 Beta

一套以 [OpenVanilla McBopomofo](https://github.com/openvanilla/McBopomofo) 為基礎的非官方實驗性 macOS 注音輸入法。

本專案專注於本機智慧候選排序、使用者學習、工程詞彙、注音按鍵順序容錯，以及不中斷的中英文混合輸入。所有智慧功能均在 Mac 本機執行，不使用雲端大型語言模型，也不會上傳鍵盤輸入或學習紀錄。

> [!IMPORTANT]
> 本專案是社群衍生版本，不是 OpenVanilla 官方版本，也未獲 OpenVanilla 或任何本文提及品牌的贊助、認可或背書。正式穩定使用請參考[官方 McBopomofo](https://github.com/openvanilla/McBopomofo)。

## 目前狀態

`v0.1.0-beta` 是測試版本，適合願意回報問題的技術測試者，不建議取代日常使用的正式輸入法。

- 獨立 App、Bundle Identifier、Preferences 與使用者資料目錄，可和官方 McBopomofo 同時安裝。
- 保留原候選產生流程，在顯示候選前加入 bigram／trigram context-aware reranking。
- 使用本機 SQLite 儲存選字統計、信心值與近期性衰減資料。
- 支援工程詞庫、基本詞組補全，以及常見工程英文 token。
- 支援部分注音鍵順序顛倒與短音節輸入修正。
- Debug Logging 預設關閉，Secure Input 時不學習、不記錄輸入內容。
- 不會自動取代官方版或設為預設輸入法。

功能細節、限制與測試數據請見 [SMART_BETA_README.md](SMART_BETA_README.md) 與 [MVP_V0.1_REPORT.md](MVP_V0.1_REPORT.md)。

## 隱私

智慧排序、工程詞庫與使用者學習均在本機完成。MVP 不包含遙測、廣告、ChatGPT API 或其他網路推論。

學習資料位於：

```text
~/Library/Application Support/SmartMcBopomofoBeta/
```

使用者可在設定中執行 `Reset Learning Data`。開啟 Debug Logging 前請先理解紀錄可能包含候選與有限前文；密碼欄位與 Secure Input 期間不會寫入學習或除錯紀錄。

## 從原始碼建置

需求：

- macOS 26 或更新版本
- Xcode 26 或更新版本
- Python 3.9 或更新版本

以 Xcode 開啟 `McBopomofo.xcodeproj`，選擇 `McBopomofo` scheme 建置。若使用專案附帶的安全建置工具：

```text
Source/Tools/build-smart-beta-safely.sh
```

該工具會在建置結束後撤銷暫存 App 的 Launch Services 登記，避免 macOS 輸入法選單累積重複項目。

## 安裝包狀態

公開 repository 目前以原始碼測試為主。未經 Developer ID 簽署與 Apple notarization 的安裝包不會列為一般使用者建議下載項目。請勿把本機 Apple Development／ad-hoc 簽署的建置誤認為可公開散布的正式安裝包。

## 上游、授權與品牌

本專案以 McBopomofo 3.1 commit `e965b78296b1322d11ce672aaf626c5e65411881` 為基線。原始 McBopomofo 與本衍生版本依根目錄 [LICENSE.txt](LICENSE.txt) 的 MIT License 發布；再散布時必須保留其中的版權聲明與授權文字。

第三方元件與資料來源見 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)，品牌聲明見 [TRADEMARKS.md](TRADEMARKS.md)。

## 已知限制

- 這是實驗性 Beta，部分應用程式、VPN／Secure Input 切換或新版 macOS 可能仍有相容性問題。
- Phrase completion 的安全選取／提交流程尚未完整接入即時 App。
- 完整 Swift Testing 流程在既有台灣點字測試處曾出現測試 host 未結束；核心 CTest 與指定 XCTest 已獨立通過。
- 公開版安裝包仍需 Developer ID 與 notarization。

## 問題回報與安全

一般問題可使用 GitHub Issues，請勿附上密碼、公司 VPN 位址、帳號、內部主機名稱、完整鍵盤紀錄或未遮蔽的除錯檔。安全問題請依 [SECURITY.md](SECURITY.md) 處理。

## 免責聲明

本軟體依 MIT License 以「現狀」提供，不附任何明示或默示擔保。此 repository 的說明是專案維護資訊，不構成法律意見；若要進行商業散布、使用第三方商標或處理其他特別法律風險，請諮詢合格法律專業人士。
