# 新 API 契约

以下契约均已在新 DarwinCore 头文件中逐项确认，以代码为准。

**头文件路径**：`3rd/include/darwincore/`

---

## EventLoopGroup

```cpp
// 3rd/include/darwincore/network/base/event_loop_group.h
explicit EventLoopGroup(size_t io_thread_count, std::string name_prefix = "io");
~EventLoopGroup();

// 不可拷贝
EventLoopGroup(const EventLoopGroup&) = delete;
EventLoopGroup& operator=(const EventLoopGroup&) = delete;

void SetThreadInitCallback(ThreadInitCallback cb);  // 必须在 Start() 前设置
bool Start();   // 启动所有线程（acceptor + I/O），阻塞直到所有 loop 就绪
void Stop();    // 停止所有线程，等待线程退出

EventLoop& GetAcceptorLoop();                       // Start() 后可用
EventLoop& GetNextIoLoop();                         // round-robin，线程安全
EventLoop& GetIoLoopForHash(uint64_t hash);         // hash 固定分配，线程安全
std::vector<std::reference_wrapper<EventLoop>> GetAllIoLoops();
size_t io_thread_count() const;
```

- `EventLoopGroup` 必须在 `Server` 之前创建，`Server` 只借用引用，不拥有。
- `Start()` 之后方可传给 `Server` 使用。

---

## network::Server

```cpp
// 3rd/include/darwincore/network/transport/server.h
explicit Server(EventLoopGroup& loop_group, std::string name = "Server");
~Server();

// 不可拷贝
Server(const Server&) = delete;
Server& operator=(const Server&) = delete;

// 开始监听（必须在设置回调之后调用）
bool StartIPv4(const std::string& host, uint16_t port);
bool StartIPv6(const std::string& host, uint16_t port);
bool StartUnix(const std::string& path);
void Stop();

// 广播到所有连接
void Broadcast(const void* data, size_t size);
void Broadcast(const std::string& data);

// 回调设置（必须在 Start 前设置）
void SetConnectionCallback(ConnectionCallback cb);
void SetMessageCallback(MessageCallback cb);
void SetWriteCompleteCallback(WriteCompleteCallback cb);

size_t ConnectionCount() const;
```

- 回调必须在 `StartIPv4()` / `StartIPv6()` / `StartUnix()` 之前设置。
- `Server` 内部维护连接表（`uint64_t → ConnectionPtr`），外部不再需要维护全局 map。

---

## ConnectionPtr / Connection

```cpp
// 3rd/include/darwincore/network/transport/connection.h
using ConnectionPtr = std::shared_ptr<Connection>;
using ConnectionCallback    = std::function<void(const ConnectionPtr&)>;
using MessageCallback       = std::function<void(const ConnectionPtr&, Buffer&, Timestamp)>;
using WriteCompleteCallback = std::function<void(const ConnectionPtr&)>;
using CloseCallback         = std::function<void(const ConnectionPtr&)>;

enum class State {
    kConnecting,
    kConnected,
    kDisconnecting,
    kDisconnected,
};

// 构造/析构（由 Server 内部调用，外部不直接构造）
Connection(EventLoop& loop, int fd, uint64_t conn_id, const std::string& peer_addr = "");
~Connection();

// 不可拷贝、不可移动
Connection(const Connection&) = delete;
Connection& operator=(const Connection&) = delete;

// ---- 生命周期（只能在所属 loop 线程调用）----
void ConnectEstablished();   // 状态 kConnecting -> kConnected，启动读
void ConnectDestroyed();     // 清理：DisableAll, Remove, close(fd)

// ---- 发送（线程安全，内部 RunInLoop）----
void Send(const void* data, size_t size);
void Send(const std::string& data);
void Send(Buffer& buffer);         // 移走 buffer 内容，高吞吐路径

// ---- 半关闭 / 强制关闭 ----
void ShutdownWrite();   // 发完 output_buf_ 后 shutdown(fd, SHUT_WR)
void ForceClose();      // 直接触发 HandleClose

// ---- 背压 ----
void PauseRead();
void ResumeRead();

// ---- 查询 ----
bool     IsConnected() const;
uint64_t GetConnectionId() const;
State    GetState() const;
EventLoop& GetLoop();
const std::string& PeerAddr() const;

// ---- 用户上下文（单值，上层挂载 session 对象）----
// Connection 直接持有 std::any 上下文
void SetContext(std::any ctx);
std::any GetContext() const;

// 使用示例：
// auto session = std::make_shared<WebSocketSession>();
// conn->SetContext(session);
// ...
// auto session = std::any_cast<std::shared_ptr<WebSocketSession>>(conn->GetContext());

// ---- 回调设置 ----
void SetConnectionCallback(ConnectionCallback cb);
void SetMessageCallback(MessageCallback cb);
void SetWriteCompleteCallback(WriteCompleteCallback cb);
void SetCloseCallback(CloseCallback cb);
CloseCallback GetCloseCallback() const;

// 访问内部缓冲区（用于协议层直接操作）
Buffer& InputBuffer();   // 即 MessageCallback 中的 Buffer&
Buffer& OutputBuffer();
```

- `ConnectionCallback` 连接建立和断开共用同一回调，通过 `conn->IsConnected()` 区分。
- `ConnectionPtr` 是 `shared_ptr`，在回调内持有是安全的，但不要在连接关闭后继续长期持有（会阻止析构）。

---

## Buffer

```cpp
// 3rd/include/darwincore/network/transport/buffer.h
static constexpr size_t kInitialSize = 4096;
static constexpr size_t kPrependSize = 8;

explicit Buffer(size_t initial_size = kInitialSize);

// 不可拷贝，可移动
Buffer(const Buffer&) = delete;
Buffer& operator=(const Buffer&) = delete;
Buffer(Buffer&&) noexcept = default;
Buffer& operator=(Buffer&&) noexcept = default;

// ---- 区域查询 ----
size_t ReadableBytes()    const;
size_t WritableBytes()    const;
size_t PrependableBytes() const;

// 返回可读区首地址
const uint8_t* Peek() const;
uint8_t*       BeginWrite();

// ---- 读取 ----
void Retrieve(size_t len);                    // 消费 len 字节（只移动读指针）
void RetrieveAll();                           // 消费所有可读字节，复位到初始状态
std::string RetrieveAsString(size_t len);     // 消费 len 字节并以 string 返回
std::string RetrieveAllAsString();            // 消费所有可读字节并以 string 返回

// ---- 写入 ----
void Append(const void* data, size_t len);
void Append(const std::string& data);
void Prepend(const void* data, size_t len);   // 在 prepend 区写入（用于帧长前缀等）

// ---- I/O ----
ssize_t ReadFd(int fd, int* saved_errno);     // 从 fd 读取
ssize_t WriteFd(int fd, int* saved_errno);    // 将可读区写入 fd

// ---- 水位 ----
void SetHighWaterMark(size_t size);
void SetLowWaterMark(size_t size);
bool IsHighWaterMark() const;
bool IsLowWaterMark()  const;
```

- `MessageCallback` 的 `Buffer&` 参数允许原地消费（调用 `Retrieve`）。
- 零拷贝路径：`Peek()` 读取指针 + `Retrieve()` 消费，无需构造 `vector<uint8_t>`。

---

## Timestamp

```cpp
// 3rd/include/darwincore/network/base/timestamp.h
// 用于 MessageCallback 参数，记录数据到达时间
// 当前实现中可忽略，但需保留在回调签名中
```
