# SPDK 應用程式概述 {#app_overview}

SPDK 主要是一個開發套件，提供函式庫和頭文件供
其他應用程式使用。但是，SPDK 還包含許多應用程式。
這些應用程式主要用於測試函式庫，但許多都是完整
功能且高質量的。SPDK 中的主要應用程式是：

- @ref iscsi
- @ref nvmf
- @ref vhost
- SPDK Target（結合上述三個的統一應用程式）

`examples` 目錄中還有許多工具和範例。

SPDK 目標都基於通用框架，因此它們有很多
共同點。該框架定義了一個稱為 `subsystem` 的概念，所有
功能都在各種子系統中實現。子系統具有統一的
初始化和拆卸路徑。

# 配置 SPDK 應用程式 {#app_config}

## 命令行參數 {#app_cmd_line_args}

SPDK 應用程式框架為所有
使用它的應用程式定義了一組基本命令行標誌。特定應用程式可能會實現額外的標誌。

參數    | 長參數             | 類型     | 預設值                | 描述
-------- | ---------------------- | -------- | ---------------------- | -----------
-c       | --config               | string   |                        | @ref cmd_arg_config_file
-d       | --limit-coredump       | flag     | false                  | @ref cmd_arg_limit_coredump
-e       | --tpoint-group         | integer  |                        | @ref cmd_arg_limit_tpoint_group_mask
-g       | --single-file-segments | flag     |                        | @ref cmd_arg_single_file_segments
-h       | --help                 | flag     |                        | 顯示所有可用參數並退出
-i       | --shm-id               | integer  |                        | @ref cmd_arg_multi_process
-m       | --cpumask              | CPU mask | 0x1                    | 應用程式 @ref cpu_mask
-n       | --mem-channels         | integer  | all channels           | 用於 DPDK 的記憶體通道數
-p       | --main-core            | integer  | first core in CPU mask | DPDK 的主（主要）核心
-r       | --rpc-socket           | string   | /var/tmp/spdk.sock     | RPC 監聽地址
-s       | --mem-size             | integer  | all hugepage memory    | @ref cmd_arg_memory_size
||        | --silence-noticelog    | flag     |                        | 禁用 notice 級別日誌記錄到 `stderr`
-u       | --no-pci               | flag     |                        | @ref cmd_arg_disable_pci_access.
||        | --wait-for-rpc         | flag     |                        | @ref cmd_arg_deferred_initialization
-B       | --pci-blocked          | B:D:F    |                        | @ref cmd_arg_pci_blocked_allowed.
-A       | --pci-allowed          | B:D:F    |                        | @ref cmd_arg_pci_blocked_allowed.
-R       | --huge-unlink          | flag     |                        | @ref cmd_arg_huge_unlink
||        | --huge-dir             | string   | the first discovered   | 從特定掛載點分配 hugepages
-L       | --logflag              | string   |                        | @ref cmd_arg_log_flags

### 配置文件 {#cmd_arg_config_file}

SPDK 應用程式使用 JSON RPC 配置文件進行配置。
有關詳細資訊，請參閱 @ref jsonrpc。

### 限制核心轉儲 {#cmd_arg_limit_coredump}

預設情況下，SPDK 應用程式會將核心文件大小的資源限制
設置為 RLIM_INFINITY。指定 `--limit-coredump` 將不會設置資源限制。

### 追蹤點組掩碼 {#cmd_arg_limit_tpoint_group_mask}

SPDK 有一個實驗性的低開銷追蹤框架。此
框架中的追蹤點被組織成追蹤點組。預設情況下，所有追蹤點
組都被禁用。`--tpoint-group` 可用於在應用程式中啟用特定的
追蹤點組子集。

注意：有關追蹤點框架的其他文檔正在進行中。

### 延遲初始化 {#cmd_arg_deferred_initialization}

SPDK 應用程式通過一組狀態進行，從 `STARTUP` 開始，以
`RUNTIME` 結束。

如果提供了 `--wait-for-rpc` 參數，SPDK 將在開始
框架初始化之前暫停。此狀態稱為 `STARTUP`。JSON RPC 服務器是
就緒的，但只有一小部分命令可用於設置初始化
參數。在 SPDK 應用程式進入
`RUNTIME` 狀態後，這些參數無法更改。當客戶端完成配置 SPDK 子系統時，它
需要發出 @ref rpc_framework_start_init RPC 命令以開始
初始化過程。在 `rpc_framework_start_init` 返回 `true` 後，SPDK
將進入 `RUNTIME` 狀態，可用命令列表變得
更大。

要查看當前狀態下可用的 RPC 方法，請發出
`rpc_get_methods`，參數 `current` 設置為 `true`。

有關更多詳細資訊，請參閱 @ref jsonrpc 文檔。

### 僅創建一個 hugetlbfs 文件 {#cmd_arg_single_file_segments}

與為每個頁面創建一個 hugetlbfs 文件不同，此選項使 SPDK 創建
每個套接字每個 hugepage 一個文件。這對於 @ref virtio 與
超過 8 個 hugepage 一起使用是必需的。請參閱 @ref virtio_2mb。

### 多進程模式 {#cmd_arg_multi_process}

當指定 `--shm-id` 時，應用程式在多進程模式下啟動。
使用相同 shm-id 的應用程式共享它們的記憶體和
[NVMe 設備](@ref nvme_multi_process)。第一個以給定 id 啟動的應用程式
成為主進程，其餘的稱為輔助進程，僅
