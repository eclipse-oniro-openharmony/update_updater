# 升级子系统Flashd使用指导<a name="ZH-CN_TOPIC_0000001148614629"></a>

-   [简介](#section184mcpsimp)
-   [说明](#section198mcpsimp)
-   [功能](#section218mcpsimp)
	-   [Update命令](#section220mcpsimp)
	-   [Format命令](#section220mcpsimp)
	-   [Erase命令](#section220mcpsimp)

-   [FAQ](#section247mcpsimp)

## 简介<a name="section184mcpsimp"></a>

Flashd是OpenHarmony升级子系统的一个刷机模式，可以提供格式化用户分区，擦除分区，刷写镜像，zip格式整包升级的功能。  
Flashd分为客户端和服务端，客户端提供统一的用户界面，服务端提供对应的功能服务，二者之间通过HDC作为数据传输通道。  


## 说明<a name="section198mcpsimp"></a>

1. 如何进入Flashd模式
在正常系统开机的情况下，执行如下命令：  
    `hdc_std shell reboot updater  `  
设备就会重启进入Flashd模式。  

2. 如何退出Flashd模式  
在系统正常开机的情况下，依次执行如下HDC命令：  
	`hdc_std shell /bin/updater_reboot  `  
设备就会退出Flashd模式重启到正常系统。  

## 功能<a name="section218mcpsimp"></a>
### Update命令<a name="section220mcpsimp"></a>
功能：提供zip格式的整包升级功能 。  
使用方法：`hdc_std update filename.zip  `  
参数说明：filename.zip用来指定zip升级包路径。  

### Format命令<a name="section220mcpsimp"></a>
功能：提供清除data分区用户数据的功能，当前只支持ext4和f2fs格式的文件系统格式化。  
使用方法：`hdc_std format [-f] data`  
参数说明：  
	(1) -f可选，表示强制执行命令，不需要用户确认，如果没有-f参数，客户端等待用户确认：  
		(a)输入yes或y(不区分大小写)表示确认执行。  
		(b)输入no或n(不区分大小写)表示取消执行。  

### Erase命令<a name="section220mcpsimp"></a>
功能：提供擦除分区的功能。  
使用方法：`hdc_std erase [-f] parition_x  `  
参数说明：  
	(1) paration_x表示要擦除的分区。  
	(2) -f可选，表示强制执行命令，不需要用户确认，如果没有-f参数，客户端等待用户确认：  
		(a)输入yes或y(不区分大小写)表示确认执行。  
		(b)输入no或n(不区分大小写)表示取消执行。  

备注：如果使用erase命令擦除系统的关键分区，会导致系统无法开机，请谨慎使用。  

## FAQ<a name="section218mcpsimp"></a>
