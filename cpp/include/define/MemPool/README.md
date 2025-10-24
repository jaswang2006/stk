# 内存池性能对比表
| 特性指标 | Bump Allocator | Free List | Bitmap Pool | Generation Pool | PMR Pool | TCMalloc | glibc malloc |
|---------|----------------|-----------|-------------|-----------------|----------|----------|--------------|
| **【核心性能】** |
| 分配速度 (cycles) | 2-5 | 5-10 (复用)<br>2-5 (新) | 5-15 (扫描开销)<br>2-5 (缓存命中) | 5-10<br>2-5 (快速路径) | 20-50 | 50-150 | 100-300 |
| 释放速度 (cycles) | 不支持 | 5-10 | 2-5 | 2-5 | 20-50 | 50-150 | 100-300 |
| 时间复杂度 | O(1) | O(1) | O(1) amortized<br>O(n) worst-case | O(1) | O(1) amortized | O(1) amortized | O(1) amortized<br>(bins/arena) |
| 访问开销 (cycles) | 0 (裸指针) | 0 (裸指针) | 0 (裸指针) | 1-2 (版本检查) | 0 (裸指针) | 0 (裸指针) | 0 (裸指针) |
| **【内存效率】** |
| 内存回收 | ❌ 不回收 | ✅ 完全回收 | ✅ 完全回收 | ✅ 完全回收 | ✅ 完全回收 | ✅ 完全回收 | ✅ 完全回收 |
| 内存碎片率 | 0% (不回收) | 5-15%<br>(链表跳跃) | 0%<br>(位图紧凑) | <5%<br>(固定槽位) | <10%<br>(池管理) | 10-15%<br>(span 管理) | 20-40%<br>(外碎片高) |
| 空间局部性 | 完美 (连续) | 差 (指针跳转) | 极好 (顺序扫描) | 极好 (数组访问) | 好 | 一般 | 差 |
| 引用大小 (bytes) | 8 (指针) | 8 (指针) | 8 (指针) | 4 (handle)<br>24-bit idx + 8-bit gen | 8 (指针) | 8 (指针) | 8 (指针) |
| **【Cache 特性】** |
| L1 访问次数 (alloc) | 1 (ptr++) | 2-3 (遍历链表) | 3-5 (扫描位图) | 2-3 (索引+槽位) | 3-6 | 5-10 | 10-20 |
| Cache 命中率 | 极高 (~100%)<br>(顺序分配) | 低-中<br>(随机跳，碎片化) | 高<br>(顺序访问，bitmap 热) | 极高<br>(数组，pool 热) | 中-高<br>(多级池) | 中<br>(per-thread) | 低-中<br>(多 arena) |
| TLB 命中率 | 100%<br>(单连续区域) | 70-85%<br>(跳跃访问) | 90-95%<br>(池内连续) | 90-95%<br>(池内连续) | 85-90%<br>(池碎片) | 75-85%<br>(span 分散) | 60-75%<br>(arena 分散) |
| Prefetch 友好度 | ⭐⭐⭐⭐⭐<br>(完美顺序) | ⭐⭐<br>(随机跳转) | ⭐⭐⭐⭐<br>(位图顺序) | ⭐⭐⭐⭐⭐<br>(数组顺序) | ⭐⭐⭐<br>(多级池) | ⭐⭐⭐<br>(per-CPU cache) | ⭐⭐<br>(unpredictable) |
| **【CPU 流水线】** |
| 分支预测数 (alloc) | 0-1<br>(capacity check) | 2-4<br>(null check + walk) | 2-3<br>(scan loop + found) | 2-4<br>(free_head check) | 3-6<br>(pool select) | 5-8<br>(size class + cache) | 10-15<br>(bin lookup) |
| 流水线停顿风险 | 极低<br>(predictable) | 中<br>(指针追踪，依赖链) | 低-中<br>(位扫描，可预测循环) | 低<br>(简单逻辑) | 中<br>(多分支) | 中高<br>(复杂逻辑) | 高<br>(bin 树) |
| SIMD 加速潜力 | ✅ (批量分配)<br>memset/批量初始化 | ❌<br>(链表遍历不可向量化) | ✅ (并行位扫描)<br>`__builtin_ctzll` | ✅ (批量索引)<br>batch alloc | ⚙️ (有限)<br>部分可用 | ⚙️ (有限)<br>tcache 批量 | ❌<br>不适用 |
| **【并发特性】** |
| 线程安全 | ❌ 单线程<br>(arena per-thread) | ⚙️ 需锁<br>(mutex 瓶颈) | ⚙️ 原子位操作<br>(atomic fetch_or) | ⚙️ 需锁/TLS<br>+ CAS | ⚙️ 多池分离<br>(pool_resource) | ✅ Per-thread cache<br>+ 中心堆 | ⚙️ Arena per-thread<br>+ 全局 bin |
| 跨线程回收 | 不适用 | 低效 (锁竞争) | 中等 (原子操作) | 中等 (版本号安全) | 高效 (transfer) | 高效 (central heap) | 高效 (arena 交换) |
| False sharing 风险 | 低<br>(单线程) | 中<br>(free list 竞争) | 低<br>(cacheline align，独立 word) | 低<br>(对齐槽位，独立槽位) | 中<br>(pool 元数据) | 中<br>(tcache 竞争) | 中-高<br>(bin 元数据) |
| **【调试与安全】** |
| 悬垂指针检测 | ❌ | ❌ | ❌ | ✅ 自动<br>(generation 不匹配) | ❌ | ❌<br>(需 sanitizer) | ❌ |
| Double-free 检测 | ❌ | ⚙️ 需额外标记<br>(free bit) | ⚙️ 检查位状态<br>(assert freed) | ✅ 自动<br>(generation++) | ⚙️ 有限<br>(pool 检查) | ✅ 元数据检查<br>(double-free 断言) | ✅ chunk 标记<br>(corrupted chunk) |
| ABA 问题 | 无 (单线程) | ⚠️ 有风险<br>(lock-free 场景) | 无<br>(位图无指针) | ✅ 免疫<br>(generation counter) | 无<br>(PMR 管理) | 无<br>(epoch-based) | 无<br>(arena 管理) |
| 内存泄漏检测 | 困难<br>(bulk reset only) | 中等<br>(需追踪 free) | 中等<br>(bitmap 扫描) | 容易<br>(未释放 handle) | 容易<br>(pool 统计) | 容易<br>(heap profiler) | 容易<br>(mtrace/valgrind) |
| **【实现细节】** |
| 实现复杂度 | 极低 (10-20 行)<br>ptr++ | 低 (50-100 行)<br>链表操作 | 中 (100-200 行)<br>位操作 + 扫描 | 中-高 (200-300 行)<br>handle + version | 低<br>(C++17 STL，标准库封装) | 极高 (5000+ 行)<br>per-CPU + span | 中<br>(glibc 内置，bins + arena) |
| 对齐控制 | 任意 (用户控制) | 固定 (对象大小) | 固定 (位图粒度) | 固定 (槽位大小) | 固定 (池配置) | 多级 (size class) | 动态 (用户请求) |
| 构造/析构 Hook | ✅ placement new | ✅ explicit call | ✅ explicit call | ✅ handle 管理 | ✅ allocator trait | ✅ C++ new/delete | ✅ malloc_hook |
| 批量操作支持 | ✅ 极快 (memcpy) | ⚙️ 需循环 | ✅ SIMD 扫描 | ✅ 批量 handle | ⚙️ allocate_bulk | ⚙️ tcache 批量 | ❌ |
| **【适用场景】** |
| 最佳场景 | • 临时缓冲<br>• 批量分配<br>• 只增不删<br>• 单帧资源 | • 中频增删<br>• 对象池<br>• 短生命周期<br>• 节点缓存 | • 高频增删<br>• 固定容量<br>• 紧凑存储需求<br>• 小对象池 (<1KB) | • 需检测悬垂<br>• Handle > 指针<br>• ECS 架构<br>• 游戏引擎实体 | • 通用哈希表<br>• STL 容器<br>• 快速开发<br>• 原型验证 | • 高并发服务<br>• 长时间运行<br>• 多线程密集<br>• Web 后端 | • 系统默认<br>• 通用场景<br>• 无特殊需求<br>• 兼容性优先 |
| LOB 应用建议 | **Order 对象**<br>(生命周期长) | 不推荐<br>(性能无优势) | **HashMap Entry**<br>(增删频繁) | **Order** (若需悬垂检测)<br>+ 安全性 | **price_levels_**<br>(快速开发) | 不适合<br>(LOB 单线程) | 不适合<br>(开销过大) |
| **【综合评分】** |
| 极致性能场景 (单线程) | ⭐⭐⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐ |
| 内存效率 (利用率) | ⭐⭐ (不回收) | ⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐ |
| 工程实用性 (易用+维护) | ⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ |
| 调试友好度 (错误检测) | ⭐⭐ (无保护) | ⭐⭐⭐ (有限) | ⭐⭐⭐ (位图检查) | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ |