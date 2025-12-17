# NVMe 驅動程式 {#nvme}

## 本文檔目錄 {#nvme_toc}

- @ref nvme_intro
- @ref nvme_examples
- @ref nvme_interface
- @ref nvme_design
- @ref nvme_fabrics_host
- @ref nvme_multi_process
- @ref nvme_hotplug
- @ref nvme_cuse
- @ref nvme_led

## 簡介 {#nvme_intro}

NVMe 驅動程式是一個 C 函式庫，可以直接連結到應用程式中，
提供與 [NVMe SSD](http://nvmexpress.org/) 的直接、零複製數據傳輸。
它完全是被動的，這意味著它不會產生
任何線程，只會響應應用程式本身的函數調用來執行操作。
該函式庫通過直接將 [PCI BAR](https://en.wikipedia.org/wiki/PCI_configuration_space) 映射到本地
進程並執行 [MMIO](https://en.wikipedia.org/wiki/Memory-mapped_I/O) 來控制 NVMe 設備。
I/O 通過隊列對異步提交，一般流程與 Linux 的
[libaio](http://man7.org/linux/man-pages/man2/io_submit.2.html) 並不完全不同。

最近，該函式庫已改進為也可以通過 NVMe over Fabrics 連接到遠程 NVMe
設備。用戶現在可以在本地 PCI 總線和遠程 NVMe over Fabrics 發現服務上調用 spdk_nvme_probe()。
API 在其他方面保持不變。

## 範例 {#nvme_examples}

### 從 Hello World 開始 {#nvme_helloworld}

提供了許多範例來演示如何使用 NVMe
函式庫。它們都在存儲庫中的 [examples/nvme](https://github.com/spdk/spdk/tree/master/examples/nvme)
目錄中。最好的起點是
[hello_world](https://github.com/spdk/spdk/blob/master/examples/nvme/hello_world/hello_world.c)。

### 使用 Fio 插件運行基準測試 {#nvme_fioplugin}

SPDK 為非常流行的 [fio](https://github.com/axboe/fio)
工具提供了一個插件，用於運行一些基本基準測試。請參閱 fio 啟動
[指南](https://github.com/spdk/spdk/blob/master/app/fio/nvme/)
以獲取更多詳細資訊。

### 使用 Perf 工具運行基準測試 {#nvme_perf}

[app/spdk_nvme_perf](https://github.com/spdk/spdk/tree/master/app/spdk_nvme_perf)
中的 NVMe perf 工具是也可以用作性能測試的範例之一。fio
工具被廣泛使用，因為它非常靈活。但是，這種靈活性增加了
開銷並降低了 SPDK 的效率。因此，SPDK 提供了一個 perf
基準測試工具，在基準測試期間具有最小的開銷。我們已經
測量到使用 perf 與使用 fio 相比，在
4K 100% 隨機讀取工作負載下，每個核心的 IOPS 最多可提高 2.6 倍。perf 基準測試工具提供了幾個
運行時選項來支援最常見的工作負載。以下範例
演示如何使用 perf。

範例：使用 perf 對本地 NVMe SSD 進行 4K 100% 隨機讀取工作負載，持續 300 秒
~~~{.sh}
perf -q 128 -o 4096 -w randread -r 'trtype:PCIe traddr:0000:04:00.0' -t 300
~~~

範例：使用 perf 對通過 NVMe-oF 通過網絡導出的遠程 NVMe SSD 進行 4K 100% 隨機讀取工作負載
~~~{.sh}
perf -q 128 -o 4096 -w randread -r 'trtype:RDMA adrfam:IPv4 traddr:192.168.100.8 trsvcid:4420' -t 300
~~~

範例：使用 perf 對所有本地 NVMe SSD 進行 4K 70/30 隨機讀取/寫入混合工作負載，持續 300 秒
~~~{.sh}
perf -q 128 -o 4096 -w randrw -M 70 -t 300
~~~

範例：使用 perf 對本地 NVMe SSD 進行擴展 LBA 格式 CRC 保護測試，
用戶必須在從 SSD 讀取 LBA 之前寫入 SSD
~~~{.sh}
perf -q 1 -o 4096 -w write -r 'trtype:PCIe traddr:0000:04:00.0' -t 300 -e 'PRACT=0,PRCKH=GUARD'
perf -q 1 -o 4096 -w read -r 'trtype:PCIe traddr:0000:04:00.0' -t 200 -e 'PRACT=0,PRCKH=GUARD'
~~~

## 公共介面 {#nvme_interface}

- spdk/nvme.h

關鍵函數                               | 描述
------------------------------------------- | -----------
spdk_nvme_probe()                           | @copybrief spdk_nvme_probe()
spdk_nvme_ctrlr_alloc_io_qpair()            | @copybrief spdk_nvme_ctrlr_alloc_io_qpair()
spdk_nvme_ctrlr_get_ns()                    | @copybrief spdk_nvme_ctrlr_get_ns()
spdk_nvme_ns_cmd_read()                     | @copybrief spdk_nvme_ns_cmd_read()
spdk_nvme_ns_cmd_readv()                    | @copybrief spdk_nvme_ns_cmd_readv()
spdk_nvme_ns_cmd_read_with_md()             | @copybrief spdk_nvme_ns_cmd_read_with_md()
spdk_nvme_ns_cmd_write()                    | @copybrief spdk_nvme_ns_cmd_write()
spdk_nvme_ns_cmd_writev()                   | @copybrief spdk_nvme_ns_cmd_writev()
spdk_nvme_ns_cmd_write_with_md()            | @copybrief spdk_nvme_ns_cmd_write_with_md()
spdk_nvme_ns_cmd_write_zeroes()             | @copybrief spdk_nvme_ns_cmd_write_zeroes()
spdk_nvme_ns_cmd_dataset_management()       | @copybrief spdk_nvme_ns_cmd_dataset_management()
spdk_nvme_ns_cmd_flush()                    | @copybrief spdk_nvme_ns_cmd_flush()
spdk_nvme_qpair_process_completions()       | @copybrief spdk_nvme_qpair_process_completions()
spdk_nvme_ctrlr_cmd_admin_raw()             | @copybrief spdk_nvme_ctrlr_cmd_admin_raw()
spdk_nvme_ctrlr_process_admin_completions() | @copybrief spdk_nvme_ctrlr_process_admin_completions()
spdk_nvme_ctrlr_cmd_io_raw()                | @copybrief spdk_nvme_ctrlr_cmd_io_raw()
spdk_nvme_ctrlr_cmd_io_raw_with_md()        | @copybrief spdk_nvme_ctrlr_cmd_io_raw_with_md()

## NVMe 驅動程式設計 {#nvme_design}

### NVMe I/O 提交 {#nvme_io_submission}

使用 nvme_ns_cmd_xxx 函數將 I/O 提交到 NVMe 命名空間。NVMe
驅動程式將 I/O 請求作為 NVMe 提交隊列條目提交到命令中指定的隊列
對。函數在命令完成之前立即返回。
應用程式必須通過調用
spdk_nvme_qpair_process_completions() 在每個有未完成 I/O 的隊列對上輪詢 I/O 完成以接收完成回調。

@sa spdk_nvme_ns_cmd_read, spdk_nvme_ns_cmd_write, spdk_nvme_ns_cmd_dataset_management,
spdk_nvme_ns_cmd_flush, spdk_nvme_qpair_process_completions

#### 融合操作 {#nvme_fuses}

要"融合"兩個命令，第一個命令應該設置 SPDK_NVME_IO_FLAGS_FUSE_FIRST
io 標誌，下一個應該設置 SPDK_NVME_IO_FLAGS_FUSE_SECOND。

此外，必須滿足以下規則才能將兩個命令作為原子單元執行：

- 命令應在同一提交隊列中彼此相鄰插入。
- LBA 範圍對於兩個命令應該是相同的。

例如，要發送融合比較和寫入操作，用戶必須調用 spdk_nvme_ns_cmd_compare
然後調用 spdk_nvme_ns_cmd_write，並確保在同一隊列上沒有其他操作在
之間提交，如下面的範例所示：

~~~c
	rc = spdk_nvme_ns_cmd_compare(ns, qpair, cmp_buf, 0, 1, nvme_fused_first_cpl_cb,
			NULL, SPDK_NVME_CMD_FUSE_FIRST);
	if (rc != 0) {
		...
	}

	rc = spdk_nvme_ns_cmd_write(ns, qpair, write_buf, 0, 1, nvme_fused_second_cpl_cb,
			NULL, SPDK_NVME_CMD_FUSE_SECOND);
	if (rc != 0) {
		...
	}
~~~

NVMe 規範目前將比較和寫入定義為融合操作。
對比較和寫入的支援由控制器標誌
SPDK_NVME_CTRLR_COMPARE_AND_WRITE_SUPPORTED 報告。

#### 擴展性能 {#nvme_scaling}

NVMe 隊列對（struct spdk_nvme_qpair）為
I/O 提供並行提交路徑。I/O 可以從不同的
線程同時在多個隊列對上提交。但是，隊列對不包含鎖或原子操作，因此給定的隊列
對一次只能由單個線程使用。此要求不是
由 NVMe 驅動程式強制執行的（這樣做需要鎖），違反此
要求會導致未定義的行為。

允許的隊列對數量由 NVMe SSD 本身決定。
規範允許數千個，但大多數設備支援 32
到 128 個。規範不保證每個隊列對的可用性能，
但在實踐中，使用單個隊列對幾乎總是
可以實現設備的全部性能。例如，如果設備聲稱能夠
在隊列深度 128 時達到每秒 450,000 次 I/O，實際上
驅動程式是使用 4 個隊列深度為 32 的隊列對，還是使用
隊列深度為 128 的單個隊列對並不重要。

鑑於上述情況，使用 SPDK 的應用程式最簡單的線程模型是
在池中生成固定數量的線程，並為每個線程專用單個 NVMe 隊列
對。進一步的改進是將每個線程固定到
單獨的 CPU 核心，SPDK 文檔經常會互換使用"CPU 核心"和
"線程"，因為我們考慮了這種線程模型。

NVMe 驅動程式在 I/O 路徑中不採用鎖，因此它在
每個線程的性能方面線性擴展，只要為每個新線程專用隊列對和 CPU 核心。
為了充分利用這種擴展，
應用程式應該考慮組織其內部數據結構，使
數據專門分配給單個線程。所有需要
該數據的操作都應該通過向擁有線程發送請求來完成。
這導致消息傳遞架構，而不是鎖定
架構，並且將在 CPU 核心之間實現優越的擴展。

### NVMe 驅動程式內部記憶體使用 {#nvme_memory_usage}

SPDK NVMe 驅動程式提供零複製數據傳輸路徑，這意味著
I/O 命令沒有數據緩衝區。但是，某些管理命令具有
數據複製，具體取決於用戶使用的 API。

每個隊列對都有許多追蹤器，用於追蹤調用者提交的命令。
I/O 隊列的追蹤器數量取決於用戶輸入的隊列
大小和從控制器功能寄存器字段讀取的值最大隊列
條目支援（MQES，基於 0 的值）。每個追蹤器具有固定大小 4096 字節，
因此每個 I/O 隊列使用的最大記憶體為：(MQES + 1) * 4 KiB。

I/O 隊列對可以在主機記憶體中分配，這用於大多數 NVMe 控制器，
一些可以支援控制器記憶體緩衝區的 NVMe 控制器可以將 I/O 隊列
對放在控制器的 PCI BAR 空間中，SPDK NVMe 驅動程式可以將 I/O 提交隊列
放入控制器記憶體緩衝區，這取決於用戶輸入和控制器功能。
每個提交隊列條目（SQE）和完成隊列條目（CQE）分別消耗 64 字節
和 16 字節。因此，每個 I/O 隊列
對使用的最大記憶體為 (MQES + 1) * (64 + 16) 字節。

## NVMe over Fabrics 主機支援 {#nvme_fabrics_host}

NVMe 驅動程式支援連接到遠程 NVMe-oF 目標並
以與本地 NVMe SSD 相同的方式與它們交互。

### 指定遠程 NVMe over Fabrics 目標 {#nvme_fabrics_trid}

連接到遠程 NVMe-oF 目標的方法非常類似
於本地 PCIe 連接的 NVMe 設備的正常枚舉過程。
要連接到遠程 NVMe over Fabrics 子系統，用戶可以調用
spdk_nvme_probe()，其中 `trid` 參數指定
NVMe-oF 目標的地址。

調用者可以手動填寫 spdk_nvme_transport_id 結構
或使用 spdk_nvme_transport_id_parse() 函數將
人類可讀的字符串表示轉換為所需的結構。

spdk_nvme_transport_id 可能包含發現服務的地址
或單個 NVM 子系統。如果指定了發現服務地址，
NVMe 函式庫將為每個
發現的 NVM 子系統調用 spdk_nvme_probe() `probe_cb`，這允許用戶選擇所需的
要附加的子系統。或者，如果地址直接指定
單個 NVM 子系統，NVMe 函式庫將僅為該子系統調用 `probe_cb`；這允許用戶跳過發現步驟
並直接連接到具有已知地址的子系統。

### RDMA 限制

請參閱 NVMe-oF target 的 @ref nvmf_rdma_limitations

## NVMe 多進程 {#nvme_multi_process}

此功能使 SPDK NVMe 驅動程式能夠支援多個進程訪問
同一個 NVMe 設備。NVMe 驅動程式從共享記憶體分配關鍵結構，以便
每個進程可以映射該記憶體並創建自己的隊列對或共享管理
隊列。每個 NVMe 控制器的 I/O 隊列對數量有限。

此功能的主要動機是支援可以附加
到長時間運行的應用程式的管理工具，執行一些維護工作或收集資訊，然後
分離。

### 配置 {#nvme_multi_process_configuration}

DPDK EAL 允許生成不同類型的進程，每個進程對
應用程式使用的 hugepage 記憶體具有不同的權限。

有兩種類型的進程：

1. 初始化共享記憶體並具有完全權限的主進程，以及
2. 可以通過映射其共享記憶體
   區域附加到主進程並執行 NVMe 操作（包括創建隊列對）的輔助進程。

此功能預設啟用，並通過為共享
記憶體組 ID 選擇值來控制。此 ID 是正整數，具有相同共享
記憶體組 ID 的兩個應用程式將共享記憶體。具有給定共享記憶體組
ID 的第一個應用程式將被視為主進程，所有其他應用程式為輔助進程。

範例：相同的 shm_id 和非重疊的核心掩碼
~~~{.sh}
spdk_nvme_perf options [AIO device(s)]...
	[-c core mask for I/O submission/completion]
	[-i shared memory group ID]

spdk_nvme_perf -q 1 -o 4096 -w randread -c 0x1 -t 60 -i 1
spdk_nvme_perf -q 8 -o 131072 -w write -c 0x10 -t 60 -i 1
~~~

### 限制 {#nvme_multi_process_limitations}

1. 共享記憶體的兩個進程可能不會在其核心掩碼中共享任何核心。
2. 如果主進程在輔助進程仍在運行時退出，這些進程
   將繼續運行。但是，無法創建新的主進程。
3. 應用程式負責協調對邏輯塊的訪問。
4. 如果進程意外退出，分配的記憶體將在最後一個
   進程退出時釋放。

@sa spdk_nvme_probe, spdk_nvme_ctrlr_process_admin_completions

## NVMe 熱插拔 {#nvme_hotplug}

在 NVMe 驅動程式級別，我們為熱插拔提供以下支援：

1. 熱插拔事件檢測：
   NVMe 函式庫的用戶可以定期調用 spdk_nvme_probe() 來檢測
   熱插拔事件。對於每個
   檢測到的新設備，將調用 probe_cb，然後調用 attach_cb。用戶還可以選擇提供一個 remove_cb，如果
   先前附加的 NVMe 設備在系統上不再存在，將調用該 remove_cb。
   對已移除設備的所有後續 I/O 將返回錯誤。

2. 帶有 IO 負載的熱移除 NVMe：
   當在 I/O 發生時熱移除設備時，對 PCI BAR 的所有訪問都會
   導致 SIGBUS 錯誤。NVMe 驅動程式通過安裝
   SIGBUS 處理程序並將 PCI BAR 重新映射到新的佔位符記憶體位置來自動處理此情況。
   這意味著在熱移除期間正在進行的 I/O 將以適當的錯誤
   代碼完成，並且不會使應用程式崩潰。

@sa spdk_nvme_probe

## NVMe 字符設備 {#nvme_cuse}

### 設計

![NVMe character devices processing diagram](nvme_cuse.svg)

對於每個控制器以及命名空間，字符設備在以下位置創建：
~~~{.sh}
    /dev/spdk/nvmeX
    /dev/spdk/nvmeXnY
    ...
~~~
其中 X 是唯一的 SPDK NVMe 控制器索引，Y 是命名空間 id。

當創建控制器和命名空間時，來自 CUSE 的請求由 pthreads 處理。
這些通過環將 I/O 或管理命令傳遞給使用
nvme_io_msg_process() 處理它們的線程。

請求在附加 NVMe 控制器時獲得的資訊的 Ioctls 會收到
立即響應，而不會通過環傳遞它們。

此介面為每個控制器保留一個額外的 qpair 用於向下發送 I/O。

### 用法

#### 為 NVMe 啟用 cuse 支援

Cuse 支援在 Linux 上預設啟用。確保安裝所需的依賴項：
~~~{.sh}
sudo scripts/pkgdep.sh
~~~

#### 創建 NVMe-CUSE 設備

首先確保準備環境（請參閱 @ref getting_started）。
這包括載入 CUSE 內核模組。
附加到運行中的 SPDK 應用程式的任何 NVMe 控制器都可以
通過 NVMe-CUSE 介面公開。關閉 SPDK 應用程式時，
NVMe-CUSE 設備將被註銷。

~~~{.sh}
$ sudo scripts/setup.sh
$ sudo modprobe cuse
$ sudo build/bin/spdk_tgt
# Continue in another session
$ sudo scripts/rpc.py bdev_nvme_attach_controller -b Nvme0 -t PCIe -a 0000:82:00.0
Nvme0n1
$ sudo scripts/rpc.py bdev_nvme_get_controllers
[
  {
    "name": "Nvme0",
    "trid": {
      "trtype": "PCIe",
      "traddr": "0000:82:00.0"
    }
  }
]
$ sudo scripts/rpc.py bdev_nvme_cuse_register -n Nvme0
$ ls /dev/spdk/
nvme0  nvme0n1
~~~

#### 使用 nvme-cli 的範例

大多數 nvme-cli 命令可以通過提供路徑來指向特定的控制器或命名空間。
這可以用於向 SPDK NVMe-CUSE 設備發出命令。

~~~{.sh}
sudo nvme id-ctrl /dev/spdk/nvme0
sudo nvme smart-log /dev/spdk/nvme0
sudo nvme id-ns /dev/spdk/nvme0n1
~~~

注意：`nvme list` 命令不顯示 SPDK NVMe-CUSE 設備，
請參閱 nvme-cli [PR #773](https://github.com/linux-nvme/nvme-cli/pull/773)。

#### 使用 smartctl 的範例

smartctl 工具根據設備路徑識別設備類型。如果沒有匹配預期的
模式，則使用 SCSI 翻譯層來識別設備。

要使用 smartctl，除了 NVMe 設備的完整路徑外，還必須使用 '-d nvme' 參數。

~~~{.sh}
    smartctl -d nvme -i /dev/spdk/nvme0
    smartctl -d nvme -H /dev/spdk/nvme1
    ...
~~~

### 限制

NVMe 命名空間創建為字符設備，它們的使用可能受到
期望塊設備的工具的限制。

SPDK 不更新 Sysfs。

SPDK NVMe CUSE 在 "/dev/spdk/" 目錄中創建節點以明確區分
與其他設備。僅在 "/dev" 目錄中搜索的工具可能無法
與 SPDK NVMe CUSE 一起使用。

未實現 SCSI 到 NVMe 翻譯層。使用此層來
識別、管理或操作設備的工具可能無法正常工作或它們的使用可能受到限制。

### SPDK_CUSE_GET_TRANSPORT ioctl 命令

nvme-cli 主要使用 IOCTL 來獲取資訊，但傳輸資訊是
通過 sysfs 獲取的。由於 SPDK 不填充 sysfs，SPDK 插件利用
SPDK/CUSE 特定的 ioctl 來獲取資訊。

~~~{.c}
#define SPDK_CUSE_GET_TRANSPORT _IOWR('n', 0x1, struct cuse_transport)
~~~

~~~{.c}
struct cuse_transport {
	char trstring[SPDK_NVMF_TRSTRING_MAX_LEN + 1];
	char traddr[SPDK_NVMF_TRADDR_MAX_LEN + 1];
} tr;
~~~

## NVMe LED 管理 {#nvme_led}

可以使用 ledctl(8) 實用程式來控制支援
NPEM（原生 PCIe 外殼管理）的系統中 LED 的狀態，即使 NVMe 設備由 SPDK 控制。
但是，在這種情況下，有必要確定插槽設備編號，因為塊設備
不可用。[ledctl.sh](https://github.com/spdk/spdk/tree/master/scripts/ledctl.sh) 腳本
可以用於幫助解決這個問題。它接受 nvme bdev 的名稱並使用
適當的選項調用 ledctl。
