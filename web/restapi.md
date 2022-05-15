# restful API协议

1. 查询当前的layouts
* Method: GET
* URI: /api/layouts
* Input: none
* Output(json):
```
{
    "layouts" : {
                "QWERTY" : ["ESC", "F1", "F2", ...],
                "NUM" : [...],
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
    "positions" : [pos1, pos2, pos3, pos4, ...], // posx is an integer
    "keycodes" : [keycode1, keycode2, keycode3, keycode4, ...] // keycode4 is a keycode string
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
