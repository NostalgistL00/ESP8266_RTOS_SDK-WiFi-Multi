# ESP8266_RTOS_SDK-WiFi-Multi
Just make it ez to configure wifi in ESP-IDF
闲的没事写的一个ESP8266_RTOS_SDK配网的一些函数，将自定义个数的WiFi信息保存在NVS Flash中，下次启动初始化之后会自动连接所保存的Wifi当中信号最好的一个，
大于保存上限的时候会将最早添加的WiFi信息删除掉。至于那个为什么叫Connect From HTTP，是因为我要做这个东西的时候是想通过HTTP实现配网的，懒得改名字了。 
