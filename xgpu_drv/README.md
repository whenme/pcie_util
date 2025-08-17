# xgpu_drv gpu demo driver

## Key features

### 1.Support multi GPU board
Such as 2 GPU board, the GPU devices will be /dev/xgpu0, /dev/xgpu1

### 2.All interfaces to user application are device files
Such as register CfgMemSize, the device file is /dev/xgpu0/reg/CfgMemSize.

### 3.User interface to access device file
1) Register mode
 With read/write as general file. In register mode, the value size will be 1-4 bytes.<br>
 It support bash shell to read/write register without application.
2) Memory mode
 With memory mode to access a section of memory. Such as read/write all configuration of GPU.<br>
 With ioctl to read/write register which is not defined as device file.
3) Interrupt interface (in development)
 GPU support MSI/MSI-x interrupt. The interrupt will be device file to user application.<br>
 User application can poll the interrupt device file for interrupts.
4) Interface in application<br>

bash shell:<br>

c/c++ code:<br>


## Sample for gpu driver(xgpu0 as example)

### 1.device file for GPU configuration
Path:&emsp; &emsp; &emsp; &emsp; /dev/xgpu0<br>
config device file: class  command  device  revision  status  vendor<br>
  &emsp;The device files are 1-4 bytes.<br>
config data file: &emsp;   config<br>
  &emsp;The device file is 256 bytes<br>

### 2.device file for GPU register
Path: &emsp; &emsp; &emsp; &emsp; /dev/xgpu0/reg<br>
register device file: CfgMemSize  VmFbLocBase  VmFbLocTop  VmFbOffset  VmLfbAddrEnd  VmLfbAddrStart<br>
