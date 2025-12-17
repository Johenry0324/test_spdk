# 塊設備用戶指南 {#bdev}

## 目標讀者 {#bdev_ug_targetaudience}

本用戶指南面向了解塊存儲、存儲驅動程式、發出 JSON-RPC
命令以及存儲服務（如 RAID、壓縮、加密等）的軟體開發人員。

## 簡介 {#bdev_ug_introduction}

SPDK 塊設備層，通常簡稱為 *bdev*，是一個 C 函式庫，
旨在等同於操作系統塊存儲層，該層
通常位於傳統內核
存儲堆棧中設備驅動程式之上。具體來說，此函式庫提供以下
功能：

* 用於實現與不同類型塊存儲設備介面的塊設備的可插拔模組 API。
* 適用於 NVMe、malloc（ramdisk）、Linux AIO、virtio-scsi、Ceph RBD、Pmem 和 Vhost-SCSI 啟動器等的驅動程式模組。
* 用於枚舉和聲明 SPDK 塊設備然後對這些設備執行操作（讀取、寫入、取消映射等）的應用程式 API。
* 用於堆疊塊設備以創建複雜 I/O 管道的設施，包括邏輯卷管理 (lvol) 和分區支持 (GPT)。
* 通過 JSON-RPC 配置塊設備。
* 請求隊列、超時和重置處理。
* 用於向塊設備發送 I/O 的多個無鎖隊列。

Bdev 模組創建抽象層，為所有設備提供通用 API。
用戶可以使用可用的 bdev 模組或創建自己的模組，其下具有任何類型的
設備（有關詳細資訊，請參閱 @ref bdev_module）。SPDK
還提供 vbdev 模組，這些模組在現有 bdev 上創建塊設備。例如
@ref bdev_ug_logical_volumes 或 @ref bdev_ug_gpt

## 先決條件 {#bdev_ug_prerequisites}

本指南假設您已經可以在您的平台上構建標準 SPDK 分發版。
塊設備層是一個 C 函式庫，具有一個名為 bdev.h 的公共
頭文件。以下章節中描述的所有 SPDK 配置都是通過使用 JSON-RPC 命令完成的。
SPDK 提供了一個基於 python 的
命令行工具，用於發送位於 `scripts/rpc.py` 的 RPC 命令。用戶
可以通過使用 `-h` 或 `--help` 標誌運行此腳本來列出可用命令。
此外，用戶可以通過運行 `scripts/rpc.py rpc_get_methods` 直接從 SPDK 應用程式檢索當前支持的 RPC 命令集。
可以通過添加 `-h` 標誌作為
命令參數來顯示每個命令的詳細幫助。

## 配置塊設備模組 {#bdev_ug_general_rpcs}

可以使用 JSON RPC 配置塊設備。可用 RPC 命令的完整列表
以及詳細資訊可以在 @ref jsonrpc_components_bdev 頁面上找到。

## 常見塊設備配置範例

## Ceph RBD {#bdev_config_rbd}

SPDK RBD bdev 驅動程式提供對 Ceph RADOS 塊
設備 (RBD) 的 SPDK 塊層訪問。Ceph RBD 設備通過 librbd 和 librados 函式庫訪問
由 Ceph 導出的 RADOS 塊設備。要創建 Ceph bdev RPC
命令，應使用 `bdev_rbd_register_cluster` 和 `bdev_rbd_create`。

SPDK 提供了兩種創建 RBD bdev 的方法。一種是為每個 RBD bdev 創建一個新的 Rados 集群對象。
另一種是為多個 RBD bdev 共享同一個 Rados 集群對象。
每個 Rados 集群對象創建少量 io_context_pool 和 messenger 線程。
Ceph 命令 `ceph config help librados_thread_count` 和 `ceph config help ms_async_op_threads`
可以幫助檢查這些線程資訊。此外，您可以通過
更新 ceph.conf 文件或使用 Ceph 配置命令來指定線程數。有關更多資訊，請參閱
[Ceph 配置](https://docs.ceph.com/en/latest/rados/configuration/ceph-conf/)
對於大量 RBD bdev，一組線程可能不足以最大化性能，
但每個 RBD bdev 一組線程可能會增加太多上下文切換。因此，可能需要
對每個集群對象的 RBD bdev 數量和線程進行性能調整。

範例命令

`rpc.py bdev_rbd_register_cluster rbd_cluster`

此命令將註冊一個名為 rbd_cluster 的集群。可選的 `--config-file` 和
`--key-file` 參數為集群指定。

要刪除已註冊的集群，請使用 bdev_rbd_unregister_cluster 命令。

`rpc.py bdev_rbd_unregister_cluster rbd_cluster`

要使用已註冊的集群創建 RBD bdev。

`rpc.py bdev_rbd_create rbd foo 512 -c rbd_cluster`

此命令將創建一個 bdev，表示來自名為 'rbd' 的池的 'foo' 映像。
當為 `bdev_rbd_create` 指定 -c 時，RBD bdev 將在 librbd 模組中與 Ceph 的一個連接共享同一個 rados 集群。
相反，如果不指定 -c，它將為每個 bdev 創建一個新的 rados 集群，每個集群有一個
集群連接。

要刪除塊設備表示，請使用 bdev_rbd_delete 命令。

`rpc.py bdev_rbd_delete Rbd0`

要調整 bdev 大小，請使用 bdev_rbd_resize 命令。

`rpc.py bdev_rbd_resize Rbd0 4096`

此命令將 Rbd0 bdev 調整為 4096 MiB。

## 加密虛擬 Bdev 模組 {#bdev_config_crypto}

可以配置加密虛擬 bdev 模組以提供靜態數據加密
適用於任何底層 bdev。該模組依賴 SPDK Accel 框架來提供
所有加密功能。
其中一個 accel 模組，dpdk_cryptodev 使用 DPDK CryptoDev API 實現，
它支持許多不同的僅軟體加密模組以及硬體
輔助支持 Intel QAT 板和 NVIDIA 加密啟用的 NIC。

對於讀取，提供給加密塊設備的緩衝區將用作未加密數據的目標緩衝區。
但是，對於寫入，使用臨時暫存緩衝區作為
加密的目標緩衝區，然後將其傳遞給底層 bdev 作為
寫入緩衝區。這樣做是為了避免在原始源緩衝區中加密數據，這
可能會在某些用例中造成問題。

以下是支持加密操作的 accel 模組的資訊：

### dpdk_cryptodev accel 模組

支持以下密碼：

- AESN-NI 多緩衝區加密輪詢模式驅動程式：RTE_CRYPTO_CIPHER_AES128_CBC
- Intel(R) QuickAssist (QAT) 加密輪詢模式驅動程式：RTE_CRYPTO_CIPHER_AES128_CBC、
  RTE_CRYPTO_CIPHER_AES128_XTS
  （注意：QAT 功能正常，但在硬體完全集成到 SPDK CI 系統之前被標記為實驗性。）
- MLX5 加密輪詢模式驅動程式：RTE_CRYPTO_CIPHER_AES256_XTS、RTE_CRYPTO_CIPHER_AES512_XTS

為了支持使用 bdev 塊偏移 (LBA) 作為初始化向量 (IV)，
加密模組將所有 I/O 分解為大小等於塊
大小的加密操作。例如，對塊大小為 512B 的 bdev 進行 4K I/O，
將導致 8 個加密操作。

### SW accel 模組

支持以下密碼：

- 使用 128 或 256 位密鑰實現的 AES_XTS 密碼，使用 ISA-L_crypto 實現

### 一般工作流程

- 設置所需的 accel 模組以執行加密操作，可以通過 `accel_assign_opc` RPC 命令完成
- 使用 `accel_crypto_key_create` RPC 命令創建命名加密密鑰。密鑰將使用分配的 accel
  模組。每個 accel 模組中的參數集和支持的密碼可能不同。
- 使用 `bdev_crypto_create` RPC 命令創建虛擬加密塊設備，提供基礎塊設備名稱和加密密鑰名稱

#### 範例

使用 dpdk_cryptodev accel 模組的範例命令
```
# 使用 `--wait-for-rpc` 參數啟動 SPDK 應用程式
rpc.py dpdk_cryptodev_scan_accel_module
rpc.py dpdk_cryptodev_set_driver crypto_aesni_mb
rpc.py accel_assign_opc -o encrypt -m dpdk_cryptodev
rpc.py accel_assign_opc -o decrypt -m dpdk_cryptodev
rpc.py framework_start_init
rpc.py accel_crypto_key_create -c AES_CBC -k 01234567891234560123456789123456 -n key_aesni_cbc_1
rpc.py bdev_crypto_create NVMe1n1 CryNvmeA -n key_aesni_cbc_1
```

這些命令將在 NVMe bdev
'NVMe1n1' 之上創建一個名為 'CryNvmeA' 的加密 vbdev，並將使用名為 `key_aesni_cbc_1` 的密鑰。
密鑰將與已分配用於加密操作的 accel 模組一起工作，在此範例中，它將是 dpdk_cryptodev。

### 加密密鑰格式

請確保密鑰以十六進制格式提供。這意味著傳遞給
rpc.py 的字串必須是二進制形式密鑰長度的兩倍。

#### 範例命令

`rpc.py accel_crypto_key_create -c AES_XTS -e 7859243a027411e581e0c40a35c8228f -k 10fee72b3d47553e065affdb48c54a81 -n sample_key`

此命令將創建一個名為 `sample_key` 的密鑰，AES 密鑰
'10fee72b3d47553e065affdb48c54a81' 和 XTS 密鑰
'7859243a027411e581e0c40a35c8228f'。換句話說，要使用的複合 AES_XTS 密鑰是
'10fee72b3d47553e065affdb48c54a817859243a027411e581e0c40a35c8228f'

### 刪除虛擬加密塊設備

要刪除 vbdev，請使用 bdev_crypto_delete 命令。

`rpc.py bdev_crypto_delete CryNvmeA`

### dpdk_cryptodev mlx5_pci 驅動程式配置

mlx5_pci 驅動程式與啟用加密的 Nvidia NIC 一起工作，需要特殊配置
DPDK 環境以啟用加密功能。可以通過配置 SPDK 事件函式庫來完成
`spdk_app_opts` 結構的 `env_context` 成員或通過傳遞相應的 CLI 參數
以下形式：`--allow=BDF,class=crypto,wcs_file=/full/path/to/wrapped/credentials`，例如
`--allow=0000:01:00.0,class=crypto,wcs_file=/path/credentials.txt`。

## 延遲 Bdev 模組 {#bdev_config_delay}

延遲 vbdev 模組旨在在較低
級別 bdev 之上應用預定的額外延遲。這使得在功能
或可擴展性測試期間能夠模擬設備的延遲特性。例如，為了模擬驅動延遲的影響，當
處理 I/O 時，可以在其上配置一個 NULL bdev 和一個延遲 bdev。

延遲 bdev 模組不旨在提供特定 NVMe 驅動延遲的高保真複製，
相反，它的主要目的是提供"大局"理解，了解通用延遲如何影響給定的
應用程式。

延遲 bdev 是使用 `bdev_delay_create` RPC 創建的。此 rpc 接受 6 個參數，一個用於名稱
延遲 bdev 和一個用於基礎 bdev 的名稱。其餘四個參數表示以下
延遲值：平均讀取延遲、平均寫入延遲、p99 讀取延遲和 p99 寫入延遲。
在延遲 bdev 的上下文中，p99 延遲意味著百分之一的 I/O 將被延遲至少
在完成到上層協議之前，p99 延遲的值。所有延遲值
都以微秒為單位測量。

範例命令：

`rpc.py bdev_delay_create -b Null0 -d delay0 -r 10 --nine-nine-read-latency 50 -w 30 --nine-nine-write-latency 90`

此命令將創建一個延遲 bdev，平均讀取和寫入延遲分別為 10 和 30 微秒，p99 讀取
和寫入延遲分別為 50 和 90 微秒。

可以使用 `bdev_delay_delete` RPC 刪除延遲 bdev

範例命令：

`rpc.py bdev_delay_delete delay0`

## GPT (GUID 分區表) {#bdev_config_gpt}

GPT 虛擬 bdev 驅動程式預設啟用，不需要任何配置。
它會自動檢測任何附加 bdev 上的 @ref bdev_ug_gpt 並將創建
可能的多個虛擬 bdev。

### SPDK GPT 分區表 {#bdev_ug_gpt}

SPDK 分區類型 GUID 是 `6527994e-2c5a-4eec-9613-8f5944074e8b`。現有的 SPDK bdev
可以通過 NBD 作為 Linux 塊設備公開，然後可以使用
標準分區工具進行分區。分區後，需要刪除 bdev 並
再次附加，以便 GPT bdev 模組看到任何更改。必須首先加載 NBD 內核模組。
要創建 NBD bdev，用戶應使用 `nbd_start_disk` RPC 命令。

範例命令

`rpc.py nbd_start_disk Malloc0 /dev/nbd0`

這將在 `/dev/nbd0` 塊設備下公開 SPDK bdev `Malloc0`。

要刪除 NBD 設備，用戶應使用 `nbd_stop_disk` RPC 命令。

範例命令

`rpc.py nbd_stop_disk /dev/nbd0`

要顯示完整或指定的 nbd 設備列表，用戶應使用 `nbd_get_disks` RPC 命令。

範例命令

`rpc.py nbd_stop_disk -n /dev/nbd0`

### 使用 NBD 創建 GPT 分區表 {#bdev_ug_gpt_create_part}

~~~bash
# 通過 JSON-RPC 將 bdev Nvme0n1 公開為內核塊設備 /dev/nbd0
rpc.py nbd_start_disk Nvme0n1 /dev/nbd0

# 創建 GPT 分區表。
parted -s /dev/nbd0 mklabel gpt

# 添加一個消耗 50% 可用空間的分區。
parted -s /dev/nbd0 mkpart MyPartition '0%' '50%'

# 將分區類型更改為 SPDK GUID。
# sgdisk 是 gdisk 包的一部分。
sgdisk -t 1:6527994e-2c5a-4eec-9613-8f5944074e8b /dev/nbd0

# 停止 NBD 設備（停止導出 /dev/nbd0）。
rpc.py nbd_stop_disk /dev/nbd0

# 現在 Nvme0n1 配置了 GPT 分區表，並且
# 第一個分區將自動公開為
# SPDK 應用程式中的 Nvme0n1p1。
~~~

## iSCSI bdev {#bdev_config_iscsi}

SPDK iSCSI bdev 驅動程式依賴於 libiscsi，因此預設不啟用。
為了使用它，使用額外的 `--with-iscsi-initiator` 配置選項構建 SPDK。

以下命令在給定的 iSCSI URL 處從單個 LUN 創建 `iSCSI0` bdev
，報告的啟動器 IQN 為 `iqn.2016-06.io.spdk:init`。

`rpc.py bdev_iscsi_create -b iSCSI0 -i iqn.2016-06.io.spdk:init --url iscsi://127.0.0.1/iqn.2016-06.io.spdk:disk1/0`

URL 採用以下格式：
`iscsi://[<username>[%<password>]@]<host>[:<port>]/<target-iqn>/<lun>`

## Linux AIO bdev {#bdev_config_aio}

SPDK AIO bdev 驅動程式通過 Linux AIO 提供對 Linux 內核塊
設備或 Linux 文件系統上的文件的 SPDK 塊層訪問。請注意，使用 O_DIRECT，因此繞過了 Linux 頁面快取。
這種模式可能與
不使用用戶空間驅動程式的用戶空間目標一樣接近典型的基於內核的目標。要創建 AIO bdev RPC 命令，應使用 `bdev_aio_create`。

範例命令

`rpc.py bdev_aio_create /dev/sda aio0`

此命令將從 /dev/sda 創建 `aio0` 設備。

`rpc.py bdev_aio_create /tmp/file file 4096`

此命令將從 /tmp/file 創建塊大小為 4096 的 `file` 設備。

要刪除 aio bdev，請使用 bdev_aio_delete 命令。

`rpc.py bdev_aio_delete aio0`

## OCF 虛擬 bdev {#bdev_config_cas}

OCF 虛擬 bdev 模組基於 [Open CAS Framework](https://github.com/Open-CAS/ocf) - 一個
高性能塊存儲快取元函式庫。
要啟用該模組，請使用 `--with-ocf` 標誌配置 SPDK。
OCF bdev 可用於為任何底層 bdev 啟用快取。

以下是創建 OCF bdev 的範例命令：

`rpc.py bdev_ocf_create Cache1 wt Malloc0 Nvme0n1`

此命令將創建新的 OCF bdev `Cache1`，將 bdev `Malloc0` 作為快取設備
，將 `Nvme0n1` 作為核心設備，初始快取模式為 `Write-Through`。
`Malloc0` 將用作 `Nvme0n1` 的快取，因此寫入 `Cache1` 的數據將存在
在 `Nvme0n1` 上。
預設情況下，OCF 將配置為快取行大小等於 4KiB
，並且將禁用非易失性元數據。

要刪除 `Cache1`：

`rpc.py bdev_ocf_delete Cache1`

在刪除期間，OCF 快取將停止，所有快取的數據將寫入核心設備。

請注意，OCF 對每個設備都有 RAM 要求。更多詳細資訊可以在
[OCF 文檔](https://open-cas.github.io/guide_system_requirements.html) 中找到。

## Malloc bdev {#bdev_config_malloc}

Malloc bdev 是 ramdisk。由於其性質，它們是易失性的。它們是從提供給 SPDK
應用程式的 hugepage 記憶體創建的。

創建 malloc bdev 的範例命令：

`rpc.py bdev_malloc_create -b Malloc0 64 512`

刪除 malloc bdev 的範例命令：

`rpc.py bdev_malloc_delete Malloc0`

## Null {#bdev_config_null}

SPDK null bdev 驅動程式是一個虛擬塊 I/O 目標，它丟棄所有寫入並返回未定義
的數據進行讀取。它對於以最小的塊
設備開銷對 bdev I/O 堆棧的其餘部分進行基準測試以及測試無法輕鬆使用 Malloc bdev 創建的配置很有用。
要創建 Null bdev RPC 命令，應使用 `bdev_null_create`。

範例命令

`rpc.py bdev_null_create Null0 8589934592 4096`

此命令將創建一個 8 petabyte 的 `Null0` 設備，塊大小為 4096。

要刪除 null bdev，請使用 bdev_null_delete 命令。

`rpc.py bdev_null_delete Null0`

## NVMe bdev {#bdev_config_nvme}

在 SPDK 中，有兩種方法可以基於 NVMe 設備創建塊設備。第一種
方法是連接本地 PCIe 驅動，第二種是連接 NVMe-oF 設備。
在這兩種情況下，用戶都應使用 `bdev_nvme_attach_controller` RPC 命令來實現。

範例命令

`rpc.py bdev_nvme_attach_controller -b NVMe1 -t PCIe -a 0000:01:00.0`

此命令將創建系統中物理設備的 NVMe bdev。

`rpc.py bdev_nvme_attach_controller -b Nvme0 -t RDMA -a 192.168.100.1 -f IPv4 -s 4420 -n nqn.2016-06.io.spdk:cnode1`

此命令將創建 NVMe-oF 資源的 NVMe bdev。

要刪除 NVMe 控制器，請使用 bdev_nvme_detach_controller 命令。

`rpc.py bdev_nvme_detach_controller Nvme0`

此命令將刪除名為 Nvme0 的 NVMe bdev。

SPDK NVMe bdev 驅動程式提供多路徑功能。請參閱
@ref nvme_multipath 了解詳細資訊。

### NVMe bdev 字符設備 {#bdev_config_nvme_cuse}

範例命令
