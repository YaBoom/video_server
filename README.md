# video_server

## 项目简介

基于HTML的视频服务器，提供视频流媒体服务。

## 技术栈

- HTML
- JavaScript
- CSS

## 功能特性

- 视频播放
- 基础流媒体服务
- 简单的用户界面

## 安装说明

\\\ash
# 克隆项目
git clone https://github.com/YaBoom/video_server.git

# 启动简单HTTP服务器
cd video_server
python -m http.server 8000
# 或使用 Node.js 的 http-server
npx http-server
\\\

## 使用方法

\\\html
<!-- 在浏览器中打开 index.html -->
<!DOCTYPE html>
<html>
<head>
    <title>Video Server</title>
</head>
<body>
    <video width="320" height="240" controls>
        <source src="your-video.mp4" type="video/mp4">
        您的浏览器不支持视频标签。
    </video>
</body>
</html>
\\\

## 项目结构

\\\
video_server/
├── index.html          # 主页面
├── css/               # 样式文件
├── js/                # JavaScript 文件
├── videos/            # 视频文件目录
└── README.md          # 项目说明文件
\\\

## 配置说明

- 在 \ideos/\ 目录中放置视频文件
- 修改 HTML 文件以指向正确的视频路径

## API文档

提供基础的视频流媒体服务。

## 贡献指南

欢迎提交Issue和Pull Request！

## 许可证

本项目采用 [MIT License](LICENSE) 许可证。
