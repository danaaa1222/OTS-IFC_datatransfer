Running environment: win10 visual studio 2019

You need to install the ADQ7DC driver in order for the ADQ API to be available in the code. The ADQ7DC is our high speed acquisition device that acquires data for transfer to PC.
  
If you want to draw on the strategy of high-speed storage, you can just replace the API about the ADQ7DC with the API of your capture card.

If you want to test the storage capacity of our high-speed streaming storage system:

  You need to equip:
  
    CPU	Intel Core i5-12600K @ 3.70GHz 10 cores/threads: 16
    
    Memory	Kingston DDR5 5200MHz 16GB
    
    SSD 	Kingston SKC3000D/2048G  Interface: M.2  Protocol: PCI-E 4.0×4 
    
    
  You need to run:
  
    Datatransfer/HSSS.cpp
    
    Datatransfer/main.cpp
    
    ADQAPI/setting.h（ADQ7DC向后传输的数据量是可控制的，在这里设置）
    
  
