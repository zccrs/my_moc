my_moc
======

自动处理Q_PROPERTY宏，只需写好Q_PROPERTY(xxx)，可自动处理在.h文件中添加相应的函数和信号的定义，在cpp文件中添加实现函数的实现。一般用这个给qml注册属性的时候写的函数代码都是模板化的。为了编程方便就做了这个自动处理软件。
