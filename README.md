# multi-person-interactable-VR-cloud-rendering
服务器端配置步骤：    

（1）使用unity打开`joyrtc`文件夹下的unity文件  

（2）在`webui`下运行npm install  

（3）运行npm run build  

（4）然后把新生成的`dist`文件覆盖到`cloud`文件夹下，然后运行go run .   

（5）打开127.0.0.1:8080，点击start即可  


	 

客户端配置步骤：  

（1）将package导入unity    

（2）在unity中选择bulid and run 将客户端安装至虚拟现实头显中


	 

自适应速率算法仿真步骤：  

1.通过该链接在设备上安装Network simulator 3: https://www.nsnam.org/wiki/Installation#Installation.  


2.使用以下命令将自适应速率算法模块下载到对应目录下:  

`
cd ns-3-dev/  `

`cd contrib/  `

`git clone https://github.com/djvergad/dash.git`

需要在`src`文件夹中创建一个名为`dash`的文件夹

3.重新配置Network simulator 3并启用示例。在`ns-3-dev`文件夹输入:

对于3.36及更新的版本：

`./ns3 configure --enable-examples`  

对于3.35及更老的版本:

`./waf configure --enable-examples`  

4.设置已经完成，通过运行示例进行验证:

对于3.36及更新的版本：  


`./ns3 run 'dash-example --users=3 --algorithms=ns3::FdashClient --linkRate=1000Kbps --bufferSpace=10000000'`  

对于3.35及更老的版本:

`./waf --run 'contrib/dash/examples/dash-example --users=3 --algorithms="ns3::FdashClient" --linkRate=1000Kbps --bufferSpace=10000000'`
