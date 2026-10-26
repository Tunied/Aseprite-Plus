# 单格编辑锁定

## 交互
- 顶部 Pixel-Perfect 右侧的挂锁 Toggle，Tooltip 按产品要求保留 `Lock Gird`；切换到 M 选区工具后仍可访问。
- 按钮按下状态与当前文档 `View > Show > Grid` 同步。点击未启用锁定的按钮显示网格，进入选格状态，顶部提示 `选择一个要编辑的网格`。
- 左键点击画布选择一个格子，消费该次点击而不落笔；格子使用文档网格原点和尺寸，边缘格裁剪到画布范围。绿色边框只在编辑器显示。
- 锁定期间再次点击按钮取消锁定并隐藏网格；菜单隐藏网格也会取消锁定。等待选格时 Esc 取消；更改网格原点/尺寸后重新等待选格。
- 锁定属于文档瞬时状态，切换图层/工具保留，各文档独立，不写入文件或撤销历史。

## 像素与选区
- 普通图像层绘图使用独立的有效 Ink Mask（用户选区 ∩ 锁定格），不替换文档选区；铅笔、橡皮、填充、形状、图章等原生绘图管线均受裁剪。
- Tilemap 的图章也使用相同 Ink Mask，允许取样点位于锁定格外。Manual 下本次图章事务临时使用 Auto 写回独立 Tile，确保格外重复引用不受影响；Auto/Stack 沿用自身规则，不改变用户颜色栏模式。取样点击由图章状态消费，不被 Tilemap 锁定拦截。
- M 可选择格外纹理并复制/剪切；浮动选区、方向键移动和复制出的纹理保留完整范围。预览和落定时只把锁定格内的目标像素写回，格外源像素也不会被剪切移除。
- 删除/剪切命令固定当次有效遮罩，撤销/重做不依赖之后的锁定状态。
- 整层移动会改变格外像素，锁定期间应改用 M 选区移动。Tilemap 除图章外的绘图及参考层特殊几何编辑仍不开放格锁定；Tilemap 浮动选区移动不属于普通像素层锁定管线。

## 实现入口
- `src/app/grid_lock.h`、`Doc::gridLock()`：状态和网格命中。
- `src/app/ui/context_bar.cpp`：按钮、选格提示和文档网格显示同步。
- 工具栏在活动文档变为空时必须同步解除文档网格偏好订阅；`DocObserverWidget` 的空站点切换不会调用 `onDocChange(nullptr)`，因此由 `ContextBar::onActiveSiteChange` 补齐。订阅必须在文档偏好销毁前断开。
- `src/app/ui/editor/standby_state.cpp`、`editor.cpp`：选格点击、等待光标和绿色边框。
- `src/app/ui/editor/tool_loop_impl.cpp`：绘图私有遮罩。
- `src/app/ui/editor/pixels_movement.cpp`、`src/app/util/expand_cel_canvas.cpp`：浮动预览与最终写入裁剪；复制/变换源保持完整。
- `src/app/cmd/clear_mask.cpp`：剪切/删除范围及重做遮罩。
- `data/icons/README.md`：本地 GameIcons 的 Lorc Padlock 来源、CC BY 3.0 授权和再生成说明。图标透明度掩码内嵌，颜色/尺寸随主题和 UI 缩放。

## 验证入口
- `OLDKING_BUILD_GRID_LOCK_TESTS=ON` 构建 `oldking_grid_lock_tests`；`misc/test_grid_lock.sh` 使用隔离偏好目录执行真实绘图/浮动选区事务。
- 同一构建选项生成 `oldking_grid_lock_ui_tests`；`misc/test_grid_lock_ui.sh` 用隔离偏好目录验证实际按钮回调、原生编辑器状态和网格重绘，覆盖空网格配置、菜单同步及浮动选区结束。仅在改动此激活路径时运行，可能短暂打开测试窗口。
- 同一选项生成 `oldking_context_bar_lifecycle_tests`；`sh misc/test_context_bar_lifecycle.sh` 使用无原生窗口的真实工具栏和隔离偏好目录，验证文档切换、空活动站点、文档先于工具栏销毁，以及同一偏好目录下的三次进程启动。
- `Editor::dropMovingPixels()` 仅能在 `isMovingPixels()` 为真时调用；普通待机状态不能弹栈。选格使用 `Site::gridBounds()`，与渲染的默认网格回退一致。
- GUI 布局、图标、选格和方向键的实际操作体验由用户最终验收。
