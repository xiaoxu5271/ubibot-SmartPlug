## How to use

Clone this component to [ESP-IDF](https://github.com/espressif/esp-idf) project (as submodule): 
```
git submodule add https://github.com/tuanpmt/espmqtt.git components/espmqtt
```

Or run a sample (make sure you have installed the [toolchain](http://esp-idf.readthedocs.io/en/latest/get-started/index.html#setup-toolchain)): 

```
git clone https://github.com/tuanpmt/espmqtt.git
cd espmqtt/examples/mqtt_tcp
make menuconfig
make flash monitor
```