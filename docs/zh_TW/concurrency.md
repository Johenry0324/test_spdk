# 訊息傳遞與並發 {#concurrency}

## 理論

SPDK 的主要目標之一是隨著硬體的添加而線性擴展。這在實踐中可能意味著很多事情。例如，從一個
SSD 移動到兩個應該使每秒 I/O 數量翻倍。或者將 CPU 核心數量翻倍應該使可能的計算量翻倍。
或者甚至將 NIC 數量翻倍應該使網絡吞吐量翻倍。為了實現這一點，軟體的執行線程必須盡可能地
彼此獨立。在實踐中，這意味著避免軟體鎖甚至原子指令。

傳統上，軟體通過將一些共享數據放在
堆上，用鎖保護它，然後讓所有執行線程
僅在訪問數據時獲取鎖來實現並發。這個模型有許多很好的特性：

* 很容易將單線程程序轉換為多線程程序
  因為您不必從單線程版本更改數據模型。您在數據周圍添加一個鎖。
* 您可以將程序編寫為同步的、命令式的語句列表，
  從上到下閱讀。
* 調度器可以中斷線程，允許高效的時間共享
  CPU 資源。

不幸的是，隨著線程數量的增加，共享數據周圍的鎖上的競爭也會增加。更細粒度的鎖定有幫助，
但也增加了程序的複雜性。即使如此，超過一定數量的競爭
鎖，線程將花費大部分時間嘗試獲取鎖，程序將不會從更多 CPU 核心中受益。

SPDK 採用了完全不同的方法。SPDK 不是將共享數據放在
所有線程在獲取鎖後訪問的全域位置，而是經常
將該數據分配給單個線程。當其他線程想要訪問數據時，
它們向擁有線程傳遞訊息以代表它們執行操作。
當然，這種策略一點也不新。例如，它是
[Erlang](http://erlang.org/download/armstrong_thesis_2003.pdf) 的核心設計原則之一，也是 [Go](https://tour.golang.org/concurrency/2) 中的主要
並發機制。SPDK 中的訊息
由函數指針和指向某些上下文的指針組成。訊息
使用 [無鎖環](http://dpdk.org/doc/guides/prog_guide/ring_lib.html) 在線程之間傳遞。訊息
傳遞通常比大多數軟體開發人員的直覺讓他們相信的要快得多，這是由於快取效應。
如果單個核心正在訪問相同的數據
（代表所有其他核心），那麼該數據更有可能
在更接近該核心的快取中。通常最有效的方法是讓每個核心處理
位於其本地快取中的一小部分數據，然後在完成時將一個小
訊息傳遞給下一個核心。

在更極端的情況下，即使訊息傳遞可能成本太高，每個線程
可能會製作數據的本地副本。線程將只引用其本地
副本。為了變更數據，線程將向每個其他線程發送訊息
告訴它們在其本地副本上執行更新。這在數據不經常變更但讀取非常頻繁時很好，
並且經常在 I/O 路徑中使用。當然，這是以記憶體大小換取計算
效率，因此它只用於最關鍵的代碼路徑。

## 訊息傳遞基礎設施

SPDK 提供了多層訊息傳遞基礎設施。例如，SPDK 中最
基本的函式庫，它們自己不做任何訊息傳遞，而是在
文檔中列舉關於何時可以調用函數的規則（例如 @ref nvme）。
但是，大多數函式庫依賴於 SPDK 的
[thread](http://www.spdk.io/doc/thread_8h.html)
抽象，位於 `libspdk_thread.a` 中。線程抽象提供了
基本的訊息傳遞框架並定義了一些關鍵原語。

### 線程

首先，`spdk_thread` 是輕量級、無堆疊執行線程的
抽象。較低級別的框架可以通過調用 `spdk_thread_poll()` 執行 `spdk_thread` 單個
時間片。較低級別的框架允許
隨時在系統線程之間移動 `spdk_thread`，只要在任何給定時間只有一個系統線程在該
`spdk_thread` 上執行 `spdk_thread_poll()`。
可以通過調用 `spdk_thread_create()` 隨時創建新的輕量級線程，並通過調用
`spdk_thread_destroy()` 銷毀。輕量級線程是 SPDK 中線程的基礎抽象。

### 輪詢器

然後在 `spdk_thread` 之上分層了一些額外的抽象。一個是 `spdk_poller`，它是
應該在給定線程上重複調用的函數的抽象。另一個是
`spdk_msg_fn`，它是一個函數指針和一個上下文指針，可以
通過 `spdk_thread_send_msg()` 發送到線程執行。

### IO 設備和通道

該函式庫還定義了兩個額外的抽象：`spdk_io_device` 和
`spdk_io_channel`。在實現 SPDK 的過程中，我們注意到相同的
模式在許多不同的函式庫中出現。為了實現
訊息傳遞策略，代碼將描述具有全域狀態的某個對象
以及與該對象相關的每個線程上下文，該上下文在 I/O 路徑中訪問以避免鎖定全域狀態。
這種模式在最底層最清楚，其中 I/O 被提交到塊設備。這些
設備通常公開多個可以分配給線程然後
無鎖訪問以提交 I/O 的隊列。為了抽象這一點，我們將
設備概括為 `spdk_io_device`，將線程特定的隊列概括為 `spdk_io_channel`。
然而，隨著時間的推移，這種模式出現在大量與我們最初選擇的名稱不太匹配的地方。
在今天的代碼中，`spdk_io_device` 是任何指針，其唯一性僅基於其
記憶體地址，而 `spdk_io_channel` 是與
特定 `spdk_io_device` 相關的每個線程上下文。

線程抽象提供函數以向任何其他線程發送訊息，向所有線程一個接一個地發送訊息，
以及向所有為給定 io_device 存在 io_channel 的線程發送訊息。

最關鍵的是，線程抽象實際上不會產生任何系統級別的
線程。相反，它依賴於某個較低級別
框架的存在，該框架產生系統線程並設置事件循環。在這些事件
循環內部，線程抽象只需要較低級別的框架
重複調用每個存在的 `spdk_thread()` 上的 `spdk_thread_poll()`。這使得 SPDK 非常可移植到各種異步、基於事件的
框架，如 [Seastar](https://www.seastar.io) 或 [libuv](https://libuv.org/)。

## 線程生命週期

`spdk_thread` 抽象本身不強加任何嚴格的線程生命週期
要求，允許在運行時創建或銷毀 `spdk_thread`。

當使用構建在 `spdk_thread` 之上的其他抽象（如 `spdk_poller` 或
`spdk_io_channel`）時，底層 `spdk_thread` 生命週期必須超過
使用它的抽象的生命週期。

應用程式必須遵守適當的去初始化/完成順序，以相反順序銷毀資源，
以便線程比使用它的資源更長壽。

簡化範例：

1. 應用程式啟動
1. `spdk_io_device_register()`
1. `spdk_thread_create()`
1. `spdk_get_io_channel()`
1. 在每個通道上工作，接收終止信號
1. `spdk_put_io_channel()`
1. `spdk_thread_exit()`
1. `spdk_thread_destroy()`
1. `spdk_io_device_unregister()`
1. 應用程式退出

## SPDK 自旋鎖

在某些情況下會使用鎖。這些應該限制在
上述訊息傳遞介面的範圍內。當需要鎖時，
應該使用 SPDK 自旋鎖而不是 POSIX 鎖。

像 `pthread_mutex_t` 和 `pthread_spinlock_t` 這樣的 POSIX 鎖不能正確
處理 SPDK 輕量級線程之間的鎖定。SPDK 的 `spdk_spinlock`
在 SPDK 函式庫和應用程式中使用是安全的。這種安全性來自
對何時可以持有鎖的限制。有關詳細資訊，請參閱
[spdk_spinlock](structspdk__spinlock.html)。

## 事件框架

SPDK 項目不想正式為其提供的所有範例應用程式選擇異步、基於事件的
框架，為了支持盡可能廣泛的框架。但是應用程式確實
需要實現異步事件循環的東西才能
運行，因此進入位於 `lib/event` 中的 `event` 框架。此框架
包括輪詢和調度輕量級線程、安裝
信號處理程序以乾淨地關閉以及基本命令行選項解析等內容。
只有已建立的應用程式才應該考慮直接集成較低
級別的函式庫。

## C 語言的限制

訊息傳遞是高效的，但它會導致異步代碼。
不幸的是，異步代碼在 C 中是一個挑戰。它通常通過
傳遞在操作完成時調用的函數指針來實現。這
將代碼切碎，使其不容易跟隨，特別是通過邏輯
分支。最好的解決方案是使用支持
[futures 和 promises](https://en.wikipedia.org/wiki/Futures_and_promises) 的語言，
例如 C++、Rust、Go 或幾乎任何其他更高級別的語言。但是，SPDK 是一個低
級別函式庫，需要非常廣泛的兼容性和可移植性，所以我們
選擇繼續使用普通的舊 C。

不過，我們確實有一些建議可以分享。對於 _簡單_ 的回調鏈，
如果您從下到上編寫函數，這是最容易的。我們的意思是如果
函數 `foo` 執行某個異步操作，當該操作完成時
調用函數 `bar`，然後函數 `bar` 執行某個操作，該操作在完成時
調用函數 `baz`，編寫它的好方法是：

```c
    void baz(void *ctx) {
            ...
    }

    void bar(void *ctx) {
            async_op(baz, ctx);
    }

    void foo(void *ctx) {
            async_op(bar, ctx);
    }
```

不要拆分這些函數 - 將它們保持為一個可以從下到上閱讀的單元。

對於更複雜的回調鏈，特別是具有邏輯分支
或循環的鏈，最好寫出狀態機。事實證明，支持 futures 和 promises 的更高級別語言只是在編譯時生成狀態
機，所以即使我們沒有在 C 中生成它們的能力，我們仍然可以手動寫出它們。
作為範例，這是一個
執行 `foo` 5 次然後調用 `bar` 的回調鏈 - 有效地
一個異步 for 循環。

```c
    enum states {
            FOO_START = 0,
            FOO_END,
            BAR_START,
            BAR_END
    };

    struct state_machine {
            enum states state;

            int count;
    };

    static void
    foo_complete(void *ctx)
    {
        struct state_machine *sm = ctx;

        sm->state = FOO_END;
        run_state_machine(sm);
    }

    static void
    foo(struct state_machine *sm)
    {
        do_async_op(foo_complete, sm);
    }

    static void
    bar_complete(void *ctx)
    {
        struct state_machine *sm = ctx;

        sm->state = BAR_END;
        run_state_machine(sm);
    }

    static void
    bar(struct state_machine *sm)
    {
        do_async_op(bar_complete, sm);
    }

    static void
    run_state_machine(struct state_machine *sm)
    {
        enum states prev_state;

        do {
            prev_state = sm->state;

            switch (sm->state) {
                case FOO_START:
                    foo(sm);
                    break;
                case FOO_END:
                    /* This is the loop condition */
                    if (sm->count++ < 5) {
                        sm->state = FOO_START;
                    } else {
                        sm->state = BAR_START;
                    }
                    break;
                case BAR_START:
                    bar(sm);
                    break;
                case BAR_END:
                    break;
            }
        } while (prev_state != sm->state);
    }

    void do_async_for(void)
    {
            struct state_machine *sm;

            sm = malloc(sizeof(*sm));
            sm->state = FOO_START;
            sm->count = 0;

            run_state_machine(sm);
    }
```

這當然很複雜，但是 `run_state_machine` 函數可以
從上到下閱讀，以清楚地了解代碼中發生的事情，
而不必追蹤每個回調。
