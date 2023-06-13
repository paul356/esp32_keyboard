var keymap_layouts = {};
var keymap_changed = {};
var basic_key_codes = null;
var quantum_funct_descs = null;
var mod_bit_names = null;
var max_layer_num = 0;

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

function _funct_select(funct_name)
{
    var selected_index = -1;
    let funct_select = _create_element("select", null, {});

    let add_option = function(opt, index, arr) {
        if (opt["desc"] === funct_name) {
            selected_index = index;
        }
        let option = _create_element("option", opt["desc"], {});
        funct_select.append(option);
    }
    quantum_funct_descs.forEach(add_option);

    /*let layer_name = target_div.getAttribute("layer-name");
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
    select.addEventListener("change", selected);*/

    //target_div.replaceWith(select);
    funct_select.selectedIndex = selected_index;

    return funct_select;
}

function _basic_code_select(basic_code)
{
    var selected_index = -1;
    let bs_select = _create_element("select", null, {});

    let add_option = function(opt, index, arr) {
        if (opt === basic_code) {
            selected_index = index;
        }
        let option = _create_element("option", opt, {});
        bs_select.append(option);
    }
    basic_key_codes.forEach(add_option);

    bs_select.selectedIndex = selected_index;

    return bs_select;
}

function _layer_select(layer_idx)
{
    var selected_index = -1;
    let layer_select = _create_element("select", null, {});

    let add_option = function(opt, index, arr) {
        if (opt.toString() === layer_idx) {
            selected_index = index;
        }
        let option = _create_element("option", opt.toString(), {});
        layer_select.append(option);
    }
    let layers = [...Array(max_layer_num).keys()];
    layers.forEach(add_option);

    layer_select.selectedIndex = selected_index;

    return layer_select;
}

function _mod_bit_select(mod_bits)
{
    let mod_bit_arr = mod_bits.split("|");
    let selected_index = -1;

    let mod_bit_select = _create_element("select", null, {"multiple" : ""});

    let add_option = function(opt, index, arr) {
        if (mod_bit_arr.includes(opt)) {
            let option = _create_element("option", opt, {"selected" : ""});
            mod_bit_select.append(option);
        } else {
            let option = _create_element("option", opt, {});
            mod_bit_select.append(option);
        }
    }
    mod_bit_names.forEach(add_option);

    return mod_bit_select;
}

function _get_funct_desc(funct_name)
{
    return quantum_funct_descs.find(element => element["desc"] === funct_name);
}

function _keystroke_clicked(event)
{
    let target_div = event.target;
    let layer_name = target_div.getAttribute("layer-name");
    let row = parseInt(target_div.getAttribute("row"));
    let col = parseInt(target_div.getAttribute("col"));

    let full_key = keymap_layouts[layer_name][row][col];
    let toks = full_key.split(" ");

    let new_div = _create_div(null, {"class" : "keystroke"});

    let funct_select = _funct_select(toks[0]);
    new_div.append(funct_select);

    let funct_desc = _get_funct_desc(toks[0]);

    let arg_handler = function(opt, index, arr) {
        switch (opt) {
        case "layer_num":
            new_div.append(_layer_select(toks[index + 1]));
            break;
        case "basic_code":
            new_div.append(_basic_code_select(toks[index + 1]));
            break;
        case "mod_bits":
            new_div.append(_mod_bit_select(toks[index + 1]));
            break;
        }
    }
    funct_desc["arg_types"].forEach(arg_handler);

    let focus_out = function(event) {
        var full_key = Array(new_div.children.length);
        for (var i = 0; i < new_div.children.length; i++) {
            let opts = new_div.children.item(i).selectedOptions;
            var argument_value = Array(opts.length);
            for (var j = 0; j < opts.length; j++) {
                argument_value[j] = opts.item(j).label;
            }
            full_key[i] = argument_value.join("|");
        }
        let full_key_str = full_key.join(" ");
        if (full_key_str !== keymap_layouts[layer_name][row][col]) {
            keymap_layouts[layer_name][row][col] = full_key_str;
            keymap_changed[layer_name][row][col] = true;
            target_div.innerHTML = _get_display_key_name(full_key_str);
        }
        new_div.replaceWith(target_div);
    }
    new_div.addEventListener("focusout", focus_out);

    target_div.replaceWith(new_div);
    funct_select.focus();
}

function _get_display_key_name(full_key)
{
    let toks = full_key.split(' ')
    if (toks[0] === 'BS') {
        return toks[1];
    } else {
        return toks[0] + "(" + toks.slice(1).join(", ") + ")";
    }
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
            let dis_str = _get_display_key_name(key);
            let key_div = _create_div(dis_str, {"class" : "keystroke", "layer-name" : name, "row" : i, "col" : j});
            key_div.addEventListener("click", _keystroke_clicked);
            layout_div.append(key_div);
        }

        row.forEach(render_key);
        layout_div.append(_create_element("br", null, {}));
    }
    keymap.forEach(render_row);
}

function _get_button_str(state)
{
    return state ? "Turn Off" : "Turn On";
}

function _render_status_line(status_json)
{
    let status_div = document.getElementById("status_line");
    status_div.append(_create_div("设备状态", {"class" : "title"}));

    for (let item in status_json) {
        status_div.append(_create_div(item + ":" + status_json[item], {"class" : "status-item"}));
    }
}

function _button_clicked(event)
{
    let button = event.target;
    let action_path_prefix = "/api/switches/";

    let state = button.getAttribute("curr_state");
    let funct = button.getAttribute("kb_function");

    button.disabled = true;

    let xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
        if (xhttp.readyState == 4) {
            if (xhttp.status == 200) {
                button.innerHTML = _get_button_str(state == "false") + " " + funct;
                button.setAttribute("curr_state", state == "false");
                button.disabled = false;
            } else {
                _handle_server_error(xhttp);
            }
        }
    };

    xhttp.open("PUT", action_path_prefix + funct, false);

    xhttp.setRequestHeader("Content-Type", "application/json");
    let data = JSON.stringify({"enabled" : state == "false"});
    xhttp.send(data);
}

function _render_switches_line(status_json)
{
    let switches_div = document.getElementById("switches_line");

    let ble_switch = _create_element("button", _get_button_str(status_json["ble_state"]) + " BLE", {"curr_state" : status_json["ble_state"], "kb_function" : "BLE"});
    let usb_switch = _create_element("button", _get_button_str(status_json["usb_state"]) + " USB", {"curr_state" : status_json["usb_state"], "kb_function" : "USB"});

    ble_switch.addEventListener("click", _button_clicked);
    usb_switch.addEventListener("click", _button_clicked);

    switches_div.append(ble_switch);
    switches_div.append(usb_switch);
}

function _get_keycodes()
{
    let keycode_path = "/api/keycodes";

    let xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
        if (xhttp.readyState == 4) {
            if (xhttp.status == 200) {
                let keycode_json = JSON.parse(xhttp.responseText);
                basic_key_codes = keycode_json["keycodes"];
                quantum_funct_descs = keycode_json["quantum_functs"];
                mod_bit_names = keycode_json["mod_bits"];
                max_layer_num = keycode_json["layer_num"];
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
                let status_json = JSON.parse(xhttp.responseText);
                _render_status_line(status_json);
                _render_switches_line(status_json);
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

