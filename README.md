# AnimatedTexture
AnimatedTexture tick in render thread

此插件是修改于 https://github.com/Jackson311/AnimatedTexture2D 的插件
大幅度减少了内存占用，将渲染部分放到了渲染线程上，去除了GIF循环时最后一帧等待时间。