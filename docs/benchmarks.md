# 基准方法与实测结果

记录日期：2026-10-23。数据位于 [`benchmarks/results/`](../benchmarks/results/)，程序为 [`render_bench.cpp`](../benchmarks/render_bench.cpp)。报告比较本项目三种遍历方式，不与 GPU 或成熟生产渲染器进行性能排名。

## 实验设计

| 项目 | 条件 |
|---|---|
| CPU | AMD Ryzen 7 7435H，16 个逻辑处理器 |
| 系统 | Windows，构建号 26300 |
| 编译 | GCC 16.2.0，Release，`-O3 -DNDEBUG` |
| 图像 | 640×360，固定相机与程序生成资源 |
| 着色 | Textured、Bilinear、透视校正开启 |
| 场景 | cube、small（4096 个小三角形）、overdraw（48 层） |
| 参数 | Tile=8/16/32/64；工作线程=1/2/4/8 |
| 重复 | 每组每轮预热 2 帧，测量 7 帧，共 3 轮 |
| 顺序 | 每轮以固定种子随机排列配置，种子为 20261024/20261025/20261026 |

每场景有 Reference 1 组、TileSerial 4 组、TileParallel 16 组，合计 63 组。每组汇总 21 个样本，共 1323 个计时帧；种子是可复现的整数参数，不表示实际运行日期。

### 计时与校验范围

`frame_ms` 使用 `steady_clock`，包括清屏、MVP、齐次裁剪、屏幕准备、分桶、光栅化以及本帧临时几何/渲染对象清理。资源生成、线程池构造、结果验证和文件输出不计入该字段。线程池在每组配置开始时构造，随后跨帧复用。

每组在计时前比较一次参考结果；每个正式计时帧停止计时后，再逐值比较 float RGB 与深度。全部 1323 行的 `exact_match=1`。预热帧不写 CSV，也不计入该数量。独立手算和边界检查另见 [验证说明](validation.md)。

`prepare_ms/bin_ms/raster_ms` 是内部阶段字段，其和不能替代完整 `frame_ms`。`auxiliary_bytes` 是部分 vector 容量估计，排除了分配器、队列、线程栈和场景纹理；Framebuffer 另列为 `framebuffer_bytes`。这些数值不是进程总内存或峰值 RSS。

## 固定配置对照

统一选 Tile=32、4 个工作线程，不用每场景挑选的最快配置代替固定对照。表中是各组 21 个帧样本的中位数。

```text
同 Tile 并行加速比 = median(T_tile_serial) / median(T_parallel)
相对 Reference    = median(T_reference)   / median(T_parallel)
```

| 场景 | Reference ms | Tile 串行 ms | Tile 并行 ms | 同 Tile 并行加速比 | 相对 Reference |
|---|---:|---:|---:|---:|---:|
| cube | 3.096 | 3.025 | 0.973 | 3.11× | 3.18× |
| small | 3.571 | 3.825 | 3.186 | 1.20× | 1.12× |
| overdraw | 73.076 | 73.374 | 20.040 | 3.66× | 3.65× |

## 参数扫描与负结果

| 场景 | 扫描内最快并行配置 | 中位数 ms | 慢于 Reference 的并行配置 |
|---|---|---:|---:|
| cube | Tile=64，threads=8 | 0.662 | 4/16 |
| small | Tile=32，threads=8 | 2.809 | 8/16 |
| overdraw | Tile=32，threads=8 | 11.269 | 4/16 |

最快配置是扫描后的探索性结果，不是跨场景或跨硬件最优参数。

- small 的参考帧中，变换与裁剪耗时的中位数为 1.296 ms，参考整帧中位数为 3.571 ms，两者之比约 36.3%。这个比值由分项中位数计算，不是每帧比例的平均值。只并行光栅化无法消除这一串行部分。
- 小 Tile 增加桶引用、任务和调度成本；大 Tile 减少任务数，也可能改变负载分布。细粒度任务不保证提速。
- 没有硬件 Cache 计数器，不能把时间改善直接归因于 Cache Miss 降低。
- 后台负载、功耗、温度和频率未完全隔离。重复与随机顺序降低了偶然性，但不足以证明统计显著性或普遍性能优势。

结论是**条件性改善**：当前硬件与样例中，足够重的光栅工作受益于 Tile 并行；串行准备和调度成本限制收益。

## 复现

先按 README 构建 Release。为了保留历史数据，以下命令把新实验写到独立目录；Linux 示例：

```sh
./build/release/render_bench --width 640 --height 360 --repeats 7 --warmups 2 --max-threads 8 --order-seed 20261024 --output output/benchmark/run_1.csv
./build/release/render_bench --width 640 --height 360 --repeats 7 --warmups 2 --max-threads 8 --order-seed 20261025 --output output/benchmark/run_2.csv
./build/release/render_bench --width 640 --height 360 --repeats 7 --warmups 2 --max-threads 8 --order-seed 20261026 --output output/benchmark/run_3.csv
```

Windows 用对应的 `.exe` 路径。`--max-threads` 是过滤候选集合 `{1,2,4,8}` 的上限，不会测试集合以外的线程数。程序有意采用固定场景及参数矩阵；新增实验应记录修改与编译环境，不能混入旧 CSV 当作同一批数据。

原始 `run_1.csv` 至 `run_3.csv` 是逐帧记录；`summary.csv` 保留每组的样本量、中位数、均值、样本标准差与线性插值 p95。修改数据后需要重新汇总，不能沿用旧表。CLI 显示的单帧 `frame_ms` 与基准程序完整帧的清理口径不同，不能直接用于替换本报告结果。
