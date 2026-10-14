# 像素仿制图章技术参考

当前功能和限制以 `AI/AI_Context/Clone_Stamp.md` 为准；静态产品边界见 `AI/AI_Context/Static_Editing_Scope.md`。

## 功能术语
- [Adobe 仿制图章说明](https://helpx.adobe.com/uk/photoshop/desktop/repair-retouch/heal-clone/retouch-images-with-the-clone-stamp-tool.html)：图章从源区域复制像素；对齐开启跨笔保留源/目标偏移，关闭每笔从原始取样点重新开始。
- PS 还提供软边、流量、源变换等功能，本项目明确不采用软边和独立流量，也不设计动画能力。
- [Aseprite 图案笔刷](https://www.aseprite.org/docs/brushes/)重复固定图案，与随目标位置移动的图章取样不同，不能直接替代仿制图章。

## 原生接入判断
- 工具栏按钮接入难度低；完整像素取样与状态管理难度中等，不需要重写编辑器。
- 原生 Tool + Ink 可以复用笔刷覆盖区域、连续笔画、目标选区和撤销事务，比以 Lua Command 按钮实现连续绘图更适合。
- 单独 Y 作为一次性取样命令，精确区分 Cmd/Ctrl+Y 的重做与 Space+Y 的笔刷翻转；无需覆盖 Alt 吸管。
- 取样点 S、初始目标 D 决定偏移 Δ=S−D，目标 P 从源 P+Δ 读取。对齐保留 Δ，关闭对齐则每笔重新计算。

## 必须处理的技术边界
- 同笔源图与目标写入隔离，避免重叠时拖影；每笔刷新源快照。
- 源点、目标画布和 Cel 偏移要统一坐标。现有 ExpandCelCanvas 会临时重定位目标 Cel，因此快照必须先获取。
- 合成取样应沿用图层透明度、混合模式和组结构，排除编辑器装饰、洋葱皮与参考层。
- 原生 Timeline 同时管理图层和帧，隐藏整个组件会丢失图层管理，需要静态绘制/命中分支。
- 标记属于编辑器覆盖层，不应进入取样、工程、导出或撤销数据。
- 自动验证能够覆盖像素、偏移、透明度、选区与撤销；视觉和连续绘图手感由用户实际验收。
