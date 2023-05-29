# restful API协议

1. 查询当前的layouts。layouts的value是layer到键值的映射，layer可以是”QWERTY“, "NUM", "Plugins"，layer的value是键盘key的二维数组，数组中元素是keycode的序号，可以从查询支持的keycode API返回的字符串数组中获取keycode字符串，从第一行按顺序到最后一行
* Method: GET
* URI: /api/layouts
* Input: none
* Output(json):
```
{
    "layouts" : {
                "QWERTY" :  [[index00, index01, index02, ...],
                             [index10, index11, index12, ...],
                             ...
                            ],
                "NUM" :     [...],
                "Plugins" : [...]
               }
}
```

2. 查询支持的keycode
* Method: GET
* URI: /api/keycodes
* Input: none
* Output(json):
```
{
    "keycodes" : [keycode1, keycode2, keycode3] // keycodex is keycode string
}
```

3. 更新键盘的keymap
* Method: PUT
* URI: /api/keymap
* Input(json):
```
{
    "layer" : "QWERTY", // QWERTY, NUM, Plugins
    "positions" : [pos1, pos2, pos3, pos4, ...], // posx is an integer = row-index * MATRIX_COLS + col-index
    "keycodes" : [keycode-index1, keycode-index2, keycode-index3, keycode-index4, ...] // keycode-index's are array indexes into /api/keycodes
}
```
* Output: none

4. 重置键盘的keymap
* Method: POST
* URI: /api/keymap
* Input(json):
```
{
    "positions" : [], // empty array
    "keycodes" : [] // empty array
}
```

5. 键盘状态
* Method: GET
* URI: /api/device-status
* Input: none
* Output:
```
{
    "boot_partition" : "ota_a"|"ota_b",
    "ble_state" : "关闭"|"未连接"|"已连接",
    "usb_state" : "打开"|"关闭",
    "start_time" : "start date"
}
```

6. 修改键盘功能开关
* Method: PUT
* URI: /api/switches/(BLE|WiFi|USB)
* Input:
```
{
    "enabled" : true|false,
    "ssid" : "ssid id",
    "passwd" : "passwd",
    "name" : "ble name"
}
```
* Output: none