# 架构与接口契约

## 数据流

```text
Scene（图元、纹理、M/V/P）
  → prepare_scene：变换、六平面齐次裁剪
  → vector<ClipTriangle>
  → Rasterizer::render：屏幕准备
      ├─ Reference：依次扫描图元包围框
      ├─ TileSerial：稳定分桶，依次处理 Tile
      └─ TileParallel：稳定分桶，线程池处理非空 Tile
  → 共用覆盖 / 深度 / 插值 / 着色核
  → Framebuffer（线性 RGB、深度）
  → PPM（8 位 sRGB）
```

| 模块 | 文件 | 职责 |
|---|---|---|
| 数学 | `include/miniraster/math.hpp` | 向量、矩阵、相机与投影 |
| 几何与裁剪 | `geometry.hpp`、`clipper.hpp`、`src/clipper.cpp` | 属性携带、六平面裁剪与扇形剖分 |
| 帧缓冲 | `framebuffer.hpp`、`src/framebuffer.cpp` | 连续颜色/深度数组与 PPM 编码 |
| 纹理 | `texture.hpp`、`src/texture.cpp` | clamp、最近邻与双线性采样 |
| 光栅 | `rasterizer.hpp`、`src/rasterizer.cpp` | 屏幕准备、稳定分桶及共用像素核 |
| 调度 | `thread_pool.hpp`、`src/thread_pool.cpp` | 任务队列、同步、异常传播与线程回收 |
| 场景 | `scenes.hpp`、`src/scenes.cpp` | 程序生成资源、变换裁剪入口 |
| 应用 | `src/main.cpp`、`cli_utf8.hpp` | 参数校验、UTF-8 路径与运行输出 |

表中头文件均位于 `include/miniraster/`。

## 空间与数值约定

- 右手系、列向量、相机朝观察空间 −Z；`p_clip = P * V * M * p_object`，最右侧先作用。
- `Mat4` 行主序存储，使用 `at(row,col)`；存储布局不改变数学乘法顺序。
- 基础值类型为 `float`；裁剪距离、交点参数、投影和插值中间值使用 `double`。
- 裁剪体为 `-w ≤ x,y,z ≤ w`。先裁剪后透视除法，边界算内部。
- 图像左上原点，X 向右、Y 向下。`x_screen=(x_ndc+1)*W/2`，`y_screen=(1-y_ndc)*H/2`。
- 像素中心为 `(x+0.5,y+0.5)`，线性下标为 `y*W+x`。有效像素是半开区域 `[0,W)×[0,H)`。

### 裁剪

Sutherland–Hodgman 逐半空间处理多边形。边交点参数为 `t=dA/(dA-dB)`，位置与全部原始属性使用同一个 `t` 插值，然后对凸多边形扇形剖分。

`clip_triangle` 对非有限位置或属性抛出异常。齐次零点和投影退化图元移除。可投影顶点要求 `w>0`，不以固定绝对 epsilon 排除微小正 `w`；这保留了可表示范围内的正齐次缩放性质。极端输入的浮点上溢、下溢或信息损失不在该性质测试的保证范围内。

### 离散覆盖

屏幕坐标通过 `llround(screen*256)` 量化，像素采样点为 `(256*x+128,256*y+128)`。顶点每轴舍入误差至多 1/512 像素。边函数使用 `int64_t`：

```text
E(a,b,p) = (b.x-a.x)*(p.y-a.y) - (b.y-a.y)*(p.x-a.x)
```

准备阶段统一正面积；需要时交换顶点及其全部属性。不做背面剔除。边内 `E>0`，`E=0` 时仅接纳 `dy<0 || (dy==0 && dx>0)` 的边。此规则依赖上述 Y 方向与边函数符号。

屏幕坐标绝对值限制为 2^20，以控制定点乘积范围。量化零面积图元跳过。`Rasterizer` 的前置条件是**输入已经齐次裁剪**；内部防御性校验不替代裁剪器。

### 深度、插值与着色

NDC z 从 `[-1,1]` 映射至 `[0,1]`。默认深度清为正无穷，严格 `<` 通过测试；等深保留先提交图元。允许 NDC z 的 1e-5 舍入余量并 clamp，不用该余量扩大 XY 覆盖。

令 `lambda_i` 为量化后三角形的屏幕重心权重：

```text
depth = sum_i(lambda_i * depth_i)
q_i = (lambda_i / w_i) / sum_j(lambda_j / w_j)
attribute = sum_i(q_i * attribute_i)
```

深度使用屏幕线性权重；UV、颜色、法线和世界位置使用透视校正权重。法线插值后归一化。`--affine` 改用 `lambda` 插普通属性，深度规则不变。

纹理左上原点，u 向右、v 向下，clamp-to-edge。双线性至多读四个 texel，无 mipmap。颜色在线性空间计算，写 PPM 时编码为 sRGB。Blinn–Phong 是简化光照；不声明物理正确、能量守恒或与图形 API 逐位一致。

## 公共渲染接口

```cpp
RenderStats Rasterizer::render(
    const std::vector<ClipTriangle>& triangles,
    Framebuffer& target,
    const RenderOptions& options = {},
    ThreadPool* pool = nullptr) const;
```

- 不清屏；调用者负责 `target.clear(...)`。覆盖诊断数组也由调用者预分配、初始化，大小必须为 `W*H`。
- `Scene` 拥有几何和纹理；`Framebuffer` 拥有连续颜色/深度数组；`RenderOptions::texture` 是借用指针。
- 调用者拥有并复用线程池。并行模式必须提供池，`render` 返回前等待本帧工作完成。
- 渲染期间不得修改输入、纹理或输出容器的大小，也不得由其他渲染调用同时写同一输出。不要修改 `colors()` / `depths()` 容量而破坏尺寸不变量。
- 非法 Tile、缺池、覆盖数组尺寸不匹配和非有限光照参数抛出异常。准备阶段跳过非法/不可投影图元。失败时不承诺输出事务回滚，调用者应重新清屏后再渲染。

## 并发不变量与异常安全

分桶在提交任务前串行完成。每个桶按图元提交顺序追加索引；Reference、TileSerial、TileParallel 因而保留等深规则。每个像素归属唯一 Tile，任务只写本 Tile 的颜色、深度、覆盖计数和独立统计槽；几何、纹理和桶在任务执行时只读。像素写入不需要原子操作或互斥锁。

线程池用互斥锁保护队列、活动计数、停止状态和异常指针，用条件变量等待可用任务及全部完成。任务体在锁外执行。`wait()` 等到队列为空且活动数为零，随后传播第一个任务异常并消费该异常状态；后续任务仍可使用池。

若 `render` 的任务入队失败，先等待已经入队的任务结束，再传播原异常，避免引用捕获的本帧局部对象提前销毁。析构停止接收新任务，排空队列并 join 所有工作线程。构造工作线程部分失败也执行回收。

`wait()` 仅供生产者线程调用，不能由该池自身的任务调用。当前接口不定义多个生产者同时组织互相独立批次的语义；线程池应由当前渲染批次专用。

独占写入与生命周期约束是并发设计依据；图像一致性测试不等于竞争检测。当前没有 ThreadSanitizer 运行结果。

## 统计字段与边界

`RenderStats` 分别报告准备、分桶、光栅和内部总耗时。`total_ms` 只覆盖 `render` 内部阶段，局部对象析构不在该内部计时中。完整帧口径由基准程序外层计时，见 [实验说明](benchmarks.md)。

`auxiliary_bytes` 只估算 prepared triangles、桶和统计数组的 vector 容量，不含任务队列、线程栈、分配器开销、场景与纹理，也不是进程峰值内存。

当前场景法线链仅处理刚体旋转。一般非均匀缩放需要逆转置矩阵；当前不提供模型导入、透明混合、阴影、抗锯齿或 GPU 后端。
