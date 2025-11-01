# videoServer

#### 介绍
基于QT框架实现音乐播放器

#### 软件架构
软件架构说明

1 使用MusicWindow来搭建主界面,使用wideget设置各种控件元素
2 使用子线程MusicDownload来实现异步歌曲下载
3 使用子线程MusicDatabase来实现歌曲存储
4 使用MusicHttp处理网络请求
