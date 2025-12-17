# 什麼是 SPDK {#about}

存儲性能開發套件 (SPDK) 提供了一套工具和
函式庫，用於編寫高性能、可擴展的用戶模式存儲
應用程式。它通過使用許多關鍵
技術來實現高性能：

* 將所有必要的驅動程式移入用戶空間，這避免了系統調用
  並使應用程式能夠進行零複製訪問。
* 輪詢硬體以獲取完成狀態，而不是依賴中斷，這
  降低了總延遲和延遲方差。
* 在 I/O 路徑中避免所有鎖，而是依賴訊息傳遞。

SPDK 的基礎是一個用戶空間、輪詢模式、異步、無鎖
[NVMe](http://www.nvmexpress.org) 驅動程式。這提供了從用戶空間應用程式直接到 SSD 的零複製、高度
並行訪問。該驅動程式是
作為一個 C 函式庫編寫的，具有單個公共頭文件。有關更多
詳細資訊，請參閱 @ref nvme。

SPDK 進一步提供了一個完整的塊堆棧作為用戶空間函式庫，該函式庫執行
與操作系統中的塊堆棧相同的許多操作。這
包括統一不同存儲設備之間的介面、隊列處理
以處理記憶體不足或 I/O 掛起等條件，以及邏輯卷
管理。有關更多資訊，請參閱 @ref bdev。

最後，SPDK 提供了
[NVMe-oF](http://www.nvmexpress.org/nvm-express-over-fabrics-specification-released)、
[iSCSI](https://en.wikipedia.org/wiki/ISCSI) 和
[vhost](http://blog.vmsplice.net/2011/09/qemu-internals-vhost-architecture.html)
服務器，這些服務器構建在這些組件之上，能夠通過
網絡或向其他進程提供磁盤。NVMe-oF 和 iSCSI 的標準 Linux 內核啟動器
與這些目標互操作，以及使用 vhost 的 QEMU。
這些服務器的 CPU 效率可能比其他
實現高一個數量級。這些目標可以用作如何實現高性能存儲目標的範例，
或用作生產
部署的基礎。
