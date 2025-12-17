# SPDK 結構概述 {#overview}

## 概述 {#dir_overview}

SPDK 由一組位於 `lib` 中的 C 函式庫組成，公共介面
頭文件位於 `include/spdk`，加上一組在 `app` 中由這些
函式庫構建的應用程式。用戶可以在其軟體中使用 C 函式庫或部署
完整的 SPDK 應用程式。

SPDK 圍繞訊息傳遞而不是鎖定設計，並且大多數 SPDK
函式庫對它們嵌入的應用程式的底層線程模型做出了幾個假設。
但是，SPDK 盡最大努力保持
對實際使用的特定訊息傳遞、事件、協程或輕量級
線程框架的不可知。為了實現這一點，所有 SPDK 函式庫
都與 `lib/thread` 中的抽象函式庫交互（公共介面位於
`include/spdk/thread.h`）。任何框架都可以初始化線程抽象
並提供回調來實現 SPDK 函式庫
需要的功能。有關此抽象的更多資訊，請參閱 @ref concurrency。

SPDK 構建在 POSIX 之上以進行大多數操作。為了使移植到非 POSIX
環境更容易，所有 POSIX 頭文件都隔離到
`include/spdk/stdinc.h` 中。但是，SPDK 需要許多 POSIX 不提供的操作，
例如枚舉系統上的 PCI 設備或
分配對 DMA 安全的記憶體。這些額外的操作都在
一個名為 `env` 的函式庫中抽象，其公共頭文件位於
`include/spdk/env.h`。預設情況下，SPDK 使用基於 DPDK 的函式庫實現 `env` 介面。
但是，可以替換該實現。有關其他資訊，請參閱 @ref
porting。

## 應用程式 {#dir_app}

`app` 頂級目錄包含完整的應用程式，由 SPDK
組件構建。有關完整概述，請參閱 @ref app_overview。

SPDK 應用程式通常可以通過少量配置
選項啟動。然後使用
JSON-RPC 執行應用程式的完整配置。有關其他資訊，請參閱 @ref jsonrpc。

## 函式庫 {#dir_lib}

`lib` 目錄包含 SPDK 的真正核心。每個組件都是一個 C 函式庫，在 `lib` 下有自己的目錄。
一些關鍵函式庫是：

- @ref bdev
- @ref nvme

## 文檔 {#dir_doc}

`doc` 頂級目錄包含所有 SPDK 的文檔。API 文檔
是使用 Doxygen 直接從代碼創建的，但更一般的文章和更長的
解釋位於此目錄中，以及 Doxygen 配置文件。

要構建文檔，只需在 doc 目錄中鍵入 `make`。

## 範例 {#dir_examples}

`examples` 頂級目錄包含一組旨在用於
參考的範例。這些與應用程式不同，應用程式執行"真實"
任務，可以合理地部署。範例要麼是高度
設計的以演示 SPDK 的某些方面，要麼被認為不夠完整
以保證將它們標記為完整的 SPDK 應用程式。

這是了解 SPDK 工作原理的好地方。特別是，請查看
`examples/nvme/hello_world`。

## Include {#dir_include}

`include` 目錄是所有頭文件所在的位置。公共 API
都放置在 `include` 的 `spdk` 子目錄中，我們強烈
建議應用程式將其包含路徑設置為頂級 `include`
目錄，並通過前綴 `spdk/` 來包含頭文件，如下所示：

~~~{.c}
#include "spdk/nvme.h"
~~~

這裡的大多數頭文件與 `lib` 目錄中的函式庫相對應。
但是，有一些獨立的頭文件。它們是：

- `assert.h`
- `barrier.h`
- `endian.h`
- `fd.h`
- `mmio.h`
- `queue.h` 和 `queue_extras.h`
- `string.h`

還有一個 `spdk_internal` 目錄，其中包含 SPDK 內函式庫廣泛包含的頭文件，
但這些頭文件不是公共 API 的一部分，不會
安裝在用戶的系統上。

## 腳本 {#dir_scripts}

`scripts` 目錄包含用於許多操作的便利腳本。最重要的兩個
是 `check_format.sh`，它將使用 astyle 和 pep8 檢查 C、C++ 和 Python
編碼風格是否符合我們定義的約定，以及 `setup.sh`，它綁定和解綁設備
從內核驅動程式。

## 測試 {#dir_tests}
