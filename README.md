运行环境：Win10 visual studio 2019

你需要安装ADQ7DC驱动，以便代码中的ADQ API可用。

  ADQ7DC是我们的高速采集卡设备，采集数据传输到PC。
  
如果你想借鉴高速存储的策略，可以用你的采集卡的API把有关ADQ7DC的API替代就可以了。

如果你想测试我们高速流存储系统的存储能力：

  你需要配备：
    CPU	Intel Core i5-12600K @ 3.70GHz 10 cores/threads: 16
    Memory	Kingston DDR5 5200MHz 16GB
    SSD 	Kingston SKC3000D/2048G  Interface: M.2  Protocol: PCI-E 4.0×4 
    
  你需要运行：
    Datatransfer/HSSS.cpp
    Datatransfer/main.cpp
    ADQAPI/setting.h（ADQ7DC向后传输的数据量是可控制的，在这里设置）
      设置方法：
  
