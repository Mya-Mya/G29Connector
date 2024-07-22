# G29Connector
G29 Connector reads the state of the connected [G29 Driving Force Racing Wheel](https://gaming.logicool.co.jp/products/driving/driving-force-racing-wheel.941-000118.html) and hosts a http server to transmit it to your program.

⚠️Note that this program only works on Windows.

# Compile
This program needs the following external libraries. Download them.

* [Logitech Steering Wheel SDK](https://www.logitechg.com/en-us/innovation/developer-lab.html)
* [cpp-httplib](https://chromium.googlesource.com/crashpad/crashpad/+/a7859e9bc63e54e972906745cbcc1b8543a4fe10/third_party/cpp-httplib/cpp-httplib/)

When compiling, the following settings are required.

* **Include Path** 
    * cpp-httplib
    * LogitechSteeringWheelSDK_X.Y.Z\Include
* **Additional Library Directories**
    * LogitechSteeringWheelSDK_X.Y.Z\Lib\x64
* **Additional Dependencies**
    * LogitechSteeringWheelLib.lib

# How to Use
Connect your G29 device to your PC before launch G29Connector. G29Connector hosts a http server to communicate with your program.

### Server Information
* **Hostname** : `localhost`
* **Port** : `3829`

### Retrieve the state of G29
* **Request**
    * Endpoint : `/`
    * Method : `GET`
    * Header : None
* **Response**
    * Content Type : `text/json`
    * Body Structure: The following

### Response Body Structure
Property|Type|Description
---|---|---
`scalars`|object|Collection of states in numerical form
`scalars.range_rad`|double|The operating steering range which is set by G29. You can change the range by [Logitech G Hub](https://www.logitechg.com/en-us/innovation/g-hub.html) app.
`scalars.steering_rad`|double|Steering angle by radian
`scalars.throttle`|double|The range is $[0,1]$.
`scalars.brake`|double|The range is $[0,1]$.
`scalars.timestamp`|double|Epoch time at which this state was collected
`buttons`|object|Collection of states in boolean form, especially whether or not each button is pressed. The following properties are available: `circle`, `triangle`, `square`, `cross`, `R2`, `R3`, `L2`, `L3`, `plus`, `minus`, `share`, `options`, `playstation`, `return`, `paddle_right`, `paddle_left`.
