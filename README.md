# ile chan的图像工坊

只是学d2l期间百无聊赖地把所有会的图像算法集合了一下

Windows 图像处理软件，使用 Visual Studio 2022、C++17 和 Win32。

## 使用

运行桌面 `image` 文件夹中的 `ile chan的图像工坊.exe`，或工程内 `bin/Release/ile chan的图像工坊.exe`。

1. 打开或拖入图像。
2. 选择算法、设置参数，点击“应用到当前图像”。后续处理接续当前结果。
3. 使用“撤销”回退，或“恢复原图”。
4. 点击“另存 BMP”保存结果。

支持 PNG、JPEG、BMP、TIFF、GIF、ICO。GIF 和多页 TIFF 读取第一帧。灰度结果导出为 8 位 BMP，彩色结果导出为 24 位 BMP。

自定义卷积支持 1–15 阶奇数方阵、系数归一化、手动除数、偏移和边界设置。系数用空格或逗号分隔。

快捷键：Ctrl+O 打开，Ctrl+S 保存，F5 应用，Ctrl+Z 撤销；编辑参数时 Ctrl+Z 撤销文字修改。

## 编译

用 Visual Studio 2022 打开 `WkyImageStudio.sln`，选择 `Release | x64`，生成解决方案。需要安装“使用 C++ 的桌面开发”、v143 工具集和 Windows SDK。

也可以运行：

```powershell
.\build.ps1 -Configuration Release
```

## 目录

- `algorithms/src`：算法和 BMP 读写源文件。
- `algorithms/include`：算法和 BMP 读写头文件。
- `src`：界面、图像处理调用、格式导入与测试源文件。
- `tests/fixtures`：格式测试用例。
- `bin/Release`：可运行程序。

运行回归检查：`.\build.ps1 -Configuration Release -Test`。测试输出和编译缓存会在运行后自动生成。
