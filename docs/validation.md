# 验证范围与回归

## 已有证据

2026-10-23 的本机 Windows GCC 16.2.0 构建中，Debug、Release 和 UBSan 三种配置均通过六类 CTest。以下记录描述已执行的本机范围；Linux、MSVC 和 ThreadSanitizer 不计为已验证结果。仓库中的持续集成以实际运行状态为准。

2026-10-24，独立目录布局重新从源码构建，Windows GCC 16.2.0 的 Debug、Release、UBSan 仍各通过 6/6 项测试。分析脚本在独立布局下重新汇总 1323 个样本、63 组配置，统计值与保留的基线数据一致。

| CTest | 文件 | 主要断言 |
|---|---|---|
| math | `tests/test_math.cpp` | 向量、矩阵、MVP、相机/投影、独立手算值与非法参数 |
| clipper | `tests/test_clipper.cpp` | 六平面、手算交点、属性携带、退化与非有限输入、固定种子性质检查 |
| rasterizer | `tests/test_rasterizer.cpp` | 共享边、深度与等深、手算插值、纹理边界、模式一致、非法输入 |
| thread_pool | `tests/test_thread_pool.cpp` | 完成等待、任务异常、异常后复用、析构排空、非法任务 |
| scenes | `tests/test_scenes.cpp` | 六个完整场景、多种 Tile、裁剪后相邻图元的逐像素单覆盖 |
| cli | `tests/test_cli.cmake` | 帮助、版本、错误退出、PPM 格式与实际中文路径输出 |

## 避免“实现互证”的检查

三种遍历共用光栅核，互相比较只能发现遍历或调度差异，不能排除共同的公式错误，因此还保留独立预期值：

- 4×4 三角形像素 `(0,0)` 的透视校正颜色权重应为 `8/9, 2/27, 1/27`，深度应为 `0.2625`；仿射对照的颜色权重为 `0.75, 0.125, 0.125`，深度不变。
- 两三角形拼接的矩形，在深度测试前对采样次数计数，内部每像素必须等于 1。将相邻三角形分别经 near/left 裁剪后再次检查相同性质。
- 将同一三角形全部齐次坐标乘以 `2^-40`，要求当前样例的颜色与深度逐值不变，防止任意正 `w` 阈值丢弃可绘制几何。
- 遮挡按近处胜出；等深按提交顺序保留先到图元。比较覆盖次数可以识别被深度测试隐藏的重复光栅化。

## 集成与基准检查

完整场景在 97×61 的非整除尺寸下测试 Tile=7/16/32 的串并行颜色/深度逐值相等。光栅测试另覆盖三个 Shader、Tile=1/7/16/100、固定种子的随机三角形，并比较覆盖计数和着色片元数。

正式基准的 1323 个计时帧都在停止计时后与参考结果逐值比较，原始 CSV 的 `exact_match` 均为 1。它们证明已记录输入及配置下的相等，不能替代独立数值检查或所有输入的正确性证明。

## 路径兼容回归

曾在 Windows GCC/CMake/Ninja 链路中复现：非 ASCII 工程路径的首次构建成功，但增量构建读取依赖缓存失败。生成构建文件与依赖缓存的路径编码不同，且编译器会把 ASCII 别名规范化回真实工具链路径。

处理方式是通过稳定 ASCII 目录别名访问工程、工具链和构建目录，并在 MinGW GCC 配置中保留 `-no-canonical-prefixes` 与 `-fno-canonical-system-headers`。修复后重新检查首次与增量构建。该结论只适用于所测试工具链，不推断所有中文路径或 Ninja 版本都存在相同问题。

命令行文件路径采用另一项独立契约：Windows 读取宽字符命令行后转为 UTF-8，文件操作用 `std::filesystem::u8path`。CLI 测试实际生成 `中文输出/三角形.ppm`，并检查 64×48 图像的 P6 头和 9229 字节长度。构建系统路径兼容与运行期 UTF-8 输出需要分别验证。

## 本地复跑

```sh
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug --parallel
ctest --test-dir build/debug --output-on-failure

cmake -S . -B build/ubsan -DCMAKE_BUILD_TYPE=Debug -DMINIRASTER_UBSAN=ON
cmake --build build/ubsan --parallel
ctest --test-dir build/ubsan --output-on-failure
```

以上单配置命令适用于 GCC/Clang。MSVC 使用 README 的多配置命令，`MINIRASTER_UBSAN` 选项不支持 MSVC。UBSan 使用 trap 模式；检测到相应问题时进程异常终止，而非输出完整 sanitizer 诊断。

## 当前未覆盖的范围

- 未提供 ThreadSanitizer 或完整竞态分析工具的运行结果；UBSan 不检测数据竞争。
- 未验证多个生产者并发批次或工作线程内调用同池 `wait()`；这两项不在接口契约内。
- 未证明任意浮点输入的相邻裁剪结果都无裂缝；测试仅覆盖明示的解析和随机样例。
- 不宣称抗锯齿、透明排序、一般法线逆转置或图形 API 兼容性，这些功能没有实现。
- 图像正确与输出文件格式正确不等于显示器色彩校准或物理光照正确。
