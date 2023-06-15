# AnimatedTexture
AnimatedTexture tick in render thread

此插件是修改于 https://github.com/Jackson311/AnimatedTexture2D 的插件
大幅度减少了内存占用，并修改加载GIF为异步加载，加入了多线程来减少卡顿现象，去除了GIF循环时最后一帧等待时间。
