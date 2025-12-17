# SPDK 中文文檔索引

本目錄包含 SPDK 框架的中文翻譯文檔。

## 已翻譯文檔

### 入門指南
- [入門指南](getting_started.md) - 如何開始使用 SPDK，包括構建和運行範例

### 核心概念
- [核心概念](concepts.md) - 核心概念索引
- [用戶空間驅動程式](userspace.md) - 用戶空間驅動程式的工作原理
- [記憶體管理](memory.md) - DMA 和記憶體管理詳解
- [並發與訊息傳遞](concurrency.md) - SPDK 的並發模型和訊息傳遞機制

### 主要組件
- [NVMe 驅動程式](nvme.md) - NVMe 驅動程式的完整指南
- [塊設備 (Bdev)](bdev.md) - 塊設備抽象層用戶指南
- [事件框架](event.md) - 事件驅動框架詳解

## 文檔結構

所有中文文檔位於 `docs/zh_TW/` 目錄下，對應於 `external/spdk/doc/` 目錄中的英文原文。

## 使用說明

1. 這些文檔是 SPDK 官方文檔的繁體中文翻譯
2. 代碼示例和技術術語保持英文
3. 文檔結構和格式與原文保持一致
4. 建議結合原始英文文檔和代碼一起閱讀

## 翻譯進度

### 核心文檔
- ✅ getting_started.md - 入門指南
- ✅ concepts.md - 核心概念索引
- ✅ about.md - 什麼是 SPDK
- ✅ overview.md - SPDK 結構概述

### 核心概念
- ✅ userspace.md - 用戶空間驅動程式
- ✅ memory.md - 記憶體管理
- ✅ concurrency.md - 並發與訊息傳遞

### 主要組件
- ✅ nvme.md - NVMe 驅動程式
- ✅ bdev.md - 塊設備指南（主要部分）
- ✅ event.md - 事件框架
- ✅ blob.md - Blobstore 程式設計師指南（開頭部分）

### 應用程式
- ✅ applications.md - SPDK 應用程式概述（部分）

## 注意事項

- 翻譯可能與最新版本的 SPDK 有細微差異
- 建議參考 [SPDK 官方文檔](https://spdk.io/doc/) 獲取最新資訊
- 如有疑問，請查閱原始英文文檔

## 貢獻

如需改進翻譯或報告問題，請提交 issue 或 pull request。
