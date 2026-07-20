# WimForge three-pass usage test / 三輪完整使用測試 — 2026-07-20

This completion audit records three full, safe usage passes against the desktop
application, documentation site, install layout, and container surface. The
baseline was `72c378e8ce07cd56da8e8cfec6a7707a5cf5942e` on `main`.

## English

### Pass profiles and results

1. **Pass 1 — bilingual, dark, 1,440 × 900.** Built the Debug desktop,
   executed all 30 then-current CTest targets, rendered every one of the 13
   application routes, and visually inspected every capture. This pass found
   transferred-repository URL drift, a non-relocatable developer install,
   untranslated shared overlays, and Settings claims that exceeded the
   implemented runtime behavior.
2. **Pass 2 — English, light, 900 × 640.** Rebuilt after the first fixes, ran
   all 32 tests, regenerated and inspected all 13 compact captures, installed
   to an isolated prefix, and exercised the installed executable through the
   Overview, Unattended Studio, Virtual Machine Lab, Settings, Group Policy,
   search, and notification paths. This pass found forced-bilingual OpenCode
   state, language initialization after demo creation, and shared temporary
   state in the interactive demo test.
3. **Pass 3 — Hong Kong Cantonese, dark, 1,440 × 900.** Regenerated and
   inspected all 13 routes in `zh-HK` and reran the complete automated matrix.
   This pass found and fixed the
   remaining English-only WinForge readiness banner. The final canonical
   gallery uses bilingual dark mode, and the two documentation-site captures
   cover exact 1,280 × 720 and 390 × 844 CSS viewports.

### Corrections delivered

- Updated active publication and documentation references for the repository
  transfer to `Ding-Ding-Projects/WimForge`, while preserving deliberately
  historical ownership records.
- Made the GNU install layout relocatable and deployed both desktop and CLI
  executables, including their required Qt runtime, under the selected prefix.
- Localized shared history, snackbar, update, OpenCode, and WinForge state labels;
  made live language switching refresh stored status, retained external-process
  diagnostics under localized labels, initialized language before project/demo
  state, and made Settings truthful about CPU, scratch reserve, journal policy,
  checkpoint policy, expected hashes, best-effort logging, and notification-only
  tombstones.
- Isolated interactive QA with a unique temporary demo root, fixed the update
  dialog accessibility role, and added regression contracts for the corrected
  behavior.
- Made automatic export validate its destination before enablement and write an
  atomic, complete `.wimforge` bundle after project commits.
- Bounded the Docker build context with `/build-*`, added a contract test, and
  removed all compiler warnings reported by the Linux container build.
- Prevented the documentation hero's compact eyebrow from establishing an
  overwide minimum grid size. Site captures were taken with isolated Edge
  device metrics; the in-app browser could not attach after three bounded
  attempts, so it was not treated as evidence.

### Verification and safety boundary

- Local target: 32/32 CTest tests.
- Container test target: 25/25 CTest tests; authenticated provisioning smoke,
  digest, isolation, health, and one-shot rendering checks passed.
- Bootstrap-build, Compose contract/config, strict MkDocs, site/link/assets,
  canonical Wiki synchronization, screenshot manifest, and diff hygiene gates
  passed.
- All 13 application screenshots and both site captures were regenerated and
  visually inspected; no partial gallery was retained.
- Usage testing stayed in the populated demo and isolated install roots. It did
  not service a real Windows image, install host packages, start or delete a
  real VM, or execute a reviewed servicing plan, because those actions can
  modify external systems and require operator-selected inputs.

## 香港粵語

### 三輪測試設定同結果

1. **第一輪 — 雙語、深色、1,440 × 900。** Build 咗 Debug desktop，跑晒當時
   30 個 CTest target，再逐頁產生同肉眼核對 13 張 app 截圖。呢輪搵到 repo
   轉移後仲有舊 URL、developer install 唔可以搬去其他 prefix、共用 overlay
   未翻譯，同 Settings 寫咗一啲 runtime 實際未做到嘅描述。
2. **第二輪 — English、淺色、900 × 640。** 修正後重新 build，跑晒 32 個
   test，重新產生兼核對 13 張 compact 截圖，再裝去隔離 prefix，用安裝後嘅
   executable 實際行過 Overview、Unattended Studio、Virtual Machine Lab、
   Settings、Group Policy、搜尋同通知流程。呢輪搵到 OpenCode 狀態被迫顯示
   雙語、demo 建立後先套用語言，同 interactive demo test 共用 temp 狀態。
3. **第三輪 — 香港粵語、深色、1,440 × 900。** 用 `zh-HK` 重新產生兼核對
   13 個 route，再跑完整自動測試。呢輪搵到最後一條只得
   English 嘅 WinForge ready banner，亦已經修好。最後 canonical gallery 用
   雙語深色；兩張文件網站截圖就用準確 1,280 × 720 同 390 × 844 CSS viewport。

### 今次交付嘅修正

- 將現行發佈同文件連結更新去 `Ding-Ding-Projects/WimForge`，有歷史用途嘅
  舊 owner 記錄就原樣保留。
- GNU install layout 而家可以跟指定 prefix 搬位；desktop、CLI 同需要嘅 Qt
  runtime 都會裝入正確位置。
- 補齊共用 history、snackbar、update、OpenCode 同 WinForge 狀態標籤嘅翻譯；切換
  語言會即時更新已儲狀態，外部 process 診斷亦會保留喺本地化標籤後面；
  project／demo 建立前先套用語言；Settings 對 CPU、scratch reserve、journal
  policy、檢查點 policy、預期 hash、盡力啟動 logging 同只限通知嘅墓碑記錄，
  而家都會按實際行為講清楚。
- Interactive QA 每次用獨立 temp demo root，修正 update dialog accessibility
  role，並加入相應 regression contract。
- 自動匯出會喺啟用前驗證目的地，工程 commit 後會原子寫出完整 `.wimforge`
  bundle。
- 用 `/build-*` 限住 Docker build context、加 contract test，亦清走 Linux
  container build 報出嘅全部 compiler warning。
- Compact 文件 hero 嘅 eyebrow 唔會再撐闊 grid。網站截圖用隔離 Edge device
  metrics 產生；in-app browser 試咗三次都 attach 唔到，所以冇當佢係測試證據。

### 驗證同安全界線

- 本機：32/32 CTest tests。
- Container test target：25/25 CTest tests；有認證 provisioning smoke、digest、
  isolation、health 同 one-shot rendering checks 全部通過。
- Bootstrap build、Compose contract/config、strict MkDocs、網站 link/assets、
  canonical Wiki 同步、截圖 manifest 同 diff hygiene gates 全部通過。
- 13 張 app 截圖加兩張網站截圖已經全部重新產生同逐張肉眼核對，冇保留半套
  gallery。
- 使用測試只喺 populated demo 同隔離 install root 入面做；冇 service 真實
  Windows image、冇裝 host package、冇開／刪真實 VM，亦冇執行 reviewed
  servicing plan，因為呢啲動作會改外部系統，必須由操作員揀實際輸入先可以做。
