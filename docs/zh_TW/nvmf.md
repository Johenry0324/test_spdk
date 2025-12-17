# NVMe over Fabrics Target {#nvmf}

@sa @ref nvme_fabrics_host
@sa @ref tracepoints

## NVMe-oF Target 入門指南 {#nvmf_getting_started}

SPDK NVMe over Fabrics target 是一個用戶空間應用程式，通過結構網絡（如以太網、Infiniband 或光纖通道）呈現塊設備。
SPDK 目前支持 RDMA 和 TCP 傳輸。

NVMe over Fabrics 規範定義了可以通過不同傳輸導出的子系統。
SPDK 選擇將導出這些子系統的軟體稱為"target"，這是 iSCSI 中使用的術語。
規範將連接到 target 的"客戶端"稱為"host"。許多人
也會將 host 稱為"initiator"，這是 iSCSI
術語中的等效術語。SPDK 將嘗試堅持使用"target"和"host"術語以匹配規範。

Linux 內核也實現了 NVMe-oF target 和 host，並且 SPDK 已針對
與 Linux 內核實現的互操作性進行了測試。

如果您想使用信號終止應用程式，請確保使用 SIGTERM，然後應用程式
將在退出前釋放所有共享記憶體資源，SIGKILL 將使共享記憶體
資源沒有機會被應用程式釋放，您可能需要手動釋放資源。

## RDMA 傳輸支持 {#nvmf_rdma_transport}

它需要支持 RDMA 的 NIC 及其相應的 OFED（OpenFabrics Enterprise Distribution）
軟體包才能運行。也許操作系統分發版提供包，但 OFED 也可在
[這裡](https://downloads.openfabrics.org/OFED/) 獲得。

### 先決條件 {#nvmf_prereqs}

要使用 RDMA 傳輸構建 nvmf_tgt，有一些額外的依賴項，
可以使用 pkgdep.sh 腳本安裝。

~~~{.sh}
sudo scripts/pkgdep.sh --rdma
~~~

然後啟用 RDMA 構建 SPDK：

~~~{.sh}
./configure --with-rdma <other config parameters>
make
~~~

構建完成後，二進制文件將位於 `build/bin` 中。

### InfiniBand/RDMA Verbs 的先決條件 {#nvmf_prereqs_verbs}

在使用 RDMA 傳輸啟動我們的 NVMe-oF target 之前，我們必須加載 InfiniBand 和 RDMA 模組
，這些模組允許用戶空間進程直接使用 InfiniBand/RDMA verbs。

~~~{.sh}
modprobe ib_cm
modprobe ib_core
# 請注意，ib_ucm 在較新版本的內核中不存在，不需要。
modprobe ib_ucm || true
modprobe ib_umad
modprobe ib_uverbs
modprobe iw_cm
modprobe rdma_cm
modprobe rdma_ucm
~~~

### RDMA NIC 的先決條件 {#nvmf_prereqs_rdma_nics}

在啟動我們的 NVMe-oF target 之前，我們必須檢測 RDMA NIC 並為它們分配 IP 地址。

### 查找 RDMA NIC 和相關網絡介面

~~~{.sh}
ls /sys/class/infiniband/*/device/net
~~~

#### Mellanox ConnectX-3 RDMA NIC

~~~{.sh}
modprobe mlx4_core
modprobe mlx4_ib
modprobe mlx4_en
~~~

#### Mellanox ConnectX-4 RDMA NIC

~~~{.sh}
modprobe mlx5_core
modprobe mlx5_ib
~~~

#### 為 RDMA NIC 分配 IP 地址

~~~{.sh}
ifconfig eth1 192.168.100.8 netmask 255.255.255.0 up
ifconfig eth2 192.168.100.9 netmask 255.255.255.0 up
~~~

### RDMA 限制 {#nvmf_rdma_limitations}

由於 RDMA NIC 對註冊的記憶體區域數量有限制，SPDK NVMe-oF
target 應用程式最終可能開始無法分配更多 DMA 記憶體。這是
DPDK 動態記憶體管理的不完善之處，最有可能在運行時保留太多
2MB hugepage 時發生。一種記憶體瓶頸是 NIC 記憶體
區域的數量，例如，某些 NIC 報告最多 2048 個記憶體區域的最大數量。這
為我們提供了使用 2MB hugepage 的總記憶體區域 4GB 記憶體限制。可以通過
使用 1GB hugepage 或在應用程式啟動時使用 `--mem-size` 或 `-s`
選項預保留記憶體來克服。所有預保留的記憶體將註冊為單個區域，但不會返回到
系統，直到 SPDK 應用程式終止。

另一個已知問題發生在 RoCE 模式下使用 E810 NIC 時。具體來說，NVMe-oF target
有時無法銷毀 qpair，因為其已發布的工作請求沒有被刷新。這可能導致
NVMe-oF target 應用程式無法乾淨地終止。

## TCP 傳輸支持 {#nvmf_tcp_transport}

該傳輸預設內置於 nvmf_tgt 中，不需要任何特殊函式庫。

## FC 傳輸支持 {#nvmf_fc_transport}

要使用 FC 傳輸構建 nvmf_tgt，需要額外的 FC LLD（低級驅動程式）代碼依賴。
請聯繫您的 FC 供應商以獲取獲取 FC 驅動程式模組的說明。

### Broadcom FC LLD 代碼

Broadcom FC NVMe 適配器的 FC LLD 驅動程式可以從
https://github.com/ecdufcdrvr/bcmufctdrvr 獲得。

### 獲取 FC LLD 模組，然後啟用 FC 構建 SPDK

克隆 SPDK repo 並初始化子模組後，構建 FC LLD 函式庫，然後可以與
fc 傳輸鏈接。

~~~{.sh}
git clone https://github.com/spdk/spdk --recursive
git clone https://github.com/ecdufcdrvr/bcmufctdrvr fc
cd fc
make DPDK_DIR=../spdk/dpdk/build SPDK_DIR=../spdk
cd ../spdk
./configure --with-fc=../fc/build
make
~~~

## 配置 SPDK NVMe over Fabrics Target {#nvmf_config}

可以使用 JSON RPC 配置 NVMe over Fabrics target。
下面詳細介紹了配置 NVMe-oF 子系統所需的基本 RPC。有關
使用 NVMe over Fabrics 特定 RPC 的更多資訊，可以在 @ref jsonrpc_components_nvmf_tgt RPC 頁面上找到。

### 使用 RPC {#nvmf_config_rpc}

以提升的權限啟動 nvmf_tgt 應用程式。一旦 target 啟動，
可以使用 nvmf_create_transport rpc 來初始化給定的傳輸。下面是一個
範例，其中 target 啟動並配置了兩種不同的傳輸。
RDMA 傳輸配置為 I/O 單元大小 8192 字節，最大 I/O 大小 131072，以及
膠囊內數據大小 8192 字節。TCP 傳輸配置為 I/O 單元大小
16384 字節，每個控制器 8 個最大 qpair，以及膠囊內數據大小 8192 字節。

~~~{.sh}
build/bin/nvmf_tgt
scripts/rpc.py nvmf_create_transport -t RDMA -u 8192 -i 131072 -c 8192
scripts/rpc.py nvmf_create_transport -t TCP -u 16384 -m 8 -c 8192
~~~

下面是一個創建 malloc bdev 並將其分配給子系統的範例。調整 bdev、
NQN、序列號和帶 RDMA 傳輸的 IP 地址以適應您自己的情況。如果您將
"rdma" 替換為"TCP"，則子系統將添加一個帶 TCP 傳輸的監聽器。

~~~{.sh}
scripts/rpc.py bdev_malloc_create -b Malloc0 512 512
scripts/rpc.py nvmf_create_subsystem nqn.2016-06.io.spdk:cnode1 -a -s SPDK00000000000001 -d SPDK_Controller1
scripts/rpc.py nvmf_subsystem_add_ns nqn.2016-06.io.spdk:cnode1 Malloc0
scripts/rpc.py nvmf_subsystem_add_listener nqn.2016-06.io.spdk:cnode1 -t rdma -a 192.168.100.8 -s 4420
~~~

### NQN 正式定義

NVMe 限定名稱或 NQN 在
[NVMe 規範](http://nvmexpress.org/wp-content/uploads/NVM_Express_Revision_1.3.pdf) 的第 7.9 節中定義。SPDK 已嘗試使用
[擴展 Backus-Naur 形式](https://en.wikipedia.org/wiki/Extended_Backus%E2%80%93Naur_form) 形式化該定義。
SPDK 模組在驗證 NQN 時使用此正式定義（如下所示）。
