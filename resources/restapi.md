# restful API协议

1. 查询当前的layouts。layouts的value是layer到键值的映射，layer可以是”QWERTY“, "NUM", "Plugins"，layer的value是键盘key的二维数组，数组中元素是键码字符串，从第一行按顺序到最后一行
* Method: GET
* URI: /api/layouts
* Input: none
* Output(json):
```
{
    "layouts" : {
                "QWERTY" :  [[code00, code01, code02, ...],
                             [code10, code11, code12, ...],
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
    "keycodes" : [keycode1, keycode2, keycode3], // keycodex is keycode string
    "mod_bits" : [],
    "quantum_functs" : [],
    "layer_num" : 3
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
    "keycodes" : [keycode1, keycode2, keycode3, keycode4, ...] // keycode's are strings
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
    "mode" : "hotspot"|"client", // for WiFi
    "enabled" : true|false, // for BLE and USB
    "ssid" : "ssid id",
    "passwd" : "passwd",
    "name" : "ble name"
}
```
* Output: none

7. 获取Macro字符串
* Method: GET
* URI: /api/macro/macro-name
* Input: none
* Output:
{
    "macro-name" : "macro-content"
}

8. 设置Macro字符串
* Method: PUT
* URI: /api/macro/macro-name
* Input:
```
{
    "macro-name" : "macro-content"
}
```
* Output: none
