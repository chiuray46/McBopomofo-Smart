# BIG-IP 與第三方輸入法失效調查

最後更新：2026-08-30T13:13:10+08:00

## 已知現象

- 未連接 F5 BIG-IP VPN 時，McBopomofo 可正常輸入。
- VPN 登入後，第三方輸入法偶爾無法繼續輸入，Apple 原廠中文輸入法通常不受影響。
- 已驗證的復原動作是打開 BIG-IP Preferences，再以 Command-W 關閉殘留視窗，刷新文字輸入服務後可恢復，且 VPN 不必斷線。
- 單純 restart McBopomofo 或切換 input source 不一定有效。

這些證據較符合「BIG-IP 登入流程持有 Secure Input 或留下 stale input/focus session」；尚不能證明是 McBopomofo candidate engine 的錯誤。

## 相關 macOS 元件

### Secure Input

Secure Event Input 會限制其他 process 觀察鍵盤事件。正常的密碼輸入結束後，持有者應釋放。若 BIG-IP 的登入或密碼視窗未正常結束，第三方 IMK input method 可能無法重新取得事件；Apple 內建 input method 因為位於系統受信任流程，行為可能不同。

### InputMethodKit

`IMKServer` 維持輸入法 process 與 client app 的 connection。焦點切換、Secure Input 進出或 stale window lifecycle 可能造成：

- `activateServer` 沒有如預期重新建立 context；
- client 仍在舊 input session；
- 第三方 input method process 存活，但沒有收到 key event；
- composition state 與目前 selected TIS source 不一致。

### TIS / Text Input Source

TIS registration、enable、select 是不同狀態。`TISSelectInputSource` 成功碼本身不能證明後續會收到 key event。診斷必須同時確認：

- bundle/input mode 已註冊；
- source enabled；
- source selected；
- Secure Input owner；
- 真正安全的輸入測試。

### Focus 與 process lifecycle

打開再關閉 BIG-IP Preferences 可能觸發：

1. BIG-IP password/control window resign key；
2. Secure Input owner release；
3. active app/focus chain 重建；
4. `TextInputMenuAgent` / `TextInputSwitcher` 重新同步 selected source；
5. IMK client 重新 activate。

這能解釋為什麼 Command-W 有效，而 restart McBopomofo 不一定有效。

## 假設與驗證方式

| 假設 | 可觀測證據 | 非破壞性驗證 |
| --- | --- | --- |
| BIG-IP 殘留 Secure Input | Secure Input PID 指向 BIG-IP | 在失效當下記錄 owner PID、process path、視窗狀態 |
| TIS selection 被重設到 ABC | selected source ID 改變 | 比對 VPN 前、登入中、登入後 selected source |
| source selected 但 IMK context stale | source ID 正確但無 key event | Beta debug 僅記錄 activate/deactivate 與 event count，不記錄文字 |
| IMK process 被系統暫停/重啟 | PID/lifecycle 改變 | 記錄 process start/exit 與 IMK callbacks |
| focus stuck in hidden BIG-IP window | frontmost/key window 不符 | 只讀取 window owner/name，不自動關閉 |

## 智慧小麥安全診斷模式

Beta 可加入不含輸入內容的 lifecycle diagnostics：

- app launch / termination timestamp；
- `activateServer` / `deactivateServer` count；
- `setValue` input mode ID；
- key event received count，不含 key code、字元或 composing text；
- Secure Input boolean 與 owner PID（若公用 API/允許的診斷方式可取得）；
- selected TIS source ID；
- VPN presence 只記錄 boolean，不改連線。

診斷 log 預設關閉，開啟時仍不記錄密碼、候選內容或完整 client text。

## 可能的安全復原方案

只有在原因被重現並確認後才考慮：

1. 偵測 Beta 已 selected 但一段合理時間沒有收到任何 event。
2. 確認 VPN helper 存在且 VPN 仍連線。
3. 確認 Secure Input owner 是 BIG-IP；若 owner 是其他 app，立即停止。
4. 顯示使用者可確認的通知，不自動送鍵、不關閉視窗。
5. 使用者確認後才執行既有的 BIG-IP Preferences 開啟/關閉流程。
6. 重新確認 Secure Input 已釋放、TIS source 正確、VPN 仍連線。

MVP v0.1 不實作自動復原，只提供文件與未來的 lifecycle diagnostic interface。

## 明確禁止

- 不停用或繞過 Secure Input。
- 不攔截公司 VPN 認證。
- 不讀取、記錄或重播密碼。
- 不停止 `svpn`、不修改 VPN route/policy。
- 不在無法確認 Secure Input owner 時關閉任何視窗。
- 不以 kill McBopomofo 當成已證實的修復。

## 下一步診斷計畫

1. 在下一次真實失效時收集同一時間點的 Secure Input owner、selected TIS source、BIG-IP window/focus、IMK lifecycle event count。
2. 執行 Command-W workaround，立刻重取同一組證據。
3. 比對哪一個狀態真正改變。
4. 至少重現三次後，才決定是否能安全內建 recovery。

