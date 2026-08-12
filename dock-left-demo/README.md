# Dock 右侧区域 Demo

这是一个纯 QML 的 `dde-shell` Dock Applet。它通过 `dockOrder: 24` 放入 Dock 右侧区，显示一个小区域和系统关闭图标；点击图标后，区域会从 Dock 布局中隐藏。

## 构建与安装

```bash
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
sudo cmake --install build
```

安装后重启 `dde-shell`，或用系统提供的 `dde-shell -p org.deepin.ds.dock-left-demo` 隔离加载。该示例的 `Parent` 为 `org.deepin.ds.dock`。

在当前 dde-shell Dock 布局中，`dockOrder` 位于 `21` 到 `30` 的 Applet 会放入右侧区。要改变它与同一区域其他部件的相对顺序，调整 `dockOrder` 即可。

若要重新显示已关闭的区域，请重启 `dde-shell` 或重新加载插件。
