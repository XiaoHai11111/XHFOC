# USB 驱动安装：

1. zadig里面option选list all devices

2. type C分两面，直到出来 xhfoc 1.0 native interface（interface 2）
3. 选择安装libusb-win32以及WCID驱动

# python环境安装：

1. 解压python
2. 配置python变量
3. get-pip.py 文件放到python根目录
4. 用cmd运行python
5. 再用python运行get-pip.py，完成后python根目录会多2个文件夹Lid 和 Scripts
6. 将Scripts目录配置到环境变量
7. CMD内运行 pip install ipython  安装ipython
8. CMD内运行 pip install pywin32  安装pywin32
9. CMD内运行 pip install pyusb    安装pyusb
