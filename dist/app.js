var keymap_layouts = {};
var keymap_changed = {};
var keycode_array = null;

function clear_label()
{
    document.getElementById("upload_resp").innerHTML = "";
}

function _handle_server_error(xhttp)
{
    if (xhttp.status == 0) {
        alert("Server closed the connection abruptly!");
        location.reload();
    } else {
        alert(xhttp.status + " Error!\n" + xhttp.responseText);
        location.reload();
    }
}

function upload_bin()
{
    document.getElementById("upload_resp").innerHTML = "";

    let upload_path = "/upload/bin_file";
    let fileInput = document.getElementById("bin_file").files;

    if (fileInput.length == 0) {
        alert("No file selected!");
    } else {
        document.getElementById("bin_file").disabled = true;
        document.getElementById("upload").disabled = true;

        let file = fileInput[0];
        let xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function() {
            if (xhttp.readyState == 4) {
                if (xhttp.status == 200) {
                    document.getElementById("upload_resp").innerHTML = xhttp.responseText;
                    document.getElementById("bin_file").disabled = false;
                    document.getElementById("upload").disabled = false;
                } else {
                    _handle_server_error(xhttp);
                }
            }
        };
        xhttp.open("POST", upload_path, true);
        xhttp.send(file);
    }
}

function _create_element(tag, content, attrs)
{
    let div = document.createElement(tag);
    for (let attr in attrs) {
        div.setAttribute(attr, attrs[attr]);
    }
    if (content != null) {
        div.innerHTML = content;
    }
    return div;
}

function _create_div(content, attrs)
{
    return _create_element("div", content, attrs);
}

function _keystroke_clicked(event)
{
    let target_div = event.target;

    let select = _create_element("select", null, {"class" : "keystroke"});
    let add_option = function(opt, index, arr) {
        let option = _create_element("option", opt, {});
        select.append(option);
    }
    keycode_array.forEach(add_option);

    let layer_name = target_div.getAttribute("layer-name");
    let i = parseInt(target_div.getAttribute("row"));
    let j = parseInt(target_div.getAttribute("col"));

    let focus_out = function(event) {
        select.replaceWith(target_div);
    }

    let selected = function(event) {
        if (keymap_layouts[layer_name][i][j] != select.selectedIndex) {
            keymap_layouts[layer_name][i][j] = select.selectedIndex;
            target_div.innerHTML = keycode_array[select.selectedIndex];
            keymap_changed[layer_name][i][j] = true;
        }
    }
    select.addEventListener("focusout", focus_out);
    select.addEventListener("change", selected);

    target_div.replaceWith(select);
    select.selectedIndex = keymap_layouts[layer_name][i][j];
    select.focus();
}

function _render_keymap(layout_div, name, keymap)
{
    keymap_layouts[name] = keymap;
    keymap_changed[name] = new Array(keymap.length);
    for (let idx = 0; idx < keymap.length; idx += 1) {
        keymap_changed[name][idx] = new Array(keymap[idx].length);
        keymap_changed[name][idx].fill(false);
    }
    
    layout_div.append(_create_div(name, {"class" : "title"}));
    
    let render_row = function(row, i, rows) {
        // render row here
        let render_key = function(key, j, row) {
            let key_div = _create_div(keycode_array[key], {"class" : "keystroke", "layer-name" : name, "row" : i, "col" : j});
            key_div.addEventListener("click", _keystroke_clicked);
            layout_div.append(key_div);
        }

        row.forEach(render_key);
        layout_div.append(_create_element("br", null, {}));
    }
    keymap.forEach(render_row);
}

function _render_status_line(status_div, status_json)
{
    status_div.append(_create_div("设备状态", {"class" : "title"}));

    for (let item in status_json) {
        status_div.append(_create_div(item + ":" + status_json[item], {"class" : "status-item"}));
    }
}

function _get_keycodes()
{
    let keycode_path = "/api/keycodes";

    let xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
        if (xhttp.readyState == 4) {
            if (xhttp.status == 200) {
                let keycode_json = JSON.parse(xhttp.responseText);
                keycode_array = keycode_json["keycodes"];
            } else {
                _handle_server_error(xhttp);
            }
        }
    }

    xhttp.open("GET", keycode_path, false);
    xhttp.send();    
}

function render_status_line()
{
    let status_path = "/api/device-status";

    let xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
        if (xhttp.readyState == 4) {
            if (xhttp.status == 200) {
                let status_div = document.getElementById("status_line");
                let status_json = JSON.parse(xhttp.responseText);
                _render_status_line(status_div, status_json);
            } else {
                _handle_server_error(xhttp);
            }
        }  
    }

    xhttp.open("GET", status_path, true);
    xhttp.send();
}

function render_layouts()
{
    _get_keycodes();
    
    let layouts_path = "/api/layouts";

    let xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
        if (xhttp.readyState == 4) {
            if (xhttp.status == 200) {
                let layout_div = document.getElementById("key_layouts");
                let layout_json = JSON.parse(xhttp.responseText);
                let keymap = layout_json["layouts"];
                for (let layer in keymap) {
                    _render_keymap(layout_div, layer, keymap[layer]);
                }
            } else {
                _handle_server_error(xhttp);
            }
        }
    }

    xhttp.open("GET", layouts_path, true);
    xhttp.send();
}

function _update_keymap_layer(layer_name)
{
    let update_arr = [];
    let keycodes = [];
    let col_size = keymap_changed[layer_name][0].length;

    let visit_row = function(row, i, arr) {
        let visit_col = function(changed, j, arr) {
            if (changed) {
                update_arr.push(i * col_size + j);
                keycodes.push(keymap_layouts[layer_name][i][j]);
            }
        }
        row.forEach(visit_col);
    }

    keymap_changed[layer_name].forEach(visit_row);

    if (update_arr.length == 0) {
        return;
    }

    let update_path = "/api/keymap";

    let xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
        if (xhttp.readyState == 4) {
            if (xhttp.status == 200) {
                document.getElementById("button_ok").disabled = false;
            } else {
                _handle_server_error(xhttp);
            }
        }
    }

    document.getElementById("button_ok").disabled = true;

    xhttp.open("PUT", update_path, true);
    xhttp.setRequestHeader("Content-Type", "application/json");
    let data = JSON.stringify({"layer" : layer_name, "positions" : update_arr, "keycodes" : keycodes});
    xhttp.send(data);
}

function update_keymap()
{
    for (let layer_name in keymap_changed) {
        _update_keymap_layer(layer_name);
    }
}

function restore_default_keymap()
{
    let reset_path = "/api/keymap";

    let xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
        if (xhttp.readyState == 4) {
            if (xhttp.status == 200) {
                location.reload();
            } else {
                _handle_server_error(xhttp);
            }
        }
    }

    document.getElementById("button_reset").disabled = true;

    xhttp.open("POST", reset_path, true);
    xhttp.setRequestHeader("Content-Type", "application/json");
    let data = JSON.stringify({"positions" : [], "keycodes" : []});
    xhttp.send(data);
}

