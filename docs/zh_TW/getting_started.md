# 入門指南 {#getting_started}

## 取得原始碼 {#getting_started_source}

~~~{.sh}
git clone https://github.com/spdk/spdk --recursive
~~~

## 安裝必要條件 {#getting_started_prerequisites}

`scripts/pkgdep.sh` 腳本會自動安裝構建 SPDK 所需的最基本依賴項。
使用 `--help` 查看安裝可選組件依賴項的資訊。

~~~{.sh}
sudo scripts/pkgdep.sh
~~~

選項 --all 將安裝 SPDK 功能所需的所有依賴項。

~~~{.sh}
sudo scripts/pkgdep.sh --all
~~~

## 構建 {#getting_started_building}

Linux:

~~~{.sh}
./configure
make
~~~

FreeBSD:
注意：確保您在 /usr/src/ 中有匹配的內核源碼

~~~{.sh}
./configure
gmake
~~~

configure 腳本有許多選項可用，可以通過運行以下命令查看：

~~~{.sh}
./configure --help
~~~

請注意，並非所有功能都預設啟用。例如，RDMA
支援（以及 NVMe over Fabrics）預設不啟用。您可以
通過以下方式啟用它：

~~~{.sh}
./configure --with-rdma
make
~~~

## 運行單元測試 {#getting_started_unittests}

通過運行單元測試來確認構建是否成功總是個好主意。

~~~{.sh}
./test/unit/unittest.sh
~~~

運行單元測試時您會看到一些錯誤訊息，但這些是
測試套件的一部分。腳本末尾的最終訊息指示
成功或失敗。

## 運行範例應用程式 {#getting_started_examples}

在運行 SPDK 應用程式之前，必須分配一些 hugepages 並且
必須將任何 NVMe 和 I/OAT 設備從原生內核驅動程序中解綁。
SPDK 包含一個腳本，可在 Linux 和 FreeBSD 上自動執行此過程。
此腳本應以 root 身份運行。它只需要在系統上運行一次。

~~~{.sh}
sudo scripts/setup.sh
~~~

要將設備重新綁定回內核，您可以運行

~~~{.sh}
sudo scripts/setup.sh reset
~~~

預設情況下，腳本分配 2048MB 的 hugepages。要更改此數字，
請按如下方式指定 HUGEMEM（以 MB 為單位）：

~~~{.sh}
sudo HUGEMEM=4096 scripts/setup.sh
~~~

在 Linux 機器上，HUGEMEM 將向上舍入到系統預設 huge page
大小邊界。

可以通過運行以下命令查看所有可用參數：

~~~{.sh}
scripts/setup.sh help
~~~

範例代碼位於 examples 目錄中。範例是構建過程的一部分
自動編譯。只需調用任何範例而不帶參數即可查看幫助輸出。
如果您的系統啟用了 IOMMU，您可以以普通用戶身份運行範例。
如果沒有，您需要以特權用戶（root）身份運行。

一個很好的入門範例是 `build/bin/spdk_nvme_identify`，它會打印
出系統上所有 NVMe 設備的資訊。

更大、功能更完整的應用程式可在 `app`
目錄中找到。這包括 [iSCSI target](https://spdk.io/doc/iscsi.html)
和 [NVMe-oF target](https://spdk.io/doc/nvmf.html) 以及工具如
[spdk_top](https://spdk.io/doc/spdk_top.html)。這個巧妙的程式模擬
常規 `top` 應用程式，並以互動列表的形式顯示 SPDK 線程、輪詢器和 SPDK 分配的
CPU 核心統計資訊。
