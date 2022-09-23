# VideoEngineLibrary

# Version:1.0.0.1

主程序代码，不包含公共组件

相关公共资源版本
```
cuda_10.2.89_440.33.01_linux
TensorRT-7.2.3.4
cudnn-10.2-linux-x64-v8.0.5.39
opencv-3.4.9
```

AI  |  CAM  |  ADD  | MODIFY |  DEL 
:---:|:---:|:---:|:---:|:---:
1  |  1  |  只读一个AI参数  |  del/add  |  del
1  |  3  |  只读一个AI参数  |  del/add  |  del
3  |  3  |  只读一个AI参数  |  del/add  |  del
1  |  2  |  只读一个AI参数  |  del/add  |  del
3  |  3  |  只读一个AI参数  |  del/add  |  del
3  |  3  |  只读一个AI参数  |  del/add  |  del

模块之间使用通信端类型

mode | performance | taskmanager | algorithm | decode | encode | http_fg | http_bg |
:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:
listen |  recv  |  recv  |  recv  |  send |  send | send | recv
send   |  null  |  s/r   |  recv  |  send |  send | send | recv
recv   |  recv  |  recv  |  recv  |  send |  send | send | recv
