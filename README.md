# QtMediaPlayer - 基于Qt的多媒体播放器

这是一个基于Qt框架开发的多媒体播放器应用，支持音频和视频播放，并具备歌词下载、频谱分析、播放历史等高级功能。

## 功能特性
- **多媒体播放**：支持音频和视频文件的播放
- **歌词同步**：自动下载并同步显示歌词
- **频谱可视化**：实时显示音频频谱
- **播放历史**：记录播放历史，方便回放
- **网络功能**：支持在线歌词下载
- **跨平台**：支持Windows及其他平台（通过Qt跨平台特性）
- **现代界面**：基于Qt Widgets的用户界面

## 项目结构
- `main.cpp` - 程序入口和主函数
- `widget.cpp` / `widget.h` / `widget.ui` - 主窗口实现
- `audioplayer.h` - 音频播放器功能
- `videoplayer.h` - 视频播放器功能
- `lyricdownloader.h` - 歌词下载功能
- `lyricparser.h` - 歌词解析功能
- `lyricwidget.h` - 歌词显示组件
- `menu.h` - 菜单功能
- `playhistory.h` - 播放历史记录
- `spectrumwidget.h` - 频谱显示组件
- `QtMediaPlayer.pro` - 项目配置文件

## 编译与运行

### 环境要求
- Qt 6.5.3 或更高版本
- C++17 兼容编译器

### Windows 平台编译
1. 使用Qt Creator打开 `QtMediaPlayer.pro` 文件
2. 选择构建套件（如：Desktop Qt 6.5.3 MinGW 64-bit）
3. 点击构建（Ctrl+B）编译项目
4. 点击运行（Ctrl+R）启动程序

## 作者
lizy0627

## 更新记录:
Ver1.1:
新增在线音乐搜索功能（onlinemusicsearch.h）
完善项目配置文件，添加网络模块支持
优化跨平台编译配置
